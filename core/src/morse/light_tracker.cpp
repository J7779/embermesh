#include "morse/light_tracker.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace ember {
namespace morse_rx {

namespace {

constexpr float kMinStep = 5.0f;      // smallest brightness step (0..255 levels) that can be a light
constexpr float kOnNoiseK = 7.3f;     // threshold = 7.3 × 30th percentile of |Δ| ≈ 4 sigma
constexpr float kQ = 0.545f;          // 30th percentile of |Δ| of white noise, in sigmas
constexpr double kMinRun = 30;        // shortest mark/space that counts (ms)
constexpr double kMaxMark = 3000;     // longest mark (a lamp held on is not keying)
constexpr double kMaxSpace = 3500;    // a longer space ends a keyed sequence
constexpr double kSelectedDropMs = 10000;
// Contrast / noise of the track's signal needed to lock, and at least this many levels. Max-pooled
// noise has a heavy upper tail, so these are well above what Gaussian statistics would suggest;
// lights below them do not decode reliably anyway.
constexpr float kLockSnr = 8;
constexpr float kLockAmp = 12;
constexpr float kQuickSnr = 12;       // four marks suffice at this contrast, else six
constexpr float kMaxLockSize = 2.2f;  // radius in cells (a bloomed point light is ≤ ~1.7)

double alpha(double dt, double tau) { return dt <= 0 ? 0 : 1.0 - std::exp(-dt / tau); }

float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

// p-quantile tracker step (stochastic approximation); absolute step proportional to the value.
void quantile_step(float* q, float d, float p, float rate) {
  float eta = rate * (*q) + rate;
  *q += d <= *q ? -eta * (1 - p) : eta * p;
  if (*q < 0.3f) *q = 0.3f;
  if (*q > 128.0f) *q = 128.0f;
}

}  // namespace

const char* target_state_name(Target::State s) {
  switch (s) {
    case Target::kLocked: return "locked";
    case Target::kLost: return "lost";
    default: return "searching";
  }
}

LightTracker::LightTracker() { reset(); }

void LightTracker::ensure_buffers() {
  if (!cur_.empty()) return;
  const size_t n = kMaxGridCells;
  for (std::vector<float>* v : {&cur_, &p1_, &p2_, &dc_, &d1_, &d2_, &ref_, &nq_, &e_, &f_, &tmp_, &scratch_})
    v->assign(n, 0.0f);
  tracks_.assign(kMaxTracks, Track());
  out_.reserve(kHist + 80);
}

void LightTracker::reset() {
  for (Track& k : tracks_) k.used = false;
  out_.clear();
  non_ = noff_ = 0;
  w_ = h_ = n_ = 0;
  nprev_ = 0;
  ref_mean_ = false;
  have_t_ = false;
  t_ = offset_ = 0;
  tp1_ = tp2_ = 0;
  frame_ms_ = 33;
  dot_ms_ = 100;
  frames_ = 0;
  sel_ = -1;
  fed_t_ = -1e300;
  wait_since_ = -1;
  pin_pending_ = false;
  ev_last_ = Target();
  ev_t_ = -1e300;
}

void LightTracker::resize(int w, int h) {
  if (w_ > 0 && h_ > 0) {
    // Same scene at another grid size (zoom, rotation): keep the tracks, rescaled.
    for (Track& k : tracks_) {
      if (!k.used) continue;
      k.x = clampf((k.x + 0.5f) * static_cast<float>(w) / static_cast<float>(w_) - 0.5f, 0, static_cast<float>(w - 1));
      k.y = clampf((k.y + 0.5f) * static_cast<float>(h) / static_cast<float>(h_) - 0.5f, 0, static_cast<float>(h - 1));
    }
  }
  w_ = w;
  h_ = h;
  n_ = w * h;
  nprev_ = 0;
  std::fill(nq_.begin(), nq_.begin() + n_, 2.0f);  // conservative until the noise is learned
}

// 5×5 maximum (separable, clipped at the borders).
void LightTracker::dilate(const float* src, float* dst) {
  float* t = tmp_.data();
  for (int y = 0; y < h_; ++y) {
    const float* r = src + static_cast<ptrdiff_t>(y) * w_;
    float* o = t + static_cast<ptrdiff_t>(y) * w_;
    for (int x = 0; x < w_; ++x) {
      int a = std::max(0, x - 2), b = std::min(w_ - 1, x + 2);
      float m = r[a];
      for (int i = a + 1; i <= b; ++i) m = std::max(m, r[i]);
      o[x] = m;
    }
  }
  for (int y = 0; y < h_; ++y) {
    int a = std::max(0, y - 2), b = std::min(h_ - 1, y + 2);
    float* o = dst + static_cast<ptrdiff_t>(y) * w_;
    for (int x = 0; x < w_; ++x) {
      float m = t[static_cast<ptrdiff_t>(a) * w_ + x];
      for (int i = a + 1; i <= b; ++i) m = std::max(m, t[static_cast<ptrdiff_t>(i) * w_ + x]);
      o[x] = m;
    }
  }
}

// The light's signal: brightest of the 3×3 cells around the track minus the local background (a
// low percentile of the ring of cells just outside the light and its bloom).
float LightTracker::signal_at(const float* img, const Track& k) const { return signal_at(img, k.x, k.y, k.size); }

float LightTracker::signal_at(const float* img, float fx, float fy, float size) const {
  int cx = std::min(std::max(static_cast<int>(std::lround(fx)), 0), w_ - 1);
  int cy = std::min(std::max(static_cast<int>(std::lround(fy)), 0), h_ - 1);
  float peak = 0;
  for (int y = std::max(0, cy - 1); y <= std::min(h_ - 1, cy + 1); ++y)
    for (int x = std::max(0, cx - 1); x <= std::min(w_ - 1, cx + 1); ++x)
      peak = std::max(peak, img[static_cast<ptrdiff_t>(y) * w_ + x]);
  int rr = std::min(std::max(static_cast<int>(std::ceil(size)) + 2, 3), 5);
  float ring[48];
  int n = 0;
  for (int dy = -rr; dy <= rr; ++dy) {
    int y = cy + dy;
    if (y < 0 || y >= h_) continue;
    bool edge_row = dy == -rr || dy == rr;
    for (int dx = -rr; dx <= rr; dx += edge_row ? 1 : 2 * rr) {
      int x = cx + dx;
      if (x < 0 || x >= w_ || n >= 48) continue;
      ring[n++] = img[static_cast<ptrdiff_t>(y) * w_ + x];
    }
  }
  float bg = 0;
  if (n >= 4) {
    std::nth_element(ring, ring + n / 4, ring + n);
    bg = ring[n / 4];
  }
  return peak - bg;
}

// While lit: follow the light inside its 3×3 (hand shake), centroid of its bright cells, size.
void LightTracker::refine(Track& k) {
  const float* img = cur_.data();
  int cx = std::min(std::max(static_cast<int>(std::lround(k.x)), 0), w_ - 1);
  int cy = std::min(std::max(static_cast<int>(std::lround(k.y)), 0), h_ - 1);
  int bx = cx, by = cy;
  float best = -1;
  for (int y = std::max(0, cy - 1); y <= std::min(h_ - 1, cy + 1); ++y)
    for (int x = std::max(0, cx - 1); x <= std::min(w_ - 1, cx + 1); ++x) {
      float v = img[static_cast<ptrdiff_t>(y) * w_ + x];
      if (v > best) { best = v; bx = x; by = y; }
    }
  float floor = best - 0.5f * std::max(amp(k), kMinStep);
  double sx = 0, sy = 0, sw = 0;
  int count = 0;
  for (int y = std::max(0, by - 2); y <= std::min(h_ - 1, by + 2); ++y)
    for (int x = std::max(0, bx - 2); x <= std::min(w_ - 1, bx + 2); ++x) {
      float v = img[static_cast<ptrdiff_t>(y) * w_ + x] - floor;
      if (v <= 0) continue;
      ++count;
      if (std::abs(x - bx) <= 1 && std::abs(y - by) <= 1) {
        sx += v * static_cast<float>(x);
        sy += v * static_cast<float>(y);
        sw += v;
      }
    }
  if (sw > 0) {
    k.x = static_cast<float>(sx / sw);
    k.y = static_cast<float>(sy / sw);
  }
  float size = 0.5f * std::sqrt(static_cast<float>(std::max(count, 1)));
  k.size += 0.3f * (size - k.size);
}

float LightTracker::amp(const Track& k) const { return std::max(k.onl - k.off, 0.0f); }
// E|Δ| = 2σ/√π for white noise.
float LightTracker::sigma(const Track& k) const { return std::max(0.886f * k.nq, 0.5f); }

void LightTracker::push_hist(Track& k, double t, float v, bool on) {
  k.hist[k.hhead] = Hist{t, v, on};
  k.hhead = (k.hhead + 1) % kHist;
  if (k.hcount < kHist) ++k.hcount;
}

// Hysteresis keying of one track's signal between its own envelopes; records mark/space runs.
void LightTracker::key(Track& k, float sig, double t) {
  double dt = t - t_;
  float floor_amp = std::max(kMinStep, 4 * sigma(k));
  float ae = std::max(amp(k), floor_amp);
  float up = k.off + 0.55f * ae, down = k.off + 0.40f * ae, mid = k.off + 0.5f * ae;
  bool on = k.on ? sig > down : sig > up;
  if (on != k.on) {
    double f = 0.5;
    if (sig != k.last) f = std::min(std::max(static_cast<double>((mid - k.last) / (sig - k.last)), 0.0), 1.0);
    double tc = t_ + f * dt;
    double dur = tc - k.edge;
    k.runs[k.rhead] = Run{tc, static_cast<float>(dur), k.on ? k.run_peak : 0.0f, k.on, k.sharp};
    // A light switching is a step within two frames; a large object drifting past is a ramp.
    k.sharp = on ? sig - std::min(k.last, k.prev) >= 0.5f * ae : std::max(k.last, k.prev) - sig >= 0.5f * ae;
    k.rhead = (k.rhead + 1) % kRuns;
    if (k.nruns < kRuns) ++k.nruns;
    bool was_on = k.on;
    k.edge = tc;
    k.on = on;
    k.same = 0;
    k.run_peak = sig;
    if (was_on) {
      ++k.total_marks;
      evaluate(k, t);
      k.last_mark = t;
    }
  } else {
    ++k.same;
  }
  if (on) {
    k.run_peak = std::max(k.run_peak, sig);
    k.seen = t;
  }
  // Envelopes: the level of the current state, the first (partly exposed) frame of a run excluded.
  float a = static_cast<float>(alpha(dt, 500));
  if (on) {
    if (sig > k.onl) k.onl += 0.4f * (sig - k.onl);
    else if (k.same >= 1) k.onl += a * (sig - k.onl);
  } else if (k.same >= 1 || sig < k.off) {
    k.off += a * (sig - k.off);
  }
  // Noise of the signal inside a state: mean |frame-to-frame change| (clipped against outliers);
  // a quantile would underestimate it on integer-valued, max-pooled data.
  if (k.same >= 2) {
    float d = std::min(std::fabs(sig - k.last), 4 * k.nq + 1);
    k.nq += 0.05f * (d - k.nq);
    // The dark-state noise alone is what false marks come from (a lit, shaken light varies more).
    if (!on) {
      float d0 = std::min(std::fabs(sig - k.last), 4 * k.nq_off + 1);
      k.nq_off += 0.05f * (d0 - k.nq_off);
    }
  }
  k.prev = k.last;
  k.last = sig;
}

// Is the recent run sequence Morse keying? Sets keyed, score, snr, marks.
void LightTracker::evaluate(Track& k, double t) {
  // The recent sequence, oldest first; a run shorter than kMinRun (a one-frame glitch) is merged
  // with its neighbours (a dip inside a mark is still one mark).
  Run seq[kRuns];
  int ns = 0;
  for (int i = 0; i < k.nruns; ++i) {
    const Run& r = k.runs[(k.rhead - 1 - i + 2 * kRuns) % kRuns];
    if (t - r.end > 12000) break;
    if (r.dur >= kMinRun && ((r.on && r.dur > kMaxMark) || (!r.on && r.dur > kMaxSpace))) break;
    seq[ns++] = r;
  }
  std::reverse(seq, seq + ns);
  float dur[kRuns], mk[kRuns], pk[kRuns];
  bool ison[kRuns];
  int na = 0, nm = 0, glitches = 0, ramps = 0;
  bool merge = false;
  for (int i = 0; i < ns; ++i) {
    const Run& r = seq[i];
    if (r.dur < kMinRun) {
      if (r.on) ++glitches;
      if (na > 0) {
        dur[na - 1] += r.dur;
        merge = true;
      }
      continue;
    }
    if (merge && na > 0 && ison[na - 1] == r.on) {
      dur[na - 1] += r.dur;
      if (r.on) {
        mk[nm - 1] = dur[na - 1];
        pk[nm - 1] = std::max(pk[nm - 1], r.peak);
      }
      merge = false;
      continue;
    }
    merge = false;
    if (na == 0 && !r.on) continue;  // sequences start with a mark
    ramps += !r.sharp;
    dur[na] = r.dur;
    ison[na] = r.on;
    ++na;
    if (r.on) {
      mk[nm] = r.dur;
      pk[nm] = r.peak;
      ++nm;
    }
  }
  k.marks = nm;
  k.snr = amp(k) / std::max(0.886f * k.nq_off, 0.5f);
  // Two kinds of failure: too little evidence yet (few marks, weak, all elements alike) says
  // nothing; evidence against keying (a ramp, a beacon, sparkles, noise on a steady light) counts.
  bool enough = nm >= 4 && glitches * 4 <= nm;
  bool contra = false, weak = !enough;
  if (enough) {
    const float fr = static_cast<float>(frame_ms_);
    contra = ramps * 5 > na;
    float s[kRuns];
    std::copy(dur, dur + na, s);
    std::sort(s, s + na);
    float u = s[(na * 3) / 10];
    int nlong = 0, nshort = 0;
    for (int i = 0; i < na; ++i) {
      nlong += dur[i] >= 2.2f * u;
      nshort += dur[i] <= 1.6f * u;
    }
    contra = contra || u < 30 || u > 700 || u < 1.3 * frame_ms_ || nlong * 10 > na * 7;
    weak = weak || nlong < 2 || nshort < 2;
    // Consistent mark brightness (the same lamp): sporadic sparkles and passing objects vary.
    float p[kRuns];
    std::copy(pk, pk + nm, p);
    std::nth_element(p, p + nm / 2, p + nm);
    float med = p[nm / 2];
    int consistent = 0;
    // ... and every mark really reaches the lit level (faint marks were noise, not this light)
    const float a = std::max(amp(k), 1.0f);
    for (int i = 0; i < nm; ++i)
      consistent += pk[i] >= 0.6f * med && pk[i] <= 1.6f * med && pk[i] - k.off >= 0.55f * a && pk[i] - k.off <= 1.8f * a;
    contra = contra || consistent * 4 < nm * 3;
    // Marks are dots and dashes: at most ~6:1 apart, and (at the supported speeds) a typical mark
    // spans at least 1.5 frames; one-frame sparkles and lit objects drifting past are neither.
    float ms[kRuns];
    std::copy(mk, mk + nm, ms);
    std::sort(ms, ms + nm);
    // (one truncated mark — the light crossing a cell edge mid-dot — is tolerated)
    bool ratio_bad = ms[nm - 1] > 6.5f * ms[nm >= 5 ? 1 : 0] || ms[nm / 2] < 1.5 * frame_ms_;
    if (nm >= 5) contra = contra || ratio_bad;  // with four marks one odd one may still be a cut dot
    else weak = weak || ratio_bad;
    // Inside a keyed sequence some spaces are short (element gaps of one dot, or three between
    // single-element letters); a beacon flashing once a second has only long ones.
    float min_space = 1e30f, max_space = 0;
    for (int i = 0; i < na; ++i)
      if (!ison[i]) {
        min_space = std::min(min_space, dur[i]);
        max_space = std::max(max_space, dur[i]);
      }
    contra = contra || min_space > 3.5f * ms[0] + fr;
    // Evidence of Morse structure: a dash among the marks, or element gaps and longer gaps.
    bool dash = ms[nm - 1] >= 2.0f * ms[0] + fr;
    int element_gaps = 0;
    for (int i = 0; i < na; ++i) element_gaps += !ison[i] && dur[i] <= 1.7f * ms[0] + fr;
    bool gaps = element_gaps >= 2 && max_space >= 2.3f * min_space + fr;
    weak = weak || !(dash || gaps);
    // A periodic beacon (standby LED, aliased flicker): nearly every recent mark+space cycle and
    // mark alike, within the camera's quantisation (a 100 ms blink measures 1-2 frames off).
    float cyc[kRuns];
    int nc = 0;
    for (int i = std::max(0, na - 16); i + 1 < na; ++i)
      if (!ison[i] && ison[i + 1]) cyc[nc++] = dur[i] + dur[i + 1];
    if (nc >= 3) {
      float c[kRuns];
      std::copy(cyc, cyc + nc, c);
      std::nth_element(c, c + nc / 2, c + nc);
      float cm = c[nc / 2];
      float mm = ms[nm / 2];
      int pc = 0, pm = 0;
      for (int i = 0; i < nc; ++i) pc += std::fabs(cyc[i] - cm) <= 0.1f * cm + 1.5f * fr;
      for (int i = 0; i < nm; ++i) pm += std::fabs(mk[i] - mm) <= 0.1f * mm + 2.0f * fr;
        contra = contra || (pc * 100 >= nc * 75 && pm * 100 >= nm * 75);
    }
    // A keyed light really goes dark: its lit level is well above its dark level (relative to the
    // local background), unlike noise riding on a steady light. And it is a point (with bloom),
    // not a large lit object passing by.
    contra = contra || amp(k) < 0.45f * std::max(k.onl, 1.0f) || k.size > kMaxLockSize;
    // Four marks are enough evidence for a clear light, or for slow keying resolved over several
    // frames per dot with tight dot/dash classes; otherwise six.
    bool tight = ms[0] >= 4 * fr;
    if (tight) {
      float split = ms[nm - 1] >= 2 * ms[0] ? std::sqrt(ms[0] * ms[nm - 1]) : 1e30f;
      float lo_min = 1e30f, lo_max = 0, hi_min = 1e30f, hi_max = 0;
      for (int i = 0; i < nm; ++i) {
        if (ms[i] < split) { lo_min = std::min(lo_min, ms[i]); lo_max = std::max(lo_max, ms[i]); }
        else { hi_min = std::min(hi_min, ms[i]); hi_max = std::max(hi_max, ms[i]); }
      }
      tight = lo_max <= 1.3f * lo_min + 0.5f * fr && (hi_max == 0 || hi_max <= 1.3f * hi_min + 0.5f * fr);
    }
    weak = weak || k.snr < kLockSnr || amp(k) < kLockAmp || (nm < 6 && k.snr < kQuickSnr && !tight);
  }
  bool pass = enough && !contra && !weak;
  if (contra) {
    k.contra = std::min(k.contra + 1, 10);
    k.streak = 0;
    k.doubt = std::min(k.doubt + 1, 6);
  } else if (pass) {
    k.contra = 0;
    ++k.streak;
  }
  // A track that has contradicted keying must pass several times in a row to count.
  bool keyed = pass && k.streak >= (k.doubt > 0 ? 3 : 1);
  if (keyed && k.streak >= 3 && k.doubt > 0) --k.doubt;
  if (keyed) {
    float morse = std::min(1.0f, 0.4f + 0.06f * static_cast<float>(nm - 4));
    float clear = clampf((k.snr - kLockSnr) / 20, 0, 1);
    k.score = 0.5f * morse + 0.5f * clear;
    if (!k.keyed) k.keyed_at = t;
    k.keyed = true;
  } else if (contra || t - k.last_mark > 8000) {
    k.keyed = false;
    k.score = 0;
  }
}

float LightTracker::gate(const Track& k, double t) const {
  bool sel = sel_ >= 0 && &tracks_[static_cast<size_t>(sel_)] == &k;
  // The light being read may move fast while dark (shake is magnified by zoom); others less so.
  bool wide = sel && !k.pinned;
  float r = 1.6f + (wide ? 10.0f : 5.0f) * static_cast<float>(std::max(0.0, t - k.seen) / 1000);
  float cap = wide ? 8.0f : 4.0f;
  return std::min(r, cap);
}

// (x1,y1) lies on the ray from the frame centre through (x0,y0), at a zoom-like ratio *r.
bool LightTracker::radial(float x1, float y1, float x0, float y0, float* r) const {
  const float cx = 0.5f * static_cast<float>(w_ - 1), cy = 0.5f * static_cast<float>(h_ - 1);
  float ax = x1 - cx, ay = y1 - cy, bx = x0 - cx, by = y0 - cy;
  float la = std::hypot(ax, ay), lb = std::hypot(bx, by);
  if (lb < 2.0f || la < 2.0f) return false;
  float cosang = (ax * bx + ay * by) / (la * lb);
  float ratio = la / lb;
  if (cosang < 0.97f || ratio < 0.15f || ratio > 7.0f || std::fabs(ratio - 1) < 0.25f) return false;
  *r = ratio;
  return true;
}

// Another track (not one started after `young_after` that never keyed) within 2.5 cells.
bool LightTracker::near_track(int x, int y, int except, double young_after) const {
  for (int i = 0; i < kMaxTracks; ++i) {
    const Track& k = tracks_[static_cast<size_t>(i)];
    if (i == except || !k.used || (!k.keyed && k.created >= young_after)) continue;
    if (std::hypot(static_cast<float>(x) - k.x, static_cast<float>(y) - k.y) < 2.5f) return true;
  }
  return false;
}

double LightTracker::lost_ms() const { return std::max(2000.0, 7.5 * dot_ms_); }
double LightTracker::hold_ms() const { return std::max(1500.0, 9.0 * dot_ms_); }

int LightTracker::new_track(const Blob& b, double t) {
  int slot = -1;
  for (int i = 0; i < kMaxTracks; ++i)
    if (!tracks_[static_cast<size_t>(i)].used) { slot = i; break; }
  if (slot < 0) {
    // Replace the stalest unkeyed track that is not being read.
    double oldest = 1e300;
    for (int i = 0; i < kMaxTracks; ++i) {
      const Track& k = tracks_[static_cast<size_t>(i)];
      if (i == sel_ || k.pinned || k.keyed) continue;
      if (k.seen < oldest && t - k.seen > 300) { oldest = k.seen; slot = i; }
    }
    // Else the steady light that has been on longest (lights that never blink are only motion
    // anchors; a few are enough).
    double longest = 2000;
    for (int i = 0; slot < 0 && i < kMaxTracks; ++i) {
      const Track& k = tracks_[static_cast<size_t>(i)];
      if (i == sel_ || k.pinned || k.keyed || !k.on) continue;
      if (t - k.edge > longest) { longest = t - k.edge; slot = i; }
    }
    if (slot < 0) return -1;
  }
  Track& k = tracks_[static_cast<size_t>(slot)];
  k.used = true;
  k.on = k.pinned = k.keyed = false;
  k.id = next_id_++;
  k.x = static_cast<float>(b.x);
  k.y = static_cast<float>(b.y);
  k.size = 0.5f;
  k.nq = std::max(nq_[static_cast<size_t>(b.y * w_ + b.x)] / kQ * 1.13f, 0.5f);
  k.nq_off = k.nq;
  k.created = k.seen = t;
  k.last_mark = k.keyed_at = 0;
  k.score = k.snr = 0;
  k.marks = k.same = k.total_marks = k.contra = k.streak = k.doubt = 0;
  k.nruns = k.rhead = k.hcount = k.hhead = 0;
  float s1 = nprev_ >= 1 ? signal_at(p1_.data(), k) : 0;
  float s2 = nprev_ >= 2 ? signal_at(p2_.data(), k) : s1;
  float sig = signal_at(cur_.data(), k);
  k.off = std::min(s1, s2);
  k.onl = std::max(sig, k.off);
  // The frames just before the light came on start its history (the decoder needs the "off").
  if (nprev_ >= 2) push_hist(k, tp2_, s2, false);
  if (nprev_ >= 1) push_hist(k, tp1_, s1, false);
  float mid = k.off + 0.5f * (k.onl - k.off);
  double f = sig != s1 ? std::min(std::max(static_cast<double>((mid - s1) / (sig - s1)), 0.0), 1.0) : 0.5;
  k.edge = nprev_ >= 1 ? tp1_ + f * (t - tp1_) : t;
  k.on = true;
  k.run_peak = k.last = sig;
  k.prev = s1;
  k.sharp = true;
  push_hist(k, t, sig, true);
  return slot;
}

void LightTracker::pin(double fx, double fy) {
  if (!std::isfinite(fx) || !std::isfinite(fy)) return;
  pin_pending_ = true;
  pin_x_ = std::min(std::max(fx, 0.0), 1.0);
  pin_y_ = std::min(std::max(fy, 0.0), 1.0);
  if (w_ > 0) apply_pin();
}

void LightTracker::unpin() {
  pin_pending_ = false;
  for (Track& k : tracks_) k.pinned = false;
}

// A tap: the nearest track within 4 cells (a keyed one preferred) is pinned. With none there yet,
// the pin waits for a light to appear at that spot (automatic selection is paused meanwhile).
void LightTracker::apply_pin() {
  float px = static_cast<float>(pin_x_ * w_ - 0.5), py = static_cast<float>(pin_y_ * h_ - 0.5);
  px = clampf(px, 0, static_cast<float>(w_ - 1));
  py = clampf(py, 0, static_cast<float>(h_ - 1));
  int best = -1;
  float bd = 1e30f;
  for (int i = 0; i < kMaxTracks; ++i) {
    const Track& k = tracks_[static_cast<size_t>(i)];
    if (!k.used) continue;
    float d = std::hypot(k.x - px, k.y - py) - (k.keyed ? 1.5f : 0.0f);
    if (d < bd) { bd = d; best = i; }
  }
  for (Track& k : tracks_) k.pinned = false;
  if (best < 0 || bd > 4) {
    sel_ = -1;
    pin_pending_ = true;
    return;
  }
  pin_pending_ = false;
  tracks_[static_cast<size_t>(best)].pinned = true;
  sel_ = best;
}

void LightTracker::choose(double t) {
  if (pin_pending_ && w_ > 0) apply_pin();
  if (pin_pending_) return;
  if (sel_ >= 0 && !tracks_[static_cast<size_t>(sel_)].used) sel_ = -1;
  if (sel_ >= 0 && tracks_[static_cast<size_t>(sel_)].pinned) return;
  double lost = lost_ms();
  int best = -1;
  for (int i = 0; i < kMaxTracks; ++i) {
    const Track& k = tracks_[static_cast<size_t>(i)];
    if (!k.used || !k.keyed || i == sel_ || t - k.seen > lost) continue;
    if (best < 0 || k.score > tracks_[static_cast<size_t>(best)].score) best = i;
  }
  // It is not keying after all (twice evidence against it).
  if (sel_ >= 0 && tracks_[static_cast<size_t>(sel_)].contra >= 2) sel_ = -1;
  if (sel_ >= 0 && best >= 0) {
    // The same light seen elsewhere (zoom step, a jerk while it was dark, back into the frame
    // somewhere else): the light being read went dark, and a keyed light of similar brightness
    // was first seen only after that. Hand over at once (its history is replayed).
    const Track& s = tracks_[static_cast<size_t>(sel_)];
    int cont = -1;
    for (int i = 0; i < kMaxTracks; ++i) {
      const Track& k = tracks_[static_cast<size_t>(i)];
      if (!k.used || !k.keyed || i == sel_ || t - k.seen > lost) continue;
      // (lit without a break for longer than any mark: it latched onto a steady light)
      bool stuck = s.on && t - s.edge > kMaxMark;
      if ((s.on && !stuck) || k.created < (stuck ? s.edge : s.seen) - 1.5 * frame_ms_) continue;
      float r = amp(k) / std::max(amp(s), 1.0f);
      if (r < 0.4f || r > 2.5f) continue;
      if (cont < 0 || k.score > tracks_[static_cast<size_t>(cont)].score) cont = i;
    }
    if (cont >= 0) {
      sel_ = cont;
      return;
    }
  }
  if (sel_ >= 0) {
    const Track& s = tracks_[static_cast<size_t>(sel_)];
    bool stuck = s.on && t - s.edge > kMaxMark;  // a steady light now, not keying
    if (t - s.seen <= lost && !stuck) return;  // still being read: never switch away from a live target
    if (best >= 0) {
      sel_ = best;  // the old one is really gone and another keyed light is here
      return;
    }
    if (t - s.seen > kSelectedDropMs) {
      tracks_[static_cast<size_t>(sel_)].used = false;
      sel_ = -1;
    }
    return;
  }
  if (best < 0) {
    wait_since_ = -1;
    return;
  }
  // Prefer the clearer light: if another one is blinking more clearly but has not shown enough
  // marks yet, wait for it a little (its history is replayed, so nothing is lost by waiting).
  const Track& b = tracks_[static_cast<size_t>(best)];
  for (int i = 0; i < kMaxTracks; ++i) {
    const Track& k = tracks_[static_cast<size_t>(i)];
    if (!k.used || i == best || k.keyed || k.total_marks < 2 || t - k.last_mark > 2000) continue;
    if (k.snr > 1.3f * b.snr) {
      if (wait_since_ < 0) wait_since_ = t;
      if (t - wait_since_ < 3000) return;
    }
  }
  wait_since_ = -1;
  sel_ = best;
}

// The selected light's samples the decoder has not seen yet (normally one; a replay after a lock,
// a reacquisition or a hand-over).
void LightTracker::feed(double t) {
  if (sel_ < 0) return;
  const Track& s = tracks_[static_cast<size_t>(sel_)];
  if (!(s.on || t - s.seen <= hold_ms() || s.pinned)) return;
  // A pinned light is read without the Morse check, but only with real contrast (no noise).
  if (s.pinned && !s.keyed && (s.snr < kLockSnr || amp(s) < kLockAmp || s.total_marks < 1)) return;
  // Soft limiter centred on the track's own hysteresis threshold (40 % of its contrast while lit,
  // 55 % while dark): steady levels for the decoder, whose contrast squelch and hysteresis would
  // otherwise trip on the max-pooled noise inside a mark of a weak light; partly exposed frames at
  // the edges stay in between, so the decoder still interpolates the crossing times.
  float a = std::max(amp(s), std::max(kMinStep, 4 * sigma(s)));
  double slope = 0.25 * a;
  if (fed_t_ < -1e299 && s.hcount > 0) {
    // First lock: the light was dark before it was first seen. A second of dark samples at the
    // measured frame interval lets the decoder learn that interval (it bridges dropped frames
    // with it) before the first mark, as if it had been watching all along.
    const Hist& h0 = s.hist[(s.hhead - s.hcount + 2 * kHist) % kHist];
    double thr = s.off + 0.55 * a;
    double level = 0.1 + 0.8 / (1 + std::exp(-std::min(std::max((h0.v - thr) / slope, -30.0), 30.0)));
    int n = static_cast<int>(std::min(60.0, 1000.0 / frame_ms_));
    for (int i = n; i >= 1; --i) out_.push_back(LightSample{h0.t - i * frame_ms_, level});
  }
  for (int i = 0; i < s.hcount; ++i) {
    const Hist& h = s.hist[(s.hhead - s.hcount + i + 2 * kHist) % kHist];
    if (h.t <= fed_t_) continue;
    double thr = s.off + (h.on ? 0.40 : 0.55) * a;
    double z = std::min(std::max((h.v - thr) / slope, -30.0), 30.0);
    double level = 0.1 + 0.8 / (1 + std::exp(-z));
    out_.push_back(LightSample{h.t, level});
    fed_t_ = h.t;
  }
}

void LightTracker::frame(const GridFrame& fr, double dot_ms) {
  out_.clear();
  if (fr.w < 1 || fr.h < 1 || fr.w > kMaxGridCells || fr.h > kMaxGridCells || fr.w * fr.h > kMaxGridCells ||
      !fr.max || !std::isfinite(fr.t_ms))
    return;
  ensure_buffers();
  if (std::isfinite(dot_ms) && dot_ms > 0) dot_ms_ = std::min(std::max(dot_ms, 20.0), 600.0);
  // Time axis: monotonic from 0, whatever the caller's origin.
  bool cut = false;
  double t;
  if (!have_t_) {
    have_t_ = true;
    offset_ = -fr.t_ms;
    t = 0;
    cut = true;
  } else {
    t = fr.t_ms + offset_;
    double dt = t - t_;
    if (!(dt > 0)) {
      if (!(dt < -5000)) return;  // duplicate or reordered frame: dropped
      offset_ += t_ - t + frame_ms_;  // the camera restarted with a new clock
      t = t_ + frame_ms_;
      cut = true;
    } else if (dt > 30000) {
      offset_ -= dt - 30000;
      t = t_ + 30000;
      cut = true;
    } else if (dt > 1500) {
      cut = true;
    }
    if (!cut) frame_ms_ += 0.05 * (std::min(std::max(dt, 5.0), 200.0) - frame_ms_);
  }
  if (fr.w != w_ || fr.h != h_) resize(fr.w, fr.h);
  const size_t n = static_cast<size_t>(n_);
  const double dt = frames_ ? t - t_ : frame_ms_;

  // 1. Global gain (auto exposure, whole-scene changes): median ratio to each cell's slow average.
  const bool use_mean = fr.mean != nullptr;
  const uint8_t* g = use_mean ? fr.mean : fr.max;
  if (nprev_ == 0 || use_mean != ref_mean_) {
    for (size_t i = 0; i < n; ++i) ref_[i] = g[i];
    ref_mean_ = use_mean;
  }
  size_t cnt = 0;
  for (size_t i = 0; i < n; ++i)
    if (ref_[i] >= 6 && g[i] < 250 && fr.max[i] < 255) scratch_[cnt++] = static_cast<float>(g[i]) / ref_[i];
  float gain = 1;
  if (cnt >= 32) {
    std::nth_element(scratch_.begin(), scratch_.begin() + static_cast<ptrdiff_t>(cnt / 2), scratch_.begin() + static_cast<ptrdiff_t>(cnt));
    gain = clampf(scratch_[cnt / 2], 0.25f, 4.0f);
  }
  float ar = static_cast<float>(alpha(dt, 2500));
  for (size_t i = 0; i < n; ++i) {
    ref_[i] += ar * (static_cast<float>(g[i]) - ref_[i]);
    cur_[i] = static_cast<float>(fr.max[i]) / gain;
  }

  // 2. Per-cell noise and motion-tolerant on/off events.
  dilate(cur_.data(), dc_.data());
  non_ = noff_ = 0;
  bool blobs = nprev_ >= 1 && !cut;
  if (blobs) {
    for (size_t i = 0; i < n; ++i) quantile_step(&nq_[i], std::fabs(cur_[i] - p1_[i]), 0.3f, 0.05f);
    const float* ref2 = nprev_ >= 2 ? d2_.data() : d1_.data();
    int above = 0;
    for (size_t i = 0; i < n; ++i) {
      e_[i] = cur_[i] - std::min(d1_[i], ref2[i]);
      f_[i] = p1_[i] - dc_[i];
    }
    // Local maxima above each cell's threshold; the strongest kMaxBlobs are kept.
    for (int pass = 0; pass < 2; ++pass) {
      const float* m = pass == 0 ? e_.data() : f_.data();
      Blob* out = pass == 0 ? on_ : off_;
      int& nb = pass == 0 ? non_ : noff_;
      for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x) {
          size_t i = static_cast<size_t>(y) * static_cast<size_t>(w_) + static_cast<size_t>(x);
          float v = m[i];
          if (v < std::max(kMinStep, kOnNoiseK * nq_[i])) continue;
          if (pass == 0) ++above;
          bool peak = true;
          for (int yy = std::max(0, y - 1); yy <= std::min(h_ - 1, y + 1) && peak; ++yy)
            for (int xx = std::max(0, x - 1); xx <= std::min(w_ - 1, x + 1); ++xx) {
              size_t j = static_cast<size_t>(yy) * static_cast<size_t>(w_) + static_cast<size_t>(xx);
              if (m[j] > v || (m[j] == v && j < i)) { peak = false; break; }
            }
          if (!peak) continue;
          // Fresh: a light switching on (off) makes most of its brightness new (gone); noise riding
          // on a steady light changes only a little of it.
          if (v < 0.6f * signal_at(pass == 0 ? cur_.data() : p1_.data(), static_cast<float>(x), static_cast<float>(y), 0.5f))
            continue;
          Blob b{x, y, v, pass == 0 ? cur_[i] : p1_[i], false, false};
          if (nb < kMaxBlobs) {
            out[nb++] = b;
          } else {
            int wi = 0;
            for (int k = 1; k < nb; ++k)
              if (out[k].e < out[wi].e) wi = k;
            if (out[wi].e < v) out[wi] = b;
          }
        }
    }
    // Lights switching on all over the frame at once: the scene changed (a lamp, a cut).
    if (above > std::max(16, n_ / 10)) non_ = noff_ = 0;
  }

  // 3. A camera jump: on/off event pairs with a common displacement move every track.
  if (non_ >= 2 && noff_ >= 2) {
    float pdx[kMaxBlobs], pdy[kMaxBlobs];
    int pa[kMaxBlobs], np = 0;
    for (int a = 0; a < non_; ++a) {
      int bi = -1;
      float bd = 13;
      for (int b = 0; b < noff_; ++b) {
        float dx = static_cast<float>(on_[a].x - off_[b].x), dy = static_cast<float>(on_[a].y - off_[b].y);
        float d = std::hypot(dx, dy);
        float ratio = on_[a].v / std::max(off_[b].v, 1.0f);
        if (d > 2 && d < bd && ratio > 0.6f && ratio < 1.67f) { bd = d; bi = b; }
      }
      if (bi < 0) continue;
      pdx[np] = static_cast<float>(on_[a].x - off_[bi].x);
      pdy[np] = static_cast<float>(on_[a].y - off_[bi].y);
      pa[np++] = a;
    }
    int best = -1, support = 0;
    for (int i = 0; i < np; ++i) {
      int s = 0;
      for (int j = 0; j < np; ++j) s += std::fabs(pdx[i] - pdx[j]) <= 1.5f && std::fabs(pdy[i] - pdy[j]) <= 1.5f;
      if (s > support) { support = s; best = i; }
    }
    if (support >= 2) {
      float mx = 0, my = 0;
      for (int j = 0; j < np; ++j)
        if (std::fabs(pdx[best] - pdx[j]) <= 1.5f && std::fabs(pdy[best] - pdy[j]) <= 1.5f) {
          mx += pdx[j];
          my += pdy[j];
          on_[pa[j]].explained = true;
        }
      mx /= static_cast<float>(support);
      my /= static_cast<float>(support);
      for (Track& k : tracks_) {
        if (!k.used) continue;
        k.x = clampf(k.x + mx, 0, static_cast<float>(w_ - 1));
        k.y = clampf(k.y + my, 0, static_cast<float>(h_ - 1));
      }
    } else {
      // A zoom step: lights move along rays from the frame centre by a common ratio.
      const float zcx = 0.5f * static_cast<float>(w_ - 1), zcy = 0.5f * static_cast<float>(h_ - 1);
      float zr[kMaxBlobs * 4];
      int za[kMaxBlobs * 4], nz = 0;
      for (int a = 0; a < non_ && nz < kMaxBlobs * 4 - kMaxBlobs; ++a)
        for (int b = 0; b < noff_; ++b) {
          float r = 0;
          if (!radial(static_cast<float>(on_[a].x), static_cast<float>(on_[a].y), static_cast<float>(off_[b].x),
                      static_cast<float>(off_[b].y), &r))
            continue;
          float ratio = on_[a].v / std::max(off_[b].v, 1.0f);
          if (ratio < 0.5f || ratio > 2.0f) continue;
          zr[nz] = r;
          za[nz++] = a;
        }
      int bz = -1, zs = 0;
      for (int i = 0; i < nz; ++i) {
        int sup = 0;
        for (int j = 0; j < nz; ++j) sup += std::fabs(zr[j] / zr[i] - 1) <= 0.08f && za[j] != za[i];
        if (sup + 1 > zs) { zs = sup + 1; bz = i; }
      }
      if (zs >= 2) {
        float z = zr[bz];
        for (int j = 0; j < nz; ++j)
          if (std::fabs(zr[j] / z - 1) <= 0.08f) on_[za[j]].explained = true;
        for (Track& k : tracks_) {
          if (!k.used) continue;
          k.x = clampf(zcx + (k.x - zcx) * z, 0, static_cast<float>(w_ - 1));
          k.y = clampf(zcy + (k.y - zcy) * z, 0, static_cast<float>(h_ - 1));
        }
      }
    }
  }

  // 4. Tracks: the selected one first, then by score.
  int order[kMaxTracks], no = 0;
  for (int i = 0; i < kMaxTracks; ++i)
    if (tracks_[static_cast<size_t>(i)].used) order[no++] = i;
  auto rank = [&](int i) {
    const Track& k = tracks_[static_cast<size_t>(i)];
    return (i == sel_ ? 1000.0f : 0.0f) + k.score + 0.01f * static_cast<float>(std::min(k.total_marks, 50));
  };
  std::sort(order, order + no, [&](int a, int b) { return rank(a) > rank(b); });
  float adx[kMaxTracks], ady[kMaxTracks];
  int nanchor = 0;
  for (int oi = 0; oi < no; ++oi) {
    Track& k = tracks_[static_cast<size_t>(order[oi])];
    float a = amp(k);
    if (!k.on) {
      float r = gate(k, t);
      int bj = -1;
      float bd = 1e30f;
      bool is_sel = order[oi] == sel_ && !k.pinned;
      for (int j = 0; j < non_; ++j) {
        const Blob& b = on_[j];
        if (b.claimed) continue;
        float d = std::hypot(static_cast<float>(b.x) - k.x, static_cast<float>(b.y) - k.y);
        float zr = 0;
        // The light being read, after a zoom step in a scene with nothing else to measure it by: a
        // fresh light of the same brightness on the ray from the centre through where it was.
        if (d > r && is_sel && a > kMinStep && b.e >= 0.35f * a && b.e <= 2.0f * a &&
            radial(static_cast<float>(b.x), static_cast<float>(b.y), k.x, k.y, &zr) && !near_track(b.x, b.y, order[oi], k.seen)) {
          on_[j].claimed = true;
          // a track started there since (by the first, partly exposed frame) is this light
          for (int q = 0; q < kMaxTracks; ++q) {
            Track& y = tracks_[static_cast<size_t>(q)];
            if (q != order[oi] && y.used && !y.keyed && y.created >= k.seen &&
                std::hypot(static_cast<float>(b.x) - y.x, static_cast<float>(b.y) - y.y) < 2.5f)
              y.used = false;
          }
          k.x = static_cast<float>(b.x);
          k.y = static_cast<float>(b.y);
          bj = -1;
          break;
        }
        if (d > r) continue;
        if (a > kMinStep && b.e < 0.35f * a) continue;
        if (d > 2.5f && near_track(b.x, b.y, order[oi], 1e300)) continue;  // another light's
        if (d > 2.5f && a > kMinStep && (b.e < 0.4f * a || b.e > 2.5f * a)) continue;
        if (d < bd) { bd = d; bj = j; }
      }
      if (bj >= 0) {
        on_[bj].claimed = true;
        if (bd > 0.75f) {
          k.x = static_cast<float>(on_[bj].x);
          k.y = static_cast<float>(on_[bj].y);
        }
      }
    } else {
      for (int j = 0; j < non_; ++j)
        if (std::hypot(static_cast<float>(on_[j].x) - k.x, static_cast<float>(on_[j].y) - k.y) <= 2.0f)
          on_[j].claimed = true;
    }
    float sig = signal_at(cur_.data(), k);
    if (k.on && sig < k.off + 0.4f * std::max(a, kMinStep) && a > kMinStep) {
      // Gone dark while lit: a jump of the camera moves it (off event here, on event there).
      for (int j = 0; j < noff_; ++j) {
        if (std::hypot(static_cast<float>(off_[j].x) - k.x, static_cast<float>(off_[j].y) - k.y) > 2.0f) continue;
        int bc = -1;
        float bd = 13;
        for (int c = 0; c < non_; ++c) {
          const Blob& b = on_[c];
          if (b.claimed) continue;
          float d = std::hypot(static_cast<float>(b.x - off_[j].x), static_cast<float>(b.y - off_[j].y));
          if (d > 2 && d < bd && b.e > 0.5f * a && b.e < 2.0f * a) { bd = d; bc = c; }
        }
        if (bc >= 0) {
          on_[bc].claimed = true;
          k.x = static_cast<float>(on_[bc].x);
          k.y = static_cast<float>(on_[bc].y);
          sig = signal_at(cur_.data(), k);
          break;
        }
      }
    }
    key(k, sig, t);
    if (k.on) {
      float ox = k.x, oy = k.y;
      refine(k);
      // A steady light (lit for a while, not keyed) moves only with the camera.
      if (!k.keyed && t - k.edge > 1000 && nanchor < kMaxTracks) {
        adx[nanchor] = k.x - ox;
        ady[nanchor++] = k.y - oy;
      }
    }
    push_hist(k, t, sig, k.on);
  }
  // Hand shake moves dark tracks too: by the median motion of the steady lights.
  // At least two steady lights must agree (a passing car is lit for long too, and moves alone).
  if (nanchor >= 2) {
    float sx[kMaxTracks], sy[kMaxTracks];
    std::copy(adx, adx + nanchor, sx);
    std::copy(ady, ady + nanchor, sy);
    std::sort(sx, sx + nanchor);
    std::sort(sy, sy + nanchor);
    float mx = sx[nanchor / 2], my = sy[nanchor / 2];
    int agree = 0;
    for (int i = 0; i < nanchor; ++i) agree += std::fabs(adx[i] - mx) <= 0.3f && std::fabs(ady[i] - my) <= 0.3f;
    if (agree * 2 <= nanchor || agree < 2) mx = my = 0;
    mx = clampf(mx, -1, 1);
    my = clampf(my, -1, 1);
    for (Track& k : tracks_)
      if (k.used && !k.on) {
        k.x = clampf(k.x + mx, 0, static_cast<float>(w_ - 1));
        k.y = clampf(k.y + my, 0, static_cast<float>(h_ - 1));
      }
  }

  // 5. Forget what is not (or no longer) a keyed light; merge duplicates; start new tracks.
  for (int i = 0; i < kMaxTracks; ++i) {
    Track& k = tracks_[static_cast<size_t>(i)];
    if (!k.used || k.pinned || i == sel_) continue;
    double unseen = t - k.seen;
    if (k.keyed && t - k.last_mark > 8000) k.keyed = false;
    if ((!k.keyed && unseen > 4000) || unseen > 8000) k.used = false;
  }
  for (int i = 0; i < kMaxTracks; ++i) {
    Track& a = tracks_[static_cast<size_t>(i)];
    if (!a.used) continue;
    for (int j = i + 1; j < kMaxTracks; ++j) {
      Track& b = tracks_[static_cast<size_t>(j)];
      if (!b.used || std::hypot(a.x - b.x, a.y - b.y) > 1.2f) continue;
      bool a_better = (a.keyed && !b.keyed) || (a.keyed == b.keyed && a.total_marks >= b.total_marks);
      bool keep_a = i == sel_ || a.pinned || (!(j == sel_ || b.pinned) && a_better);
      (keep_a ? b : a).used = false;
      if (!a.used) break;
    }
  }
  if (sel_ >= 0 && !tracks_[static_cast<size_t>(sel_)].used) sel_ = -1;
  int created = 0;
  for (int j = 0; j < non_ && created < 4; ++j) {
    const Blob& b = on_[j];
    if (b.claimed || b.explained) continue;
    bool near = false;
    for (const Track& k : tracks_)
      if (k.used && std::hypot(static_cast<float>(b.x) - k.x, static_cast<float>(b.y) - k.y) < 2.5f) { near = true; break; }
    if (near) continue;
    if (new_track(b, t) >= 0) ++created;
  }

  // 6. Choose the light to read and hand its new samples to the decoder.
  t_ = t;
  choose(t);
  feed(t);

  // 7. Keep the previous two frames and their dilations (buffers swapped, never reallocated).
  p2_.swap(p1_);
  p1_.swap(cur_);
  d2_.swap(d1_);
  d1_.swap(dc_);
  tp2_ = tp1_;
  tp1_ = t;
  if (nprev_ < 2) ++nprev_;
  ++frames_;
}

Target LightTracker::target() const {
  Target r;
  r.pinned = pin_pending_;
  if (frames_ == 0) return r;
  double lost = lost_ms();
  for (int i = 0; i < kMaxTracks && i < static_cast<int>(tracks_.size()); ++i) {
    const Track& k = tracks_[static_cast<size_t>(i)];
    if (k.used && k.keyed && t_ - k.seen <= lost) ++r.candidates;
  }
  if (sel_ < 0) return r;
  const Track& s = tracks_[static_cast<size_t>(sel_)];
  double unseen = t_ - s.seen;
  r.pinned = s.pinned;
  r.state = unseen > lost ? Target::kLost : Target::kLocked;
  r.x = std::min(std::max((s.x + 0.5) / w_, 0.0), 1.0);
  r.y = std::min(std::max((s.y + 0.5) / h_, 0.0), 1.0);
  r.size = std::min(1.0, std::max(3.0, 4.0 * s.size + 1.0) / w_);
  double c = s.keyed ? s.score : 0.3 * std::min(1.0, s.snr / 15.0);
  if (r.state == Target::kLost) c *= std::max(0.0, 1.0 - (unseen - lost) / 5000.0);
  r.confidence = std::round(std::min(std::max(c, 0.0), 1.0) * 100) / 100;
  return r;
}

bool LightTracker::take_event(Target* out) {
  Target c = target();
  const Target& p = ev_last_;
  bool state = c.state != p.state || c.pinned != p.pinned;
  bool other = c.candidates != p.candidates ||
               (c.state != Target::kSearching &&
                (std::fabs(c.x - p.x) > 0.01 || std::fabs(c.y - p.y) > 0.01 || std::fabs(c.size - p.size) > 0.01 ||
                 std::fabs(c.confidence - p.confidence) >= 0.1));
  if (!state && !(other && t_ - ev_t_ >= 200)) return false;
  ev_last_ = c;
  ev_t_ = t_;
  if (out) *out = c;
  return true;
}

}  // namespace morse_rx
}  // namespace ember
