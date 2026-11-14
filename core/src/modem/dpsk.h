// EmberMesh core / modem — fast differential PSK modems for clean audio cables (sound card <->
// radio data port / line in), HDLC framed. Not Direwolf-compatible (own definition below).
//
//   psk2400: DQPSK, 1200 Bd, 2 bit/symbol -> 2400 bit/s, carrier 1800 Hz, RRC alpha 0.5
//            (occupies ~900..2700 Hz). Internal rate 9600 Hz (8 samples/symbol).
//   psk4800: D8PSK, 1600 Bd, 3 bit/symbol -> 4800 bit/s, carrier 1800 Hz, RRC alpha 0.35
//            (occupies ~720..2880 Hz). Internal rate 9600 Hz (6 samples/symbol).
//
// Waveform (normative):
//   bits    = HDLC bit stream exactly as modem/hdlc.h (flags, stuffing, FCS, LSB first) WITHOUT
//             NRZI: txdelay_ms of flags, frames separated by one flag, then txtail_ms of flags (>= 2).
//   scramble: self-synchronising G3RUH polynomial x^17 + x^12 + 1:
//             out = in ^ s[11] ^ s[16]; then shift out into s (s[0] newest), s initially 0.
//   symbols: consecutive groups of log2(M) scrambled bits, first bit = most significant -> value b;
//            phase index k with gray(k) = b (gray(k) = k ^ (k >> 1)); phase advance
//            dphi = (2k + 1) * pi / M (never zero, so every symbol has a transition).
//   pulse:   a_n = exp(j * phi_n) at the symbol rate, root-raised-cosine (span 8 symbols),
//            s(t) = Re{ sum a_n p(t - nT) * exp(j 2 pi 1800 t) }; scaled so the peak is ~1.0.
//            Bits that do not fill a whole symbol at the end are padded with 0.
//
// Receiver: complex down-conversion, RRC matched filter, power normalisation, Gardner timing error
// detector with linear interpolation, differential detection y_k conj(y_{k-1}), nearest phase,
// Gray decode, descrambler, HDLC. Differential detection tolerates small frequency offsets
// (< ~1.5% of the symbol rate) without carrier recovery. Needs flat-ish audio: no FM
// pre/de-emphasis path (use the radio's 9600/data port or a flat line input).
// RxQuality.snr_db = 1 / (2 * var(phase error)) in dB (symbol SNR estimate).
// DCD: phase-error EMA small (constellation recognised) or an HDLC frame in progress.
//
// Params: carrier_hz (1800; 1200..2400).
#pragma once

#include <memory>
#include <string>

#include "modem/modem.h"

namespace ember {
namespace modem {

std::unique_ptr<Modem> create_dpsk(const std::string& name, uint32_t sample_rate, const ModemOptions& opt,
                                   std::string* error);

}  // namespace modem
}  // namespace ember
