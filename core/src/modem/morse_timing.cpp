#include "modem/morse_timing.h"

#include <algorithm>
#include <cmath>

#include "modem/morse.h"

namespace ember {
namespace modem {

namespace {

constexpr size_t kMarkHistory = 8;
constexpr size_t kGapHistory = 12;
constexpr size_t kFitHistory = 16;
constexpr size_t kMaxPattern = 12;
constexpr size_t kMaxOut = 64 * 1024;  // undrained output cap (callers drain every feed)
const char* const kUndecodable = "\xE2\x96\xA1";  // U+25A1 WHITE SQUARE

void remember(std::vector<double>& v, double x, size_t cap) {
  v.push_back(x);
  if (v.size() > cap) v.erase(v.begin());
}

// Two-class split of positive durations on a log scale (Otsu). False if the classes are not
// separated by at least min_ratio; lo/hi are the class geometric means.
bool split2(const std::vector<double>& v, double min_ratio, double* lo, double* hi) {
  if (v.size() < 3) return false;
  std::vector<double> l;
  l.reserve(v.size());
  for (double x : v) l.push_back(std::log(x > 1e-6 ? x : 1e-6));
  std::sort(l.begin(), l.end());
  double best = -1, blo = 0, bhi = 0, total = 0;
  for (double x : l) total += x;
  double acc = 0;
  for (size_t k = 1; k < l.size(); ++k) {
    acc += l[k - 1];
    double n0 = static_cast<double>(k), n1 = static_cast<double>(l.size() - k);
    double m0 = acc / n0, m1 = (total - acc) / n1;
    double bv = n0 * n1 * (m1 - m0) * (m1 - m0);
    if (bv > best) { best = bv; blo = m0; bhi = m1; }
  }
  if (bhi - blo < std::log(min_ratio)) return false;
  *lo = std::exp(blo);
  *hi = std::exp(bhi);
  return true;
}

double geo_mean(const std::vector<double>& v) {
  double m = 0;
  for (double x : v) m += std::log(x > 1e-6 ? x : 1e-6);
  return std::exp(m / static_cast<double>(v.size()));
}

}  // namespace

void MorseTiming::configure(const Config& c) {
  cfg_ = c;
  if (!(cfg_.min_wpm > 0)) cfg_.min_wpm = 3;
  if (!(cfg_.max_wpm >= cfg_.min_wpm)) cfg_.max_wpm = 60;
  if (!(cfg_.min_mark_ms >= 0) || cfg_.min_mark_ms > 1000) cfg_.min_mark_ms = 0;
  if (!(cfg_.wpm_hint >= 0) || cfg_.wpm_hint > 200) cfg_.wpm_hint = 0;
  reset();
}

void MorseTiming::reset() {
  on_ = false;
  have_time_ = false;
  now_ = mark_start_ = off_since_ = 0;
  mark_open_ = false;
  space_start_ = -1;
  double w = cfg_.wpm_hint > 0 ? cfg_.wpm_hint : 15;
  if (w < cfg_.min_wpm) w = cfg_.min_wpm;
  if (w > cfg_.max_wpm) w = cfg_.max_wpm;
  dit_ = 1200.0 / w;
  dah_ = 3 * dit_;
  marks_.clear();
  gaps_.clear();
  spaces_.clear();
  fit_.clear();
  char_marks_.clear();
  char_done_ = word_done_ = true;
  out_.clear();
  chars_ = 0;
}

void MorseTiming::clamp_speed() {
  if (dah_ > 6.0 * dit_) dit_ += 0.3 * (dah_ / 3 - dit_);
  if (dah_ < 1.6 * dit_) dah_ += 0.3 * (3 * dit_ - dah_);
  double lo = 1200.0 / cfg_.max_wpm, hi = 1200.0 / cfg_.min_wpm;
  dit_ = std::min(std::max(dit_, lo), hi);
  dah_ = std::min(std::max(dah_, 1.6 * lo), 6 * hi);
}

void MorseTiming::commit_mark(double d) {
  if (d > 12 * dah_ && d > 2000) {  // a carrier, a lamp left on: not an element
    char_marks_.clear();
    return;
  }
  remember(marks_, d, kMarkHistory);
  double lo, hi;
  if (split2(marks_, 1.6, &lo, &hi)) {
    dit_ += 0.5 * (lo - dit_);
    dah_ += 0.5 * (hi - dah_);
  } else if (marks_.size() == 2 && std::max(marks_[0], marks_[1]) >= 2 * std::min(marks_[0], marks_[1])) {
    // Two elements of clearly different length: one of each class.
    dit_ = std::min(marks_[0], marks_[1]);
    dah_ = std::max(marks_[0], marks_[1]);
  } else if (marks_.size() >= 2 && !char_spaces_.empty() && unit_from_spaces() > 0 &&
             *std::max_element(marks_.begin(), marks_.end()) < 1.8 * *std::min_element(marks_.begin(), marks_.end())) {
    // One class of marks so far ("0" = -----, "5" = .....): element gaps are one unit, so the
    // marks are dashes if they are clearly longer than the gaps between them.
    double mean = geo_mean(marks_), unit = unit_from_spaces();
    if (mean > 2.0 * unit) {
      dah_ += 0.7 * (mean - dah_);
      dit_ += 0.7 * (unit - dit_);
    } else {
      dit_ += 0.7 * (mean - dit_);
      dah_ += 0.7 * (3 * mean - dah_);
    }
  } else if (marks_.size() >= 3) {
    // One class only so far ("EEEE", "MOM"): it is the class nearer its estimate (log scale).
    double mean = geo_mean(marks_);
    if (std::fabs(std::log(mean / dit_)) < std::fabs(std::log(mean / dah_))) dit_ += 0.5 * (mean - dit_);
    else dah_ += 0.5 * (mean - dah_);
  } else if (marks_.size() == 1 && cfg_.wpm_hint <= 0) {
    // First element after a pause, speed unknown: take it as a dot. If it was a dash the next
    // element corrects it; guessing a too-long unit merges characters rather than splitting
    // every element into its own character.
    dit_ = d;
    dah_ = 3 * d;
  }
  clamp_speed();
  bool dash = d >= std::sqrt(dit_ * dah_);
  remember(fit_, std::fabs(std::log(d / (dash ? dah_ : dit_))), kFitHistory);
  if (char_marks_.size() <= kMaxPattern) {
    if (!char_marks_.empty()) char_spaces_.push_back(last_space_);
    char_marks_.push_back(d);
  }
  char_done_ = word_done_ = false;
}

void MorseTiming::commit_space(double s) {
  last_space_ = s;
  double u = dot_ms();
  if (s < 40 * u) remember(spaces_, s, 16);
  if (s > char_threshold() && s < 40 * u) remember(gaps_, s, kGapHistory);  // Farnsworth gaps can be ~15 dots
}

// Element gaps (1 unit) vs character gaps (3 units, more with Farnsworth): split the observed
// spaces when both are present; otherwise 2 units. Sloppy senders keep their own proportions.
double MorseTiming::char_threshold() const {
  double unit = std::min(dit_, dot_ms());
  double lo, hi;
  std::vector<double> sp;
  for (double s : spaces_)
    if (s < 12 * unit) sp.push_back(s);
  if (split2(sp, 1.8, &lo, &hi)) return std::min(std::max(std::sqrt(lo * hi), 1.6 * unit), 4.0 * unit);
  return 2.0 * unit;
}

double MorseTiming::word_threshold() const {
  double u = dot_ms();
  double lo, hi;
  if (split2(gaps_, 1.6, &lo, &hi)) return std::max(4.5 * u, std::sqrt(lo * hi));
  if (gaps_.size() >= 2) {
    double mean = geo_mean(gaps_);
    // One class of gaps: 5..10 dots are word gaps (single letters/digits separated by spaces);
    // longer ones are stretched (Farnsworth) character gaps.
    if (mean > 10 * u) return 1.8 * mean;
  }
  return 5 * u;
}

void MorseTiming::emit(const std::string& s) {
  if (out_.size() > kMaxOut) return;
  out_ += s;
}

void MorseTiming::check_gap(double off) {
  double u = dot_ms();
  if (!char_done_ && !char_marks_.empty() && off > char_threshold()) {
    std::string s = decode_marks(0, char_marks_.size(), 0);
    if (s.empty()) s = kUndecodable;
    emit(s);
    ++chars_;
    char_marks_.clear();
    char_spaces_.clear();
    char_done_ = true;
  }
  if (!word_done_ && char_done_ && off > word_threshold()) {
    emit(" ");
    word_done_ = true;
  }
  // A long silence ends the "over": forget the element history (the next sender, or the same one
  // after a pause, may use another speed) but keep the last estimate as the starting point.
  if (off > std::max(cfg_.idle_forget_ms, cfg_.idle_forget_dots * u) && !marks_.empty()) {
    marks_.clear();
    fit_.clear();
    spaces_.clear();
    if (cfg_.wpm_hint > 0) {
      dit_ = 1200.0 / cfg_.wpm_hint;
      dah_ = 3 * dit_;
      clamp_speed();
    }
  }
}

void MorseTiming::advance(double t) {
  if (!std::isfinite(t)) return;
  if (!have_time_) {
    have_time_ = true;
    now_ = t;
  }
  if (t < now_) t = now_;
  now_ = t;
  if (mark_open_ && !on_) {
    // Drop-outs shorter than this inside a mark are bridged. Capped by the mark itself, so the
    // element gaps of a sender who just got faster are not mistaken for drop-outs.
    double bridge = std::min({0.4 * dit_, 0.5 * (off_since_ - mark_start_), cfg_.max_bridge_ms});
    if (t - off_since_ >= bridge) {
      double dur = off_since_ - mark_start_;
      // Glitches (flicker spikes, impulses) are short in absolute terms too: a wrong speed
      // estimate must not make real dots disappear.
      double glitch = std::max(cfg_.min_mark_ms, std::min(0.3 * dit_, cfg_.max_bridge_ms));
      mark_open_ = false;
      if (dur >= glitch) {
        if (space_start_ >= 0) commit_space(mark_start_ - space_start_);
        commit_mark(dur);
        space_start_ = off_since_;
      }
    }
  }
  if (!on_ && !mark_open_ && space_start_ >= 0) check_gap(t - space_start_);
}

void MorseTiming::key(bool on, double t) {
  if (!std::isfinite(t)) return;
  advance(t);
  t = now_;
  if (on == on_) return;
  if (on) {
    if (!mark_open_) mark_start_ = t;  // else: a short drop-out inside the mark, bridged
    mark_open_ = false;
  } else {
    off_since_ = t;
    mark_open_ = true;
  }
  on_ = on;
}

void MorseTiming::take_text(std::string* out) {
  if (out) *out += out_;
  out_.clear();
}

// Decodes marks [a, b) of the current character. If they are not a known character (typical of
// hand keying that rushes a character gap), splits at the longest internal space that is clearly
// longer than an element gap and decodes both halves. Prosign names apply per piece.
std::string MorseTiming::decode_marks(size_t a, size_t b, int depth) const {
  // A space inside the character that is a character gap at the speed known *now*: the marks
  // before it were timed while the speed was still unknown (the first element of an over is taken
  // as a dot, so after a leading dash "T E" looked like one character "N"). Cut there first, as the
  // real-time gap check would have done with the right speed.
  if (depth < 3 && b - a >= 2) {
    double thr = char_threshold();
    size_t cut = b;
    double longest = thr;
    for (size_t i = a; i + 1 < b; ++i)
      if (i < char_spaces_.size() && char_spaces_[i] > longest) { longest = char_spaces_[i]; cut = i; }
    if (cut != b) {
      std::string l = decode_marks(a, cut + 1, depth + 1), r = decode_marks(cut + 1, b, depth + 1);
      if (!l.empty() && !r.empty()) return l + r;
    }
  }
  double split = std::sqrt(dit_ * dah_);
  std::string pat;
  for (size_t i = a; i < b; ++i) pat += char_marks_[i] >= split ? '-' : '.';
  std::string s = pat.size() > kMaxPattern ? std::string() : morse::decode(pat);
  if (!s.empty() && cfg_.prosign_names && s.size() == 1) {
    switch (s[0]) {
      case '+': s = "<AR>"; break;
      case '=': s = "<BT>"; break;
      case '(': s = "<KN>"; break;
      case '&': s = "<AS>"; break;
      default: break;
    }
  }
  if (!s.empty() || b - a < 2 || depth >= 3) return s;
  size_t best = b;
  double longest = 1.6 * std::min(dit_, dot_ms());
  for (size_t i = a; i + 1 < b; ++i)
    if (i < char_spaces_.size() && char_spaces_[i] > longest) { longest = char_spaces_[i]; best = i; }
  if (best == b) return s;
  std::string l = decode_marks(a, best + 1, depth + 1), r = decode_marks(best + 1, b, depth + 1);
  if (l.empty() || r.empty()) return s;
  return l + r;
}

// Unit estimate from the spaces inside the current character (element gaps = 1 unit), or 0.
double MorseTiming::unit_from_spaces() const {
  std::vector<double> sp;
  for (double s : char_spaces_) sp.push_back(s);
  if (sp.empty()) return 0;
  return geo_mean(sp);
}

std::string MorseTiming::classify() const {
  std::string p;
  double split = std::sqrt(dit_ * dah_);
  for (double d : char_marks_) p += d >= split ? '-' : '.';
  return p;
}

std::string MorseTiming::pending() const {
  std::string p = classify();
  if (p.size() > kMaxPattern) p.resize(kMaxPattern);
  double held = on_ ? now_ - mark_start_ : mark_open_ ? off_since_ - mark_start_ : -1;
  if (held >= 0 && (on_ || mark_open_) && held >= 0.3 * dit_) p += held >= std::sqrt(dit_ * dah_) ? '-' : '.';
  return p;
}

double MorseTiming::wpm() const {
  if (marks_.size() < 2 && chars_ == 0) return 0;
  double w = 1200.0 / dot_ms();
  return std::round(w * 10) / 10;
}

double MorseTiming::quality() const {
  if (fit_.size() < 4) return 0;
  double m = 0;
  for (double x : fit_) m += x;
  m /= static_cast<double>(fit_.size());
  double q = 1.0 - m / 0.45;  // |log| deviation 0.45 ~ 1.57x off the class centre
  q = std::min(std::max(q, 0.0), 1.0);
  return q * std::min(1.0, static_cast<double>(fit_.size()) / 8.0);
}

}  // namespace modem
}  // namespace ember
