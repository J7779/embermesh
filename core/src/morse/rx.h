// EmberMesh core / morse — real-time Morse receive (docs/RPC_API.md §6 "Morse receive"):
//
//   * LightKeyer: brightness samples (0..1) of a camera region watching a flashlight, lamp or
//     blinking screen, at irregular times (10..60 fps, jitter, dropped frames). Adaptive
//     envelope (state-dependent tracking of the "on" and "off" levels, so camera auto-exposure
//     pumping, slow ambient drift and level wander from a shaking hand are followed), a threshold
//     with hysteresis between them, a contrast squelch, and sub-frame interpolation of the
//     crossing times.
//   * AudioDecoder: 16-bit PCM at any rate 8..192 kHz, resampled to 8 kHz and decoded by the same
//     CwReceiver as the keyboard-mode CW modem (tone auto-detect 300..1500 Hz, drift tracking).
//   * Both feed modem::MorseTiming (speed estimate, hand-timing tolerance, real-time gaps).
//   * Session / Sessions: the RPC-facing state (transcript of the last 4000 characters, counters,
//     at most 4 sessions, idle expiry). Every input is validated; NaN/inf/negative/huge levels,
//     non-monotonic or absurd timestamps and giant batches are safe.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "dsp/resampler.h"
#include "modem/cw_rx.h"
#include "modem/morse_timing.h"

namespace ember {
namespace morse_rx {

constexpr size_t kMaxSessions = 4;
constexpr uint64_t kIdleExpiryMs = 60000;
constexpr size_t kTranscriptChars = 4000;
constexpr size_t kMaxLevelsPerFeed = 20000;
constexpr size_t kMaxPcmBytesPerFeed = 4u << 20;
constexpr uint32_t kMinAudioRate = 8000, kMaxAudioRate = 192000;

class LightKeyer {
 public:
  explicit LightKeyer(const modem::MorseTiming::Config& tc);
  void reset();
  // One brightness sample at time t (ms). Non-finite values are ignored; levels clamp to 0..1.
  void sample(double t_ms, double level);
  modem::MorseTiming& timing() { return timing_; }
  const modem::MorseTiming& timing() const { return timing_; }
  double level() const;      // 0..1 normalised (between the off and on levels)
  double threshold() const;  // absolute brightness threshold 0..1
  // 25th percentile of |frame-to-frame change| ~ 0.45 sigma of white sensor noise.
  bool contrast_ok() const { return hi_ - lo_ >= std::max(kMinContrast, 18 * noise_); }

 private:
  static constexpr double kMinContrast = 0.02;
  modem::MorseTiming timing_;
  bool have_ = false, key_ = false;
  double last_t_ = 0, last_v_ = 0, offset_ = 0;  // offset_ maps the caller's times onto a monotonic axis
  double hi_ = 0, lo_ = 0;
  double noise_ = 0.015;  // running 25th percentile of |frame-to-frame change| (robust to transitions)
  double frame_ms_ = 33;  // running frame interval
  double prev_v_[2] = {0, 0};  // the two samples before last_v_ (edge-sharpness gate)
  int nprev_ = 0;  // mean |frame-to-frame change| inside a state (robust running estimate)
};

class AudioDecoder {
 public:
  AudioDecoder(uint32_t rate, double tone_hz, const modem::MorseTiming::Config& tc);
  void feed(const int16_t* pcm, size_t n);
  void reset();
  modem::MorseTiming& timing() { return rx_.timing(); }
  const modem::CwReceiver& rx() const { return rx_; }

 private:
  uint32_t rate_;
  double tone_;
  modem::MorseTiming::Config tc_;
  dsp::Resampler rs_;
  modem::CwReceiver rx_;
  std::vector<float> in_, out_;
};

struct Status {
  std::string transcript;
  uint64_t chars = 0;
  double wpm = 0;
  bool on = false;
  double level = 0, threshold = 0, quality = 0;
  std::string pending;
  bool audio = false;
  double tone_hz = 0, snr_db = 0;
};

class Session {
 public:
  // source "light" or "audio". Returns null (and *error) for bad parameters.
  static std::unique_ptr<Session> create(const std::string& source, uint32_t sample_rate, double wpm_hint,
                                         double tone_hz, std::string* error);
  bool is_audio() const { return audio_ != nullptr; }
  const std::string& source() const { return source_; }
  // Light: uniformly spaced samples from t_ms, or per-sample times (same length as levels).
  void feed_uniform(double t_ms, double dt_ms, const std::vector<double>& levels);
  void feed_times(const std::vector<double>& times_ms, const std::vector<double>& levels);
  // Audio: mono s16le.
  void feed_pcm(const int16_t* pcm, size_t n);
  // New text since the last call (for the morse.text event); also appended to the transcript.
  std::string take_new();
  Status status() const;
  void reset();
  std::string transcript() const { return transcript_; }

 private:
  Session() = default;
  modem::MorseTiming& timing();
  const modem::MorseTiming& timing() const;
  void collect();

  std::string source_;
  modem::MorseTiming::Config tc_;
  std::unique_ptr<LightKeyer> light_;
  std::unique_ptr<AudioDecoder> audio_;
  std::string transcript_, new_;
  uint64_t chars_ = 0;
};

// Session table with ids, a cap and idle expiry (times are the caller's monotonic ms).
class Sessions {
 public:
  // Returns the new id, or "" with *error ("busy" when kMaxSessions are open).
  std::string start(std::unique_ptr<Session> s, uint64_t now_ms, std::string* error);
  Session* get(const std::string& id, uint64_t now_ms);  // also refreshes the idle timer
  bool stop(const std::string& id, std::string* transcript);
  // Closes sessions idle for kIdleExpiryMs; returns their ids.
  std::vector<std::string> expire(uint64_t now_ms);
  size_t size() const { return s_.size(); }

 private:
  struct Entry {
    std::unique_ptr<Session> s;
    uint64_t last = 0;
  };
  std::map<std::string, Entry> s_;
  uint64_t next_ = 1;
};

}  // namespace morse_rx
}  // namespace ember
