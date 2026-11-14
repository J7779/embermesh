// EmberMesh core / modem — AFSK + bit-level HDLC packet modems.
//
//   afsk1200: Bell 202. Mark (line level 1) 1200 Hz, space 2200 Hz, 1200 bd, NRZI, HDLC
//             (modem/hdlc.h). Interoperates with APRS / AX.25 1200 bd packet (TNCs, Direwolf,
//             radios with built-in TNC). Internal rate 12 kHz.
//   afsk300:  HF packet. Mark 1600 Hz, space 1800 Hz (200 Hz shift, as Direwolf's default for 300 bd),
//             300 bd, same framing. Internal rate 9600 Hz. (Some HF gear uses 1600/1800 inverted or
//             other centre tones: set mark_hz / space_hz.)
//
// TX: phase-continuous FSK. txdelay_ms of flags, the queued frames back to back separated by one
// flag, then txtail_ms of flags (>= 1). 1 ms raised-cosine ramps at both ends.
//
// RX demodulator (designed for ugly handheld-radio audio):
//   band-pass FIR -> mark & space quadrature correlators (complex mix + 1.2-bit boxcar; measured
//   better than Hann/Hamming windows of 1..2.5 bits on flat and tilted audio) ->
//   envelopes |m|, |s| -> N parallel slicers, each with its own bit-clock PLL, NRZI decoder and HDLC
//   decoder:
//     slicer 0:   per-tone peak/valley normalisation (automatic twist compensation, like the "ATC"
//                 of hardware demodulators)
//     slicers 1+: |m| - g*|s| for g in {1, 0.5, 0.71, 1.41, 2, 2.83} — fixed twist hypotheses
//                 covering pre-emphasised (space boosted) to de-emphasised (space cut up to 9 dB)
//                 audio.
//   The first slicer to complete a frame wins; identical frames from other slicers within 12 bit
//   times are suppressed. RxQuality.decoder tells which slicer decoded it.
//   PLL: phase advance baud/fs per sample, sample at phase wrap, transitions steer the phase toward
//   0.5 with gain 0.5 (searching) / 0.25 (after a flag), using the interpolated zero crossing.
//
// RxQuality.snr_db is the "eye SNR": mean^2/variance of the slicer output sampled at the bit
// decisions over the frame (post-detection quality; ~10 dB = marginal, > 20 dB = clean).
//
// DCD: some slicer has >= 22 of its last 32 transitions within +-0.15 bit of the expected
// position, or an HDLC frame is being collected right after a flag.
//
// Params (ModemOptions::params):
//   mark_hz, space_hz, baud   override tones / rate (50..2400 bd, tones 300..3500 Hz)
//   slicers                   1 = single classic decoder (slicer 1, g = 1), 7 = all (default)
//   corr_len, corr_win, corr_cut   correlator experiments: length in bits (1.2), window
//                             (0 = boxcar, 1 Hann, 2 Hamming, 3 Blackman, 4 Kaiser), low-pass cutoff
//                             in units of baud for windows other than the boxcar
#pragma once

#include <memory>
#include <string>

#include "modem/modem.h"

namespace ember {
namespace modem {

// Factory hook for "afsk1200" / "afsk300".
std::unique_ptr<Modem> create_afsk(const std::string& name, uint32_t sample_rate, const ModemOptions& opt,
                                   std::string* error);

}  // namespace modem
}  // namespace ember
