#include "modem/dpsk.h"

#include <cmath>
#include <vector>

#include "dsp/fir.h"
#include "dsp/nco.h"
#include "ember/crc.h"
#include "modem/hdlc.h"

namespace ember {
namespace modem {

namespace {

using dsp::Cf;
constexpr double kFs = 9600;
constexpr size_t kSpan = 8;

class G3ruh {
 public:
  uint8_t scramble(uint8_t in) {
    uint8_t out = static_cast<uint8_t>((in ^ (s_ >> 11) ^ (s_ >> 16)) & 1);
    s_ = ((s_ << 1) | out) & 0x1FFFF;
    return out;
  }
  uint8_t descramble(uint8_t in) {
    uint8_t out = static_cast<uint8_t>((in ^ (s_ >> 11) ^ (s_ >> 16)) & 1);
    s_ = ((s_ << 1) | (in & 1)) & 0x1FFFF;
    return out;
  }
  void reset() { s_ = 0; }

 private:
  uint32_t s_ = 0;
};

class DpskModem final : public Modem {
 public:
  DpskModem(uint32_t rate, const ModemOptions& opt, int m, double baud, double alpha, double fc)
      : Modem(rate, 9600, opt), M_(m), baud_(baud), alpha_(alpha), fc_(fc) {
    bps_ = m == 8 ? 3 : 2;
    sps_ = static_cast<size_t>(kFs / baud + 0.5);
  }

  bool setup(const std::string& name) {
    info_.name = name;
    info_.title = M_ == 8 ? "D8PSK 4800 bit/s (HDLC, clean cable)" : "DQPSK 2400 bit/s (HDLC, clean cable)";
    info_.bitrate_bps = static_cast<uint32_t>(baud_ * bps_);
    info_.max_frame = 2048;
    info_.frames = true;
    info_.bw_lo_hz = static_cast<float>(fc_ - baud_ * (1 + alpha_) / 2);
    info_.bw_hi_hz = static_cast<float>(fc_ + baud_ * (1 + alpha_) / 2);
    if (!init_base()) return false;
    rrc_ = dsp::fir_rrc(sps_, kSpan, alpha_);
    mf_.set_taps(rrc_);
    txf_.set_taps(rrc_);
    nco_.set_freq(fc_, kFs);
    tnco_.set_freq(fc_, kFs);
    hdlc_.init(opt_.min_frame, opt_.max_frame);
    hist_.assign(64, Cf());
    next_ = static_cast<double>(sps_);
    // Peak normalisation of the TX waveform: RRC pulses with random phases peak at ~1.6x the
    // per-symbol amplitude after unit-energy normalisation scaled by sqrt(sps).
    tx_gain_ = static_cast<float>(1.0 / (std::sqrt(static_cast<double>(sps_)) * 0.55 * 1.6 * 1.7));
    return true;
  }

  bool detect() const override { return perr_ < 0.35f || (hdlc_.collecting() && hdlc_.bits_since_flag() < 200); }

 protected:
  void demod(const float* x, size_t n) override {
    for (size_t i = 0; i < n; ++i) {
      Cf z = mf_.process(nco_.next_conj() * x[i]);
      pow_ += 0.002f * (dsp::norm2(z) - pow_);
      hist_[hpos_ % hist_.size()] = z;
      ++hpos_;
      ++t_;
      // Symbol strobe when the interpolation point (and the half-symbol point before it) is in.
      while (t_ - 1.0 >= next_) {
        Cf y = interp(next_);
        Cf ymid = interp(next_ - static_cast<double>(sps_) / 2);
        // Gardner TED, normalised by signal power.
        float e = ((y.re - yprev_.re) * ymid.re + (y.im - yprev_.im) * ymid.im) / (pow_ + 1e-20f);
        if (e > 1) e = 1;
        if (e < -1) e = -1;
        next_ += static_cast<double>(sps_) - 0.08 * e * static_cast<double>(sps_) * 0.5;
        symbol(y);
        yprev_ = y;
      }
    }
  }

  bool tx_begin() override {
    std::vector<Bytes> frames;
    TxItem it;
    while (frames.size() < opt_.max_frames_per_tx && take_tx(&it)) frames.push_back(std::move(it.data));
    if (frames.empty()) return false;
    double bitrate = baud_ * bps_;
    size_t pre = static_cast<size_t>(std::ceil(opt_.txdelay_ms * bitrate / 8000.0));
    size_t post = static_cast<size_t>(std::ceil(opt_.txtail_ms * bitrate / 8000.0));
    if (pre < 8) pre = 8;
    if (post < 2) post = 2;
    std::vector<uint8_t> bits = hdlc_encode(frames, pre, post);
    G3ruh sc;
    for (uint8_t& b : bits) b = sc.scramble(b);
    while (bits.size() % static_cast<size_t>(bps_)) bits.push_back(0);
    syms_.clear();
    double phi = 0;
    for (size_t i = 0; i < bits.size(); i += static_cast<size_t>(bps_)) {
      unsigned b = 0;
      for (int k = 0; k < bps_; ++k) b = (b << 1) | bits[i + static_cast<size_t>(k)];
      unsigned k = b;  // inverse Gray
      for (unsigned sh = 1; sh < 8; sh <<= 1) k ^= k >> sh;
      phi += (2.0 * k + 1) * dsp::kPi / M_;
      syms_.push_back(dsp::expj(phi));
    }
    txf_.reset();
    tnco_.reset();
    gi_ = 0;
    total_ = (syms_.size() + kSpan) * sps_;
    return true;
  }

  size_t tx_generate(float* out, size_t max) override {
    size_t n = 0;
    while (n < max && gi_ < total_) {
      size_t sym = static_cast<size_t>(gi_ / sps_);  // < syms_.size() + kSpan (gi_ < total_)
      Cf a;
      if (gi_ % sps_ == 0 && sym < syms_.size()) a = syms_[sym];
      Cf b = txf_.process(a) * tx_gain_ * static_cast<float>(sps_);
      Cf c = tnco_.next();
      out[n++] = b.re * c.re - b.im * c.im;
      ++gi_;
    }
    return n;
  }

  void tx_reset() override { syms_.clear(); total_ = gi_ = 0; }

 private:
  Cf interp(double t) const {
    // t is an absolute sample time; hist_ holds the last 64 samples, newest at t_ - 1.
    double fl = std::floor(t);
    double mu = t - fl;
    uint64_t i0 = static_cast<uint64_t>(fl < 0 ? 0 : fl);
    if (t_ < 1 || i0 + 64 < t_) return Cf();
    Cf a = hist_[i0 % hist_.size()];
    Cf b = (i0 + 1 < t_) ? hist_[(i0 + 1) % hist_.size()] : a;
    return a * static_cast<float>(1 - mu) + b * static_cast<float>(mu);
  }

  void symbol(Cf y) {
    Cf d = y * dsp::conj(prev_sym_);
    prev_sym_ = y;
    if (dsp::norm2(d) < 1e-30f) return;
    double ang = std::atan2(d.im, d.re) - dsp::kPi / M_;
    double step = dsp::kTwoPi / M_;
    double kf = std::floor(ang / step + 0.5);
    double err = ang - kf * step;
    int k = static_cast<int>(kf);
    k %= M_;
    if (k < 0) k += M_;
    // Normalised phase-error power: ~0 for a clean constellation, ~1 for noise (uniform error).
    perr_ += 0.02f * (static_cast<float>(err * err) / static_cast<float>(step * step / 12) - perr_);
    unsigned b = static_cast<unsigned>(k ^ (k >> 1));
    for (int i = bps_ - 1; i >= 0; --i) {
      uint8_t bit = desc_.descramble(static_cast<uint8_t>((b >> i) & 1));
      ev_s2_ += err * err;
      ++ev_n_;
      if (hdlc_.push(bit)) deliver();
      if (hdlc_.bits_since_flag() == 0 && !hdlc_.collecting()) { ev_s2_ = 0; ev_n_ = 0; }
    }
  }

  void deliver() {
    RxQuality q;
    if (ev_n_ > 8) {
      double var = ev_s2_ / static_cast<double>(ev_n_);
      // Differential detection doubles the noise variance of the phase difference.
      q.has_snr = true;
      q.snr_db = static_cast<float>(10 * std::log10(1.0 / (var + 1e-9)));
      if (q.snr_db > 50) q.snr_db = 50;
    }
    emit_frame(hdlc_.frame(), q);
  }

  int M_;
  double baud_, alpha_, fc_;
  int bps_ = 2;
  size_t sps_ = 8;
  std::vector<float> rrc_;
  dsp::FirC mf_, txf_;
  dsp::Nco nco_, tnco_;
  float tx_gain_ = 1;
  // rx
  std::vector<Cf> hist_;
  uint64_t hpos_ = 0, t_ = 0;
  double next_ = 8;
  float pow_ = 1e-6f;
  Cf yprev_, prev_sym_{1, 0};
  float perr_ = 1;
  G3ruh desc_;
  HdlcDecoder hdlc_;
  double ev_s2_ = 0;
  size_t ev_n_ = 0;
  // tx
  std::vector<Cf> syms_;
  uint64_t gi_ = 0, total_ = 0;
};

}  // namespace

std::unique_ptr<Modem> create_dpsk(const std::string& name, uint32_t sample_rate, const ModemOptions& opt,
                                   std::string* error) {
  static const char* const kKeys[] = {"carrier_hz", nullptr};
  if (!check_keys(opt.params, kKeys, error)) return nullptr;
  double fc;
  if (!param_double(opt.params, "carrier_hz", 1800, 1200, 2400, &fc, error)) return nullptr;
  bool fast = name == "psk4800";
  std::unique_ptr<DpskModem> m(new DpskModem(sample_rate, opt, fast ? 8 : 4, fast ? 1600 : 1200, fast ? 0.35 : 0.5, fc));
  if (!m->setup(name)) {
    if (error) *error = "bad sample rate";
    return nullptr;
  }
  return m;
}

}  // namespace modem
}  // namespace ember
