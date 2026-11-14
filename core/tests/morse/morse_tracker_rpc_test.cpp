// morse.rx.* with whole-frame brightness grids over the C API (docs/RPC_API.md §12): the
// "morse_frames" feature flag, frames feeds, MorseStatus.target, the morse.target event,
// morse.rx.target (tap / auto), old levels sessions unchanged, hostile parameters, and a seeded
// garbage/fuzz campaign against the RPC and the tracker itself.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "base/codec.h"
#include "base/json.h"
#include "doctest.h"
#include "ember/ember.h"
#include "morse/light_tracker.h"
#include "morse/rx.h"
#include "morse_sim.h"
#include "scene_sim.h"

using namespace ember;
using namespace morsesim;
using namespace scenesim;

namespace {

struct Node {
  ember_node* n = nullptr;
  uint64_t now = 1700000000000ull;
  std::vector<Json> events;
  Node() {
    std::vector<uint8_t> ent(32, 7);
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
};

std::string b64(const std::vector<uint8_t>& v) { return base64_encode(ByteView(v.data(), v.size())); }

Json frame_json(const Frame& f, int w, int h, bool mean) {
  Json j = Json::object({{"t_ms", f.t}, {"w", w}, {"h", h}, {"max_b64", b64(f.max)}});
  if (mean) j.set("mean_b64", b64(f.mean));
  return j;
}

Scene night(const Timeline* tl) {
  Scene sc;
  sc.fps = 30;
  sc.jitter_ms = 2.5;
  sc.drop = 0.03;
  sc.bg = 12;
  sc.texture = 10;
  sc.noise = 2.5;
  sc.shot = 0.3;
  Light l;
  l.x = 200;
  l.y = 100;
  l.amp = 70;
  l.sigma_px = 0.6;
  l.keyed = tl;
  sc.lights.push_back(l);
  Light lamp;
  lamp.x = 400;
  lamp.y = 220;
  lamp.amp = 200;
  sc.lights.push_back(lamp);
  return sc;
}

}  // namespace

TEST_SUITE("morse") {

TEST_CASE("morse.rx frames: feature flag, a frames feed reads the light, target and morse.target events") {
  Node nd;
  bool ok;
  Json info = nd.call("node.info", Json::object(), &ok);
  REQUIRE(ok);
  bool flag = false;
  for (const Json& f : info.get("features").items()) flag = flag || (f.is_string() && f.as_string() == "morse_frames");
  CHECK(flag);

  Json st = nd.call("morse.rx.start", Json::object({{"source", "light"}}), &ok);
  REQUIRE(ok);
  std::string sid = st.get_string("session");
  Rng r(3);
  HandOptions h;
  h.wpm = 12;
  Timeline tl = hand_key("SOS DE N0CALL K", h, r);
  Scene sc = night(&tl);
  Renderer rd(sc, 3);
  std::vector<Frame> fr = rd.run(end_of(tl) + 3000);
  std::vector<std::string> states;
  for (size_t i = 0; i < fr.size(); i += 4) {  // batches of 4 frames, max and mean grids
    Json frames = Json::array();
    for (size_t k = i; k < std::min(fr.size(), i + 4); ++k) frames.push(frame_json(fr[k], sc.gw, sc.gh, k % 2 == 0));
    Json s = nd.call("morse.rx.feed", Json::object({{"session", sid}, {"frames", frames}}), &ok);
    REQUIRE(ok);
    REQUIRE(s.get("target").is_object());
    const Json& t = s.get("target");
    std::string state = t.get_string("state");
    CHECK((state == "searching" || state == "locked" || state == "lost"));
    if (states.empty() || states.back() != state) states.push_back(state);
    CHECK(t.get("confidence").is_number());
    CHECK(t.get("candidates").is_number());
    if (state == "searching") {
      CHECK(!t.has("x"));
    } else {
      for (const char* k : {"x", "y", "size"}) {
        CHECK(t.get(k).is_number());
        CHECK(t.get_double(k) >= 0);
        CHECK(t.get_double(k) <= 1);
      }
    }
    for (const char* k : {"transcript", "pending"}) CHECK(s.get(k).is_string());
    for (const char* k : {"chars", "wpm", "level", "threshold", "quality"}) CHECK(s.get(k).is_number());
  }
  Json s = nd.call("morse.rx.status", Json::object({{"session", sid}}), &ok);
  CHECK(normalise(s.get_string("transcript")) == "SOS DE N0CALL K");
  const Json& t = s.get("target");
  CHECK(std::fabs(t.get_double("x") - 200.5 / 512) < 0.03);
  CHECK(std::fabs(t.get_double("y") - 100.5 / 288) < 0.05);
  CHECK(t.get_int("candidates") <= 1);
  REQUIRE(states.size() >= 2);
  CHECK(states[0] == "searching");
  CHECK(states[1] == "locked");
  // morse.target: state changes are announced, movement at most 5 times a second.
  std::vector<Json> tev;
  std::string streamed;
  for (const Json& e : nd.events) {
    if (e.get_string("event") == "morse.target") tev.push_back(e.get("data"));
    if (e.get_string("event") == "morse.text") streamed += e.get("data").get_string("text");
  }
  CHECK(normalise(streamed) == "SOS DE N0CALL K");
  REQUIRE(tev.size() >= 2);
  bool saw_locked = false, saw_lost = false;
  for (const Json& e : tev) {
    CHECK(e.get_string("session") == sid);
    saw_locked = saw_locked || e.get_string("state") == "locked";
    saw_lost = saw_lost || e.get_string("state") == "lost";
    CHECK(e.get("confidence").is_number());
    if (e.get_string("state") != "searching") CHECK(e.get("x").is_number());
  }
  CHECK(saw_locked);
  CHECK(saw_lost);  // the sender stopped
  double span_s = (fr.back().t - fr.front().t) / 1000;
  CHECK(static_cast<double>(tev.size()) <= 5 * span_s + 4);

  // A tap pins the nearest light; auto releases it.
  Json pinned = nd.call("morse.rx.target", Json::object({{"session", sid}, {"x", 0.39}, {"y", 0.35}}), &ok);
  REQUIRE(ok);
  CHECK(pinned.get("target").get_bool("pinned"));
  Json autos = nd.call("morse.rx.target", Json::object({{"session", sid}, {"auto", true}}), &ok);
  REQUIRE(ok);
  CHECK(!autos.get("target").get_bool("pinned"));
  // Once fed with frames, the session refuses levels (do not mix the two).
  std::string code;
  nd.call("morse.rx.feed", Json::object({{"session", sid}, {"t_ms", 0}, {"dt_ms", 33}, {"levels", Json::array({0.5})}}), &ok, &code);
  CHECK((!ok && code == "bad_params"));
}

TEST_CASE("morse.rx frames: levels sessions are unchanged (no target field)") {
  Node nd;
  bool ok;
  std::string sid = nd.call("morse.rx.start", Json::object({{"source", "light"}})).get_string("session");
  Json s = nd.call("morse.rx.feed", Json::object({{"session", sid}, {"t_ms", 0}, {"dt_ms", 33}, {"levels", Json::array({0.1, 0.9})}}), &ok);
  REQUIRE(ok);
  CHECK(!s.has("target"));
  for (const Json& e : nd.events) CHECK(e.get_string("event") != "morse.target");
}

TEST_CASE("morse.rx frames: hostile parameters get clean errors") {
  Node nd;
  bool ok;
  std::string code;
  std::string sid = nd.call("morse.rx.start", Json::object({{"source", "light"}})).get_string("session");
  std::string aid = nd.call("morse.rx.start", Json::object({{"source", "audio"}})).get_string("session");
  std::vector<uint8_t> cells(64 * 36, 10);
  Json good = Json::object({{"t_ms", 0}, {"w", 64}, {"h", 36}, {"max_b64", b64(cells)}});
  auto feed = [&](const Json& frames) {
    nd.call("morse.rx.feed", Json::object({{"session", sid}, {"frames", frames}}), &ok, &code);
    return ok ? std::string("ok") : code;
  };
  auto with = [&](const char* k, const Json& v) {
    Json f = good;
    f.set(k, v);
    return Json::array({f});
  };
  CHECK(feed(Json::array({good})) == "ok");
  CHECK(feed(Json::array()) == "bad_params");
  CHECK(feed(Json::object()) == "bad_params");                    // not an array
  CHECK(feed(Json::array({Json(5)})) == "bad_params");            // not an object
  CHECK(feed(with("w", 0)) == "bad_params");                      // 0×0, 0×h
  CHECK(feed(with("h", 0)) == "bad_params");
  CHECK(feed(with("w", -3)) == "bad_params");
  CHECK(feed(with("w", 1.5)) == "bad_params");
  CHECK(feed(with("w", "64")) == "bad_params");
  CHECK(feed(with("w", 5000)) == "bad_params");
  {
    std::vector<uint8_t> big(100 * 100, 1);
    Json f = Json::object({{"t_ms", 0}, {"w", 100}, {"h", 100}, {"max_b64", b64(big)}});
    CHECK(feed(Json::array({f})) == "too_large");                 // w·h over 4096
  }
  CHECK(feed(with("max_b64", "!!!!not base64")) == "bad_params");
  CHECK(feed(with("max_b64", b64(std::vector<uint8_t>(100, 1)))) == "bad_params");  // wrong size
  CHECK(feed(with("max_b64", "")) == "bad_params");
  CHECK(feed(with("max_b64", 17)) == "bad_params");
  CHECK(feed(with("max_b64", std::string(100000, 'A'))) == "too_large");
  CHECK(feed(with("mean_b64", b64(std::vector<uint8_t>(10, 1)))) == "bad_params");
  CHECK(feed(with("mean_b64", "%%%")) == "bad_params");
  CHECK(feed(with("t_ms", 1e300)) == "bad_params");
  CHECK(feed(with("t_ms", "now")) == "bad_params");
  {
    Json f = good;
    Json frames = Json::array();
    for (int i = 0; i < 65; ++i) frames.push(f);
    CHECK(feed(frames) == "too_large");                           // gigantic batch
  }
  // frames and levels together, frames for an audio session, bad targets
  nd.call("morse.rx.feed", Json::object({{"session", sid}, {"frames", Json::array({good})}, {"levels", Json::array({0.1})}}), &ok, &code);
  CHECK((!ok && code == "bad_params"));
  nd.call("morse.rx.target", Json::object({{"session", aid}, {"x", 0.5}, {"y", 0.5}}), &ok, &code);
  CHECK((!ok && code == "bad_params"));
  for (const Json& p : {Json::object({{"session", sid}}), Json::object({{"session", sid}, {"x", 0.5}}),
                        Json::object({{"session", sid}, {"x", 2}, {"y", 0.5}}),
                        Json::object({{"session", sid}, {"x", 0.5}, {"y", 0.5}, {"auto", true}}),
                        Json::object({{"session", sid}, {"auto", "yes"}})}) {
    nd.call("morse.rx.target", p, &ok, &code);
    CHECK((!ok && code == "bad_params"));
  }
  nd.call("morse.rx.target", Json::object({{"session", "nope"}, {"auto", true}}), &ok, &code);
  CHECK((!ok && code == "not_found"));
  // Non-monotonic, duplicate, absurd and reset timestamps, size changes: accepted and harmless.
  for (double t : {5.0, 5.0, 1.0, -1e15, 1e15, 40.0, 1e15 - 1, 0.0, 33.0}) {
    Json f = good;
    f.set("t_ms", t);
    CHECK(feed(Json::array({f})) == "ok");
  }
  std::vector<uint8_t> one(1, 255);
  CHECK(feed(Json::array({Json::object({{"t_ms", 70}, {"w", 1}, {"h", 1}, {"max_b64", b64(one)}})})) == "ok");
  std::vector<uint8_t> tall(4096, 200);
  CHECK(feed(Json::array({Json::object({{"t_ms", 99}, {"w", 1}, {"h", 4096}, {"max_b64", b64(tall)}})})) == "ok");
  Json s = nd.call("morse.rx.status", Json::object({{"session", sid}}), &ok);
  REQUIRE(ok);
  CHECK(s.get_int("chars") == 0);
}

TEST_CASE("morse.rx frames: seeded garbage campaign (RPC and tracker) never crashes or produces text") {
  Node nd;
  bool ok;
  std::string code;
  std::string sid = nd.call("morse.rx.start", Json::object({{"source", "light"}})).get_string("session");
  Rng r(77);
  for (int it = 0; it < 400; ++it) {
    Json frames = Json::array();
    int nf = static_cast<int>(r.next() % 6);
    for (int k = 0; k < nf; ++k) {
      int w = static_cast<int>(r.next() % 80), h = static_cast<int>(r.next() % 60);
      if (r.unit() < 0.1) w = -w;
      size_t len = r.unit() < 0.8 ? static_cast<size_t>(std::max(0, w * h)) : static_cast<size_t>(r.next() % 6000);
      std::vector<uint8_t> bytes(len);
      for (uint8_t& b : bytes) b = static_cast<uint8_t>(r.next());
      Json f = Json::object();
      double t = r.unit() < 0.1 ? (r.unit() - 0.5) * 2e15 : it * 33.0 + (r.unit() - 0.5) * 100;
      if (r.unit() < 0.95) f.set("t_ms", t);
      if (r.unit() < 0.95) f.set("w", w);
      if (r.unit() < 0.95) f.set("h", h);
      std::string m = b64(bytes);
      if (r.unit() < 0.05 && !m.empty()) m[r.next() % m.size()] = '*';
      if (r.unit() < 0.95) f.set("max_b64", m);
      if (r.unit() < 0.3) f.set("mean_b64", b64(bytes));
      if (r.unit() < 0.05) f.set("max_b64", Json::array({1, 2}));
      frames.push(f);
    }
    nd.call("morse.rx.feed", Json::object({{"session", sid}, {"frames", frames}}), &ok, &code);
    if (!ok) CHECK((code == "bad_params" || code == "too_large"));
  }
  Json s = nd.call("morse.rx.status", Json::object({{"session", sid}}), &ok);
  REQUIRE(ok);
  CHECK(s.get("target").is_object());
  CHECK(s.get_int("chars") == 0);

  // The tracker alone: random sizes (valid), random bytes, hostile times.
  morse_rx::LightTracker tr;
  std::vector<uint8_t> mx(morse_rx::kMaxGridCells), mn(morse_rx::kMaxGridCells);
  const double nan = std::numeric_limits<double>::quiet_NaN(), inf = std::numeric_limits<double>::infinity();
  const double times[] = {nan, inf, -inf, 1e300, -1e300, 0, -0.0, 1e-300};
  double t = 0;
  for (int it = 0; it < 3000; ++it) {
    morse_rx::GridFrame g;
    int shape = static_cast<int>(r.next() % 8);
    g.w = shape == 0 ? 1 : shape == 1 ? 64 : shape == 2 ? 4096 : 1 + static_cast<int>(r.next() % 90);
    g.h = shape == 0 ? 1 : shape == 1 ? 36 : shape == 2 ? 1 : 1 + static_cast<int>(r.next() % 50);
    if (g.w * g.h > morse_rx::kMaxGridCells) g.h = morse_rx::kMaxGridCells / g.w;
    int pattern = static_cast<int>(r.next() % 4);
    for (int i = 0; i < g.w * g.h; ++i) {
      mx[static_cast<size_t>(i)] = pattern == 0 ? static_cast<uint8_t>(r.next()) : pattern == 1 ? 255 : pattern == 2 ? 0
                                   : static_cast<uint8_t>((it & 1) ? 255 : 0);  // the whole frame flashing
      mn[static_cast<size_t>(i)] = static_cast<uint8_t>(mx[static_cast<size_t>(i)] / 2);
    }
    t += r.unit() < 0.9 ? 33 : (r.unit() - 0.5) * 20000;
    g.t_ms = r.unit() < 0.05 ? times[r.next() % 8] : t;
    g.max = mx.data();
    g.mean = r.unit() < 0.5 ? mn.data() : nullptr;
    tr.frame(g, r.unit() < 0.1 ? nan : r.unit() * 1000);
    for (const morse_rx::LightSample& smp : tr.samples()) {
      CHECK(std::isfinite(smp.t_ms));
      CHECK(smp.level >= 0);
      CHECK(smp.level <= 1);
    }
    if (it % 100 == 0) tr.pin(r.unit(), r.unit());
    if (it % 170 == 0) tr.unpin();
    if (it % 500 == 0) tr.pin(nan, 0.5);
    morse_rx::Target tg = tr.target();
    CHECK(std::isfinite(tg.x));
    CHECK(std::isfinite(tg.y));
    CHECK(tg.x >= 0);
    CHECK(tg.x <= 1);
    CHECK(tg.confidence >= 0);
    CHECK(tg.confidence <= 1);
  }
  // Invalid frames are ignored outright.
  morse_rx::GridFrame bad;
  bad.w = 0;
  bad.h = 5;
  bad.max = mx.data();
  tr.frame(bad, 0);
  CHECK(tr.samples().empty());
  bad.w = 4096;
  bad.h = 2;
  tr.frame(bad, 0);
  CHECK(tr.samples().empty());
  bad.w = 4;
  bad.h = 4;
  bad.max = nullptr;
  tr.frame(bad, 0);
  CHECK(tr.samples().empty());
}

}  // TEST_SUITE
