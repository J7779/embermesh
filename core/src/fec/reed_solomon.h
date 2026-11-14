// EmberMesh FEC — systematic Reed-Solomon codes over GF(256) (fec/gf256.h, poly 0x11D, alpha=2).
//
// RS(n, k): n = k + nroots <= 255 symbols (bytes). Generator g(x) = prod_{i=0}^{nroots-1}
// (x - alpha^(fcr+i)). Codewords are data || parity, first byte = highest-degree coefficient.
// Shortened codes are implicit: any data length 1..255-nroots works with the same object (the
// missing leading symbols are taken as zero). With fcr = 0 the parity matches QR codes / ISO
// 18004 ("HELLO WORLD" 1-M vector in the tests).
//
// Decoding corrects any combination of e errors and f erasures (known-bad positions) with
// 2e + f <= nroots, using Berlekamp-Massey (erasure-initialised), Chien search restricted to the
// real (shortened) positions, and Forney. Every decode result is re-verified by recomputing the
// syndromes, so decode() never returns success with a non-codeword; beyond the correction radius
// it fails cleanly (or, with probability roughly 1/t! for heavy corruption, mis-corrects to a
// different valid codeword — use a CRC on top, as fec/packet.h does).
//
// Memory: ~260 bytes per object, no heap. Thread-compatible (decode is const).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ember/bytes.h"

namespace ember::fec {

class ReedSolomon {
 public:
  static constexpr size_t kMaxN = 255;

  // nroots: number of parity symbols, 1..254. fcr: first consecutive root exponent (0 or 1 are
  // common; CCSDS uses 112 with a different field, so it is not bit-compatible with CCSDS).
  explicit ReedSolomon(size_t nroots, uint8_t fcr = 0);

  bool valid() const { return nroots_ != 0; }
  size_t nroots() const { return nroots_; }
  size_t max_data() const { return kMaxN - nroots_; }  // k of the unshortened code
  uint8_t fcr() const { return fcr_; }

  // Computes nroots parity bytes for data (1..max_data() bytes). Returns false on bad sizes.
  bool encode(ByteView data, uint8_t* parity) const;
  // Returns data || parity, or empty on bad sizes.
  Bytes encode(ByteView data) const;

  // Decodes a codeword of `len` bytes (data_len + nroots, i.e. nroots+1 .. 255) in place.
  // erasures: optional list of positions (0-based index into codeword) known to be unreliable;
  // duplicates or out-of-range positions make the call fail.
  // Returns the number of symbols changed (0 if the codeword was clean), or -1 if uncorrectable
  // (the buffer is then left exactly as passed in).
  int decode(uint8_t* codeword, size_t len, const size_t* erasures = nullptr, size_t n_erasures = 0) const;
  int decode(Bytes& codeword, const size_t* erasures = nullptr, size_t n_erasures = 0) const {
    return decode(codeword.data(), codeword.size(), erasures, n_erasures);
  }

  // True if all syndromes are zero (cheap check without correcting).
  bool check(ByteView codeword) const;

  // Generator polynomial coefficient of x^i (i = 0..nroots; coefficient nroots is 1).
  uint8_t generator(size_t i) const { return i <= nroots_ ? gen_[i] : 0; }

 private:
  bool syndromes(const uint8_t* cw, size_t len, uint8_t* s) const;  // returns true if all zero
  size_t nroots_ = 0;
  uint8_t fcr_ = 0;
  std::array<uint8_t, 256> gen_{};
};

}  // namespace ember::fec
