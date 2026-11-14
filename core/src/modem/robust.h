// EmberMesh core / modem — "Ember robust": slow, strongly synchronised, FEC-protected MFSK for
// acoustic coupling (phone held to a radio's speaker/mic), weak signals and HF.
//
// ================================ WAVEFORM SPECIFICATION (v1) ================================
// All constants below are normative; an independent implementation following them interoperates.
//
// Sample clock: the modem is specified at fs = 8000 Hz (any device rate is resampled to it).
//
// Speeds (modem name: symbol length N samples, symbol rate R = fs/N, tone spacing = R):
//   robust250   N = 64    R = 125     Bd   tones  625.0   .. 2500.0 Hz
//   robust125   N = 128   R = 62.5    Bd          1062.5   .. 2000.0 Hz
//   robust62    N = 256   R = 31.25   Bd          1281.25  .. 1750.0 Hz   ("robust" = robust62)
//   robust31    N = 512   R = 15.625  Bd          1390.625 .. 1625.0 Hz
// Net user bit rate before overheads = 2 * R (4 bits/symbol, rate-1/2 code): 250/125/62.5/31.25 b/s.
//
// Tones: 16 tones, tone k (0..15) has frequency f_k = (B + k) * R with B = 1500/R - 7
//   (B = 5, 17, 41, 89 for N = 64, 128, 256, 512). Every tone has an integer number of cycles per
//   symbol, symbol i of tone k is sin(2*pi*f_k*n/fs), n = 0..N-1 (phase restarts each symbol, which
//   is phase continuous because the cycle count is integral). Orthogonal non-coherent MFSK.
//   A transmission starts and ends with a 5 ms raised-cosine amplitude ramp.
//
// Transmission = PREAMBLE, then for each frame: SYNC HEADER PAYLOAD, then TAIL.
//   PREAMBLE: ceil(txdelay_ms * R / 1000) symbols alternating tone 0, tone 15, tone 0, ...
//             (only for PTT/VOX settling; receivers do not need it).
//   TAIL:     max(1, ceil(txtail_ms * R / 1000)) symbols, same alternation.
//   SYNC:     16 symbols, tones = Costas array (Welch, p = 17, g = 3): c_i = (3^(i+1) mod 17) - 1
//             = 2 8 9 12 4 14 10 15 13 7 6 3 11 1 5 0.
//   HEADER:   15 symbols carrying 60 coded bits:
//               byte0 = L >> 4, byte1 = (L & 0x0F) << 4 | F, byte2 = CRC-8(byte0, byte1)
//               (L = payload length 1..4095, F = 4 flag bits, transmit 0, receivers ignore;
//                CRC-8 = ember::crc8: poly 0x07, init 0x00).
//               24 bits MSB first + 6 zero tail bits -> K=7 r=1/2 encoder (modem/conv_code.h) ->
//               60 coded bits -> INTERLEAVE(60) -> WHITEN -> MAP.
//   PAYLOAD:  bytes D[0..L-1] followed by CRC-32 (ember::crc32 of D) as 4 bytes, most significant
//             byte first; all bits MSB first; + 6 zero tail bits -> encoder -> M = 2*(8*(L+4)+6)
//             coded bits, zero-padded to a multiple of 4 -> INTERLEAVE(n) -> WHITEN -> MAP.
//             Symbols = n/4.
//   INTERLEAVE(n): output bit j (0..n-1) = input bit (a*j) mod n, where a is the smallest integer
//             >= max(1, floor(n*3819/10000)) with gcd(a, n) = 1.
//   WHITEN:   XOR each interleaved bit with a PN9 sequence restarted for every header and payload:
//             s = 0x1FF; per bit: out = s & 1; fb = (s ^ (s >> 5)) & 1; s = (s >> 1) | (fb << 8);
//             bit ^= out.
//   MAP:      consecutive groups of 4 bits b0 b1 b2 b3 (b0 first) form v = 8b0+4b1+2b2+b3;
//             tone = v ^ (v >> 1) (Gray code).
// ==============================================================================================
//
// Receiver (this implementation):
//   * search: every N/8 samples a 2N-point zero-padded FFT of the last N samples gives tone powers at
//     half-bin resolution over +-(afc_hz + R/2); rows are normalised by their mean. The sync metric
//     for each half-bin offset is the mean normalised power on the 16 Costas positions over the last
//     16 symbols (noise ~1.0). A peak above sync_threshold (3.0) that is not pinned to the edge of
//     the offset range opens a candidate at the best timing/offset (parabolic refinement of both),
//     once the metric has not improved for half a symbol. Up to 3 candidates decode concurrently.
//     While a payload is decoding, new candidates need >= half of its sync metric; when a header is
//     accepted, competing candidates with less than half its metric are dropped (so a false header
//     that passes the CRC-8 cannot block a real frame that follows).
//   * header acceptance: CRC-8 ok, <= 14 of 60 coded bits corrected, min_frame <= L <= max_frame.
//   * per symbol: direct DFT of the 16 tones + 2 guard bins with the tracked frequency offset;
//     early/late (+-N/8) energy on the decided tone steers timing (handles sample-clock drift of
//     +-1000 ppm); neighbour-bin ratio steers frequency (slow drift).
//   * soft decisions: noncoherent log-likelihoods log I0(2 sqrt(Es*E_k)/s2) per tone, exact
//     log-sum-exp per bit, then deinterleave and Viterbi. A payload whose tone contrast collapses
//     for 64 symbols (transmission gone) is abandoned.
//   * RxQuality: snr_db = channel SNR estimate in 3 kHz (Es/N0 per bin * R / 3000), freq_offset_hz,
//     corrected = coded bit errors repaired (saturating at 65535).
//   * DCD: a candidate is decoding a payload, a sync peak is being evaluated, or the in-band
//     spectrum is tone-like (row peak/mean EMA > 8).
//
// Params: afc_hz (search range +-, default 80, 10..300), sync_threshold (default 3.0, 2.0..10).
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ember/bytes.h"
#include "modem/modem.h"

namespace ember {
namespace modem {

std::unique_ptr<Modem> create_robust(const std::string& name, uint32_t sample_rate, const ModemOptions& opt,
                                     std::string* error);

// Codec layer of the waveform above (no audio), exposed for tests and alternative front ends.
namespace robust {

constexpr int kTones = 16;
constexpr int kSyncLen = 16;
constexpr int kHeaderSymbols = 15;
constexpr size_t kMaxPayload = 4095;
extern const uint8_t kCostas[kSyncLen];

// Interleaver multiplier a for n bits (see INTERLEAVE).
size_t interleave_step(size_t n);
// Number of payload symbols for an L-byte payload.
size_t payload_symbols(size_t len);
// Appends SYNC + HEADER + PAYLOAD tone indices for one frame. Returns false if len is 0 or > 4095.
bool frame_tones(ByteView payload, std::vector<uint8_t>& tones);
// Bit LLRs (4 per symbol, b0 first; > 0 means 1) from the 16 tone energies of one symbol.
// es, s2: estimated signal energy and noise energy per bin.
void symbol_llrs(const float* energies, float es, float s2, float* llr4);
// Decodes a header block from 60 LLRs (whitened, interleaved order as received).
// Returns true with *len set if the CRC-8 checks and the code fits (*bit_errs = corrected bits).
bool decode_header(const float* llr60, uint16_t* len, size_t* bit_errs);
// Decodes a payload block of `len` bytes from 4*payload_symbols(len) LLRs. True if the CRC-32 checks.
bool decode_payload(const float* llr, size_t len, Bytes* out, size_t* bit_errs);

}  // namespace robust
}  // namespace modem
}  // namespace ember
