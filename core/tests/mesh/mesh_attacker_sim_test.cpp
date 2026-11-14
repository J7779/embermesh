// Mesh simulator: an attacker node on a 10-node mesh (security review scenario suite). Each
// scenario runs one attack from the review (and finally all at once) while ordinary users keep
// talking, and asserts that legitimate traffic is still delivered, acknowledged and stored.
//
// Topology: a line of 10 nodes n0 - n1 - ... - n9 on a fast shared medium (each node hears only its
// neighbours); the attacker "eve" hears and is heard by n0..n4 only.
#include <chrono>

#include "doctest.h"
#include "mesh/sim.h"
#include "proto/packet.h"
#include "proto/payload.h"
#include "proto/secure.h"

using namespace sim;
using namespace ember::proto;

namespace {

struct Mesh {
  Sim s;
  Medium* m = nullptr;
  std::vector<Node*> n;
  Node* eve = nullptr;
  SimLink* eve_link = nullptr;
  crypto::Csprng rng{ByteView(std::string_view("eve-attacker-rng-0123456789abcdef!"))};
  Identity forger = Identity::generate(rng);

  explicit Mesh(uint64_t seed) : s(seed) {
    m = &s.add_medium(lan_caps());
    for (int i = 0; i < 10; ++i) {
      n.push_back(&s.add_node("n" + std::to_string(i)));
      s.join(*n.back(), *m);
    }
    eve = &s.add_node("eve");
    eve_link = &s.join(*eve, *m);
    for (int i = 0; i < 10; ++i)
      for (int j = 0; j < 10; ++j)
        if (i != j && std::abs(i - j) > 1) m->connect(*n[static_cast<size_t>(i)], *n[static_cast<size_t>(j)], false, false);
    for (int i = 5; i < 10; ++i) m->connect(*eve, *n[static_cast<size_t>(i)], false);
    for (int i = 0; i < 10; ++i)
      for (int j = i + 1; j < 10; ++j) s.introduce(*n[static_cast<size_t>(i)], *n[static_cast<size_t>(j)]);
    s.run(5000);
  }

  // Raw frame from eve (any hop id she likes).
  void inject(uint32_t hop, FrameType t, ByteView body) {
    ByteWriter w;
    FrameHeader fh;
    fh.type = t;
    fh.hop_src = hop;
    write_frame_header(w, fh);
    w.bytes(body);
    eve_link->send(w.data(), 0);
  }
  NodeId rand_node() {
    NodeId x;
    rng.fill(x.data(), 8);
    return x;
  }
  Bytes forged(const NodeId& origin, Kind k, ByteView body, Prio prio, uint8_t ttl, uint64_t created_ms) {
    Header h;
    h.kind = k;
    h.prio = prio;
    h.ttl = ttl;
    h.hop_limit = ttl;
    h.origin = origin;
    h.pkt_id = rng.u32() | 1;
    h.created = unix_ms_to_wire(created_ms);
    h.lifetime_min = 1440;
    return seal_signed(h, body, forger);
  }

  // Legitimate traffic: a broadcast from n9 and a DM n0 -> n9, both must arrive (and the DM be
  // acknowledged) within `ms`; the broadcast must then be stored at every node.
  void check_legit(const std::string& tag, uint64_t ms = 120000) {
    std::string bc = "broadcast " + tag, dm = "dm " + tag;
    n[9]->eng->send_text(Dest::broadcast(), bc);
    SendResult r = n[0]->eng->send_text(Dest::to_node(n[9]->nid()), dm);
    REQUIRE(r.ok);
    uint64_t t = s.run_until([&] {
      for (int i = 0; i < 9; ++i)
        if (!n[static_cast<size_t>(i)]->rec.got_text(bc)) return false;
      return n[9]->rec.got_text(dm) && n[0]->rec.status_of(r.id) == MsgStatus::Delivered;
    }, ms);
    CHECK_MESSAGE(t != UINT64_MAX, tag);
    for (int i = 0; i < 10; ++i) CHECK_MESSAGE(n[static_cast<size_t>(i)]->rec.count_text(bc) <= 1, tag);
    s.run(6000);  // a gc pass
    for (int i = 0; i < 9; ++i) {
      uint32_t cnt = 0;
      Bytes b = n[static_cast<size_t>(i)]->eng->export_bundle(BundleFilter(), &cnt);
      std::vector<ByteView> pk;
      decode_bundle(b, pk);
      bool have = false;
      for (auto p : pk) {
        PacketView pv;
        TextBody tb;
        if (parse_packet(p, pv) && pv.h.kind == Kind::Text && pv.h.origin == n[9]->nid() && decode(pv.body, tb) &&
            tb.text == bc)
          have = true;
      }
      CHECK_MESSAGE(have, tag << ": broadcast stored at n" << i);
    }
  }
  void clocks_ok(const std::string& tag) {
    for (auto* x : n)
      CHECK_MESSAGE(std::llabs(static_cast<long long>(x->eng->now_ms()) - static_cast<long long>(s.now())) < 2000, tag);
  }

  // --- attacks ---------------------------------------------------------------------------------
  void time_attack(int rounds) {
    for (int i = 0; i < rounds; ++i) {
      Beacon b;
      b.node = rand_node();
      b.has_time = true;
      b.time_quality = 0;
      b.time_ms = s.now() + 40 * 86400000ull;
      inject(short_id(b.node), FrameType::Control, encode_beacon(b));
      Identity syb = Identity::generate(rng);
      AnnounceBody a;
      a.keys = syb.public_keys();
      a.time_quality = 0;
      Header h;
      h.kind = Kind::Announce;
      h.origin = syb.node_id();
      h.pkt_id = rng.u32() | 1;
      h.created = unix_ms_to_wire(s.now() + 40 * 86400000ull);
      h.lifetime_min = 600;
      h.ttl = 3;
      h.hop_limit = 3;
      inject(short_id(h.origin), FrameType::Packet, seal_signed(h, encode(a), syb));
      s.run(50);
    }
  }
  void sos_flood(int count) {
    for (int i = 0; i < count; ++i) {
      SosBody sb;
      sb.text = "FAKE " + std::to_string(i);
      inject(rng.u32(), FrameType::Packet, forged(rand_node(), Kind::Sos, encode(sb), Prio::Emergency, 15, s.now()));
      if (i % 20 == 0) s.step();
    }
  }
  void ack_flood(int count) {
    for (int i = 0; i < count; ++i) {
      AckBody ab;
      AckEntry e;
      e.pkt_id = rng.u32();
      rng.fill(e.secret.data(), 8);
      ab.acks.push_back(e);
      Header h;
      h.kind = Kind::Ack;
      h.dst_type = DestType::Node;
      h.dst_node = rand_node();
      h.origin = rand_node();
      h.pkt_id = rng.u32() | 1;
      h.created = unix_ms_to_wire(s.now());
      h.lifetime_min = 600;
      h.ttl = 7;
      h.hop_limit = 7;
      inject(rng.u32(), FrameType::Packet, seal_none(h, encode(ab)));
      if (i % 20 == 0) s.step();
    }
  }
  void sybil_flood(int count) {
    for (int i = 0; i < count; ++i) {
      Identity syb = Identity::generate(rng);
      AnnounceBody a;
      a.keys = syb.public_keys();
      Header h;
      h.kind = Kind::Announce;
      h.origin = syb.node_id();
      h.pkt_id = rng.u32() | 1;
      h.created = unix_ms_to_wire(s.now());
      h.lifetime_min = 600;
      h.ttl = 7;
      h.hop_limit = 7;
      inject(short_id(h.origin), FrameType::Packet, seal_signed(h, encode(a), syb));
      if (i % 20 == 0) s.step();
    }
  }
  void sync_flood(int count) {
    Beacon b;
    b.node = rand_node();
    uint32_t hop = short_id(b.node);
    inject(hop, FrameType::Control, encode_beacon(b));
    s.step();
    for (int r = 0; r < count; ++r) {
      for (int k = 0; k < 5; ++k) {
        SyncMsg m;
        m.target = short_id(n[static_cast<size_t>(k)]->nid());
        for (int i = 0; i < 8; ++i) {
          SyncItem it;
          it.type = SyncItem::Want;
          for (int j = 0; j < 255; ++j) it.ids.push_back(rng.u32());
          m.items.push_back(it);
        }
        inject(hop, FrameType::Control, encode_sync(m));
      }
      s.step();
    }
  }
};

}  // namespace

TEST_SUITE("mesh") {

TEST_CASE("attacker sim: clock poisoning (beacons + sybil announces) on a 10-node mesh") {
  Mesh x(901);
  x.check_legit("before");
  x.time_attack(40);
  x.s.run(15000);
  x.clocks_ok("time");
  x.check_legit("after time attack");
}

TEST_CASE("attacker sim: forged SOS flood and junk ACKs") {
  Mesh x(902);
  x.check_legit("before");
  x.sos_flood(1500);
  x.ack_flood(800);
  x.check_legit("during flood", 180000);
  for (auto* nd : x.n) {
    size_t unv = 0;
    for (auto& p : nd->rec.sos) unv += !p.first.authenticated;
    CHECK(unv <= 60);  // budgeted per link and globally
  }
}

TEST_CASE("attacker sim: a forged cancel cannot clear a real SOS anywhere") {
  Mesh x(903);
  x.n[7]->eng->sos_start("trapped", std::nullopt, 60);
  x.s.run(5000);
  for (int i = 0; i < 10; ++i)
    if (i != 7) REQUIRE(!x.n[static_cast<size_t>(i)]->rec.sos.empty());
  SosBody c;
  c.active = false;
  for (int k = 0; k < 5; ++k)
    x.inject(x.rng.u32(), FrameType::Packet, x.forged(x.n[7]->nid(), Kind::Sos, encode(c), Prio::Emergency, 15, x.s.now() + 3000000));
  x.s.run(400000);
  for (int i = 0; i < 10; ++i) {
    if (i == 7) continue;
    for (auto& p : x.n[static_cast<size_t>(i)]->rec.sos) CHECK(p.second.active);
  }
  x.check_legit("after cancel attack");
}

TEST_CASE("attacker sim: replays, forged hop limits and sybils") {
  Mesh x(904);
  std::vector<Bytes> captured;
  x.s.tx_hook = [&](SimLink& from, const Bytes& f) {
    if (from.node() == x.n[1]) captured.push_back(f);
  };
  // n1 -> n2 DM, and a neighbours-only broadcast from n1
  SendResult r = x.n[1]->eng->send_text(Dest::to_node(x.n[2]->nid()), "only once");
  SendOptions o;
  o.ttl_hops = 1;
  x.n[1]->eng->send_text(Dest::broadcast(), "neighbours only", o);
  REQUIRE(x.s.run_until([&] { return x.n[1]->rec.status_of(r.id) == MsgStatus::Delivered; }, 30000) != UINT64_MAX);
  x.s.tx_hook = nullptr;
  x.sos_flood(500);
  x.sybil_flood(1500);
  x.s.run(5000);
  for (auto& f : captured) {
    FrameHeader fh;
    ByteView body;
    if (!parse_frame(f, fh, body) || fh.type != FrameType::Packet) continue;
    Bytes p = body.to_bytes();
    set_hop_byte(p, 15, 0);
    x.inject(x.rng.u32(), FrameType::Packet, p);
  }
  x.s.run(30000);
  CHECK(x.n[2]->rec.count_text("only once") == 1);
  for (int i = 3; i < 10; ++i) CHECK_FALSE(x.n[static_cast<size_t>(i)]->rec.got_text("neighbours only"));
  for (auto* nd : x.n) CHECK(nd->eng->peers().size() <= 300);
  x.check_legit("after replay / sybils");
}

TEST_CASE("attacker sim: SYNC CPU flood from a beaconed neighbour") {
  Mesh x(905);
  for (int i = 0; i < 200; ++i) x.n[9]->eng->send_text(Dest::broadcast(), "backlog " + std::to_string(i));
  x.s.run(20000);
  auto t0 = std::chrono::steady_clock::now();
  x.sync_flood(60);
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  MESSAGE("60 rounds of hostile SYNC to 5 nodes: " << ms << " ms of simulation");
  CHECK(ms < 10000);
  x.check_legit("after sync flood");
}

TEST_CASE("attacker sim: everything at once") {
  Mesh x(906);
  x.time_attack(20);
  x.sos_flood(600);
  x.ack_flood(300);
  x.sybil_flood(600);
  x.sync_flood(30);
  x.clocks_ok("all");
  x.check_legit("all attacks", 180000);
  MeshStats st = x.n[2]->eng->stats();
  MESSAGE("n2 after everything: stored=" << st.stored_messages << " dropped_rate=" << st.dropped_rate
          << " dropped_auth=" << st.dropped_auth << " peers=" << x.n[2]->eng->peers().size());
}

}  // TEST_SUITE
