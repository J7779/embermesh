// Shared helpers for modem tests: drive a TX modem to completion, push audio through the channel
// simulator into an RX modem in irregular chunks, and score what came out.
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "doctest.h"
#include "dsp/channel_sim.h"
#include "dsp/dsp.h"
#include "modem/modem.h"

namespace mt {

using ember::Bytes;
using ember::ByteView;
using ember::dsp::ChannelParams;
using ember::dsp::ChannelSim;
using ember::dsp::Prng;
using ember::modem::Modem;
using ember::modem::ModemOptions;
using ember::modem::RxItem;
using ember::modem::RxKind;

inline const std::vector<uint32_t>& all_rates() {
  static const std::vector<uint32_t> r = {8000, 11025, 16000, 22050, 32000, 44100, 48000};
  return r;
}

inline std::unique_ptr<Modem> make(const std::string& name, uint32_t rate, ModemOptions o = ModemOptions()) {
  std::string err;
  auto m = ember::modem::create_modem(name, rate, o, &err);
  INFO("create_modem(" << name << ", " << rate << "): " << err);
  REQUIRE(m != nullptr);
  return m;
}

// Pulls until the modem goes idle, in chunks of `chunk` samples.
inline std::vector<float> drain_tx(Modem& m, size_t chunk = 1000, size_t limit = 200000000) {
  std::vector<float> out;
  std::vector<float> buf(chunk);
  while (out.size() < limit) {
    size_t n = m.pull(buf.data(), chunk);
    out.insert(out.end(), buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(n));
    if (n < chunk && !m.tx_pending()) break;
  }
  return out;
}

// Feeds audio in pseudo-random chunk sizes (1..2000), collecting all items.
inline std::vector<RxItem> feed(Modem& m, const std::vector<float>& x, uint64_t seed = 1) {
  Prng r(seed);
  std::vector<RxItem> items;
  size_t pos = 0;
  while (pos < x.size()) {
    size_t n = 1 + r.below(2000);
    if (n > x.size() - pos) n = x.size() - pos;
    m.rx(&x[pos], n);
    pos += n;
    RxItem it;
    while (m.pop(&it)) items.push_back(std::move(it));
  }
  return items;
}

inline Bytes random_bytes(Prng& r, size_t n) {
  Bytes b(n);
  for (auto& v : b) v = static_cast<uint8_t>(r.u32());
  return b;
}

struct Score {
  int sent = 0;
  int good = 0;   // sent frames recovered (each counted once)
  int extra = 0;  // received frames that match nothing sent (false / corrupted frames)
  int dups = 0;   // sent frames recovered more than once
  std::string text;
  std::vector<RxItem> items;
};

inline Score score(const std::vector<Bytes>& sent, std::vector<RxItem> items) {
  Score s;
  s.sent = static_cast<int>(sent.size());
  std::vector<int> hits(sent.size(), 0);
  for (const RxItem& it : items) {
    if (it.kind == RxKind::Text) { s.text += it.text; continue; }
    bool found = false;
    for (size_t i = 0; i < sent.size(); ++i) {
      if (sent[i] == it.data && hits[i] == 0) { hits[i] = 1; found = true; break; }
    }
    if (!found) {
      bool dup = std::find(sent.begin(), sent.end(), it.data) != sent.end();
      if (dup) ++s.dups; else ++s.extra;
    }
  }
  for (int h : hits) s.good += h;
  s.items = std::move(items);
  return s;
}

struct LoopCfg {
  uint32_t tx_rate = 48000, rx_rate = 48000;
  ChannelParams ch;
  ModemOptions tx_opt, rx_opt;
  size_t lead_ms = 200, trail_ms = 300;
  bool one_tx = true;  // queue all frames into one transmission (else one transmission per frame)
};

// TX all frames, pass through the channel, RX. Returns the score.
inline Score loopback(const std::string& name, const std::vector<Bytes>& frames, const LoopCfg& c) {
  auto tx = make(name, c.tx_rate, c.tx_opt);
  auto rx = make(name, c.rx_rate, c.rx_opt);
  std::vector<float> audio;
  for (size_t i = 0; i < frames.size(); ++i) {
    REQUIRE(tx->send_frame(ByteView(frames[i])));
    if (!c.one_tx || i + 1 == frames.size()) {
      auto a = drain_tx(*tx);
      audio.insert(audio.end(), a.begin(), a.end());
      if (!c.one_tx) audio.insert(audio.end(), c.tx_rate / 10, 0.0f);
    }
  }
  if (c.rx_rate != c.tx_rate) {
    ember::dsp::Resampler rs;
    rs.init(c.tx_rate, c.rx_rate);
    std::vector<float> y;
    rs.process(audio.data(), audio.size(), y);
    rs.flush(y);
    audio.swap(y);
  }
  ChannelSim sim(c.ch, c.rx_rate);
  auto rxa = sim.run(audio, c.lead_ms * c.rx_rate / 1000, c.trail_ms * c.rx_rate / 1000);
  return score(frames, feed(*rx, rxa, c.ch.seed + 77));
}

// Text loopback for keyboard modes.
inline std::string loopback_text(const std::string& name, const std::string& text, const LoopCfg& c) {
  auto tx = make(name, c.tx_rate, c.tx_opt);
  auto rx = make(name, c.rx_rate, c.rx_opt);
  REQUIRE(tx->send_text(text));
  auto audio = drain_tx(*tx);
  if (c.rx_rate != c.tx_rate) {
    ember::dsp::Resampler rs;
    rs.init(c.tx_rate, c.rx_rate);
    std::vector<float> y;
    rs.process(audio.data(), audio.size(), y);
    rs.flush(y);
    audio.swap(y);
  }
  ChannelSim sim(c.ch, c.rx_rate);
  auto rxa = sim.run(audio, c.lead_ms * c.rx_rate / 1000, c.trail_ms * c.rx_rate / 1000);
  std::string out;
  for (auto& it : feed(*rx, rxa, c.ch.seed + 5)) out += it.text;
  return out;
}

// Levenshtein distance (character error counting for text modes).
inline size_t edit_distance(const std::string& a, const std::string& b) {
  std::vector<size_t> prev(b.size() + 1), cur(b.size() + 1);
  for (size_t j = 0; j <= b.size(); ++j) prev[j] = j;
  for (size_t i = 1; i <= a.size(); ++i) {
    cur[0] = i;
    for (size_t j = 1; j <= b.size(); ++j) {
      size_t sub = prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
      cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, sub});
    }
    prev.swap(cur);
  }
  return prev[b.size()];
}

// Pure-noise / garbage robustness: feeds `seconds` of each kind of junk, returns frames produced.
inline int garbage_frames(Modem& m, double seconds, uint64_t seed) {
  Prng r(seed);
  size_t n = static_cast<size_t>(seconds * m.sample_rate());
  std::vector<float> x(n);
  int frames = 0;
  RxItem it;
  // Gaussian noise at several levels.
  for (float lvl : {0.001f, 0.1f, 0.5f}) {
    for (auto& v : x) v = static_cast<float>(r.gauss()) * lvl;
    m.rx(x.data(), x.size());
    while (m.pop(&it)) frames += it.kind == RxKind::Frame ? 1 : 0;
  }
  // Full-scale uniform junk, NaN/Inf bursts, impulses, DC, silence.
  for (size_t i = 0; i < n; ++i) x[i] = static_cast<float>(r.uniform() * 2 - 1);
  for (size_t i = 0; i < n; i += 97) x[i] = (i % 3 == 0) ? NAN : ((i % 3 == 1) ? INFINITY : -INFINITY);
  m.rx(x.data(), x.size());
  for (size_t i = 0; i < n; ++i) x[i] = (i % 1000 == 0) ? 1.0f : 0.0f;
  m.rx(x.data(), x.size());
  for (size_t i = 0; i < n; ++i) x[i] = 0.7f;
  m.rx(x.data(), x.size());
  std::vector<int16_t> pcm(n);
  for (auto& v : pcm) v = static_cast<int16_t>(r.u32());
  m.rx_pcm16(pcm.data(), pcm.size());
  m.rx(nullptr, 100);
  m.rx(x.data(), 0);
  while (m.pop(&it)) frames += it.kind == RxKind::Frame ? 1 : 0;
  return frames;
}

// Wall-clock seconds for a callable (CPU measurements in perf tests only).
template <typename F>
double time_s(F&& f) {
  auto t0 = std::chrono::steady_clock::now();
  f();
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace mt
