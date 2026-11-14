// Synthetic camera scenes for the whole-frame light tracker (docs/RPC_API.md §12): renders a
// full-resolution 8-bit luma image per frame and reduces it to max / mean grids exactly as a
// capture host does, so sub-cell and single-pixel lights are tested honestly (a one-pixel light
// only shows if it is brighter than the brightest background pixel of its cell; sensor noise is
// max-pooled, i.e. biased upward).
//
// Modelled: background texture (dark room or daylight), per-pixel sensor noise (read + shot),
// point lights with a sub-pixel Gaussian spot, saturation and bloom; keyed lights (a Morse
// timeline, integrated over the exposure), steady lights, mains flicker integrated over the
// exposure (aliases at 24/30 fps), moving lights (headlights) and large moving objects (someone
// walking past); global gain changes (auto-exposure pumping, and AE that follows the mean);
// hand shake (random-walk velocity of the camera), sudden jumps, the camera pointing away for a
// while; frame-time jitter and dropped frames. Seeded and deterministic.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "morse_sim.h"

namespace scenesim {

using morsesim::Rng;
using morsesim::Timeline;

struct Light {
  double x = 0, y = 0;       // world position, pixels (the camera starts looking at 0..W, 0..H)
  double amp = 100;          // peak luma added at the centre (may exceed 255: saturates)
  double sigma_px = 0.5;     // spot size: 0.5 ≈ one or two pixels
  double bloom = 0;          // halo amplitude as a fraction of amp ...
  double bloom_px = 0;       // ... and its radius (sigma, pixels)
  const Timeline* keyed = nullptr;  // null: steady
  double flicker = 0, flicker_hz = 100;  // brightness modulation (mains: 100 or 120 Hz)
  double vx = 0, vy = 0;     // world velocity, pixels per second
  double t_on = -1e300, t_off = 1e300;  // the light exists in [t_on, t_off)
  bool blink = false;        // a periodic blinker: blink_on_ms on every blink_period_ms
  double blink_on_ms = 100, blink_period_ms = 1000;
};

struct Scene {
  int W = 512, H = 288;      // pixels
  int gw = 64, gh = 36;      // grid cells
  double bg = 10;            // base luma
  double texture = 0;        // background texture amplitude (luma), e.g. 40 for daylight
  double texture_px = 24;    // texture feature size
  double noise = 2;          // read noise sigma (luma)
  double shot = 0;           // shot noise: sigma += shot * sqrt(luma)
  double fps = 30, drop = 0, exposure = 0.5;  // exposure: fraction of the frame period
  double jitter_ms = 2;            // jitter of the timestamps the host reports
  double capture_jitter_ms = 0.3;  // jitter of the real exposure times (sensors run on a steady clock)
  double ae_pump = 0, ae_pump_hz = 0.4;  // a global gain wobble
  double ae_follow = 0, ae_target = 60, ae_tau_ms = 600;  // AE pulled toward a mean luma
  double shake_px_s = 0;     // RMS camera speed of hand shake (pixels per second)
  double shake_tau_ms = 400;
  double jump_every_ms = 0, jump_px = 0;  // sudden jerks
  std::vector<std::pair<double, double>> away;  // the camera points elsewhere during these times
  std::vector<std::pair<double, double>> zoom;  // {from t_ms, factor}: the user changes the zoom step
  std::vector<Light> lights;
};

struct Frame {
  double t = 0;                 // the timestamp the host reports
  double tc = 0;                // when the frame was really exposed
  double cam_x = 0, cam_y = 0;  // camera offset (world = image + offset)
  double zoom = 1;              // image = centre + (world - offset - centre) * zoom
  bool away = false;
  std::vector<uint8_t> max, mean;
};

// Truth: a light's position as a fraction of the frame at frame f.
inline void truth(const Scene& s, const Light& l, const Frame& f, double* fx, double* fy) {
  double wx = l.x + l.vx * f.t / 1000, wy = l.y + l.vy * f.t / 1000;
  double cx = s.W / 2.0, cy = s.H / 2.0;
  *fx = (cx + (wx - f.cam_x - cx) * f.zoom + 0.5) / s.W;
  *fy = (cy + (wy - f.cam_y - cy) * f.zoom + 0.5) / s.H;
}

class Renderer {
 public:
  Renderer(const Scene& s, uint64_t seed) : s_(s), r_(seed), img_(static_cast<size_t>(s.W) * s.H), gauss_(65536) {
    for (double& g : gauss_) g = r_.gauss();
    // Value-noise texture (world coordinates, wraps every 64 features), cached for the area a
    // shaking camera covers (computed directly beyond it).
    for (double& v : tex_) v = r_.unit();
    if (s_.texture > 0) {
      cache_.resize(static_cast<size_t>(s_.W + 2 * kMargin) * static_cast<size_t>(s_.H + 2 * kMargin));
      for (int y = 0; y < s_.H + 2 * kMargin; ++y)
        for (int x = 0; x < s_.W + 2 * kMargin; ++x)
          cache_[static_cast<size_t>(y) * static_cast<size_t>(s_.W + 2 * kMargin) + static_cast<size_t>(x)] =
              s_.texture * texture(x - kMargin, y - kMargin);
    }
  }

  // Frames from t = 0 to duration_ms (dropped ones are not returned).
  std::vector<Frame> run(double duration_ms) {
    std::vector<Frame> out;
    double period = 1000.0 / s_.fps;
    double last_t = -1;
    for (double tn = 0; tn < duration_ms; tn += period) {
      double tc = tn + s_.capture_jitter_ms * r_.gauss();  // when the frame really was exposed
      double t = tn + s_.jitter_ms * r_.gauss();            // the timestamp the host reports
      if (t <= last_t) t = last_t + 0.5;
      double dt = last_t < 0 ? period : t - last_t;
      last_t = t;
      step_camera(dt, t);
      Frame f = render(tc);
      f.t = t;
      f.tc = tc;
      if (r_.unit() < s_.drop) continue;
      out.push_back(std::move(f));
    }
    return out;
  }

 private:
  double texture(double x, double y) const {
    double u = x / s_.texture_px, v = y / s_.texture_px;
    double fu = std::floor(u), fv = std::floor(v);
    auto at = [&](double a, double b) {
      uint32_t ia = static_cast<uint32_t>(static_cast<int64_t>(a) & 63), ib = static_cast<uint32_t>(static_cast<int64_t>(b) & 63);
      return tex_[ia * 64 + ib];
    };
    double du = u - fu, dv = v - fv;
    double a = at(fu, fv) * (1 - du) + at(fu + 1, fv) * du;
    double b = at(fu, fv + 1) * (1 - du) + at(fu + 1, fv + 1) * du;
    return a * (1 - dv) + b * dv;
  }

  void step_camera(double dt, double t) {
    if (s_.shake_px_s > 0) {
      double k = dt / s_.shake_tau_ms;
      double sd = s_.shake_px_s * std::sqrt(2 * k);
      vx_ += -vx_ * k + sd * r_.gauss();
      vy_ += -vy_ * k + sd * r_.gauss();
      cx_ += vx_ * dt / 1000;
      cy_ += vy_ * dt / 1000;
      // stay roughly on target (a person re-aims)
      cx_ *= 1 - 0.3 * dt / 1000;
      cy_ *= 1 - 0.3 * dt / 1000;
    }
    if (s_.jump_every_ms > 0 && r_.unit() < dt / s_.jump_every_ms) {
      cx_ += s_.jump_px * (r_.unit() < 0.5 ? -1 : 1) * (0.5 + r_.unit());
      cy_ += s_.jump_px * (r_.unit() < 0.5 ? -1 : 1) * (0.3 + 0.5 * r_.unit());
    }
    (void)t;
  }

  double light_level(const Light& l, double t, double exp_ms) const {
    if (t < l.t_on || t - exp_ms >= l.t_off) return 0;
    double lit = 1;
    if (l.keyed) lit = morsesim::lit_fraction(*l.keyed, t - exp_ms, t);
    if (l.blink) {
      double ph = std::fmod(t, l.blink_period_ms);
      lit = ph < l.blink_on_ms ? 1 : 0;
    }
    double fl = 1;
    if (l.flicker > 0) {
      // sin² intensity of a lamp on mains (flicker_hz = twice the mains frequency), averaged over
      // the exposure: aliases at 24/30 fps.
      double w = 2 * 3.141592653589793 * l.flicker_hz / 1000;
      double a = t - exp_ms, b = t;
      double avg = (std::sin(w * b) - std::sin(w * a)) / (w * (b - a));
      fl = 1 + l.flicker * avg;
    }
    return lit * fl;
  }

  Frame render(double t) {
    Frame f;
    f.t = t;
    double ox = cx_, oy = cy_;
    for (const auto& a : s_.away)
      if (t >= a.first && t < a.second) {
        ox += 3.0 * s_.W;  // somewhere else entirely
        f.away = true;
      }
    f.cam_x = ox;
    f.cam_y = oy;
    double z = 1;
    for (const auto& zz : s_.zoom)
      if (t >= zz.first) z = zz.second;
    f.zoom = z;
    const double zcx = s_.W / 2.0, zcy = s_.H / 2.0;
    double exp_ms = std::max(1.0, s_.exposure * 1000.0 / s_.fps);
    const int W = s_.W, H = s_.H;
    // background
    int iox = static_cast<int>(std::floor(ox)), ioy = static_cast<int>(std::floor(oy));
    bool cached = s_.texture > 0 && z == 1 && iox >= -kMargin && iox <= kMargin && ioy >= -kMargin && ioy <= kMargin;
    for (int y = 0; y < H; ++y) {
      double* row = &img_[static_cast<size_t>(y) * W];
      if (s_.texture <= 0) {
        for (int x = 0; x < W; ++x) row[x] = s_.bg;
      } else if (cached) {
        const double* c = &cache_[static_cast<size_t>(y + ioy + kMargin) * static_cast<size_t>(W + 2 * kMargin) +
                                  static_cast<size_t>(iox + kMargin)];
        for (int x = 0; x < W; ++x) row[x] = s_.bg + c[x];
      } else {
        for (int x = 0; x < W; ++x)
          row[x] = s_.bg + s_.texture * (z == 1 ? texture(x + iox, y + ioy)
                                                : texture((x - zcx) / z + zcx + ox, (y - zcy) / z + zcy + oy));
      }
    }
    // lights
    for (const Light& l : s_.lights) {
      double lv = light_level(l, t, exp_ms);
      if (lv <= 0) continue;
      double px = zcx + (l.x + l.vx * t / 1000 - ox - zcx) * z, py = zcy + (l.y + l.vy * t / 1000 - oy - zcy) * z;
      double rad = std::max(3 * l.sigma_px, 3 * l.bloom_px) + 1;
      int x0 = static_cast<int>(std::floor(px - rad)), x1 = static_cast<int>(std::ceil(px + rad));
      int y0 = static_cast<int>(std::floor(py - rad)), y1 = static_cast<int>(std::ceil(py + rad));
      if (x1 < 0 || y1 < 0 || x0 >= W || y0 >= H) continue;
      for (int y = std::max(0, y0); y <= std::min(H - 1, y1); ++y)
        for (int x = std::max(0, x0); x <= std::min(W - 1, x1); ++x) {
          double dx = x - px, dy = y - py, r2 = dx * dx + dy * dy;
          double v = std::exp(-r2 / (2 * l.sigma_px * l.sigma_px));
          if (l.bloom > 0) v += l.bloom * std::exp(-r2 / (2 * l.bloom_px * l.bloom_px));
          img_[static_cast<size_t>(y) * W + x] += l.amp * lv * v;
        }
    }
    // gain: AE following the mean, plus a pumping wobble
    double gain = gain_;
    if (s_.ae_follow > 0) {
      double sum = 0;
      for (size_t i = 0; i < img_.size(); i += 7) sum += img_[i];
      double mean = sum / static_cast<double>((img_.size() + 6) / 7);
      double want = s_.ae_target / std::max(mean, 1.0);
      gain_ += s_.ae_follow * (1 - std::exp(-1000.0 / s_.fps / s_.ae_tau_ms)) * (want - gain_);
      gain_ = std::min(std::max(gain_, 0.4), 2.5);
      gain = gain_;
    }
    if (s_.ae_pump > 0) gain *= 1 + s_.ae_pump * std::sin(2 * 3.141592653589793 * s_.ae_pump_hz * t / 1000);
    // sensor noise, quantisation, max/mean pooling
    const int gw = s_.gw, gh = s_.gh;
    f.max.assign(static_cast<size_t>(gw) * gh, 0);
    std::vector<uint32_t>& sum = sum_;
    std::vector<uint32_t>& cnt = cnt_;
    sum.assign(static_cast<size_t>(gw) * gh, 0);
    cnt.assign(static_cast<size_t>(gw) * gh, 0);
    for (int y = 0; y < H; ++y) {
      int cy = y * gh / H;
      for (int x = 0; x < W; ++x) {
        int cxl = x * gw / W;
        double v = img_[static_cast<size_t>(y) * W + x] * gain;
        double sd = s_.noise + (s_.shot > 0 ? s_.shot * std::sqrt(std::max(v, 0.0)) : 0.0);
        v += sd * gauss_[r_.next() >> 48];
        int q = static_cast<int>(std::lround(v));
        q = std::min(std::max(q, 0), 255);
        size_t c = static_cast<size_t>(cy) * gw + cxl;
        if (q > f.max[c]) f.max[c] = static_cast<uint8_t>(q);
        sum[c] += static_cast<uint32_t>(q);
        ++cnt[c];
      }
    }
    f.mean.resize(f.max.size());
    for (size_t c = 0; c < f.max.size(); ++c) f.mean[c] = static_cast<uint8_t>(cnt[c] ? sum[c] / cnt[c] : 0);
    return f;
  }

  static constexpr int kMargin = 160;
  Scene s_;
  Rng r_;
  std::vector<double> cache_;
  std::vector<uint32_t> sum_, cnt_;
  std::vector<double> img_;
  std::vector<double> gauss_;
  double tex_[64 * 64];
  double cx_ = 0, cy_ = 0, vx_ = 0, vy_ = 0, gain_ = 1;
};

}  // namespace scenesim
