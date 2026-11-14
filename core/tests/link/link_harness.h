// Test harness for link drivers: a fake LinkHost with an injectable clock, and a programmable pipe
// (chunking, loss, corruption, delay) that connects one driver's endpoint output to another's input.
//
// Sample callsigns in tests must never be real or plausibly issued: use N0CALL-n (with distinct
// SSIDs) where that suffices, and the never-issued Q prefix with a digit, QX<digit><letters>
// (QX0ABC, QX1ABC, QX0XYZ ...), where distinct base calls are needed (amateur mode refuses N0CALL).
// A digit keeps them distinct from the licence-free pseudo station ids, which are "QX" + 4 letters.
// Published examples keep their source (W1AW, the APRS specification's WU2Z / KB2ICI-14).
#pragma once

#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/json.h"
#include "ember/bytes.h"
#include "ember/link.h"

namespace linktest {

using ember::Bytes;
using ember::ByteView;

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed = 1) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
  uint64_t next() {
    s ^= s << 13; s ^= s >> 7; s ^= s << 17;
    return s;
  }
  uint32_t below(uint32_t n) { return n ? static_cast<uint32_t>(next() % n) : 0; }
  double unit() { return static_cast<double>(next() >> 11) / 9007199254740992.0; }
  uint8_t byte() { return static_cast<uint8_t>(next()); }
  Bytes bytes(size_t n) { Bytes b(n); for (auto& x : b) x = byte(); return b; }
};

struct Clock {
  uint64_t t = 1700000000000ull;
};

class FakeHost : public ember::LinkHost {
 public:
  explicit FakeHost(Clock* c, uint64_t seed = 7) : clk(c), rng(seed) {}
  struct RxFrame {
    Bytes data;
    ember::RxMeta meta;
  };
  Clock* clk;
  Prng rng;
  std::vector<RxFrame> frames;
  std::vector<Bytes> writes;
  std::vector<ember::InteropText> texts;
  std::vector<std::pair<std::string, std::string>> events;  // name, json
  std::function<void(ByteView)> on_write;                   // optional sink (pipe)

  void link_frame_received(ember::Link&, ByteView frame, const ember::RxMeta& meta) override {
    frames.push_back({frame.to_bytes(), meta});
  }
  void link_io_write(ember::Link&, ByteView bytes) override {
    writes.push_back(bytes.to_bytes());
    if (on_write) on_write(bytes);
  }
  void link_interop_text(ember::Link&, const ember::InteropText& msg) override { texts.push_back(msg); }
  void link_event(ember::Link&, const char* name, const std::string& json) override {
    events.emplace_back(name, json);
  }
  uint64_t now_ms() const override { return clk->t; }
  void random_bytes(uint8_t* out, size_t n) override {
    for (size_t i = 0; i < n; ++i) out[i] = rng.byte();
  }

  Bytes written() const {
    Bytes all;
    for (const auto& w : writes) all.insert(all.end(), w.begin(), w.end());
    return all;
  }
  size_t count_events(const std::string& name) const {
    size_t n = 0;
    for (const auto& e : events) n += e.first == name;
    return n;
  }
  void clear() { frames.clear(); writes.clear(); texts.clear(); events.clear(); }
};

struct PipeOpts {
  bool datagram = false;     // keep write boundaries (one write == one on_io_bytes)
  size_t max_chunk = 0;      // stream mode: re-chunk into random sizes 1..max_chunk (0 = as written)
  double drop = 0;           // probability a write (datagram) / chunk (stream) is lost
  double corrupt = 0;        // probability each byte gets one random bit flipped
  uint32_t delay_ms = 0;     // delivery latency
  uint64_t seed = 1;
};

// Bytes written by one link (via its FakeHost) are delivered to `dst` after the delay.
class Pipe {
 public:
  Pipe(Clock* c, PipeOpts o) : clk_(c), o_(o), rng_(o.seed) {}
  void set_dst(ember::Link* dst) { dst_ = dst; }
  void set_opts(PipeOpts o) { o_ = o; }
  bool enabled = true;

  void push(ByteView b) {
    if (!enabled) return;
    if (o_.datagram || o_.max_chunk == 0) {
      queue(b.to_bytes());
      return;
    }
    size_t off = 0;
    while (off < b.size()) {
      size_t n = 1 + rng_.below(static_cast<uint32_t>(o_.max_chunk));
      if (n > b.size() - off) n = b.size() - off;
      queue(b.sub(off, n).to_bytes());
      off += n;
    }
  }
  void pump() {
    while (!q_.empty() && q_.front().first <= clk_->t) {
      Bytes b = std::move(q_.front().second);
      q_.pop_front();
      if (dst_) dst_->on_io_bytes(ByteView(b));
    }
  }
  size_t pending() const { return q_.size(); }

 private:
  void queue(Bytes b) {
    if (o_.drop > 0 && rng_.unit() < o_.drop) return;
    if (o_.corrupt > 0)
      for (auto& x : b)
        if (rng_.unit() < o_.corrupt) x ^= static_cast<uint8_t>(1u << rng_.below(8));
    q_.emplace_back(clk_->t + o_.delay_ms, std::move(b));
  }
  Clock* clk_;
  PipeOpts o_;
  Prng rng_;
  ember::Link* dst_ = nullptr;
  std::deque<std::pair<uint64_t, Bytes>> q_;
};

// Two drivers back to back: a's endpoint output feeds b's input and vice versa.
struct Pair {
  Clock clk;
  FakeHost ha{&clk, 11}, hb{&clk, 22};
  std::unique_ptr<ember::Link> a, b;
  Pipe ab{&clk, PipeOpts()}, ba{&clk, PipeOpts()};
  std::string error;

  Pair(const std::string& type, const std::string& cfg_a, const std::string& cfg_b, PipeOpts o = PipeOpts()) {
    PipeOpts o2 = o;
    o2.seed = o.seed * 31 + 5;
    ab.set_opts(o);
    ba.set_opts(o2);
    a = ember::create_link(type, cfg_a, ha, &error);
    if (!a) return;
    b = ember::create_link(type, cfg_b, hb, &error);
    if (!b) return;
    ab.set_dst(b.get());
    ba.set_dst(a.get());
    ha.on_write = [this](ByteView v) { ab.push(v); };
    hb.on_write = [this](ByteView v) { ba.push(v); };
  }
  // Drivers may write to the host while shutting down: detach the pipes first, then destroy the
  // links before the pipes they were writing into.
  ~Pair() {
    ha.on_write = nullptr;
    hb.on_write = nullptr;
    b.reset();
    a.reset();
  }
  bool ok() const { return a && b; }
  void connect() {
    a->on_io_state(true);
    b->on_io_state(true);
  }
  void step(uint32_t ms) {
    clk.t += ms;
    a->tick(clk.t);
    b->tick(clk.t);
    ab.pump();
    ba.pump();
  }
  void run(uint32_t total_ms, uint32_t step_ms = 10) {
    for (uint32_t t = 0; t < total_ms; t += step_ms) step(step_ms);
  }
  // Runs until `pred` holds or the time budget is exhausted. Returns pred().
  template <class P>
  bool run_until(P pred, uint32_t max_ms, uint32_t step_ms = 10) {
    for (uint32_t t = 0; t < max_ms; t += step_ms) {
      if (pred()) return true;
      step(step_ms);
    }
    return pred();
  }
};

inline ember::Json parse_json(const std::string& s) {
  ember::Json j;
  ember::Json::parse(s, j);
  return j;
}

}  // namespace linktest
