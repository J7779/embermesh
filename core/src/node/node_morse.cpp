// EmberMesh node — Morse receive sessions (docs/RPC_API.md §6): morse.rx.start / feed / status /
// reset / stop and the morse.text event. The decoding lives in core/src/morse/rx.*.
#include <cmath>

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
  return j;
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
  } else {
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
  return rpc_ok(status_json(st, !s->is_audio()));
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
