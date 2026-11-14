// Mesh engine: regressions for the security review (docs/PROTOCOL.md §11 "Admission control and
// time"). Each test ports one reproduction from the security review (an unauthenticated radio
// node talking to a victim through a fake link) and asserts the hardened behaviour.
#include <chrono>
#include <map>
#include <set>

#include "doctest.h"
#include "mesh/engine.h"
#include "proto/packet.h"
#include "proto/payload.h"
#include "proto/secure.h"

using namespace ember;
using namespace ember::mesh;
using namespace ember::proto;

namespace {

constexpr uint64_t kT0 = 1794000000000ull;  // 2026-11
constexpr uint64_t kDay = 86400000ull;

struct FakeLink : Link {
  LinkCaps c;
  std::vector<Bytes> sent;
  FakeLink() { c.mtu = 4000; c.bitrate_bps = 1000000; c.half_duplex = false; c.broadcast = true; }
  const char* type() const override { return "fake"; }
  LinkCaps caps() const override { return c; }
  LinkState state() const override { return LinkState::Up; }
  bool send(ByteView f, uint8_t) override { sent.push_back(f.to_bytes()); return true; }
};

struct Del : MeshDelegate {
  int texts = 0, texts_auth = 0, sos = 0, sos_cancel = 0, sos_unauth = 0, bulletins = 0, bulletin_deletes = 0;
  std::vector<std::pair<NodeId, MsgId>> reads;
  void on_text(const RxInfo& rx, const TextBody&) override { texts++; if (rx.authenticated) texts_auth++; }
  void on_sos(const RxInfo& rx, const SosBody& b) override {
    sos++;
    if (!b.active) sos_cancel++;
    if (!rx.authenticated) sos_unauth++;
  }
  void on_bulletin(const RxInfo&, const BulletinBody& b) override { bulletins++; if (b.is_delete) bulletin_deletes++; }
  void on_read(const NodeId& r, const MsgId& m) override { reads.push_back({r, m}); }
};

struct Persist : MeshPersistence {
  std::map<uint64_t, std::pair<Bytes, StoredMeta>> pk;
  std::map<MsgId, uint64_t> replay;
  void store_put(uint64_t w, ByteView p, const StoredMeta& m) override { pk[w] = {p.to_bytes(), m}; }
  void store_remove(uint64_t w) override { pk.erase(w); }
  void replay_put(const MsgId& m, uint64_t until) override { replay[m] = until; }
  void replay_remove(const MsgId& m) override { replay.erase(m); }
};

struct Ctx {
  Identity id;
  FakeLink link;
  Del del;
  std::unique_ptr<MeshEngine> eng;
  uint64_t now = kT0;
  explicit Ctx(uint8_t seedbyte, MeshConfig cfg = MeshConfig(), uint64_t start = kT0) : now(start) {
    crypto::Key32 s{};
    s.fill(seedbyte);
    id = Identity::from_seed(s);
    Bytes ent(32, seedbyte);
    eng.reset(new MeshEngine(id, ent, cfg));
    eng->set_delegate(&del);
    eng->attach_link(1, &link);
    eng->tick(now);
  }
  void advance(uint64_t ms) { now += ms; eng->tick(now); }
  // Packets this node put on its link (parsed), optionally only of one kind.
  std::vector<PacketView> sent_packets(std::vector<Bytes>& keep) {
    keep = link.sent;
    std::vector<PacketView> out;
    for (auto& f : keep) {
      FrameHeader fh;
      ByteView body;
      PacketView pv;
      if (parse_frame(f, fh, body) && fh.type == FrameType::Packet && parse_packet(body, pv)) out.push_back(pv);
    }
    return out;
  }
};

Bytes frame(uint32_t hop, FrameType t, ByteView body) {
  ByteWriter w;
  FrameHeader fh;
  fh.type = t;
  fh.hop_src = hop;
  write_frame_header(w, fh);
  w.bytes(body);
  return w.take();
}

crypto::Csprng& attacker_rng() {
  static crypto::Csprng r(ByteView(std::string_view("attacker-seed-0123456789abcdef0123")));
  return r;
}
NodeId rand_node() { NodeId n; attacker_rng().fill(n.data(), 8); return n; }

// A Signed broadcast "from" an arbitrary origin; the signature is real but by a key nobody
// associates with `origin`, i.e. garbage from the victim's point of view.
Bytes forged(const NodeId& origin, Kind k, ByteView body, uint64_t mesh_ms, Prio prio, uint8_t ttl,
             uint32_t pkt_id = 0, uint16_t life_min = 1440) {
  static Identity signer = Identity::generate(attacker_rng());
  Header h;
  h.kind = k;
  h.dst_type = DestType::Broadcast;
  h.prio = prio;
  h.ttl = ttl;
  h.origin = origin;
  h.pkt_id = pkt_id ? pkt_id : (attacker_rng().u32() | 1);
  h.created = unix_ms_to_wire(mesh_ms);
  h.lifetime_min = life_min;
  return seal_signed(h, body, signer);
}

void feed_packet(Ctx& v, uint32_t hop, const Bytes& pkt) { v.eng->on_frame(1, frame(hop, FrameType::Packet, pkt), RxMeta()); }

void pipe(Ctx& from, Ctx& to) {
  auto frames = from.link.sent;
  from.link.sent.clear();
  for (auto& f : frames) to.eng->on_frame(1, f, RxMeta());
}

void introduce(Ctx& a, Ctx& b) {
  a.eng->add_known_keys(b.id.public_keys());
  b.eng->add_known_keys(a.id.public_keys());
}

int count_from(const MeshEngine& e, const NodeId& origin) {
  uint32_t cnt = 0;
  Bytes bundle = e.export_bundle(BundleFilter(), &cnt);
  std::vector<ByteView> pk;
  decode_bundle(bundle, pk);
  int n = 0;
  for (auto p : pk) {
    PacketView pv;
    if (parse_packet(p, pv) && pv.h.origin == origin) n++;
  }
  return n;
}

void beacon_time(Ctx& v, uint64_t t, uint8_t quality) {
  Beacon bc;
  bc.node = rand_node();
  bc.has_time = true;
  bc.time_quality = quality;
  bc.time_ms = t;
  v.eng->on_frame(1, frame(short_id(bc.node), FrameType::Control, encode_beacon(bc)), RxMeta());
}

// A self-certifying announcement from a fresh (sybil) identity with a chosen `created` time.
Bytes sybil_announce(uint64_t created_ms, uint8_t time_quality = 0) {
  Identity s = Identity::generate(attacker_rng());
  AnnounceBody a;
  a.keys = s.public_keys();
  a.time_quality = time_quality;
  Header h;
  h.kind = Kind::Announce;
  h.origin = s.node_id();
  h.pkt_id = attacker_rng().u32() | 1;
  h.created = unix_ms_to_wire(created_ms);
  h.lifetime_min = 600;
  h.ttl = 3;
  h.hop_limit = 3;
  return seal_signed(h, encode(a), s);
}

// Delivers an announcement as if transmitted by its origin (hop_src == short id of the origin).
void feed_announce(Ctx& v, const Bytes& pkt) {
  PacketView pv;
  REQUIRE(parse_packet(pkt, pv));
  feed_packet(v, short_id(pv.h.origin), pkt);
}

}  // namespace

TEST_SUITE("mesh") {

// ---------------------------------------------------------------------------------------------
// C1 + M1: mesh time
TEST_CASE("attack C1: unauthenticated beacons cannot move the clock or wipe the store") {
  Ctx v(1), b(2);
  introduce(v, b);
  b.eng->send_text(Dest::broadcast(), "legit public message");
  b.advance(50);
  pipe(b, v);
  v.advance(50);
  REQUIRE(count_from(*v.eng, b.id.node_id()) >= 1);
  for (int i = 0; i < 4; ++i) beacon_time(v, v.now + 40 * kDay, 0);
  v.advance(6000);
  v.advance(6000);
  int64_t skew = static_cast<int64_t>(v.eng->now_ms()) - static_cast<int64_t>(v.now);
  CHECK(std::llabs(skew) < 1000);
  CHECK(v.eng->time_info().source == TimeSource::System);
  CHECK(count_from(*v.eng, b.id.node_id()) >= 1);
  CHECK(v.eng->stats().dropped_expired == 0);
  int before = v.del.texts;
  b.eng->send_text(Dest::broadcast(), "fresh legit message");
  b.advance(50);
  pipe(b, v);
  v.advance(50);
  CHECK(v.del.texts == before + 1);
}

TEST_CASE("attack C1: sybil announces cannot move a sane clock; an unset clock needs several agreeing origins") {
  Ctx v(1);
  for (int i = 0; i < 6; ++i) feed_announce(v, sybil_announce(v.now + 40 * kDay));
  v.advance(6000);
  CHECK(std::llabs(static_cast<int64_t>(v.eng->now_ms()) - static_cast<int64_t>(v.now)) < 1000);
  // A node without a clock (host time 1970) learns time from authenticated, agreeing origins.
  Ctx u(3, MeshConfig(), 5000);
  CHECK(u.eng->time_info().quality == 15);
  uint64_t real = kT0;
  feed_announce(u, sybil_announce(real));
  u.advance(1000);
  CHECK(u.eng->time_info().quality == 15);  // one origin is not enough
  feed_announce(u, sybil_announce(real + 1000));
  feed_announce(u, sybil_announce(real + 40 * kDay));  // an outlier does not win
  u.advance(1000);
  CHECK(u.eng->time_info().source == TimeSource::Mesh);
  CHECK(std::llabs(static_cast<int64_t>(u.eng->now_ms()) - static_cast<int64_t>(real)) < 10000);
}

TEST_CASE("attack C1: a local clock jump does not purge the store") {
  Ctx v(1), b(2);
  introduce(v, b);
  b.eng->send_text(Dest::broadcast(), "keep me");
  b.advance(50);
  pipe(b, v);
  v.advance(50);
  REQUIRE(count_from(*v.eng, b.id.node_id()) == 1);
  v.eng->set_time(v.now + 40 * kDay, TimeSource::Manual);  // operator typo
  v.advance(6000);
  v.eng->set_time(v.now, TimeSource::Manual);  // corrected
  v.advance(6000);
  CHECK(count_from(*v.eng, b.id.node_id()) == 1);
}

TEST_CASE("attack M1: absurd beacon times are rejected without overflow") {
  Ctx v(1);
  uint64_t before = v.eng->now_ms();
  for (uint64_t t : {0x8000000000000000ull, ~0ull, 1ull, 0ull}) beacon_time(v, t, 0);
  v.advance(10);
  CHECK(v.eng->now_ms() - before < 1000);
  Json st = v.eng->save_state();
  st.at_key("time").set("offset_ms", static_cast<long long>(INT64_MIN + 5));
  Ctx w(2);
  CHECK(w.eng->load_state(st));
  w.advance(10);
  CHECK(std::llabs(static_cast<int64_t>(w.eng->now_ms()) - static_cast<int64_t>(w.now)) < 1000);
}

// ---------------------------------------------------------------------------------------------
// C2: unknown-origin floods
TEST_CASE("attack C2: forged SOS flood is budgeted, flagged and bounded") {
  Ctx v(1);
  int n = 1000;
  for (int i = 0; i < n; ++i) {
    SosBody s;
    s.text = "FAKE SOS " + std::to_string(i);
    feed_packet(v, 0x11110000u + static_cast<uint32_t>(i), forged(rand_node(), Kind::Sos, encode(s), v.eng->now_ms(), Prio::Emergency, 15));
    v.now += 3;
    if (i % 100 == 0) v.eng->tick(v.now);
  }
  v.advance(100);
  size_t relayed = 0;
  for (auto& f : v.link.sent) {
    FrameHeader fh;
    ByteView b;
    PacketView pv;
    if (parse_frame(f, fh, b) && fh.type == FrameType::Packet && parse_packet(b, pv) && pv.h.kind == Kind::Sos) {
      relayed++;
      CHECK(pv.h.ttl <= 2);  // unknown origins travel at most two more hops
    }
  }
  MESSAGE("forged SOS: delivered=" << v.del.sos << " stored=" << v.eng->stats().stored_messages << " relayed=" << relayed);
  CHECK(v.del.sos == v.del.sos_unauth);  // all flagged unverified
  CHECK(v.del.sos >= 1);                 // a stranger in trouble is still heard
  CHECK(v.del.sos <= 20);
  CHECK(relayed <= 64);  // the per-link unknown-origin budget: burst 60 + 1/s refill
  CHECK(v.eng->stats().stored_messages <= 600);  // 10 % of the 5000-packet quota (+ own)
}

TEST_CASE("attack C2: junk from unknown origins never evicts verified stored traffic") {
  MeshConfig cfg;
  cfg.store_max_packets = 500;
  Ctx v(1, cfg), b(2, cfg);
  introduce(v, b);
  for (int i = 0; i < 20; ++i) b.eng->send_text(Dest::broadcast(), "legit stored " + std::to_string(i));
  b.advance(50);
  pipe(b, v);
  v.advance(50);
  REQUIRE(count_from(*v.eng, b.id.node_id()) == 20);
  for (int i = 0; i < 520; ++i) {
    feed_packet(v, 0x22220000u + static_cast<uint32_t>(i), forged(rand_node(), Kind::Sos, ByteView(), v.eng->now_ms(), Prio::Emergency, 15));
    v.now += 3;
    if (i % 100 == 0) v.eng->tick(v.now);
  }
  // junk ACKs (sec None) too
  for (int i = 0; i < 520; ++i) {
    AckBody ab;
    AckEntry e;
    e.pkt_id = attacker_rng().u32();
    attacker_rng().fill(e.secret.data(), 8);
    ab.acks.push_back(e);
    Header h;
    h.kind = Kind::Ack;
    h.dst_type = DestType::Node;
    h.dst_node = rand_node();
    h.origin = rand_node();
    h.pkt_id = attacker_rng().u32() | 1;
    h.created = unix_ms_to_wire(v.eng->now_ms());
    h.lifetime_min = 600;
    h.ttl = 3;
    h.hop_limit = 3;
    feed_packet(v, 0x23230000u + static_cast<uint32_t>(i), seal_none(h, encode(ab)));
    v.now += 3;
    if (i % 100 == 0) v.eng->tick(v.now);
  }
  v.advance(100);
  CHECK(count_from(*v.eng, b.id.node_id()) == 20);
  CHECK(v.eng->stats().stored_messages <= 500);
}

// ---------------------------------------------------------------------------------------------
// H1: forged SOS cancel
TEST_CASE("attack H1: a forged cancel cannot clear a stranger's SOS") {
  Ctx v(1), a(4);
  a.eng->sos_start("trapped under rubble", std::nullopt, 60);
  a.advance(50);
  std::vector<Bytes> keep;
  for (auto& f : a.link.sent) {
    FrameHeader fh;
    ByteView body;
    PacketView pv;
    if (parse_frame(f, fh, body) && fh.type == FrameType::Packet && parse_packet(body, pv) && pv.h.kind == Kind::Sos)
      v.eng->on_frame(1, f, RxMeta());
  }
  a.link.sent.clear();
  REQUIRE(v.del.sos == 1);
  SosBody c;
  c.active = false;
  feed_packet(v, 0x33330001u, forged(a.id.node_id(), Kind::Sos, encode(c), v.eng->now_ms() + 3000000, Prio::Emergency, 15));
  v.advance(10);
  v.advance(6 * 60000);  // the held cancel times out unverified
  CHECK(v.del.sos_cancel == 0);
  uint32_t cnt = 0;
  Bytes bundle = v.eng->export_bundle(BundleFilter(), &cnt);
  std::vector<ByteView> pk;
  decode_bundle(bundle, pk);
  int active = 0, cancels = 0;
  for (auto p : pk) {
    PacketView pv;
    SosBody sb;
    if (parse_packet(p, pv) && pv.h.kind == Kind::Sos && pv.h.origin == a.id.node_id() && decode(pv.body, sb))
      (sb.active ? active : cancels)++;
  }
  CHECK(active == 1);
  CHECK(cancels == 0);
  // The genuine cancel is honoured once A's key is known.
  a.eng->sos_stop();
  a.advance(50);
  v.eng->add_known_keys(a.id.public_keys());
  pipe(a, v);
  v.advance(50);
  CHECK(v.del.sos_cancel == 1);
}

TEST_CASE("attack H1: unauthenticated bulletin deletes are ignored") {
  Ctx v(1), b(2);
  introduce(v, b);
  BulletinBody bb;
  bb.title = "water at the school";
  bb.text = "clean, bring containers";
  SendResult r = b.eng->post_bulletin(bb);
  b.advance(50);
  pipe(b, v);
  v.advance(50);
  REQUIRE(count_from(*v.eng, b.id.node_id()) == 1);
  BulletinBody del;
  del.is_delete = true;
  del.delete_pkt_id = r.id.pkt_id;
  // forged delete: origin B, someone else's signature
  feed_packet(v, 0x44440002u, forged(b.id.node_id(), Kind::Bulletin, encode(del), v.eng->now_ms(), Prio::Normal, 3));
  v.advance(50);
  CHECK(count_from(*v.eng, b.id.node_id()) == 1);
  CHECK(v.del.bulletin_deletes == 0);
}

// ---------------------------------------------------------------------------------------------
// H2: replay-set collisions
TEST_CASE("attack H2: a colliding origin cannot censor a genuine DM") {
  auto msg_hash = [](const NodeId& o, uint32_t p) {
    return node_id_u64(o) * 0x9E3779B97F4A7C15ull ^ (uint64_t(p) * 0xC2B2AE3D27D4EB4Full) ^ p;
  };
  auto inv64 = [](uint64_t a) { uint64_t x = a; for (int i = 0; i < 6; ++i) x *= 2 - a * x; return x; };
  Ctx v(1), b(2);
  introduce(v, b);
  SendResult r = b.eng->send_text(Dest::to_node(v.id.node_id()), "genuine encrypted DM");
  b.advance(50);
  std::vector<Bytes> held = b.link.sent;
  b.link.sent.clear();
  uint64_t target = msg_hash(r.id.origin, r.id.pkt_id);
  uint32_t p2 = 0x12345678;
  uint64_t o2 = (target ^ (uint64_t(p2) * 0xC2B2AE3D27D4EB4Full) ^ p2) * inv64(0x9E3779B97F4A7C15ull);
  NodeId fake = node_id_from_u64(o2);
  REQUIRE(msg_hash(fake, p2) == target);
  feed_packet(v, 0x44440001u, forged(fake, Kind::Sos, ByteView(), v.eng->now_ms(), Prio::Emergency, 15, p2));
  for (auto& f : held) v.eng->on_frame(1, f, RxMeta());
  v.advance(50);
  CHECK(v.del.texts == 1);
}

// ---------------------------------------------------------------------------------------------
// H4: replay after flushing ack / dedup state
TEST_CASE("attack H4: flushing ack and dedup state does not let a captured DM replay") {
  Ctx v(1), b(2);
  introduce(v, b);
  b.eng->send_text(Dest::to_node(v.id.node_id()), "pay 100 to X");
  b.advance(50);
  std::vector<Bytes> captured = b.link.sent;
  pipe(b, v);
  v.advance(50);
  REQUIRE(v.del.texts == 1);
  for (int i = 0; i < 1100; ++i) {
    AckBody ab;
    for (int j = 0; j < 16; ++j) {
      AckEntry e;
      e.pkt_id = attacker_rng().u32();
      attacker_rng().fill(e.secret.data(), 8);
      ab.acks.push_back(e);
    }
    Header h;
    h.kind = Kind::Ack;
    h.dst_type = DestType::Node;
    h.dst_node = rand_node();
    h.origin = rand_node();
    h.pkt_id = attacker_rng().u32() | 1;
    h.created = unix_ms_to_wire(v.eng->now_ms());
    h.lifetime_min = 60;
    h.ttl = 0;
    h.hop_limit = 0;
    feed_packet(v, 0x55550000u + static_cast<uint32_t>(i), seal_none(h, encode(ab)));
    v.now += 3;
    if (i % 100 == 0) v.eng->tick(v.now);
  }
  for (int i = 0; i < 8300; ++i) {
    feed_packet(v, 0x66660000u + static_cast<uint32_t>(i), forged(rand_node(), Kind::Sos, ByteView(), v.eng->now_ms(), Prio::Low, 0));
    v.now += 3;
    if (i % 100 == 0) v.eng->tick(v.now);
  }
  v.advance(6000);
  for (auto& f : captured) v.eng->on_frame(1, f, RxMeta());
  v.advance(50);
  CHECK(v.del.texts == 1);
}

TEST_CASE("attack H4: replay memory is persisted and survives a restart") {
  for (bool restore : {true, false}) {
    CAPTURE(restore);
    Persist per;
    std::vector<Bytes> captured;
    Ctx b(2);
    {
      Ctx v(1);
      v.eng->set_persistence(&per);
      introduce(v, b);
      b.eng->send_text(Dest::to_node(v.id.node_id()), "pay 100 to X");
      b.advance(50);
      captured = b.link.sent;
      pipe(b, v);
      v.advance(50);
      REQUIRE(v.del.texts == 1);
      REQUIRE(per.replay.size() == 1);
    }
    Ctx v2(1);  // same identity, fresh process; store not restored (e.g. evicted / cleared)
    v2.eng->add_known_keys(b.id.public_keys());
    if (restore)
      for (auto& kv : per.replay) v2.eng->restore_replay(kv.first, kv.second);
    for (auto& f : captured) v2.eng->on_frame(1, f, RxMeta());
    v2.advance(50);
    CHECK(v2.del.texts == (restore ? 0 : 1));  // the control run shows what the memory prevents
  }
}

// ---------------------------------------------------------------------------------------------
// M2: forged hop byte
TEST_CASE("attack M2: a forged hop byte cannot extend a packet beyond its originator's hop limit") {
  Ctx v(1), b(2);
  introduce(v, b);
  SendOptions o;
  o.ttl_hops = 1;
  b.eng->send_text(Dest::broadcast(), "neighbours only", o);
  b.advance(50);
  std::vector<Bytes> fr = b.link.sent;
  pipe(b, v);
  v.advance(500);
  size_t before = v.link.sent.size();
  for (auto f : fr) {
    FrameHeader fh;
    ByteView body;
    if (!parse_frame(f, fh, body) || fh.type != FrameType::Packet) continue;
    Bytes pkt = body.to_bytes();
    set_hop_byte(pkt, 15, 0);
    feed_packet(v, 0x77770001u, pkt);
  }
  v.advance(500);
  int relayed_text = 0;
  for (size_t i = before; i < v.link.sent.size(); ++i) {
    FrameHeader fh;
    ByteView body;
    PacketView pv;
    if (parse_frame(v.link.sent[i], fh, body) && fh.type == FrameType::Packet && parse_packet(body, pv) &&
        pv.h.kind == Kind::Text)
      relayed_text++;
  }
  CHECK(relayed_text == 0);
}

// ---------------------------------------------------------------------------------------------
// H3: SYNC amplification
// A signed broadcast from a real identity the victim knows (verified store traffic).
Bytes genuine(const Identity& from, Kind k, ByteView body, uint64_t mesh_ms) {
  Header h;
  h.kind = k;
  h.origin = from.node_id();
  h.pkt_id = attacker_rng().u32() | 1;
  h.created = unix_ms_to_wire(mesh_ms);
  h.lifetime_min = 1440;
  h.ttl = 3;
  h.hop_limit = 3;
  return seal_signed(h, body, from);
}

TEST_CASE("attack H3: SYNC WANT with random ids is cheap and needs a beacon first") {
  Ctx v(1), src(9);
  v.eng->add_known_keys(src.id.public_keys());
  for (int i = 0; i < 5000; ++i) {
    Bytes p = genuine(src.id, Kind::Text, ByteView(std::string_view("x")), v.eng->now_ms());
    StoredMeta m;
    m.received_ms = v.eng->now_ms();
    m.expires_ms = v.eng->now_ms() + kDay;
    m.flags = kStoredTransmittable;
    v.eng->restore_packet(p, m);
  }
  REQUIRE(v.eng->stats().stored_messages == 5000);
  uint32_t hop = 0x88880001u;
  SyncMsg m;
  m.target = short_id(v.id.node_id());
  for (int i = 0; i < 64; ++i) {
    SyncItem it;
    it.type = SyncItem::Want;
    for (int j = 0; j < 255; ++j) it.ids.push_back(attacker_rng().u32());
    m.items.push_back(it);
  }
  Bytes f = frame(hop, FrameType::Control, encode_sync(m));
  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < 20; ++i) v.eng->on_frame(1, f, RxMeta());
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  MESSAGE("20 hostile SYNC frames: " << ms << " ms");
  CHECK(ms < 400);  // was ~274 ms per frame at -O2 before the fix
  CHECK(v.eng->stats().sync_pushed == 0);
  // Same attack from a neighbour that beaconed first: bounded work per message and per minute.
  Beacon bc;
  bc.node = rand_node();
  bc.node[0] = 0x88; bc.node[1] = 0x88; bc.node[2] = 0x00; bc.node[3] = 0x01;
  v.eng->on_frame(1, frame(hop, FrameType::Control, encode_beacon(bc)), RxMeta());
  t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < 200; ++i) v.eng->on_frame(1, f, RxMeta());
  ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  MESSAGE("200 hostile SYNC frames from a beaconed neighbour: " << ms << " ms");
  CHECK(ms < 1000);
  CHECK(v.eng->stats().sync_pushed == 0);
}

TEST_CASE("attack H3: WANT for real ids pushes a bounded amount per neighbour and round") {
  Ctx v(1), b(2), src(9);
  v.eng->add_known_keys(src.id.public_keys());
  std::vector<uint32_t> sids;
  for (int i = 0; i < 400; ++i) {
    Bytes p = genuine(src.id, Kind::Text, ByteView(std::string(900, 'x')), v.eng->now_ms());
    StoredMeta m;
    m.received_ms = v.eng->now_ms();
    m.expires_ms = v.eng->now_ms() + kDay;
    m.flags = kStoredTransmittable;
    v.eng->restore_packet(p, m);
    sids.push_back(sync_id(wire_id(p)));
  }
  // a beaconing neighbour asks for everything, many times
  Beacon bc;
  bc.node = b.id.node_id();
  v.eng->on_frame(1, frame(short_id(bc.node), FrameType::Control, encode_beacon(bc)), RxMeta());
  v.link.sent.clear();
  for (int r = 0; r < 20; ++r) {
    SyncMsg m;
    m.target = short_id(v.id.node_id());
    for (size_t k = 0; k < sids.size(); k += 255) {
      SyncItem it;
      it.type = SyncItem::Want;
      for (size_t j = k; j < sids.size() && j < k + 255; ++j) it.ids.push_back(sids[j]);
      m.items.push_back(it);
    }
    v.eng->on_frame(1, frame(short_id(bc.node), FrameType::Control, encode_sync(m)), RxMeta());
  }
  v.advance(10);
  size_t bytes = 0;
  for (auto& f : v.link.sent) bytes += f.size();
  MESSAGE("bytes queued for 20 full WANTs: " << bytes);
  CHECK(bytes <= 128 * 1024);
}

// ---------------------------------------------------------------------------------------------
// M3: sybils
TEST_CASE("attack M3: sybil announces are budgeted and never evict contacts or established peers") {
  Ctx v(1), b(2);
  v.eng->add_known_keys(b.id.public_keys());  // a contact
  for (int i = 0; i < 3000; ++i) {
    feed_packet(v, 0x99990000u + static_cast<uint32_t>(i), sybil_announce(v.eng->now_ms()));
    v.now += 20;
    if (i % 50 == 0) v.eng->tick(v.now);
  }
  v.advance(10);
  size_t peers = v.eng->peers().size();
  MESSAGE("peers after 3000 sybil announces in 60 s: " << peers);
  CHECK(peers <= 200);
  CHECK(v.eng->has_keys(b.id.node_id()));
}

// ---------------------------------------------------------------------------------------------
// M4: unauthenticated deliveries do not pre-empt genuine ones
TEST_CASE("attack M4: an unauthenticated look-alike is superseded by the genuine authenticated copy") {
  Ctx v(1), b(2);
  b.eng->add_known_keys(v.id.public_keys());
  SendResult r = b.eng->send_text(Dest::broadcast(), "the real text");
  b.advance(50);
  std::vector<Bytes> genuine = b.link.sent;
  b.link.sent.clear();
  TextBody tb;
  tb.text = "forged text";
  feed_packet(v, 0xAAAA0001u, forged(b.id.node_id(), Kind::Text, encode(tb), v.eng->now_ms(), Prio::Normal, 3, r.id.pkt_id));
  v.advance(6 * 60000);  // held for verification, then shown unverified
  CHECK(v.del.texts == 1);
  CHECK(v.del.texts_auth == 0);
  v.eng->add_known_keys(b.id.public_keys());
  for (auto& f : genuine) v.eng->on_frame(1, f, RxMeta());
  v.advance(50);
  CHECK(v.del.texts_auth == 1);
}

// ---------------------------------------------------------------------------------------------
// L5: read receipts
TEST_CASE("attack L5: a read receipt only covers messages actually sent to its author") {
  Ctx a(1), b(2), c(3);
  introduce(a, b);
  introduce(a, c);
  SendResult to_c = a.eng->send_text(Dest::to_node(c.id.node_id()), "for C only");
  a.advance(50);
  a.link.sent.clear();
  b.eng->send_read_receipt(a.id.node_id(), {to_c.id.pkt_id});
  b.advance(50);
  pipe(b, a);
  a.advance(50);
  CHECK(a.del.reads.empty());
  CHECK(a.eng->status(to_c.id) != MsgStatus::Read);
}

// ---------------------------------------------------------------------------------------------
// L6: frame station id
TEST_CASE("attack L6: an unauthenticated frame callsign is not shown as a peer's callsign") {
  Ctx v(1), b(2);
  v.eng->add_known_keys(b.id.public_keys());
  Beacon bc;
  bc.node = b.id.node_id();
  ByteWriter w;
  FrameHeader fh;
  fh.type = FrameType::Control;
  fh.hop_src = short_id(bc.node);
  fh.station = "W1FAKE";
  write_frame_header(w, fh);
  w.bytes(encode_beacon(bc));
  v.eng->on_frame(1, w.data(), RxMeta());
  auto p = v.eng->peer(b.id.node_id());
  REQUIRE(p);
  CHECK(p->callsign != "W1FAKE");
}

}  // TEST_SUITE
