// EmberMesh core / modem — DTMF (ITU-T Q.23 tones) encoder and decoder, as text.
//
// Characters: 0-9 * # A B C D (a-d accepted). ',' inserts a 500 ms pause. Spaces are ignored; any
// other character makes send_text() return false.
// Tones: rows 697 770 852 941 Hz x columns 1209 1336 1477 1633 Hz, each at half of tx_level.
// TX: txdelay_ms silence, then each digit tone_ms on / gap_ms off, then txtail_ms silence.
//
// RX (8 kHz internal): 205-sample (25.6 ms) Goertzel blocks on the 8 frequencies. A block detects a
// digit when the strongest row and column tone are each >= 6 dB above the other tones of their
// group, together carry >= 60% of the block energy, and the twist is within +-8 dB (radio audio is
// tilted). A digit is reported once after two consecutive detecting blocks and re-armed after a
// non-detecting block, so tone >= 60 ms and gap >= 55 ms are required (defaults are 100/100 ms).
// RxQuality.snr_db = dual-tone power vs residual power in the block, dB.
//
// Params: tone_ms (100; 40..1000), gap_ms (100; 40..1000).
#pragma once

#include <memory>
#include <string>

#include "modem/modem.h"

namespace ember {
namespace modem {

std::unique_ptr<Modem> create_dtmf(const std::string& name, uint32_t sample_rate, const ModemOptions& opt,
                                   std::string* error);

}  // namespace modem
}  // namespace ember
