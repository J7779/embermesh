#include "host/endpoints/rigctl.h"

#include <cstdlib>

#include "host/endpoint.h"
#include "host/endpoints/ptt.h"
#include "host/util.h"

namespace ember_host {

// ---- encoders ------------------------------------------------------------------------------------

std::string rigctl_cmd_set_freq(uint64_t hz) { return "F " + std::to_string(hz) + "\n"; }

std::string rigctl_cmd_set_mode(const std::string& mode, int passband) {
  // Only pass a plain mode token (USB, LSB, FM, PKTUSB...): never let config inject extra lines.
  std::string m;
  for (char c : mode) {
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) m += c;
    else if (c >= 'a' && c <= 'z') m += char(c - 'a' + 'A');
  }
  if (m.empty()) m = "FM";
  if (passband < 0) passband = 0;
  return "M " + m + " " + std::to_string(passband) + "\n";
}

std::string rigctl_cmd_set_ptt(bool on) { return on ? "T 1\n" : "T 0\n"; }
std::string rigctl_cmd_get_freq() { return "f\n"; }

bool rigctl_parse_rprt(std::string_view line, int* code) {
  std::string t = trim(line);
  if (!starts_with(t, "RPRT")) return false;
  std::string rest = trim(std::string_view(t).substr(4));
  if (rest.empty() || rest.size() > 11) return false;
  size_t i = 0;
  bool neg = false;
  if (rest[0] == '-') {
    neg = true;
    i = 1;
  }
  if (i >= rest.size()) return false;
  long v = 0;
  for (; i < rest.size(); ++i) {
    if (rest[i] < '0' || rest[i] > '9') return false;
    v = v * 10 + (rest[i] - '0');
  }
  if (code) *code = int(neg ? -v : v);
  return true;
}

// ---- client --------------------------------------------------------------------------------------

RigctlClient::RigctlClient(Loop& loop, std::string host, uint16_t port)
    : loop_(loop), host_(std::move(host)), port_(port) {}

RigctlClient::~RigctlClient() {
  if (timer_) loop_.cancel(timer_);
  timer_ = 0;
  stream_.reset();
}

void RigctlClient::send(std::string cmd, Done done) {
  if (cmd.empty() || cmd.back() != '\n') cmd += '\n';
  if (queue_.size() >= 32) {
    // Drop the oldest command that is not in flight.
    auto it = queue_.begin();
    if (in_flight_) ++it;
    if (it != queue_.end()) {
      Done d = std::move(it->done);
      queue_.erase(it);
      if (d) d(kRigctlDisconnected, std::string());
    }
  }
  queue_.push_back(Cmd{std::move(cmd), std::move(done)});
  pump();
}

void RigctlClient::pump() {
  if (in_flight_ || queue_.empty()) return;
  if (!stream_ || !stream_->connected()) {
    if (connecting_) return;
    connecting_ = true;
    rx_.clear();
    stream_.reset(new TcpStream(loop_));
    stream_->on_connect = [this](const std::string& err) {
      connecting_ = false;
      if (!err.empty()) {
        EH_LOGW("rigctld %s:%u: %s", host_.c_str(), unsigned(port_), err.c_str());
        fail_all(kRigctlConnectFailed);
        return;
      }
      pump();
    };
    stream_->on_data = [this](const uint8_t* d, size_t n) {
      rx_.append(reinterpret_cast<const char*>(d), n);
      if (rx_.size() > 4096) rx_.erase(0, rx_.size() - 4096);
      size_t nl;
      while ((nl = rx_.find('\n')) != std::string::npos) {
        std::string line = rx_.substr(0, nl);
        rx_.erase(0, nl + 1);
        on_line(line);
      }
    };
    stream_->on_close = [this](const std::string& reason) {
      connecting_ = false;
      // Never touch stream_ from inside its own callback: finish on the next loop turn (which may
      // reconnect for the remaining queued commands).
      if (in_flight_) {
        if (timer_) loop_.cancel(timer_);
        timer_ = loop_.timer(0, [this] {
          timer_ = 0;
          complete(kRigctlDisconnected, std::string());
        });
      }
    };
    stream_->connect(host_, port_, 3000);
    return;
  }
  in_flight_ = true;
  stream_->write(queue_.front().line);
  timer_ = loop_.timer(3000, [this] {
    timer_ = 0;
    EH_LOGW("rigctld %s:%u: command timeout", host_.c_str(), unsigned(port_));
    if (stream_) stream_->close();
    complete(kRigctlTimeout, std::string());
  });
}

void RigctlClient::on_line(const std::string& raw) {
  if (!in_flight_ || queue_.empty()) return;
  std::string line = trim(raw);
  if (line.empty()) return;
  int code = 0;
  if (rigctl_parse_rprt(line, &code)) {
    complete(code, std::string());
    return;
  }
  // Value line of a get command (lowercase letter commands).
  const std::string& cmd = queue_.front().line;
  if (!cmd.empty() && cmd[0] >= 'a' && cmd[0] <= 'z') complete(0, line);
}

void RigctlClient::complete(int code, const std::string& reply) {
  if (timer_) {
    loop_.cancel(timer_);
    timer_ = 0;
  }
  if (!in_flight_ || queue_.empty()) {
    in_flight_ = false;
    pump();
    return;
  }
  in_flight_ = false;
  last_rprt_ = code;
  Done d = std::move(queue_.front().done);
  queue_.pop_front();
  if (d) d(code, reply);
  pump();
}

void RigctlClient::fail_all(int code) {
  std::deque<Cmd> q;
  q.swap(queue_);
  in_flight_ = false;
  last_rprt_ = code;
  for (auto& c : q)
    if (c.done) c.done(code, std::string());
}

// ---- PTT -----------------------------------------------------------------------------------------

namespace {

class RigctldPtt : public PttControl {
 public:
  RigctldPtt(Loop& loop, const std::string& host, uint16_t port) : client_(loop, host, port) {}
  void set(bool on) override {
    if (have_ && on == on_) return;
    have_ = true;
    on_ = on;
    client_.set_ptt(on, [on](int code, const std::string&) {
      if (code != 0) EH_LOGW("rigctld PTT %s failed (RPRT %d)", on ? "on" : "off", code);
    });
  }
  bool ok() const override { return client_.last_rprt() == 0; }
  std::string describe() const override {
    return "rigctld " + client_.host() + ":" + std::to_string(client_.port());
  }

 private:
  RigctlClient client_;
  bool have_ = false;
  bool on_ = false;
};

}  // namespace

std::unique_ptr<PttControl> make_rigctld_ptt(Loop& loop, const std::string& host, uint16_t port) {
  return std::unique_ptr<PttControl>(new RigctldPtt(loop, host.empty() ? "127.0.0.1" : host, port ? port : 4532));
}

std::unique_ptr<PttControl> make_ptt_from_config(Loop& loop, const Json& cfg, std::string* err) {
  std::string mode = to_lower(cfg_str(cfg, "ptt", "vox"));
  if (mode.empty() || mode == "vox" || mode == "none") return nullptr;
  if (mode == "serial_rts" || mode == "serial_dtr" || mode == "serial_both") {
    std::string port = cfg_str(cfg, "ptt_port");
    if (port.empty()) {
      if (err) *err = "ptt_port is required for serial PTT";
      return nullptr;
    }
#ifdef EMBER_HOST_HAVE_SERIAL
    std::string line = mode == "serial_rts" ? "rts" : mode == "serial_dtr" ? "dtr" : "both";
    return make_serial_line_ptt(port, line, cfg_bool(cfg, "ptt_invert", false), err);
#else
    if (err) *err = "serial support not built";
    return nullptr;
#endif
  }
  if (mode == "cm108" || mode == "cm119" || mode == "hid") {
#ifdef EMBER_HOST_HAVE_AUDIO
    return make_cm108_ptt(cfg_str(cfg, "ptt_device", "auto"), int(cfg_int(cfg, "ptt_gpio", 3)), cfg_bool(cfg, "ptt_invert", false), err);
#else
    if (err) *err = "CM108 HID PTT needs the sound-card build (EMBER_HOST_AUDIO)";
    return nullptr;
#endif
  }
  if (mode == "rigctld" || mode == "cat") {
    int64_t p = cfg_int(cfg, "rig_port", 4532);
    if (p <= 0 || p > 65535) {
      if (err) *err = "bad rig_port";
      return nullptr;
    }
    return make_rigctld_ptt(loop, cfg_str(cfg, "rig_host", "127.0.0.1"), uint16_t(p));
  }
  if (err) *err = "unknown ptt mode '" + mode + "'";
  return nullptr;
}

}  // namespace ember_host
