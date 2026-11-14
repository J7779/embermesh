// Node: persistence across restarts, encryption at rest, lock / unlock / re-key, duress and panic
// wipes, corrupted stores, bounded history, and storage / wire size measurements.
#include <filesystem>
#include <fstream>
#include <iterator>

#include "node/node_harness.h"
#include "proto/packet.h"

using namespace nt;
namespace fs = std::filesystem;

namespace {

bool dir_contains(const std::string& dir, const std::string& needle) {
  for (auto& e : fs::directory_iterator(dir)) {
    if (!e.is_regular_file()) continue;
    std::ifstream in(e.path(), std::ios::binary);
    std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (s.find(needle) != std::string::npos) return true;
  }
  return false;
}

std::string data_file(const std::string& dir) {
  for (auto& e : fs::directory_iterator(dir))
    if (e.path().filename().string().rfind("data-", 0) == 0) return e.path().string();
  return "";
}

uint64_t dir_bytes(const std::string& dir) {
  uint64_t n = 0;
  for (auto& e : fs::directory_iterator(dir))
    if (e.is_regular_file()) n += e.file_size();
  return n;
}

}  // namespace

TEST_SUITE("node") {

TEST_CASE("persist: identity, contacts, channels, messages, settings, kv and links survive a restart") {
  std::string dir = temp_dir("persist-all");
  TNode b("", 41);
  std::string a_id, ch_open, ch_priv, dm_id;
  int64_t link1 = 0, link2 = 0;
  {
    TNode a(dir, 40);
    a_id = a.id;
    a.ok("node.set_profile", J(R"({"name":"Ann","callsign":"QX0ANN","status":"ok"})"));
    a.ok("contacts.add", Json::object({{"card", b.card()}}));
    a.ok("contacts.update", Json::object({{"node_id", b.id}, {"favorite", true}, {"name", "Bobby"}}));
    ch_open = a.ok("channels.create", J(R"({"name":"Market"})")).get("channel").get_string("id");
    ch_priv = a.ok("channels.create", J(R"({"name":"Camp","passphrase":"river"})")).get("channel").get_string("id");
    a.ok("channels.update", Json::object({{"id", ch_open}, {"muted", true}}));
    a.ok("settings.set", J(R"({"settings":{"store_forward_days":5,"theme":"night","default_ttl_hops":9}})"));
    a.ok("kv.set", J(R"({"ns":"notes","key":"water","value":{"litres":20}})"));
    link1 = a.ok("links.add", J(R"({"type":"raw","name":"Hub","config":{"preset":"websocket"},"endpoint":{"kind":"websocket","config":{"url":"ws://x/ws"},"host_key":"hub"}})"))
                .get("link").get_int("id");
    link2 = a.ok("links.add", J(R"({"type":"raw","name":"Temp","persist":false})")).get("link").get_int("id");
    a.ok("messages.send", J(R"({"conv":"ch:public","text":"public before restart"})"));
    dm_id = a.ok("messages.send", Json::object({{"conv", "dm:" + b.id}, {"text", "unsent dm resumes"}})).get("message").get_string("id");
    a.ok("messages.send", Json::object({{"conv", "ch:" + ch_priv}, {"text", "camp note"}}));
    a.ok("position.set", J(R"({"lat":1.5,"lon":2.5})"));
    a.ok("netplan.set", J(R"({"entries":[{"label":"net","freq_hz":7100000,"mode":"lsb"}]})"));
  }
  TNode a(dir, 99);  // different entropy, same storage
  CHECK(a.id == a_id);
  Json info = a.ok("node.info");
  CHECK(info.get_string("name") == "Ann");
  CHECK(info.get_string("callsign") == "QX0ANN");
  CHECK(info.get_string("storage") == "disk");
  Json contacts = a.ok("contacts.list").get("contacts");
  REQUIRE(contacts.size() == 1);
  CHECK(contacts[0].get_string("name") == "Bobby");
  CHECK(contacts[0].get_bool("favorite"));
  CHECK(contacts[0].get("fingerprint").size() == 8);
  std::set<std::string> chans;
  for (const Json& c : a.items("channels.list", "channels")) {
    chans.insert(c.get_string("id"));
    if (c.get_string("id") == ch_open) CHECK(c.get_bool("muted"));
  }
  CHECK(chans.count("public"));
  CHECK(chans.count(ch_open));
  CHECK(chans.count(ch_priv));
  Json s = a.ok("settings.get").get("settings");
  CHECK(s.get_int("store_forward_days") == 5);
  CHECK(s.get_string("theme") == "night");
  CHECK(a.ok("kv.get", J(R"({"ns":"notes","key":"water"})")).get("value").get_int("litres") == 20);
  Json links = a.ok("links.list").get("links");
  REQUIRE(links.size() == 1);
  CHECK(links[0].get_int("id") == link1);
  CHECK(links[0].get_string("state") == "down");
  CHECK(links[0].get_string("name") == "Hub");
  CHECK(links[0].get("endpoint").get_string("host_key") == "hub");
  CHECK(links[0].get("config").get_string("preset") == "websocket");
  CHECK(a.has_text("ch:public", "public before restart"));
  CHECK(a.has_text("ch:" + ch_priv, "camp note"));
  CHECK(a.find_message("dm:" + b.id, dm_id).get_string("status") == "queued");
  CHECK(a.ok("position.get").get("position").get_double("lat") == doctest::Approx(1.5));
  CHECK(a.ok("netplan.get").get("plan").get("entries").size() == 1);
  // Ids are never reused.
  int64_t link3 = a.ok("links.add", J(R"({"type":"raw"})")).get("link").get_int("id");
  CHECK(link3 > link2);
  // The unsent DM resumes once the host reopens the endpoint (links.endpoint_state).
  Net net(40);
  int lan = net.add_medium();
  net.attach(a, static_cast<uint32_t>(link1), lan);
  a.ok("links.endpoint_state", Json::object({{"id", link1}, {"connected", true}}));
  net.join(b, lan);
  b.ok("contacts.add", Json::object({{"card", a.card()}}));
  REQUIRE(net.run_until([&] { return b.has_text("dm:" + a.id, "unsent dm resumes"); }, 120000, 50));
  REQUIRE(net.run_until([&] { return a.find_message("dm:" + b.id, dm_id).get_string("status") == "delivered"; }, 60000, 50));
}

TEST_CASE("persist: passphrase encrypts everything at rest; lock, wrong passphrase, re-key, remove") {
  std::string dir = temp_dir("persist-sec");
  std::string id;
  {
    TNode a(dir, 50);
    id = a.id;
    a.ok("node.set_profile", J(R"({"name":"SecretSam"})"));
    a.ok("kv.set", J(R"({"ns":"notes","key":"k","value":"SECRET-NOTE-123"})"));
    a.ok("messages.send", J(R"({"conv":"ch:public","text":"SECRET-MESSAGE-456"})"));
    a.ok("security.set_passphrase", J(R"({"new":"correct horse"})"));
    Json st = a.ok("security.status");
    CHECK(st.get_bool("has_passphrase"));
    CHECK_FALSE(st.get_bool("locked"));
    CHECK(a.err("security.set_passphrase", J(R"({"old":"nope","new":"x"})")) == "denied");
  }
  CHECK_FALSE(dir_contains(dir, "SECRET-NOTE-123"));
  CHECK_FALSE(dir_contains(dir, "SECRET-MESSAGE-456"));
  CHECK_FALSE(dir_contains(dir, "SecretSam"));
  {
    TNode a(dir, 51);
    CHECK(a.id == "");
    Json info = a.ok("node.info");
    CHECK(info.get_bool("locked"));
    CHECK(info.get_string("node_id") == "");
    CHECK(a.err("messages.list", J(R"({"conv":"ch:public"})")) == "locked");
    CHECK(a.err("kv.get", J(R"({"ns":"notes","key":"k"})")) == "locked");
    CHECK(a.err("contacts.list") == "locked");
    CHECK(a.err("security.set_passphrase", J(R"({"new":"x"})")) == "locked");
    a.ok("tools.list");  // the survival library works while locked
    a.ok("guides.categories");
    a.ok("rf.bands");
    a.ok("links.types");
    CHECK(a.ok("links.list").get("links").size() == 0);
    Json st = a.ok("security.status");
    CHECK(st.get_bool("locked"));
    CHECK(st.get_bool("has_passphrase"));
    CHECK(a.err("security.unlock", J(R"({"passphrase":"wrong horse"})")) == "denied");
    CHECK(a.ok("security.unlock", J(R"({"passphrase":"correct horse"})")).get_bool("unlocked"));
    a.drain();
    CHECK(a.saw("node.ready"));
    CHECK(a.ok("node.info").get_string("node_id") == id);
    CHECK(a.ok("kv.get", J(R"({"ns":"notes","key":"k"})")).get("value").as_string() == "SECRET-NOTE-123");
    CHECK(a.has_text("ch:public", "SECRET-MESSAGE-456"));
    a.ok("security.set_passphrase", J(R"({"old":"correct horse","new":"battery staple"})"));
  }
  {
    TNode a(dir, 52);
    CHECK(a.err("security.unlock", J(R"({"passphrase":"correct horse"})")) == "denied");
    a.ok("security.unlock", J(R"({"passphrase":"battery staple"})"));
    CHECK(a.ok("node.info").get_string("node_id") == id);
    a.ok("security.set_passphrase", J(R"({"old":"battery staple","new":""})"));
    CHECK_FALSE(a.ok("security.status").get_bool("has_passphrase"));
  }
  CHECK(dir_contains(dir, "SECRET-NOTE-123"));  // control: plaintext again after removing the passphrase
  TNode a(dir, 53);
  CHECK_FALSE(a.ok("node.info").get_bool("locked"));
  CHECK(a.id == id);
}

TEST_CASE("security: runtime lock gates the RPC but the node keeps relaying and storing") {
  Net net(51);
  int lan = net.add_medium();
  TNode a("", 54), b("", 55);
  net.join(a, lan);
  net.join(b, lan);
  introduce(a, b);
  net.run(1000);
  a.ok("security.set_passphrase", J(R"({"new":"pw"})"));
  a.ok("security.lock");
  a.drain();
  CHECK(a.saw("security.locked"));
  CHECK(a.ok("node.info").get_bool("locked"));
  CHECK(a.ok("node.info").get_string("node_id") == "");
  CHECK(a.err("messages.list", Json::object({{"conv", "dm:" + b.id}})) == "locked");
  a.events.clear();
  b.ok("messages.send", Json::object({{"conv", "dm:" + a.id}, {"text", "arrives while locked"}}));
  net.run(5000);
  CHECK(a.count("message.new") == 0);  // no content leaks to a locked UI
  CHECK(a.err("security.unlock", J(R"({"passphrase":"nope"})")) == "denied");
  a.ok("security.unlock", J(R"({"passphrase":"pw"})"));
  CHECK(a.has_text("dm:" + b.id, "arrives while locked"));
}

TEST_CASE("security: duress passphrase silently wipes and looks like a normal unlock") {
  std::string dir = temp_dir("duress");
  std::string id;
  std::vector<std::string> duress_events, normal_events;
  auto unlock_events = [](TNode& n, const char* pass, std::vector<std::string>* names) {
    n.drain();
    size_t n0 = n.events.size();
    Json r = n.ok("security.unlock", Json::object({{"passphrase", pass}}));
    n.drain();
    for (size_t i = n0; i < n.events.size(); ++i) names->push_back(n.events[i].name);
    return r;
  };
  {
    TNode a(dir, 60);
    id = a.id;
    a.ok("node.set_profile", J(R"({"name":"Ann","callsign":"QX0ANN","status":"at the school"})"));
    a.ok("settings.set", J(R"({"settings":{"region":"EU","units":"imperial","theme":"night","low_power":true}})"));
    a.ok("kv.set", J(R"({"ns":"n","key":"k","value":"DURESS-SECRET"})"));
    a.ok("contacts.add", J(R"({"node_id":"0011223344556677","name":"Friend"})"));
    CHECK(a.err("security.set_duress", J(R"({"passphrase":"fake"})")) == "bad_params");  // needs a passphrase first
    a.ok("security.set_passphrase", J(R"({"new":"real one"})"));
    CHECK(a.err("security.set_duress", J(R"({"passphrase":"real one"})")) == "bad_params");
    a.ok("security.set_duress", J(R"({"passphrase":"fake"})"));
    CHECK(a.ok("security.status").get_bool("has_duress"));
    CHECK(a.err("security.set_passphrase", J(R"({"old":"real one","new":"fake"})")) == "bad_params");
  }
  {
    TNode a(dir, 61);
    CHECK_FALSE(a.ok("security.status").get_bool("has_duress"));  // not revealed while locked
    CHECK(unlock_events(a, "fake", &duress_events).get_bool("unlocked"));
    Json info = a.ok("node.info");
    CHECK_FALSE(info.get_bool("locked"));
    CHECK(info.get_string("node_id").size() == 16);
    CHECK(info.get_string("node_id") != id);
    // Presentation state survives, so the UI does not show first-run setup.
    CHECK(info.get_string("name") == "Ann");
    CHECK(info.get_string("callsign") == "QX0ANN");
    CHECK(info.get_string("status") == "at the school");
    Json set = a.ok("settings.get").get("settings");
    CHECK(set.get_string("region") == "EU");
    CHECK(set.get_string("units") == "imperial");
    CHECK(set.get_string("theme") == "night");
    CHECK(set.get_bool("low_power"));
    bool pub = false;
    for (const Json& c : a.items("channels.list", "channels")) pub = pub || c.get_string("id") == "public";
    CHECK(pub);
    CHECK(a.ok("kv.get", J(R"({"ns":"n","key":"k"})")).get("value").is_null());
    CHECK(a.ok("contacts.list").get("contacts").size() == 0);
    Json st = a.ok("security.status");
    CHECK(st.get_bool("has_passphrase"));
    CHECK(st.get_bool("has_duress"));  // same as on the real device
  }
  CHECK_FALSE(dir_contains(dir, "DURESS-SECRET"));
  TNode a(dir, 62);
  CHECK(a.err("security.unlock", J(R"({"passphrase":"real one"})")) == "denied");
  // The duress passphrase keeps working as the storage passphrase, with the same profile.
  CHECK(unlock_events(a, "fake", &normal_events).get_bool("unlocked"));
  CHECK(a.ok("node.info").get_string("name") == "Ann");
  // A duress unlock emits exactly what a normal unlock emits.
  std::string de, ne;
  for (auto& x : duress_events) de += x + " ";
  for (auto& x : normal_events) ne += x + " ";
  CHECK(de == ne);
  CHECK(std::find(normal_events.begin(), normal_events.end(), "node.ready") != normal_events.end());
}

TEST_CASE("security: panic wipe destroys identity and data, also while locked") {
  std::string dir = temp_dir("wipe");
  std::string old_id, new_id;
  {
    TNode a(dir, 70);
    old_id = a.id;
    a.ok("kv.set", J(R"({"ns":"n","key":"k","value":"WIPE-ME"})"));
    a.ok("links.add", J(R"({"type":"raw"})"));
    a.ok("contacts.add", J(R"({"node_id":"0011223344556677"})"));
    CHECK(a.err("security.wipe", J(R"({"confirm":"erase"})")) == "bad_params");
    a.ok("security.wipe", J(R"({"confirm":"ERASE"})"));
    new_id = a.ok("node.info").get_string("node_id");
    CHECK(new_id.size() == 16);
    CHECK(new_id != old_id);
    CHECK(a.ok("contacts.list").get("contacts").size() == 0);
    CHECK(a.ok("links.list").get("links").size() == 0);
    CHECK(a.ok("kv.get", J(R"({"ns":"n","key":"k"})")).get("value").is_null());
    a.ok("security.set_passphrase", J(R"({"new":"pw"})"));
  }
  CHECK_FALSE(dir_contains(dir, "WIPE-ME"));
  {
    TNode a(dir, 71);
    CHECK(a.ok("node.info").get_bool("locked"));
    a.ok("security.wipe", J(R"({"confirm":"ERASE"})"));  // panic wipe works while locked
    Json info = a.ok("node.info");
    CHECK_FALSE(info.get_bool("locked"));
    CHECK(info.get_string("node_id") != new_id);
    CHECK_FALSE(a.ok("security.status").get_bool("has_passphrase"));
  }
}

TEST_CASE("persist: corrupted, truncated or foreign store files never crash the node") {
  for (int mode = 0; mode < 5; ++mode) {
    CAPTURE(mode);
    std::string dir = temp_dir("corrupt-" + std::to_string(mode));
    std::string id;
    {
      TNode a(dir, 80 + static_cast<uint64_t>(mode));
      id = a.id;
      for (int i = 0; i < 30; ++i)
        a.ok("messages.send", Json::object({{"conv", "ch:public"}, {"text", "message " + std::to_string(i)}}));
      a.ok("kv.set", J(R"({"ns":"n","key":"k","value":"v"})"));
    }
    std::string f = data_file(dir);
    REQUIRE(!f.empty());
    uint64_t size = fs::file_size(f);
    if (mode == 0) {
      fs::resize_file(f, size / 2);  // torn / truncated
    } else if (mode == 1) {
      std::fstream io(f, std::ios::in | std::ios::out | std::ios::binary);  // smash both header copies
      std::string junk(1024, '\x5a');
      io.write(junk.data(), static_cast<std::streamsize>(junk.size()));
    } else if (mode == 2) {
      std::fstream io(f, std::ios::in | std::ios::out | std::ios::binary);  // bit rot in the middle
      io.seekp(static_cast<std::streamoff>(size / 2));
      std::string junk(64, '\xff');
      io.write(junk.data(), static_cast<std::streamsize>(junk.size()));
    } else if (mode == 3) {
      fs::resize_file(f, 0);
    } else {
      fs::remove(f);
      std::ofstream(dir + "/data-7.ekv", std::ios::binary) << std::string(5000, '\x01');
      std::ofstream(dir + "/README.txt") << "not a store";
    }
    TNode a(dir, 90 + static_cast<uint64_t>(mode));
    Json info = a.ok("node.info");
    CHECK(info.get_string("node_id").size() == 16);
    if (mode == 0 || mode == 2) CHECK(a.id == id);  // salvage keeps the identity
    a.ok("messages.list", J(R"({"conv":"ch:public"})"));
    a.ok("messages.send", J(R"({"conv":"ch:public","text":"still alive"})"));
    CHECK(a.has_text("ch:public", "still alive"));
  }
}

TEST_CASE("persist: bounded history evicts channel chatter before direct messages") {
  TNode a("", 100, kStart, ",\"max_messages\":60");
  TNode b("", 101);
  introduce(a, b);
  for (int i = 0; i < 10; ++i) a.ok("messages.send", Json::object({{"conv", "dm:" + b.id}, {"text", "dm " + std::to_string(i)}}));
  for (int i = 0; i < 150; ++i)
    a.ok("messages.send", Json::object({{"conv", "ch:public"}, {"text", "chatter " + std::to_string(i)}}));
  std::vector<Json> pub = a.messages("ch:public", 1000);
  CHECK(pub.size() <= 50);
  CHECK(pub.back().get_string("text") == "chatter 149");  // newest kept
  CHECK(a.messages("dm:" + b.id, 1000).size() == 10);     // DMs untouched
  CHECK(a.ok("node.info").get_string("node_id") == a.id);  // identity never evicted
}

TEST_CASE("persist: message paging (latest N before ts) is exact") {
  TNode a("", 102);
  for (int i = 0; i < 120; ++i) {
    a.ok("messages.send", Json::object({{"conv", "ch:public"}, {"text", "m" + std::to_string(i)}}));
    if (i % 7 == 0) ember_tick(a.n, kStart + static_cast<uint64_t>(i) * 1000);
  }
  std::vector<std::string> all;
  int64_t before = -1;
  for (int page = 0; page < 20; ++page) {
    Json q = Json::object({{"conv", "ch:public"}, {"limit", 25}});
    if (before >= 0) q.set("before_ts", before);
    Json r = a.ok("messages.list", q);
    std::vector<std::string> chunk;
    for (const Json& m : r.get("messages").items()) chunk.push_back(m.get_string("text"));
    REQUIRE(!chunk.empty());
    for (size_t i = 1; i < r.get("messages").size(); ++i)
      CHECK(r.get("messages")[i - 1].get_int("ts") <= r.get("messages")[i].get_int("ts"));
    all.insert(all.begin(), chunk.begin(), chunk.end());
    before = r.get("messages")[0].get_int("ts");
    if (!r.get_bool("more")) break;
  }
  CHECK(all.size() == 120);
  std::set<std::string> uniq(all.begin(), all.end());
  CHECK(uniq.size() == 120);
}

TEST_CASE("measure: bytes on the wire for a short DM and storage per message") {
  Net net(110);
  int lan = net.add_medium();
  std::string dir = temp_dir("measure");
  TNode a(dir, 110), b("", 111);
  net.join(a, lan);
  net.join(b, lan);
  introduce(a, b);
  net.run(5000);
  std::vector<size_t> dm_frames;
  net.tx_hook = [&](const Net::Port& p, const std::string& bytes) {
    if (p.node != &a) return;
    ember::proto::FrameHeader h;
    ember::ByteView body;
    ember::ByteView f{std::string_view(bytes)};
    ember::proto::PacketView pv;
    if (ember::proto::parse_frame(f, h, body) && h.type == ember::proto::FrameType::Packet &&
        ember::proto::parse_packet(body, pv) && pv.h.kind == ember::proto::Kind::Text)
      dm_frames.push_back(bytes.size());
  };
  a.ok("messages.send", Json::object({{"conv", "dm:" + b.id}, {"text", "meet at the well at noon"}}));
  REQUIRE(net.run_until([&] { return b.has_text("dm:" + a.id, "meet at the well at noon"); }, 5000));
  REQUIRE(!dm_frames.empty());
  net.tx_hook = nullptr;
  net.run(2000);
  uint64_t before = dir_bytes(dir);
  Json st0 = a.ok("mesh.stats");
  const int n = 200;
  for (int i = 0; i < n; ++i)
    a.ok("messages.send", Json::object({{"conv", "ch:public"}, {"text", "status report number " + std::to_string(i) + ": all well"}}));
  net.run(2000);
  uint64_t after = dir_bytes(dir);
  Json st1 = a.ok("mesh.stats");
  int64_t live = (st1.get_int("db_live_bytes") - st0.get_int("db_live_bytes")) / n;
  int64_t recs = (st1.get_int("db_records") - st0.get_int("db_records")) / n;
  MESSAGE("short DM (24 chars): " << dm_frames[0] << " bytes on the wire (one raw frame). Per sent public message: "
          << live << " live bytes in " << recs << " records (message JSON + id index + DTN copy), "
          << (after - before) / n << " bytes of log growth before compaction");
  CHECK(live < 1000);
  CHECK(dm_frames[0] < 200);
}

}  // TEST_SUITE
