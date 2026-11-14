#include "fec/reed_solomon.h"

#include <algorithm>

#include "fec/gf256.h"

namespace ember::fec {

namespace gf = gf256;

ReedSolomon::ReedSolomon(size_t nroots, uint8_t fcr) {
  if (nroots == 0 || nroots >= kMaxN) return;  // invalid: valid() == false
  nroots_ = nroots;
  fcr_ = fcr;
  gen_.fill(0);
  gen_[0] = 1;
  for (size_t i = 0; i < nroots; ++i) {
    uint8_t r = gf::exp(static_cast<unsigned>(fcr) + static_cast<unsigned>(i));
    // multiply by (x + r)
    for (size_t j = i + 1; j > 0; --j) gen_[j] = static_cast<uint8_t>(gen_[j - 1] ^ gf::mul(gen_[j], r));
    gen_[0] = gf::mul(gen_[0], r);
  }
}

bool ReedSolomon::encode(ByteView data, uint8_t* parity) const {
  if (!valid() || data.empty() || data.size() > max_data()) return false;
  const size_t n = nroots_;
  for (size_t j = 0; j < n; ++j) parity[j] = 0;
  for (uint8_t d : data) {
    uint8_t fb = static_cast<uint8_t>(d ^ parity[0]);
    if (fb) {
      uint8_t lfb = gf::log(fb);
      for (size_t j = 0; j + 1 < n; ++j) {
        uint8_t g = gen_[n - 1 - j];
        parity[j] = static_cast<uint8_t>(parity[j + 1] ^ (g ? gf::kTables.exp[lfb + gf::log(g)] : 0));
      }
      parity[n - 1] = gen_[0] ? gf::kTables.exp[lfb + gf::log(gen_[0])] : 0;
    } else {
      for (size_t j = 0; j + 1 < n; ++j) parity[j] = parity[j + 1];
      parity[n - 1] = 0;
    }
  }
  return true;
}

Bytes ReedSolomon::encode(ByteView data) const {
  if (!valid() || data.empty() || data.size() > max_data()) return Bytes();
  Bytes out(data.size() + nroots_);
  std::copy(data.begin(), data.end(), out.begin());
  encode(data, out.data() + data.size());
  return out;
}

bool ReedSolomon::syndromes(const uint8_t* cw, size_t len, uint8_t* s) const {
  uint8_t any = 0;
  for (size_t j = 0; j < nroots_; ++j) {
    uint8_t root = gf::exp(static_cast<unsigned>(fcr_) + static_cast<unsigned>(j));
    uint8_t acc = 0;
    if (root == 0) { s[j] = 0; continue; }
    unsigned lr = gf::log(root);
    for (size_t i = 0; i < len; ++i) {
      acc = acc ? static_cast<uint8_t>(gf::kTables.exp[gf::log(acc) + lr] ^ cw[i]) : cw[i];
    }
    s[j] = acc;
    any |= acc;
  }
  return any == 0;
}

bool ReedSolomon::check(ByteView cw) const {
  if (!valid() || cw.size() <= nroots_ || cw.size() > kMaxN) return false;
  uint8_t s[256];
  return syndromes(cw.data(), cw.size(), s);
}

int ReedSolomon::decode(uint8_t* cw, size_t len, const size_t* eras, size_t ne) const {
  if (!valid() || cw == nullptr || len <= nroots_ || len > kMaxN) return -1;
  const size_t nr = nroots_;
  uint8_t s[256];
  if (syndromes(cw, len, s)) {
    // Clean codeword; still validate the erasure list so misuse is reported consistently.
    if (ne > nr) return -1;
    return 0;
  }
  if (ne > nr || (ne && eras == nullptr)) return -1;

  uint8_t lambda[256] = {0};
  uint8_t b[256] = {0};
  uint8_t t[256] = {0};
  lambda[0] = 1;
  {
    bool seen[256] = {false};
    for (size_t e = 0; e < ne; ++e) {
      size_t p = eras[e];
      if (p >= len || seen[p]) return -1;
      seen[p] = true;
      uint8_t x = gf::exp(static_cast<unsigned>(len - 1 - p));
      for (size_t k = e + 1; k > 0; --k) lambda[k] = static_cast<uint8_t>(lambda[k] ^ gf::mul(x, lambda[k - 1]));
    }
  }
  for (size_t k = 0; k <= nr; ++k) b[k] = lambda[k];

  size_t el = ne;
  for (size_t r = ne; r < nr; ++r) {
    uint8_t d = 0;
    for (size_t i = 0; i <= r; ++i) d = static_cast<uint8_t>(d ^ gf::mul(lambda[i], s[r - i]));
    if (d == 0) {
      for (size_t k = nr; k > 0; --k) b[k] = b[k - 1];
      b[0] = 0;
      continue;
    }
    t[0] = lambda[0];
    for (size_t k = 1; k <= nr; ++k) t[k] = static_cast<uint8_t>(lambda[k] ^ gf::mul(d, b[k - 1]));
    if (2 * el <= r + ne) {
      el = r + 1 + ne - el;
      uint8_t dinv = gf::inv(d);
      for (size_t k = 0; k <= nr; ++k) b[k] = gf::mul(lambda[k], dinv);
    } else {
      for (size_t k = nr; k > 0; --k) b[k] = b[k - 1];
      b[0] = 0;
    }
    for (size_t k = 0; k <= nr; ++k) lambda[k] = t[k];
  }

  size_t deg = 0;
  for (size_t k = 0; k <= nr; ++k)
    if (lambda[k]) deg = k;
  if (deg == 0 || deg > nr || deg != el) return -1;
  // errors + erasures must fit: 2*(deg - ne) + ne <= nr
  if (deg < ne || 2 * (deg - ne) + ne > nr) return -1;

  // Chien search over the real positions only.
  size_t loc[256];
  size_t nloc = 0;
  for (size_t p = 0; p < len; ++p) {
    unsigned e = static_cast<unsigned>(len - 1 - p);
    unsigned xinv_log = (255 - e % 255) % 255;  // log of X^{-1}
    uint8_t v = 0;
    for (size_t k = 0; k <= deg; ++k)
      if (lambda[k]) v = static_cast<uint8_t>(v ^ gf::kTables.exp[(gf::log(lambda[k]) + xinv_log * k) % 255]);
    if (v == 0) {
      if (nloc >= deg) return -1;
      loc[nloc++] = p;
    }
  }
  if (nloc != deg) return -1;

  // Omega(x) = S(x) * Lambda(x) mod x^nr
  uint8_t omega[256] = {0};
  for (size_t k = 0; k < nr; ++k) {
    uint8_t acc = 0;
    for (size_t i = 0; i <= k && i <= deg; ++i) acc = static_cast<uint8_t>(acc ^ gf::mul(lambda[i], s[k - i]));
    omega[k] = acc;
  }

  uint8_t mags[256];
  for (size_t j = 0; j < nloc; ++j) {
    unsigned e = static_cast<unsigned>(len - 1 - loc[j]);
    unsigned xinv_log = (255 - e % 255) % 255;
    uint8_t num = 0;
    for (size_t k = 0; k < nr; ++k)
      if (omega[k]) num = static_cast<uint8_t>(num ^ gf::kTables.exp[(gf::log(omega[k]) + xinv_log * k) % 255]);
    uint8_t den = 0;
    for (size_t k = 1; k <= deg; k += 2)
      if (lambda[k]) den = static_cast<uint8_t>(den ^ gf::kTables.exp[(gf::log(lambda[k]) + xinv_log * (k - 1)) % 255]);
    if (den == 0) return -1;
    unsigned a = (256u - fcr_) % 255u;  // (1 - fcr) mod 255
    uint8_t xpow = gf::exp((e % 255) * a);
    mags[j] = gf::mul(xpow, gf::div(num, den));
  }

  int changed = 0;
  for (size_t j = 0; j < nloc; ++j) {
    if (mags[j]) {
      cw[loc[j]] ^= mags[j];
      ++changed;
    }
  }
  if (!syndromes(cw, len, s)) {
    for (size_t j = 0; j < nloc; ++j) cw[loc[j]] ^= mags[j];  // undo
    return -1;
  }
  return changed;
}

}  // namespace ember::fec
