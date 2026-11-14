// EmberMesh core / dsp — narrow-band FM demodulator for SDR receive (rtl_tcp style IQ).
//
// Input: interleaved unsigned 8-bit I/Q (I0 Q0 I1 Q1 ..., 127.5 = zero), as streamed by rtl_tcp /
// rtl_sdr, at any rate from 32 kHz to 3.2 MS/s (typically 240 k, 1.024 M, 2.048 M, 2.4 M).
// Or complex float via process_cf().
//
// Pipeline:  u8 -> complex float -> NCO tune (offset_hz) -> multistage decimating FIR (factor <= 8 per
// stage, Kaiser 60 dB) down to an IF rate >= if_min_hz -> channel FIR (+-channel_bw/2) ->
// quadrature discriminator angle(z[n] * conj(z[n-1])) scaled so +-deviation -> +-1.0 ->
// optional de-emphasis -> arbitrary-ratio resampler to audio_rate -> 4th-order audio low-pass ->
// DC blocker (removes the DC caused by residual mistuning).
//
// CPU at 2.048 MS/s is dominated by the first stage (~4 complex MACs per input sample).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "dsp/agc.h"
#include "dsp/biquad.h"
#include "dsp/dsp.h"
#include "dsp/fir.h"
#include "dsp/nco.h"
#include "dsp/resampler.h"

namespace ember {
namespace dsp {

struct NbfmConfig {
  uint32_t iq_rate = 2048000;   // complex samples per second
  uint32_t audio_rate = 48000;  // output PCM rate (any >= 8000)
  double offset_hz = 0;         // the wanted signal sits at +offset_hz in the IQ spectrum
  double channel_bw_hz = 12500; // two-sided channel filter width (12.5 kHz NBFM; 25 kHz wide)
  double deviation_hz = 5000;   // peak deviation that maps to audio amplitude 1.0
  double deemphasis_us = 0;     // 0 = flat (best for data); 530/750 us = typical NBFM voice
  double audio_lp_hz = 4000;    // audio low-pass corner (clamped below audio_rate/2)
  double if_min_hz = 24000;     // decimate to the lowest integer-ratio rate >= this (and >= channel bw)
};

class NbfmDemod {
 public:
  NbfmDemod() = default;
  bool init(const NbfmConfig& cfg);
  const NbfmConfig& config() const { return cfg_; }
  double if_rate() const { return if_rate_; }
  // Interleaved unsigned 8-bit IQ. Odd trailing bytes are kept for the next call. Appends audio.
  void process_u8(const uint8_t* iq, size_t nbytes, std::vector<float>& audio);
  void process_cf(const Cf* iq, size_t n, std::vector<float>& audio);
  // Smoothed mean |z|^2 at the channel filter output (signal + noise power, full scale IQ = 2.0).
  float channel_power() const { return chan_pow_; }
  void reset();

 private:
  void push_if(Cf z, std::vector<float>& audio_if);

  NbfmConfig cfg_;
  bool ok_ = false;
  Nco nco_;
  bool tune_ = false;
  std::vector<FirDecimC> stages_;
  FirC chan_;
  double if_rate_ = 0;
  Cf prev_{1, 0};
  float scale_ = 1;
  Biquad deemph_;
  bool use_deemph_ = false;
  Resampler rs_;
  Biquad lp1_, lp2_;
  bool use_lp_ = false;
  DcBlocker dc_;
  std::vector<float> if_audio_, rs_out_;
  std::vector<Cf> stage_buf_;
  int pending_byte_ = -1;
  float chan_pow_ = 0;
};

}  // namespace dsp
}  // namespace ember
