// EmberMesh core / modem — CW (Morse) keyer and adaptive-speed decoder.
//
// TX: on/off keyed sine at tone_hz with raised-cosine key edges (rise_ms) to avoid clicks. Timing
// unit = 1.2 / wpm seconds: dit 1, dah 3, element gap 1, character gap 3, word gap 7 units.
// Farnsworth (farnsworth_wpm < wpm): characters at wpm, extra spacing so the overall speed is
// farnsworth_wpm (ARRL formula: ta = (60*c - 37.2*s)/(s*c) s; char gap = 3ta/19, word gap = 7ta/19).
// txdelay_ms / txtail_ms are silence (for PTT). Text: see morse.h (prosigns as "<SK>").
//
// RX: complex mix to baseband at the (auto-tracked) tone, decimating low-pass (bw_hz) to a 500 Hz
// envelope, adaptive floor/peak thresholds with hysteresis and a squelch (peak >= 3x floor), then:
//   * marks are split into dit/dah by 2-cluster tracking (geometric-mean threshold), so the decoder
//     locks to 5..60 wpm within a few characters regardless of the starting estimate;
//   * gaps: > 2 dots ends a character; > max(5 dots, 5/3 of the running character gap) is a word
//     gap (handles Farnsworth spacing).
//   * AFC: when idle the strongest steady tone in 300..1500 Hz (2 s averaged spectrum, > 10 dB over
//     the median) becomes the receive tone.
// Output: RxKind::Text with characters, ' ' for word gaps, "<SK>" for prosigns, '*' for unknown
// patterns. RxQuality.snr_db = keying envelope peak/floor ratio in dB.
//
// Params: wpm (20; 5..60), farnsworth_wpm (0 = off), tone_hz (700; 300..2500), rise_ms (5),
//         bw_hz (100; 30..400), afc (true).
#pragma once

#include <memory>
#include <string>

#include "modem/modem.h"

namespace ember {
namespace modem {

std::unique_ptr<Modem> create_cw(const std::string& name, uint32_t sample_rate, const ModemOptions& opt,
                                 std::string* error);

}  // namespace modem
}  // namespace ember
