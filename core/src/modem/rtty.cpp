#include "modem/rtty.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "dsp/agc.h"
#include "dsp/nco.h"

namespace ember {
namespace modem {

namespace ita2 {
namespace {
const char kLetters[32] = {0, 'E', '\n', 'A', ' ', 'S', 'I', 'U', '\r', 'D', 'R', 'J', 'N', 'F', 'C', 'K',
                           'T', 'Z', 'L', 'W', 'H', 'Y', 'P', 'Q', 'O', 'B', 'G', 0, 'M', 'X', 'V', 0};
const char kFigures[32] = {0, '3', '\n', '-', ' ', '\a', '8', '7', '\r', '$', '4', '\'', ',', '!', ':', '(',
                           '5', '"', ')', '2', '#', '6', '0', '1', '9', '?', '&', 0, '.', '/', ';', 0};
}  // namespace

int encode(char c, bool* figs, bool* either) {
  if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  *either = (c == ' ' || c == '\n' || c == '\r');
  for (int i = 0; i < 32; ++i) {
    if (kLetters[i] && kLetters[i] == c) { *figs = false; return i; }
  }
  for (int i = 0; i < 32; ++i) {
    if (kFigures[i] && kFigures[i] == c) { *figs = true; return i; }
  }
  return -1;
}

char decode(int code, bool figs) {
  if (code < 0 || code > 31) return 0;
  return figs ? kFigures[code] : kLetters[code];
}
}  // namespace ita2

namespace {

using dsp::Cf;
constexpr double kFs = 8000;

// O(1) complex boxcar.
class Boxcar {
 public:
  void init(size_t n) { buf_.assign(n ? n : 1, Cf()); pos_ = 0; re_ = im_ = 0; }
  Cf push(Cf x) {
    Cf& old = buf_[pos_];
    re_ += static_cast<double>(x.re) - old.re;
    im_ += static_cast<double>(x.im) - old.im;
    old = x;
    if (++pos_ == buf_.size()) pos_ = 0;
    return Cf(static_cast<float>(re_), static_cast<float>(im_));
  }

 private:
  std::vector<Cf> buf_;
  size_t pos_ = 0;
  double re_ = 0, im_ = 0;
};

class RttyModem final : public Modem {
 public:
  RttyModem(uint32_t rate, const ModemOptions& opt) : Modem(rate, 8000, opt) {}

  bool setup(double mark, double shift, double baud, double stop, bool reverse, bool usos, float sq) {
    mark_ = reverse ? mark + shift : mark;
    space_ = reverse ? mark : mark + shift;
    baud_ = baud;
    stop_ = stop;
    usos_ = usos;
    squelch_ = sq;
    info_.name = "rtty";
    info_.title = "RTTY (ITA2 Baudot)";
    info_.bitrate_bps = static_cast<uint32_t>(baud + 0.5);
    info_.frames = false;
    info_.text = true;
    info_.bw_lo_hz = static_cast<float>(std::min(mark_, space_) - baud);
    info_.bw_hi_hz = static_cast<float>(std::max(mark_, space_) + baud);
    if (!init_base()) return false;
    spb_ = kFs / baud_;
    size_t L = static_cast<size_t>(std::lround(spb_));
    bm_.init(L);
    bs_.init(L);
    nm_.set_freq(mark_, kFs);
    ns_.set_freq(space_, kFs);
    float dec = dsp::ema_coeff(16.0 / baud_, kFs);
    pm_.init(0.05f, dec);
    ps_.init(0.05f, dec);
    inc_m_ = dsp::Nco::cycles_to_u32(mark_ / kFs);
    inc_s_ = dsp::Nco::cycles_to_u32(space_ / kFs);
    return true;
  }

  bool detect() const override { return quality_ > squelch_; }

 protected:
  bool accept_text(std::string_view t) const override {
    for (char c : t) {
      bool f, e;
      if (ita2::encode(c, &f, &e) >= 0) return true;
    }
    return false;
  }

  void demod(const float* x, size_t n) override {
    for (size_t i = 0; i < n; ++i) {
      float m = dsp::cabs(bm_.push(nm_.next_conj() * x[i]));
      float s = dsp::cabs(bs_.push(ns_.next_conj() * x[i]));
      pm_.push(m);
      ps_.push(s);
      float d = pm_.normalise(m) - ps_.normalise(s);
      raw_space_ = s > m;
      contrast_ = (m + s) > 1e-20f ? std::fabs(m - s) / (m + s) : 0;
      step(d);
    }
  }

  bool tx_begin() override {
    TxItem it;
    if (!take_tx(&it)) return false;
    el_.clear();
    double pre = std::max<double>(opt_.txdelay_ms, 300) / 1000 * baud_;
    double post = std::max<double>(opt_.txtail_ms, 100) / 1000 * baud_;
    el_.push_back({1, pre});
    bool figs = false;
    bool known = false;
    auto put = [&](int code) {
      el_.push_back({0, 1.0});
      for (int b = 0; b < 5; ++b) el_.push_back({static_cast<uint8_t>((code >> b) & 1), 1.0});
      el_.push_back({1, stop_});
    };
    put(ita2::kLtrs);
    known = true;
    for (uint8_t raw : it.data) {
      char c = static_cast<char>(raw);
      if (c == '\n') {
        put(8);  // CR
        put(2);  // LF
        continue;
      }
      bool f, either;
      int code = ita2::encode(c, &f, &either);
      if (code < 0) continue;
      if (!either && (!known || f != figs)) {
        put(f ? ita2::kFigs : ita2::kLtrs);
        figs = f;
        known = true;
      }
      put(code);
      if (code == 4 && usos_) {  // space: receivers unshift to letters
        figs = false;
      }
    }
    el_.push_back({1, post});
    ei_ = 0;
    epos_ = 0;
    gi_ = 0;
    ramp_ = 40;
    total_ = 0;
    for (const El& e : el_) total_ += e.len * spb_;
    return true;
  }

  size_t tx_generate(float* out, size_t max) override {
    size_t n = 0;
    while (n < max && ei_ < el_.size()) {
      const El& e = el_[ei_];
      phase_ += e.mark ? inc_m_ : inc_s_;
      float v = dsp::sin_u32(phase_);
      double left = total_ - static_cast<double>(gi_);
      if (gi_ < ramp_) v *= 0.5f - 0.5f * std::cos(static_cast<float>(dsp::kPi) * gi_ / ramp_);
      else if (left < ramp_) v *= 0.5f - 0.5f * std::cos(static_cast<float>(dsp::kPi) * static_cast<float>(left) / ramp_);
      out[n++] = v;
      ++gi_;
      epos_ += 1.0;
      if (epos_ >= e.len * spb_) {
        epos_ -= e.len * spb_;
        ++ei_;
      }
    }
    return n;
  }

 private:
  struct El {
    uint8_t mark;
    double len;  // in bit times
  };

  // Async character decoding on the normalised discriminator output d (> 0 = mark).
  void step(float d) {
    t_ += 1.0;
    mark_run_ = d > 0 ? mark_run_ + 1 : 0;
    if (!in_char_) {
      // A start bit must follow >= 0.9 bit of mark (the stop/idle), unless it directly chains onto
      // a correctly framed character. This avoids locking onto data bits (classic RYRY misframe).
      if (prev_ > 0 && d <= 0 && (chained_ || prev_mark_run_ >= 0.9 * spb_)) {
        float den = prev_ - d;
        double frac = den > 0 ? prev_ / den : 0.5;
        tzc_ = t_ - 1.0 + frac;
        in_char_ = true;
        bit_ = 0;
        code_ = 0;
        char_contrast_ = 0;
      }
    } else {
      double target = tzc_ + (bit_ + 0.5) * spb_;
      if (t_ >= target) {
        double back = t_ - target;
        float v = d - (d - prev_) * static_cast<float>(back > 1 ? 1 : back);
        // Squelch statistic: mark/space contrast at bit centres (~0.9 for a signal, ~0.3 noise),
        // per character and as an EMA for DCD.
        quality_ += 0.12f * (contrast_ - quality_);
        char_contrast_ += contrast_;
        if (bit_ == 0) {
          // False start: no space at the centre (raw envelopes too, so ATC transients when a
          // signal appears cannot fake one).
          if (v > 0 || !raw_space_ || contrast_ < 0.75f * squelch_) { in_char_ = false; chained_ = false; }
        } else if (bit_ <= 5) {
          if (v > 0) code_ |= 1 << (bit_ - 1);
        } else {
          in_char_ = false;
          chained_ = v > 0;
          if (v > 0) deliver(code_);
          // On a framing error, resynchronise on the next mark->space edge after idle mark.
        }
        ++bit_;
      }
    }
    prev_mark_run_ = mark_run_;
    prev_ = d;
  }

  void deliver(int code) {
    if (code == ita2::kLtrs) { figs_ = false; return; }
    if (code == ita2::kFigs) { figs_ = true; return; }
    char c = ita2::decode(code, figs_);
    if (code == 4 && usos_) figs_ = false;
    if (!c || c == '\r' || c == '\a') return;
    if (char_contrast_ / 7 < squelch_) return;
    RxQuality q;
    q.has_snr = true;
    float qq = quality_ < 0.999f ? quality_ : 0.999f;
    q.snr_db = 10 * std::log10(qq * qq / (1 - qq * qq));
    emit_text(std::string(1, c), q);
  }

  double mark_ = 2125, space_ = 2295, baud_ = 45.45, stop_ = 1.5, spb_ = 176;
  bool usos_ = true;
  float squelch_ = 0.4f;
  dsp::Nco nm_, ns_;
  Boxcar bm_, bs_;
  dsp::PeakValley pm_, ps_;
  float quality_ = 0, prev_ = 0, contrast_ = 0;
  bool raw_space_ = false;
  float char_contrast_ = 0;
  double t_ = 0, tzc_ = 0;
  bool in_char_ = false, figs_ = false, chained_ = false;
  double mark_run_ = 0, prev_mark_run_ = 0;
  int bit_ = 0, code_ = 0;
  // tx
  std::vector<El> el_;
  size_t ei_ = 0;
  double epos_ = 0, total_ = 0;
  uint64_t gi_ = 0, ramp_ = 40;
  uint32_t phase_ = 0, inc_m_ = 0, inc_s_ = 0;
};

}  // namespace

std::unique_ptr<Modem> create_rtty(const std::string&, uint32_t sample_rate, const ModemOptions& opt,
                                   std::string* error) {
  static const char* const kKeys[] = {"mark_hz", "shift_hz", "baud", "stop_bits", "reverse", "usos", "squelch", nullptr};
  if (!check_keys(opt.params, kKeys, error)) return nullptr;
  double mark, shift, baud, stop, sq;
  bool rev, usos;
  if (!param_double(opt.params, "mark_hz", 2125, 300, 3000, &mark, error)) return nullptr;
  if (!param_double(opt.params, "shift_hz", 170, 50, 1000, &shift, error)) return nullptr;
  if (!param_double(opt.params, "baud", 45.45, 20, 300, &baud, error)) return nullptr;
  if (!param_double(opt.params, "stop_bits", 1.5, 1, 2, &stop, error)) return nullptr;
  if (!param_double(opt.params, "squelch", 0.4, 0, 1, &sq, error)) return nullptr;
  if (!param_bool(opt.params, "reverse", false, &rev, error)) return nullptr;
  if (!param_bool(opt.params, "usos", true, &usos, error)) return nullptr;
  if (mark + shift > 3800) {
    if (error) *error = "tones above 3800 Hz";
    return nullptr;
  }
  std::unique_ptr<RttyModem> m(new RttyModem(sample_rate, opt));
  if (!m->setup(mark, shift, baud, stop, rev, usos, static_cast<float>(sq))) {
    if (error) *error = "bad sample rate";
    return nullptr;
  }
  return m;
}

}  // namespace modem
}  // namespace ember
