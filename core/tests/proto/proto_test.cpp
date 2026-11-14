#include <chrono>
#include <set>

#include "base/json.h"
#include "crypto/identity.h"
#include "crypto/rng.h"
#include "doctest.h"
#include "proto/packet.h"
#include "proto/payload.h"
#include "proto/secure.h"

using namespace ember;
using namespace ember::proto;

namespace {

struct Xs {
  uint64_t s;
  explicit Xs(uint64_t seed) : s(seed ? seed : 1) {}
  uint64_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
  uint32_t below(uint32_t n) { return static_cast<uint32_t>(next() % n); }
};

Identity make_id(const char* seed) {
  crypto::Csprng r{ByteView(std::string_view(seed))};
  r.seed(ByteView(std::string_view("pad-to-thirty-two-bytes-of-seed-material")));
  return Identity::generate(r);
}

Header base_header(const Identity& from) {
  Header h;
  h.kind = Kind::Text;
  h.pkt_id = 0x01020304;
  h.origin = from.node_id();
  h.created = 123456;
  h.lifetime_min = 120;
  h.ttl = 5;
  return h;
}

// Every mutation of a valid packet: must never crash; returns how many were still accepted by `ok`.
template <typename F>
int mutate_all(const Bytes& pkt, F ok, uint64_t seed, int rounds) {
  Xs x(seed);
  int accepted = 0;
  for (int i = 0; i < rounds; ++i) {
    Bytes b = pkt;
    switch (x.below(4)) {
      case 0: b[x.below(static_cast<uint32_t>(b.size()))] ^= static_cast<uint8_t>(1u << x.below(8)); break;
      case 1: b.resize(x.below(static_cast<uint32_t>(b.size()))); break;
      case 2: b.insert(b.begin() + x.below(static_cast<uint32_t>(b.size())), static_cast<uint8_t>(x.next())); break;
      default: for (int k = 0; k < 3; ++k) b[x.below(static_cast<uint32_t>(b.size()))] = static_cast<uint8_t>(x.next()); break;
    }
    if (ok(b)) ++accepted;
  }
  return accepted;
}

}  // namespace

TEST_SUITE("proto") {

TEST_CASE("header round trip, sizes and validation") {
  Identity a = make_id("a");
  for (int dst = 0; dst < 3; ++dst) {
    Header h = base_header(a);
    h.dst_type = static_cast<DestType>(dst);
    h.dst_node = make_id("b").node_id();
    h.channel = 0xCAFEBABE;
    h.want_ack = dst == 1;
    h.ack_commit = AckCommit{1, 2, 3, 4, 5, 6, 7, 8};
    h.prio = Prio::High;
    h.sec = dst == 1 ? Sec::PairMac : Sec::Signed;
    Bytes pkt = dst == 1 ? seal_pair_mac(h, ByteView(std::string_view("hi")), crypto::Key32{})
                         : seal_signed(h, ByteView(std::string_view("hi")), a);
    PacketView p;
    REQUIRE(parse_packet(pkt, p));
    CHECK(p.h.dst_type == h.dst_type);
    CHECK(p.h.pkt_id == h.pkt_id);
    CHECK(p.h.origin == h.origin);
    CHECK(p.h.created == h.created);
    CHECK(p.h.lifetime_min == h.lifetime_min);
    CHECK(p.h.ttl == 5);
    CHECK(p.h.prio == Prio::High);
    CHECK(p.body.to_string() == "hi");
    if (dst == 1) { CHECK(p.h.dst_node == h.dst_node); CHECK(p.h.ack_commit == h.ack_commit); CHECK(p.h.want_ack); }
    if (dst == 2) CHECK(p.h.channel == 0xCAFEBABE);
    CHECK(p.auth_header.size() == header_size(h));
  }
  // v2: broadcast header is 22 bytes; DM with ack commitment 38; + 8 salt for AEAD.
  Header h = base_header(a);
  CHECK(header_size(h) == 22);
  h.dst_type = DestType::Node;
  h.want_ack = true;
  CHECK(header_size(h) == 38);
  h.sec = Sec::PairAead;
  CHECK(header_size(h) == 46);
  h.sec = Sec::Signed;

  // Invalid combinations are rejected.
  auto raw = [&](uint8_t p0, uint8_t kind) {
    Bytes b(40, 0);
    b[0] = p0;
    b[1] = kind;
    b[20] = 1;  // lifetime (low byte)
    return b;
  };
  PacketView p;
  CHECK_FALSE(parse_packet(raw(0x01, 1), p));               // want_ack with broadcast
  CHECK_FALSE(parse_packet(raw(uint8_t(2 << 5), 1), p));    // PairMac broadcast
  CHECK_FALSE(parse_packet(raw(uint8_t(4 << 5), 1), p));    // ChanAead broadcast
  CHECK_FALSE(parse_packet(raw(uint8_t(5 << 5), 1), p));    // unknown sec
  CHECK_FALSE(parse_packet(raw(uint8_t(3 << 3), 1), p));    // unknown dst
  CHECK_FALSE(parse_packet(raw(0x00, 1), p));               // Sec::None for TEXT
  CHECK(parse_packet(raw(0x00, uint8_t(Kind::Ack)), p));    // Sec::None ACK ok
  CHECK_FALSE(parse_packet(raw(uint8_t(1 << 5), 0), p));    // kind 0
  Bytes short_sig = raw(uint8_t(1 << 5), 1);
  CHECK_FALSE(parse_packet(short_sig, p));                  // signed needs 64-byte tag
  Bytes zero_life = raw(0x00, uint8_t(Kind::Ack));
  zero_life[20] = 0;
  CHECK_FALSE(parse_packet(zero_life, p));
  CHECK_FALSE(parse_packet(Bytes(kMaxPacket + 1, 0), p));
}

TEST_CASE("wire id ignores the hop byte only") {
  Identity a = make_id("a");
  Bytes pkt = seal_signed(base_header(a), ByteView(std::string_view("x")), a);
  uint64_t id = wire_id(pkt);
  Bytes hopped = pkt;
  set_hop_byte(hopped, 2, 9);
  CHECK(wire_id(hopped) == id);
  PacketView p;
  REQUIRE(parse_packet(hopped, p));
  CHECK(p.h.ttl == 2);
  CHECK(p.h.hops == 9);
  CHECK(verify_signed(p, a.sign_pk()));  // hop byte not covered by the signature
  for (size_t i = 0; i < pkt.size(); ++i) {
    if (i == kHopByteOffset) continue;
    Bytes b = pkt;
    b[i] ^= 1;
    CHECK(wire_id(b) != id);
  }
}

TEST_CASE("frame header, station id and fragments") {
  FrameHeader h;
  h.type = FrameType::Packet;
  h.station = "QX4ABC-7";
  h.hop_src = 0xA1B2C3D4;
  ByteWriter w;
  write_frame_header(w, h);
  w.str("body");
  FrameHeader g;
  ByteView body;
  REQUIRE(parse_frame(w.data(), g, body));
  CHECK(g.station == "QX4ABC-7");
  CHECK(g.hop_src == 0xA1B2C3D4);
  CHECK(body.to_string() == "body");
  CHECK(frame_overhead(0) == 5);
  CHECK(frame_overhead(8) == 14);
  Bytes f = w.data();
  f[0] |= 0x01;  // reserved bit
  CHECK_FALSE(parse_frame(f, g, body));
  f = w.data();
  f[0] = static_cast<uint8_t>((f[0] & 0x3F) | 0x40);  // version 1 (pre hop-limit / salt)
  CHECK_FALSE(parse_frame(f, g, body));
  f[0] = static_cast<uint8_t>((f[0] & 0x3F) | 0xC0);  // version 3
  CHECK_FALSE(parse_frame(f, g, body));
  f = w.data();
  f[2] = 'a';  // lower-case callsign
  CHECK_FALSE(parse_frame(f, g, body));
  CHECK_FALSE(parse_frame(ByteView(w.data()).sub(0, 14), g, body));  // no body

  // Fragmentation across MTUs, including the minimum.
  Bytes unit(1000);
  for (size_t i = 0; i < unit.size(); ++i) unit[i] = static_cast<uint8_t>(i * 31);
  FrameHeader fh;
  fh.hop_src = 7;
  for (size_t mtu : {kMinMtu, size_t(60), size_t(200), size_t(255)}) {
    auto frames = fragment_unit(unit, mtu, fh, 0x1234);
    REQUIRE_FALSE(frames.empty());
    Bytes re;
    uint8_t count = 0;
    for (size_t i = 0; i < frames.size(); ++i) {
      CHECK(frames[i].size() <= mtu);
      FrameHeader x;
      ByteView b;
      REQUIRE(parse_frame(frames[i], x, b));
      CHECK(x.type == FrameType::Fragment);
      Fragment fr;
      REQUIRE(parse_fragment(b, fr));
      CHECK(fr.index == i);
      CHECK(fr.frag_id == 0x1234);
      if (i == 0) { CHECK(fr.total_len == 1000); count = fr.count; }
      re.insert(re.end(), fr.data.begin(), fr.data.end());
    }
    CHECK(count == frames.size());
    CHECK(re == unit);
  }
  CHECK(fragment_unit(unit, 10, fh, 1).empty());                      // MTU too small
  CHECK(fragment_unit(Bytes(kMaxUnit + 1), 255, fh, 1).empty());      // too big
  Bytes huge(kMaxUnit);
  CHECK(fragment_unit(huge, kMinMtu, fh, 1).empty());                 // > 128 fragments
  Fragment fr;
  CHECK_FALSE(parse_fragment(Bytes{0, 1, 0, 1, 9}, fr));  // count 1
  CHECK_FALSE(parse_fragment(Bytes{0, 1, 3, 2, 9}, fr));  // index >= count
  CHECK_FALSE(parse_fragment(Bytes{0, 1, 0, 2, 0x20, 0x00, 9}, fr));  // total too large
  CHECK_FALSE(parse_fragment(Bytes{0, 1, 1, 2}, fr));     // no data
}

TEST_CASE("payload round trips") {
  Identity a = make_id("a");
  {
    TextBody t;
    t.text = "Meet at the bridge at 14:00 \xF0\x9F\x94\xA5";
    t.reply_to = MsgId{a.node_id(), 77};
    TextBody u;
    REQUIRE(decode(encode(t), u));
    CHECK(u.text == t.text);
    REQUIRE(u.reply_to);
    CHECK(*u.reply_to == *t.reply_to);
    TextBody big;
    big.text = std::string(5000, 'a');
    REQUIRE(decode(encode(big), u));
    CHECK(u.text.size() == kMaxText);  // truncated by the encoder
  }
  {
    AckBody b;
    b.acks.push_back({5, {1, 2, 3, 4, 5, 6, 7, 8}});
    b.acks.push_back({6, {}});
    AckBody c;
    REQUIRE(decode(encode(b), c));
    CHECK(c.acks.size() == 2);
    CHECK(c.acks[0].secret == b.acks[0].secret);
    CHECK_FALSE(decode(Bytes{0}, c));
    CHECK_FALSE(decode(Bytes{1, 0, 0}, c));
  }
  {
    AnnounceBody b;
    b.keys = a.public_keys();
    b.name = "Ann";
    b.callsign = "W1AW";
    b.status = "at camp";
    AnnounceBody c;
    REQUIRE(decode(encode(b), c));
    CHECK(c.keys == b.keys);
    CHECK(c.name == "Ann");
    CHECK(c.callsign == "W1AW");
    CHECK(c.status == "at camp");
    b.callsign = "bad sign";
    REQUIRE(decode(encode(b), c));
    CHECK(c.callsign.empty());  // invalid callsigns are dropped by the encoder
  }
  {
    SosBody s;
    s.text = "broken leg, need evac";
    Position p = Position::from_degrees(51.5007, -0.1246);
    p.has_alt = true;
    p.alt_m = -12;
    p.has_accuracy = true;
    p.accuracy_m = 8;
    s.pos = p;
    s.seq = 9;
    SosBody t;
    REQUIRE(decode(encode(s), t));
    CHECK(t.text == s.text);
    CHECK(t.active);
    CHECK(t.seq == 9);
    REQUIRE(t.pos);
    CHECK(*t.pos == p);
    CHECK(t.pos->lat() == doctest::Approx(51.5007));
  }
  {
    CheckinBody c;
    c.status = CheckinStatus::Injured;
    c.note = "ankle";
    CheckinBody d;
    REQUIRE(decode(encode(c), d));
    CHECK(d.status == CheckinStatus::Injured);
    CHECK(d.note == "ankle");
    CHECK_FALSE(d.pos);
    CheckinStatus st;
    CHECK(parse_checkin_status("need_help", st));
    CHECK(st == CheckinStatus::NeedHelp);
    CHECK_FALSE(parse_checkin_status("fine", st));
  }
  {
    BulletinBody b;
    b.category = BulletinCategory::Water;
    b.title = "Clean well";
    b.text = "Well behind the school is clean, boil anyway.";
    b.pos = Position::from_degrees(-33.9, 151.2);
    BulletinBody c;
    REQUIRE(decode(encode(b), c));
    CHECK(c.title == b.title);
    CHECK(c.text == b.text);
    CHECK(c.category == BulletinCategory::Water);
    b.is_delete = true;
    b.delete_pkt_id = 99;
    REQUIRE(decode(encode(b), c));
    CHECK(c.is_delete);
    CHECK(c.delete_pkt_id == 99);
    BulletinCategory cat;
    CHECK(parse_bulletin_category("transport", cat));
    CHECK(cat == BulletinCategory::Transport);
  }
  {
    FileBody f;
    f.file_id = 42;
    f.index = 0;
    f.count = 3;
    f.total_size = 1000;
    f.name = "map.png";
    f.mime = "image/png";
    f.data = Bytes(100, 7);
    FileBody g;
    REQUIRE(decode(encode(f), g));
    CHECK(g.name == "map.png");
    CHECK(g.data.size() == 100);
    f.index = 2;
    REQUIRE(decode(encode(f), g));
    CHECK(g.name.empty());
    f.index = 3;
    CHECK_FALSE(decode(encode(f), g));
  }
  {
    NetPlanBody n;
    n.entries.push_back({"Camp net", 146520000, "voice", "100.0", "every hour :00", "simplex"});
    n.entries.push_back({"Data", 144390000, "afsk1200", "", "", ""});
    NetPlanBody m;
    REQUIRE(decode(encode(n), m));
    REQUIRE(m.entries.size() == 2);
    CHECK(m.entries[0].freq_hz == 146520000);
    CHECK(m.entries[0].tone == "100.0");
    CHECK(m.entries[1].mode == "afsk1200");
  }
  {
    PingBody p{0xDEADBEEF};
    PingBody q;
    REQUIRE(decode(encode(p), q));
    CHECK(q.nonce == 0xDEADBEEF);
    PongBody r{5, 3};
    PongBody s;
    REQUIRE(decode(encode(r), s));
    CHECK(s.ping_hops == 3);
    WhoisBody w{a.node_id()};
    WhoisBody v;
    REQUIRE(decode(encode(w), v));
    CHECK(v.target == a.node_id());
    AppBody ap{7, Bytes{1, 2}};
    AppBody aq;
    REQUIRE(decode(encode(ap), aq));
    CHECK(aq.app_type == 7);
    CHECK(aq.data == Bytes{1, 2});
  }
}

TEST_CASE("payload decoders survive fuzzing") {
  Identity a = make_id("a");
  SosBody s;
  s.text = "help";
  s.pos = Position::from_degrees(1, 2);
  BulletinBody b;
  b.title = "t";
  b.text = "hello there";
  AnnounceBody an;
  an.keys = a.public_keys();
  an.name = "n";
  NetPlanBody np;
  np.entries.push_back({"x", 1, "m", "", "", ""});
  std::vector<Bytes> seeds = {encode(s), encode(b), encode(an), encode(np), encode(TextBody{"hello", {}})};
  Xs x(99);
  for (int i = 0; i < 20000; ++i) {
    Bytes in = seeds[x.below(static_cast<uint32_t>(seeds.size()))];
    in[x.below(static_cast<uint32_t>(in.size()))] ^= static_cast<uint8_t>(x.next());
    if (x.below(2)) in.resize(x.below(static_cast<uint32_t>(in.size()) + 1));
    TextBody t; AckBody ak; ReadBody rd; AnnounceBody au; PositionBody po; SosBody so; SosAckBody sa;
    CheckinBody ci; BulletinBody bu; FileBody fi; NetPlanBody ne; PingBody pi; PongBody pg; WhoisBody wh; AppBody ap;
    decode(in, t); decode(in, ak); decode(in, rd); decode(in, au); decode(in, po); decode(in, so);
    decode(in, sa); decode(in, ci); decode(in, bu); decode(in, fi); decode(in, ne); decode(in, pi);
    decode(in, pg); decode(in, wh); decode(in, ap);
    CHECK(utf8_valid(t.text));
  }
}

TEST_CASE("signed packets: verify, reject tampering and wrong keys") {
  Identity a = make_id("a"), b = make_id("b");
  Bytes pkt = seal_signed(base_header(a), encode(TextBody{"hello world", {}}), a);
  PacketView p;
  REQUIRE(parse_packet(pkt, p));
  CHECK(verify_signed(p, a.sign_pk()));
  CHECK_FALSE(verify_signed(p, b.sign_pk()));
  int accepted = mutate_all(pkt, [&](const Bytes& m) {
    PacketView q;
    if (!parse_packet(m, q)) return false;
    // A mutation of only the hop byte is legitimately still valid.
    if (m.size() == pkt.size() && wire_id(m) == wire_id(pkt)) return false;
    return verify_signed(q, a.sign_pk());
  }, 7, 3000);
  CHECK(accepted == 0);
}

TEST_CASE("pairwise AEAD: both directions share a key, others cannot read") {
  Identity a = make_id("a"), b = make_id("b"), c = make_id("c");
  crypto::Key32 kab, kba, kac;
  REQUIRE(pair_key(a, b.dh_pk(), kab));
  REQUIRE(pair_key(b, a.dh_pk(), kba));
  REQUIRE(pair_key(a, c.dh_pk(), kac));
  CHECK(kab == kba);
  CHECK(kab != kac);
  Header h = base_header(a);
  h.dst_type = DestType::Node;
  h.dst_node = b.node_id();
  h.want_ack = true;
  AckSecret sec = ack_secret(kab, a.node_id(), h.pkt_id);
  h.ack_commit = ack_commit(sec);
  Bytes body = encode(TextBody{"secret plan", {}});
  Bytes pkt = seal_pair_aead(h, body, kab);
  // Ciphertext does not contain the plaintext.
  std::string s(pkt.begin(), pkt.end());
  CHECK(s.find("secret") == std::string::npos);
  PacketView p;
  REQUIRE(parse_packet(pkt, p));
  Bytes out;
  REQUIRE(open_pair_aead(p, kba, out));
  CHECK(out == body);
  CHECK_FALSE(open_pair_aead(p, kac, out));
  // Recipient derives the same ack secret; the commitment matches.
  CHECK(ack_commit(ack_secret(kba, p.h.origin, p.h.pkt_id)) == p.h.ack_commit);
  // Overhead: header 38 + salt 8 + mac 16 for an acknowledged DM.
  CHECK(pkt.size() == 46 + body.size() + 16);
  int accepted = mutate_all(pkt, [&](const Bytes& m) {
    PacketView q;
    Bytes o;
    if (!parse_packet(m, q)) return false;
    if (m.size() == pkt.size() && wire_id(m) == wire_id(pkt)) return false;
    return open_pair_aead(q, kab, o);
  }, 11, 3000);
  CHECK(accepted == 0);
  // PairMac.
  Bytes mac_pkt = seal_pair_mac(h, encode(PingBody{1}), kab);
  REQUIRE(parse_packet(mac_pkt, p));
  CHECK(verify_pair_mac(p, kba));
  CHECK_FALSE(verify_pair_mac(p, kac));
}

TEST_CASE("channels: key derivation, encryption, member signatures") {
  auto t0 = std::chrono::steady_clock::now();
  ChannelKey ch = derive_channel_key("  Camp   NORTH ", "river stone 42");
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  MESSAGE("channel key derivation (Argon2id 8 MiB x3) took " << ms << " ms in this build");
  CHECK(ch.encrypted);
  CHECK(ch.name == "camp north");
  ChannelKey same = channel_from_key("camp north", ch.key);
  CHECK(same.id == ch.id);
  ChannelKey other = channel_from_key("camp north", crypto::Key32{1});
  CHECK(other.id != ch.id);
  ChannelKey open1 = open_channel("Market"), open2 = open_channel("market ");
  CHECK(open1.id == open2.id);
  CHECK_FALSE(open1.encrypted);
  CHECK(open1.id != open_channel("markets").id);

  Identity a = make_id("a"), b = make_id("b");
  Header h = base_header(a);
  h.dst_type = DestType::Channel;
  Bytes body = encode(TextBody{"members only", {}});
  Bytes pkt = seal_chan(h, body, ch, a);
  PacketView p;
  REQUIRE(parse_packet(pkt, p));
  CHECK(p.h.channel == ch.id);
  Bytes out;
  crypto::Sig64 sig;
  REQUIRE(open_chan(p, ch, out, sig));
  CHECK(out == body);
  CHECK(verify_chan_sig(p, out, sig, a.sign_pk()));
  CHECK_FALSE(verify_chan_sig(p, out, sig, b.sign_pk()));
  CHECK_FALSE(open_chan(p, other, out, sig));
}

TEST_CASE("control messages") {
  Identity a = make_id("a");
  Beacon bc;
  bc.node = a.node_id();
  bc.has_time = true;
  bc.time_ms = 1700000000123ull;
  bc.time_quality = 3;
  bc.store_count = 300;
  bc.store_sum = 0xFEEDF00D;
  Bytes e = encode_beacon(bc);
  CHECK(e[0] == uint8_t(Ctrl::Beacon));
  Beacon d;
  REQUIRE(decode_beacon(ByteView(e).sub(1), d));
  CHECK(d.node == bc.node);
  CHECK(d.time_ms == bc.time_ms);
  CHECK(d.time_quality == 3);
  CHECK(d.store_count == 300);
  CHECK(d.store_sum == 0xFEEDF00D);
  CHECK(e.size() <= 32);

  SyncMsg m;
  m.target = 0x11223344;
  SyncItem dig;
  dig.type = SyncItem::Dig;
  dig.bits = 4;
  dig.prefix = 0xA0000000;
  dig.children[3] = {5, 77};
  SyncItem ids;
  ids.type = SyncItem::Ids;
  ids.bits = 8;
  ids.prefix = 0xAB000000;
  ids.ids = {0xAB000001, 0xABFFFFFF};
  SyncItem want;
  want.type = SyncItem::Want;
  want.ids = {1, 2, 3};
  m.items = {dig, ids, want};
  Bytes se = encode_sync(m);
  SyncMsg sd;
  REQUIRE(decode_sync(ByteView(se).sub(1), sd));
  CHECK(sd.target == m.target);
  REQUIRE(sd.items.size() == 3);
  CHECK(sd.items[0].children[3] == dig.children[3]);
  CHECK(sd.items[1].ids == ids.ids);
  CHECK(sd.items[2].ids == want.ids);
  CHECK(se.size() == 1 + 4 + 1 + sync_item_size(dig) + sync_item_size(ids) + sync_item_size(want));
  // An Ids item whose ids do not match its prefix is rejected, as is a non-aligned prefix.
  SyncMsg bad = m;
  bad.items[1].ids.push_back(0x12345678);
  CHECK_FALSE(decode_sync(ByteView(encode_sync(bad)).sub(1), sd));
  bad = m;
  bad.items[0].prefix = 0xA1000000;
  CHECK_FALSE(decode_sync(ByteView(encode_sync(bad)).sub(1), sd));

  FragNack n;
  n.target = 9;
  n.frag_id = 77;
  n.count = 10;
  n.missing = {0, 9};
  FragNack o;
  REQUIRE(decode_frag_nack(ByteView(encode_frag_nack(n)).sub(1), o));
  CHECK(o.missing == n.missing);
  CHECK(o.count == 10);

  Xs x(5);
  for (int i = 0; i < 20000; ++i) {
    Bytes in = x.below(2) ? se : e;
    in[x.below(static_cast<uint32_t>(in.size()))] ^= static_cast<uint8_t>(x.next());
    if (x.below(3) == 0) in.resize(x.below(static_cast<uint32_t>(in.size())) + 1);
    SyncMsg s2;
    Beacon b2;
    FragNack f2;
    decode_sync(ByteView(in).sub(1), s2);
    decode_beacon(ByteView(in).sub(1), b2);
    decode_frag_nack(ByteView(in).sub(1), f2);
    CHECK(s2.items.size() <= kMaxSyncItems);
  }
}

TEST_CASE("bundle container") {
  std::vector<Bytes> pkts = {Bytes{1, 2, 3}, Bytes(500, 9), Bytes{}};
  Bytes bun = encode_bundle(pkts);
  std::vector<ByteView> out;
  REQUIRE(decode_bundle(bun, out));
  REQUIRE(out.size() == 3);
  CHECK(out[1].to_bytes() == pkts[1]);
  for (size_t i = 0; i < bun.size(); ++i) {
    Bytes b = bun;
    b[i] ^= 0x40;
    CHECK_FALSE(decode_bundle(b, out));
  }
  CHECK_FALSE(decode_bundle(ByteView(bun).sub(0, bun.size() - 1), out));
  CHECK_FALSE(decode_bundle(Bytes{}, out));
  CHECK(decode_bundle(encode_bundle({}), out));
  CHECK(out.empty());
}

TEST_CASE("MsgId hex") {
  MsgId m{make_id("a").node_id(), 0x0A0B0C0D};
  std::string h = m.to_hex();
  CHECK(h.size() == 24);
  MsgId n;
  REQUIRE(MsgId::parse(h, n));
  CHECK(n == m);
  CHECK_FALSE(MsgId::parse(h.substr(1), n));
  CHECK_FALSE(MsgId::parse("zz" + h.substr(2), n));
}

}  // TEST_SUITE

TEST_SUITE("proto") {

TEST_CASE("v2: authenticated hop limit and hedged AEAD nonce salt") {
  crypto::Csprng rng(ByteView(std::string_view("proto-v2-test-seed-0123456789abcdef")));
  Identity a = Identity::generate(rng), b = Identity::generate(rng);
  Header h;
  h.kind = Kind::Text;
  h.origin = a.node_id();
  h.pkt_id = 77;
  h.created = 200000000;
  h.lifetime_min = 60;
  h.ttl = 2;
  h.hop_limit = 2;
  Bytes s = seal_signed(h, ByteView(std::string_view("hi")), a);
  PacketView p;
  REQUIRE(parse_packet(s, p));
  CHECK(p.h.hop_limit == 2);
  CHECK(verify_signed(p, a.public_keys().sign_pk));
  // the hop limit is authenticated
  Bytes t = s;
  t[kLimitByteOffset] = 0xF0;
  PacketView q;
  REQUIRE(parse_packet(t, q));
  CHECK_FALSE(verify_signed(q, a.public_keys().sign_pk));
  // ttl above the limit or reserved bits in the limit byte do not parse
  t = s;
  set_hop_byte(t, 3, 0);
  CHECK_FALSE(parse_packet(t, q));
  t = s;
  t[kLimitByteOffset] |= 0x01;
  CHECK_FALSE(parse_packet(t, q));
  // a relayed copy keeps verifying
  t = s;
  set_hop_byte(t, 1, 1);
  REQUIRE(parse_packet(t, q));
  CHECK(verify_signed(q, a.public_keys().sign_pk));
  CHECK(wire_id(t) == wire_id(s));

  // L3: same header, different plaintexts or entropy -> different salts / nonces
  crypto::Key32 k;
  REQUIRE(pair_key(a, b.public_keys().dh_pk, k));
  Header d = h;
  d.dst_type = DestType::Node;
  d.dst_node = b.node_id();
  uint8_t e1[16] = {1}, e2[16] = {2};
  Bytes x1 = seal_pair_aead(d, ByteView(std::string_view("attack at dawn")), k, ByteView(e1, 16));
  Bytes x2 = seal_pair_aead(d, ByteView(std::string_view("attack at dusk")), k, ByteView(e1, 16));
  Bytes x3 = seal_pair_aead(d, ByteView(std::string_view("attack at dawn")), k, ByteView(e2, 16));
  Bytes x4 = seal_pair_aead(d, ByteView(std::string_view("attack at dusk")), k);  // no entropy at all
  Bytes x5 = seal_pair_aead(d, ByteView(std::string_view("attack at dawn")), k);
  PacketView p1, p2, p3, p4, p5;
  REQUIRE(parse_packet(x1, p1));
  REQUIRE(parse_packet(x2, p2));
  REQUIRE(parse_packet(x3, p3));
  REQUIRE(parse_packet(x4, p4));
  REQUIRE(parse_packet(x5, p5));
  std::set<std::array<uint8_t, 8>> salts = {p1.h.salt, p2.h.salt, p3.h.salt, p4.h.salt, p5.h.salt};
  CHECK(salts.size() == 5);
  Bytes out;
  crypto::Key32 kb;
  REQUIRE(pair_key(b, a.public_keys().dh_pk, kb));
  REQUIRE(open_pair_aead(p2, kb, out));
  CHECK(std::string(out.begin(), out.end()) == "attack at dusk");
  // the salt is authenticated
  Bytes bad = x1;
  bad[header_size(p1.h) - 1] ^= 1;
  PacketView pb;
  REQUIRE(parse_packet(bad, pb));
  CHECK_FALSE(open_pair_aead(pb, kb, out));
}

}  // TEST_SUITE
