#include "modem/cw.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "dsp/fft.h"
#include "dsp/fir.h"
#include "dsp/nco.h"
#include "modem/cw_rx.h"
#include "modem/morse.h"

namespace ember {
namespace modem {

namespace {

using dsp::Cf;

constexpr double kFs = 8000;
constexpr double kIdleResetS = 1.0;      // min key-up time (and >= 15 dots) after which timing is forgotten

class CwModem final : public Modem {
 public:
  CwModem(uint32_t rate, const ModemOptions& opt) : Modem(rate, 8000, opt) {}

  bool setup(double wpm, double fwpm, double tone, double rise_ms, double bw, bool afc) {
    wpm_ = wpm;
    fwpm_ = (fwpm > 0 && fwpm < wpm) ? fwpm : wpm;
    tone_ = tone;
    rise_ = static_cast<size_t>(rise_ms * kFs / 1000);
    info_.name = "cw";
    info_.title = "CW / Morse";
    info_.bitrate_bps = static_cast<uint32_t>(wpm * 50 / 60 + 0.5);  // "PARIS" = 50 units
    info_.frames = false;
    info_.text = true;
    info_.bw_lo_hz = static_cast<float>(tone - 100);
    info_.bw_hi_hz = static_cast<float>(tone + 100);
    if (!init_base()) return false;
    CwReceiver::Config rc;
    rc.tone_hz = tone;
    rc.bw_hz = bw;
    rc.afc = afc;
    rc.timing.wpm_hint = wpm;
    rc.timing.min_wpm = 5;
    rc.timing.max_wpm = 60;
    rc.timing.min_mark_ms = 10;
    rc.timing.idle_forget_ms = kIdleResetS * 1000;
    rc.timing.idle_forget_dots = 15;
    return rx_.init(rc);
  }

  bool detect() const override { return rx_.busy(); }

 protected:
  bool accept_text(std::string_view t) const override {
    std::string tok;
    return morse::encode(t, &tok);
  }

  void demod(const float* x, size_t n) override {
    rx_.process(x, n);
    if (!rx_.timing().has_text()) return;
    std::string s;
    rx_.timing().take_text(&s);
    RxQuality q;
    q.has_snr = true;
    q.snr_db = static_cast<float>(rx_.snr_db());
    q.freq_offset_hz = static_cast<float>(rx_.tone_hz() - base_tone_);
    emit_text(s, q);
  }

  bool tx_begin() override {
    TxItem it;
    if (!take_tx(&it)) return false;
    std::string tok;
    std::string text(it.data.begin(), it.data.end());
    if (!morse::encode(text, &tok)) return false;
    double u = 1.2 / wpm_ * kFs;
    double cg = 3 * u, wg = 7 * u;
    if (fwpm_ < wpm_) {
      double ta = (60 * wpm_ - 37.2 * fwpm_) / (fwpm_ * wpm_) * kFs;
      cg = 3 * ta / 19;
      wg = 7 * ta / 19;
    }
    seg_.clear();
    seg_.push_back({false, static_cast<size_t>(opt_.txdelay_ms * kFs / 1000)});
    size_t start = 0;
    while (start < tok.size()) {
      size_t bar = tok.find('|', start);
      if (bar == std::string::npos) bar = tok.size();
      std::string t = tok.substr(start, bar - start);
      start = bar + 1;
      if (t == " ") {
        // Word gap replaces the character gap already appended.
        if (!seg_.empty() && !seg_.back().on) seg_.back().len = static_cast<size_t>(wg);
        continue;
      }
      for (size_t k = 0; k < t.size(); ++k) {
        seg_.push_back({true, static_cast<size_t>((t[k] == '-' ? 3 : 1) * u)});
        seg_.push_back({false, static_cast<size_t>(k + 1 < t.size() ? u : cg)});
      }
    }
    seg_.push_back({false, static_cast<size_t>(opt_.txtail_ms * kFs / 1000) + rise_});
    si_ = 0;
    spos_ = 0;
    env_ = 0;
    tnco_.set_freq(tone_, kFs);
    tnco_.reset();
    return true;
  }

  size_t tx_generate(float* out, size_t max) override {
    size_t n = 0;
    float step = rise_ ? 1.0f / static_cast<float>(rise_) : 1.0f;
    while (n < max && si_ < seg_.size()) {
      const Seg& s = seg_[si_];
      if (spos_ >= s.len) { ++si_; spos_ = 0; continue; }
      // Linear ramp of the phase variable, shaped by a raised cosine.
      ramp_ = s.on ? std::min(1.0f, ramp_ + step) : std::max(0.0f, ramp_ - step);
      env_ = 0.5f - 0.5f * std::cos(static_cast<float>(dsp::kPi) * ramp_);
      out[n++] = env_ * tnco_.next_sin();
      ++spos_;
    }
    return n;
  }

  void tx_reset() override { seg_.clear(); si_ = 0; }

 private:
  struct Seg {
    bool on;
    size_t len;
  };

  // config
  double wpm_ = 20, fwpm_ = 20, tone_ = 700, base_tone_ = 700;
  size_t rise_ = 40;
  // rx
  CwReceiver rx_;
  // tx
  std::vector<Seg> seg_;
  size_t si_ = 0, spos_ = 0;
  float env_ = 0, ramp_ = 0;
  dsp::Nco tnco_;

 public:
  void set_base_tone(double t) { base_tone_ = t; }
};

}  // namespace

std::unique_ptr<Modem> create_cw(const std::string&, uint32_t sample_rate, const ModemOptions& opt,
                                 std::string* error) {
  static const char* const kKeys[] = {"wpm", "farnsworth_wpm", "tone_hz", "rise_ms", "bw_hz", "afc", nullptr};
  if (!check_keys(opt.params, kKeys, error)) return nullptr;
  double wpm, fw, tone, rise, bw;
  bool afc;
  if (!param_double(opt.params, "wpm", 20, 5, 60, &wpm, error)) return nullptr;
  if (!param_double(opt.params, "farnsworth_wpm", 0, 0, 60, &fw, error)) return nullptr;
  if (!param_double(opt.params, "tone_hz", 700, 300, 2500, &tone, error)) return nullptr;
  if (!param_double(opt.params, "rise_ms", 5, 0, 20, &rise, error)) return nullptr;
  if (!param_double(opt.params, "bw_hz", 100, 30, 400, &bw, error)) return nullptr;
  if (!param_bool(opt.params, "afc", true, &afc, error)) return nullptr;
  std::unique_ptr<CwModem> m(new CwModem(sample_rate, opt));
  m->set_base_tone(tone);
  if (!m->setup(wpm, fw, tone, rise, bw, afc)) {
    if (error) *error = "bad sample rate";
    return nullptr;
  }
  return m;
}

}  // namespace modem
}  // namespace ember
