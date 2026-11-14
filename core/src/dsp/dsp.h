// EmberMesh core / dsp — common types and helpers for the signal-processing module.
//
// The dsp module is a small, dependency-free toolbox used by the soft modems (core/src/modem/):
//   dsp.h         complex type, constants, dB helpers, seeded PRNG
//   nco.h         numerically controlled oscillator (table based, phase continuous)
//   fft.h         radix-2 complex FFT
//   fir.h         windowed-sinc FIR design (low/high/band-pass, Hilbert, RRC) + streaming filters
//   biquad.h      RBJ-cookbook biquads, first-order (de/pre-emphasis) sections
//   goertzel.h    single-bin DFT detector
//   agc.h         AGC, DC blocker, peak/valley envelope tracker
//   resampler.h   arbitrary-ratio streaming polyphase resampler
//   level.h       signal level metering and band-power SNR estimation
//   channel_sim.h AWGN / radio channel simulator for tests
//   nbfm.h        NBFM demodulator from rtl_tcp-style unsigned 8-bit IQ
//
// Conventions: float samples, nominal full scale is [-1, 1]; frequencies in Hz with an explicit
// sample rate; every object is single-threaded, allocation happens only in init()/constructors
// (and in vector-appending process() calls); no exceptions, no global mutable state.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ember {
namespace dsp {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

// Minimal complex float (std::complex<float> is avoided because its operator* carries
// NaN-handling branches that are slow in unoptimised and WASM builds).
struct Cf {
  float re = 0, im = 0;
  constexpr Cf() = default;
  constexpr Cf(float r, float i) : re(r), im(i) {}
};
inline Cf operator+(Cf a, Cf b) { return {a.re + b.re, a.im + b.im}; }
inline Cf operator-(Cf a, Cf b) { return {a.re - b.re, a.im - b.im}; }
inline Cf operator*(Cf a, Cf b) { return {a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re}; }
inline Cf operator*(Cf a, float s) { return {a.re * s, a.im * s}; }
inline Cf& operator+=(Cf& a, Cf b) { a.re += b.re; a.im += b.im; return a; }
inline Cf conj(Cf a) { return {a.re, -a.im}; }
inline float norm2(Cf a) { return a.re * a.re + a.im * a.im; }
inline float cabs(Cf a) { return std::sqrt(norm2(a)); }
inline float carg(Cf a) { return std::atan2(a.im, a.re); }
inline Cf expj(double rad) { return {static_cast<float>(std::cos(rad)), static_cast<float>(std::sin(rad))}; }

inline float db_to_lin(float db) { return std::pow(10.0f, db / 20.0f); }      // amplitude
inline float db_to_pow(float db) { return std::pow(10.0f, db / 10.0f); }      // power
inline float pow_to_db(float p) { return p > 1e-30f ? 10.0f * std::log10(p) : -300.0f; }
inline float lin_to_db(float a) { return a > 1e-15f ? 20.0f * std::log10(a) : -300.0f; }

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

// Converts PCM16 to float in [-1, 1) and back (with saturation and rounding).
inline float pcm16_to_float(int16_t s) { return static_cast<float>(s) * (1.0f / 32768.0f); }
inline int16_t float_to_pcm16(float x) {
  float v = x * 32767.0f;
  if (!(v > -32768.0f)) return -32768;  // also catches NaN
  if (v >= 32767.0f) return 32767;
  return static_cast<int16_t>(std::lrint(v));
}

// Deterministic PRNG (xorshift64*), for tests, dithering and simulation. Never seeded from the OS.
class Prng {
 public:
  explicit Prng(uint64_t seed = 1) : s_(seed ? seed : 0x9E3779B97F4A7C15ull) {}
  uint64_t next() {
    s_ ^= s_ >> 12;
    s_ ^= s_ << 25;
    s_ ^= s_ >> 27;
    return s_ * 0x2545F4914F6CDD1Dull;
  }
  uint32_t u32() { return static_cast<uint32_t>(next() >> 32); }
  // Uniform in [0, 1).
  double uniform() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
  // Uniform integer in [0, n). n must be > 0.
  uint32_t below(uint32_t n) { return static_cast<uint32_t>(uniform() * n) % n; }
  // Standard normal (Box-Muller, caches the second value).
  double gauss() {
    if (has_spare_) { has_spare_ = false; return spare_; }
    double u1 = uniform(), u2 = uniform();
    if (u1 < 1e-300) u1 = 1e-300;
    double r = std::sqrt(-2.0 * std::log(u1));
    spare_ = r * std::sin(kTwoPi * u2);
    has_spare_ = true;
    return r * std::cos(kTwoPi * u2);
  }

 private:
  uint64_t s_;
  double spare_ = 0;
  bool has_spare_ = false;
};

}  // namespace dsp
}  // namespace ember
