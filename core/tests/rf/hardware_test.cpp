// rf.hardware ("What you need"): coverage, platform consistency, filters, RPC shape.
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/json.h"
#include "doctest.h"
#include "ember/ember.h"
#include "ember/link.h"
#include "rf/hardware.h"

using namespace ember;

namespace {

// Independent transcription of the endpoint tables in apps/<platform>/README.md (the oracle the
// derived platform lists are checked against; keep in step with those READMEs).
const std::map<std::string, std::set<std::string>>& readme_kinds() {
  static const std::map<std::string, std::set<std::string>> k = {
      {"android", {"ble_peer", "udp", "audio", "serial", "ble", "bt_spp", "tcp", "tcp_server", "websocket", "wifi_direct",
                   "loopback", "ws_server"}},
      {"ios", {"ble_peer", "multipeer", "audio", "tcp", "rtl_tcp", "tcp_server", "websocket", "udp", "ble", "loopback",
               "ws_server"}},
      {"web", {"websocket", "webrtc", "serial", "ble", "audio", "loopback"}},
      {"desktop", {"tcp", "tcp_server", "udp", "websocket", "ws_server", "serial", "audio", "audio_pipe", "rtl_tcp",
                   "loopback"}},
  };
  return k;
}

Json link_types() {
  Json t;
  REQUIRE(Json::parse(link_types_json(), t));
  return t;
}

}  // namespace

TEST_SUITE("rf") {

TEST_CASE("rf.hardware covers every link type and every audio preset") {
  std::set<std::string> types, presets;
  Json lt = link_types();
  for (const Json& t : lt.items()) {
    types.insert(t.get_string("type"));
    if (t.get_string("type") != "audio") continue;
    for (const Json& f : t.get("config").items())
      if (f.get_string("key") == "preset")
        for (const Json& o : f.get("options").items()) presets.insert(o.get_string("value"));
  }
  REQUIRE(types.size() >= 8);
  REQUIRE(presets.size() == 5);
  std::set<std::string> got_types, got_presets;
  for (const auto& m : rf::hardware_methods()) {
    got_types.insert(m.link_type);
    if (m.link_type == "audio") got_presets.insert(m.preset);
  }
  for (const auto& t : types) CHECK_MESSAGE(got_types.count(t), "no method for link type " << t);
  for (const auto& p : presets) CHECK_MESSAGE(got_presets.count(p), "no method for audio preset " << p);
  // The connection methods the contract lists are all there.
  std::set<std::string> ids;
  for (const auto& m : rf::hardware_methods()) ids.insert(m.id);
  for (const char* id : {"radio_vox_cable", "radio_ptt_interface", "radio_acoustic", "bluetooth_tnc", "serial_tnc",
                         "meshtastic", "rnode", "lora_at", "serial_radio_module", "sdr_dongle", "phone_to_phone_direct",
                         "hub_hotspot", "sneakernet", "morse_light", "morse_audio"})
    CHECK_MESSAGE(ids.count(id), id);
}

TEST_CASE("rf.hardware platforms match the platform READMEs and the core's endpoint kinds") {
  std::map<std::string, std::set<std::string>> type_kinds;
  Json lt = link_types();
  for (const Json& t : lt.items())
    for (const Json& k : t.get("endpoint_kinds").items()) type_kinds[t.get_string("type")].insert(k.as_string());
  for (const auto& m : rf::hardware_methods()) {
    CAPTURE(m.id);
    CHECK(!m.platforms.empty());
    CHECK((m.proven == "emulated" || m.proven == "untested hardware"));
    CHECK(!m.items.empty());
    CHECK(!m.title.empty());
    CHECK(!m.steps_summary.empty());
    // the manual chapter is a real guide
    std::ifstream g(std::string(EMBER_REPO_ROOT) + "/guides/content/" + m.manual_chapter + ".md");
    CHECK_MESSAGE(g.good(), "missing guide " << m.manual_chapter);
    if (m.link_type != "none") {
      REQUIRE(type_kinds.count(m.link_type));
      for (const auto& k : m.any_kinds) CHECK_MESSAGE(type_kinds[m.link_type].count(k), k << " not an endpoint of " << m.link_type);
    }
    for (const auto& p : m.platforms) {
      CAPTURE(p);
      const std::set<std::string>& have = readme_kinds().at(p);
      bool any = m.any_kinds.empty();
      for (const auto& k : m.any_kinds) any = any || have.count(k);
      if (std::find(m.os_serial_platforms.begin(), m.os_serial_platforms.end(), p) != m.os_serial_platforms.end())
        any = any || have.count("serial");
      CHECK(any);
      for (const auto& k : m.all_kinds) CHECK(have.count(k));
    }
    // and no supporting platform is left out
    for (const auto& kv : readme_kinds()) {
      bool any = m.any_kinds.empty();
      for (const auto& k : m.any_kinds) any = any || kv.second.count(k);
      if (std::find(m.os_serial_platforms.begin(), m.os_serial_platforms.end(), kv.first) != m.os_serial_platforms.end())
        any = any || kv.second.count("serial");
      bool all = true;
      for (const auto& k : m.all_kinds) all = all && kv.second.count(k);
      bool listed = std::find(m.platforms.begin(), m.platforms.end(), kv.first) != m.platforms.end();
      CHECK(listed == (any && all));
    }
  }
  // Known limits surface as notes / exclusions.
  auto ptt = rf::hardware_methods("", "radio_ptt_interface");
  REQUIRE(ptt.size() == 1);
  CHECK(std::find(ptt[0].platforms.begin(), ptt[0].platforms.end(), "ios") == ptt[0].platforms.end());
  CHECK(ptt[0].note_ios.find("no USB serial") != std::string::npos);
  CHECK(ptt[0].note_web.find("Chrome") != std::string::npos);
  CHECK(rf::hardware_methods("", "radio_vox_cable")[0].note_web.find("secure") != std::string::npos);
}

TEST_CASE("rf.hardware filters and RPC shape") {
  auto ios = rf::hardware_methods("ios");
  for (const auto& m : ios) CHECK(std::find(m.platforms.begin(), m.platforms.end(), "ios") != m.platforms.end());
  CHECK(ios.size() < rf::hardware_methods().size());
  CHECK(rf::hardware_methods("", "meshtastic").size() == 1);
  CHECK(rf::hardware_methods("web", "sdr_dongle").empty());
  CHECK(rf::hardware_methods("", "nope").empty());

  std::vector<uint8_t> ent(32, 9);
  ember_node* n = ember_create("{}", ent.data(), ent.size());
  REQUIRE(n);
  auto call = [&](const char* params) {
    char* r = ember_call(n, "rf.hardware", params);
    Json j;
    REQUIRE(Json::parse(std::string(r), j));
    ember_free(r);
    return j;
  };
  Json all = call("{}");
  REQUIRE(all.get_bool("ok"));
  const Json& ms = all.get("result").get("methods");
  REQUIRE(ms.size() == rf::hardware_methods().size());
  for (const Json& m : ms.items()) {
    for (const char* k : {"id", "title", "what_it_connects", "link_type", "steps_summary", "manual_chapter", "proven"})
      CHECK(m.get(k).is_string());
    CHECK(m.get("platforms").is_array());
    CHECK(m.get("platform_notes").is_object());
    for (const Json& i : m.get("items").items()) {
      CHECK(i.get("name").is_string());
      CHECK(i.get("why").is_string());
      CHECK(i.get("required").is_bool());
      CHECK(i.get("alternatives").is_array());
    }
  }
  Json web = call(R"({"platform":"web","method":"radio_vox_cable"})");
  REQUIRE(web.get_bool("ok"));
  REQUIRE(web.get("result").get("methods").size() == 1);
  CHECK(web.get("result").get("methods").items()[0].get_string("preset") == "handheld_vox");
  CHECK(!call(R"({"platform":"amiga"})").get_bool("ok"));
  ember_destroy(n);
}

TEST_CASE("rf.hardware manual chapters are operator-manual chapters (pinned list)") {
  // ids are embermesh/<file name under guides/content/embermesh without .md>
  const std::set<std::string> chapters = {
      "embermesh/handheld-audio-modem", "embermesh/hub-setup", "embermesh/kiss-tnc-packet", "embermesh/lora",
      "embermesh/morse-reading",        "embermesh/phone-to-phone", "embermesh/radio-gateway",
      "embermesh/sdr-listening",        "embermesh/sneakernet",     "embermesh/what-you-need",
      "embermesh/connections"};
  for (const auto& m : rf::hardware_methods("", "")) {
    CAPTURE(m.id);
    CHECK(chapters.count(m.manual_chapter) == 1);
  }
  for (const auto& m : rf::hardware_methods("", "")) {
    if (m.id.find("morse") != std::string::npos) CHECK(m.manual_chapter == "embermesh/morse-reading");
    if (m.id == "bluetooth_tnc") CHECK(std::find(m.platforms.begin(), m.platforms.end(), "desktop") != m.platforms.end());
  }
}

}  // TEST_SUITE
