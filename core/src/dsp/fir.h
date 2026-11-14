// EmberMesh core / dsp — windowed-sinc FIR design and streaming FIR filters.
//
// Design functions return linear-phase taps (odd length recommended, group delay (taps-1)/2) with
// unity gain at DC (low-pass), at the band centre (band-pass) or at Nyquist (high-pass).
// All frequencies are in Hz relative to `fs`. Invalid arguments produce an empty vector or a
// single unity tap, never a crash.
#pragma once

#include <cstddef>
#include <vector>

#include "dsp/dsp.h"

namespace ember {
namespace dsp {

enum class Window { Rect, Hann, Hamming, Blackman, Kaiser };

// Window value w(i) for i in [0, n). `beta` is used for Kaiser only.
double window_value(Window w, size_t i, size_t n, double beta = 8.0);
// Zeroth-order modified Bessel function (for the Kaiser window).
double bessel_i0(double x);
// Kaiser beta for a stop-band attenuation in dB.
double kaiser_beta(double atten_db);
// Odd tap count estimate for a Kaiser-windowed filter (Harris/Kaiser formula), clamped to [3, max_taps].
size_t kaiser_taps(double atten_db, double transition_hz, double fs, size_t max_taps = 4095);

std::vector<float> fir_lowpass(size_t taps, double cutoff_hz, double fs, Window w = Window::Hamming, double beta = 8.0);
std::vector<float> fir_highpass(size_t taps, double cutoff_hz, double fs, Window w = Window::Hamming, double beta = 8.0);
std::vector<float> fir_bandpass(size_t taps, double lo_hz, double hi_hz, double fs, Window w = Window::Hamming, double beta = 8.0);
// Type-III Hilbert transformer (odd taps, zero centre tap and even-offset taps). For an input
// cos(wt) the output is sin(wt) delayed by (taps-1)/2 samples (pass-band only; ripple near DC and
// Nyquist depends on the length).
std::vector<float> fir_hilbert(size_t taps, Window w = Window::Blackman);
// Root-raised-cosine pulse, `span` symbols long, `sps` samples per symbol (integer), roll-off alpha.
// Normalised to unit energy.
std::vector<float> fir_rrc(size_t sps, size_t span, double alpha);
// Frequency response magnitude of `taps` at f (for tests / diagnostics).
double fir_gain(const std::vector<float>& taps, double f_hz, double fs);

// Streaming real FIR. Delay line is stored twice so each output is one contiguous dot product.
class Fir {
 public:
  Fir() = default;
  explicit Fir(std::vector<float> taps) { set_taps(std::move(taps)); }
  void set_taps(std::vector<float> taps);
  size_t size() const { return taps_.size(); }
  const std::vector<float>& taps() const { return taps_; }
  void reset();
  float process(float x);
  void process(const float* in, float* out, size_t n);
  // Push without computing an output (for decimation).
  void push(float x);
  // Dot product over the current delay line.
  float output() const;

 private:
  std::vector<float> taps_;  // reversed
  std::vector<float> buf_;   // 2 * size
  size_t pos_ = 0;
};

// Streaming FIR with real taps applied to a complex stream.
class FirC {
 public:
  FirC() = default;
  explicit FirC(std::vector<float> taps) { set_taps(std::move(taps)); }
  void set_taps(std::vector<float> taps);
  size_t size() const { return taps_.size(); }
  void reset();
  void push(Cf x);
  Cf output() const;
  Cf process(Cf x) { push(x); return output(); }

 private:
  std::vector<float> taps_;  // reversed
  std::vector<float> re_, im_;
  size_t pos_ = 0;
};

// Complex decimating FIR: feed every input sample, get an output every `factor` inputs.
class FirDecimC {
 public:
  FirDecimC() = default;
  void init(std::vector<float> taps, size_t factor);
  size_t factor() const { return factor_; }
  void reset();
  // Returns true and sets *out when an output sample is due.
  bool push(Cf x, Cf* out);

 private:
  FirC fir_;
  size_t factor_ = 1, phase_ = 0;
};

// Real decimating FIR.
class FirDecim {
 public:
  FirDecim() = default;
  void init(std::vector<float> taps, size_t factor);
  size_t factor() const { return factor_; }
  void reset();
  bool push(float x, float* out);

 private:
  Fir fir_;
  size_t factor_ = 1, phase_ = 0;
};

// Integer delay line (for delay-matching filter branches).
class Delay {
 public:
  explicit Delay(size_t n = 0) { set(n); }
  void set(size_t n) { buf_.assign(n + 1, 0.0f); pos_ = 0; }
  float process(float x) {
    if (buf_.size() <= 1) return x;
    buf_[pos_] = x;
    pos_ = (pos_ + 1) % buf_.size();
    return buf_[pos_];
  }

 private:
  std::vector<float> buf_;
  size_t pos_ = 0;
};

}  // namespace dsp
}  // namespace ember
