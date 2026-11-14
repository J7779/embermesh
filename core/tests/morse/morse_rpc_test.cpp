// morse.rx.* over the C API (docs/RPC_API.md §6): result shapes, the morse.text event, session
// limits and expiry, and hostile parameters.
#include <string>
#include <vector>

#include "base/codec.h"
#include "base/json.h"
#include "doctest.h"
#include "ember/ember.h"
#include "morse_sim.h"

using namespace ember;
using namespace morsesim;

namespace {

struct Node {
  ember_node* n = nullptr;
  uint64_t now = 1700000000000ull;
  std::vector<Json> events;
  Node() {
    std::vector<uint8_t> ent(32, 9);
    n = ember_create("{}", ent.data(), ent.size());
    ember_tick(n, now);
    drain();
  }
  ~Node() { ember_destroy(n); }
  void drain() {
    ember_out o;
    while (ember_poll(n, &o)) {
      if (o.kind != EMBER_OUT_EVENT) continue;
      Json j;
      if (Json::parse(std::string(reinterpret_cast<const char*>(o.data), o.len), j)) events.push_back(j);
    }
  }
  Json call(const std::string& m, const Json& p, bool* ok = nullptr, std::string* code = nullptr) {
    char* r = ember_call(n, m.c_str(), p.dump().c_str());
    Json env;
    REQUIRE(Json::parse(std::string(r), env));
    ember_free(r);
    drain();
    if (ok) *ok = env.get_bool("ok");
    if (code) *code = env.get("error").get_string("code");
    return env.get_bool("ok") ? env.get("result") : env.get("error");
  }
  void advance(uint64_t ms) {
    now += ms;
    ember_tick(n, now);
    drain();
  }
};

Json levels_json(const std::vector<double>& v, size_t a, size_t b) {
  Json arr = Json::array();
  for (size_t i = a; i < b && i < v.size(); ++i) arr.push(v[i]);
  return arr;
}

}  // namespace

TEST_SUITE("morse") {

TEST_CASE("morse.rx: light session decodes, streams morse.text and reports MorseStatus") {
  Node nd;
  bool ok;
  Json st = nd.call("morse.rx.start", Json::object({{"source", "light"}}), &ok);
  REQUIRE(ok);
  std::string sid = st.get_string("session");
  CHECK(!sid.empty());
  CHECK(st.get_string("source") == "light");
  Rng r(1);
  HandOptions h;
  h.wpm = 12;
  Timeline tl = hand_key("SOS SOS DE N0CALL K", h, r);
  CameraOptions co;
  co.fps = 30;
  Frames f = camera(tl, end_of(tl) + 3000, co, r);
  size_t chars_mid = 0;
  for (size_t i = 0; i < f.v.size(); i += 10) {
    Json p = Json::object({{"session", sid}, {"t_ms", f.t[i]}, {"dt_ms", 1000.0 / 30}, {"levels", levels_json(f.v, i, i + 10)}});
    // Alternate the two feed forms: uniform and per-sample times.
    if ((i / 10) % 2) {
      Json times = Json::array();
      for (size_t k = i; k < i + 10 && k < f.t.size(); ++k) times.push(f.t[k]);
      p = Json::object({{"session", sid}, {"times_ms", times}, {"levels", levels_json(f.v, i, i + 10)}});
    }
    Json s = nd.call("morse.rx.feed", p, &ok);
    REQUIRE(ok);
    if (i == f.v.size() / 2) chars_mid = static_cast<size_t>(s.get_int("chars"));
    for (const char* k : {"transcript", "pending"}) CHECK(s.get(k).is_string());
    for (const char* k : {"chars", "wpm", "level", "threshold", "quality"}) CHECK(s.get(k).is_number());
    CHECK(s.get("on").is_bool());
  }
  Json s = nd.call("morse.rx.status", Json::object({{"session", sid}}), &ok);
  REQUIRE(ok);
  CHECK(normalise(s.get_string("transcript")) == "SOS SOS DE N0CALL K");
  CHECK(chars_mid > 0);  // text appeared before the message ended
  CHECK(s.get_double("wpm") > 9);
  CHECK(s.get_double("wpm") < 15);
  CHECK(s.get_double("quality") > 0.5);
  std::string streamed;
  for (const Json& e : nd.events)
    if (e.get_string("event") == "morse.text") {
      CHECK(e.get("data").get_string("session") == sid);
      CHECK(e.get("data").get_string("source") == "light");
      streamed += e.get("data").get_string("text");
    }
  CHECK(normalise(streamed) == normalise(s.get_string("transcript")));
  CHECK(nd.call("morse.rx.reset", Json::object({{"session", sid}}), &ok).is_object());
  CHECK(nd.call("morse.rx.status", Json::object({{"session", sid}})).get_int("chars") == 0);
  Json stop = nd.call("morse.rx.stop", Json::object({{"session", sid}}), &ok);
  CHECK(ok);
  CHECK(stop.get("transcript").is_string());
  std::string code;
  nd.call("morse.rx.feed", Json::object({{"session", sid}, {"t_ms", 0}, {"dt_ms", 33}, {"levels", Json::array({0.5})}}), &ok, &code);
  CHECK(!ok);
  CHECK(code == "not_found");  // feed after stop
}

TEST_CASE("morse.rx: audio session over pcm_b64") {
  Node nd;
  bool ok;
  Json st = nd.call("morse.rx.start", Json::object({{"source", "audio"}, {"sample_rate", 16000}}), &ok);
  REQUIRE(ok);
  std::string sid = st.get_string("session");
  Rng r(2);
  HandOptions h;
  h.wpm = 20;
  Timeline tl = hand_key("CQ CQ DE N0CALL <AR>", h, r);
  AudioOptions ao;
  ao.rate = 16000;
  ao.tone_hz = 850;
  ao.snr_db = 15;
  std::vector<int16_t> pcm = audio(tl, end_of(tl) + 2000, ao, r);
  for (size_t off = 0; off < pcm.size(); off += 3200) {  // 200 ms per feed
    size_t n = std::min<size_t>(3200, pcm.size() - off);
    Bytes b(n * 2);
    for (size_t i = 0; i < n; ++i) {
      b[2 * i] = static_cast<uint8_t>(pcm[off + i]);
      b[2 * i + 1] = static_cast<uint8_t>(static_cast<uint16_t>(pcm[off + i]) >> 8);
    }
    Json s = nd.call("morse.rx.feed", Json::object({{"session", sid}, {"pcm_b64", base64_encode(ByteView(b))}}), &ok);
    REQUIRE(ok);
    CHECK(s.get("tone_hz").is_number());
    CHECK(s.get("snr_db").is_number());
  }
  Json s = nd.call("morse.rx.status", Json::object({{"session", sid}}));
  CHECK(normalise(s.get_string("transcript")) == "CQ CQ DE N0CALL <AR>");
  CHECK(std::fabs(s.get_double("tone_hz") - 850) < 25);
  CHECK(s.get_double("wpm") > 16);
  CHECK(s.get_double("wpm") < 24);
}

TEST_CASE("morse.rx: limits, expiry and hostile parameters") {
  Node nd;
  bool ok;
  std::string code;
  nd.call("morse.rx.start", Json::object({{"source", "radio"}}), &ok, &code);
  CHECK((!ok && code == "bad_params"));
  nd.call("morse.rx.start", Json::object({{"source", "audio"}, {"sample_rate", 4000}}), &ok, &code);
  CHECK((!ok && code == "bad_params"));
  nd.call("morse.rx.start", Json::object({{"source", "audio"}, {"sample_rate", 10000000}}), &ok, &code);
  CHECK((!ok && code == "bad_params"));
  nd.call("morse.rx.start", Json::object({{"source", "audio"}, {"tone_hz", 5000}}), &ok, &code);
  CHECK((!ok && code == "bad_params"));
  std::vector<std::string> ids;
  for (int i = 0; i < 4; ++i) {
    Json s = nd.call("morse.rx.start", Json::object({{"source", i % 2 ? "audio" : "light"}}), &ok);
    REQUIRE(ok);
    ids.push_back(s.get_string("session"));
  }
  nd.call("morse.rx.start", Json::object({{"source", "light"}}), &ok, &code);
  CHECK((!ok && code == "busy"));
  // hostile feeds
  nd.call("morse.rx.feed", Json::object({{"session", ids[1]}, {"pcm_b64", "!!!notbase64"}}), &ok, &code);
  CHECK((!ok && code == "bad_params"));
  nd.call("morse.rx.feed", Json::object({{"session", ids[1]}, {"pcm_b64", "AAE="}}), &ok);  // odd byte count
  CHECK(ok);
  nd.call("morse.rx.feed", Json::object({{"session", ids[0]}, {"levels", Json::array({"x"})}, {"t_ms", 0}, {"dt_ms", 33}}), &ok, &code);
  CHECK((!ok && code == "bad_params"));
  nd.call("morse.rx.feed", Json::object({{"session", ids[0]}, {"levels", Json::array({0.1})}, {"t_ms", 0}, {"dt_ms", 0}}), &ok, &code);
  CHECK((!ok && code == "bad_params"));
  nd.call("morse.rx.feed", Json::object({{"session", ids[0]}, {"levels", Json::array({0.1, 0.2})}, {"times_ms", Json::array({1})}}), &ok, &code);
  CHECK((!ok && code == "bad_params"));
  Json big = Json::array();
  for (int i = 0; i < 20001; ++i) big.push(0.5);
  nd.call("morse.rx.feed", Json::object({{"session", ids[0]}, {"levels", big}, {"t_ms", 0}, {"dt_ms", 33}}), &ok, &code);
  CHECK((!ok && code == "too_large"));
  // absurd but well-formed values are accepted and harmless
  Json weird = Json::array({-1e300, 1e300, -5, 7, 0.5, 1e-300, 0});
  Json times = Json::array({1e15, -1e15, 0, 0, -3, 1e12, 5});
  nd.call("morse.rx.feed", Json::object({{"session", ids[0]}, {"levels", weird}, {"times_ms", times}}), &ok);
  CHECK(ok);
  nd.call("morse.rx.feed", Json::object({{"session", ids[0]}, {"levels", weird}, {"t_ms", -1e15}, {"dt_ms", 10000}}), &ok);
  CHECK(ok);
  Json s = nd.call("morse.rx.status", Json::object({{"session", ids[0]}}), &ok);
  CHECK(ok);
  CHECK(s.get("transcript").is_string());
  nd.call("morse.rx.status", Json::object({{"session", "nope"}}), &ok, &code);
  CHECK((!ok && code == "not_found"));
  // 60 s without a feed closes a session
  nd.advance(30000);
  nd.call("morse.rx.status", Json::object({{"session", ids[2]}}), &ok);  // keeps ids[2] alive
  CHECK(ok);
  nd.advance(31000);
  nd.call("morse.rx.status", Json::object({{"session", ids[3]}}), &ok, &code);
  CHECK((!ok && code == "not_found"));
  nd.call("morse.rx.status", Json::object({{"session", ids[2]}}), &ok);
  CHECK(ok);
  Json n = nd.call("morse.rx.start", Json::object({{"source", "light"}}), &ok);  // room again
  CHECK(ok);
}

}  // TEST_SUITE
