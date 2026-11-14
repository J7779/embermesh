// rtl_tcp endpoint: TCP client to an rtl_tcp server. A byte pipe like `tcp`: the core's rtl_tcp link
// driver tunes the dongle (its frequency_hz / sample_rate / gain settings, written as rtl_tcp
// commands through this endpoint) and needs the 12-byte "RTL0" server header to go Up, so the
// header and the u8 I/Q stream are passed through unchanged. The endpoint only parses the header
// for its status (tuner name) and adds what the driver does not do: bias tee, direct sampling and,
// with tune:true, its own tuning commands (for use with drivers that do not tune).
#include "host/endpoints/rtl_tcp.h"

#include <memory>

#include "host/endpoint.h"
#include "host/stream.h"
#include "host/util.h"

namespace ember_host {

std::string rtltcp_command(uint8_t cmd, uint32_t param) {
  std::string s(5, '\0');
  s[0] = char(cmd);
  s[1] = char((param >> 24) & 0xFF);
  s[2] = char((param >> 16) & 0xFF);
  s[3] = char((param >> 8) & 0xFF);
  s[4] = char(param & 0xFF);
  return s;
}

static uint32_t be32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

int rtltcp_parse_header(const uint8_t* data, size_t len, RtlTcpHeader* out) {
  // Reject bad magic as early as the bytes allow.
  static const char magic[4] = {'R', 'T', 'L', '0'};
  for (size_t i = 0; i < len && i < 4; ++i)
    if (data[i] != uint8_t(magic[i])) return -1;
  if (len < kRtlTcpHeaderLen) return 0;
  if (out) {
    out->tuner = be32(data + 4);
    out->gain_count = be32(data + 8);
  }
  return 1;
}

const char* rtltcp_tuner_name(uint32_t tuner) {
  switch (tuner) {
    case 1: return "E4000";
    case 2: return "FC0012";
    case 3: return "FC0013";
    case 4: return "FC2580";
    case 5: return "R820T";
    case 6: return "R828D";
    default: return "unknown";
  }
}

bool rtltcp_valid_sample_rate(uint32_t r) {
  return (r >= 225001 && r <= 300000) || (r >= 900001 && r <= 3200000);
}

namespace {

class RtlTcpEndpoint : public Endpoint {
 public:
  RtlTcpEndpoint(Loop& loop, EndpointSink& sink, const Json& cfg) : Endpoint(loop, sink, cfg) {
    host_ = cfg_str(cfg, "host", "127.0.0.1");
    port_ = uint16_t(cfg_int(cfg, "port", 1234));
    freq_ = uint32_t(cfg_int(cfg, "freq_hz", 144390000));
    rate_ = uint32_t(cfg_int(cfg, "sample_rate", 1024000));
    gain_db_ = cfg_num(cfg, "gain_db", 0);
    ppm_ = int32_t(cfg_int(cfg, "ppm", 0));
    agc_ = cfg_bool(cfg, "agc", false);
    bias_ = cfg_bool(cfg, "bias_tee", false);
    direct_ = int32_t(cfg_int(cfg, "direct_sampling", 0));
    tune_ = cfg_bool(cfg, "tune", false);
  }
  ~RtlTcpEndpoint() override { close(); }

  const char* kind() const override { return "rtl_tcp"; }

  void open() override {
    close();
    header_.clear();
    have_header_ = false;
    stream_.reset(new TcpStream(loop_));
    stream_->on_connect = [this](const std::string& err) {
      if (!err.empty()) {
        defer_state(false, err);
        return;
      }
      send_config();
    };
    stream_->on_data = [this](const uint8_t* d, size_t n) { on_bytes(d, n); };
    stream_->on_close = [this](const std::string& reason) { defer_state(false, reason); };
    stream_->connect(host_, port_, 5000);
  }

  void close() override {
    if (state_timer_) {
      loop_.cancel(state_timer_);
      state_timer_ = 0;
    }
    if (stream_) stream_->close();
    stream_.reset();
    connected_ = false;
  }

  void write(const uint8_t* data, size_t len) override {
    if (stream_ && stream_->connected()) stream_->write(data, len);
  }

  Json status() const override {
    Json s = Json::object();
    s.set("host", host_);
    s.set("port", port_);
    s.set("tuner", tuner_.tuner);
    s.set("tuner_name", have_header_ ? rtltcp_tuner_name(tuner_.tuner) : "");
    s.set("gain_count", tuner_.gain_count);
    s.set("freq_hz", freq_);
    s.set("sample_rate", rate_);
    s.set("iq_bytes", double(iq_bytes_));
    return s;
  }

 private:
  void defer_state(bool up, const std::string& err) {
    if (state_timer_) loop_.cancel(state_timer_);
    // Deferred: the host may destroy/reopen us from ep_state, which must not happen while the
    // TcpStream is inside its own callback.
    state_timer_ = loop_.timer(0, [this, up, err] {
      state_timer_ = 0;
      if (!up && stream_) {
        stream_->close();
        stream_.reset();
      }
      set_state(up, err);
    });
  }

  void send_config() {
    std::string c;
    if (direct_ != 0) c += rtltcp_command(kRtlSetDirectSampling, uint32_t(direct_));
    if (bias_) c += rtltcp_command(kRtlSetBiasTee, 1);
    if (!tune_) {
      if (!c.empty()) stream_->write(c);
      return;
    }
    c += rtltcp_command(kRtlSetSampleRate, rate_);
    c += rtltcp_command(kRtlSetFreq, freq_);
    if (ppm_ != 0) c += rtltcp_command_signed(kRtlSetFreqCorrection, ppm_);
    if (gain_db_ == 0) {
      c += rtltcp_command(kRtlSetGainMode, 0);
    } else {
      c += rtltcp_command(kRtlSetGainMode, 1);
      c += rtltcp_command_signed(kRtlSetGain, int32_t(gain_db_ * 10.0 + (gain_db_ >= 0 ? 0.5 : -0.5)));
    }
    c += rtltcp_command(kRtlSetAgcMode, agc_ ? 1 : 0);
    stream_->write(c);
  }

  void on_bytes(const uint8_t* d, size_t n) {
    if (!have_header_) {
      size_t need = kRtlTcpHeaderLen - header_.size();
      size_t take = n < need ? n : need;
      header_.append(reinterpret_cast<const char*>(d), take);
      d += take;
      n -= take;
      header_delivered_ = false;
      int r = rtltcp_parse_header(reinterpret_cast<const uint8_t*>(header_.data()), header_.size(), &tuner_);
      if (r < 0) {
        stream_->set_read_paused(true);
        defer_state(false, "not an rtl_tcp server (bad header magic)");
        return;
      }
      if (r == 0) return;
      have_header_ = true;
      EH_LOGI("rtl_tcp %s:%u: tuner %s, %u gains", host_.c_str(), unsigned(port_), rtltcp_tuner_name(tuner_.tuner),
              unsigned(tuner_.gain_count));
      // Report "up" before the first bytes are delivered, then hand the driver its header.
      set_state(true, "");
      sink().ep_status_changed(*this);
      if (!stream_) return;
      deliver(reinterpret_cast<const uint8_t*>(header_.data()), header_.size());
      header_delivered_ = true;
      if (!stream_) return;
    }
    if (n) {
      iq_bytes_ += n;
      deliver(d, n);
    }
  }

  std::string host_;
  uint16_t port_;
  uint32_t freq_, rate_;
  double gain_db_;
  int32_t ppm_, direct_;
  bool agc_, bias_, tune_ = false, header_delivered_ = false;
  std::unique_ptr<TcpStream> stream_;
  std::string header_;
  bool have_header_ = false;
  RtlTcpHeader tuner_;
  uint64_t iq_bytes_ = 0;
  uint64_t state_timer_ = 0;
};

Json rtl_fields() {
  Json f = Json::array();
  f.push(Field("host", "rtl_tcp host", "text").def("127.0.0.1").required().help("Machine running `rtl_tcp -a 0.0.0.0`"));
  f.push(Field("port", "Port", "number").def(1234).min(1).max(65535));
  f.push(Field("tune", "Tune from these settings", "bool").def(false).advanced().help(
      "Normally the link's own settings (frequency, sample rate, gain) tune the dongle; turn on only for links that do not"));
  f.push(Field("freq_hz", "Frequency", "number").def(144390000).unit("Hz").min(1).advanced().help("Only with 'Tune from these settings'"));
  f.push(Field("sample_rate", "Sample rate", "select")
             .def(240000)
             .advanced()
             .option(240000, "240 ksps")
             .option(250000, "250 ksps")
             .option(1024000, "1.024 Msps")
             .option(1200000, "1.2 Msps")
             .option(1920000, "1.92 Msps")
             .option(2048000, "2.048 Msps")
             .option(2400000, "2.4 Msps")
             .help("225001-300000 or 900001-3200000 samples/s"));
  f.push(Field("gain_db", "Tuner gain", "number").def(0).unit("dB").min(0).max(50).step(0.1).advanced().help("0 = automatic gain"));
  f.push(Field("ppm", "Frequency correction", "number").def(0).unit("ppm").min(-200).max(200).advanced());
  f.push(Field("agc", "RTL2832 digital AGC", "bool").def(false).advanced());
  f.push(Field("bias_tee", "Bias tee (powers an LNA)", "bool").def(false).advanced());
  f.push(Field("direct_sampling", "Direct sampling (HF)", "select")
             .def(0)
             .option(0, "Off")
             .option(1, "I branch")
             .option(2, "Q branch")
             .advanced());
  return f;
}

// Bounded non-blocking connect probe.
bool probe_tcp(const std::string& host, uint16_t port, int timeout_ms) {
  SockAddr a;
  if (!resolve_numeric(host, port, &a)) return false;
  bool inprog = false;
  std::string err;
  sock_t s = tcp_connect_start(a, &inprog, &err);
  if (s == kInvalidSock) return false;
  bool ok = !inprog;
  if (inprog) {
    PollItem it{s, kPollOut, 0};
    uint64_t deadline = steady_ms() + uint64_t(timeout_ms);
    while (true) {
      int64_t left = int64_t(deadline) - int64_t(steady_ms());
      if (left <= 0) break;
      int rc = poll_sockets(&it, 1, int(left));
      if (rc > 0) {
        ok = sock_pending_error(s) == 0 && (it.revents & kPollOut) && !(it.revents & kPollErr);
        break;
      }
      if (rc < 0) break;
    }
  }
  sock_close(s);
  return ok;
}

}  // namespace

void register_sdr_endpoints(EndpointRegistry& r) {
  EndpointKind k;
  k.kind = "rtl_tcp";
  k.title = "RTL-SDR (rtl_tcp server)";
  k.description = "Receive-only software radio: connects to an rtl_tcp server and feeds raw I/Q to the core's demodulator.";
  k.scannable = true;
  k.config = rtl_fields();
  k.link_types = {"rtl_tcp"};
  k.default_link_type = "rtl_tcp";
  Json fields = k.config;
  k.create = [fields](Loop& loop, EndpointSink& sink, const Json& cfg_in, std::string* error) -> std::unique_ptr<Endpoint> {
    Json cfg = cfg_with_defaults(fields, cfg_in);
    if (cfg_str(cfg, "host").empty()) {
      if (error) *error = "host is required";
      return nullptr;
    }
    int64_t port = cfg_int(cfg, "port", 1234);
    if (port < 1 || port > 65535) {
      if (error) *error = "port must be 1-65535";
      return nullptr;
    }
    int64_t freq = cfg_int(cfg, "freq_hz", 0);
    if (freq <= 0 || freq > 0xFFFFFFFFLL) {
      if (error) *error = "freq_hz out of range";
      return nullptr;
    }
    int64_t rate = cfg_int(cfg, "sample_rate", 0);
    if (rate <= 0 || rate > 0xFFFFFFFFLL || !rtltcp_valid_sample_rate(uint32_t(rate))) {
      if (error) *error = "sample_rate must be 225001-300000 or 900001-3200000";
      return nullptr;
    }
    int64_t ds = cfg_int(cfg, "direct_sampling", 0);
    if (ds < 0 || ds > 2) {
      if (error) *error = "direct_sampling must be 0, 1 or 2";
      return nullptr;
    }
    return std::unique_ptr<Endpoint>(new RtlTcpEndpoint(loop, sink, cfg));
  };
  k.scan = [](const Json& params) -> Json {
    int64_t t = cfg_int(params, "timeout_ms", 300);
    if (t > 500) t = 500;
    if (t < 20) t = 20;
    Json devices = Json::array();
    if (probe_tcp("127.0.0.1", 1234, int(t))) {
      Json d = Json::object();
      d.set("id", "127.0.0.1:1234");
      d.set("name", "rtl_tcp on this computer");
      d.set("detail", "127.0.0.1:1234");
      Json c = Json::object();
      c.set("host", "127.0.0.1");
      c.set("port", 1234);
      d.set("config", c);
      devices.push(d);
    }
    Json out = Json::object();
    out.set("devices", devices);
    return out;
  };
  r.add(std::move(k));
}

}  // namespace ember_host
