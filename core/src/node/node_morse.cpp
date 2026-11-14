// EmberMesh node — Morse receive sessions (docs/RPC_API.md §6, §12): morse.rx.start / feed /
// status / reset / stop / target and the morse.text / morse.target events. The decoding lives in
// core/src/morse/rx.*, the whole-frame light tracker in core/src/morse/light_tracker.*.
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "base/codec.h"
#include "node/node.h"

namespace ember::node {

namespace {

Json status_json(const morse_rx::Status& s, bool light) {
  Json j = Json::object();
  j.set("transcript", s.transcript);
  j.set("chars", s.chars);
  j.set("wpm", s.wpm);
  j.set("on", s.on);
  j.set("level", std::round(s.level * 1000) / 1000);
  if (light) j.set("threshold", std::round(s.threshold * 1000) / 1000);
  j.set("quality", s.quality);
  j.set("pending", s.pending);
  if (s.audio) {
    j.set("tone_hz", s.tone_hz);
    j.set("snr_db", s.snr_db);
  }
  if (s.has_target) {
    const morse_rx::Target& t = s.target;
    Json o = Json::object({{"state", morse_rx::target_state_name(t.state)}});
    if (t.state != morse_rx::Target::kSearching) {
      o.set("x", std::round(t.x * 1000) / 1000);
      o.set("y", std::round(t.y * 1000) / 1000);
      o.set("size", std::round(t.size * 1000) / 1000);
    }
    o.set("confidence", t.confidence);
    o.set("candidates", t.candidates);
    o.set("pinned", t.pinned);
    j.set("target", o);
  }
  return j;
}

Json target_event(const std::string& id, const morse_rx::Target& t) {
  Json e = Json::object({{"session", id}, {"state", morse_rx::target_state_name(t.state)}});
  if (t.state != morse_rx::Target::kSearching) {
    e.set("x", std::round(t.x * 1000) / 1000);
    e.set("y", std::round(t.y * 1000) / 1000);
    e.set("size", std::round(t.size * 1000) / 1000);
  }
  e.set("confidence", t.confidence);
  e.set("candidates", t.candidates);
  return e;
}

// frames:[{t_ms, w, h, max_b64, mean_b64?}] (§12). Everything is checked before anything is fed.
bool read_frames(Params& p, const Json& arr, std::vector<Bytes>* bufs, std::vector<morse_rx::GridFrame>* out) {
  if (arr.size() == 0) return p.error("frames must not be empty");
  if (arr.size() > morse_rx::kMaxFramesPerFeed)
    return p.error("frames has too many frames (max " + std::to_string(morse_rx::kMaxFramesPerFeed) + " per feed)",
                   "too_large");
  const size_t max_b64 = (morse_rx::kMaxGridCells + 2) / 3 * 4 + 8;
  bufs->resize(arr.size() * 2);
  out->resize(arr.size());
  for (size_t i = 0; i < arr.size(); ++i) {
    const Json& f = arr[i];
    std::string at = "frames[" + std::to_string(i) + "]";
    if (!f.is_object()) return p.error(at + " must be an object");
    Params fp(f);
    double t = 0;
    int64_t w = 0, h = 0;
    std::string mx, mn;
    if (!fp.number("t_ms", &t, -1e15, 1e15, true) || !fp.integer("w", &w, 1, morse_rx::kMaxGridCells, true) ||
        !fp.integer("h", &h, 1, morse_rx::kMaxGridCells, true) || !fp.str("max_b64", &mx, max_b64, true, false) ||
        !fp.str("mean_b64", &mn, max_b64, false)) {
      RpcResult r = fp.fail();
      return p.error(at + ": " + r.message, r.code.c_str());
    }
    if (w * h > morse_rx::kMaxGridCells)
      return p.error(at + ": w*h must be at most " + std::to_string(morse_rx::kMaxGridCells), "too_large");
    Bytes& bm = (*bufs)[2 * i];
    Bytes& bn = (*bufs)[2 * i + 1];
    if (!base64_decode(mx, bm)) return p.error(at + ".max_b64 is not valid base64");
    if (bm.size() != static_cast<size_t>(w * h)) return p.error(at + ".max_b64 must hold exactly w*h bytes");
    morse_rx::GridFrame& g = (*out)[i];
    g.t_ms = t;
    g.w = static_cast<int>(w);
    g.h = static_cast<int>(h);
    g.max = bm.data();
    g.mean = nullptr;
    if (!mn.empty()) {
      if (!base64_decode(mn, bn)) return p.error(at + ".mean_b64 is not valid base64");
      if (bn.size() != bm.size()) return p.error(at + ".mean_b64 must hold exactly w*h bytes");
      g.mean = bn.data();
    }
  }
  return true;
}

bool read_numbers(Params& p, const char* key, std::vector<double>* out, bool required) {
  const Json* a = nullptr;
  if (!p.array(key, &a, required)) return false;
  if (!a) return true;
  if (a->size() > morse_rx::kMaxLevelsPerFeed)
    return p.error(std::string(key) + " has too many samples (max " + std::to_string(morse_rx::kMaxLevelsPerFeed) +
                       " per feed)",
                   "too_large");
  out->reserve(a->size());
  for (const Json& v : a->items()) {
    if (!v.is_number()) return p.error(std::string(key) + " must contain only numbers");
    out->push_back(v.as_double());
  }
  return true;
}

}  // namespace

morse_rx::Session* Node::morse_session(Params& p, RpcResult* err) {
  std::string id;
  if (!p.str("session", &id, 64, true, false)) {
    *err = p.fail();
    return nullptr;
  }
  morse_rx::Session* s = morse_.get(id, mono_);
  if (!s) *err = rpc_err("not_found", "no such Morse session (stopped, or closed after 60 s without a feed)");
  return s;
}

RpcResult Node::m_morse_rx_start(Params& p) {
  std::string source;
  int64_t rate = 8000;
  double wpm = 0, tone = 0;
  if (!p.str("source", &source, 16, true, false) || !p.integer("sample_rate", &rate, 1, 100000000, false) ||
      !p.number("wpm_hint", &wpm, 0, 60, false) || !p.number("tone_hz", &tone, 0, 100000, false))
    return p.fail();
  std::string err;
  auto s = morse_rx::Session::create(source, static_cast<uint32_t>(rate), wpm, tone, &err);
  if (!s) return rpc_err("bad_params", err);
  std::string id = morse_.start(std::move(s), mono_, &err);
  if (id.empty()) return rpc_err("busy", "at most 4 Morse receive sessions can be open; stop one first");
  return rpc_ok(Json::object({{"session", id}, {"source", source}}));
}

RpcResult Node::m_morse_rx_feed(Params& p) {
  RpcResult err;
  morse_rx::Session* s = morse_session(p, &err);
  if (!s) return err;
  std::string id = p.raw().get_string("session");
  if (s->is_audio()) {
    std::string b64;
    if (!p.str("pcm_b64", &b64, morse_rx::kMaxPcmBytesPerFeed / 3 * 4 + 8, true)) return p.fail();
    Bytes pcm;
    if (!base64_decode(b64, pcm)) return rpc_err("bad_params", "pcm_b64 is not valid base64");
    if (pcm.size() % 2) pcm.pop_back();
    std::vector<int16_t> x(pcm.size() / 2);
    for (size_t i = 0; i < x.size(); ++i)
      x[i] = static_cast<int16_t>(static_cast<uint16_t>(pcm[2 * i] | pcm[2 * i + 1] << 8));
    s->feed_pcm(x.data(), x.size());
  } else if (p.has("frames")) {
    const Json* arr = nullptr;
    if (!p.array("frames", &arr, true)) return p.fail();
    if (p.has("levels")) return rpc_err("bad_params", "give either frames or levels, not both");
    std::vector<Bytes> bufs;
    std::vector<morse_rx::GridFrame> frames;
    if (!read_frames(p, *arr, &bufs, &frames)) return p.fail();
    s->feed_frames(frames.data(), frames.size());
  } else {
    if (s->has_frames())
      return rpc_err("bad_params", "this session is fed with frames; start another session for levels");
    std::vector<double> levels, times;
    if (!read_numbers(p, "levels", &levels, true) || !read_numbers(p, "times_ms", &times, false)) return p.fail();
    if (p.has("times_ms")) {
      if (times.size() != levels.size()) return rpc_err("bad_params", "times_ms and levels must have the same length");
      s->feed_times(times, levels);
    } else {
      double t0 = 0, dt = 0;
      if (!p.number("t_ms", &t0, -1e15, 1e15, true) || !p.number("dt_ms", &dt, 0.01, 10000, true)) return p.fail();
      s->feed_uniform(t0, dt, levels);
    }
  }
  morse_rx::Status st = s->status();
  std::string text = s->take_new();
  if (!text.empty())
    emit("morse.text", Json::object({{"session", id}, {"source", s->source()}, {"text", text}, {"wpm", st.wpm}}));
  morse_rx::Target tg;
  if (s->take_target_event(&tg)) emit("morse.target", target_event(id, tg));
  return rpc_ok(status_json(st, !s->is_audio()));
}

RpcResult Node::m_morse_rx_target(Params& p) {
  RpcResult err;
  morse_rx::Session* s = morse_session(p, &err);
  if (!s) return err;
  if (s->is_audio()) return rpc_err("bad_params", "morse.rx.target applies to light sessions");
  bool autom = false, has_auto = false;
  double x = 0, y = 0;
  bool hx = false, hy = false;
  if (!p.boolean("auto", &autom, &has_auto) || !p.number("x", &x, 0, 1, false, &hx) || !p.number("y", &y, 0, 1, false, &hy))
    return p.fail();
  if (has_auto && autom) {
    if (hx || hy) return rpc_err("bad_params", "give either {x, y} or {auto:true}");
    s->target_auto();
  } else if (hx && hy) {
    s->target_pin(x, y);
  } else {
    return rpc_err("bad_params", "x and y (0..1 of the frame) or auto:true are required");
  }
  morse_rx::Status st = s->status();
  morse_rx::Target tg;
  if (s->take_target_event(&tg)) emit("morse.target", target_event(p.raw().get_string("session"), tg));
  return rpc_ok(status_json(st, true));
}

RpcResult Node::m_morse_rx_status(Params& p) {
  RpcResult err;
  morse_rx::Session* s = morse_session(p, &err);
  if (!s) return err;
  return rpc_ok(status_json(s->status(), !s->is_audio()));
}

RpcResult Node::m_morse_rx_reset(Params& p) {
  RpcResult err;
  morse_rx::Session* s = morse_session(p, &err);
  if (!s) return err;
  s->reset();
  return rpc_ok();
}

RpcResult Node::m_morse_rx_stop(Params& p) {
  std::string id, transcript;
  if (!p.str("session", &id, 64, true, false)) return p.fail();
  if (!morse_.stop(id, &transcript)) return rpc_err("not_found", "no such Morse session");
  return rpc_ok(Json::object({{"transcript", transcript}}));
}

}  // namespace ember::node
