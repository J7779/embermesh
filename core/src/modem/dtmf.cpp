#include "modem/dtmf.h"

#include <cmath>
#include <vector>

#include "dsp/goertzel.h"
#include "dsp/nco.h"

namespace ember {
namespace modem {

namespace {

constexpr double kFs = 8000;
constexpr size_t kBlock = 205;
const double kRow[4] = {697, 770, 852, 941};
const double kCol[4] = {1209, 1336, 1477, 1633};
const char kKeys[4][5] = {"123A", "456B", "789C", "*0#D"};

bool lookup(char c, int* r, int* col) {
  if (c >= 'a' && c <= 'd') c = static_cast<char>(c - 'a' + 'A');
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      if (kKeys[i][j] == c) { *r = i; *col = j; return true; }
  return false;
}

class DtmfModem final : public Modem {
 public:
  DtmfModem(uint32_t rate, const ModemOptions& opt, double tone_ms, double gap_ms)
      : Modem(rate, 8000, opt), tone_(static_cast<size_t>(tone_ms * 8)), gap_(static_cast<size_t>(gap_ms * 8)) {}

  bool setup() {
    info_.name = "dtmf";
    info_.title = "DTMF digits";
    info_.bitrate_bps = 16;
    info_.frames = false;
    info_.text = true;
    info_.bw_lo_hz = 650;
    info_.bw_hi_hz = 1700;
    if (!init_base()) return false;
    for (int i = 0; i < 4; ++i) { g_[i].set_freq(kRow[i], kFs); g_[4 + i].set_freq(kCol[i], kFs); }
    return true;
  }

  bool detect() const override { return last_ >= 0; }

 protected:
  bool accept_text(std::string_view t) const override {
    bool any = false;
    for (char c : t) {
      int r, col;
      if (c == ' ') continue;
      if (c == ',') { any = true; continue; }
      if (!lookup(c, &r, &col)) return false;
      any = true;
    }
    return any;
  }

  void demod(const float* x, size_t n) override {
    for (size_t i = 0; i < n; ++i) {
      for (auto& g : g_) g.push(x[i]);
      energy_ += static_cast<double>(x[i]) * x[i];
      if (++cnt_ < kBlock) continue;
      block();
      cnt_ = 0;
      energy_ = 0;
      for (auto& g : g_) g.reset();
    }
  }

  bool tx_begin() override {
    TxItem it;
    if (!take_tx(&it)) return false;
    seg_.clear();
    seg_.push_back({-1, -1, static_cast<size_t>(opt_.txdelay_ms * 8)});
    for (uint8_t c : it.data) {
      int r, col;
      if (c == ',') { seg_.push_back({-1, -1, 4000}); continue; }
      if (!lookup(static_cast<char>(c), &r, &col)) continue;
      seg_.push_back({r, col, tone_});
      seg_.push_back({-1, -1, gap_});
    }
    seg_.push_back({-1, -1, static_cast<size_t>(opt_.txtail_ms * 8)});
    si_ = pos_ = 0;
    return true;
  }

  size_t tx_generate(float* out, size_t max) override {
    size_t n = 0;
    while (n < max && si_ < seg_.size()) {
      const Seg& s = seg_[si_];
      if (pos_ >= s.len) {
        ++si_;
        pos_ = 0;
        if (si_ < seg_.size() && seg_[si_].r >= 0) {
          a_.set_freq(kRow[seg_[si_].r], kFs);
          b_.set_freq(kCol[seg_[si_].c], kFs);
          a_.reset();
          b_.reset();
        }
        continue;
      }
      float v = 0;
      if (s.r >= 0) {
        // 2 ms raised-cosine edges.
        size_t edge = 16, left = s.len - pos_;
        float w = 1;
        if (pos_ < edge) w = 0.5f - 0.5f * std::cos(static_cast<float>(dsp::kPi) * pos_ / edge);
        else if (left < edge) w = 0.5f - 0.5f * std::cos(static_cast<float>(dsp::kPi) * left / edge);
        v = 0.5f * w * (a_.next_sin() + b_.next_sin());
      }
      out[n++] = v;
      ++pos_;
    }
    return n;
  }

 private:
  struct Seg {
    int r, c;
    size_t len;
  };

  void block() {
    float p[8];
    for (int i = 0; i < 8; ++i) p[i] = g_[i].tone_power();
    int r = 0, c = 4;
    for (int i = 1; i < 4; ++i) if (p[i] > p[r]) r = i;
    for (int i = 5; i < 8; ++i) if (p[i] > p[c]) c = i;
    bool ok = p[r] > 1e-8f && p[c] > 1e-8f;
    for (int i = 0; i < 4 && ok; ++i) if (i != r && p[i] * 4 > p[r]) ok = false;
    for (int i = 4; i < 8 && ok; ++i) if (i != c && p[i] * 4 > p[c]) ok = false;
    double ms = energy_ / kBlock;
    if (ok && (p[r] + p[c]) < 0.6 * ms) ok = false;
    if (ok) {
      float tw = p[c] / p[r];
      if (tw > 6.3f || tw < 1 / 6.3f) ok = false;
    }
    int d = ok ? r * 4 + (c - 4) : -1;
    if (d >= 0 && d == prev_ && d != reported_) {
      RxQuality q;
      double resid = ms - (p[r] + p[c]);
      q.has_snr = true;
      q.snr_db = static_cast<float>(10 * std::log10((p[r] + p[c]) / (resid > 1e-12 ? resid : 1e-12)));
      emit_text(std::string(1, kKeys[r][c - 4]), q);
      reported_ = d;
    }
    if (d < 0) reported_ = -1;
    prev_ = d;
    last_ = d;
  }

  size_t tone_, gap_;
  dsp::Goertzel g_[8];
  size_t cnt_ = 0;
  double energy_ = 0;
  int prev_ = -1, reported_ = -1, last_ = -1;
  std::vector<Seg> seg_;
  size_t si_ = 0, pos_ = 0;
  dsp::Nco a_, b_;
};

}  // namespace

std::unique_ptr<Modem> create_dtmf(const std::string&, uint32_t sample_rate, const ModemOptions& opt,
                                   std::string* error) {
  static const char* const kKeys2[] = {"tone_ms", "gap_ms", nullptr};
  if (!check_keys(opt.params, kKeys2, error)) return nullptr;
  double t, g;
  if (!param_double(opt.params, "tone_ms", 100, 40, 1000, &t, error)) return nullptr;
  if (!param_double(opt.params, "gap_ms", 100, 40, 1000, &g, error)) return nullptr;
  std::unique_ptr<DtmfModem> m(new DtmfModem(sample_rate, opt, t, g));
  if (!m->setup()) {
    if (error) *error = "bad sample rate";
    return nullptr;
  }
  return m;
}

}  // namespace modem
}  // namespace ember
