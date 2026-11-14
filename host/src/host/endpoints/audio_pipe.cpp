// Virtual sound card: raw PCM through named pipes (FIFOs) or files instead of a sound device.
//
// The endpoint behaves like a sound card running in real time: every block (20 ms) it asks the
// core's soft-modem link for exactly the samples that are due (ember_audio_tx, padded with silence)
// and writes them to `output_path`, and it feeds whatever arrived on `input_path` to
// ember_audio_rx. Format: signed 16-bit little-endian mono at `sample_rate` (sox/arecord/aplay
// "-t raw -e signed -b 16 -c 1"). Uses:
//   * two nodes on one machine wired sample-to-sample (A.out -> B.in, B.out -> A.in): CI tests of
//     the audio link without hardware or OS permissions;
//   * piping to other programs (sox, direwolf -r 48000 -, rtl_fm | ..., virtual cables).
// A FIFO without a reader just drops the samples (counted); regular input files are read at the
// real-time rate. POSIX only (named pipes); not registered on Windows.
#if !defined(_WIN32)

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "host/endpoint.h"
#include "host/util.h"

namespace ember_host {

namespace {

// write() that never raises SIGPIPE (a FIFO whose reader went away returns EPIPE instead).
ssize_t write_nosig(int fd, const void* p, size_t n) {
#if defined(__APPLE__)
  return ::write(fd, p, n);  // F_SETNOSIGPIPE is set on the descriptor
#else
  sigset_t block, old;
  sigemptyset(&block);
  sigaddset(&block, SIGPIPE);
  pthread_sigmask(SIG_BLOCK, &block, &old);
  ssize_t r = ::write(fd, p, n);
  int e = errno;
  if (r < 0 && e == EPIPE) {
    struct timespec zero = {0, 0};
    sigtimedwait(&block, nullptr, &zero);  // consume the pending SIGPIPE
  }
  pthread_sigmask(SIG_SETMASK, &old, nullptr);
  errno = e;
  return r;
#endif
}

class AudioPipeEndpoint : public Endpoint {
 public:
  AudioPipeEndpoint(Loop& l, EndpointSink& s, const Json& c) : Endpoint(l, s, c), alive_(std::make_shared<bool>(true)) {
    in_path_ = cfg_str(c, "input_path");
    out_path_ = cfg_str(c, "output_path");
    rate_ = uint32_t(cfg_int(c, "sample_rate", 48000));
    int64_t b = cfg_int(c, "block_ms", 20);
    block_ms_ = uint32_t(b < 5 ? 5 : b > 200 ? 200 : b);
    int64_t j = cfg_int(c, "block_jitter_ms", 0);
    jitter_ms_ = uint32_t(j < 0 ? 0 : j > 2000 ? 2000 : j);
    make_fifos_ = cfg_bool(c, "create_fifos", true);
    double nd = cfg_num(c, "rx_noise_dbfs", -70);
    noise_amp_ = nd <= -120 ? 0.0 : 32768.0 * std::pow(10.0, nd / 20.0);
    // Optional channel model on the output (what the "radio" does to our transmit audio).
    tx_gain_ = std::pow(10.0, cfg_num(c, "tx_gain_db", 0) / 20.0);
    double tn = cfg_num(c, "tx_noise_dbfs", -120);
    tx_noise_amp_ = tn <= -120 ? 0.0 : 32768.0 * std::pow(10.0, tn / 20.0);
    double lp = cfg_num(c, "tx_lowpass_hz", 0), hp = cfg_num(c, "tx_highpass_hz", 0);
    const double two_pi = 6.283185307179586;
    lp_a_ = lp > 0 ? 1.0 - std::exp(-two_pi * lp / double(rate_)) : 1.0;
    hp_a_ = hp > 0 ? std::exp(-two_pi * hp / double(rate_)) : 0.0;
    vox_attack_ = uint64_t(cfg_int(c, "vox_attack_ms", 0)) * rate_ / 1000;
    vox_hang_ = uint64_t(cfg_int(c, "vox_hang_ms", 0)) * rate_ / 1000;
    vox_thr_ = 32768.0 * std::pow(10.0, cfg_num(c, "vox_threshold_dbfs", -40) / 20.0);
    vox_ = vox_attack_ > 0 || vox_hang_ > 0;
    vox_decay_ = std::exp(-1.0 / (0.010 * double(rate_)));
  }
  ~AudioPipeEndpoint() override {
    *alive_ = false;
    close();
  }
  const char* kind() const override { return "audio_pipe"; }
  bool is_audio() const override { return true; }
  bool auto_reconnect() const override { return true; }

  void open() override {
    close();
    std::string err;
    if (in_path_.empty() && out_path_.empty()) err = "set input_path and/or output_path";
    if (err.empty() && !in_path_.empty()) open_input(&err);
    if (err.empty() && !out_path_.empty() && make_fifos_) make_fifo(out_path_, &err);
    std::weak_ptr<bool> w = alive_;
    if (!err.empty()) {
      close();
      loop_.timer(0, [this, w, err] {
        auto a = w.lock();
        if (a && *a) set_state(false, err);
      });
      return;
    }
    t0_ = steady_ms();
    produced_ = consumed_ = 0;
    running_ = true;
    try_open_output();
    timer_ = loop_.timer(block_ms_, [this] { on_block(); });
    EH_LOGI("audio_pipe: in '%s' out '%s' @ %u Hz", in_path_.c_str(), out_path_.c_str(), unsigned(rate_));
    loop_.timer(0, [this, w] {
      auto a = w.lock();
      if (a && *a && running_) set_state(true);
    });
  }

  void close() override {
    running_ = false;
    if (timer_) {
      loop_.cancel(timer_);
      timer_ = 0;
    }
    watch_input(false);
    if (out_watch_) loop_.unwatch(out_watch_);
    out_watch_ = 0;
    if (in_fd_ >= 0) ::close(in_fd_);
    if (out_fd_ >= 0) ::close(out_fd_);
    in_fd_ = out_fd_ = -1;
    in_carry_.clear();
    out_pending_.clear();
    connected_ = false;
  }

  void write(const uint8_t*, size_t) override {}  // PCM only
  void set_ptt(bool on) override { ptt_on_ = on; }

  Json status() const override {
    Json s = Json::object();
    s.set("input_path", in_path_);
    s.set("output_path", out_path_);
    s.set("sample_rate", rate_);
    s.set("reader_connected", out_fd_ >= 0);
    s.set("rx_samples", double(rx_samples_));
    s.set("tx_samples", double(tx_samples_));
    s.set("tx_signal_samples", double(tx_signal_));
    if (tx_dropped_) s.set("tx_dropped_samples", double(tx_dropped_));
    s.set("ptt_on", ptt_on_);
    if (vox_) s.set("vox_keyups", double(vox_keyups_));
    if (stalls_) s.set("stalls", double(stalls_));
    double dbfs = peak_ <= 0 ? -120.0 : 20.0 * std::log10(double(peak_) / 32768.0);
    s.set("rx_level_dbfs", std::round(dbfs * 10) / 10);
    s.set("rx_max_dbfs", max_peak_ <= 0 ? -120.0 : std::round(200.0 * std::log10(double(max_peak_) / 32768.0)) / 10);
    s.set("rx_signal_samples", double(rx_signal_));
    return s;
  }

 private:
  bool make_fifo(const std::string& path, std::string* err) {
    struct stat st;
    if (::stat(path.c_str(), &st) == 0) return true;
    if (::mkfifo(path.c_str(), 0600) != 0 && errno != EEXIST) {
      *err = "mkfifo " + path + ": " + std::strerror(errno);
      return false;
    }
    return true;
  }

  void open_input(std::string* err) {
    if (make_fifos_ && !make_fifo(in_path_, err)) return;
    in_fd_ = ::open(in_path_.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (in_fd_ < 0) {
      *err = "open " + in_path_ + ": " + std::strerror(errno);
      return;
    }
    struct stat st;
    in_is_file_ = ::fstat(in_fd_, &st) == 0 && S_ISREG(st.st_mode);
  }

  void try_open_output() {
    if (out_path_.empty() || out_fd_ >= 0) return;
    struct stat st;
    bool exists = ::stat(out_path_.c_str(), &st) == 0;
    int flags = O_WRONLY | O_NONBLOCK | O_CLOEXEC;
    if (!exists || S_ISREG(st.st_mode)) flags |= O_CREAT | O_TRUNC;
    out_fd_ = ::open(out_path_.c_str(), flags, 0600);  // FIFO without a reader: ENXIO, retried
    if (out_fd_ < 0) return;
#if defined(__APPLE__) && defined(F_SETNOSIGPIPE)
    ::fcntl(out_fd_, F_SETNOSIGPIPE, 1);
#endif
#if defined(__linux__) && defined(F_SETPIPE_SZ)
    ::fcntl(out_fd_, F_SETPIPE_SZ, 1 << 20);  // room for ~5 s at 48 kHz
#endif
    out_pending_.clear();
    out_written_ = 0;  // a new reader starts at a sample boundary
  }

  void on_block() {
    timer_ = 0;
    if (!running_) return;
    uint64_t now = steady_ms();
    uint64_t total = (now - t0_) * uint64_t(rate_) / 1000;
    // A late timer (busy computer, stretched VM) is caught up sample-accurately: the missed samples
    // are produced now, in one burst, exactly as a sound card's buffered callback would deliver
    // them; the channel model and the far end count in samples, so bursts change nothing. Only a
    // real stall (suspend, debugger) longer than kMaxCatchUpS is skipped, and counted.
    uint64_t max_catch = uint64_t(rate_) * kMaxCatchUpS;
    if (total > produced_ + max_catch) {
      ++stalls_;
      produced_ = total - max_catch;
    }
    if (total > consumed_ + max_catch) consumed_ = total - max_catch;
    size_t due = size_t(total - produced_);
    produce(due);
    if (consume(size_t(total - consumed_)) > 0) watch_input(true);
    uint32_t next = block_ms_;
    if (jitter_ms_) {  // testing: deliver audio in irregular bursts, like an overloaded host
      jrng_ = jrng_ * 1664525u + 1013904223u;
      next += (jrng_ >> 8) % (jitter_ms_ + 1);
    }
    if (running_) timer_ = loop_.timer(next, [this] { on_block(); });
  }

  void produce(size_t n) {
    if (n == 0) return;
    if (buf_.size() < n) buf_.resize(n);
    size_t got = sink().ep_audio_tx(*this, buf_.data(), n, rate_);
    if (got > n) got = n;
    for (size_t i = 0; i < got; ++i)
      if (buf_[i] > 64 || buf_[i] < -64) ++tx_signal_;
    std::memset(buf_.data() + got, 0, (n - got) * sizeof(int16_t));  // silence keeps the clock
    channel_model(buf_.data(), n);
    produced_ += n;
    tx_samples_ += n;
    if (out_path_.empty()) return;
    try_open_output();
    if (out_fd_ < 0) {
      tx_dropped_ += n;
      return;
    }
    size_t base = out_pending_.size();
    out_pending_.resize(base + n * 2);
    for (size_t i = 0; i < n; ++i) {
      uint16_t v = uint16_t(buf_[i]);
      out_pending_[base + 2 * i] = uint8_t(v & 0xff);
      out_pending_[base + 2 * i + 1] = uint8_t(v >> 8);
    }
    flush_output();
    // What the reader could not take yet waits (a FIFO holds only 16-64 KiB); beyond
    // kMaxCatchUpS of backlog the oldest whole samples are dropped, keeping the stream aligned.
    size_t cap = size_t(rate_) * 2 * kMaxCatchUpS;
    if (out_pending_.size() > cap) {
      size_t keep_head = (out_written_ & 1) ? 1 : 0;  // the rest of a half-written sample must go out
      size_t drop = (out_pending_.size() - cap) & ~size_t(1);
      if (drop + keep_head > out_pending_.size()) drop = (out_pending_.size() - keep_head) & ~size_t(1);
      tx_dropped_ += drop / 2;
      out_pending_.erase(out_pending_.begin() + long(keep_head), out_pending_.begin() + long(keep_head + drop));
    }
  }

  // Writes as much pending output as the FIFO takes now, and keeps writing whenever it becomes
  // writable again (not only at the next block): otherwise a reader that is late once would make
  // the backlog grow for ever, as the pipe can take only one buffer per block.
  void flush_output() {
    if (out_fd_ < 0) {
      out_pending_.clear();
      return;
    }
    size_t off = 0;
    while (off < out_pending_.size()) {
      ssize_t w = write_nosig(out_fd_, out_pending_.data() + off, out_pending_.size() - off);
      if (w > 0) {
        off += size_t(w);
        continue;
      }
      if (w < 0 && errno == EINTR) continue;
      if (w < 0 && errno == EPIPE) {  // reader went away: reopen when one comes back
        if (out_watch_) loop_.unwatch(out_watch_);
        out_watch_ = 0;
        ::close(out_fd_);
        out_fd_ = -1;
      }
      break;  // EAGAIN: wait for writability
    }
    out_written_ += off;
    out_pending_.erase(out_pending_.begin(), out_pending_.begin() + long(off));
    bool want = out_fd_ >= 0 && !out_pending_.empty();
    if (want && !out_watch_) {
      out_watch_ = loop_.watch(out_fd_, kPollOut, [this](short) { flush_output(); });
    } else if (!want && out_watch_) {
      loop_.unwatch(out_watch_);
      out_watch_ = 0;
    }
  }

  // VOX: the "radio" transmits only after the audio has been above the threshold for vox_attack
  // (the start of each transmission is lost) and keeps transmitting vox_hang after it drops; while
  // it is not transmitting the far end hears nothing. Then band-limiting, gain and channel noise.
  void channel_model(int16_t* x, size_t n) {
    if (!vox_ && tx_gain_ == 1.0 && tx_noise_amp_ == 0 && lp_a_ == 1.0 && hp_a_ == 0.0) return;
    for (size_t i = 0; i < n; ++i) {
      double v = x[i];
      if (vox_) {
        // Envelope (peak follower, ~10 ms decay) so zero crossings of a tone do not count as silence.
        vox_env_ = std::max(std::fabs(v), vox_env_ * vox_decay_);
        bool loud = vox_env_ > vox_thr_;
        if (loud) {
          ++vox_loud_run_;
          vox_quiet_run_ = 0;
          if (!vox_keyed_ && vox_loud_run_ >= vox_attack_) {
            vox_keyed_ = true;
            ++vox_keyups_;
          }
        } else {
          vox_loud_run_ = 0;
          if (vox_keyed_ && ++vox_quiet_run_ > vox_hang_ + rate_ / 50) vox_keyed_ = false;  // + 20 ms syllable gap
        }
        if (!vox_keyed_) v = 0;
      }
      lp_y_ += lp_a_ * (v - lp_y_);
      v = lp_y_;
      if (hp_a_ > 0) {
        double y = hp_a_ * (hp_y_ + v - hp_x_);
        hp_x_ = v;
        hp_y_ = y;
        v = y;
      }
      v *= tx_gain_;
      if (tx_noise_amp_ > 0) {
        rng_tx_ = rng_tx_ * 1664525u + 1013904223u;
        v += (double(rng_tx_ >> 8) / double(1u << 24) - 0.5) * 2.0 * tx_noise_amp_;
      }
      x[i] = int16_t(v > 32767 ? 32767 : v < -32768 ? -32768 : std::lround(v));
    }
  }

  // FIFO input is read as soon as it is readable (a watch), so the endpoint adds no latency of its
  // own; while no writer is connected the watch is dropped (a writer-less FIFO polls as readable
  // forever) and the block timer polls instead.
  void watch_input(bool on) {
    if (on == (in_watch_ != 0) || in_is_file_ || in_fd_ < 0) return;
    if (on) {
      in_watch_ = loop_.watch(in_fd_, kPollIn, [this](short) {
        if (consume(0) == 0) watch_input(false);
      });
    } else {
      loop_.unwatch(in_watch_);
      in_watch_ = 0;
    }
  }

  size_t consume(size_t due) {
    if (in_fd_ < 0) {
      consumed_ += due;
      return 0;
    }
    // FIFOs: read everything that is there (the writer paces). Files: at the real-time rate.
    size_t want_bytes = in_is_file_ ? due * 2 : size_t(1) << 20;
    uint8_t tmp[16384];
    size_t got_total = 0;
    while (got_total < want_bytes) {
      size_t chunk = std::min(sizeof tmp, want_bytes - got_total);
      ssize_t r = ::read(in_fd_, tmp, chunk);
      if (r < 0 && errno == EINTR) continue;
      if (r <= 0) break;  // EAGAIN, or 0 = no writer connected right now / end of file
      got_total += size_t(r);
      in_carry_.insert(in_carry_.end(), tmp, tmp + r);
      size_t ns = in_carry_.size() / 2;
      if (ns == 0) continue;
      if (rx_.size() < ns) rx_.resize(ns);
      for (size_t i = 0; i < ns; ++i) {
        int16_t v = int16_t(uint16_t(in_carry_[2 * i]) | uint16_t(in_carry_[2 * i + 1]) << 8);
        if (noise_amp_ > 0) {
          // A real input always has a noise floor; bit-exact digital silence is not a radio channel
          // (and can leave a modem's carrier detect latched on).
          rng_ = rng_ * 1664525u + 1013904223u;
          double n = (double(rng_ >> 8) / double(1u << 24) - 0.5) * 2.0 * noise_amp_;
          double x = double(v) + n;
          v = int16_t(x > 32767 ? 32767 : x < -32768 ? -32768 : std::lround(x));
        }
        rx_[i] = v;
        int a = v < 0 ? -int(v) : int(v);
        if (a > win_peak_) win_peak_ = a;
        if (a > max_peak_) max_peak_ = a;
        if (a > 1000) ++rx_signal_;
      }
      in_carry_.erase(in_carry_.begin(), in_carry_.begin() + long(ns * 2));
      sink().ep_audio_rx(*this, rx_.data(), ns, rate_);
      rx_samples_ += ns;
      win_count_ += ns;
      if (win_count_ >= rate_ / 2) {
        peak_ = win_peak_;
        win_peak_ = 0;
        win_count_ = 0;
      }
      if (!running_) return got_total;
    }
    consumed_ += due;
    return got_total;
  }

  std::string in_path_, out_path_;
  uint32_t rate_ = 48000, block_ms_ = 20;
  bool make_fifos_ = true;
  static constexpr uint64_t kMaxCatchUpS = 10;
  uint32_t jitter_ms_ = 0, jrng_ = 0x2545f491u;
  uint64_t stalls_ = 0;
  double noise_amp_ = 0;
  double tx_gain_ = 1.0, tx_noise_amp_ = 0, lp_a_ = 1.0, hp_a_ = 0.0, lp_y_ = 0, hp_x_ = 0, hp_y_ = 0, vox_thr_ = 328;
  bool vox_ = false, vox_keyed_ = false;
  double vox_env_ = 0, vox_decay_ = 0.998;
  uint64_t vox_attack_ = 0, vox_hang_ = 0, vox_loud_run_ = 0, vox_quiet_run_ = 0, vox_keyups_ = 0;
  uint32_t rng_tx_ = 0x9e3779b9u;
  uint32_t rng_ = 0x12345678u;
  int in_fd_ = -1, out_fd_ = -1;
  bool in_is_file_ = false;
  bool running_ = false;
  bool ptt_on_ = false;
  uint64_t timer_ = 0;
  uint64_t in_watch_ = 0, out_watch_ = 0;
  uint64_t out_written_ = 0;  // bytes written to the output so far (sample alignment)
  uint64_t t0_ = 0, produced_ = 0, consumed_ = 0;
  uint64_t rx_samples_ = 0, tx_samples_ = 0, tx_dropped_ = 0, tx_signal_ = 0;
  int win_peak_ = 0, peak_ = 0, max_peak_ = 0;
  uint64_t rx_signal_ = 0;
  size_t win_count_ = 0;
  std::vector<int16_t> buf_, rx_;
  std::vector<uint8_t> in_carry_, out_pending_;
  std::shared_ptr<bool> alive_;
};

}  // namespace

void register_audio_pipe_endpoints(EndpointRegistry& reg) {
  EndpointKind k;
  k.kind = "audio_pipe";
  k.title = "Audio pipe (virtual sound card)";
  k.description =
      "Raw 16-bit mono PCM through named pipes or files instead of a sound card: wire two nodes "
      "sample-to-sample for testing, or pipe to sox, Direwolf, rtl_fm or a virtual audio cable.";
  k.config.push(Field("input_path", "Input (PCM from the radio)", "text").def("").help(
      "Named pipe or file with s16le mono PCM; created as a FIFO if missing. Empty = transmit only."));
  k.config.push(Field("output_path", "Output (PCM to the radio)", "text").def("").help(
      "Named pipe or file to write s16le mono PCM to; created as a FIFO if missing. Empty = receive only."));
  k.config.push(Field("sample_rate", "Sample rate", "number").def(48000).unit("Hz").min(8000).max(192000));
  k.config.push(Field("block_ms", "Block size", "number").def(20).unit("ms").min(5).max(200).advanced());
  k.config.push(Field("create_fifos", "Create missing paths as FIFOs", "bool").def(true).advanced());
  // Channel model (testing / training): what a radio path does to the transmitted audio.
  k.config.push(Field("block_jitter_ms", "Testing: irregular delivery", "number").def(0).unit("ms").min(0).max(2000).advanced().help(
      "Adds up to this much random delay between blocks (audio then arrives in bursts, as on an overloaded computer)"));
  k.config.push(Field("vox_attack_ms", "Simulated VOX: lost start of each transmission", "number").def(0).unit("ms").min(0).max(2000).advanced());
  k.config.push(Field("vox_hang_ms", "Simulated VOX: hang time", "number").def(0).unit("ms").min(0).max(5000).advanced());
  k.config.push(Field("vox_threshold_dbfs", "Simulated VOX: threshold", "number").def(-40).unit("dBFS").min(-90).max(0).advanced());
  k.config.push(Field("tx_lowpass_hz", "Channel low-pass", "number").def(0).unit("Hz").min(0).max(20000).advanced());
  k.config.push(Field("tx_highpass_hz", "Channel high-pass", "number").def(0).unit("Hz").min(0).max(5000).advanced());
  k.config.push(Field("tx_gain_db", "Channel gain", "number").def(0).unit("dB").min(-60).max(20).advanced());
  k.config.push(Field("tx_noise_dbfs", "Channel noise", "number").def(-120).unit("dBFS").min(-120).max(0).advanced());
  k.config.push(Field("rx_noise_dbfs", "Input noise floor", "number").def(-70).unit("dBFS").min(-120).max(-20).advanced().help(
      "Noise added to the input like a real sound card (-120 = none)"));
  k.link_types = {"audio"};
  k.default_link_type = "audio";
  Json fields = k.config;
  k.create = [fields](Loop& loop, EndpointSink& sink, const Json& cfg_in, std::string* error) -> std::unique_ptr<Endpoint> {
    Json cfg = cfg_with_defaults(fields, cfg_in);
    int64_t rate = cfg_int(cfg, "sample_rate", 48000);
    if (rate < 8000 || rate > 192000) {
      *error = "sample_rate must be 8000-192000";
      return nullptr;
    }
    if (cfg_str(cfg, "input_path").empty() && cfg_str(cfg, "output_path").empty()) {
      *error = "set input_path and/or output_path";
      return nullptr;
    }
    return std::unique_ptr<Endpoint>(new AudioPipeEndpoint(loop, sink, cfg));
  };
  reg.add(std::move(k));
}

}  // namespace ember_host

#endif  // !_WIN32
