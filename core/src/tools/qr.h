// EmberMesh core — QR Code Model 2 encoder (ISO/IEC 18004), byte mode, versions 1-40, ECC L/M/Q/H.
// Pure computation; output is a square matrix of modules (true = dark). Mirrors RPC `tools.qr`.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ember/bytes.h"

namespace ember {
namespace qr {

enum class Ecc : int { L = 0, M = 1, Q = 2, H = 3 };

struct QrCode {
  int version = 0;   // 1..40
  int size = 0;      // 17 + 4·version
  Ecc ecc = Ecc::M;
  int mask = -1;     // 0..7
  std::vector<uint8_t> modules;  // size*size, row-major, 1 = dark
  bool get(int x, int y) const { return modules[static_cast<size_t>(y) * size + x] != 0; }
  // Rows as "0101…" strings (RPC tools.qr shape).
  std::vector<std::string> rows() const;
};

struct EncodeOptions {
  Ecc ecc = Ecc::M;
  int min_version = 1;
  int max_version = 40;
  int force_mask = -1;     // -1 = choose the lowest-penalty mask
  bool boost_ecc = false;  // raise ECC level when it still fits the chosen version
};

// Encodes `data` in byte mode. Returns false (and sets *error) if the data does not fit in
// max_version at the requested ECC level, or if options are invalid.
bool encode(ByteView data, const EncodeOptions& opt, QrCode* out, std::string* error);
inline bool encode_text(const std::string& text, Ecc ecc, QrCode* out, std::string* error) {
  EncodeOptions o;
  o.ecc = ecc;
  return encode(ByteView(std::string_view(text)), o, out, error);
}

bool parse_ecc(const std::string& s, Ecc* out);  // "L","M","Q","H" (case-insensitive)
// Max byte-mode payload length for a version / ECC level.
int byte_capacity(int version, Ecc ecc);

}  // namespace qr
}  // namespace ember
