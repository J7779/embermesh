#include "modem/psk31.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "dsp/fir.h"
#include "dsp/nco.h"

namespace ember {
namespace modem {

namespace psk31 {
namespace {
const char* const kVaricode[128] = {
    "1010101011", "1011011011", "1011101101", "1101110111", "1011101011", "1101011111", "1011101111", "1011111101",
    "1011111111", "11101111",   "11101",      "1101101111", "1011011101", "11111",      "1101110101", "1110101011",
    "1011110111", "1011110101", "1110101101", "1110101111", "1101011011", "1101101011", "1101101101", "1101010111",
    "1101111011", "1101111101", "1110110111", "1101010101", "1101011101", "1110111011", "1011111011", "1101111111",
    "1",          "111111111",  "101011111",  "111110101",  "111011011",  "1011010101", "1010111011", "101111111",
    "11111011",   "11110111",   "101101111",  "111011111",  "1110101",    "110101",     "1010111",    "110101111",
    "10110111",   "10111101",   "11101101",   "11111111",   "101110111",  "101011011",  "101101011",  "110101101",
    "110101011",  "110110111",  "11110101",   "110111101",  "111101101",  "1010101",    "111010111",  "1010101111",
    "1010111101", "1111101",    "11101011",   "10101101",   "10110101",   "1110111",    "11011011",   "11111101",
    "101010101",  "1111111",    "111111101",  "101111101",  "11010111",   "10111011",   "11011101",   "10101011",
    "11010101",   "111011101",  "10101111",   "1101111",    "1101101",    "101010111",  "110110101",  "101011101",
    "101110101",  "101111011",  "1010101101", "111110111",  "111101111",  "111111011",  "1010111111", "101101101",
    "1011011111", "1011",       "1011111",    "101111",     "101101",     "11",         "111101",     "1011011",
    "101011",     "1101",       "111101011",  "10111111",   "11011",      "111011",     "1111",       "111",
    "111111",     "110111111",  "10101",      "10111",      "101",        "110111",     "1111011",    "1101011",
    "11011111",   "1011101",    "111010101",  "1010110111", "110111011",  "1010110101", "1011010111", "1110110101",
};
}  // namespace

const char* varicode(unsigned char c) { return c < 128 ? kVaricode[c] : nullptr; }

int varicode_decode(const char* bits, size_t n) {
  if (!bits || n == 0 || n > 10) return -1;
  for (int c = 0; c < 128; ++c) {
    if (std::strlen(kVaricode[c]) == n && std::memcmp(kVaricode[c], bits, n) == 0) return c;
  }
  return -1;
}
}  // namespace psk31

namespace {

using dsp::Cf;
constexpr double kFs = 8000;
constexpr size_t kSps = 256;   // samples per symbol at 8 kHz
constexpr size_t kDec = 16;    // -> 500 Hz, 16 samples/symbol
constexpr size_t kPh = kSps / kDec;

class Psk31Modem final : public Modem {
 public:
  Psk31Modem(uint32_t rate, const ModemOptions& opt, double fc, float sq, bool afc)
      : Modem(rate, 8000, opt), fc_(fc), fc0_(fc), squelch_(sq), afc_(afc) {}

  bool setup() {
    info_.name = "psk31";
    info_.title = "PSK31 (BPSK varicode)";
    info_.bitrate_bps = 31;
    info_.frames = false;
    info_.text = true;
    info_.bw_lo_hz = static_cast<float>(fc_ - 40);
    info_.bw_hi_hz = static_cast<float>(fc_ + 40);
    if (!init_base()) return false;
    dec_.init(dsp::fir_lowpass(dsp::kaiser_taps(50, 140, kFs, 511), 70, kFs, dsp::Window::Kaiser, dsp::kaiser_beta(50)), kDec);
    std::vector<float> mf(kPh * 2);
    float s = 0;
    for (size_t i = 0; i < mf.size(); ++i) {
      mf[i] = static_cast<float>(dsp::window_value(dsp::Window::Hann, i, mf.size()));
      s += mf[i];
    }
    for (float& v : mf) v /= s;
    mf_.set_taps(mf);
    nco_.set_freq(fc_, kFs);
    return true;
  }

  bool detect() const override { return quality_ > squelch_; }

 protected:
  bool accept_text(std::string_view t) const override {
    for (char c : t) if (static_cast<unsigned char>(c) < 128) return true;
    return false;
  }

  void demod(const float* x, size_t n) override {
    for (size_t i = 0; i < n; ++i) {
      Cf z;
      if (!dec_.push(nco_.next_conj() * x[i], &z)) continue;
      // Squaring-loop frequency detector at 500 Hz, ahead of the narrow matched filter so noise
      // alone stays incoherent: z^2 removes the BPSK modulation and rotates by 2 * 2*pi*df/500
      // per sample (unambiguous to +-125 Hz).
      Cf w = z * z;
      Cf r = w * dsp::conj(w_prev_);
      w_prev_ = w;
      fd_acc_ = fd_acc_ * 0.98f + r;
      fd_mag_ = 0.98f * fd_mag_ + dsp::cabs(r);
      z = mf_.process(z);
      size_t ph = cnt_ % kPh;
      ++cnt_;
      float e = dsp::norm2(z);
      energy_[ph] = 0.97f * energy_[ph] + 0.03f * e;
      sym_e_ += e;
      if (++since_ >= period_) {
        since_ = 0;
        sample_ph_ = ph;
        symbol(z);
      }
    }
  }

  bool tx_begin() override {
    TxItem it;
    if (!take_tx(&it)) return false;
    bits_.clear();
    size_t pre = static_cast<size_t>(std::ceil(opt_.txdelay_ms * 31.25 / 1000));
    size_t post = static_cast<size_t>(std::ceil(opt_.txtail_ms * 31.25 / 1000));
    if (pre < 32) pre = 32;
    if (post < 16) post = 16;
    bits_.assign(pre, 0);
    for (uint8_t c : it.data) {
      const char* v = psk31::varicode(c);
      if (!v) continue;
      for (const char* p = v; *p; ++p) bits_.push_back(*p == '1');
      bits_.push_back(0);
      bits_.push_back(0);
    }
    bits_.insert(bits_.end(), post, 0);
    gi_ = 0;
    sign_ = 1;
    tnco_.set_freq(fc0_, kFs);
    tnco_.reset();
    return true;
  }

  size_t tx_generate(float* out, size_t max) override {
    size_t n = 0;
    size_t total = (bits_.size() + 2) * kSps;  // + ramp-in and ramp-out symbols
    while (n < max && gi_ < total) {
      size_t sym = gi_ / kSps, t = gi_ % kSps;
      float ft = static_cast<float>(t) / kSps;
      float a;
      if (sym == 0) {
        a = 0.5f - 0.5f * std::cos(static_cast<float>(dsp::kPi) * ft);
      } else if (sym == bits_.size() + 1) {
        a = sign_ * (0.5f + 0.5f * std::cos(static_cast<float>(dsp::kPi) * ft));
      } else {
        bool rev = bits_[sym - 1] == 0;
        a = rev ? sign_ * std::cos(static_cast<float>(dsp::kPi) * ft) : sign_;
        if (t == kSps - 1 && rev) sign_ = -sign_;
      }
      if (sym == 0 && t == 0) sign_ = 1;
      out[n++] = a * tnco_.next_sin();
      ++gi_;
    }
    return n;
  }

 private:
  void symbol(Cf z) {
    Cf d = z * dsp::conj(prev_);
    prev_ = z;
    // Signal-gone detector: a symbol far weaker than the recent signal closes the squelch at once
    // (otherwise the slow quality average would decode the tail of a transmission as text).
    float pz = sym_e_ / static_cast<float>(period_);  // mean |z|^2 over the symbol
    sym_e_ = 0;
    // While the squelch is closed the reference follows the current level too; otherwise a stale
    // reference from an earlier, stronger signal (or junk such as a steady carrier) keeps pushing
    // every weaker transmission's reversal symbols under 5 % and the squelch never reopens.
    amp_ += (quality_ > squelch_ ? 0.05f : 0.1f) * (pz - amp_);
    if (pz < 0.05f * amp_) quality_ *= 0.3f;
    float m = dsp::norm2(d);
    if (m > 1e-30f) {
      Cf d2 = d * d;
      float q = d2.re / m;
      quality_ += 0.06f * (q - quality_);
      // AFC once per symbol when the squaring loop sees a coherent carrier.
      float coh = fd_mag_ > 1e-30f ? dsp::cabs(fd_acc_) / fd_mag_ : 0.0f;
      if (afc_ && coh > 0.7f) {
        double df = dsp::carg(fd_acc_) / (2 * dsp::kTwoPi) * 500.0;
        fc_ += 0.15 * df;
        if (fc_ < fc0_ - 100) fc_ = fc0_ - 100;
        if (fc_ > fc0_ + 100) fc_ = fc0_ + 100;
        nco_.set_freq(fc_, kFs);
      }
    }
    // Timing: move the sampling phase one step toward the energy maximum.
    size_t best = 0;
    for (size_t p = 1; p < kPh; ++p) if (energy_[p] > energy_[best]) best = p;
    long diff = static_cast<long>(best) - static_cast<long>(sample_ph_);
    if (diff > static_cast<long>(kPh / 2)) diff -= kPh;
    if (diff < -static_cast<long>(kPh / 2)) diff += kPh;
    // Next symbol one sample later/earlier (never sample twice or skip a symbol).
    period_ = kPh + (diff > 0 ? 1 : (diff < 0 ? -1 : 0));

    int bit = d.re > 0 ? 1 : 0;
    auto append = [&](char c) {
      if (nbits_ < sizeof(code_)) code_[nbits_] = c;
      if (nbits_ < 255) ++nbits_;
    };
    if (bit) {
      if (zeros_ == 1 && nbits_ > 0) append('0');  // a single 0 inside a code
      append('1');
      zeros_ = 0;
    } else if (++zeros_ == 2 && nbits_ > 0) {        // "00": end of character
      int c = nbits_ <= 10 ? psk31::varicode_decode(code_, nbits_) : -1;
      if (c >= 0 && quality_ > squelch_ && c != '\r' && c != 0) {
        RxQuality q;
        q.has_snr = true;
        float qq = quality_ < 0.999f ? quality_ : 0.999f;
        q.snr_db = 10 * std::log10((1 + qq) / (1 - qq));
        q.freq_offset_hz = static_cast<float>(fc_ - fc0_);
        emit_text(std::string(1, static_cast<char>(c)), q);
      }
      nbits_ = 0;
    }
  }

  double fc_, fc0_;
  float squelch_;
  bool afc_;
  dsp::Nco nco_;
  dsp::FirDecimC dec_;
  dsp::FirC mf_;
  float energy_[kPh] = {};
  size_t cnt_ = 0, sample_ph_ = 0, since_ = 0, period_ = kPh;
  Cf prev_{1, 0};
  float quality_ = 0, amp_ = 0, fd_mag_ = 0, sym_e_ = 0;
  Cf w_prev_{0, 0}, fd_acc_{0, 0};
  char code_[16] = {};
  size_t nbits_ = 0, zeros_ = 0;
  // tx
  std::vector<uint8_t> bits_;
  size_t gi_ = 0;
  float sign_ = 1;
  dsp::Nco tnco_;
};

}  // namespace

std::unique_ptr<Modem> create_psk31(const std::string&, uint32_t sample_rate, const ModemOptions& opt,
                                    std::string* error) {
  static const char* const kKeys[] = {"center_hz", "squelch", "afc", nullptr};
  if (!check_keys(opt.params, kKeys, error)) return nullptr;
  double fc, sq;
  bool afc;
  if (!param_double(opt.params, "center_hz", 1000, 300, 3000, &fc, error)) return nullptr;
  if (!param_double(opt.params, "squelch", 0.35, 0, 1, &sq, error)) return nullptr;
  if (!param_bool(opt.params, "afc", true, &afc, error)) return nullptr;
  std::unique_ptr<Psk31Modem> m(new Psk31Modem(sample_rate, opt, fc, static_cast<float>(sq), afc));
  if (!m->setup()) {
    if (error) *error = "bad sample rate";
    return nullptr;
  }
  return m;
}

}  // namespace modem
}  // namespace ember
