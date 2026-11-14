// EmberMesh core / modem — CW (on/off keyed tone) receiver front end at 8 kHz: tone mixing and a
// narrow envelope filter, adaptive floor/peak keying with hysteresis and a noise squelch, and
// tone auto-detection / drift tracking (AFC, 300..1500 Hz by default). Key transitions feed the
// shared MorseTiming back end. Used by the keyboard-mode CW modem (modem/cw.cpp) and by the Morse
// receive sessions (morse/), so both decode identically.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "dsp/dsp.h"
#include "dsp/fft.h"
#include "dsp/fir.h"
#include "dsp/nco.h"
#include "modem/morse_timing.h"

namespace ember {
namespace modem {

class CwReceiver {
 public:
  static constexpr double kRate = 8000;  // input sample rate
  struct Config {
    double tone_hz = 700;   // starting tone
    double bw_hz = 100;     // envelope filter bandwidth
    bool afc = true;        // find and follow the tone
    double afc_lo_hz = 300, afc_hi_hz = 1500;
    MorseTiming::Config timing;
  };

  bool init(const Config& c);
  void reset();  // clears keying and timing state, keeps the tone
  // Feeds 8 kHz samples (finite, roughly +-1).
  void process(const float* x, size_t n);

  MorseTiming& timing() { return timing_; }
  const MorseTiming& timing() const { return timing_; }
  bool key() const { return key_; }
  bool busy() const;  // keyed, a character in progress, or just after a mark
  double tone_hz() const { return tone_; }
  double snr_db() const;
  // Signal meter 0..1 (envelope relative to the recent peak).
  double level() const;
  double time_ms() const { return ticks_ * kTickMs; }

 private:
  static constexpr size_t kDecim = 16;  // envelope rate 500 Hz
  static constexpr double kTickMs = 1000.0 * kDecim / kRate;

  void tick(float e);
  void afc_step();

  Config cfg_;
  double tone_ = 700;
  dsp::Nco nco_;
  dsp::FirDecimC lp_;
  float env_s_ = 0, floor_ = 1e-6f, peak_ = 1e-6f;
  bool key_ = false, seen_ = false;
  int warmup_ = 0;
  uint64_t ticks_ = 0;
  double last_mark_end_ms_ = -1e9;
  MorseTiming timing_;
  dsp::Fft fft_;
  std::vector<dsp::Cf> fbuf_;
  std::vector<float> spec_, hist_;
  size_t hpos_ = 0, fcount_ = 0, afc_n_ = 0;
};

}  // namespace modem
}  // namespace ember
