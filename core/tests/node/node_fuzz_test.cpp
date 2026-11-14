// Node: hostile input — seeded fuzz of ember_call (random methods, random / garbage / huge /
// deeply nested JSON), garbage on every link type, audio noise, mutated genuine mesh frames, calls
// while locked and after a wipe. Must never crash, leak (ASan) or grow without bound.
#include <set>

#include "node/node_harness.h"

using namespace nt;

namespace {

const char* const kMethods[] = {
    "node.info", "node.set_profile", "node.set_time", "identity.card", "identity.export", "identity.import",
    "contacts.list", "contacts.add", "contacts.update", "contacts.remove", "channels.list", "channels.create",
    "channels.invite", "channels.join", "channels.update", "channels.leave", "conversations.list",
    "conversations.clear", "messages.list", "messages.send", "messages.retry", "messages.mark_read",
    "messages.delete", "messages.search", "files.send", "files.get", "interop.send", "sos.start", "sos.stop",
    "sos.status", "sos.list", "sos.ack", "checkin.send", "checkin.list", "position.set", "position.get",
    "position.share", "bulletins.list", "bulletins.post", "bulletins.delete", "mesh.peers", "mesh.stats",
    "mesh.ping", "mesh.announce", "netplan.get", "netplan.set", "links.types", "links.list", "links.add",
    "links.configure", "links.remove", "links.endpoint_state", "guides.categories", "guides.list", "guides.get",
    "guides.search", "tools.list", "tools.run", "rf.bands", "tools.qr", "bundle.export", "bundle.import",
    "settings.get", "settings.set", "kv.get", "kv.set", "kv.delete", "kv.list", "security.status",
    "security.set_passphrase", "security.unlock", "security.lock", "security.set_duress", "security.wipe",
    "host.info", "nope", "node.", ".info", "messages.send.extra",
};
const char* const kKeys[] = {"conv", "text", "id", "node_id", "name", "card", "invite", "passphrase", "new", "old",
                             "settings", "ns", "key", "value", "type", "config", "endpoint", "connected", "link_id",
                             "data_b64", "query", "limit", "before_ts", "status", "position", "lat", "lon", "entries",
                             "category", "title", "expires_s", "scope", "confirm", "ts", "source", "inputs", "ecc",
                             "muted", "blocked", "verified", "priority", "ttl_hops", "interval_s", "mime", "backup"};

struct Gen {
  Rng r;
  explicit Gen(uint64_t s) : r(s) {}
  std::string str() {
    static const char* pool[] = {"", "x", "ch:public", "dm:0123456789abcdef", "io:aprs", "ERASE", "raw", "kiss",
                                 "public", "gps", "all", "water", "ok", "M", "AAAA", "ember:card?id=00", "ember:invite?v=1&n=x",
                                 "0123456789abcdef01234567", "\xc3\xa9t\xc3\xa9", "ch:deadbeef", "for_node"};
    switch (r.below(5)) {
      case 0: return pool[r.below(sizeof pool / sizeof pool[0])];
      case 1: {
        std::string s(r.below(40), 'a');
        for (char& c : s) c = static_cast<char>(32 + r.below(95));
        return s;
      }
      case 2: return std::string(r.below(3) == 0 ? 100000 : 3000, 'z');
      default: return pool[r.below(6)];
    }
  }
  Json value(int depth) {
    switch (r.below(depth > 4 ? 5 : 8)) {
      case 0: return Json();
      case 1: return Json(r.below(2) == 1);
      case 2: return Json(static_cast<int64_t>(r.next() % 2000) - 1000);
      case 3: return Json(static_cast<int64_t>(r.next()));
      case 4: return Json(str());
      case 5: {
        Json a = Json::array();
        for (uint32_t i = r.below(5); i > 0; --i) a.push(value(depth + 1));
        return a;
      }
      default: return object(depth + 1);
    }
  }
  Json object(int depth) {
    Json o = Json::object();
    for (uint32_t i = r.below(6); i > 0; --i) o.set(kKeys[r.below(sizeof kKeys / sizeof kKeys[0])], value(depth));
    return o;
  }
};

bool valid_envelope(const char* r) {
  if (!r) return false;
  std::optional<Json> j = Json::parse(r);
  if (!j || !j->is_object() || !j->get("ok").is_bool()) return false;
  if (j->get_bool("ok")) return j->get("result").is_object();
  return j->get("error").get("code").is_string() && j->get("error").get("message").is_string();
}

}  // namespace

TEST_SUITE("node") {

TEST_CASE("fuzz: random methods and random / garbage / huge / deep params never crash") {
  TNode a("", 200, kStart, ",\"max_outbox_items\":2048");
  Net net(200);
  int lan = net.add_medium();
  net.join(a, lan);
  Gen g(200);
  int argon_budget = 2;  // passphrase channels cost ~0.6 s each under ASan
  std::set<std::string> codes;
  for (int i = 0; i < 2500; ++i) {
    const char* m = kMethods[g.r.below(sizeof kMethods / sizeof kMethods[0])];
    std::string params;
    uint32_t mode = g.r.below(20);
    if (mode == 0) {
      params = std::string(g.r.below(200), '[') + "1";  // deep / unbalanced
    } else if (mode == 1) {
      params.resize(g.r.below(300));
      for (char& c : params) c = static_cast<char>(g.r.next());
    } else if (mode == 2) {
      params = "{\"a\":" + std::string(100, '[') + std::string(100, ']') + "}";
    } else {
      Json p = g.object(0);
      if ((std::string(m) == "channels.create" || std::string(m) == "channels.join") && p.get("passphrase").is_string() &&
          !p.get_string("passphrase").empty()) {
        if (argon_budget-- <= 0) p.erase("passphrase");
      }
      if (std::string(m) == "security.wipe" && g.r.below(10) != 0) p.erase("confirm");
      params = p.dump();
    }
    char* r = ember_call(a.n, m, params.c_str());
    REQUIRE(valid_envelope(r));
    std::optional<Json> j = Json::parse(r);
    if (!j->get_bool("ok")) codes.insert(j->get("error").get_string("code"));
    ember_free(r);
    if (i % 25 == 0) net.step(50);
    a.ok("security.status");
    if (a.ok("security.status").get_bool("locked")) a.call("security.wipe", J(R"({"confirm":"ERASE"})"));
  }
  std::set<std::string> allowed = {"bad_params", "not_found", "not_implemented", "unsupported", "busy",
                                   "denied",     "locked",    "too_large",       "io_error"};
  for (auto& c : codes) {
    INFO(c);
    CHECK(allowed.count(c));
  }
  CHECK_FALSE(codes.count("internal"));
  a.ok("node.info");
}

TEST_CASE("fuzz: garbage bytes on every link type and audio noise") {
  TNode a("", 210);
  std::vector<uint32_t> ids;
  for (const char* t : {"raw", "stream", "kiss", "audio", "meshtastic", "rnode", "lora_at", "rtl_tcp"}) {
    Json l = a.ok("links.add", Json::object({{"type", t}}));
    uint32_t id = static_cast<uint32_t>(l.get("link").get_int("id"));
    a.ok("links.endpoint_state", Json::object({{"id", id}, {"connected", true}}));
    ids.push_back(id);
  }
  Rng r(210);
  uint64_t now = kStart;
  std::vector<uint8_t> buf(70000);
  std::vector<int16_t> pcm(4800), out(4800);
  for (int round = 0; round < 400; ++round) {
    for (uint32_t id : ids) {
      size_t len = round % 50 == 0 ? 65536 : r.below(700);
      for (size_t i = 0; i < len; ++i) {
        uint32_t k = r.below(8);
        buf[i] = k == 0 ? 0xC0 : k == 1 ? 0xDB : k == 2 ? 0x94 : static_cast<uint8_t>(r.next());
      }
      ember_link_rx(a.n, id, buf.data(), len);
    }
    for (auto& s : pcm) s = static_cast<int16_t>(r.next());
    uint32_t rates[] = {8000, 22050, 44100, 48000, 7999};
    ember_audio_rx(a.n, ids[3], pcm.data(), pcm.size(), rates[r.below(5)]);
    ember_audio_tx(a.n, ids[3], out.data(), out.size(), 48000);
    now += 20;
    ember_tick(a.n, now);
    a.drain();
  }
  a.ok("node.info");
  Json st = a.ok("mesh.stats");
  CHECK(st.get_int("rx_frames") >= 0);
  CHECK(a.ok("links.list").get("links").size() == 8);
}

TEST_CASE("fuzz: mutated, truncated and replayed genuine mesh frames") {
  Net net(220);
  int lan = net.add_medium();
  TNode a("", 220), b("", 221);
  net.join(a, lan);
  uint32_t lb = net.join(b, lan);
  introduce(a, b);
  std::vector<std::string> captured;
  net.tx_hook = [&](const Net::Port& p, const std::string& bytes) {
    if (p.node == &a && captured.size() < 300) captured.push_back(bytes);
  };
  net.run(1000);
  a.ok("messages.send", Json::object({{"conv", "dm:" + b.id}, {"text", "genuine"}}));
  a.ok("messages.send", J(R"({"conv":"ch:public","text":"genuine public"})"));
  a.ok("sos.start", J(R"({"text":"help","interval_s":10})"));
  a.ok("bulletins.post", J(R"({"category":"info","title":"t"})"));
  net.run(3000);
  REQUIRE(!captured.empty());
  Rng r(221);
  for (int i = 0; i < 3000; ++i) {
    std::string f = captured[r.below(static_cast<uint32_t>(captured.size()))];
    switch (r.below(4)) {
      case 0: f[r.below(static_cast<uint32_t>(f.size()))] ^= static_cast<char>(1 << r.below(8)); break;
      case 1: f.resize(r.below(static_cast<uint32_t>(f.size()))); break;
      case 2: f += std::string(r.below(50), static_cast<char>(r.next())); break;
      default: break;  // replay
    }
    if (!f.empty()) ember_link_rx(b.n, lb, reinterpret_cast<const uint8_t*>(f.data()), f.size());
    if (i % 100 == 0) net.step(20);
  }
  net.run(2000);
  int copies = 0;
  for (auto& m : b.messages("dm:" + a.id)) copies += m.get_string("text") == "genuine";
  CHECK(copies == 1);  // replays never duplicate
  CHECK(b.ok("bulletins.list").get("bulletins").size() == 1);
  // Bit flips in the origin make "new" senders; SOS is delivered even unauthenticated
  // (PROTOCOL.md §4) but such alerts are flagged signed_ok:false. a's own alert exists once.
  int from_a = 0;
  for (const Json& al : b.items("sos.list", "alerts")) {
    if (al.get_string("node_id") == a.id) {
      ++from_a;
      CHECK(al.get_bool("signed_ok"));
    } else {
      CHECK_FALSE(al.get_bool("signed_ok"));
    }
  }
  CHECK(from_a == 1);
}

TEST_CASE("fuzz: every method while locked and after a wipe") {
  TNode a("", 230);
  a.ok("security.set_passphrase", J(R"({"new":"pw"})"));
  a.ok("security.lock");
  Gen g(230);
  for (const char* m : kMethods) {
    for (int k = 0; k < 3; ++k) {
      Json p = g.object(0);
      if (std::string(m) == "security.wipe" || std::string(m) == "security.unlock") continue;
      char* r = ember_call(a.n, m, p.dump().c_str());
      REQUIRE(valid_envelope(r));
      ember_free(r);
    }
  }
  CHECK(a.ok("node.info").get_bool("locked"));
  a.ok("security.wipe", J(R"({"confirm":"ERASE"})"));
  for (const char* m : kMethods) {
    Json p = g.object(0);
    if (std::string(m) == "security.lock") continue;
    char* r = ember_call(a.n, m, p.dump().c_str());
    REQUIRE(valid_envelope(r));
    ember_free(r);
  }
  a.ok("node.info");
}

}  // TEST_SUITE
