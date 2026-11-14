// Node: multi-node scenarios driven only through the C API over the in-test network.
#include <map>

#include "base/codec.h"
#include "framing/ax25.h"
#include "framing/kiss.h"
#include "node/node_harness.h"
#include "proto/packet.h"

using namespace nt;

namespace {

// Reassembles mesh frames seen on the air into packets so tests can inspect their security mode.
struct AirInspector {
  std::map<std::pair<uint32_t, uint16_t>, std::map<uint8_t, ember::Bytes>> frags;
  std::vector<ember::Bytes> packets;
  std::vector<std::string> stations;
  size_t frames = 0, unparsed = 0;
  void feed(ember::ByteView frame) {
    frames++;
    ember::proto::FrameHeader h;
    ember::ByteView body;
    if (!ember::proto::parse_frame(frame, h, body)) {
      unparsed++;
      return;
    }
    stations.push_back(h.station);
    if (h.type == ember::proto::FrameType::Packet) {
      packets.push_back(body.to_bytes());
      return;
    }
    if (h.type != ember::proto::FrameType::Fragment) return;
    ember::proto::Fragment f;
    if (!ember::proto::parse_fragment(body, f)) return;
    auto& m = frags[{h.hop_src, f.frag_id}];
    m[f.index] = f.data.to_bytes();
    if (m.size() == f.count) {
      ember::Bytes unit;
      for (auto& kv : m) unit.insert(unit.end(), kv.second.begin(), kv.second.end());
      if (!unit.empty() && unit[0] == 0) packets.push_back(ember::Bytes(unit.begin() + 1, unit.end()));
      frags.erase({h.hop_src, f.frag_id});
    }
  }
};

// KISS byte stream -> AX.25 UI frames -> mesh frames.
struct KissTap {
  ember::kiss::Decoder dec{4096};
  AirInspector air;
  void feed(const std::string& bytes) {
    dec.feed(ember::ByteView(std::string_view(bytes)), [&](const ember::kiss::Frame& f) {
      ember::ax25::Frame af;
      if (ember::ax25::decode(f.payload, &af)) air.feed(ember::ByteView(af.info));
    });
  }
};

bool conv_has(TNode& n, const std::string& conv, const std::string& text) { return n.has_text(conv, text); }

}  // namespace

TEST_SUITE("node") {

TEST_CASE("mesh: DM delivery with delivered and read receipts") {
  Net net(1);
  int lan = net.add_medium();
  TNode a("", 1), b("", 2);
  a.ok("node.set_profile", J(R"({"name":"Alice"})"));
  b.ok("node.set_profile", J(R"({"name":"Bob"})"));
  net.join(a, lan);
  net.join(b, lan);
  introduce(a, b);
  net.run(1000);
  Json m = a.ok("messages.send", Json::object({{"conv", "dm:" + b.id}, {"text", "hello bob, meet at the well"}}))
               .get("message");
  CHECK(m.get_bool("encrypted"));
  std::string mid = m.get_string("id");
  REQUIRE(net.run_until([&] { return b.has_text("dm:" + a.id, "hello bob, meet at the well"); }, 5000));
  Json got = b.find_message("dm:" + a.id, mid);
  CHECK(got.get_string("status") == "received");
  CHECK(got.get_bool("encrypted"));
  CHECK(got.get_bool("signed_ok"));
  CHECK(got.get_string("from") == a.id);
  CHECK(got.get_string("from_name") == "Alice");
  CHECK(got.get_int("hops") == 0);
  CHECK(got.get_int("via_link") >= 1);
  CHECK(b.saw("message.new", [&](const Json& d) { return d.get("message").get_string("id") == mid; }));
  CHECK(b.saw("conversation.updated"));
  CHECK(b.saw("peer.seen"));
  REQUIRE(net.run_until([&] { return a.find_message("dm:" + b.id, mid).get_string("status") == "delivered"; }, 10000));
  CHECK(a.saw("message.status", [&](const Json& d) { return d.get_string("id") == mid && d.get_string("status") == "delivered"; }));
  bool found = false;
  for (const Json& c : b.items("conversations.list", "conversations"))
    if (c.get_string("id") == "dm:" + a.id) {
      found = true;
      CHECK(c.get_int("unread") == 1);
      CHECK(c.get_string("title") == "Alice");
      CHECK(c.get("last_message").get_string("id") == mid);
    }
  CHECK(found);
  b.ok("messages.mark_read", Json::object({{"conv", "dm:" + a.id}}));
  REQUIRE(net.run_until([&] { return a.find_message("dm:" + b.id, mid).get_string("status") == "read"; }, 10000));
  // Status never regresses and the message is delivered exactly once.
  net.run(30000, 100);
  CHECK(a.find_message("dm:" + b.id, mid).get_string("status") == "read");
  int copies = 0;
  for (auto& x : b.messages("dm:" + a.id)) copies += x.get_string("id") == mid;
  CHECK(copies == 1);
  // Live contact fields come from the peer table.
  Json c = a.ok("contacts.list").get("contacts")[0];
  CHECK(c.get_bool("direct"));
  CHECK(c.get_int("hops") == 0);
  CHECK(c.get_int("last_seen_ts") > 0);
  CHECK(c.get_string("name") == "Bob");
}

TEST_CASE("mesh: DM to a node whose keys are unknown waits for the key (WHOIS)") {
  Net net(2);
  int lan = net.add_medium();
  TNode a("", 3), b("", 4);
  net.join(a, lan);
  net.join(b, lan);
  Json m = a.ok("messages.send", Json::object({{"conv", "dm:" + b.id}, {"text", "are you there?"}})).get("message");
  CHECK(m.get_string("status") == "queued");
  REQUIRE(net.run_until([&] { return b.has_text("dm:" + a.id, "are you there?"); }, 30000));
  REQUIRE(net.run_until([&] { return a.find_message("dm:" + b.id, m.get_string("id")).get_string("status") == "delivered"; }, 30000));
}

TEST_CASE("mesh: Public, open and passphrase channels are isolated") {
  Net net(3);
  int lan = net.add_medium();
  TNode a("", 5), b("", 6), c("", 7);
  net.join(a, lan);
  net.join(b, lan);
  net.join(c, lan);
  net.run(1000);
  a.ok("messages.send", J(R"({"conv":"ch:public","text":"public hello"})"));
  REQUIRE(net.run_until([&] { return conv_has(b, "ch:public", "public hello") && conv_has(c, "ch:public", "public hello"); }, 5000));
  Json pm = b.messages("ch:public").back();
  CHECK_FALSE(pm.get_bool("encrypted"));
  CHECK(pm.get_bool("signed_ok"));

  Json camp = a.ok("channels.create", J(R"({"name":"Camp","passphrase":"river stones"})")).get("channel");
  CHECK(camp.get_bool("encrypted"));
  std::string inv = a.ok("channels.invite", Json::object({{"id", camp.get("id")}})).get_string("invite");
  CHECK(b.ok("channels.join", Json::object({{"invite", inv}})).get("channel").get_string("id") == camp.get_string("id"));
  std::string conv = "ch:" + camp.get_string("id");
  a.ok("messages.send", Json::object({{"conv", conv}, {"text", "members only"}}));
  REQUIRE(net.run_until([&] { return conv_has(b, conv, "members only"); }, 5000));
  Json bm = b.messages(conv).back();
  CHECK(bm.get_bool("encrypted"));
  CHECK(bm.get_bool("signed_ok"));
  net.run(3000);
  // c relays but holds no key: no such conversation and no plaintext anywhere.
  CHECK(c.err("messages.list", Json::object({{"conv", conv}})) == "not_found");
  CHECK(c.ok("messages.search", J(R"({"query":"members only"})")).get("messages").size() == 0);
  // A wrong passphrase gives a different channel.
  Json wrong = c.ok("channels.join", J(R"({"name":"Camp","passphrase":"wrong"})")).get("channel");
  CHECK(wrong.get_string("id") != camp.get_string("id"));
  // members_seen counts distinct senders.
  b.ok("messages.send", Json::object({{"conv", conv}, {"text", "ack from b"}}));
  REQUIRE(net.run_until([&] { return conv_has(a, conv, "ack from b"); }, 5000));
  CHECK(a.ok("channels.list").get("channels")[1].get_int("members_seen") >= 2);
  // Open channel: only joined nodes see it.
  Json mk = a.ok("channels.create", J(R"({"name":"Market"})")).get("channel");
  c.ok("channels.join", J(R"({"name":"market"})"));
  a.ok("messages.send", Json::object({{"conv", "ch:" + mk.get_string("id")}, {"text", "selling eggs"}}));
  REQUIRE(net.run_until([&] { return conv_has(c, "ch:" + mk.get_string("id"), "selling eggs"); }, 5000));
  CHECK_FALSE(c.messages("ch:" + mk.get_string("id")).back().get_bool("encrypted"));
  net.run(2000);
  CHECK(b.ok("messages.search", J(R"({"query":"selling eggs"})")).get("messages").size() == 0);
  // Unread counts and channel.updated events.
  CHECK(c.saw("channel.updated"));
  bool unread = false;
  for (const Json& ch : c.items("channels.list", "channels"))
    if (ch.get_string("id") == mk.get_string("id")) unread = ch.get_int("unread") == 1;
  CHECK(unread);
}

TEST_CASE("mesh: 3-hop relay along a line") {
  Net net(4);
  int l1 = net.add_medium(), l2 = net.add_medium(), l3 = net.add_medium();
  TNode a("", 8), m1("", 9), m2("", 10), b("", 11);
  net.join(a, l1);
  net.join(m1, l1);
  net.join(m1, l2);
  net.join(m2, l2);
  net.join(m2, l3);
  net.join(b, l3);
  introduce(a, b);
  net.run(2000);
  Json m = a.ok("messages.send", Json::object({{"conv", "dm:" + b.id}, {"text", "three hops away"}})).get("message");
  REQUIRE(net.run_until([&] { return b.has_text("dm:" + a.id, "three hops away"); }, 20000));
  CHECK(b.find_message("dm:" + a.id, m.get_string("id")).get_int("hops") == 2);
  REQUIRE(net.run_until([&] { return a.find_message("dm:" + b.id, m.get_string("id")).get_string("status") == "delivered"; }, 30000));
  CHECK(a.saw("message.status", [&](const Json& d) { return d.get_string("status") == "sent" || d.get_string("status") == "relayed"; }));
  // Relays never saw the plaintext.
  CHECK(m1.ok("messages.search", J(R"({"query":"three hops"})")).get("messages").size() == 0);
  CHECK(m1.ok("mesh.stats").get_int("relayed") > 0);
  // Peers table: b is known 2 hops away.
  bool seen = false;
  for (const Json& p : a.items("mesh.peers", "peers"))
    if (p.get_string("node_id") == b.id) seen = true;
  CHECK(seen);
}

TEST_CASE("mesh: store-carry-forward across a partition") {
  Net net(5);
  int l1 = net.add_medium(), l2 = net.add_medium();
  TNode a("", 12), m("", 13), b("", 14);
  net.join(a, l1);
  net.join(m, l1);
  net.join(m, l2);
  net.join(b, l2);
  introduce(a, b);
  net.run(2000);
  net.isolate(b);  // b walks away
  Json msg = a.ok("messages.send", Json::object({{"conv", "dm:" + b.id}, {"text", "carried by the mule"}})).get("message");
  net.run(5000);
  CHECK_FALSE(b.has_text("dm:" + a.id, "carried by the mule"));
  net.isolate(a);  // a goes off air; only m holds a copy
  net.isolate(b, false);
  REQUIRE(net.run_until([&] { return b.has_text("dm:" + a.id, "carried by the mule"); }, 180000, 50));
  net.isolate(a, false);
  REQUIRE(net.run_until([&] { return a.find_message("dm:" + b.id, msg.get_string("id")).get_string("status") == "delivered"; }, 240000, 50));
}

TEST_CASE("mesh: SOS beacon propagates, repeats, is acknowledged and cancelled") {
  Net net(6);
  int lan = net.add_medium();
  TNode a("", 15), b("", 16);
  a.ok("node.set_profile", J(R"({"name":"Ann"})"));
  net.join(a, lan);
  net.join(b, lan);
  introduce(a, b);
  net.run(1000);
  a.ok("sos.start", J(R"({"text":"trapped under rubble","interval_s":10,"position":{"lat":40.1,"lon":-70.2}})"));
  REQUIRE(net.run_until([&] { return b.count("sos.received") >= 1; }, 5000));
  Json ev = b.last("sos.received")->data;
  CHECK(ev.get_string("node_id") == a.id);
  CHECK(ev.get_string("text") == "trapped under rubble");
  CHECK(ev.get("position").get_double("lat") == doctest::Approx(40.1));
  CHECK(conv_has(b, "ch:public", "trapped under rubble"));
  CHECK(b.messages("ch:public").back().get_string("kind") == "sos");
  REQUIRE(net.run_until([&] { return b.count("sos.received") >= 2; }, 25000));  // repeating beacon
  Json alerts = b.ok("sos.list").get("alerts");
  REQUIRE(alerts.size() == 1);
  CHECK(alerts[0].get_string("name") == "Ann");
  CHECK_FALSE(alerts[0].get_bool("acked"));
  b.ok("sos.ack", Json::object({{"node_id", a.id}}));
  CHECK(b.ok("sos.list").get("alerts")[0].get_bool("acked"));
  REQUIRE(net.run_until([&] { return a.ok("sos.status").get("acked_by").size() == 1; }, 10000));
  CHECK(a.ok("sos.status").get("acked_by")[0].get_string("node_id") == b.id);
  CHECK(a.saw("sos.updated"));
  CHECK(a.ok("sos.status").get_int("sent") >= 2);
  a.ok("sos.stop");
  REQUIRE(net.run_until([&] { return b.ok("sos.list").get("alerts").size() == 0; }, 10000));
  size_t n = b.count("sos.received");
  net.run(30000, 100);
  CHECK(b.count("sos.received") == n);
}

TEST_CASE("mesh: check-ins, position sharing, ping and net plans") {
  Net net(7);
  int lan = net.add_medium();
  TNode a("", 17), b("", 18);
  net.join(a, lan);
  net.join(b, lan);
  introduce(a, b);
  net.run(1000);
  a.ok("checkin.send", J(R"({"status":"injured","note":"broken ankle","position":{"lat":10.5,"lon":20.25}})"));
  REQUIRE(net.run_until([&] { return b.count("checkin.received") >= 1; }, 5000));
  Json ci = b.last("checkin.received")->data;
  CHECK(ci.get_string("status") == "injured");
  CHECK(ci.get_string("note") == "broken ankle");
  CHECK(ci.get("position").get_double("lon") == doctest::Approx(20.25));
  Json list = b.ok("checkin.list").get("checkins");
  REQUIRE(list.size() == 1);
  CHECK(list[0].get_string("node_id") == a.id);

  a.ok("position.set", J(R"({"lat":-33.86,"lon":151.2})"));
  Json pm = a.ok("position.share", Json::object({{"conv", "dm:" + b.id}})).get("message");
  REQUIRE(net.run_until([&] { return b.find_message("dm:" + a.id, pm.get_string("id")).is_object(); }, 5000));
  Json got = b.find_message("dm:" + a.id, pm.get_string("id"));
  CHECK(got.get_string("kind") == "position");
  CHECK(got.get("position").get_double("lat") == doctest::Approx(-33.86));

  a.ok("mesh.ping", Json::object({{"node_id", b.id}}));
  REQUIRE(net.run_until([&] { return a.count("mesh.pong") >= 1; }, 5000));
  CHECK(a.last("mesh.pong")->data.get_string("node_id") == b.id);
  CHECK(a.last("mesh.pong")->data.get("rtt_ms").is_int());

  a.ok("netplan.set", J(R"({"entries":[{"label":"Camp net","freq_hz":146520000,"mode":"voice","tone":"100.0"}]})"));
  REQUIRE(net.run_until([&] { return b.count("netplan.heard") >= 1; }, 5000));
  Json heard = b.ok("netplan.get").get("heard");
  REQUIRE(heard.size() == 1);
  CHECK(heard[0].get_string("from") == a.id);
  CHECK(heard[0].get("entries")[0].get_string("tone") == "100.0");
}

TEST_CASE("mesh: bulletins flood, can be withdrawn and expire") {
  Net net(8);
  int lan = net.add_medium();
  TNode a("", 19), b("", 20);
  net.join(a, lan);
  net.join(b, lan);
  net.run(1000);
  Json keep = a.ok("bulletins.post", J(R"({"category":"water","title":"Clean well","text":"Behind the school","expires_s":120})")).get("bulletin");
  Json gone = a.ok("bulletins.post", J(R"({"category":"hazard","title":"Bridge out","expires_s":3600})")).get("bulletin");
  REQUIRE(net.run_until([&] { return b.ok("bulletins.list").get("bulletins").size() == 2; }, 5000));
  CHECK(b.count("bulletin.new") == 2);
  Json bl = b.ok("bulletins.list", J(R"({"category":"water"})")).get("bulletins");
  REQUIRE(bl.size() == 1);
  CHECK_FALSE(bl[0].get_bool("mine"));
  CHECK(bl[0].get_string("id") == keep.get_string("id"));
  CHECK(b.err("bulletins.delete", Json::object({{"id", gone.get("id")}})) == "denied");
  a.ok("bulletins.delete", Json::object({{"id", gone.get("id")}}));
  REQUIRE(net.run_until([&] { return b.ok("bulletins.list").get("bulletins").size() == 1; }, 10000));
  net.run(130000, 200);
  CHECK(b.ok("bulletins.list").get("bulletins").size() == 0);
  CHECK(a.ok("bulletins.list").get("bulletins").size() == 0);
}

TEST_CASE("mesh: small file transfer with progress") {
  Net net(9);
  int lan = net.add_medium();
  TNode a("", 21), b("", 22);
  net.join(a, lan);
  net.join(b, lan);
  introduce(a, b);
  net.run(1000);
  std::string raw(3000, '\0');
  for (size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<char>(i * 13);
  Json f = a.ok("files.send", Json::object({{"conv", "dm:" + b.id}, {"name", "map.bin"}, {"mime", "application/octet-stream"},
                                            {"data_b64", ember::base64_encode(ember::ByteView(std::string_view(raw)))}}))
               .get("message");
  std::string id = f.get_string("id");
  REQUIRE(net.run_until([&] { return b.find_message("dm:" + a.id, id).is_object(); }, 20000));
  Json got = b.find_message("dm:" + a.id, id);
  CHECK(got.get_string("kind") == "file");
  CHECK(got.get("file").get_int("size") == 3000);
  CHECK(got.get("file").get_bool("complete"));
  Json data = b.ok("files.get", Json::object({{"id", id}}));
  ember::Bytes back;
  REQUIRE(ember::base64_decode(data.get_string("data_b64"), back));
  CHECK(std::string(back.begin(), back.end()) == raw);
  CHECK(data.get_string("name") == "map.bin");
  CHECK(b.saw("file.progress", [&](const Json& d) { return d.get_string("id") == id && d.get_int("total") > 0; }));
  REQUIRE(net.run_until([&] { return a.find_message("dm:" + b.id, id).get_string("status") == "delivered"; }, 20000));
}

TEST_CASE("mesh: bundle export -> import on a node with no links (sneakernet)") {
  Net net(10);
  int lan = net.add_medium();
  TNode a("", 23), b("", 24), c("", 25);
  net.join(a, lan);
  net.join(b, lan);
  a.ok("contacts.add", Json::object({{"card", c.card()}}));
  c.ok("contacts.add", Json::object({{"card", a.card()}}));
  net.run(1000);
  a.ok("messages.send", Json::object({{"conv", "dm:" + c.id}, {"text", "carried on a usb stick"}}));
  a.ok("messages.send", J(R"({"conv":"ch:public","text":"public news"})"));
  a.ok("bulletins.post", J(R"({"category":"info","title":"water","text":"well is clean"})"));
  net.run(3000);
  Json ex = b.ok("bundle.export", J(R"({"scope":"all"})"));  // b carries a's traffic
  CHECK(ex.get_int("count") >= 3);
  CHECK(ex.get_int("bytes") > 0);
  Json im = c.ok("bundle.import", Json::object({{"data_b64", ex.get("data_b64")}}));
  CHECK(im.get_int("imported") == ex.get_int("count"));
  CHECK(im.get_int("for_me") >= 1);
  for (int i = 0; i < 20; ++i) {
    ember_tick(c.n, net.now + 20 * i);
    c.drain();
  }
  CHECK(c.has_text("dm:" + a.id, "carried on a usb stick"));
  CHECK(c.has_text("ch:public", "public news"));
  CHECK(c.messages("dm:" + a.id).back().get_int("via_link") == 0);
  CHECK(c.ok("bulletins.list").get("bulletins").size() == 1);
  Json again = c.ok("bundle.import", Json::object({{"data_b64", ex.get("data_b64")}}));
  CHECK(again.get_int("imported") == 0);
  CHECK(again.get_int("duplicates") == ex.get_int("count"));
  // A bundle only for c is smaller and still carries the DM.
  Json mine = b.ok("bundle.export", Json::object({{"scope", "for_node"}, {"node_id", c.id}}));
  CHECK(mine.get_int("count") >= 1);
  CHECK(b.ok("bundle.export", J(R"({"scope":"all","max_bytes":10})")).get_int("count") == 0);
}

TEST_CASE("mesh: blocked contacts are ignored") {
  Net net(11);
  int lan = net.add_medium();
  TNode a("", 26), b("", 27), c("", 28);
  net.join(a, lan);
  net.join(b, lan);
  net.join(c, lan);
  introduce(a, b);
  net.run(1000);
  a.ok("contacts.update", Json::object({{"node_id", b.id}, {"blocked", true}}));
  b.ok("messages.send", Json::object({{"conv", "dm:" + a.id}, {"text", "let me in"}}));
  b.ok("messages.send", J(R"({"conv":"ch:public","text":"spam from b"})"));
  REQUIRE(net.run_until([&] { return conv_has(c, "ch:public", "spam from b"); }, 5000));
  net.run(5000);
  CHECK_FALSE(a.has_text("dm:" + b.id, "let me in"));
  CHECK_FALSE(a.has_text("ch:public", "spam from b"));
  a.ok("contacts.update", Json::object({{"node_id", b.id}, {"blocked", false}}));
  b.ok("messages.send", J(R"({"conv":"ch:public","text":"unblocked now"})"));
  REQUIRE(net.run_until([&] { return conv_has(a, "ch:public", "unblocked now"); }, 5000));
}

TEST_CASE("mesh: legal-mode KISS link never carries ciphertext; Public traffic crosses") {
  Net net(12);
  int lan = net.add_medium();
  int radio = net.add_medium(0, 30, true);
  TNode a("", 29), gw("", 30), h("", 31);
  gw.ok("node.set_profile", J(R"({"callsign":"QX0GW"})"));
  h.ok("node.set_profile", J(R"({"callsign":"QX0HH"})"));
  net.join(a, lan);
  net.join(gw, lan);
  uint32_t gk = net.join(gw, radio, "kiss");  // defaults: amateur licence
  net.join(h, radio, "kiss");
  Json gl;
  for (const Json& l : gw.items("links.list", "links"))
    if (l.get_int("id") == gk) gl = l;
  CHECK_FALSE(gl.get("caps").get_bool("encryption_allowed"));
  CHECK(gl.get("caps").get_bool("requires_station_id"));
  CHECK(gl.get("status").is_object());
  std::map<std::pair<void*, uint32_t>, KissTap> taps;
  net.tx_hook = [&](const Net::Port& p, const std::string& bytes) {
    if (p.medium == radio) taps[{p.node, p.link}].feed(bytes);
  };
  introduce(a, h);
  Json camp = a.ok("channels.create", J(R"({"name":"Priv","passphrase":"pw"})")).get("channel");
  std::string inv = a.ok("channels.invite", Json::object({{"id", camp.get("id")}})).get_string("invite");
  gw.ok("channels.join", Json::object({{"invite", inv}}));
  h.ok("channels.join", Json::object({{"invite", inv}}));
  net.run(2000);
  a.ok("messages.send", Json::object({{"conv", "dm:" + h.id}, {"text", "secret dm must not cross"}}));
  a.ok("messages.send", Json::object({{"conv", "ch:" + camp.get_string("id")}, {"text", "private channel must not cross"}}));
  a.ok("messages.send", J(R"({"conv":"ch:public","text":"public may cross"})"));
  REQUIRE(net.run_until([&] { return h.has_text("ch:public", "public may cross"); }, 120000, 50));
  net.run(60000, 50);
  CHECK_FALSE(h.has_text("dm:" + a.id, "secret dm must not cross"));
  CHECK_FALSE(h.has_text("ch:" + camp.get_string("id"), "private channel must not cross"));
  size_t packets = 0, frames = 0;
  for (auto& kv : taps) {
    frames += kv.second.air.frames;
    for (const ember::Bytes& p : kv.second.air.packets) {
      ember::proto::PacketView pv;
      REQUIRE(ember::proto::parse_packet(p, pv));
      CHECK(ember::proto::legal_cleartext(pv.h.sec));
      packets++;
    }
    for (const std::string& st : kv.second.air.stations) CHECK((st == "QX0GW" || st == "QX0HH"));
  }
  CHECK(frames > 0);
  CHECK(packets > 0);
  uint64_t blocked = 0;
  for (const Json& l : gw.items("mesh.stats", "per_link")) blocked += static_cast<uint64_t>(l.get_int("blocked_legal"));
  CHECK(blocked > 0);
  MESSAGE("legal KISS link: " << frames << " frames, " << packets << " packets, all cleartext; " << blocked
                              << " ciphertext transmissions blocked at the gateway");
}

TEST_CASE("mesh: mixed driver chain raw <-> stream <-> kiss") {
  Net net(13);
  int lan = net.add_medium();
  int cable = net.add_medium(0, 10, true);
  int radio = net.add_medium(0, 30, true);
  TNode a("", 32), m1("", 33), m2("", 34), b("", 35);
  m2.ok("node.set_profile", J(R"({"callsign":"QX0MM"})"));
  b.ok("node.set_profile", J(R"({"callsign":"QX0BB"})"));
  net.join(a, lan, "raw");
  net.join(m1, lan, "raw");
  net.join(m1, cable, "stream");
  net.join(m2, cable, "stream");
  net.join(m2, radio, "kiss", J(R"({"licence":"licence_free"})"));
  net.join(b, radio, "kiss", J(R"({"licence":"licence_free"})"));
  introduce(a, b);
  net.run(3000);
  Json m = a.ok("messages.send", Json::object({{"conv", "dm:" + b.id}, {"text", "across three drivers"}})).get("message");
  REQUIRE(net.run_until([&] { return b.has_text("dm:" + a.id, "across three drivers"); }, 60000, 50));
  CHECK(b.find_message("dm:" + a.id, m.get_string("id")).get_int("hops") == 2);
  REQUIRE(net.run_until([&] { return a.find_message("dm:" + b.id, m.get_string("id")).get_string("status") == "delivered"; }, 120000, 50));
  b.ok("messages.send", J(R"({"conv":"ch:public","text":"reply over kiss stream raw"})"));
  REQUIRE(net.run_until([&] { return a.has_text("ch:public", "reply over kiss stream raw"); }, 60000, 50));
}

TEST_CASE("mesh: link events, activity and state polling") {
  Net net(14);
  int lan = net.add_medium();
  TNode a("", 36), b("", 37);
  uint32_t la = net.join(a, lan);
  net.join(b, lan);
  net.run(3000);
  a.ok("messages.send", J(R"({"conv":"ch:public","text":"activity please"})"));
  net.run(3000);
  CHECK(a.saw("link.activity", [&](const Json& d) { return d.get_int("link_id") == la && d.get_string("dir") == "tx" && d.get_int("bytes") > 0; }));
  CHECK(b.saw("link.activity", [&](const Json& d) { return d.get_string("dir") == "rx"; }));
  // Rate limited: far fewer activity events than frames.
  size_t act = a.count("link.activity");
  CHECK(act < a.tx_items);
  a.ok("links.endpoint_state", Json::object({{"id", la}, {"connected", false}}));
  a.drain();
  CHECK(a.last("link.state")->data.get("link").get_string("state") == "down");
}

TEST_CASE("mesh: APRS interop over KISS: io:aprs conversation, auto-ack, reply") {
  Net net(15);
  int radio = net.add_medium(0, 30, true);
  TNode a("", 38), b("", 39);
  a.ok("node.set_profile", J(R"({"callsign":"QX0AA"})"));
  b.ok("node.set_profile", J(R"({"callsign":"QX0BB"})"));
  uint32_t ka = net.join(a, radio, "kiss");
  net.join(b, radio, "kiss");
  net.run(1000);
  Json m = a.ok("interop.send", Json::object({{"link_id", ka}, {"to", "QX0BB"}, {"text", "hello aprs"}})).get("message");
  CHECK(m.get_string("kind") == "interop");
  CHECK(m.get_string("conv") == "io:aprs");
  CHECK(m.get("interop").get_string("network") == "aprs");
  CHECK(m.get("interop").get_string("to") == "QX0BB");
  REQUIRE(net.run_until([&] { return b.has_text("io:aprs", "hello aprs"); }, 30000, 50));
  Json got = b.messages("io:aprs").back();
  CHECK(got.get_string("kind") == "interop");
  CHECK(got.get("interop").get_string("from").find("QX0AA") == 0);
  CHECK_FALSE(got.get_bool("encrypted"));
  bool listed = false;
  for (const Json& c : b.items("conversations.list", "conversations"))
    if (c.get_string("id") == "io:aprs") {
      listed = true;
      CHECK(c.get_string("kind") == "interop");
      CHECK(c.get_string("title") == "APRS");
      CHECK(c.get_int("unread") == 1);
    }
  CHECK(listed);
  // b's TNC acked automatically -> a's message is delivered.
  REQUIRE(net.run_until([&] { return a.find_message("io:aprs", m.get_string("id")).get_string("status") == "delivered"; }, 60000, 50));
  CHECK(a.saw("link.aprs_ack"));
  // Reply inside the conversation goes back to the sender.
  b.ok("messages.send", J(R"({"conv":"io:aprs","text":"roger"})"));
  REQUIRE(net.run_until([&] { return a.has_text("io:aprs", "roger"); }, 30000, 50));
}

}  // TEST_SUITE
