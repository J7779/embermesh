#include "base/codec.h"

namespace ember {

namespace {

const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const char kB64Url[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
const char kB32[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

std::string b64_encode(ByteView d, const char* alpha, bool pad) {
  std::string out;
  out.reserve((d.size() + 2) / 3 * 4);
  size_t i = 0;
  for (; i + 3 <= d.size(); i += 3) {
    uint32_t v = (uint32_t(d[i]) << 16) | (uint32_t(d[i + 1]) << 8) | d[i + 2];
    out.push_back(alpha[(v >> 18) & 63]);
    out.push_back(alpha[(v >> 12) & 63]);
    out.push_back(alpha[(v >> 6) & 63]);
    out.push_back(alpha[v & 63]);
  }
  size_t rem = d.size() - i;
  if (rem == 1) {
    uint32_t v = uint32_t(d[i]) << 16;
    out.push_back(alpha[(v >> 18) & 63]);
    out.push_back(alpha[(v >> 12) & 63]);
    if (pad) out += "==";
  } else if (rem == 2) {
    uint32_t v = (uint32_t(d[i]) << 16) | (uint32_t(d[i + 1]) << 8);
    out.push_back(alpha[(v >> 18) & 63]);
    out.push_back(alpha[(v >> 12) & 63]);
    out.push_back(alpha[(v >> 6) & 63]);
    if (pad) out.push_back('=');
  }
  return out;
}

// mode: 0 = std, 1 = url, 2 = either
int b64_val(char c, int mode) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+' && mode != 1) return 62;
  if (c == '/' && mode != 1) return 63;
  if (c == '-' && mode != 0) return 62;
  if (c == '_' && mode != 0) return 63;
  return -1;
}

bool b64_decode(std::string_view t, Bytes& out, int mode) {
  out.clear();
  // Strip padding (at most 2, only when the padded length is a multiple of 4).
  size_t n = t.size();
  size_t pads = 0;
  while (n > 0 && t[n - 1] == '=' && pads < 3) { --n; ++pads; }
  if (pads > 2) return false;
  if (pads && t.size() % 4 != 0) return false;
  if (n % 4 == 1) return false;
  if (pads && ((n % 4 == 2 && pads != 2) || (n % 4 == 3 && pads != 1) || n % 4 == 0)) return false;
  out.reserve(n * 3 / 4);
  uint32_t acc = 0;
  int bits = 0;
  for (size_t i = 0; i < n; ++i) {
    int v = b64_val(t[i], mode);
    if (v < 0) { out.clear(); return false; }
    acc = (acc << 6) | static_cast<uint32_t>(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<uint8_t>(acc >> bits));
      acc &= (1u << bits) - 1;
    }
  }
  if (acc != 0) { out.clear(); return false; }  // non-canonical trailing bits
  return true;
}

int b32_val(char c) {
  if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  if (c == 'O') return 0;
  if (c == 'I' || c == 'L') return 1;
  for (int i = 0; i < 32; ++i)
    if (kB32[i] == c) return i;
  return -1;
}

}  // namespace

std::string base64_encode(ByteView data) { return b64_encode(data, kB64, true); }
std::string base64url_encode(ByteView data) { return b64_encode(data, kB64Url, false); }
bool base64_decode(std::string_view text, Bytes& out) { return b64_decode(text, out, 0); }
bool base64url_decode(std::string_view text, Bytes& out) { return b64_decode(text, out, 1); }
bool base64_decode_lenient(std::string_view text, Bytes& out) {
  std::string clean;
  clean.reserve(text.size());
  for (char c : text)
    if (c != ' ' && c != '\n' && c != '\r' && c != '\t') clean.push_back(c);
  return b64_decode(clean, out, 2);
}

std::string base32_encode(ByteView data) {
  std::string out;
  out.reserve((data.size() * 8 + 4) / 5);
  uint32_t acc = 0;
  int bits = 0;
  for (uint8_t b : data) {
    acc = (acc << 8) | b;
    bits += 8;
    while (bits >= 5) {
      bits -= 5;
      out.push_back(kB32[(acc >> bits) & 31]);
    }
    acc &= (1u << bits) - 1;
  }
  if (bits > 0) out.push_back(kB32[(acc << (5 - bits)) & 31]);
  return out;
}

bool base32_decode(std::string_view text, Bytes& out) {
  out.clear();
  uint32_t acc = 0;
  int bits = 0;
  size_t symbols = 0;
  for (char c : text) {
    if (c == '-' || c == ' ') continue;
    int v = b32_val(c);
    if (v < 0) { out.clear(); return false; }
    ++symbols;
    acc = (acc << 5) | static_cast<uint32_t>(v);
    bits += 5;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<uint8_t>(acc >> bits));
    }
    acc &= (1u << bits) - 1;
  }
  // Valid symbol counts leave 0..4 spare bits that must be zero; counts like 1, 3, 6 (mod 8) are
  // impossible encodings.
  size_t m = symbols % 8;
  if (m == 1 || m == 3 || m == 6 || acc != 0) { out.clear(); return false; }
  return true;
}

}  // namespace ember
