// EmberMesh core / modem — Morse timing back end shared by every Morse receiver: the keyboard-mode
// CW modem (audio), the Morse receive sessions for camera light and microphone audio (morse/).
//
// Input: key transitions (on/off) with times in milliseconds, plus advance(t) calls while nothing
// changes, so character and word gaps are recognised in real time (a character is emitted as soon
// as its gap is long enough, not when the next one starts).
//
// Robustness for hand keying and camera input:
//   * Element speed is learned from the marks themselves (two-class split on a log scale), so
//     dash:dot from about 1.6:1 to 6:1 decodes; it re-adapts within a few characters when the
//     sender changes speed, and is forgotten after a long idle period.
//   * Very short marks (glitches, flicker) are discarded and very short spaces inside a mark
//     (dropped frames, flicker dips, fading) are bridged; both thresholds scale with the speed.
//   * Character / word gap thresholds come from the observed gaps, so Farnsworth spacing works.
// Output: decoded text (prosigns as <XX>, an undecodable element group as U+25A1 "□"), the
// elements of the character in progress, the speed estimate and a timing-lock quality 0..1.
// Bounded memory; arbitrary (even hostile) time values never cause UB.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ember {
namespace modem {

class MorseTiming {
 public:
  struct Config {
    double wpm_hint = 0;         // starting speed (0 = 15 WPM); learned speed takes over quickly
    double min_wpm = 3, max_wpm = 60;
    double min_mark_ms = 0;      // marks shorter than this are always glitches (e.g. 1 audio tick)
    bool prosign_names = false;  // <AR> <BT> <KN> <AS> instead of + = ( & for the shared patterns
    // After this much key-up (and at least idle_forget_dots dots) the element history is dropped;
    // with a wpm_hint the speed returns to the hint, otherwise the last estimate is kept.
    double idle_forget_ms = 3000, idle_forget_dots = 25;
    // Longest drop-out inside a mark that is bridged (also limited to 0.4 dot and half the mark).
    // A bound independent of the speed estimate, so a wrong estimate cannot merge real elements.
    double max_bridge_ms = 30;
  };

  MorseTiming() { configure(Config()); }
  explicit MorseTiming(const Config& c) { configure(c); }
  void configure(const Config& c);
  // Updates max_bridge_ms without resetting (the light keyer follows the camera frame interval).
  void set_max_bridge_ms(double ms) { if (ms >= 0 && ms <= 1000) cfg_.max_bridge_ms = ms; }
  // Clears text, pending elements and the learned speed.
  void reset();

  // The key changed to `on` at time t (ms). Times that go backwards are treated as "now".
  void key(bool on, double t_ms);
  // Time passes with the key unchanged: recognises gaps in real time.
  void advance(double t_ms);

  // Text decoded since the last call (appends nothing if none).
  void take_text(std::string* out);
  bool has_text() const { return !out_.empty(); }
  // Elements of the character in progress, e.g. "-.-" (includes a mark still being held).
  std::string pending() const;
  bool on() const { return on_; }
  double wpm() const;  // 0 = no estimate yet
  double quality() const;
  uint64_t chars() const { return chars_; }
  double dot_ms() const { return 0.5 * (dit_ + dah_ / 3); }

 private:
  void commit_mark(double dur);
  void commit_space(double dur);
  void check_gap(double off_ms);
  void emit(const std::string& s);
  double word_threshold() const;
  double char_threshold() const;
  double unit_from_spaces() const;
  std::vector<double> spaces_;  // recent spaces between marks (element and character gaps)
  void clamp_speed();

  Config cfg_;
  // key state
  bool on_ = false;
  double now_ = 0;
  bool have_time_ = false;
  double mark_start_ = 0;      // start of the (possibly bridged) mark in progress
  double off_since_ = 0;       // last off transition
  bool mark_open_ = false;     // a mark ended but may still be bridged
  double space_start_ = -1;    // end of the last committed mark (-1 = none yet)
  double last_space_ = 0;      // the space before the most recent committed mark
  // learned timing (ms)
  double dit_ = 80, dah_ = 240;
  std::vector<double> marks_, gaps_, fit_;
  std::vector<double> char_marks_;  // mark durations of the character in progress
  std::vector<double> char_spaces_; // spaces between those marks (size = marks - 1)
  std::string decode_marks(size_t a, size_t b, int depth) const;  // marks [a, b), split if needed
  std::string classify() const;     // its elements with the current speed estimate
  bool char_done_ = true, word_done_ = true;
  // output
  std::string out_;
  uint64_t chars_ = 0;
};

}  // namespace modem
}  // namespace ember
