// EmberMesh store — on-disk format codecs. See store/kv_format.h.
#include "store/kv_format.h"

#include <cstring>

namespace ember::store::fmt {
namespace {

struct CrcTable {
  uint32_t t[256];
  constexpr CrcTable() : t() {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
      t[i] = c;
    }
  }
};
constexpr CrcTable kCrc;

constexpr uint8_t kMagic[8] = {'E', 'M', 'B', 'E', 'R', 'K', 'V', 0};

void put_le(uint8_t* p, uint64_t v, int n) {
  for (int i = 0; i < n; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
uint64_t get_le(const uint8_t* p, int n) {
  uint64_t v = 0;
  for (int i = 0; i < n; ++i) v |= uint64_t(p[i]) << (8 * i);
  return v;
}

std::string make_name(const char* prefix, uint64_t gen) {
  char buf[24];
  int n = 0;
  do {
    buf[n++] = static_cast<char>('0' + gen % 10);
    gen /= 10;
  } while (gen);
  std::string s(prefix);
  while (n) s.push_back(buf[--n]);
  s += ".ekv";
  return s;
}

}  // namespace

uint32_t crc32(ByteView data, uint32_t prev) {
  uint32_t c = ~prev;
  for (uint8_t b : data) c = kCrc.t[(c ^ b) & 0xFF] ^ (c >> 8);
  return ~c;
}

Bytes encode_header(const Header& h) {
  Bytes b(kHeaderSize, 0);
  std::memcpy(b.data(), kMagic, 8);
  put_le(&b[8], h.version, 4);
  put_le(&b[12], h.flags, 4);
  put_le(&b[16], h.generation, 8);
  put_le(&b[24], h.base_seq, 8);
  std::memcpy(&b[32], h.salt, 16);
  size_t pl = h.params.size() <= kParamsMax ? h.params.size() : kParamsMax;
  put_le(&b[48], pl, 2);
  if (pl) std::memcpy(&b[50], h.params.data(), pl);
  size_t kl = h.keycheck.size() <= kKeyCheckMax ? h.keycheck.size() : kKeyCheckMax;
  put_le(&b[306], kl, 2);
  if (kl) std::memcpy(&b[308], h.keycheck.data(), kl);
  put_le(&b[508], crc32(ByteView(b.data(), 508)), 4);
  return b;
}

Bytes header_aad(const Header& h) {
  Bytes b = encode_header(h);
  b.resize(kKeyCheckAadLen);
  return b;
}

bool decode_header(ByteView b, Header* h) {
  if (b.size() < kHeaderSize) return false;
  if (std::memcmp(b.data(), kMagic, 8) != 0) return false;
  if (crc32(b.sub(0, 508)) != static_cast<uint32_t>(get_le(b.data() + 508, 4))) return false;
  size_t pl = static_cast<size_t>(get_le(b.data() + 48, 2));
  size_t kl = static_cast<size_t>(get_le(b.data() + 306, 2));
  if (pl > kParamsMax || kl > kKeyCheckMax) return false;
  h->version = static_cast<uint32_t>(get_le(b.data() + 8, 4));
  h->flags = static_cast<uint32_t>(get_le(b.data() + 12, 4));
  h->generation = get_le(b.data() + 16, 8);
  h->base_seq = get_le(b.data() + 24, 8);
  std::memcpy(h->salt, b.data() + 32, 16);
  h->params.assign(b.data() + 50, b.data() + 50 + pl);
  h->keycheck.assign(b.data() + 308, b.data() + 308 + kl);
  return true;
}

std::string data_name(uint64_t gen) { return make_name("data-", gen); }
std::string tmp_name(uint64_t gen) { return make_name("tmp-", gen); }

bool parse_name(const std::string& name, char* kind, uint64_t* gen) {
  size_t start;
  if (name.compare(0, 5, "data-") == 0) {
    *kind = 'd';
    start = 5;
  } else if (name.compare(0, 4, "tmp-") == 0) {
    *kind = 't';
    start = 4;
  } else {
    return false;
  }
  if (name.size() < start + 1 + 4 || name.compare(name.size() - 4, 4, ".ekv") != 0) return false;
  size_t end = name.size() - 4;
  if (end - start > 20) return false;
  if (name[start] == '0' && end - start > 1) return false;
  uint64_t v = 0;
  for (size_t i = start; i < end; ++i) {
    char c = name[i];
    if (c < '0' || c > '9') return false;
    uint64_t d = static_cast<uint64_t>(c - '0');
    if (v > (UINT64_MAX - d) / 10) return false;
    v = v * 10 + d;
  }
  *gen = v;
  return true;
}

}  // namespace ember::store::fmt
