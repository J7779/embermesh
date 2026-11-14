// EmberMesh core / modem — PSK31 (BPSK31, G3PLX) keyboard mode.
//
// 31.25 Bd differential BPSK on a single carrier (center_hz, default 1000 Hz). Bit 0 = phase
// reversal, bit 1 = no change. Reversals are amplitude shaped: the envelope follows cos(pi t/T)
// through zero across the symbol, giving the standard ~60 Hz wide PSK31 spectrum.
// Characters use the standard PSK31 varicode (ASCII 0..127), each followed by "00". Idle = zeros.
// TX: max(32, txdelay) symbols of reversals as preamble, text, max(16, txtail) reversals, with a
// one-symbol amplitude ramp at each end. '\n' is sent as LF; received CR is dropped.
//
// RX: mix to baseband with AFC, decimate to 500 Hz (16 samples/symbol), Hann matched filter, symbol
// timing from the per-phase average |z|^2 (amplitude dips at reversals), differential detection
// Re(z_k conj z_{k-1}), AFC from a squaring loop at 500 Hz (z^2 rotation; captures +-100 Hz, gated
// on its coherence). A squelch on the BPSK constellation
// quality EMA (Re(d^2)/|d|^2: ~1 for a clean signal, ~0 for noise) gates text output and DCD; it
// closes at once when the symbol amplitude collapses (end of transmission).
// RxQuality.snr_db estimated from the constellation quality.
//
// Params: center_hz (1000; 300..3000), squelch (0.35; 0..1), afc (true).
#pragma once

#include <memory>
#include <string>

#include "modem/modem.h"

namespace ember {
namespace modem {

std::unique_ptr<Modem> create_psk31(const std::string& name, uint32_t sample_rate, const ModemOptions& opt,
                                    std::string* error);

namespace psk31 {
// Varicode for ASCII c (0..127) as a string of '0'/'1' (without the "00" separator), or nullptr.
const char* varicode(unsigned char c);
// Inverse lookup: returns the ASCII code or -1.
int varicode_decode(const char* bits, size_t n);
}  // namespace psk31

}  // namespace modem
}  // namespace ember
