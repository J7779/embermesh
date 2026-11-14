// EmberMesh core / morse — finds and follows a blinking light anywhere in the camera frame
// (docs/RPC_API.md §12 "Morse camera reader"), so nobody has to drag a box onto it.
//
// Input: one small brightness grid per camera frame (w×h cells, w·h ≤ 4096), each cell the
// MAXIMUM luma of the pixels it covers (a one-pixel light still decides its cell), optionally
// the cell means too. Output: the brightness series of the tracked light for the existing light
// decoder (LightKeyer), and the target (state, position, size, confidence, candidates).
//
// How it decides (details in core/src/morse/README.md):
//   1. Global gain: the median ratio of every cell to its own slow average removes auto-exposure
//      pumping and whole-scene changes.
//   2. Motion-tolerant change detection: a cell "switches on" only if it is brighter than the
//      5×5 neighbourhood maximum of each of the two previous frames (so steady lights moving up
//      to ~1 cell per frame, i.e. hand shake, do not count), by more than a multiple of that
//      cell's own frame-to-frame noise (a robust 30th-percentile tracker, which absorbs the
//      upward bias of max-pooled sensor noise). "Switch-off" is the mirror image. Pairs of
//      on/off events with a common displacement are a camera jump and move every track.
//   3. Tracks (at most 12): each follows one bright spot (a gate that grows while it is dark,
//      for shake during long gaps), measures its own signal = max of the 3×3 cells around it
//      minus a low percentile of a ring of cells around that (local background, bloom-aware),
//      keys it with hysteresis between its own on/off envelopes and records mark/space runs.
//   4. Morse-likeness: at least 4 marks in a row, marks and spaces 35 ms..3 s, both "short"
//      and "long" runs (≥ 2.2× the unit: dashes or character gaps — periodic flicker has none),
//      consistent mark brightness, contrast ≥ 5× the track's own noise. Only such a track is
//      read; the clearest one wins and is kept until it is really gone or the user taps another.
//   5. Every track keeps the last few seconds of its signal, so when one is locked its history
//      is replayed into the decoder: the first characters (sent before the lock) are not lost.
// Bounded memory (allocated once, on the first frame), no allocation per frame, O(cells) per
// frame. Every input is hostile: sizes, pointers and timestamps are checked.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ember {
namespace morse_rx {

constexpr int kMaxGridCells = 4096;

struct GridFrame {
  double t_ms = 0;
  int w = 0, h = 0;
  const uint8_t* max = nullptr;   // w·h bytes, row by row from the top-left
  const uint8_t* mean = nullptr;  // optional, same layout
};

struct Target {
  enum State : uint8_t { kSearching = 0, kLocked = 1, kLost = 2 };
  State state = kSearching;
  double x = 0, y = 0;    // 0..1 of the frame (centre of the light); valid unless searching
  double size = 0;        // 0..1 of the frame width (ring diameter)
  double confidence = 0;  // 0..1
  int candidates = 0;     // keyed lights currently seen
  bool pinned = false;
};

const char* target_state_name(Target::State s);

struct LightSample {
  double t_ms;
  double level;  // 0..1
};

class LightTracker {
 public:
  LightTracker();
  void reset();
  // One frame. Ignored (no effect) if the grid is invalid or the time is not finite; duplicate or
  // reordered frames are dropped, a large jump back re-bases the time axis. dot_ms is the
  // decoder's current dot estimate (0 = unknown), used for the "lost" and hold times.
  void frame(const GridFrame& f, double dot_ms);
  // Decoder input produced by the last frame(): the tracked light's level, in time order on the
  // tracker's own monotonic axis (several samples after a lock or a reacquisition: the replay).
  const std::vector<LightSample>& samples() const { return out_; }
  void pin(double fx, double fy);  // a user tap, 0..1 of the frame
  void unpin();
  bool pinned() const { return pin_pending_ || (sel_ >= 0 && tracks_[static_cast<size_t>(sel_)].pinned); }
  Target target() const;
  // True (and *t filled) when the target should be announced: a state change, a change in the
  // number of candidates, or movement — at most 5 times a second of frame time while moving.
  bool take_event(Target* t);
  bool started() const { return frames_ > 0; }
  uint64_t frames() const { return frames_; }

 private:
  static constexpr int kMaxTracks = 12;
  static constexpr int kHist = 320;  // signal history per track (≥ 5 s at 60 fps)
  static constexpr int kRuns = 32;
  static constexpr int kMaxBlobs = 32;
  struct Run {
    double end;
    float dur, peak;
    bool on;
    bool sharp;  // the edge that started it took at most two frames
  };
  struct Hist {
    double t;
    float v;
    bool on;  // the track's own (hysteresis) state after this sample
  };
  struct Track {
    bool used = false, on = false, pinned = false, keyed = false, sharp = true;
    uint32_t id = 0;
    float x = 0, y = 0, size = 0;
    float off = 0, onl = 0, nq = 1;  // envelopes and mean |change| of the signal (both states)
    float nq_off = 1;                // mean |change| while dark
    float last = 0, prev = 0, run_peak = 0;
    double created = 0, edge = 0, seen = 0, last_mark = 0, keyed_at = 0;
    float score = 0, snr = 0;
    int marks = 0, same = 0, total_marks = 0;
    int contra = 0;  // consecutive evaluations that contradict keying
    int streak = 0;  // consecutive evaluations that look like keying
    int doubt = 0;   // contradictions seen (decays with passes)
    int nruns = 0, rhead = 0;
    int hcount = 0, hhead = 0;
    Run runs[kRuns];
    Hist hist[kHist];
  };
  struct Blob {
    int x, y;
    float e, v;
    bool claimed, explained;
  };

  void ensure_buffers();
  void resize(int w, int h);
  void dilate(const float* src, float* dst);
  float signal_at(const float* img, const Track& k) const;
  float signal_at(const float* img, float x, float y, float size) const;
  void refine(Track& k);
  void key(Track& k, float sig, double t);
  void evaluate(Track& k, double t);
  void push_hist(Track& k, double t, float v, bool on);
  int new_track(const Blob& b, double t);
  void choose(double t);
  void apply_pin();
  void feed(double t);
  float gate(const Track& k, double t) const;
  bool radial(float x1, float y1, float x0, float y0, float* r) const;
  bool near_track(int x, int y, int except, double young_after) const;
  double lost_ms() const;
  double hold_ms() const;
  float amp(const Track& k) const;
  float sigma(const Track& k) const;

  // per-cell state (kMaxGridCells each, allocated once)
  std::vector<float> cur_, p1_, p2_, dc_, d1_, d2_, ref_, nq_, e_, f_, tmp_, scratch_;
  std::vector<Track> tracks_;
  std::vector<LightSample> out_;
  Blob on_[kMaxBlobs], off_[kMaxBlobs];
  int non_ = 0, noff_ = 0;
  int w_ = 0, h_ = 0, n_ = 0;
  int nprev_ = 0;  // previous frames available (0..2)
  bool ref_mean_ = false;
  bool have_t_ = false;
  double t_ = 0, offset_ = 0, frame_ms_ = 33, dot_ms_ = 100;
  double tp1_ = 0, tp2_ = 0;  // times of the previous two frames
  uint64_t frames_ = 0;
  uint32_t next_id_ = 1;
  int sel_ = -1;           // selected track index
  double fed_t_ = -1e300;  // last decoder sample time
  double wait_since_ = -1;
  bool pin_pending_ = false;
  double pin_x_ = 0, pin_y_ = 0;
  // event state
  Target ev_last_;
  double ev_t_ = -1e300;
};

}  // namespace morse_rx
}  // namespace ember
