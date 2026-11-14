// Sound-card endpoint: miniaudio capture + playback devices feeding the core's soft-modem link
// (ember_audio_rx / ember_audio_tx through EndpointSink), plus transmitter keying (VOX, serial
// RTS/DTR, rigctld). No modem logic here.
//
// Threads: miniaudio runs each device's data callback on its own audio thread; those callbacks only
// call the thread-safe sink audio hooks, apply gain in place / into a preallocated scratch buffer
// and update atomics. Everything else (open/close, PTT, rigctld) happens on the loop thread.
#include "host/endpoints/audio.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include "miniaudio/miniaudio.h"

#include "host/capture.h"
#include "host/endpoint.h"
#include "host/endpoints/ptt.h"
#include "host/endpoints/rigctl.h"
#include "host/util.h"

namespace ember_host {

namespace {

std::mutex g_ctx_mu;
ma_context* g_ctx = nullptr;  // process lifetime, created lazily
std::string g_ctx_error;
// Device opens that exceeded their timeout and are still blocked in the OS (typically the macOS
// microphone-permission prompt that nobody answers). While one is stuck, new opens fail at once
// instead of piling up more blocked threads.
std::atomic<int> g_stuck_opens{0};
std::mutex g_stuck_mu;
std::string g_stuck_error;

// Caller holds g_ctx_mu.
ma_context* context_locked(std::string* err) {
  if (g_ctx) return g_ctx;
  if (!g_ctx_error.empty()) {
    if (err) *err = g_ctx_error;
    return nullptr;
  }
  ma_context* c = new ma_context;
  ma_result r = ma_context_init(nullptr, 0, nullptr, c);
  if (r != MA_SUCCESS) {
    delete c;
    g_ctx_error = std::string("audio backend unavailable: ") + ma_result_description(r);
    if (err) *err = g_ctx_error;
    return nullptr;
  }
  g_ctx = c;
  return g_ctx;
}

bool icontains(const std::string& hay, const std::string& needle) {
  return to_lower(hay).find(to_lower(needle)) != std::string::npos;
}

// Finds a device id by exact name, then case-insensitive substring. Caller holds g_ctx_mu.
bool find_device(ma_context* ctx, bool capture, const std::string& name, ma_device_id* id, std::string* found) {
  ma_device_info *play = nullptr, *cap = nullptr;
  ma_uint32 np = 0, nc = 0;
  if (ma_context_get_devices(ctx, &play, &np, &cap, &nc) != MA_SUCCESS) return false;
  ma_device_info* list = capture ? cap : play;
  ma_uint32 n = capture ? nc : np;
  for (ma_uint32 i = 0; i < n; ++i)
    if (name == list[i].name) {
      *id = list[i].id;
      *found = list[i].name;
      return true;
    }
  for (ma_uint32 i = 0; i < n; ++i)
    if (icontains(list[i].name, name)) {
      *id = list[i].id;
      *found = list[i].name;
      return true;
    }
  return false;
}

float db_to_gain(double db) {
  if (db < -40) db = -40;
  if (db > 40) db = 40;
  return float(std::pow(10.0, db / 20.0));
}

inline int16_t sat16(float v) {
  if (v > 32767.f) return 32767;
  if (v < -32768.f) return -32768;
  return int16_t(std::lrintf(v));
}

constexpr size_t kScratch = 4096;

// Device initialisation runs on a worker thread: on macOS ma_device_init() for a capture device
// blocks inside CoreAudio until the user answers the microphone-permission prompt (forever on a
// headless box), and it must never freeze the loop thread. The job hands the initialised (not yet
// started) devices back to the loop thread, which starts them only if the endpoint still wants them.
struct OpenJob {
  std::mutex mu;
  Loop* loop = nullptr;  // nulled (under mu) when the endpoint cancels / dies
  // inputs
  std::string in_name, out_name;
  uint32_t rate = 48000;
  void* user = nullptr;
  ma_device_data_proc cap_cb = nullptr, play_cb = nullptr;
  ma_device_notification_proc notify_cb = nullptr;
  bool fallback_default = false;  // a named device that is missing: use the system default instead of failing
  // outputs
  std::unique_ptr<ma_device> cap, play;
  bool cap_init = false, play_init = false;
  bool in_fallback = false, out_fallback = false;
  bool timed_out = false;  // under mu
  std::string in_actual, out_actual, err;

  void uninit() {
    if (cap_init) ma_device_uninit(cap.get());
    if (play_init) ma_device_uninit(play.get());
    cap_init = play_init = false;
    cap.reset();
    play.reset();
  }
  ~OpenJob() { uninit(); }
};

bool disabled_name(const std::string& n) {
  std::string l = to_lower(n);
  return l == "none" || l == "off" || l == "disabled";
}

bool init_direction(OpenJob& j, bool capture) {
  const std::string& want = capture ? j.in_name : j.out_name;
  ma_context* ctx = nullptr;
  ma_device_id id;
  bool have_id = false;
  {
    std::lock_guard<std::mutex> lk(g_ctx_mu);
    ctx = context_locked(&j.err);
    if (!ctx) return false;
    if (!want.empty()) {
      std::string found;
      have_id = find_device(ctx, capture, want, &id, &found);
      if (!have_id) {
        if (!j.fallback_default) {
          // Usually an unplugged USB interface: fail, and the host retries with backoff until it is back.
          j.err = std::string(capture ? "input" : "output") + " device '" + want + "' not found (unplugged?)";
          return false;
        }
        EH_LOGW("audio: %s device '%s' not found, using system default", capture ? "input" : "output", want.c_str());
        (capture ? j.in_fallback : j.out_fallback) = true;
      }
    }
  }
  ma_device_config dc = ma_device_config_init(capture ? ma_device_type_capture : ma_device_type_playback);
  if (capture) {
    dc.capture.pDeviceID = have_id ? &id : nullptr;
    dc.capture.format = ma_format_s16;
    dc.capture.channels = 1;
    dc.dataCallback = j.cap_cb;
  } else {
    dc.playback.pDeviceID = have_id ? &id : nullptr;
    dc.playback.format = ma_format_s16;
    dc.playback.channels = 1;
    dc.dataCallback = j.play_cb;
  }
  dc.sampleRate = j.rate;
  dc.pUserData = j.user;
  dc.notificationCallback = j.notify_cb;
  std::unique_ptr<ma_device> dev(new ma_device);
  ma_result r = ma_device_init(ctx, &dc, dev.get());
  if (r != MA_SUCCESS) {
    j.err = std::string(capture ? "input device: " : "output device: ") + ma_result_description(r);
    return false;
  }
  ma_device_info info;
  std::string actual;
  if (ma_device_get_info(dev.get(), capture ? ma_device_type_capture : ma_device_type_playback, &info) == MA_SUCCESS)
    actual = info.name;
  if (capture) {
    j.cap = std::move(dev);
    j.cap_init = true;
    j.in_actual = actual;
  } else {
    j.play = std::move(dev);
    j.play_init = true;
    j.out_actual = actual;
  }
  return true;
}

void run_open_job(OpenJob& j) {
  if (!disabled_name(j.in_name) && !init_direction(j, true)) return;
  if (!disabled_name(j.out_name) && !init_direction(j, false)) return;
  if (!j.cap_init && !j.play_init) j.err = "both input and output are disabled";
}

class AudioEndpoint : public Endpoint {
 public:
  AudioEndpoint(Loop& loop, EndpointSink& sink, const Json& cfg) : Endpoint(loop, sink, cfg) {
    in_name_ = cfg_str(cfg, "input_device");
    out_name_ = cfg_str(cfg, "output_device");
    rate_ = uint32_t(cfg_int(cfg, "sample_rate", 48000));
    fallback_default_ = cfg_bool(cfg, "fallback_default", false);
    int64_t to = cfg_int(cfg, "open_timeout_ms", 8000);
    open_timeout_ms_ = uint64_t(to < 500 ? 500 : to > 120000 ? 120000 : to);
    rx_gain_ = db_to_gain(cfg_num(cfg, "rx_gain_db", 0));
    tx_gain_ = db_to_gain(cfg_num(cfg, "tx_gain_db", 0));
    ptt_mode_ = to_lower(cfg_str(cfg, "ptt", "vox"));
    ptt_auto_ = cfg_bool(cfg, "ptt_auto", false);
    int64_t tail = cfg_int(cfg, "ptt_tail_ms", 150);
    ptt_tail_ms_ = uint64_t(tail < 0 ? 0 : tail > 5000 ? 5000 : tail);
    scratch_.resize(kScratch);
    alive_ = std::make_shared<bool>(true);
  }
  ~AudioEndpoint() override {
    *alive_ = false;
    close();
  }

  const char* kind() const override { return "audio"; }
  bool is_audio() const override { return true; }
  bool auto_reconnect() const override { return true; }

  void open() override {
    close();
    if (g_stuck_opens.load() > 0) {
      std::string why;
      {
        std::lock_guard<std::mutex> lk(g_stuck_mu);
        why = g_stuck_error;
      }
      std::weak_ptr<bool> alive = alive_;
      open_timer_ = loop_.timer(0, [this, alive, why] {
        auto a = alive.lock();
        if (!a || !*a) return;
        open_timer_ = 0;
        set_state(false, why + " (an earlier open is still blocked)");
      });
      return;
    }
    auto job = std::make_shared<OpenJob>();
    job->loop = &loop_;
    job->in_name = in_name_;
    job->out_name = out_name_;
    job->rate = rate_;
    job->user = this;
    job->cap_cb = &AudioEndpoint::capture_cb;
    job->play_cb = &AudioEndpoint::playback_cb;
    job->notify_cb = &AudioEndpoint::notify_cb;
    job->fallback_default = fallback_default_;
    job_ = job;
    opening_ = true;
    std::weak_ptr<bool> alive = alive_;
    open_timer_ = loop_.timer(open_timeout_ms_, [this, alive, job] {
      auto a = alive.lock();
      if (!a || !*a || job_ != job) return;
      open_timer_ = 0;
      std::string why = format(
          "the sound device did not open within %u s. On macOS this usually means the microphone "
          "permission prompt is waiting: allow microphone access for the program that runs emberd "
          "(System Settings > Privacy & Security > Microphone), or set input_device to \"none\" for "
          "transmit only, then reconnect",
          unsigned(open_timeout_ms_ / 1000));
      {
        std::lock_guard<std::mutex> lk(job->mu);
        job->loop = nullptr;  // the worker uninitialises whatever it gets once the OS returns
        job->timed_out = true;
      }
      {
        std::lock_guard<std::mutex> lk(g_stuck_mu);
        g_stuck_error = why;
      }
      g_stuck_opens.fetch_add(1);
      EH_LOGW("audio: %s", why.c_str());
      job_.reset();
      opening_ = false;
      set_state(false, why);
    });
    std::thread([job, alive, this] {
      run_open_job(*job);
      std::lock_guard<std::mutex> lk(job->mu);
      if (job->timed_out) {
        g_stuck_opens.fetch_sub(1);
        EH_LOGI("audio: a device open that had timed out returned (%s)", job->err.empty() ? "ok" : job->err.c_str());
      }
      if (!job->loop) {
        job->uninit();  // cancelled while we were blocked
        return;
      }
      job->loop->post([job, alive, this] {
        auto a = alive.lock();
        if (!a || !*a || job_ != job) {
          job->uninit();
          return;
        }
        finish_open();
      });
    }).detach();
  }

  void close() override {
    if (job_) {
      std::lock_guard<std::mutex> lk(job_->mu);
      job_->loop = nullptr;
    }
    job_.reset();  // a finished-but-unstarted job uninits its devices in ~OpenJob
    opening_ = false;
    if (open_timer_) {
      loop_.cancel(open_timer_);
      open_timer_ = 0;
    }
    if (ptt_timer_) {
      loop_.cancel(ptt_timer_);
      ptt_timer_ = 0;
    }
    close_devices();
    if (ptt_ && ptt_on_) ptt_->set(false);
    ptt_on_ = false;
    ptt_.reset();
    rig_.reset();
    connected_ = false;
  }

  void write(const uint8_t*, size_t) override {}  // audio links exchange PCM, not bytes

  void set_ptt(bool on) override {
    event_ptt_ = on;
    apply_ptt(on || auto_active());
  }

  Json status() const override {
    Json s = Json::object();
    s.set("input_device", in_actual_.empty() ? in_name_ : in_actual_);
    s.set("output_device", out_actual_.empty() ? out_name_ : out_actual_);
    if (in_fallback_) s.set("input_fallback", true);
    if (out_fallback_) s.set("output_fallback", true);
    if (opening_) s.set("opening", true);
    s.set("sample_rate", rate_);
    s.set("ptt", ptt_mode_);
    s.set("ptt_on", ptt_on_);
    int peak = peak_.load();
    double dbfs = peak <= 0 ? -120.0 : 20.0 * std::log10(double(peak) / 32768.0);
    s.set("rx_level_dbfs", std::round(dbfs * 10) / 10);
    s.set("overruns", double(overruns_.load()));
    return s;
  }

 private:
  // Loop thread: the worker finished initialising (or failed).
  void finish_open() {
    std::shared_ptr<OpenJob> job = job_;
    opening_ = false;
    if (open_timer_) {
      loop_.cancel(open_timer_);
      open_timer_ = 0;
    }
    std::string err = job->err;
    in_actual_ = job->in_actual;
    out_actual_ = job->out_actual;
    in_fallback_ = job->in_fallback;
    out_fallback_ = job->out_fallback;
    if (err.empty()) {
      cap_ = std::move(job->cap);
      play_ = std::move(job->play);
      cap_init_ = job->cap_init;
      play_init_ = job->play_init;
      job->cap_init = job->play_init = false;
    }
    job_.reset();
    if (err.empty() && !start_now(&err)) {
      // fallthrough to failure
    }
    if (!err.empty()) {
      close_devices();
      ptt_.reset();
      rig_.reset();
      set_state(false, err);
      return;
    }
    running_ = true;
    set_state(true);
    schedule_ptt_timer();
  }

  bool start_now(std::string* err) {
    // Keying + optional one-shot radio setup before audio starts.
    std::string perr;
    ptt_ = make_ptt_from_config(loop_, config_, &perr);
    if (!perr.empty()) {
      *err = "PTT: " + perr;
      return false;
    }
    uint64_t rf = uint64_t(cfg_int(config_, "rig_freq_hz", 0));
    std::string rm = cfg_str(config_, "rig_mode");
    if (rf > 0 || !rm.empty()) {
      int64_t rp = cfg_int(config_, "rig_port", 4532);
      rig_.reset(new RigctlClient(loop_, cfg_str(config_, "rig_host", "127.0.0.1"),
                                  uint16_t(rp > 0 && rp <= 65535 ? rp : 4532)));
      if (rf > 0)
        rig_->set_freq(rf, [rf](int code, const std::string&) {
          if (code != 0) EH_LOGW("rigctld: set frequency %llu failed (%d)", (unsigned long long)rf, code);
        });
      if (!rm.empty())
        rig_->set_mode(rm, 0, [rm](int code, const std::string&) {
          if (code != 0) EH_LOGW("rigctld: set mode %s failed (%d)", rm.c_str(), code);
        });
    }
    if (cap_init_) {
      ma_result r = ma_device_start(cap_.get());
      if (r != MA_SUCCESS) {
        *err = std::string("start input: ") + ma_result_description(r);
        return false;
      }
    }
    if (play_init_) {
      ma_result r = ma_device_start(play_.get());
      if (r != MA_SUCCESS) {
        *err = std::string("start output: ") + ma_result_description(r);
        return false;
      }
    }
    EH_LOGI("audio: in '%s' out '%s' @ %u Hz, ptt %s", in_actual_.c_str(), out_actual_.c_str(), unsigned(rate_),
            ptt_ ? ptt_->describe().c_str() : ptt_mode_.c_str());
    return true;
  }

  void close_devices() {
    running_ = false;  // a "stopped" notification from our own uninit is not a device loss
    // ma_device_uninit stops the device and waits for its callback to return.
    if (cap_init_) ma_device_uninit(cap_.get());
    if (play_init_) ma_device_uninit(play_.get());
    cap_init_ = play_init_ = false;
    cap_.reset();
    play_.reset();
  }

  // ---- audio threads ----
  // Device unplugged / stopped by the OS: drop the endpoint (the host reconnects with backoff and
  // the device is found again by name once it is back).
  static void notify_cb(const ma_device_notification* n) {
    if (n->type != ma_device_notification_type_stopped) return;
    auto* self = static_cast<AudioEndpoint*>(n->pDevice->pUserData);
    if (!self->running_.load()) return;
    self->running_ = false;
    std::weak_ptr<bool> w = self->alive_;
    self->loop_.post([self, w] {
      auto a = w.lock();
      if (!a || !*a) return;
      EH_LOGW("audio: sound device stopped (unplugged?)");
      self->close_devices();
      self->set_state(false, "the sound device stopped (unplugged?); reconnecting when it is back");
    });
  }
  static void capture_cb(ma_device* dev, void*, const void* input, ma_uint32 frames) {
    auto* self = static_cast<AudioEndpoint*>(dev->pUserData);
    if (!input || frames == 0) return;
    self->on_capture(static_cast<const int16_t*>(input), frames);
  }
  void on_capture(const int16_t* in, size_t n) {
    size_t off = 0;
    while (off < n) {
      size_t c = std::min(kScratch, n - off);
      int16_t* s = scratch_.data();
      for (size_t i = 0; i < c; ++i) {
        int16_t v = rx_gain_ == 1.f ? in[off + i] : sat16(float(in[off + i]) * rx_gain_);
        s[i] = v;
        int a = v < 0 ? -int(v) : int(v);
        if (a > win_peak_) win_peak_ = a;
      }
      sink().ep_audio_rx(*this, s, c, rate_);
      off += c;
    }
    win_count_ += n;
    if (win_count_ >= rate_ / 2) {
      peak_.store(win_peak_);
      win_peak_ = 0;
      win_count_ = 0;
    }
  }

  static void playback_cb(ma_device* dev, void* output, const void*, ma_uint32 frames) {
    auto* self = static_cast<AudioEndpoint*>(dev->pUserData);
    if (!output || frames == 0) return;
    self->on_playback(static_cast<int16_t*>(output), frames);
  }
  void on_playback(int16_t* out, size_t n) {
    size_t got = sink().ep_audio_tx(*this, out, n, rate_);
    if (got > n) got = n;
    if (got < n) std::memset(out + got, 0, (n - got) * sizeof(int16_t));
    bool loud = false;
    for (size_t i = 0; i < got; ++i) {
      if (tx_gain_ != 1.f) out[i] = sat16(float(out[i]) * tx_gain_);
      if (out[i] > 64 || out[i] < -64) loud = true;
    }
    // While transmitting the driver must fill the whole buffer; a short fill is a TX underrun.
    if (got > 0 && got < n && was_tx_) overruns_.fetch_add(1);
    was_tx_ = got > 0;
    if (loud) last_tx_ms_.store(steady_ms());
  }

  // ---- PTT (loop thread) ----
  bool auto_active() const {
    if (!ptt_auto_) return false;
    uint64_t t = last_tx_ms_.load();
    return t != 0 && steady_ms() - t <= ptt_tail_ms_;
  }
  void apply_ptt(bool on) {
    if (on == ptt_on_) return;
    ptt_on_ = on;
    if (ptt_) ptt_->set(on);
    sink().ep_status_changed(*this);
  }
  void schedule_ptt_timer() {
    if (!ptt_auto_) return;
    ptt_timer_ = loop_.timer(20, [this] {
      ptt_timer_ = 0;
      apply_ptt(event_ptt_ || auto_active());
      schedule_ptt_timer();
    });
  }

  std::string in_name_, out_name_, in_actual_, out_actual_;
  bool in_fallback_ = false, out_fallback_ = false;
  uint32_t rate_;
  float rx_gain_, tx_gain_;
  std::string ptt_mode_;
  bool ptt_auto_;
  uint64_t ptt_tail_ms_;
  std::unique_ptr<ma_device> cap_, play_;
  bool cap_init_ = false, play_init_ = false;
  std::vector<int16_t> scratch_;  // capture thread only
  int win_peak_ = 0;              // capture thread only
  size_t win_count_ = 0;          // capture thread only
  bool was_tx_ = false;           // playback thread only
  std::atomic<int> peak_{0};
  std::atomic<uint64_t> overruns_{0};
  std::atomic<uint64_t> last_tx_ms_{0};
  std::unique_ptr<PttControl> ptt_;
  std::unique_ptr<RigctlClient> rig_;
  bool ptt_on_ = false;
  bool event_ptt_ = false;
  bool opening_ = false;
  bool fallback_default_ = false;
  std::atomic<bool> running_{false};
  uint64_t ptt_timer_ = 0;
  uint64_t open_timer_ = 0;
  uint64_t open_timeout_ms_ = 8000;
  std::shared_ptr<OpenJob> job_;
  std::shared_ptr<bool> alive_;
};

Json audio_fields() {
  Json f = Json::array();
  f.push(Field("input_device", "Input (from radio)", "text").def("").help(
      "Capture device name (Scan lists them); empty = system default, \"none\" = transmit only"));
  f.push(Field("output_device", "Output (to radio)", "text").def("").help(
      "Playback device name; empty = system default, \"none\" = receive only"));
  f.push(Field("sample_rate", "Sample rate", "select")
             .def(48000)
             .unit("Hz")
             .option(8000, "8000")
             .option(11025, "11025")
             .option(16000, "16000")
             .option(22050, "22050")
             .option(44100, "44100")
             .option(48000, "48000"));
  f.push(Field("ptt", "Transmit keying (PTT)", "select")
             .def("vox")
             .option("vox", "VOX (radio keys itself on audio)")
             .option("serial_rts", "Serial port RTS line")
             .option("serial_dtr", "Serial port DTR line")
             .option("cm108", "USB sound-card GPIO (CM108/CM119 HID, AIOC)")
             .option("rigctld", "CAT via Hamlib rigctld")
             .option("none", "None (receive only)"));
  f.push(Field("ptt_port", "PTT serial port", "text").def("").help(
      "For serial RTS/DTR: e.g. /dev/ttyUSB0 (Digirig: its CP210x port), /dev/ttyACM0 (AIOC), /dev/cu.usbserial-1410, COM3"));
  f.push(Field("ptt_device", "PTT HID device", "text").def("auto").advanced().help(
      "For CM108/CM119 GPIO: auto, or /dev/hidrawN (Linux) / VID:PID (macOS)"));
  f.push(Field("ptt_gpio", "PTT GPIO pin", "number").def(3).min(1).max(8).advanced().help("CM108 GPIO pin wired to PTT (usually 3)"));
  f.push(Field("ptt_invert", "Invert PTT line", "bool").def(false).advanced());
  f.push(Field("rig_host", "rigctld host", "text").def("127.0.0.1").advanced());
  f.push(Field("rig_port", "rigctld port", "number").def(4532).min(1).max(65535).advanced());
  f.push(Field("rig_freq_hz", "Set radio frequency on start", "number").def(0).unit("Hz").min(0).advanced().help(
      "Via rigctld; 0 = leave the radio alone"));
  f.push(Field("rig_mode", "Set radio mode on start", "text").def("").advanced().help("e.g. FM, USB, PKTUSB (via rigctld)"));
  f.push(Field("rx_gain_db", "Receive gain", "number").def(0).unit("dB").min(-40).max(40).step(0.5).advanced());
  f.push(Field("tx_gain_db", "Transmit gain", "number").def(0).unit("dB").min(-40).max(40).step(0.5).advanced());
  f.push(Field("ptt_auto", "Key PTT whenever audio is sent", "bool").def(false).advanced().help(
      "For drivers that do not request PTT explicitly"));
  f.push(Field("ptt_tail_ms", "PTT tail", "number").def(150).unit("ms").min(0).max(5000).advanced());
  f.push(Field("fallback_default", "Use the default device if the named one is missing", "bool").def(false).advanced().help(
      "Off: a missing device (unplugged USB interface) is an error and is retried until it is back"));
  f.push(Field("open_timeout_ms", "Device open timeout", "number").def(8000).unit("ms").min(500).max(120000).advanced().help(
      "Give up (and report why) if the sound device does not open in time, e.g. a pending OS permission prompt"));
  return f;
}

}  // namespace

// ---- capture source for the Morse reader (host.capture.start kind mic_pcm) ------------------------

namespace {
struct MicState {
  std::mutex mu;
  bool abandoned = false;  // the owner is gone (or the open timed out): uninit when init returns
  bool started = false;
  bool reported = false;
  std::unique_ptr<ma_device> dev;
  PcmFn on_pcm;
  PcmErrFn on_error;
  void report(const std::string& e) {
    // caller holds mu
    if (reported || abandoned) return;
    reported = true;
    if (on_error) on_error(e);
  }
};

void mic_cb(ma_device* d, void*, const void* input, ma_uint32 frames) {
  auto* st = static_cast<MicState*>(d->pUserData);
  if (input && frames && st->on_pcm) st->on_pcm(static_cast<const int16_t*>(input), frames);
}

void mic_notify(const ma_device_notification* n) {
  if (n->type != ma_device_notification_type_stopped) return;
  auto* st = static_cast<MicState*>(n->pDevice->pUserData);
  // Called on a miniaudio thread, possibly while the owner is uninitialising: only report if live.
  std::unique_lock<std::mutex> lk(st->mu, std::try_to_lock);
  if (lk.owns_lock() && st->started) st->report("the input device stopped (unplugged?)");
}

class MicSource : public PcmSource {
 public:
  explicit MicSource(std::shared_ptr<MicState> st) : st_(std::move(st)) {}
  ~MicSource() override {
    std::unique_ptr<ma_device> dev;
    {
      std::lock_guard<std::mutex> lk(st_->mu);
      st_->abandoned = true;
      if (st_->started) dev = std::move(st_->dev);
      st_->started = false;
    }
    if (dev) ma_device_uninit(dev.get());  // waits for the data callback to return
  }

 private:
  std::shared_ptr<MicState> st_;
};
}  // namespace

std::unique_ptr<PcmSource> open_mic_source(const std::string& device, uint32_t rate, uint32_t timeout_ms, PcmFn on_pcm,
                                           PcmErrFn on_error) {
  auto st = std::make_shared<MicState>();
  st->on_pcm = std::move(on_pcm);
  st->on_error = std::move(on_error);
  if (g_stuck_opens.load() > 0) {
    std::lock_guard<std::mutex> lk(st->mu);
    st->report("the sound system is still blocked by an earlier device open (on macOS: microphone permission?)");
    return std::unique_ptr<PcmSource>(new MicSource(st));
  }
  std::thread([st, device, rate] {
    std::string err;
    ma_context* ctx;
    ma_device_id id;
    bool have_id = false;
    {
      std::lock_guard<std::mutex> lk(g_ctx_mu);
      ctx = context_locked(&err);
      if (ctx && !device.empty() && !disabled_name(device)) {
        std::string found;
        have_id = find_device(ctx, true, device, &id, &found);
        if (!have_id) err = "input device '" + device + "' not found (unplugged?)";
      }
    }
    std::unique_ptr<ma_device> dev;
    if (ctx && err.empty()) {
      ma_device_config dc = ma_device_config_init(ma_device_type_capture);
      dc.capture.pDeviceID = have_id ? &id : nullptr;
      dc.capture.format = ma_format_s16;
      dc.capture.channels = 1;
      dc.sampleRate = rate;
      dc.dataCallback = mic_cb;
      dc.notificationCallback = mic_notify;
      dc.pUserData = st.get();
      dev.reset(new ma_device);
      ma_result r = ma_device_init(ctx, &dc, dev.get());
      if (r != MA_SUCCESS) {
        err = std::string("input device: ") + ma_result_description(r);
        dev.reset();
      }
    }
    std::lock_guard<std::mutex> lk(st->mu);
    if (!dev) {
      st->report(err);
      return;
    }
    if (st->abandoned) {
      ma_device_uninit(dev.get());
      return;
    }
    st->dev = std::move(dev);
    st->started = true;  // before start: the data callback may run immediately
    if (ma_device_start(st->dev.get()) != MA_SUCCESS) {
      st->started = false;
      ma_device_uninit(st->dev.get());
      st->dev.reset();
      st->report("could not start the input device");
    }
  }).detach();
  // Watchdog: a device open blocked by an unanswered OS permission prompt.
  std::thread([st, timeout_ms] {
    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
    std::lock_guard<std::mutex> lk(st->mu);
    if (!st->started && !st->reported && !st->abandoned)
      st->report(format("the input device did not open within %u s (on macOS: allow microphone access for the program "
                        "running emberd in System Settings > Privacy & Security > Microphone)",
                        unsigned(timeout_ms / 1000)));
  }).detach();
  return std::unique_ptr<PcmSource>(new MicSource(st));
}

std::vector<AudioDeviceInfo> enumerate_audio_devices(std::string* error) {
  std::vector<AudioDeviceInfo> r;
  std::lock_guard<std::mutex> lk(g_ctx_mu);
  ma_context* ctx = context_locked(error);
  if (!ctx) return r;
  ma_device_info *play = nullptr, *cap = nullptr;
  ma_uint32 np = 0, nc = 0;
  ma_result rc = ma_context_get_devices(ctx, &play, &np, &cap, &nc);
  if (rc != MA_SUCCESS) {
    if (error) *error = ma_result_description(rc);
    return r;
  }
  for (ma_uint32 i = 0; i < nc; ++i) r.push_back(AudioDeviceInfo{cap[i].name, true, cap[i].isDefault != 0});
  for (ma_uint32 i = 0; i < np; ++i) r.push_back(AudioDeviceInfo{play[i].name, false, play[i].isDefault != 0});
  return r;
}

void register_audio_endpoints(EndpointRegistry& reg) {
  EndpointKind k;
  k.kind = "audio";
  k.title = "Sound card (radio audio)";
  k.description =
      "Radio audio through a sound card or USB audio interface (or acoustic coupling) for the core's soft modems; "
      "PTT by VOX, serial RTS/DTR or rigctld.";
  k.scannable = true;
  k.config = audio_fields();
  k.link_types = {"audio"};
  k.default_link_type = "audio";
  Json fields = k.config;
  k.create = [fields](Loop& loop, EndpointSink& sink, const Json& cfg_in, std::string* error) -> std::unique_ptr<Endpoint> {
    Json cfg = cfg_with_defaults(fields, cfg_in);
    int64_t rate = cfg_int(cfg, "sample_rate", 48000);
    if (rate < 8000 || rate > 192000) {
      if (error) *error = "sample_rate must be 8000-192000";
      return nullptr;
    }
    std::string ptt = to_lower(cfg_str(cfg, "ptt", "vox"));
    if (ptt != "vox" && ptt != "none" && ptt != "serial_rts" && ptt != "serial_dtr" && ptt != "serial_both" &&
        ptt != "rigctld" && ptt != "cat" && ptt != "cm108" && ptt != "cm119" && ptt != "hid") {
      if (error) *error = "unknown ptt mode '" + ptt + "'";
      return nullptr;
    }
    if ((ptt == "serial_rts" || ptt == "serial_dtr" || ptt == "serial_both") && cfg_str(cfg, "ptt_port").empty()) {
      if (error) *error = "ptt_port is required for serial PTT";
      return nullptr;
    }
    return std::unique_ptr<Endpoint>(new AudioEndpoint(loop, sink, cfg));
  };
  k.scan = [](const Json& params) -> Json {
    // Enumeration runs on a worker with a deadline: CoreAudio can block it behind a device open
    // that is waiting on the macOS microphone-permission prompt, and the loop thread must not hang.
    struct Box {
      std::mutex mu;
      std::condition_variable cv;
      bool done = false;
      std::vector<AudioDeviceInfo> devs;
      std::string err;
    };
    static std::atomic<int> stuck{0};
    int64_t t = cfg_int(params, "timeout_ms", 3000);
    t = t < 200 ? 200 : t > 10000 ? 10000 : t;
    std::string err;
    std::vector<AudioDeviceInfo> list;
    if (stuck.load() > 0) {
      err = "the sound system is not answering (an earlier device query is still blocked; on macOS check the microphone permission)";
    } else {
      auto box = std::make_shared<Box>();
      std::thread([box] {
        std::string e;
        auto r = enumerate_audio_devices(&e);
        std::lock_guard<std::mutex> lk(box->mu);
        box->devs = std::move(r);
        box->err = e;
        box->done = true;
        box->cv.notify_all();
      }).detach();
      std::unique_lock<std::mutex> lk(box->mu);
      if (box->cv.wait_for(lk, std::chrono::milliseconds(t), [&] { return box->done; })) {
        list = box->devs;
        err = box->err;
      } else {
        err = "the sound system did not answer within " + std::to_string(t) +
              " ms (on macOS: is a microphone-permission prompt waiting?)";
        stuck.fetch_add(1);
        std::thread([box] {  // clears the flag once the blocked query finally returns
          std::unique_lock<std::mutex> l2(box->mu);
          box->cv.wait(l2, [&] { return box->done; });
          stuck.fetch_sub(1);
        }).detach();
      }
    }
    Json devices = Json::array();
    for (const auto& d : list) {
      Json j = Json::object();
      j.set("id", std::string(d.capture ? "in:" : "out:") + d.name);
      j.set("name", d.name);
      j.set("detail", std::string(d.capture ? "input" : "output") + (d.is_default ? " (default)" : ""));
      Json c = Json::object();
      c.set(d.capture ? "input_device" : "output_device", d.name);
      j.set("config", c);
      devices.push(j);
    }
    Json out = Json::object();
    out.set("devices", devices);
    if (!err.empty()) out.set("error", err);
    // Everything the "Connect a radio" wizard can key PTT with, as ready-made audio endpoint config.
    Json ptt = Json::array();
    auto add = [&](const std::string& id, const std::string& name, const std::string& detail, Json c) {
      Json j = Json::object();
      j.set("id", id);
      j.set("name", name);
      if (!detail.empty()) j.set("detail", detail);
      j.set("config", c);
      ptt.push(j);
    };
    add("vox", "VOX (the radio keys itself on audio)", "No cable for PTT; use the handheld_vox preset", Json::parse_or_null(R"({"ptt":"vox"})"));
#ifdef EMBER_HOST_HAVE_SERIAL
    Json sp = scan_serial(Json::object());
    for (const auto& d : sp.get("devices").elements()) {
      std::string port = d.get("config").get("port").as_string();
      Json c = d.get("audio_ptt_config");
      if (!c.is_object()) c = Json::parse_or_null(R"({"ptt":"serial_rts"})"), c.set("ptt_port", port);
      add("serial:" + port, d.get("name").as_string(port) + " (" + port + ")", d.get("ptt_hint").as_string("RTS/DTR line of this serial port"), c);
    }
#endif
    for (const auto& h : list_cm108_devices()) {
      Json c = Json::object();
      c.set("ptt", "cm108");
      c.set("ptt_device", h.substr(0, h.find(' ')));
      c.set("ptt_gpio", 3);
      add("hid:" + h, "USB sound-card GPIO " + h, "CM108/CM119 HID PTT (AIOC in CM108 mode, many interfaces)", c);
    }
    add("rigctld", "CAT via Hamlib rigctld (127.0.0.1:4532)", "Run rigctld -m <model> -r <port> first", Json::parse_or_null(R"({"ptt":"rigctld"})"));
    out.set("ptt_devices", ptt);
    return out;
  };
  reg.add(std::move(k));
}

}  // namespace ember_host
