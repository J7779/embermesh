#include "modem/afsk.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "dsp/agc.h"
#include "dsp/fir.h"
#include "dsp/nco.h"
#include "ember/crc.h"
#include "modem/hdlc.h"

namespace ember {
namespace modem {

namespace {

using dsp::Cf;

constexpr int kMaxSlicers = 7;
const float kGains[kMaxSlicers] = {0.0f /* normalised */, 1.0f, 0.5f, 0.70710678f, 1.41421356f, 2.0f, 2.82842712f};

struct Slicer {
  float prev = 0;
  double phase = 0;
  NrziDecoder nrzi;
  HdlcDecoder hdlc;
  uint32_t hist = 0;   // 1 bit per transition: good timing
  bool dcd = false;
  uint32_t bits_no_xing = 0;  // bit periods since the last zero crossing
  double s1 = 0, s2 = 0;
  uint32_t n = 0;
};

struct Recent {
  uint32_t crc = 0;
  size_t len = 0;
  uint64_t at = 0;
};

class AfskModem final : public Modem {
 public:
  AfskModem(uint32_t rate, uint32_t irate, const ModemOptions& opt, double mark, double space, double baud,
            int nslicers)
      : Modem(rate, irate, opt), mark_(mark), space_(space), baud_(baud), nslicers_(nslicers) {}

  bool setup(const std::string& name) {
    info_.name = name;
    info_.title = name == "afsk300" ? "AFSK 300 bd HF packet (HDLC)" : "AFSK 1200 bd Bell 202 packet (HDLC/AX.25)";
    info_.bitrate_bps = static_cast<uint32_t>(baud_);
    info_.max_frame = 2048;
    info_.frames = true;
    info_.text = false;
    double lo = std::min(mark_, space_), hi = std::max(mark_, space_);
    info_.bw_lo_hz = static_cast<float>(lo - baud_ / 2);
    info_.bw_hi_hz = static_cast<float>(hi + baud_ / 2);
    if (!init_base()) return false;

    double fs = irate_;
    spb_ = fs / baud_;
    size_t pre_taps = static_cast<size_t>(std::ceil(2.5 * spb_)) | 1;
    double plo = lo - 0.6 * baud_, phi = hi + 0.6 * baud_;
    if (plo < 100) plo = 100;
    if (phi > 0.48 * fs) phi = 0.48 * fs;
    pre_.set_taps(dsp::fir_bandpass(pre_taps, plo, phi, fs, dsp::Window::Hamming));
    size_t ct = static_cast<size_t>(std::lround(corr_len_bits_ * spb_)) | 1;
    // Default: a boxcar of 1.2 bit (the non-coherent matched filter, slightly stretched), which
    // measured best on flat and de-/pre-emphasised audio. Other windows kept for experiments.
    std::vector<float> lp = corr_win_ == 0 ? std::vector<float>(ct, 1.0f / static_cast<float>(ct))
                                           : dsp::fir_lowpass(ct, corr_cut_ * baud_, fs, static_cast<dsp::Window>(corr_win_));
    mf_.set_taps(lp);
    sf_.set_taps(lp);
    nco_m_.set_freq(mark_, fs);
    nco_s_.set_freq(space_, fs);
    // Envelope normalisation: fast attack, ~ 30 bit decay.
    float att = 0.5f, dec = dsp::ema_coeff(30.0 / baud_, fs);
    pv_m_.init(att, dec);
    pv_s_.init(att, dec);
    for (int k = 0; k < kMaxSlicers; ++k) sl_[k].hdlc.init(opt_.min_frame, opt_.max_frame);
    step_ = baud_ / fs;
    inc_m_ = dsp::Nco::cycles_to_u32(mark_ / fs);
    inc_s_ = dsp::Nco::cycles_to_u32(space_ / fs);
    return true;
  }

  bool detect() const override {
    for (int k = 0; k < nslicers_; ++k) {
      const Slicer& s = sl_[first_ + k];
      // A flag alone is not a carrier: random noise contains 01111110 several times a second at
      // 1200 bit/s. Count "collecting after a flag" only with reasonable bit-timing lock too.
      if (s.dcd || (s.hdlc.collecting() && s.hdlc.bits_since_flag() < 64 && popcount(s.hist) >= 20)) return true;
    }
    return false;
  }

 protected:
  void demod(const float* x, size_t n) override {
    for (size_t i = 0; i < n; ++i) {
      float v = pre_.process(x[i]);
      Cf cm = nco_m_.next_conj(), cs = nco_s_.next_conj();
      mf_.push(cm * v);
      sf_.push(cs * v);
      float m = dsp::cabs(mf_.output()), s = dsp::cabs(sf_.output());
      ++now_;
      if (first_ == 0) {
        pv_m_.push(m);
        pv_s_.push(s);
        slice(0, pv_m_.normalise(m) - pv_s_.normalise(s));
      }
      for (int k = (first_ == 0 ? 1 : first_); k < first_ + nslicers_; ++k) slice(k, m - kGains[k] * s);
    }
  }

  bool tx_begin() override {
    std::vector<Bytes> frames;
    TxItem it;
    while (frames.size() < opt_.max_frames_per_tx && take_tx(&it)) frames.push_back(std::move(it.data));
    if (frames.empty()) return false;
    size_t pre = static_cast<size_t>(std::ceil(opt_.txdelay_ms * baud_ / 8000.0));
    size_t post = static_cast<size_t>(std::ceil(opt_.txtail_ms * baud_ / 8000.0));
    if (pre < 1) pre = 1;
    if (post < 1) post = 1;
    bits_ = hdlc_encode(frames, pre, post);
    uint8_t level = 1;
    nrzi_encode(bits_, &level);
    bit_idx_ = 0;
    bit_pos_ = 0;
    phase_ = 0;
    gen_i_ = 0;
    total_ = static_cast<uint64_t>(std::ceil(static_cast<double>(bits_.size()) * spb_));
    ramp_ = static_cast<uint64_t>(irate_ / 1000);
    return true;
  }

  size_t tx_generate(float* out, size_t max) override {
    size_t n = 0;
    while (n < max && gen_i_ < total_ && bit_idx_ < bits_.size()) {
      phase_ += bits_[bit_idx_] ? inc_m_ : inc_s_;
      float v = dsp::sin_u32(phase_);
      uint64_t left = total_ - gen_i_;
      if (gen_i_ < ramp_) v *= 0.5f - 0.5f * std::cos(static_cast<float>(dsp::kPi) * gen_i_ / ramp_);
      else if (left < ramp_) v *= 0.5f - 0.5f * std::cos(static_cast<float>(dsp::kPi) * left / ramp_);
      out[n++] = v;
      ++gen_i_;
      bit_pos_ += step_;
      if (bit_pos_ >= 1.0) { bit_pos_ -= 1.0; ++bit_idx_; }
    }
    return n;
  }

  void tx_reset() override {
    bits_.clear();
    bit_idx_ = 0;
    total_ = gen_i_ = 0;
  }

 private:
  void slice(int k, float v) {
    Slicer& s = sl_[k];
    s.phase += step_;
    if ((v >= 0) != (s.prev >= 0)) {
      float den = s.prev - v;
      double frac = den != 0 ? s.prev / den : 0.5;  // crossing position within this sample interval
      if (frac < 0) frac = 0;
      if (frac > 1) frac = 1;
      double at = s.phase - (1.0 - frac) * step_;
      double err = at - 0.5;
      err -= std::floor(err + 0.5);  // wrap to [-0.5, 0.5)
      bool good = std::fabs(err) < 0.15;
      s.bits_no_xing = 0;
      s.hist = (s.hist << 1) | (good ? 1u : 0u);
      int pc = popcount(s.hist);
      if (pc >= 22) s.dcd = true;
      else if (pc < 16) s.dcd = false;
      bool locked = s.hdlc.bits_since_flag() < 2000;
      s.phase -= err * (locked ? 0.25 : 0.5);
    }
    if (s.phase >= 1.0) {
      s.phase -= 1.0;
      // NRZI + bit stuffing guarantee a transition at least every 6 bits in a real signal; with no
      // crossings at all (silence, DC) the timing history would otherwise freeze DCD on.
      if (++s.bits_no_xing > 16) {
        s.dcd = false;
        s.hist = 0;
      }
      double back = s.phase / step_;  // samples since the ideal sampling instant
      if (back > 1) back = 1;
      float val = v - (v - s.prev) * static_cast<float>(back);
      s.s1 += std::fabs(val);
      s.s2 += static_cast<double>(val) * val;
      ++s.n;
      uint8_t bit = s.nrzi.push(val > 0 ? 1 : 0);
      if (s.hdlc.push(bit)) deliver(k, s);
      if (s.hdlc.bits_since_flag() == 0 && !s.hdlc.collecting()) {
        // flag: restart eye statistics for the next frame
        s.s1 = s.s2 = 0;
        s.n = 0;
      }
    }
    s.prev = v;
  }

  void deliver(int k, Slicer& s) {
    ByteView f = s.hdlc.frame();
    uint32_t c = crc32(f);
    uint64_t window = static_cast<uint64_t>(12 * spb_);
    for (const Recent& r : recent_) {
      if (r.crc == c && r.len == f.size() && now_ - r.at < window) return;
    }
    recent_[recent_i_] = Recent{c, f.size(), now_};
    recent_i_ = (recent_i_ + 1) % kRecent;
    RxQuality q;
    q.decoder = static_cast<uint8_t>(k);
    if (s.n > 8) {
      double mean = s.s1 / s.n, var = s.s2 / s.n - mean * mean;
      if (var < 1e-12 * (mean * mean + 1e-30)) var = 1e-12 * (mean * mean + 1e-30);
      q.has_snr = true;
      q.snr_db = static_cast<float>(10 * std::log10(mean * mean / var));
      if (q.snr_db > 60) q.snr_db = 60;
    }
    emit_frame(f, q);
  }

  static int popcount(uint32_t v) {
    int c = 0;
    while (v) { v &= v - 1; ++c; }
    return c;
  }

  static constexpr int kRecent = 8;

  double mark_, space_, baud_;
  int nslicers_;
  int first_ = 0;  // index of the first active slicer (1 when single-slicer mode)
  double spb_ = 10, step_ = 0.1;
  double corr_len_bits_ = 1.2, corr_cut_ = 0.5;
  int corr_win_ = 0;
  dsp::Fir pre_;
  dsp::FirC mf_, sf_;
  dsp::Nco nco_m_, nco_s_;
  dsp::PeakValley pv_m_, pv_s_;
  Slicer sl_[kMaxSlicers];
  Recent recent_[kRecent];
  int recent_i_ = 0;
  uint64_t now_ = 0;

  // TX
  std::vector<uint8_t> bits_;
  size_t bit_idx_ = 0;
  double bit_pos_ = 0;
  uint32_t phase_ = 0, inc_m_ = 0, inc_s_ = 0;
  uint64_t total_ = 0, gen_i_ = 0, ramp_ = 12;

 public:
  void set_single() { first_ = 1; nslicers_ = 1; }
  void set_corr(double len_bits, double cut, int win) { corr_len_bits_ = len_bits; corr_cut_ = cut; corr_win_ = win; }
};

}  // namespace

std::unique_ptr<Modem> create_afsk(const std::string& name, uint32_t sample_rate, const ModemOptions& opt,
                                   std::string* error) {
  static const char* const kKeys[] = {"mark_hz", "space_hz", "baud", "slicers", "corr_len", "corr_cut", "corr_win", nullptr};
  if (!check_keys(opt.params, kKeys, error)) return nullptr;
  bool hf = name == "afsk300";
  double mark, space, baud, slicers, clen, ccut, cwin;
  if (!param_double(opt.params, "mark_hz", hf ? 1600 : 1200, 300, 3500, &mark, error)) return nullptr;
  if (!param_double(opt.params, "space_hz", hf ? 1800 : 2200, 300, 3500, &space, error)) return nullptr;
  if (!param_double(opt.params, "baud", hf ? 300 : 1200, 50, 2400, &baud, error)) return nullptr;
  if (!param_double(opt.params, "slicers", 7, 1, 7, &slicers, error)) return nullptr;
  if (!param_double(opt.params, "corr_len", 1.2, 0.3, 3.0, &clen, error)) return nullptr;
  if (!param_double(opt.params, "corr_cut", 0.5, 0.01, 2.0, &ccut, error)) return nullptr;
  if (!param_double(opt.params, "corr_win", 0, 0, 4, &cwin, error)) return nullptr;
  if (std::fabs(mark - space) < baud / 4) {
    if (error) *error = "mark/space too close for the baud rate";
    return nullptr;
  }
  // Internal rate: >= 4.4x the highest tone and a few samples per bit.
  double hi = std::max(mark, space);
  uint32_t irate = baud >= 600 ? 12000 : 9600;
  if (hi * 2.3 > irate) irate = 16000;
  std::unique_ptr<AfskModem> m(new AfskModem(sample_rate, irate, opt, mark, space, baud, 7));
  m->set_corr(clen, ccut, static_cast<int>(cwin));
  if (slicers < 2) m->set_single();
  if (!m->setup(name)) {
    if (error) *error = "bad sample rate";
    return nullptr;
  }
  return m;
}

}  // namespace modem
}  // namespace ember
