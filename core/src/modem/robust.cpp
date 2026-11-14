#include "modem/robust.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "dsp/dsp.h"
#include "dsp/fft.h"
#include "dsp/nco.h"
#include "ember/crc.h"
#include "modem/conv_code.h"

namespace ember {
namespace modem {
namespace robust {

const uint8_t kCostas[kSyncLen] = {2, 8, 9, 12, 4, 14, 10, 15, 13, 7, 6, 3, 11, 1, 5, 0};

namespace {

size_t gcd(size_t a, size_t b) {
  while (b) { size_t t = a % b; a = b; b = t; }
  return a;
}

// XOR with PN9 (see WHITEN in robust.h).
void whiten(uint8_t* bits, size_t n) {
  uint16_t s = 0x1FF;
  for (size_t i = 0; i < n; ++i) {
    uint8_t out = s & 1;
    uint16_t fb = (s ^ (s >> 5)) & 1;
    s = static_cast<uint16_t>((s >> 1) | (fb << 8));
    bits[i] ^= out;
  }
}

// Same sequence applied to LLRs (sign flip where the PN bit is 1).
void whiten_llr(float* llr, size_t n) {
  uint16_t s = 0x1FF;
  for (size_t i = 0; i < n; ++i) {
    uint8_t out = s & 1;
    uint16_t fb = (s ^ (s >> 5)) & 1;
    s = static_cast<uint16_t>((s >> 1) | (fb << 8));
    if (out) llr[i] = -llr[i];
  }
}

void interleave(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
  size_t n = in.size();
  out.resize(n);
  size_t a = interleave_step(n);
  for (size_t j = 0; j < n; ++j) out[j] = in[static_cast<size_t>((static_cast<uint64_t>(a) * j) % n)];
}

void deinterleave_llr(const float* in, size_t n, std::vector<float>& out) {
  out.assign(n, 0.0f);
  size_t a = interleave_step(n);
  for (size_t j = 0; j < n; ++j) out[static_cast<size_t>((static_cast<uint64_t>(a) * j) % n)] = in[j];
}

void bits_to_tones(const std::vector<uint8_t>& bits, std::vector<uint8_t>& tones) {
  for (size_t i = 0; i + 3 < bits.size(); i += 4) {
    unsigned v = (bits[i] << 3) | (bits[i + 1] << 2) | (bits[i + 2] << 1) | bits[i + 3];
    tones.push_back(static_cast<uint8_t>(v ^ (v >> 1)));
  }
}

void push_byte_bits(std::vector<uint8_t>& bits, uint8_t b) {
  for (int i = 7; i >= 0; --i) bits.push_back((b >> i) & 1);
}

size_t coded_len(size_t len) { return 2 * (8 * (len + 4) + 6); }

// log(I0(x)), Abramowitz & Stegun 9.8.1 / 9.8.2.
float log_i0(float x) {
  if (x < 3.75f) {
    float t = (x / 3.75f) * (x / 3.75f);
    float i0 = 1 + t * (3.5156229f + t * (3.0899424f + t * (1.2067492f + t * (0.2659732f + t * (0.0360768f + t * 0.0045813f)))));
    return std::log(i0);
  }
  float t = 3.75f / x;
  float p = 0.39894228f + t * (0.01328592f + t * (0.00225319f + t * (-0.00157565f + t * (0.00916281f +
            t * (-0.02057706f + t * (0.02635537f + t * (-0.01647633f + t * 0.00392377f)))))));
  return x - 0.5f * std::log(x) + std::log(p);
}

struct BitSets {
  // tone -> 4 bits (b0 first) of its Gray-decoded value
  uint8_t bit[kTones][4];
  BitSets() {
    for (int t = 0; t < kTones; ++t) {
      unsigned v = static_cast<unsigned>(t);  // Gray decode
      for (unsigned s = 1; s < 4; s <<= 1) v ^= v >> s;
      for (int i = 0; i < 4; ++i) bit[t][i] = (v >> (3 - i)) & 1;
    }
  }
};
const BitSets& bitsets() {
  static const BitSets b;
  return b;
}

}  // namespace

size_t interleave_step(size_t n) {
  if (n <= 2) return 1;
  size_t a = n * 3819 / 10000;
  if (a < 1) a = 1;
  while (gcd(a, n) != 1) ++a;
  return a;
}

size_t payload_symbols(size_t len) { return (coded_len(len) + 3) / 4; }

bool frame_tones(ByteView payload, std::vector<uint8_t>& tones) {
  size_t len = payload.size();
  if (len == 0 || len > kMaxPayload) return false;
  for (int i = 0; i < kSyncLen; ++i) tones.push_back(kCostas[i]);
  // Header
  uint8_t h[3];
  h[0] = static_cast<uint8_t>(len >> 4);
  h[1] = static_cast<uint8_t>((len & 0x0F) << 4);
  h[2] = crc8(ByteView(h, 2));
  std::vector<uint8_t> bits, coded, il;
  for (uint8_t b : h) push_byte_bits(bits, b);
  bits.insert(bits.end(), 6, 0);
  conv_encode(bits.data(), bits.size(), coded);
  interleave(coded, il);
  whiten(il.data(), il.size());
  bits_to_tones(il, tones);
  // Payload
  bits.clear();
  coded.clear();
  for (uint8_t b : payload) push_byte_bits(bits, b);
  uint32_t c = crc32(payload);
  for (int i = 3; i >= 0; --i) push_byte_bits(bits, static_cast<uint8_t>(c >> (8 * i)));
  bits.insert(bits.end(), 6, 0);
  conv_encode(bits.data(), bits.size(), coded);
  while (coded.size() % 4) coded.push_back(0);
  interleave(coded, il);
  whiten(il.data(), il.size());
  bits_to_tones(il, tones);
  return true;
}

void symbol_llrs(const float* e, float es, float s2, float* llr4) {
  const BitSets& bs = bitsets();
  if (!(s2 > 1e-30f)) s2 = 1e-30f;
  if (!(es > 0)) es = s2;
  float m[kTones];
  float mx = -1e30f;
  for (int k = 0; k < kTones; ++k) {
    float ek = e[k] > 0 ? e[k] : 0;
    m[k] = log_i0(2.0f * std::sqrt(es * ek) / s2);
    if (m[k] > mx) mx = m[k];
  }
  for (int i = 0; i < 4; ++i) {
    float a1 = 0, a0 = 0;
    for (int k = 0; k < kTones; ++k) {
      float v = std::exp(m[k] - mx);
      if (bs.bit[k][i]) a1 += v; else a0 += v;
    }
    float l = std::log((a1 + 1e-30f) / (a0 + 1e-30f));
    if (l > 30) l = 30;
    if (l < -30) l = -30;
    llr4[i] = l;
  }
}

bool decode_header(const float* llr60, uint16_t* len, size_t* bit_errs) {
  float w[60];
  std::memcpy(w, llr60, sizeof(w));
  whiten_llr(w, 60);
  std::vector<float> d;
  deinterleave_llr(w, 60, d);
  Viterbi v;
  std::vector<uint8_t> bits;
  size_t errs = v.decode(d.data(), 30, bits);
  if (bit_errs) *bit_errs = errs;
  uint8_t h[3] = {0, 0, 0};
  for (int i = 0; i < 24; ++i) h[i / 8] = static_cast<uint8_t>((h[i / 8] << 1) | bits[static_cast<size_t>(i)]);
  if (crc8(ByteView(h, 2)) != h[2]) return false;
  uint16_t l = static_cast<uint16_t>((h[0] << 4) | (h[1] >> 4));
  if (l == 0) return false;
  *len = l;
  return true;
}

bool decode_payload(const float* llr, size_t len, Bytes* out, size_t* bit_errs) {
  if (len == 0 || len > kMaxPayload || !out) return false;
  size_t n = 4 * payload_symbols(len);
  size_t m = coded_len(len);
  std::vector<float> w(llr, llr + n);
  whiten_llr(w.data(), n);
  std::vector<float> d;
  deinterleave_llr(w.data(), n, d);
  Viterbi v;
  std::vector<uint8_t> bits;
  size_t errs = v.decode(d.data(), m / 2, bits);
  if (bit_errs) *bit_errs = errs;
  size_t nbytes = len + 4;
  Bytes b(nbytes, 0);
  for (size_t i = 0; i < nbytes * 8; ++i) b[i / 8] = static_cast<uint8_t>((b[i / 8] << 1) | bits[i]);
  uint32_t c = crc32(ByteView(b.data(), len));
  uint32_t got = (static_cast<uint32_t>(b[len]) << 24) | (static_cast<uint32_t>(b[len + 1]) << 16) |
                 (static_cast<uint32_t>(b[len + 2]) << 8) | b[len + 3];
  if (c != got) return false;
  b.resize(len);
  *out = std::move(b);
  return true;
}

}  // namespace robust

// =================================================================================================

namespace {

using dsp::Cf;
using namespace robust;

constexpr int kMaxCands = 3;
constexpr int kGuard = 1;                // guard bins each side of the 16 tones in the symbol DFT
constexpr int kBins = kTones + 2 * kGuard;
constexpr size_t kMaxHeaderErrs = 14;    // of 60 coded bits; random data averages ~20

struct Cand {
  bool payload = false;
  uint64_t t0 = 0;       // absolute sample index of header symbol 0 (before tau)
  double tau = 0;        // timing correction, samples
  double foff = 0;       // Hz
  size_t sym = 0;        // symbols processed
  float s2 = 0, es = 0;  // noise / signal energy per bin estimates
  double snr_acc = 0;
  size_t snr_n = 0;
  float metric = 0;
  uint16_t len = 0;
  size_t need = kHeaderSymbols;
  size_t hdr_errs = 0;
  float tone_ema = 6;
  size_t weak_run = 0;
  std::vector<float> llr;
};

class RobustModem final : public Modem {
 public:
  RobustModem(uint32_t rate, const ModemOptions& opt, size_t n, double afc_hz, float thr)
      : Modem(rate, 8000, opt), N_(n), thr_(thr) {
    R_ = 8000.0 / static_cast<double>(N_);
    B_ = static_cast<int>(std::lround(1500.0 / R_)) - 7;
    G_ = static_cast<int>(std::ceil((afc_hz + R_ / 2) / R_ - 1e-9));  // + half a bin: edges are rejected
    if (G_ < 1) G_ = 1;
    hop_ = N_ / 8;
    e_ = N_ / 8;
  }

  bool setup(const std::string& name) {
    info_.name = name;
    info_.title = "Ember robust MFSK-16 " + std::to_string(static_cast<int>(2 * R_ + 0.5)) + " b/s (FEC)";
    info_.bitrate_bps = static_cast<uint32_t>(2 * R_);
    info_.max_frame = 1024;
    info_.frames = true;
    info_.text = false;
    info_.bw_lo_hz = static_cast<float>((B_ - 0.5) * R_);
    info_.bw_hi_hz = static_cast<float>((B_ + 15.5) * R_);
    if (opt_.max_frame > kMaxPayload) opt_.max_frame = static_cast<uint16_t>(kMaxPayload);
    if (opt_.max_frame > info_.max_frame) info_.max_frame = opt_.max_frame;
    if (!init_base()) return false;
    ring_size_ = 8 * N_;
    ring_.assign(ring_size_, 0.0f);
    if (!fft_.init(2 * N_)) return false;
    fbuf_.resize(2 * N_);
    row_w_ = static_cast<size_t>(2 * 15 + 4 * G_ + 1);
    rows_.assign(kRowRing * row_w_, 0.0f);
    row_end_.assign(kRowRing, 0);
    nh_ = static_cast<size_t>(4 * G_ + 1);
    S_.assign(nh_, 0.0f);
    prevS_.assign(nh_, 0.0f);
    tw_.resize(N_);
    for (size_t m = 0; m < N_; ++m) tw_[m] = dsp::expj(-dsp::kTwoPi * static_cast<double>(m) / static_cast<double>(N_));
    z_.resize(N_ + 2 * e_);
    return true;
  }

  bool detect() const override {
    for (const Cand& c : cands_) if (c.payload && c.weak_run < 16) return true;
    return in_peak_ || tone_ema_ > kToneDcd;
  }

 protected:
  void demod(const float* x, size_t n) override {
    for (size_t i = 0; i < n; ++i) {
      ring_[wr_ % ring_size_] = x[i];
      ++wr_;
      if (++hop_count_ >= hop_) {
        hop_count_ = 0;
        if (wr_ >= N_) search_row();
        // Service decoders every hop so the sample ring (8 symbols) never overruns.
        if (!cands_.empty()) run_candidates();
      }
    }
  }

  bool tx_begin() override {
    tones_.clear();
    size_t pre = static_cast<size_t>(std::ceil(opt_.txdelay_ms * R_ / 1000.0 - 1e-9));
    size_t post = static_cast<size_t>(std::ceil(opt_.txtail_ms * R_ / 1000.0 - 1e-9));
    if (post < 1) post = 1;
    for (size_t i = 0; i < pre; ++i) tones_.push_back(i % 2 ? 15 : 0);
    TxItem it;
    size_t frames = 0;
    while (frames < opt_.max_frames_per_tx && take_tx(&it)) {
      frame_tones(ByteView(it.data), tones_);
      ++frames;
    }
    if (!frames) return false;
    for (size_t i = 0; i < post; ++i) tones_.push_back(i % 2 ? 15 : 0);
    total_ = tones_.size() * N_;
    gi_ = 0;
    ramp_ = 40;
    return true;
  }

  size_t tx_generate(float* out, size_t max) override {
    size_t n = 0;
    while (n < max && gi_ < total_) {
      size_t sym = static_cast<size_t>(gi_ / N_), m = static_cast<size_t>(gi_ % N_);  // gi_ < total_ = size * N_
      uint32_t inc = static_cast<uint32_t>((static_cast<uint64_t>(B_ + tones_[sym]) << 32) / N_);
      float v = dsp::sin_u32(static_cast<uint32_t>(inc * m));
      uint64_t left = total_ - gi_;
      if (gi_ < ramp_) v *= 0.5f - 0.5f * std::cos(static_cast<float>(dsp::kPi) * gi_ / ramp_);
      else if (left < ramp_) v *= 0.5f - 0.5f * std::cos(static_cast<float>(dsp::kPi) * left / ramp_);
      out[n++] = v;
      ++gi_;
    }
    return n;
  }

  void tx_reset() override {
    tones_.clear();
    total_ = gi_ = 0;
  }

 private:
  static constexpr size_t kRowRing = 136;  // >= 15*8 + 1 rows of history
  static constexpr float kToneDcd = 8.0f;

  float sample(uint64_t i) const { return ring_[i % ring_size_]; }

  // ---- search ---------------------------------------------------------------------------------
  void search_row() {
    for (size_t m = 0; m < N_; ++m) fbuf_[m] = Cf(sample(wr_ - N_ + m), 0);
    for (size_t m = N_; m < 2 * N_; ++m) fbuf_[m] = Cf(0, 0);
    fft_.forward(fbuf_.data());
    size_t slot = static_cast<size_t>(rows_done_ % kRowRing);
    float* row = &rows_[slot * row_w_];
    long base = 2L * (B_ - G_);
    double sum = 0;
    float mx = 0;
    for (size_t p = 0; p < row_w_; ++p) {
      long idx = base + static_cast<long>(p);
      float v = (idx > 0 && idx < static_cast<long>(N_)) ? dsp::norm2(fbuf_[static_cast<size_t>(idx)]) : 0.0f;
      row[p] = v;
      sum += v;
      if (v > mx) mx = v;
    }
    float mean = static_cast<float>(sum / static_cast<double>(row_w_));
    if (mean > 1e-30f) {
      float inv = 1.0f / mean;
      for (size_t p = 0; p < row_w_; ++p) row[p] *= inv;
      tone_ema_ += 0.15f * (mx * inv - tone_ema_);
    } else {
      tone_ema_ += 0.15f * (1.0f - tone_ema_);
    }
    row_end_[slot] = wr_;
    ++rows_done_;
    if (rows_done_ < 15 * 8 + 1) return;

    // Sync metric for each half-bin offset h = -2G .. 2G.
    uint64_t r = rows_done_ - 1;
    float best = -1;
    size_t besth = 0;
    for (size_t hi = 0; hi < nh_; ++hi) {
      float acc = 0;
      for (int i = 0; i < kSyncLen; ++i) {
        uint64_t rr = r - static_cast<uint64_t>(15 - i) * 8;
        const float* rw = &rows_[static_cast<size_t>(rr % kRowRing) * row_w_];
        acc += rw[2 * kCostas[i] + hi];  // p = 2(k+G) + h, h + 2G = hi
      }
      S_[hi] = acc / kSyncLen;
      if (S_[hi] > best) { best = S_[hi]; besth = hi; }
    }
    // While a payload is being decoded, a new sync must be at least half as strong as that frame's
    // (data symbols produce weak sync-like sidelobes; a genuinely new transmission after a
    // truncated one is about as strong).
    float gate = thr_;
    for (const Cand& c : cands_) {
      if (c.payload) gate = std::max(gate, 0.5f * c.metric);
    }
    {
      if (!in_peak_ && best > gate) {
        in_peak_ = true;
        peak_best_ = -1;
        peak_end_ = rows_done_ + 4;
      }
      if (in_peak_) {
        if (best > peak_best_) {
          peak_end_ = rows_done_ + 4;  // close 4 hops after the last improvement
          peak_best_ = best;
          peak_h_ = besth;
          peak_row_ = r;
          peak_prev_t_ = prevS_[besth];
          peak_next_t_ = best;  // until the next row arrives
          peak_hm_ = besth > 0 ? S_[besth - 1] : best;
          peak_hp_ = besth + 1 < nh_ ? S_[besth + 1] : best;
        } else if (r == peak_row_ + 1) {
          peak_next_t_ = S_[peak_h_];
        }
        if (rows_done_ >= peak_end_) {
          in_peak_ = false;
          // A genuine sync peaks inside the search range; maxima pinned to its edge are data or
          // noise sidelobes.
          if (peak_h_ > 0 && peak_h_ + 1 < nh_) open_candidate();
        }
      }
    }
    prevS_.swap(S_);
  }

  static double parabolic(double a, double b, double c) {
    double den = a - 2 * b + c;
    if (std::fabs(den) < 1e-12) return 0;
    double d = 0.5 * (a - c) / den;
    return d > 0.5 ? 0.5 : (d < -0.5 ? -0.5 : d);
  }

  void open_candidate() {
    double dt = parabolic(peak_prev_t_, peak_best_, peak_next_t_) * static_cast<double>(hop_);
    double dh = parabolic(peak_hm_, peak_best_, peak_hp_);
    Cand c;
    uint64_t tend = row_end_[static_cast<size_t>(peak_row_ % kRowRing)];
    c.t0 = tend;
    c.tau = dt;
    c.foff = (static_cast<double>(peak_h_) - 2.0 * G_ + dh) * R_ / 2.0;
    c.metric = peak_best_;
    c.llr.reserve(4 * kHeaderSymbols);
    if (cands_.size() >= kMaxCands) {
      size_t worst = 0;
      for (size_t i = 1; i < cands_.size(); ++i) if (cands_[i].metric < cands_[worst].metric) worst = i;
      if (cands_[worst].metric >= c.metric) return;
      cands_.erase(cands_.begin() + static_cast<std::ptrdiff_t>(worst));
    }
    cands_.push_back(std::move(c));
  }

  // ---- per-candidate symbol demodulation ----------------------------------------------------
  void run_candidates() {
    for (size_t i = 0; i < cands_.size();) {
      int st = advance(cands_[i]);
      if (st < 0) {
        cands_.erase(cands_.begin() + static_cast<std::ptrdiff_t>(i));
        continue;
      }
      if (st == 1) {
        // Header accepted: drop competitors with a much weaker sync (sidelobes / noise). Comparable
        // or stronger ones keep decoding in parallel, so a false header that slipped through the
        // CRC-8 cannot deafen the receiver to a real frame.
        Cand keep = std::move(cands_[i]);
        std::vector<Cand> rest;
        for (size_t j = 0; j < cands_.size(); ++j) {
          if (j != i && cands_[j].metric >= 0.5f * keep.metric) rest.push_back(std::move(cands_[j]));
        }
        cands_.clear();
        cands_.push_back(std::move(keep));
        for (Cand& c : rest) cands_.push_back(std::move(c));
        i = 0;
        continue;
      }
      ++i;
    }
  }

  // Returns -1 to drop, 1 when the header was just accepted, 0 otherwise.
  int advance(Cand& c) {
    while (true) {
      double start = static_cast<double>(c.t0) + static_cast<double>(c.sym * N_) + c.tau;
      if (start < static_cast<double>(e_)) return -1;
      uint64_t s = static_cast<uint64_t>(std::llround(start));
      if (s + N_ + e_ > wr_) return 0;                   // wait for more samples
      if (wr_ - (s - e_) > ring_size_) return -1;        // fell behind (should not happen)
      float E[kBins], el = 0, ee = 0;
      int kmax = demod_symbol(c, s, E, &ee, &el);
      const float* et = E + kGuard;
      // Noise / signal statistics.
      float sum = 0;
      for (int k = 0; k < kTones; ++k) sum += et[k];
      float emax = et[kmax];
      float noise = (sum - emax) / (kTones - 1);
      if (c.sym == 0) {
        c.s2 = noise > 1e-30f ? noise : 1e-30f;
        c.es = emax - noise > c.s2 ? emax - noise : c.s2;
      } else {
        float a = c.sym < 8 ? 0.3f : 0.1f;
        c.s2 += a * (noise - c.s2);
        float sig = emax - c.s2;
        if (sig < 0.1f * c.s2) sig = 0.1f * c.s2;
        c.es += a * (sig - c.es);
      }
      if (c.s2 < 1e-30f) c.s2 = 1e-30f;
      float ratio = emax / (sum / kTones + 1e-30f);
      c.tone_ema += 0.1f * (ratio - c.tone_ema);
      c.weak_run = c.tone_ema < 3.6f ? c.weak_run + 1 : 0;
      c.snr_acc += c.es / c.s2;
      ++c.snr_n;
      // Tracking, only on confident symbols.
      bool conf = emax > 4.0f * c.s2;
      if (conf && ee + el > 0) {
        double err = (el - ee) / (el + ee);
        double gain = c.payload ? 0.2 : 0.4;
        c.tau += gain * err * static_cast<double>(N_ - e_) / 2.0;
        float lo = E[kGuard + kmax - 1], hi = E[kGuard + kmax + 1];
        float a0 = std::sqrt(emax), ah = std::sqrt(hi), al = std::sqrt(lo);
        double d = hi > lo ? ah / (a0 + ah) : -al / (a0 + al);
        c.foff += (c.payload ? 0.05 : 0.1) * d * R_;
      }
      float l4[4];
      symbol_llrs(et, c.es, c.s2, l4);
      c.llr.insert(c.llr.end(), l4, l4 + 4);
      ++c.sym;
      if (!c.payload && c.llr.size() == 4 * kHeaderSymbols) {
        uint16_t len = 0;
        size_t errs = 0;
        bool hok = decode_header(c.llr.data(), &len, &errs);
        if (!hok || errs > kMaxHeaderErrs || len > opt_.max_frame || len < opt_.min_frame) return -1;
        c.payload = true;
        c.len = len;
        c.hdr_errs = errs;
        c.need = payload_symbols(len);
        c.llr.clear();
        c.llr.reserve(4 * c.need);
        return 1;
      }
      if (c.payload) {
        if (c.weak_run > 64) { ++stats_.rx_bad; return -1; }  // signal gone
        if (c.llr.size() == 4 * c.need) {
          Bytes out;
          size_t errs = 0;
          if (decode_payload(c.llr.data(), c.len, &out, &errs)) {
            RxQuality q;
            double r = c.snr_acc / static_cast<double>(c.snr_n ? c.snr_n : 1);
            q.has_snr = true;
            q.snr_db = static_cast<float>(10 * std::log10(r * R_ / 3000.0 + 1e-12));
            q.freq_offset_hz = static_cast<float>(c.foff);
            size_t tot = errs + c.hdr_errs;
            q.corrected = static_cast<uint16_t>(tot > 65535 ? 65535 : tot);
            emit_frame(ByteView(out), q);
          } else {
            ++stats_.rx_bad;
          }
          return -1;
        }
      }
    }
  }

  // Tone energies (with guard bins) of the symbol starting at absolute sample s; returns the
  // decided tone and the early/late energies of that tone.
  int demod_symbol(const Cand& c, uint64_t s, float* E, float* ee, float* el) {
    // Mix by -foff over [s - e, s + N + e).
    size_t len = N_ + 2 * e_;
    double w = -dsp::kTwoPi * c.foff / 8000.0;
    Cf rot = dsp::expj(w), ph(1, 0);
    for (size_t m = 0; m < len; ++m) {
      float x = sample(s - e_ + m);
      z_[m] = ph * x;
      ph = ph * rot;
      if ((m & 63) == 63) {  // renormalise
        float a = dsp::cabs(ph);
        if (a > 0) ph = ph * (1.0f / a);
      }
    }
    const Cf* z0 = &z_[e_];
    int kmax = 0;
    float emax = -1;
    for (int b = 0; b < kBins; ++b) {
      long bin = B_ - kGuard + b;
      float e = dft_energy(z0, bin);
      E[b] = e;
      int k = b - kGuard;
      if (k >= 0 && k < kTones && e > emax) { emax = e; kmax = k; }
    }
    *ee = dft_energy(&z_[0], B_ + kmax);
    *el = dft_energy(&z_[2 * e_], B_ + kmax);
    return kmax;
  }

  float dft_energy(const Cf* z, long bin) const {
    if (bin < 0) bin = 0;
    size_t step = static_cast<size_t>(bin) % N_, idx = 0;
    float re = 0, im = 0;
    for (size_t m = 0; m < N_; ++m) {
      const Cf& t = tw_[idx];
      re += z[m].re * t.re - z[m].im * t.im;
      im += z[m].re * t.im + z[m].im * t.re;
      idx += step;
      if (idx >= N_) idx -= N_;
    }
    return re * re + im * im;
  }

  size_t N_;
  float thr_;
  double R_ = 31.25;
  int B_ = 41, G_ = 2;
  size_t hop_ = 32, e_ = 32;
  // search
  std::vector<float> ring_;
  size_t ring_size_ = 0;
  uint64_t wr_ = 0;
  size_t hop_count_ = 0;
  dsp::Fft fft_;
  std::vector<Cf> fbuf_;
  size_t row_w_ = 0, nh_ = 0;
  std::vector<float> rows_;
  std::vector<uint64_t> row_end_;
  uint64_t rows_done_ = 0;
  std::vector<float> S_, prevS_;
  bool in_peak_ = false;
  float peak_best_ = 0, peak_prev_t_ = 0, peak_next_t_ = 0, peak_hm_ = 0, peak_hp_ = 0;
  size_t peak_h_ = 0;
  uint64_t peak_row_ = 0, peak_end_ = 0;
  float tone_ema_ = 1;
  // decode
  std::vector<Cand> cands_;
  std::vector<Cf> tw_, z_;
  // tx
  std::vector<uint8_t> tones_;
  uint64_t total_ = 0, gi_ = 0, ramp_ = 40;
};

}  // namespace

std::unique_ptr<Modem> create_robust(const std::string& name, uint32_t sample_rate, const ModemOptions& opt,
                                     std::string* error) {
  static const char* const kKeys[] = {"afc_hz", "sync_threshold", nullptr};
  if (!check_keys(opt.params, kKeys, error)) return nullptr;
  size_t n;
  if (name == "robust250") n = 64;
  else if (name == "robust125") n = 128;
  else if (name == "robust62") n = 256;
  else if (name == "robust31") n = 512;
  else {
    if (error) *error = "unknown robust speed";
    return nullptr;
  }
  double afc, thr;
  if (!param_double(opt.params, "afc_hz", 80, 10, 300, &afc, error)) return nullptr;
  if (!param_double(opt.params, "sync_threshold", 3.0, 2.0, 10.0, &thr, error)) return nullptr;
  std::unique_ptr<RobustModem> m(new RobustModem(sample_rate, opt, n, afc, static_cast<float>(thr)));
  if (!m->setup(name)) {
    if (error) *error = "bad sample rate";
    return nullptr;
  }
  return m;
}

}  // namespace modem
}  // namespace ember
