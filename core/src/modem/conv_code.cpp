#include "modem/conv_code.h"

namespace ember {
namespace modem {

namespace {
constexpr uint8_t kG0 = 0171, kG1 = 0133;

inline uint8_t parity7(uint8_t v) {
  v ^= v >> 4;
  v ^= v >> 2;
  v ^= v >> 1;
  return v & 1;
}

struct Table {
  uint8_t c0[128], c1[128];
  Table() {
    for (int s = 0; s < 128; ++s) {
      c0[s] = parity7(static_cast<uint8_t>(s & kG0));
      c1[s] = parity7(static_cast<uint8_t>(s & kG1));
    }
  }
};
const Table& table() {
  static const Table t;
  return t;
}
}  // namespace

void conv_encode(const uint8_t* bits, size_t n, std::vector<uint8_t>& out) {
  const Table& t = table();
  uint8_t sr = 0;
  out.reserve(out.size() + 2 * n);
  for (size_t i = 0; i < n; ++i) {
    sr = static_cast<uint8_t>(((sr << 1) | (bits[i] & 1)) & 0x7F);
    out.push_back(t.c0[sr]);
    out.push_back(t.c1[sr]);
  }
}

size_t Viterbi::decode(const float* llr, size_t n, std::vector<uint8_t>& out) {
  out.assign(n, 0);
  if (!n || !llr) return 0;
  const Table& t = table();
  dec_.assign(n, 0);
  float m[64], nm[64];
  const float kNeg = -1e30f;
  for (int s = 0; s < 64; ++s) m[s] = kNeg;
  m[0] = 0;
  for (size_t i = 0; i < n; ++i) {
    float l0 = llr[2 * i], l1 = llr[2 * i + 1];
    // Branch metric for register value sr: correlation of expected bits with the LLRs.
    float bm[4] = {-l0 - l1, -l0 + l1, l0 - l1, l0 + l1};  // index = c0*2 + c1
    uint64_t d = 0;
    float best = kNeg;
    for (int ns = 0; ns < 64; ++ns) {
      // Predecessors: p = (ns >> 1) | (x << 5); register = ns | (x << 6).
      int p0 = ns >> 1, p1 = p0 | 32;
      int r0 = ns, r1 = ns | 64;
      float a = m[p0] + bm[t.c0[r0] * 2 + t.c1[r0]];
      float b = m[p1] + bm[t.c0[r1] * 2 + t.c1[r1]];
      if (b > a) {
        nm[ns] = b;
        d |= (uint64_t)1 << ns;
      } else {
        nm[ns] = a;
      }
      if (nm[ns] > best) best = nm[ns];
    }
    dec_[i] = d;
    for (int s = 0; s < 64; ++s) m[s] = nm[s] - best;  // keep metrics bounded
  }
  int s = 0;
  for (size_t i = n; i-- > 0;) {
    out[i] = static_cast<uint8_t>(s & 1);
    int x = static_cast<int>((dec_[i] >> s) & 1);
    s = (s >> 1) | (x << 5);
  }
  // Count corrected hard decisions.
  size_t errs = 0;
  uint8_t sr = 0;
  for (size_t i = 0; i < n; ++i) {
    sr = static_cast<uint8_t>(((sr << 1) | out[i]) & 0x7F);
    float l0 = llr[2 * i], l1 = llr[2 * i + 1];
    if (l0 != 0 && (l0 > 0) != (t.c0[sr] == 1)) ++errs;
    if (l1 != 0 && (l1 > 0) != (t.c1[sr] == 1)) ++errs;
  }
  return errs;
}

}  // namespace modem
}  // namespace ember
