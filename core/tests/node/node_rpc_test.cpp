// Node: RPC conformance — every core method of docs/RPC_API.md §2: happy-path result shapes,
// missing / wrong-typed / oversized params, unknown methods and unknown ids.
#include <set>

#include "node/node_harness.h"

using namespace nt;

namespace {

// Every core method in docs/RPC_API.md §2 (host.* excluded).
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
};

void has_keys(const Json& o, std::initializer_list<const char*> keys) {
  REQUIRE(o.is_object());
  for (const char* k : keys) {
    INFO("missing key " << k << " in " << o.dump());
    CHECK(o.has(k));
  }
}

void check_message(const Json& m) {
  has_keys(m, {"id", "conv", "from", "from_name", "outgoing", "ts", "received_ts", "text", "kind", "status",
               "priority", "encrypted", "signed_ok", "hops", "via_link"});
  CHECK(m.get("id").is_string());
  CHECK(m.get("ts").is_int());
  std::set<std::string> kinds = {"text", "sos", "position", "checkin", "file", "interop", "system"};
  CHECK(kinds.count(m.get_string("kind")));
  std::set<std::string> st = {"queued", "sent", "relayed", "delivered", "read", "failed", "received"};
  CHECK(st.count(m.get_string("status")));
}

const std::string kNoSuchNode = "0123456789abcdef";
const std::string kNoSuchMsg = "0123456789abcdef01234567";

}  // namespace

TEST_SUITE("node") {

TEST_CASE("rpc: every documented method is implemented (no not_implemented)") {
  TNode a;
  for (const char* m : kMethods) {
    Json e = a.call(m, Json::object());
    INFO(m << " -> " << e.dump());
    if (!e.get_bool("ok")) CHECK(e.get("error").get_string("code") != "not_implemented");
  }
  CHECK(a.err("no.such.method") == "not_implemented");
  CHECK(a.err("host.info") == "unsupported");
  CHECK(a.err("") == "bad_params");
  CHECK(a.err(std::string(300, 'x')) == "bad_params");
}

TEST_CASE("rpc: node / identity") {
  TNode a("", 3);
  Json i = a.ok("node.info");
  has_keys(i, {"version", "node_id", "name", "callsign", "status", "fingerprint", "card", "platform", "time_ms",
               "time_source", "uptime_ms", "locked", "features"});
  CHECK(i.get_string("node_id").size() == 16);
  CHECK(i.get_string("name") == "");  // first-run detection: fresh identity has no name
  CHECK(i.get("fingerprint").size() == 8);
  CHECK(i.get_string("card").rfind("ember:card?", 0) == 0);
  CHECK(i.get_bool("locked") == false);
  CHECK(i.get_string("time_source") == "system");
  CHECK(i.get_int("time_ms") >= static_cast<int64_t>(kStart));

  Json p = a.ok("node.set_profile", J(R"({"name":"Ann","callsign":"qx4abc","status":"at camp"})"));
  CHECK(p.get_string("name") == "Ann");
  CHECK(p.get_string("callsign") == "QX4ABC");
  CHECK(a.err("node.set_profile", J(R"({"name":5})")) == "bad_params");
  CHECK(a.err("node.set_profile", Json::object({{"name", std::string(33, 'n')}})) == "too_large");
  CHECK(a.err("node.set_profile", J(R"({"callsign":"bad call!"})")) == "bad_params");
  CHECK(a.ok("node.set_profile", J(R"({"callsign":""})")).get_string("callsign") == "");

  Json t = a.ok("node.set_time", Json::object({{"ts", kStart + 3600000}, {"source", "gps"}}));
  CHECK(t.get("time_ms").is_int());
  a.drain();
  CHECK(a.saw("time.updated"));
  CHECK(a.ok("node.info").get_string("time_source") == "gps");
  CHECK(a.err("node.set_time", J(R"({"ts":"x","source":"gps"})")) == "bad_params");
  CHECK(a.err("node.set_time", J(R"({"ts":1767225600000,"source":"sundial"})")) == "bad_params");
  CHECK(a.err("node.set_time", J(R"({"source":"gps"})")) == "bad_params");

  std::string card = a.ok("identity.card").get_string("card");
  CHECK(card.find(a.id) != std::string::npos);
  Json bk = a.ok("identity.export", J(R"({"passphrase":"backup pass"})"));
  CHECK(bk.get_string("backup").rfind("ember:backup?", 0) == 0);
  CHECK(a.err("identity.export", J(R"({"passphrase":""})")) == "bad_params");
  TNode b("", 4);
  CHECK(b.err("identity.import", Json::object({{"backup", bk.get("backup")}, {"passphrase", "wrong"}})) == "denied");
  CHECK(b.err("identity.import", J(R"({"backup":"ember:backup?v=1&d=AAAA","passphrase":"x"})")) == "bad_params");
  CHECK(b.err("identity.import", J(R"({"backup":"not a backup","passphrase":"x"})")) == "bad_params");
  Json imp = b.ok("identity.import", Json::object({{"backup", bk.get("backup")}, {"passphrase", "backup pass"}}));
  CHECK(imp.get_string("node_id") == a.id);
  CHECK(imp.get_string("name") == "Ann");
}

TEST_CASE("rpc: contacts") {
  TNode a("", 5), b("", 6);
  Json c = a.ok("contacts.add", Json::object({{"card", b.card()}})).get("contact");
  has_keys(c, {"node_id", "name", "callsign", "verified", "blocked", "favorite", "status", "last_seen_ts", "hops",
               "direct", "via_link", "fingerprint"});
  CHECK(c.get_string("node_id") == b.id);
  CHECK(c.get("fingerprint").size() == 8);
  CHECK(c.get_bool("verified") == false);
  a.drain();
  CHECK(a.saw("contact.updated"));
  Json c2 = a.ok("contacts.add", J(R"({"node_id":"00112233445566AA","name":"Zed"})")).get("contact");
  CHECK(c2.get_string("node_id") == "00112233445566aa");
  CHECK(c2.get_string("name") == "Zed");
  CHECK(c2.get("fingerprint").size() == 0);
  CHECK(a.ok("contacts.list").get("contacts").size() == 2);
  Json u = a.ok("contacts.update", Json::object({{"node_id", b.id}, {"verified", true}, {"favorite", true}, {"name", "Bobby"}}));
  CHECK(u.get("contact").get_bool("verified"));
  CHECK(u.get("contact").get_string("name") == "Bobby");
  CHECK(a.err("contacts.update", Json::object({{"node_id", kNoSuchNode}, {"verified", true}})) == "not_found");
  CHECK(a.err("contacts.update", Json::object({{"node_id", b.id}, {"verified", "yes"}})) == "bad_params");
  CHECK(a.err("contacts.add", J(R"({"card":"ember:card?id=zz"})")) == "bad_params");
  CHECK(a.err("contacts.add", J(R"({"node_id":"123"})")) == "bad_params");
  CHECK(a.err("contacts.add", J(R"({})")) == "bad_params");
  CHECK(a.err("contacts.add", Json::object({{"card", a.card()}})) == "bad_params");  // own card
  CHECK(a.err("contacts.add", Json::object({{"card", std::string(5000, 'e')}})) == "too_large");
  a.ok("contacts.remove", J(R"({"node_id":"00112233445566aa"})"));
  CHECK(a.err("contacts.remove", J(R"({"node_id":"00112233445566aa"})")) == "not_found");
  CHECK(a.err("contacts.remove", J(R"({"node_id":7})")) == "bad_params");
}

TEST_CASE("rpc: channels") {
  TNode a("", 7);
  Json l = a.ok("channels.list");
  REQUIRE(l.get("channels").size() == 1);
  Json pub = l.get("channels")[0];
  has_keys(pub, {"id", "name", "encrypted", "is_default", "muted", "unread", "members_seen"});
  CHECK(pub.get_string("id") == "public");
  CHECK(pub.get_bool("is_default"));
  CHECK_FALSE(pub.get_bool("encrypted"));
  Json open = a.ok("channels.create", J(R"({"name":"Market"})")).get("channel");
  CHECK_FALSE(open.get_bool("encrypted"));
  CHECK(open.get_string("id").size() == 8);
  CHECK(a.ok("channels.create", J(R"({"name":"Market"})")).get("channel").get_string("id") == open.get_string("id"));
  std::string inv = a.ok("channels.invite", Json::object({{"id", open.get("id")}})).get_string("invite");
  CHECK(inv.rfind("ember:invite?", 0) == 0);
  TNode b("", 8);
  Json joined = b.ok("channels.join", Json::object({{"invite", inv}})).get("channel");
  CHECK(joined.get_string("id") == open.get_string("id"));
  CHECK(b.ok("channels.join", J(R"({"name":"market"})")).get("channel").get_string("id") == open.get_string("id"));
  CHECK(a.ok("channels.invite", J(R"({"id":"public"})")).get_string("invite").find("ch=public") != std::string::npos);
  CHECK(a.ok("channels.update", Json::object({{"id", open.get("id")}, {"muted", true}})).get("channel").get_bool("muted"));
  CHECK(a.err("channels.update", J(R"({"id":"deadbeef","muted":true})")) == "not_found");
  CHECK(a.err("channels.update", Json::object({{"id", open.get("id")}, {"muted", 1}})) == "bad_params");
  CHECK(a.err("channels.create", J(R"({"name":"   "})")) == "bad_params");
  CHECK(a.err("channels.create", J(R"({"name":"public"})")) == "bad_params");
  CHECK(a.err("channels.create", J(R"({})")) == "bad_params");
  CHECK(a.err("channels.create", Json::object({{"name", std::string(100, 'x')}})) == "too_large");
  CHECK(a.err("channels.join", J(R"({"invite":"ember:invite?v=1&n=x&k=AAAA"})")) == "bad_params");
  CHECK(a.err("channels.join", J(R"({"invite":"garbage"})")) == "bad_params");
  CHECK(a.err("channels.join", J(R"({})")) == "bad_params");
  CHECK(a.err("channels.invite", J(R"({"id":"nope"})")) == "not_found");
  CHECK(a.err("channels.leave", J(R"({"id":"public"})")) == "denied");
  CHECK(a.err("channels.leave", J(R"({"id":"00000000"})")) == "not_found");
  a.ok("channels.leave", Json::object({{"id", open.get("id")}}));
  CHECK(a.ok("channels.list").get("channels").size() == 1);
}

TEST_CASE("rpc: conversations / messages / files / interop") {
  TNode a("", 9), b("", 10);
  introduce(a, b);
  std::string dm = "dm:" + b.id;
  Json m = a.ok("messages.send", Json::object({{"conv", dm}, {"text", "hi"}, {"priority", "high"}, {"ttl_hops", 3},
                                               {"expires_s", 3600}, {"want_ack", true}}))
               .get("message");
  check_message(m);
  CHECK(m.get_string("priority") == "high");
  CHECK(m.get_string("status") == "queued");
  CHECK(m.get("expires_ts").is_int());
  Json pm = a.ok("messages.send", J(R"({"conv":"ch:public","text":"public hello"})")).get("message");
  CHECK_FALSE(pm.get_bool("encrypted"));
  CHECK(a.err("messages.send", Json::object({{"conv", dm}, {"text", ""}})) == "bad_params");
  CHECK(a.err("messages.send", Json::object({{"conv", dm}, {"text", "   "}})) == "bad_params");
  CHECK(a.err("messages.send", Json::object({{"conv", dm}, {"text", std::string(2001, 'x')}})) == "too_large");
  CHECK(a.err("messages.send", Json::object({{"conv", dm}, {"text", "x"}, {"priority", "urgent"}})) == "bad_params");
  CHECK(a.err("messages.send", Json::object({{"conv", dm}, {"text", "x"}, {"ttl_hops", 99}})) == "bad_params");
  CHECK(a.err("messages.send", Json::object({{"conv", dm}, {"text", 5}})) == "bad_params");
  CHECK(a.err("messages.send", J(R"({"conv":"ch:deadbeef","text":"x"})")) == "not_found");
  CHECK(a.err("messages.send", J(R"({"conv":"xx:1","text":"x"})")) == "bad_params");
  CHECK(a.err("messages.send", Json::object({{"conv", "dm:" + a.id}, {"text", "self"}})) == "bad_params");
  CHECK(a.err("messages.send", J(R"({"text":"x"})")) == "bad_params");

  Json conv = a.ok("conversations.list");
  bool has_dm = false, has_pub = false;
  for (const Json& c : conv.get("conversations").items()) {
    has_keys(c, {"id", "kind", "title", "unread", "muted", "last_ts"});
    has_dm = has_dm || c.get_string("id") == dm;
    has_pub = has_pub || c.get_string("id") == "ch:public";
  }
  CHECK(has_dm);
  CHECK(has_pub);
  CHECK(conv.get("conversations")[0].get_int("last_ts") >= conv.get("conversations")[1].get_int("last_ts"));

  Json list = a.ok("messages.list", Json::object({{"conv", dm}}));
  CHECK(list.get("messages").size() == 1);
  CHECK(list.get("more").is_bool());
  CHECK(a.err("messages.list", Json::object({{"conv", dm}, {"limit", 0}})) == "bad_params");
  CHECK(a.err("messages.list", Json::object({{"conv", dm}, {"before_ts", "yesterday"}})) == "bad_params");
  CHECK(a.err("messages.list", J(R"({"conv":"ch:12345678"})")) == "not_found");
  CHECK(a.ok("messages.list", J(R"({"conv":"dm:00000000000000ff"})")).get("messages").size() == 0);

  CHECK(a.ok("messages.retry", Json::object({{"id", m.get("id")}})).get("message").get_string("id") == m.get_string("id"));
  CHECK(a.err("messages.retry", Json::object({{"id", kNoSuchMsg}})) == "not_found");
  CHECK(a.err("messages.retry", J(R"({"id":12})")) == "bad_params");
  a.ok("messages.mark_read", Json::object({{"conv", dm}}));
  CHECK(a.err("messages.mark_read", J(R"({"conv":"nope"})")) == "bad_params");

  Json s = a.ok("messages.search", J(R"({"query":"HELLO"})"));
  REQUIRE(s.get("messages").size() == 1);
  CHECK(s.get("messages")[0].get_string("text") == "public hello");
  CHECK(a.ok("messages.search", J(R"({"query":"  "})")).get("messages").size() == 0);
  CHECK(a.err("messages.search", J(R"({})")) == "bad_params");

  // files
  Json f = a.ok("files.send", Json::object({{"conv", dm}, {"name", "note.txt"}, {"mime", "text/plain"}, {"data_b64", "aGVsbG8gZmlsZQ=="}}))
               .get("message");
  check_message(f);
  CHECK(f.get_string("kind") == "file");
  has_keys(f.get("file"), {"name", "mime", "size", "complete"});
  CHECK(f.get("file").get_int("size") == 10);
  Json fg = a.ok("files.get", Json::object({{"id", f.get("id")}}));
  CHECK(fg.get_string("data_b64") == "aGVsbG8gZmlsZQ==");
  CHECK(fg.get_string("name") == "note.txt");
  CHECK(fg.get_string("mime") == "text/plain");
  CHECK(a.err("files.get", Json::object({{"id", kNoSuchMsg}})) == "not_found");
  CHECK(a.err("files.send", Json::object({{"conv", dm}, {"name", "x"}, {"data_b64", "!!!"}})) == "bad_params");
  CHECK(a.err("files.send", Json::object({{"conv", dm}, {"name", "x"}, {"data_b64", ""}})) == "bad_params");
  std::string big((129 * 1024 + 2) / 3 * 4, 'A');
  CHECK(a.err("files.send", Json::object({{"conv", dm}, {"name", "x"}, {"data_b64", big}})) == "too_large");

  // interop: no such link; a raw link cannot send plain text
  CHECK(a.err("interop.send", J(R"({"link_id":99,"text":"cq"})")) == "not_found");
  uint32_t lid = static_cast<uint32_t>(a.ok("links.add", J(R"({"type":"raw","config":{}})")).get("link").get_int("id"));
  CHECK(a.err("interop.send", Json::object({{"link_id", lid}, {"text", "cq"}})) == "unsupported");
  CHECK(a.err("interop.send", Json::object({{"link_id", lid}})) == "bad_params");

  a.ok("messages.delete", Json::object({{"id", m.get("id")}}));
  CHECK(a.err("messages.delete", Json::object({{"id", m.get("id")}})) == "not_found");
  a.ok("conversations.clear", J(R"({"conv":"ch:public"})"));
  CHECK(a.ok("messages.list", J(R"({"conv":"ch:public"})")).get("messages").size() == 0);
  CHECK(a.err("conversations.clear", J(R"({"conv":"dm:zz"})")) == "bad_params");
}

TEST_CASE("rpc: emergency features") {
  TNode a("", 11);
  Json s = a.ok("sos.start", J(R"({"text":"trapped","interval_s":30,"position":{"lat":40.5,"lon":-73.25,"alt":12,"accuracy":5,"ts":1}})"));
  CHECK(s.get_bool("active"));
  CHECK(s.get("since_ts").is_int());
  Json st = a.ok("sos.status");
  has_keys(st, {"active", "since_ts", "text", "sent", "acked_by"});
  CHECK(st.get_string("text") == "trapped");
  CHECK(a.err("sos.start", J(R"({"interval_s":2})")) == "bad_params");
  CHECK(a.err("sos.start", J(R"({"position":{"lat":100,"lon":0}})")) == "bad_params");
  CHECK(a.err("sos.start", J(R"({"position":"here"})")) == "bad_params");
  CHECK_FALSE(a.ok("sos.stop").get_bool("active"));
  CHECK_FALSE(a.ok("sos.status").get_bool("active"));
  CHECK(a.ok("sos.list").get("alerts").is_array());
  CHECK(a.err("sos.ack", Json::object({{"node_id", kNoSuchNode}})) == "not_found");
  CHECK(a.err("sos.ack", J(R"({})")) == "bad_params");

  a.ok("checkin.send", J(R"({"status":"need_help","note":"out of water"})"));
  Json cl = a.ok("checkin.list");
  REQUIRE(cl.get("checkins").size() == 1);
  has_keys(cl.get("checkins")[0], {"node_id", "name", "status", "note", "ts"});
  CHECK(a.err("checkin.send", J(R"({"status":"fine"})")) == "bad_params");
  CHECK(a.err("checkin.send", Json::object({{"status", "ok"}, {"note", std::string(600, 'n')}})) == "too_large");

  CHECK_FALSE(a.ok("position.get").has("position"));
  CHECK(a.err("position.share", J(R"({"conv":"ch:public"})")) == "not_found");
  a.ok("position.set", J(R"({"lat":51.5,"lon":-0.12,"accuracy":8})"));
  Json pg = a.ok("position.get").get("position");
  has_keys(pg, {"lat", "lon", "ts"});
  CHECK(pg.get_double("lat") == doctest::Approx(51.5));
  CHECK(a.err("position.set", J(R"({"lat":"51","lon":0})")) == "bad_params");
  CHECK(a.err("position.set", J(R"({"lat":0,"lon":181})")) == "bad_params");
  Json ps = a.ok("position.share", J(R"({"conv":"ch:public"})")).get("message");
  check_message(ps);
  CHECK(ps.get_string("kind") == "position");
  CHECK(ps.get("position").is_object());

  Json b = a.ok("bulletins.post", J(R"({"category":"water","title":"Clean well","text":"Behind the school","expires_s":3600,"position":{"lat":1,"lon":2}})"))
               .get("bulletin");
  has_keys(b, {"id", "from", "from_name", "ts", "expires_ts", "category", "title", "text", "mine"});
  CHECK(b.get_bool("mine"));
  CHECK(b.get_int("expires_ts") - b.get_int("ts") == 3600000);
  CHECK(a.ok("bulletins.list").get("bulletins").size() == 1);
  CHECK(a.ok("bulletins.list", J(R"({"category":"food"})")).get("bulletins").size() == 0);
  CHECK(a.err("bulletins.list", J(R"({"category":"gossip"})")) == "bad_params");
  CHECK(a.err("bulletins.post", J(R"({"category":"gossip","title":"x"})")) == "bad_params");
  CHECK(a.err("bulletins.post", J(R"({"category":"info","title":""})")) == "bad_params");
  CHECK(a.err("bulletins.post", J(R"({"category":"info","title":"t","expires_s":5})")) == "bad_params");
  CHECK(a.err("bulletins.post", Json::object({{"category", "info"}, {"title", "t"}, {"text", std::string(1600, 'x')}})) == "too_large");
  CHECK(a.err("bulletins.delete", Json::object({{"id", kNoSuchMsg}})) == "not_found");
  a.ok("bulletins.delete", Json::object({{"id", b.get("id")}}));
  CHECK(a.ok("bulletins.list").get("bulletins").size() == 0);
}

TEST_CASE("rpc: mesh / netplan / bundles") {
  TNode a("", 12);
  CHECK(a.ok("mesh.peers").get("peers").is_array());
  Json st = a.ok("mesh.stats");
  has_keys(st, {"stored_messages", "stored_bytes", "relayed", "dropped_duplicates", "dropped_ttl", "tx_frames",
                "rx_frames", "rx_bad", "uptime_ms", "per_link"});
  // A node whose keys are not known yet can still be pinged (the engine floods the ping).
  CHECK(a.ok("mesh.ping", Json::object({{"node_id", kNoSuchNode}})).get_bool("sent"));
  CHECK(a.err("mesh.ping", Json::object({{"node_id", a.id}})) == "bad_params");
  CHECK(a.err("mesh.ping", J(R"({"node_id":"xyz"})")) == "bad_params");
  a.ok("mesh.announce");

  Json np = a.ok("netplan.set", J(R"({"entries":[{"label":"Camp net","freq_hz":146520000,"mode":"voice","schedule":"hourly"}]})"));
  has_keys(np.get("plan"), {"from", "from_name", "ts", "entries"});
  CHECK(np.get("plan").get("entries")[0].get_int("freq_hz") == 146520000);
  Json g = a.ok("netplan.get");
  CHECK(g.has("plan"));
  CHECK(g.get("heard").is_array());
  CHECK(a.err("netplan.set", J(R"({"entries":[{"label":"x"}]})")) == "bad_params");
  CHECK(a.err("netplan.set", J(R"({"entries":"none"})")) == "bad_params");
  Json many = Json::object({{"entries", Json::array()}});
  for (int i = 0; i < 17; ++i) many.at_key("entries").push(Json::object({{"freq_hz", 1000 + i}}));
  CHECK(a.err("netplan.set", many) == "bad_params");
  a.ok("netplan.set", J(R"({"entries":[]})"));
  CHECK_FALSE(a.ok("netplan.get").has("plan"));
  // advertised modes == modem catalogue names + voice etc.; old spellings normalised on input
  {
    Json modes = a.ok("netplan.get").get("modes");
    std::set<std::string> ms;
    for (const Json& m : modes.items()) ms.insert(m.as_string());
    for (const char* m : {"voice", "afsk1200", "afsk300", "psk2400", "psk4800", "robust250", "robust125", "robust62",
                          "robust31", "cw", "rtty", "psk31", "dtmf", "lora", "meshtastic", "other"})
      CHECK_MESSAGE(ms.count(m), m);
    CHECK_FALSE(ms.count("bpsk31"));
    Json pl = a.ok("netplan.set", J(R"({"entries":[{"freq_hz":7070000,"mode":"bpsk31"},{"freq_hz":7071000,"mode":"PSK-31"},
        {"freq_hz":145500000,"mode":"Robust 62"},{"freq_hz":3560000,"mode":"Morse"},{"freq_hz":144390000,"mode":"AFSK1200"},
        {"freq_hz":146520000},{"freq_hz":28120000,"mode":"usb"}]})")).get("plan");
    const char* want[] = {"psk31", "psk31", "robust62", "cw", "afsk1200", "voice", "usb"};
    REQUIRE(pl.get("entries").size() == 7);
    for (size_t i = 0; i < 7; ++i) CHECK(pl.get("entries")[i].get_string("mode") == want[i]);
    a.ok("netplan.set", J(R"({"entries":[]})"));
  }

  a.ok("messages.send", J(R"({"conv":"ch:public","text":"for the bundle"})"));
  Json ex = a.ok("bundle.export", J(R"({"scope":"all"})"));
  has_keys(ex, {"data_b64", "count", "bytes"});
  CHECK(ex.get_int("count") >= 1);
  CHECK(a.ok("bundle.export", J(R"({"scope":"conv","conv":"ch:public"})")).get_int("count") >= 1);
  CHECK(a.ok("bundle.export", Json::object({{"scope", "for_node"}, {"node_id", kNoSuchNode}})).get_int("count") >= 1);
  CHECK(a.err("bundle.export", J(R"({"scope":"everything"})")) == "bad_params");
  CHECK(a.err("bundle.export", J(R"({"scope":"conv"})")) == "bad_params");
  CHECK(a.err("bundle.export", J(R"({"scope":"for_node","node_id":"x"})")) == "bad_params");
  Json im = a.ok("bundle.import", Json::object({{"data_b64", ex.get("data_b64")}}));
  has_keys(im, {"imported", "duplicates", "for_me"});
  CHECK(im.get_int("imported") == 0);
  CHECK(a.err("bundle.import", J(R"({"data_b64":"AAAA"})")) == "bad_params");
  CHECK(a.err("bundle.import", J(R"({"data_b64":"%%%"})")) == "bad_params");
  CHECK(a.err("bundle.import", J(R"({})")) == "bad_params");
}

TEST_CASE("rpc: links") {
  TNode a("", 13);
  Json t = a.ok("links.types");
  std::set<std::string> types;
  for (const Json& x : t.get("types").items()) {
    has_keys(x, {"type", "title", "description", "endpoint_kinds", "config"});
    types.insert(x.get_string("type"));
  }
  for (const char* k : {"raw", "stream", "kiss", "audio", "meshtastic", "rnode", "lora_at", "rtl_tcp"}) CHECK(types.count(k));
  Json l = a.ok("links.add", J(R"({"type":"raw","name":"LAN","config":{"preset":"udp_lan"},"endpoint":{"kind":"udp","config":{"port":47474},"host_key":"k1"}})"))
               .get("link");
  has_keys(l, {"id", "type", "name", "state", "endpoint", "config", "caps", "stats", "status"});
  has_keys(l.get("caps"), {"mtu", "bitrate_bps", "broadcast", "half_duplex", "reliable", "encryption_allowed",
                           "requires_station_id", "duty_cycle_permille", "cost"});
  has_keys(l.get("stats"), {"tx_frames", "rx_frames", "tx_bytes", "rx_bytes", "rx_errors", "airtime_ms", "last_rx_ts", "queue"});
  CHECK(l.get_string("state") == "down");
  CHECK(l.get("endpoint").get_string("host_key") == "k1");
  CHECK(l.get("config").get_string("preset") == "udp_lan");
  int64_t id = l.get_int("id");
  CHECK(id >= 1);
  a.ok("links.endpoint_state", Json::object({{"id", id}, {"connected", true}}));
  a.drain();
  CHECK(a.saw("link.state", [&](const Json& d) { return d.get("link").get_string("state") == "up"; }));
  a.ok("links.endpoint_state", Json::object({{"id", id}, {"connected", false}, {"error", "cable unplugged"}}));
  Json ll = a.ok("links.list").get("links");
  REQUIRE(ll.size() == 1);
  CHECK(ll[0].get_string("state") == "error");
  CHECK(ll[0].get_string("error") == "cable unplugged");
  Json c = a.ok("links.configure", Json::object({{"id", id}, {"name", "Wi-Fi"}, {"config", Json::object({{"cost", 3}})}})).get("link");
  CHECK(c.get_string("name") == "Wi-Fi");
  CHECK(c.get("caps").get_int("cost") == 3);
  CHECK(a.err("links.configure", Json::object({{"id", id}, {"config", Json::object({{"mtu", 5}})}})) == "bad_params");
  CHECK(a.err("links.configure", J(R"({"id":999})")) == "not_found");
  CHECK(a.err("links.add", J(R"({"type":"warp"})")) == "bad_params");
  CHECK(a.err("links.add", J(R"({"type":"raw","config":{"preset":"nope"}})")) == "bad_params");
  CHECK(a.err("links.add", J(R"({"type":"raw","config":[]})")) == "bad_params");
  CHECK(a.err("links.add", J(R"({})")) == "bad_params");
  CHECK(a.err("links.endpoint_state", Json::object({{"id", id}})) == "bad_params");
  CHECK(a.err("links.endpoint_state", J(R"({"id":999,"connected":true})")) == "not_found");
  for (const char* k : {"stream", "kiss", "audio", "meshtastic", "rnode", "lora_at", "rtl_tcp"}) {
    INFO(k);
    a.ok("links.add", Json::object({{"type", k}}));
  }
  a.ok("links.remove", Json::object({{"id", id}}));
  CHECK(a.err("links.remove", Json::object({{"id", id}})) == "not_found");
  CHECK(a.err("links.remove", J(R"({"id":"one"})")) == "bad_params");
}

TEST_CASE("rpc: LoRa link defaults follow the region setting; explicit configs win") {
  TNode a("", 14);
  auto region_default = [&](const char* type) {
    Json types = a.ok("links.types");
    for (const Json& x : types.get("types").items()) {
      if (x.get_string("type") != type) continue;
      for (const Json& f : x.get("config").items())
        if (f.get_string("key") == "region") return f.get_string("default");
    }
    return std::string("?");
  };
  // default setting region "US"
  CHECK(region_default("rnode") == "us915");
  CHECK(region_default("lora_at") == "us915");
  struct Case { const char* region; const char* lora; };
  const Case cases[] = {{"EU", "eu868"}, {"UK", "eu868"}, {"de", "eu868"}, {"ZA", "eu868"}, {"AU", "au915"},
                        {"NZ", "au915"}, {"BR", "au915"}, {"IN", "in865"}, {"JP", "as923"}, {"US", "us915"},
                        {"OTHER", "eu868"} /* unknown: driver default unchanged */};
  for (const Case& c : cases) {
    CAPTURE(c.region);
    a.ok("settings.set", Json::object({{"settings", Json::object({{"region", c.region}})}}));
    CHECK(region_default("rnode") == c.lora);
    CHECK(region_default("lora_at") == c.lora);
    Json l = a.ok("links.add", J(R"({"type":"rnode","persist":false})")).get("link");
    CHECK(l.get("config").get_string("region") == c.lora);
    CHECK(l.get("status").get_string("region") == c.lora);
    a.ok("links.remove", Json::object({{"id", l.get_int("id")}}));
  }
  // meshtastic's own "region" (auto / device codes) is not touched
  a.ok("settings.set", J(R"({"settings":{"region":"AU"}})"));
  CHECK(region_default("meshtastic") != "au915");
  // explicit region is kept
  Json l = a.ok("links.add", J(R"({"type":"lora_at","config":{"region":"eu433"},"persist":false})")).get("link");
  CHECK(l.get("config").get_string("region") == "eu433");
  // an explicit frequency for the driver default region still works (old configs without region)
  l = a.ok("links.add", J(R"({"type":"rnode","config":{"frequency_hz":869525000},"persist":false})")).get("link");
  CHECK(l.get("status").get_string("region") == "eu868");
  // the region is stored with the link: changing the setting later does not retune it
  l = a.ok("links.add", J(R"({"type":"lora_at","persist":false})")).get("link");
  CHECK(l.get("config").get_string("region") == "au915");
  a.ok("settings.set", J(R"({"settings":{"region":"EU"}})"));
  bool found = false;
  Json list = a.ok("links.list");
  for (const Json& x : list.get("links").items())
    if (x.get_int("id") == l.get_int("id")) {
      found = true;
      CHECK(x.get("config").get_string("region") == "au915");
    }
  CHECK(found);
}

TEST_CASE("rpc: settings / kv / security status") {
  TNode a("", 14);
  Json s = a.ok("settings.get").get("settings");
  has_keys(s, {"region", "relay_enabled", "store_forward_days", "beacon_interval_s", "low_power", "position_sharing",
               "default_ttl_hops", "units", "theme", "links_autostart"});
  Json n = a.ok("settings.set", J(R"({"settings":{"region":"eu","relay_enabled":false,"default_ttl_hops":12,"low_power":true,"ui_font":"large"}})"))
               .get("settings");
  CHECK(n.get_string("region") == "EU");
  CHECK_FALSE(n.get_bool("relay_enabled"));
  CHECK(n.get_int("default_ttl_hops") == 12);
  CHECK(n.get_string("ui_font") == "large");
  CHECK(a.err("settings.set", J(R"({"settings":{"default_ttl_hops":40}})")) == "bad_params");
  CHECK(a.err("settings.set", J(R"({"settings":{"relay_enabled":"no"}})")) == "bad_params");
  CHECK(a.err("settings.set", J(R"({"settings":{"theme":"pink"}})")) == "bad_params");
  CHECK(a.err("settings.set", J(R"({"settings":[]})")) == "bad_params");
  CHECK(a.err("settings.set", Json::object({{"settings", Json::object({{"blob", std::string(70000, 'x')}})}})) == "too_large");
  CHECK(a.ok("settings.get").get("settings").get_int("default_ttl_hops") == 12);  // failed sets change nothing

  CHECK(a.ok("kv.get", J(R"({"ns":"notes","key":"a"})")).get("value").is_null());
  a.ok("kv.set", J(R"({"ns":"notes","key":"a","value":{"text":"boil water","n":[1,2]}})"));
  a.ok("kv.set", J(R"({"ns":"notes","key":"b","value":null})"));
  a.ok("kv.set", J(R"({"ns":"other","key":"a","value":1})"));
  CHECK(a.ok("kv.get", J(R"({"ns":"notes","key":"a"})")).get("value").get_string("text") == "boil water");
  Json keys = a.ok("kv.list", J(R"({"ns":"notes"})")).get("keys");
  CHECK(keys.size() == 2);
  CHECK(a.ok("kv.list", J(R"({"ns":"notes","prefix":"b"})")).get("keys").size() == 1);
  a.ok("kv.delete", J(R"({"ns":"notes","key":"a"})"));
  CHECK(a.ok("kv.get", J(R"({"ns":"notes","key":"a"})")).get("value").is_null());
  CHECK(a.err("kv.set", J(R"({"ns":"notes","key":"a"})")) == "bad_params");
  CHECK(a.err("kv.set", J(R"({"ns":"","key":"a","value":1})")) == "bad_params");
  CHECK(a.err("kv.get", J(R"({"ns":"x"})")) == "bad_params");
  CHECK(a.err("kv.set", Json::object({{"ns", "n"}, {"key", "k"}, {"value", std::string(300 * 1024, 'v')}})) == "too_large");
  a.drain();
  CHECK(a.saw("storage.dirty"));

  Json sec = a.ok("security.status");
  has_keys(sec, {"locked", "has_passphrase", "has_duress"});
  CHECK_FALSE(sec.get_bool("has_passphrase"));
  CHECK(a.err("security.lock") == "bad_params");
  CHECK(a.err("security.set_duress", J(R"({"passphrase":"x"})")) == "bad_params");
  CHECK(a.err("security.wipe", J(R"({"confirm":"yes"})")) == "bad_params");
  CHECK(a.ok("security.unlock", J(R"({"passphrase":"anything"})")).get_bool("unlocked"));
  CHECK(a.err("security.set_passphrase", J(R"({})")) == "bad_params");
}

TEST_CASE("rpc: params must be a JSON object; garbage is rejected cleanly") {
  TNode a("", 15);
  for (const char* bad : {"[]", "1", "\"x\"", "null", "{", "{\"a\":}", "{\"a\":1,\"a\":2}", "\xff\xfe"}) {
    char* r = ember_call(a.n, "node.info", bad);
    REQUIRE(r);
    std::optional<Json> j = Json::parse(r);
    ember_free(r);
    REQUIRE(j);
    CHECK_FALSE(j->get_bool("ok"));
    CHECK(j->get("error").get_string("code") == "bad_params");
  }
  char* r = ember_call(a.n, "node.info", "  ");
  CHECK(Json::parse(r)->get_bool("ok"));
  ember_free(r);
  r = ember_call(a.n, "node.info", nullptr);
  CHECK(Json::parse(r)->get_bool("ok"));
  ember_free(r);
  // Unknown params are ignored.
  CHECK(a.call("node.info", J(R"({"whatever":[1,2,3]})")).get_bool("ok"));
}

}  // TEST_SUITE
