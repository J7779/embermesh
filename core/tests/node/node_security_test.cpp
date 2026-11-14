// Node: regressions for the security review findings handled in the node layer (H4 durable replay
// memory, M3 conversation-table pressure, M4 unverified look-alikes, L7 outbox, L2 channels).
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>

#include "node/store_crypto.h"
#include "store/kv_store.h"

#include "node/node_harness.h"

using namespace nt;
using namespace ember;

TEST_SUITE("node") {

TEST_CASE("review H4: a captured DM cannot be replayed after the message was deleted, even after a restart") {
  std::string dir = temp_dir("replay");
  Net net(501);
  int lan = net.add_medium();
  auto a = std::make_unique<TNode>(dir, 501);
  TNode b("", 502);
  uint32_t la = net.join(*a, lan);
  net.join(b, lan);
  introduce(*a, b);
  std::vector<std::string> captured;
  net.tx_hook = [&](const Net::Port& p, const std::string& bytes) {
    if (p.node == &b) captured.push_back(bytes);
  };
  b.ok("messages.send", Json::object({{"conv", "dm:" + a->id}, {"text", "pay 100 to X"}}));
  REQUIRE(net.run_until([&] { return a->has_text("dm:" + b.id, "pay 100 to X"); }, 20000));
  net.tx_hook = nullptr;
  REQUIRE(!captured.empty());
  a->ok("conversations.clear", Json::object({{"conv", "dm:" + b.id}}));
  CHECK_FALSE(a->has_text("dm:" + b.id, "pay 100 to X"));
  net.run(2000);
  // restart A from its storage
  net.forget(*a);
  a.reset();
  a = std::make_unique<TNode>(dir, 503);
  net.add(*a);
  net.attach(*a, la, lan);
  a->ok("links.endpoint_state", Json::object({{"id", la}, {"connected", true}}));
  net.run(500);
  for (auto& f : captured) ember_link_rx(a->n, la, reinterpret_cast<const uint8_t*>(f.data()), f.size());
  net.run(2000);
  CHECK_FALSE(a->has_text("dm:" + b.id, "pay 100 to X"));
}

TEST_CASE("review M3: strangers filling the conversation table never delete real conversations") {
  Net net(510);
  int lan = net.add_medium();
  TNode a("", 510, kStart, ",\"max_conversations\":6");
  TNode friend_("", 511);
  net.join(a, lan);
  net.join(friend_, lan);
  introduce(a, friend_);
  a.ok("messages.send", Json::object({{"conv", "dm:" + friend_.id}, {"text", "are you ok?"}}));
  REQUIRE(net.run_until([&] { return friend_.has_text("dm:" + a.id, "are you ok?"); }, 20000));
  // 12 strangers DM us (each an authenticated but unknown identity)
  std::vector<std::unique_ptr<TNode>> strangers;
  for (int i = 0; i < 12; ++i) {
    strangers.push_back(std::make_unique<TNode>("", 520 + static_cast<uint64_t>(i)));
    net.join(*strangers.back(), lan);
    strangers.back()->ok("contacts.add", Json::object({{"card", a.card()}}));
  }
  net.run(3000);
  for (auto& s : strangers) s->ok("messages.send", Json::object({{"conv", "dm:" + a.id}, {"text", "hello from a stranger"}}));
  net.run(30000);
  CHECK(a.has_text("dm:" + friend_.id, "are you ok?"));
  bool friend_listed = false;
  for (const Json& c : a.items("conversations.list", "conversations")) friend_listed |= c.get_string("id") == "dm:" + friend_.id;
  CHECK(friend_listed);
}

TEST_CASE("review L7: an event flood never evicts link frames waiting in the outbox") {
  TNode a("", 530, kStart, ",\"max_outbox_items\":32");
  uint32_t id = static_cast<uint32_t>(a.ok("links.add", J(R"({"type":"raw","config":{}})")).get("link").get_int("id"));
  a.ok("links.endpoint_state", Json::object({{"id", id}, {"connected", true}}));
  a.drain();
  ember_tick(a.n, kStart + 20);
  a.ok("mesh.announce");
  ember_tick(a.n, kStart + 40);  // the announcement is now queued as link TX
  for (int i = 0; i < 200; ++i) a.ok("kv.set", Json::object({{"ns", "n"}, {"key", "k" + std::to_string(i)}, {"value", i}}));
  for (int i = 0; i < 40; ++i) a.ok("messages.send", Json::object({{"conv", "ch:public"}, {"text", "flood " + std::to_string(i)}}));
  uint64_t before = a.tx_items;
  a.drain();
  CHECK(a.tx_items > before);  // TX survived the events that overflowed the queue
}

TEST_CASE("review L2: random-key channels and passphrase strength") {
  TNode a("", 540);
  Json weak = a.ok("channels.create", J(R"({"name":"Camp","passphrase":"river"})"));
  CHECK(weak.get_string("passphrase_strength") == "weak");
  Json strong = a.ok("channels.create", J(R"({"name":"Camp2","passphrase":"correct horse battery staple"})"));
  CHECK(strong.get_string("passphrase_strength") == "strong");
  Json rnd = a.ok("channels.create", J(R"({"name":"Camp","random_key":true})"));
  CHECK(rnd.get("channel").get_bool("encrypted"));
  CHECK(rnd.get("channel").get_string("id") != weak.get("channel").get_string("id"));
  CHECK_FALSE(rnd.has("passphrase_strength"));
  std::string inv = a.ok("channels.invite", Json::object({{"id", rnd.get("channel").get_string("id")}})).get_string("invite");
  TNode b("", 541);
  CHECK(b.ok("channels.join", Json::object({{"invite", inv}})).get("channel").get_string("id") == rnd.get("channel").get_string("id"));
}

TEST_CASE("links: list reports the effective config; configure can replace the endpoint keeping the id") {
  TNode a("", 550);
  a.ok("node.set_profile", J(R"({"callsign":"QX0ANN"})"));
  Json l = a.ok("links.add", J(R"({"type":"kiss","config":{},"endpoint":{"kind":"serial","config":{"port":"/dev/a"}}})")).get("link");
  int64_t id = l.get_int("id");
  CHECK(l.get("config").get_string("callsign") == "QX0ANN");  // what the driver runs with, not N0CALL
  Json lst = a.ok("links.list");
  CHECK(lst.get("links")[0].get("config").get_string("callsign") == "QX0ANN");
  a.drain();
  Json c = a.ok("links.configure", Json::object({{"id", id}, {"endpoint", J(R"({"kind":"serial","config":{"port":"/dev/b","baud":9600}})")}})).get("link");
  CHECK(c.get_int("id") == id);
  CHECK(c.get("endpoint").get("config").get_string("port") == "/dev/b");
  a.drain();
  CHECK(a.saw("link.state", [&](const Json& d) { return d.get("link").get("endpoint").get("config").get_string("port") == "/dev/b"; }));
  CHECK(a.err("links.configure", Json::object({{"id", id}, {"endpoint", "x"}})) == "bad_params");
}

namespace {
std::vector<std::string> data_files(const std::string& dir) {
  std::vector<std::string> v;
  for (auto& e : std::filesystem::directory_iterator(dir)) {
    std::string n = e.path().filename().string();
    if (n.rfind("data-", 0) == 0) v.push_back(n);
  }
  std::sort(v.begin(), v.end());
  return v;
}
}  // namespace

TEST_CASE("store: a planted plaintext generation next to an encrypted store never downgrades it") {
  std::string dir = temp_dir("plant-enc"), plain = temp_dir("plant-plain");
  {
    TNode a(dir, 560);
    a.ok("node.set_profile", J(R"({"name":"Real"})"));
    a.ok("kv.set", J(R"({"ns":"n","key":"k","value":"REAL-DATA"})"));
    a.ok("security.set_passphrase", J(R"({"new":"pw"})"));
  }
  {
    TNode p(plain, 561);
    p.ok("node.set_profile", J(R"({"name":"Attacker"})"));
    p.ok("kv.set", J(R"({"ns":"n","key":"k","value":"PLANTED"})"));
    for (int i = 0; i < 3; ++i) {  // age it to a higher generation number than the real store
      p.ok("security.set_passphrase", J(R"({"new":"x"})"));
      p.ok("security.set_passphrase", J(R"({"old":"x","new":""})"));
    }
  }
  REQUIRE(std::filesystem::exists(dir + "/ember-encrypted.marker"));
  auto enc = data_files(dir), pl = data_files(plain);
  REQUIRE(enc.size() == 1);
  REQUIRE(pl.size() == 1);
  unsigned g = 0, gp = 0;
  REQUIRE(std::sscanf(enc[0].c_str(), "data-%u", &g) == 1);
  REQUIRE(std::sscanf(pl[0].c_str(), "data-%u", &gp) == 1);
  REQUIRE(gp > g);
  std::filesystem::copy_file(plain + "/" + pl[0], dir + "/" + pl[0]);
  {
    TNode a(dir, 562);
    Json st = a.ok("security.status");
    CHECK(st.get_bool("locked"));
    CHECK(st.get_bool("tampered"));
    CHECK(!st.get_string("tamper_message").empty());
    CHECK(a.ok("node.info").get_string("name") == "");  // nothing of the planted store is shown
    CHECK(a.ok("security.unlock", J(R"({"passphrase":"pw"})")).get_bool("unlocked"));
    CHECK(a.ok("node.info").get_string("name") == "Real");
    CHECK(a.ok("kv.get", J(R"({"ns":"n","key":"k"})")).get_string("value") == "REAL-DATA");
    CHECK(a.ok("security.status").get_bool("tampered"));
  }
  // The encrypted store deleted outright, a plaintext one in its place: the marker keeps the node
  // locked; nothing is wiped or opened.
  for (auto& f : data_files(dir)) std::filesystem::remove(dir + "/" + f);
  std::filesystem::copy_file(plain + "/" + pl[0], dir + "/" + pl[0]);
  {
    TNode a(dir, 563);
    Json st = a.ok("security.status");
    CHECK(st.get_bool("locked"));
    CHECK(st.get_bool("tampered"));
    CHECK(a.err("security.unlock", J(R"({"passphrase":"pw"})")) == "denied");
    CHECK(a.ok("security.status").get_bool("locked"));
    CHECK(a.ok("node.info").get_string("name") == "");
  }
  CHECK(std::filesystem::exists(dir + "/" + pl[0]));  // not wiped: the user decides (security.wipe)
}

TEST_CASE("store: hostile KDF parameters in the store header are rejected quickly, nothing is wiped") {
  std::string dir = temp_dir("hostile-kdf");
  {
    std::unique_ptr<store::Vfs> vfs = store::new_os_vfs();
    node::StorageParams sp;
    crypto::Csprng rng(ByteView(std::string_view("hostile-kdf-test-seed-0123456789ab")));
    sp = node::StorageParams::fresh(crypto::KdfParams{256 * 1024, 32}, rng);
    store::Options o;
    crypto::Key32 k{};
    o.cipher = std::make_unique<node::AeadRecordCipher>(k);
    o.cipher_params = sp.encode();
    o.entropy = [&](uint8_t* b, size_t n) { rng.fill(b, n); };
    std::unique_ptr<store::KvStore> kv;
    REQUIRE(store::KvStore::open(vfs.get(), dir, std::move(o), &kv) == store::Status::Ok);
    kv->close();
  }
  auto files = data_files(dir);
  REQUIRE(!files.empty());
  auto t0 = std::chrono::steady_clock::now();
  TNode a(dir, 570);
  CHECK(a.ok("security.status").get_bool("tampered"));
  CHECK(a.err("security.unlock", J(R"({"passphrase":"anything"})")) == "denied");
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  CHECK(ms < 2000);  // 256 MiB x 32 passes of Argon2 would take minutes
  CHECK(data_files(dir) == files);
}

TEST_CASE("store: an encrypted store written by the pre-v2 core opens, migrates and keeps working") {
  std::string dir = temp_dir("v1-store");
  std::string src = std::string(EMBER_TEST_DATA_DIR) + "/node/v1-encrypted-store";
  for (auto& e : std::filesystem::directory_iterator(src))
    if (e.path().extension() == ".ekv") std::filesystem::copy_file(e.path(), dir + "/" + e.path().filename().string());
  for (int round = 0; round < 2; ++round) {
    CAPTURE(round);
    TNode a(dir, 580 + static_cast<uint64_t>(round));
    Json st = a.ok("security.status");
    CHECK(st.get_bool("locked"));
    CHECK_FALSE(st.get_bool("tampered"));
    CHECK(a.err("security.unlock", J(R"({"passphrase":"wrong"})")) == "denied");
    CHECK(a.ok("security.unlock", J(R"({"passphrase":"old pass"})")).get_bool("unlocked"));
    Json info = a.ok("node.info");
    CHECK(info.get_string("name") == "Old Store");
    CHECK(info.get_string("callsign") == "KD2OLD");
    CHECK(info.get("fingerprint").size() == 8);
    CHECK(a.ok("kv.get", J(R"({"ns":"notes","key":"water"})")).get_string("value") == "V1-SECRET-20L");
    CHECK(a.ok("kv.get", J(R"({"ns":"notes","key":"after"})")).get_string("value") == "post-encryption");
    CHECK(a.has_text("ch:public", "written by the v1 store"));
    a.ok("kv.set", Json::object({{"ns", "notes"}, {"key", "round"}, {"value", round}}));
  }
  CHECK(std::filesystem::exists(dir + "/ember-encrypted.marker"));
}

}  // TEST_SUITE
