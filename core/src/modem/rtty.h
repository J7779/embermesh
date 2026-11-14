// EmberMesh core / modem — RTTY: 45.45 Bd ITA2 (Baudot), 170 Hz shift, AFSK.
//
// Tones: mark 2125 Hz, space 2295 Hz (amateur AFSK convention; "reverse" swaps them).
// Character: 1 start bit (space), 5 data bits LSB first, 1.5 stop bits (mark). Idle = mark.
// Code: ITA2 with US-TTY figures (as used on amateur RTTY): LTRS 0x1F, FIGS 0x1B.
// TX: txdelay_ms (>= 300 ms) of idle mark, LTRS, then the text (upper-cased; '\n' -> CR LF;
// characters without an ITA2 code are skipped), shift characters inserted as needed (and FIGS is
// repeated after a space so unshift-on-space receivers stay in sync), then txtail_ms (>= 100) mark.
//
// RX: mark/space correlators (complex mix + 1-bit boxcar), per-tone peak/valley normalisation (ATC,
// tolerates selective fading and audio tilt), async start-bit detection on the interpolated
// mark->space crossing, sampling at bit centres, stop-bit check (framing errors are discarded).
// Unshift-on-space (usos) is on by default. A squelch on the mark/space contrast |m-s|/(m+s) at the
// bit decision instants (EMA) gates output and DCD. A start edge is accepted only after >= 0.9 bit
// of mark or directly after a well-framed character (prevents RYRY misframing).
// RxQuality.snr_db: contrast-based estimate.
//
// Params: mark_hz (2125), shift_hz (170; 50..1000), baud (45.45; 20..300), stop_bits (1.5; 1..2),
//         reverse (false), usos (true), squelch (0.4; 0..1).
#pragma once

#include <memory>
#include <string>

#include "modem/modem.h"

namespace ember {
namespace modem {

std::unique_ptr<Modem> create_rtty(const std::string& name, uint32_t sample_rate, const ModemOptions& opt,
                                   std::string* error);

namespace ita2 {
// Code (0..31) for a character in the given shift (figs = false: letters), or -1.
int encode(char c, bool* needs_figs, bool* either);
// Character for a code in a shift; 0 for none / shift codes.
char decode(int code, bool figs);
constexpr int kLtrs = 0x1F, kFigs = 0x1B;
}  // namespace ita2

}  // namespace modem
}  // namespace ember
