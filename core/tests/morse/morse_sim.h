// Test models for Morse receive: hand keying (sloppy timing, dash:dot ratio, Farnsworth, speed
// changes), a camera watching a light (frame rate, jitter, dropped frames, exposure integration,
// auto-exposure pumping, ambient drift, lamp flicker aliasing, sensor noise, level wander from a
// shaking hand), and an audio channel (tone, drift, AWGN at a given SNR, QSB fading, clicks).
// All seeded and deterministic. Key timelines come from the existing SEND side, ember::morse::timing
// (tools/morse.h), so every test is an encode -> pattern -> channel -> decode round trip.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "tools/morse.h"

namespace morsesim {

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ull + 1) {}
  uint64_t next() {
    s ^= s << 13; s ^= s >> 7; s ^= s << 17;
    return s;
  }
  double unit() { return static_cast<double>(next() >> 11) / 9007199254740992.0; }
  double gauss() {
    double u1 = std::max(unit(), 1e-12), u2 = unit();
    return std::sqrt(-2 * std::log(u1)) * std::cos(6.283185307179586 * u2);
  }
};

// Key-down intervals [start, end) in ms.
using Timeline = std::vector<std::pair<double, double>>;

struct HandOptions {
  double wpm = 15;
  double farnsworth_wpm = 0;
  double dash_ratio = 3;   // dash / dot
  double jitter = 0;       // relative sigma of every element and gap
  double lead_ms = 1500;   // key-up before the first element
};

// Timeline for `text` (via ember::morse::timing at the nominal speed, then re-timed by hand).
inline Timeline hand_key(const std::string& text, const HandOptions& o, Rng& r, double t0 = 0) {
  std::vector<int> p = ember::morse::timing(text, o.wpm, o.farnsworth_wpm);
  double dot = 1200.0 / o.wpm;
  Timeline tl;
  double t = t0 + o.lead_ms;
  for (size_t i = 0; i < p.size(); ++i) {
    double d = p[i];
    double k = 1 + o.jitter * r.gauss();
    if (k < 0.35) k = 0.35;
    if (i % 2 == 0) {
      bool dash = d > 2 * dot;
      double len = (dash ? o.dash_ratio : 1.0) * dot * k;
      tl.push_back({t, t + len});
      t += len;
    } else {
      t += d * k;
    }
  }
  return tl;
}

inline double end_of(const Timeline& tl) { return tl.empty() ? 0 : tl.back().second; }

// Fraction of [a, b) during which the key is down.
inline double lit_fraction(const Timeline& tl, double a, double b) {
  if (b <= a) return 0;
  double on = 0;
  auto it = std::lower_bound(tl.begin(), tl.end(), std::make_pair(a, -1e300),
                             [](const std::pair<double, double>& x, const std::pair<double, double>& y) {
                               return x.second < y.first;
                             });
  if (it != tl.begin()) --it;
  for (; it != tl.end() && it->first < b; ++it) {
    double s = std::max(a, it->first), e = std::min(b, it->second);
    if (e > s) on += e - s;
  }
  return on / (b - a);
}

struct CameraOptions {
  double fps = 30;
  double jitter_ms = 2;        // frame time jitter (sigma)
  double drop = 0.0;           // probability a frame is lost
  double exposure = 0.5;       // exposure time as a fraction of the frame period
  double on_level = 0.75, off_level = 0.15;  // scene brightness, before auto exposure
  double ae_strength = 0.0;    // 0..1: auto-exposure pulls the image toward mid grey
  double ae_tau_ms = 700;
  double drift = 0.0;          // amplitude of a slow ambient change
  double flicker = 0.0;        // lamp flicker depth (100 Hz mains, aliased by the frame rate)
  double noise = 0.01;         // sensor noise sigma
  double wander = 0.0;         // relative random walk of the on-level (hand shake)
};

struct Frames {
  std::vector<double> t, v;
};

inline Frames camera(const Timeline& tl, double duration_ms, const CameraOptions& o, Rng& r) {
  Frames f;
  double period = 1000.0 / o.fps;
  double gain = 1.0, wander = 1.0;
  for (double tn = 0; tn < duration_ms; tn += period) {
    double t = tn + o.jitter_ms * r.gauss();
    wander *= 1 + o.wander * 0.15 * r.gauss();
    wander = std::min(std::max(wander, 0.5), 1.5);
    double exp_ms = std::max(1.0, o.exposure * period);
    double lit = lit_fraction(tl, t - exp_ms, t);
    double fl = 1 + o.flicker * std::sin(2 * 3.141592653589793 * 100 * t / 1000.0 + r.unit() * 0.3);
    double amb = o.off_level * (1 + o.drift * std::sin(2 * 3.141592653589793 * t / 20000.0));
    double scene = amb + lit * (o.on_level - o.off_level) * wander * fl;
    // Auto exposure: the gain slowly moves to bring the frame toward 0.45.
    double a = 1 - std::exp(-period / o.ae_tau_ms);
    double want = 0.45 / std::max(scene, 0.02);
    gain += o.ae_strength * a * (want - gain);
    gain = std::min(std::max(gain, 0.3), 3.0);
    double v = scene * gain + o.noise * r.gauss();
    v = std::min(std::max(v, 0.0), 1.0);
    if (r.unit() < o.drop) continue;
    f.t.push_back(t);
    f.v.push_back(v);
  }
  return f;
}

struct AudioOptions {
  uint32_t rate = 8000;
  double tone_hz = 700;
  double drift_hz_per_s = 0;
  double snr_db = 30;          // tone power vs noise power in a 2500 Hz bandwidth
  double qsb_depth = 0;        // 0..1 fading depth
  double qsb_hz = 0.2;
  double clicks_per_s = 0;     // impulse noise
  double amplitude = 0.3;
  double rise_ms = 5;
};

inline std::vector<int16_t> audio(const Timeline& tl, double duration_ms, const AudioOptions& o, Rng& r) {
  size_t n = static_cast<size_t>(duration_ms * o.rate / 1000);
  std::vector<int16_t> out(n);
  double ps = o.amplitude * o.amplitude / 2;
  double pn_2500 = ps / std::pow(10, o.snr_db / 10);
  double sigma = std::sqrt(pn_2500 * (o.rate / 2.0) / 2500.0);
  double ph = 0;
  size_t k = 0;
  double env = 0, rise = std::max(1.0, o.rise_ms * o.rate / 1000);
  for (size_t i = 0; i < n; ++i) {
    double t = 1000.0 * static_cast<double>(i) / o.rate;
    while (k < tl.size() && tl[k].second <= t) ++k;
    bool on = k < tl.size() && tl[k].first <= t;
    env += on ? (env < 1 ? 1 / rise : 0) : (env > 0 ? -1 / rise : 0);
    env = std::min(std::max(env, 0.0), 1.0);
    double f = o.tone_hz + o.drift_hz_per_s * t / 1000;
    ph += 2 * 3.141592653589793 * f / o.rate;
    double fade = 1 - o.qsb_depth * 0.5 * (1 + std::sin(2 * 3.141592653589793 * o.qsb_hz * t / 1000));
    double s = o.amplitude * env * fade * std::sin(ph) + sigma * r.gauss();
    if (o.clicks_per_s > 0 && r.unit() < o.clicks_per_s / o.rate) s += (r.unit() < 0.5 ? -0.9 : 0.9);
    double v = s * 32767;
    out[i] = static_cast<int16_t>(std::min(std::max(v, -32768.0), 32767.0));
  }
  return out;
}

// Splits "<SOS>" style prosigns into one symbol each; other bytes are symbols themselves.
inline std::vector<std::string> symbols(const std::string& s) {
  std::vector<std::string> v;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '<') {
      size_t e = s.find('>', i);
      if (e != std::string::npos && e - i <= 5) {
        v.push_back(s.substr(i, e - i + 1));
        i = e;
        continue;
      }
    }
    unsigned char c = static_cast<unsigned char>(s[i]);
    size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    v.push_back(s.substr(i, n));
    i += n - 1;
  }
  return v;
}

inline std::string normalise(const std::string& s) {
  std::string o;
  for (char c : s) {
    char u = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c;
    if (u == ' ' && (o.empty() || o.back() == ' ')) continue;
    o.push_back(u);
  }
  while (!o.empty() && o.back() == ' ') o.pop_back();
  return o;
}

// Character error rate: edit distance / reference length (spaces count as characters).
inline double cer(const std::string& ref, const std::string& got) {
  std::vector<std::string> a = symbols(normalise(ref)), b = symbols(normalise(got));
  std::vector<size_t> prev(b.size() + 1), cur(b.size() + 1);
  for (size_t j = 0; j <= b.size(); ++j) prev[j] = j;
  for (size_t i = 1; i <= a.size(); ++i) {
    cur[0] = i;
    for (size_t j = 1; j <= b.size(); ++j)
      cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
    std::swap(prev, cur);
  }
  return a.empty() ? (b.empty() ? 0 : 1) : static_cast<double>(prev[b.size()]) / static_cast<double>(a.size());
}

}  // namespace morsesim
