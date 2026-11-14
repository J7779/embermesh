// C API (ember.h): NULL / garbage-argument safety, envelopes, outbox semantics, guides loading,
// thread safety of the per-node lock.
#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

#include "guides/fixture.h"
#include "node/node_harness.h"

using namespace nt;

namespace {
const uint8_t kEnt[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                          17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};

Json call_raw(ember_node* n, const char* m, const char* p) {
  char* r = ember_call(n, m, p);
  REQUIRE(r);
  std::optional<Json> j = Json::parse(r);
  ember_free(r);
  REQUIRE(j);
  return *j;
}
}  // namespace

TEST_SUITE("capi") {

TEST_CASE("capi: version, create / destroy and entropy rules") {
  const char* v = ember_version();
  REQUIRE(v);
  CHECK(std::strlen(v) >= 5);
  CHECK(ember_create("{}", nullptr, 32) == nullptr);
  CHECK(ember_create("{}", kEnt, 31) == nullptr);
  for (const char* cfg : {static_cast<const char*>(nullptr), "", "{", "[1,2]", "null", "{\"storage_dir\":5,\"now_ms\":\"x\"}",
                          "{\"platform\":\"\\u0000\",\"kdf_mem_kib\":-5,\"max_messages\":1e300}"}) {
    ember_node* n = ember_create(cfg, kEnt, sizeof kEnt);
    REQUIRE(n);
    Json info = call_raw(n, "node.info", nullptr);
    CHECK(info.get_bool("ok"));
    ember_destroy(n);
  }
  ember_destroy(nullptr);
  ember_free(nullptr);
}

TEST_CASE("capi: NULL and garbage arguments on every entry point") {
  ember_node* n = ember_create(R"({"storage_dir":"","now_ms":1767225600000})", kEnt, sizeof kEnt);
  REQUIRE(n);
  Json e = call_raw(nullptr, "node.info", "{}");
  CHECK_FALSE(e.get_bool("ok"));
  CHECK(e.get("error").get_string("code") == "bad_params");
  e = call_raw(n, nullptr, "{}");
  CHECK(e.get("error").get_string("code") == "bad_params");
  CHECK(call_raw(n, "node.info", nullptr).get_bool("ok"));
  CHECK(call_raw(n, "node.info", "").get_bool("ok"));
  std::string longm(10000, 'm');
  CHECK(call_raw(n, longm.c_str(), "{}").get("error").get_string("code") == "bad_params");
  CHECK(call_raw(n, "node.\xff\xfe", "{}").get("error").get_string("code") == "bad_params");
  ember_out o;
  CHECK(ember_poll(nullptr, &o) == 0);
  CHECK(o.kind == EMBER_OUT_NONE);
  CHECK(ember_poll(n, nullptr) == 0);
  ember_tick(nullptr, 5);
  CHECK(ember_next_wakeup_ms(nullptr) <= 60000);
  ember_add_entropy(nullptr, kEnt, 32);
  ember_add_entropy(n, nullptr, 32);
  ember_add_entropy(n, kEnt, 0);
  ember_link_rx(nullptr, 1, kEnt, 32);
  ember_link_rx(n, 1, nullptr, 32);
  ember_link_rx(n, 0, kEnt, 32);
  ember_link_rx(n, 0xffffffffu, kEnt, 32);
  int16_t pcm[64] = {0};
  ember_audio_rx(nullptr, 1, pcm, 64, 8000);
  ember_audio_rx(n, 1, nullptr, 64, 8000);
  ember_audio_rx(n, 1, pcm, 64, 0);
  CHECK(ember_audio_tx(nullptr, 1, pcm, 64, 8000) == 0);
  CHECK(ember_audio_tx(n, 1, nullptr, 64, 8000) == 0);
  CHECK(ember_audio_tx(n, 1, pcm, 0, 8000) == 0);
  CHECK(ember_audio_tx(n, 999, pcm, 64, 48000) == 0);
  CHECK(ember_guides_load(nullptr, kEnt, 32, 1) < 0);
  CHECK(ember_guides_load(n, nullptr, 32, 1) < 0);
  CHECK(ember_guides_load(n, kEnt, 0, 0) < 0);
  CHECK(ember_guides_load(n, kEnt, 32, 1) < 0);
  ember_tick(n, 0);
  ember_tick(n, UINT64_MAX / 2);
  ember_destroy(n);
}

TEST_CASE("capi: node.ready first, events are NUL-terminated JSON, link TX items carry the link id") {
  ember_node* n = ember_create(R"({"storage_dir":"","now_ms":1767225600000})", kEnt, sizeof kEnt);
  REQUIRE(n);
  ember_out o;
  REQUIRE(ember_poll(n, &o) == 1);
  CHECK(o.kind == EMBER_OUT_EVENT);
  CHECK(o.link_id == 0);
  CHECK(o.data[o.len] == 0);
  std::optional<Json> ev = Json::parse(std::string_view(reinterpret_cast<const char*>(o.data), o.len));
  REQUIRE(ev);
  CHECK(ev->get_string("event") == "node.ready");
  CHECK(ev->get("data").is_object());
  while (ember_poll(n, &o)) {
  }
  Json add = call_raw(n, "links.add", R"({"type":"raw","config":{}})");
  uint32_t id = static_cast<uint32_t>(add.get("result").get("link").get_int("id"));
  REQUIRE(id > 0);
  call_raw(n, "links.endpoint_state", ("{\"id\":" + std::to_string(id) + ",\"connected\":true}").c_str());
  call_raw(n, "mesh.announce", "{}");
  call_raw(n, "messages.send", R"({"conv":"ch:public","text":"hello air"})");
  int tx = 0;
  for (int i = 0; i < 50; ++i) {
    ember_tick(n, 1767225600000ull + 20u * static_cast<unsigned>(i));
    while (ember_poll(n, &o)) {
      if (o.kind == EMBER_OUT_LINK_TX) {
        CHECK(o.link_id == id);
        CHECK(o.len > 0);
        CHECK(o.len <= 1400);  // one datagram per item, within the preset MTU
        ++tx;
      }
    }
  }
  CHECK(tx >= 2);
  ember_destroy(n);
}

TEST_CASE("capi: outbox is bounded and drops oldest items when nobody polls") {
  ember_node* n = ember_create(R"({"storage_dir":"","now_ms":1767225600000,"max_outbox_items":32})", kEnt, sizeof kEnt);
  REQUIRE(n);
  for (int i = 0; i < 200; ++i) call_raw(n, "kv.set", R"({"ns":"a","key":"b","value":1})");
  for (int i = 0; i < 200; ++i) call_raw(n, "channels.update", R"({"id":"public","muted":false})");
  ember_out o;
  int count = 0;
  while (ember_poll(n, &o)) ++count;
  CHECK(count <= 32);
  CHECK(call_raw(n, "mesh.stats", "{}").get("result").get_int("outbox_dropped") > 0);
  ember_destroy(n);
}

TEST_CASE("capi: guides load by copy and zero-copy, garbage rejected, library replaceable") {
  const ember::Bytes& pack = guides_test::fixture_pack();
  REQUIRE(pack.size() > 100);
  ember_node* n = ember_create(R"({"storage_dir":""})", kEnt, sizeof kEnt);
  REQUIRE(n);
  {
    std::vector<uint8_t> tmp(pack.begin(), pack.end());
    CHECK(ember_guides_load(n, tmp.data(), tmp.size(), 1) == 0);
    std::fill(tmp.begin(), tmp.end(), 0);  // the core kept its own copy
  }
  Json c = call_raw(n, "guides.categories", "{}");
  CHECK(c.get("result").get_int("total") > 5);
  std::vector<uint8_t> keep(pack.begin(), pack.end());
  CHECK(ember_guides_load(n, keep.data(), keep.size(), 0) == 0);
  CHECK(call_raw(n, "guides.get", R"({"id":"water/purification-boiling"})").get_bool("ok"));
  std::vector<uint8_t> bad = keep;
  bad[bad.size() / 2] ^= 0x40;
  CHECK(ember_guides_load(n, bad.data(), bad.size(), 1) < 0);
  CHECK(call_raw(n, "guides.categories", "{}").get("result").get_int("total") == 0);
  CHECK(ember_guides_load(n, keep.data(), keep.size(), 0) == 0);
  CHECK(call_raw(n, "guides.categories", "{}").get("result").get_int("total") > 5);
  ember_destroy(n);
}

#ifndef __EMSCRIPTEN__  // no pthreads in the WASM build
TEST_CASE("capi: concurrent callers are serialised by the per-node lock") {
  ember_node* n = ember_create(R"({"storage_dir":"","now_ms":1767225600000})", kEnt, sizeof kEnt);
  REQUIRE(n);
  std::atomic<int> oks{0};
  std::atomic<bool> stop{false};
  std::thread ticker([&] {
    uint64_t t = 1767225600000ull;
    while (!stop) {
      ember_tick(n, t += 20);
      ember_out o;
      while (ember_poll(n, &o)) {
      }
      ember_next_wakeup_ms(n);
    }
  });
  std::vector<std::thread> callers;
  for (int k = 0; k < 3; ++k)
    callers.emplace_back([&, k] {
      for (int i = 0; i < 60; ++i) {
        std::string p = "{\"ns\":\"t" + std::to_string(k) + "\",\"key\":\"" + std::to_string(i) + "\",\"value\":" + std::to_string(i) + "}";
        char* r = ember_call(n, "kv.set", p.c_str());
        if (r && std::strstr(r, "\"ok\":true")) ++oks;
        ember_free(r);
        r = ember_call(n, "messages.send", R"({"conv":"ch:public","text":"thread"})");
        ember_free(r);
      }
    });
  for (auto& t : callers) t.join();
  stop = true;
  ticker.join();
  CHECK(oks == 180);
  Json l = call_raw(n, "kv.list", R"({"ns":"t1"})");
  CHECK(l.get("result").get("keys").size() == 60);
  ember_destroy(n);
}
#endif

}  // TEST_SUITE
