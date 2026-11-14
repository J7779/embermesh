// Mesh engine: multi-node simulations — topologies, loss, partitions, data mules, bridging,
// legal mode, fragmentation, clocks, queues, TTL, retries, quotas, bundles, persistence.
#include <map>
#include <set>

#include "doctest.h"
#include "mesh/sim.h"
#include "proto/packet.h"

using namespace sim;

namespace {

std::vector<Node*> line(Sim& s, Medium& m, int n, const MeshConfig& cfg = Sim::default_config()) {
  std::vector<Node*> v;
  for (int i = 0; i < n; ++i) {
    Node& x = s.add_node("n" + std::to_string(i), cfg);
    s.join(x, m);
    v.push_back(&x);
  }
  for (int i = 0; i < n; ++i)
    for (int j = 0; j < n; ++j)
      if (i != j && std::abs(i - j) > 1) m.connect(*v[i], *v[j], false, false);
  return v;
}

// Reassembles fragments seen on a medium so tests can inspect every packet that went on air.
struct AirInspector {
  std::map<std::pair<uint32_t, uint16_t>, std::map<uint8_t, Bytes>> frags;
  std::vector<Bytes> packets;      // complete packets seen
  std::vector<std::string> stations;
  size_t frames = 0;
  void feed(const Bytes& frame) {
    frames++;
    proto::FrameHeader h;
    ByteView body;
    if (!proto::parse_frame(frame, h, body)) return;
    stations.push_back(h.station);
    if (h.type == proto::FrameType::Packet) { packets.push_back(body.to_bytes()); return; }
    if (h.type != proto::FrameType::Fragment) return;
    proto::Fragment f;
    if (!proto::parse_fragment(body, f)) return;
    auto& m = frags[{h.hop_src, f.frag_id}];
    m[f.index] = f.data.to_bytes();
    if (m.size() == f.count) {
      Bytes unit;
      for (auto& kv : m) unit.insert(unit.end(), kv.second.begin(), kv.second.end());
      if (!unit.empty() && unit[0] == 0) packets.push_back(Bytes(unit.begin() + 1, unit.end()));
      frags.erase({h.hop_src, f.frag_id});
    }
  }
};

}  // namespace

TEST_SUITE("mesh") {

TEST_CASE("5-node line: DM end to end, broadcast reaches all, ack returns") {
  Sim s(10);
  Medium& r = s.add_medium(lora_caps());
  auto n = line(s, r, 5);
  s.introduce_all();
  s.run(5000);
  SendResult res = n[0]->eng->send_text(Dest::to_node(n[4]->nid()), "end of the line");
  REQUIRE(res.ok);
  n[0]->eng->send_text(Dest::broadcast(), "hello everyone");
  REQUIRE(s.run_until([&] { return n[4]->rec.got_text("end of the line"); }, 60000) != UINT64_MAX);
  CHECK(n[4]->rec.texts.back().rx.hops >= 3);
  CHECK(s.run_until([&] { return n[0]->rec.status_of(res.id) == MsgStatus::Delivered; }, 60000) != UINT64_MAX);
  CHECK(s.run_until([&] {
    for (int i = 1; i < 5; ++i) if (!n[i]->rec.got_text("hello everyone")) return false;
    return true;
  }, 60000) != UINT64_MAX);
  for (int i = 1; i < 5; ++i) CHECK(n[i]->rec.count_text("hello everyone") == 1);
  CHECK(n[0]->rec.status_of(res.id) == MsgStatus::Delivered);
}

TEST_CASE("25-node grid with collisions: flood coverage and suppression") {
  Sim s(11);
  Medium& r = s.add_medium(lora_caps());
  r.set_collisions(true);
  std::vector<Node*> g;
  MeshConfig gc = Sim::default_config();
  gc.default_ttl = 15;  // diameter is 8 hops; floods under collisions often arrive the long way round
  for (int i = 0; i < 25; ++i) {
    Node& x = s.add_node("g" + std::to_string(i), gc);
    s.join(x, r);
    g.push_back(&x);
  }
  for (int i = 0; i < 25; ++i)
    for (int j = 0; j < 25; ++j) {
      int dx = std::abs(i % 5 - j % 5), dy = std::abs(i / 5 - j / 5);
      if (i != j && dx + dy > 1) r.connect(*g[i], *g[j], false, false);
    }
  s.introduce_all();
  s.run(60000);  // let announcements / beacons settle
  uint64_t air_before = r.frames_on_air;
  g[0]->eng->send_text(Dest::broadcast(), "grid flood");
  uint64_t t = s.run_until([&] {
    for (auto* x : g) if (x != g[0] && !x->rec.got_text("grid flood")) return false;
    return true;
  }, 300000);
  CHECK(t != UINT64_MAX);
  MESSAGE("25-node grid: broadcast reached all 24 in " << t << " ms, " << (r.frames_on_air - air_before)
          << " frames on air (incl. beacons/sync), collisions so far " << r.frames_collided);
  SendResult dm = g[0]->eng->send_text(Dest::to_node(g[24]->nid()), "corner to corner");
  uint64_t t2 = s.run_until([&] { return g[24]->rec.got_text("corner to corner"); }, 300000);
  CHECK(t2 != UINT64_MAX);
  CHECK(s.run_until([&] { return g[0]->rec.status_of(dm.id) == MsgStatus::Delivered; }, 300000) != UINT64_MAX);
  uint64_t suppressed = 0;
  for (auto* x : g) suppressed += x->eng->stats().suppressed;
  MESSAGE("grid DM corner->corner " << t2 << " ms; rebroadcasts suppressed " << suppressed);
  for (auto* x : g)
    if (x != g[0]) CHECK(x->rec.count_text("grid flood") == 1);
}

TEST_CASE("partition then heal: store reconciliation delivers backlog") {
  Sim s(12);
  Medium& lan = s.add_medium(lan_caps());
  std::vector<Node*> v;
  for (int i = 0; i < 4; ++i) {
    Node& x = s.add_node("p" + std::to_string(i));
    s.join(x, lan);
    v.push_back(&x);
  }
  s.introduce_all();
  s.run(3000);
  auto cut = [&](bool on) {
    for (int i : {0, 1})
      for (int j : {2, 3}) lan.connect(*v[i], *v[j], on);
  };
  cut(false);
  std::vector<SendResult> sent;
  for (int k = 0; k < 10; ++k)
    sent.push_back(v[0]->eng->send_text(Dest::to_node(v[3]->nid()), "while apart " + std::to_string(k)));
  v[1]->eng->send_text(Dest::broadcast(), "public while apart");
  s.run(60000);
  CHECK_FALSE(v[3]->rec.got_text("while apart 0"));
  CHECK(v[0]->rec.got_text("public while apart"));
  CHECK_FALSE(v[2]->rec.got_text("public while apart"));
  cut(true);
  uint64_t t = s.run_until([&] {
    for (int k = 0; k < 10; ++k) if (!v[3]->rec.got_text("while apart " + std::to_string(k))) return false;
    return v[2]->rec.got_text("public while apart");
  }, 120000);
  CHECK(t != UINT64_MAX);
  MESSAGE("partition heal: 10 DMs + 1 broadcast reconciled " << t << " ms after healing");
  CHECK(s.run_until([&] {
    for (auto& r : sent) if (v[0]->rec.status_of(r.id) != MsgStatus::Delivered) return false;
    return true;
  }, 120000) != UINT64_MAX);
  uint64_t rounds = 0;
  for (auto* x : v) rounds += x->eng->stats().sync_rounds;
  CHECK(rounds > 0);
}

TEST_CASE("data mule carries messages between islands that never connect") {
  Sim s(13);
  Medium& islandA = s.add_medium(lan_caps());
  Medium& islandB = s.add_medium(lora_caps());
  Node& a1 = s.add_node("a1");
  Node& a2 = s.add_node("a2");
  Node& b1 = s.add_node("b1");
  Node& b2 = s.add_node("b2");
  Node& mule = s.add_node("mule");
  s.join(a1, islandA);
  s.join(a2, islandA);
  s.join(b1, islandB);
  s.join(b2, islandB);
  SimLink& ma = s.join(mule, islandA);
  SimLink& mb = s.join(mule, islandB);
  s.introduce_all();
  mb.set_up(false);
  s.run(3000);
  SendResult r1 = a1.eng->send_text(Dest::to_node(b2.nid()), "news from camp A");
  b1.eng->send_text(Dest::broadcast(), "camp B is safe");
  s.run(30000);  // mule sits in camp A and picks up the store
  ma.set_up(false);
  s.run(600000);  // ten minutes walking: nothing connected
  CHECK_FALSE(b2.rec.got_text("news from camp A"));
  mb.set_up(true);  // arrives in camp B
  REQUIRE(s.run_until([&] { return b2.rec.got_text("news from camp A"); }, 300000) != UINT64_MAX);
  CHECK(b2.rec.texts.back().rx.encrypted);
  CHECK(s.run_until([&] { return mule.rec.got_text("camp B is safe"); }, 300000) != UINT64_MAX);
  // Mule walks back with the ack and B's news.
  s.run(30000);
  mb.set_up(false);
  s.run(300000);
  ma.set_up(true);
  CHECK(s.run_until([&] { return a1.rec.got_text("camp B is safe") &&
                                 a1.rec.status_of(r1.id) == MsgStatus::Delivered; }, 300000) != UINT64_MAX);
}

TEST_CASE("30% loss on a 5-node line: everything is eventually delivered exactly once") {
  Sim s(14);
  Medium& r = s.add_medium(lora_caps());
  r.set_loss(0.30);
  auto n = line(s, r, 5);
  s.introduce_all();
  s.run(10000);
  std::vector<SendResult> dms;
  for (int k = 0; k < 10; ++k) {
    dms.push_back(n[0]->eng->send_text(Dest::to_node(n[4]->nid()), "lossy dm " + std::to_string(k)));
    n[4]->eng->send_text(Dest::broadcast(), "lossy bc " + std::to_string(k));
    s.run(2000);
  }
  auto count = [&] {
    int got = 0;
    for (int k = 0; k < 10; ++k) got += n[4]->rec.got_text("lossy dm " + std::to_string(k));
    for (int k = 0; k < 10; ++k) got += n[0]->rec.got_text("lossy bc " + std::to_string(k));
    return got;
  };
  s.run(60000);
  int at_60s = count();
  uint64_t t = s.run_until([&] { return count() == 20; }, 900000);
  int delivered = 0;
  for (auto& d : dms) delivered += n[0]->rec.status_of(d.id) == MsgStatus::Delivered;
  MESSAGE("30% loss, 4 hops: " << at_60s << "/20 delivered 60 s after the last send; all 20 after "
          << (t == UINT64_MAX ? -1 : static_cast<long long>(t + 60000)) << " ms; DM acks back: " << delivered << "/10");
  CHECK(t != UINT64_MAX);
  CHECK(s.run_until([&] {
    int d = 0;
    for (auto& x : dms) d += n[0]->rec.status_of(x.id) == MsgStatus::Delivered;
    return d == 10;
  }, 900000) != UINT64_MAX);
  for (int k = 0; k < 10; ++k) CHECK(n[4]->rec.count_text("lossy dm " + std::to_string(k)) == 1);
}

TEST_CASE("MTU 60: long text and a file are fragmented and reassembled, with loss") {
  Sim s(15);
  LinkCaps c = lora_caps();
  c.mtu = 60;
  Medium& r = s.add_medium(c);
  r.set_loss(0.10);
  Node& a = s.add_node("a");
  Node& b = s.add_node("b");
  s.join(a, r);
  s.join(b, r);
  s.introduce(a, b);
  s.run(5000);
  std::string longtext;
  for (int i = 0; i < 40; ++i) longtext += "word" + std::to_string(i) + " ";
  SendResult t = a.eng->send_text(Dest::to_node(b.nid()), longtext);
  REQUIRE(t.ok);
  Bytes file(1500);
  for (size_t i = 0; i < file.size(); ++i) file[i] = static_cast<uint8_t>(i * 7 + 1);
  SendResult f = a.eng->send_file(Dest::to_node(b.nid()), "f.bin", "application/octet-stream", file);
  REQUIRE(f.ok);
  CHECK(s.run_until([&] { return b.rec.got_text(longtext); }, 300000) != UINT64_MAX);
  CHECK(s.run_until([&] { return !b.rec.files.empty(); }, 600000) != UINT64_MAX);
  if (!b.rec.files.empty()) CHECK(b.rec.files[0].data == file);
  CHECK(s.run_until([&] { return a.rec.status_of(t.id) == MsgStatus::Delivered &&
                                 a.rec.status_of(f.id) == MsgStatus::Delivered; }, 600000) != UINT64_MAX);
  for (auto* l : {a.links[0].get(), b.links[0].get()}) (void)l;
}

TEST_CASE("bridging: fast LAN <-> slow radio gateway, both directions") {
  Sim s(16);
  Medium& lan = s.add_medium(lan_caps());
  Medium& radio = s.add_medium(afsk_caps());
  Node& phone = s.add_node("phone");
  Node& gw = s.add_node("gateway");
  Node& far1 = s.add_node("far1");
  Node& far2 = s.add_node("far2");
  s.join(phone, lan);
  s.join(gw, lan);
  s.join(gw, radio);
  s.join(far1, radio);
  s.join(far2, radio);
  radio.connect(gw, far2, false);  // far2 only reachable via far1
  s.introduce_all();
  s.run(10000);
  SendResult r = phone.eng->send_text(Dest::to_node(far2.nid()), "to the hills");
  REQUIRE(s.run_until([&] { return far2.rec.got_text("to the hills"); }, 300000) != UINT64_MAX);
  far2.eng->send_text(Dest::to_node(phone.nid()), "received, thanks");
  CHECK(s.run_until([&] { return phone.rec.got_text("received, thanks"); }, 300000) != UINT64_MAX);
  CHECK(s.run_until([&] { return phone.rec.status_of(r.id) == MsgStatus::Delivered; }, 300000) != UINT64_MAX);
  CHECK(gw.eng->stats().relayed > 0);
  auto st = gw.eng->stats();
  for (auto& ls : st.per_link) CHECK(ls.queue_bytes <= gw.eng->config().link_queue_max_bytes);
}

TEST_CASE("legal mode: no ciphertext and always a callsign on encryption_allowed=false links") {
  Sim s(17);
  Medium& lan = s.add_medium(lan_caps());
  LinkCaps hc = afsk_caps();
  hc.encryption_allowed = false;
  hc.requires_station_id = true;
  Medium& ham = s.add_medium(hc);
  Node& a = s.add_node("a");
  Node& gw = s.add_node("gw");
  Node& h = s.add_node("h");
  s.join(a, lan);
  s.join(gw, lan);
  s.join(gw, ham);
  s.join(h, ham);
  AirInspector air;
  s.tx_hook = [&](SimLink& from, const Bytes& f) {
    if (from.medium() == &ham) air.feed(f);
  };
  s.introduce_all();
  // Without callsigns nothing may be transmitted on the ham link at all.
  s.run(20000);
  CHECK(air.frames == 0);
  gw.eng->set_profile("gw", "QX0GW", "");
  h.eng->set_profile("h", "QX0HH", "");
  a.eng->send_text(Dest::to_node(h.nid()), "secret dm must not cross");
  proto::ChannelKey ch = proto::channel_from_key("priv", crypto::Key32{7});
  a.eng->join_channel(ch);
  gw.eng->join_channel(ch);
  h.eng->join_channel(ch);
  a.eng->send_text(Dest::to_channel(ch.id), "private channel must not cross");
  a.eng->send_text(Dest::broadcast(), "public may cross");
  a.eng->sos_start("help", std::nullopt, 60);
  REQUIRE(s.run_until([&] { return h.rec.got_text("public may cross") && !h.rec.sos.empty(); }, 300000) != UINT64_MAX);
  s.run(300000);  // plenty of time for retries and reconciliation to try their luck
  CHECK_FALSE(h.rec.got_text("secret dm must not cross"));
  CHECK_FALSE(h.rec.got_text("private channel must not cross"));
  CHECK(air.frames > 0);
  size_t checked = 0;
  for (const Bytes& p : air.packets) {
    proto::PacketView pv;
    REQUIRE(proto::parse_packet(p, pv));
    CHECK(proto::legal_cleartext(pv.h.sec));
    checked++;
  }
  for (const std::string& st : air.stations) CHECK((st == "QX0GW" || st == "QX0HH"));
  CHECK(gw.eng->stats().per_link.size() == 2);
  uint64_t blocked = 0;
  for (auto& ls : gw.eng->stats().per_link) blocked += ls.blocked_legal;
  CHECK(blocked > 0);
  MESSAGE("legal link: " << air.frames << " frames, " << checked << " packets inspected, " << blocked
          << " ciphertext transmissions blocked at the gateway");
}

TEST_CASE("hostile input: garbage, bit flips, truncation, replays and forgeries never crash or get accepted") {
  Sim s(18);
  Medium& lan = s.add_medium(lan_caps());
  Node& a = s.add_node("a");
  Node& b = s.add_node("b");
  Node& victim = s.add_node("victim");
  SimLink& la = s.join(a, lan);
  s.join(b, lan);
  s.join(victim, lan);
  la.log_frames = true;
  s.introduce_all();
  s.run(3000);
  a.eng->send_text(Dest::to_node(victim.nid()), "genuine dm");
  a.eng->send_text(Dest::broadcast(), "genuine public");
  proto::BulletinBody bl;
  bl.title = "t";
  bl.text = "genuine bulletin";
  a.eng->post_bulletin(bl);
  s.run(5000);
  REQUIRE(victim.rec.got_text("genuine dm"));
  std::vector<Bytes> captured = la.sent_log;
  REQUIRE(captured.size() > 3);
  size_t texts_before = victim.rec.texts.size();
  size_t bulletins_before = victim.rec.bulletins.size();
  Rng rng(1234);
  RxMeta meta;
  uint32_t vl = victim.links[0]->id();
  for (int i = 0; i < 30000; ++i) {
    Bytes f;
    switch (rng.below(6)) {
      case 0:  // pure garbage
        f.resize(rng.below(300));
        for (auto& x : f) x = static_cast<uint8_t>(rng.next());
        break;
      case 1:  // replay
        f = captured[rng.below(static_cast<uint32_t>(captured.size()))];
        break;
      case 2: {  // bit flips
        f = captured[rng.below(static_cast<uint32_t>(captured.size()))];
        int flips = 1 + static_cast<int>(rng.below(3));
        for (int k = 0; k < flips; ++k) f[rng.below(static_cast<uint32_t>(f.size()))] ^= static_cast<uint8_t>(1u << rng.below(8));
        break;
      }
      case 3:  // truncation
        f = captured[rng.below(static_cast<uint32_t>(captured.size()))];
        f.resize(rng.below(static_cast<uint32_t>(f.size())));
        break;
      case 4: {  // forged: valid structure, claims to be from a, random signature/mac
        proto::Header h;
        h.kind = proto::Kind::Text;
        h.origin = a.nid();
        h.pkt_id = static_cast<uint32_t>(rng.next()) | 1;
        h.created = proto::unix_ms_to_wire(victim.eng->now_ms());
        h.lifetime_min = 60;
        h.dst_type = rng.below(2) ? proto::DestType::Broadcast : proto::DestType::Node;
        h.dst_node = victim.nid();
        h.sec = h.dst_type == proto::DestType::Node ? proto::Sec::PairAead : proto::Sec::Signed;
        ByteWriter w;
        proto::write_frame_header(w, proto::FrameHeader{proto::FrameType::Packet, "", 0x01020304});
        proto::write_header(w, h);
        Bytes junk(rng.below(64) + 64);
        for (auto& x : junk) x = static_cast<uint8_t>(rng.next());
        w.bytes(junk);
        f = w.take();
        break;
      }
      default: {  // structurally valid fragments / control frames with random content
        ByteWriter w;
        proto::FrameType ft = rng.below(2) ? proto::FrameType::Fragment : proto::FrameType::Control;
        proto::write_frame_header(w, proto::FrameHeader{ft, "", static_cast<uint32_t>(rng.below(8))});
        Bytes junk(rng.below(120) + 1);
        for (auto& x : junk) x = static_cast<uint8_t>(rng.next());
        if (ft == proto::FrameType::Fragment && junk.size() > 4) { junk[3] = static_cast<uint8_t>(2 + rng.below(4)); junk[2] = static_cast<uint8_t>(rng.below(junk[3])); }
        w.bytes(junk);
        f = w.take();
        break;
      }
    }
    victim.eng->on_frame(vl, f, meta);
    if (i % 500 == 0) s.step();
  }
  s.run(10000);
  CHECK(victim.rec.texts.size() == texts_before);
  CHECK(victim.rec.bulletins.size() == bulletins_before);
  auto st = victim.eng->stats();
  CHECK(st.rx_bad > 1000);
  CHECK(st.dropped_auth > 0);
  CHECK(st.dropped_duplicates > 0);
  MESSAGE("fuzz: rx_bad=" << st.rx_bad << " dropped_auth=" << st.dropped_auth << " dups=" << st.dropped_duplicates
          << " stored=" << st.stored_messages << " peers=" << victim.eng->peers().size());
  CHECK(victim.eng->peers().size() <= 2048);
  // The victim still works afterwards.
  a.eng->send_text(Dest::to_node(victim.nid()), "still alive?");
  CHECK(s.run_until([&] { return victim.rec.got_text("still alive?"); }, 10000) != UINT64_MAX);
}

TEST_CASE("mesh time: an unset clock converges on a GPS-disciplined contact; a sane clock is never moved") {
  Sim s(19);
  Medium& lan = s.add_medium(lan_caps());
  Node& gps = s.add_node("gps");
  Node& skew = s.add_node("skew");
  Node& unset = s.add_node("unset");
  Node& far = s.add_node("far");
  skew.clock_skew_ms = 3 * 3600 * 1000;                    // 3 hours fast
  unset.clock_skew_ms = -static_cast<int64_t>(s.now()) + 5000;  // clock reads 1970
  s.join(gps, lan);
  s.join(skew, lan);
  s.join(unset, lan);
  s.join(far, lan);
  lan.connect(gps, far, false);  // far only hears skew/unset
  s.introduce_all();
  s.step();
  gps.eng->set_time(s.now(), TimeSource::Gps);
  s.run(120000);
  auto err = [&](Node& n) { return std::llabs(static_cast<long long>(n.eng->now_ms()) - static_cast<long long>(s.now())); };
  // A sane system clock always wins, even when it is wrong (security review C1): only the node
  // without a clock learns mesh time, from authenticated announcements of its contacts.
  CHECK(std::llabs(static_cast<long long>(err(skew)) - 3 * 3600 * 1000ll) < 2000);
  CHECK(skew.eng->time_info().source == TimeSource::System);
  CHECK(err(unset) < 2000);
  CHECK(unset.eng->time_info().source == TimeSource::Mesh);
  CHECK(unset.eng->time_info().quality >= 9);
  CHECK(err(far) < 2000);
  // Messages from the formerly-unset node carry sane timestamps and are delivered.
  unset.eng->send_text(Dest::to_node(gps.nid()), "what time is it");
  CHECK(s.run_until([&] { return gps.rec.got_text("what time is it"); }, 10000) != UINT64_MAX);
  if (!gps.rec.texts.empty()) CHECK(std::llabs(static_cast<long long>(gps.rec.texts.back().rx.created_ms) - static_cast<long long>(s.now())) < 5000);
  MESSAGE("mesh time error after 2 min: skew=" << err(skew) << " ms unset=" << err(unset) << " ms far=" << err(far) << " ms");
}

TEST_CASE("queue overflow on a 1200 bd link: bounded queue, SOS jumps the backlog") {
  Sim s(20);
  Medium& r = s.add_medium(afsk_caps());
  MeshConfig qc = Sim::default_config();
  qc.link_queue_max_bytes = 16 * 1024;
  Node& a = s.add_node("a", qc);
  Node& b = s.add_node("b");
  s.join(a, r);
  s.join(b, r);
  s.introduce_all();
  s.run(5000);
  for (int k = 0; k < 400; ++k)
    a.eng->send_text(Dest::broadcast(), "bulk chatter message number " + std::to_string(k) + " with some padding text");
  auto st = a.eng->stats();
  CHECK(st.per_link[0].queue_bytes <= a.eng->config().link_queue_max_bytes);
  CHECK(st.per_link[0].dropped_queue_full > 0);
  a.eng->sos_start("SOS SOS", std::nullopt, 60);
  REQUIRE(s.run_until([&] { return !b.rec.sos.empty(); }, 30000) != UINT64_MAX);
  size_t bulk_before_sos = b.rec.texts.size();
  MESSAGE("1200 bd backlog: SOS arrived after " << bulk_before_sos << " of 400 queued bulk messages");
  CHECK(bulk_before_sos < 10);
  a.eng->sos_stop();
}

TEST_CASE("TTL exhaustion is a strict hop limit (flooding and reconciliation)") {
  Sim s(21);
  Medium& lan = s.add_medium(lan_caps());
  auto n = line(s, lan, 5);
  s.introduce_all();
  s.run(3000);
  SendOptions o;
  o.ttl_hops = 2;
  n[0]->eng->send_text(Dest::broadcast(), "two hops only", o);
  s.run(120000);  // long enough for several sync rounds
  CHECK(n[1]->rec.got_text("two hops only"));
  CHECK(n[2]->rec.got_text("two hops only"));
  CHECK_FALSE(n[3]->rec.got_text("two hops only"));
  CHECK_FALSE(n[4]->rec.got_text("two hops only"));
  CHECK(n[2]->eng->stats().dropped_ttl > 0);
  o.ttl_hops = 1;
  n[0]->eng->send_text(Dest::broadcast(), "neighbours only", o);
  s.run(30000);
  CHECK(n[1]->rec.got_text("neighbours only"));
  CHECK_FALSE(n[2]->rec.got_text("neighbours only"));
}

TEST_CASE("ack loss: sender retries with backoff, receiver re-acks, delivered exactly once") {
  Sim s(22);
  Medium& lan = s.add_medium(lan_caps());
  Node& a = s.add_node("a");
  Node& b = s.add_node("b");
  s.join(a, lan);
  s.join(b, lan);
  s.introduce(a, b);
  MeshConfig c = a.eng->config();
  c.sync_enabled = false;  // isolate the end-to-end retry machinery
  a.eng->set_config(c);
  b.eng->set_config(c);
  s.run(3000);
  lan.connect(b, a, false, false);  // b -> a direction dead: acks are lost
  SendResult r = a.eng->send_text(Dest::to_node(b.nid()), "did you get this?");
  REQUIRE(s.run_until([&] { return b.rec.got_text("did you get this?"); }, 5000) != UINT64_MAX);
  s.run(120000);
  CHECK(a.eng->status(r.id) != MsgStatus::Delivered);
  uint64_t retries = a.eng->stats().retries;
  CHECK(retries >= 2);
  CHECK(retries <= 4);  // 20 s, 40 s, 80 s ... exponential backoff
  lan.connect(b, a, true, false);
  CHECK(s.run_until([&] { return a.rec.status_of(r.id) == MsgStatus::Delivered; }, 400000) != UINT64_MAX);
  CHECK(b.rec.count_text("did you get this?") == 1);
  MESSAGE("ack loss: " << a.eng->stats().retries << " retries until delivered");
}

TEST_CASE("storage quota: eviction keeps own and emergency traffic") {
  Sim s(23);
  Medium& lan = s.add_medium(lan_caps());
  MeshConfig small = Sim::default_config();
  small.store_max_packets = 60;
  Node& a = s.add_node("a", small);
  Node& b = s.add_node("b");
  s.join(a, lan);
  s.join(b, lan);
  s.introduce_all();
  s.run(3000);
  b.eng->sos_start("help me", std::nullopt, 60);
  SendResult own = a.eng->send_text(Dest::to_node(b.nid()), "mine");
  (void)own;
  for (int k = 0; k < 200; ++k) {
    b.eng->send_text(Dest::broadcast(), "chatter " + std::to_string(k));
    b.eng->send_position(Dest::broadcast(), Position::from_degrees(k * 0.01, 1));
    if (k % 20 == 0) s.run(500);
  }
  s.run(20000);
  auto st = a.eng->stats();
  CHECK(st.stored_messages <= 60);
  CHECK(st.store_evicted > 0);
  // SOS from b is still carried by a.
  bool has_sos = false;
  uint32_t n = 0;
  Bytes bun = a.eng->export_bundle(BundleFilter{}, &n);
  std::vector<ByteView> pk;
  REQUIRE(proto::decode_bundle(bun, pk));
  for (ByteView p : pk) {
    proto::PacketView pv;
    if (proto::parse_packet(p, pv) && pv.h.kind == proto::Kind::Sos) has_sos = true;
  }
  CHECK(has_sos);
  // Replaceable kinds keep only the latest position per origin.
  int positions = 0;
  for (ByteView p : pk) {
    proto::PacketView pv;
    if (proto::parse_packet(p, pv) && pv.h.kind == proto::Kind::Position) positions++;
  }
  CHECK(positions <= 1);
  b.eng->sos_stop();
}

TEST_CASE("sneakernet bundle round trip") {
  Sim s(24);
  Medium& lan = s.add_medium(lan_caps());
  Node& a = s.add_node("a");
  Node& b = s.add_node("b");
  Node& c = s.add_node("c");  // never connected
  s.join(a, lan);
  s.join(b, lan);
  s.introduce_all();
  s.step();
  c.eng->tick(s.now());
  s.run(3000);
  a.eng->send_text(Dest::to_node(c.nid()), "carried on a usb stick");
  a.eng->send_text(Dest::broadcast(), "public news");
  proto::BulletinBody bl;
  bl.title = "water";
  bl.text = "well is clean";
  a.eng->post_bulletin(bl);
  s.run(3000);
  uint32_t count = 0;
  Bytes bundle = b.eng->export_bundle(BundleFilter{}, &count);  // b carries a's traffic
  CHECK(count >= 4);
  c.eng->tick(s.now());
  BundleImportResult r = c.eng->import_bundle(bundle);
  CHECK(r.ok);
  CHECK(r.imported == count);
  CHECK(r.for_me == 1);
  for (int i = 0; i < 20; ++i) c.eng->tick(s.now() + i * 10);
  CHECK(c.rec.got_text("carried on a usb stick"));
  CHECK(c.rec.got_text("public news"));
  CHECK(c.rec.texts.front().rx.from_bundle);
  CHECK_FALSE(c.rec.bulletins.empty());
  BundleImportResult again = c.eng->import_bundle(bundle);
  CHECK(again.imported == 0);
  CHECK(again.duplicates == count);
  Bytes bad = bundle;
  bad[bad.size() / 2] ^= 1;
  CHECK_FALSE(c.eng->import_bundle(bad).ok);
  BundleFilter f;
  f.scope = BundleFilter::ToDest;
  f.dest = Dest::to_node(c.nid());
  uint32_t n2 = 0;
  a.eng->export_bundle(f, &n2);
  CHECK(n2 == 1);
  f.max_bytes = 50;
  a.eng->export_bundle(f, &n2);
  CHECK(n2 == 0);
}

TEST_CASE("duty cycle budget is respected on a 1% link") {
  Sim s(25);
  LinkCaps c = lora_caps();
  c.duty_cycle_permille = 10;
  Medium& r = s.add_medium(c);
  Node& a = s.add_node("a");
  Node& b = s.add_node("b");
  s.join(a, r);
  s.join(b, r);
  s.introduce_all();
  s.run(1000);
  for (int k = 0; k < 200; ++k) a.eng->send_text(Dest::broadcast(), "duty test message " + std::to_string(k));
  s.run(30 * 60000);
  auto st = a.eng->stats();
  uint64_t air = st.per_link[0].airtime_ms;
  // 1 % of 31 minutes plus the initial bucket (1 % of 10 minutes) and one frame of slack.
  uint64_t budget = 31 * 60000 / 100 + 6000 + 1000;
  MESSAGE("1% duty: " << air << " ms airtime in 31 min (budget " << budget << " ms), duty now "
          << st.per_link[0].duty_used_permille << " permille, delivered " << b.rec.texts.size());
  CHECK(air <= budget);
  CHECK(b.rec.texts.size() > 0);
}

TEST_CASE("persistence: store and state survive a restart") {
  struct Mem : MeshPersistence {
    std::map<uint64_t, std::pair<Bytes, StoredMeta>> rows;
    int dirty = 0;
    void store_put(uint64_t w, ByteView p, const StoredMeta& m) override { rows[w] = {p.to_bytes(), m}; }
    void store_remove(uint64_t w) override { rows.erase(w); }
    void state_dirty() override { dirty++; }
  } mem;
  Sim s(26);
  Medium& lan = s.add_medium(lan_caps());
  Node& a = s.add_node("a");
  Node& b = s.add_node("b");
  a.eng->set_persistence(&mem);
  s.join(a, lan);
  SimLink& lb = s.join(b, lan);
  s.run(3000);  // announcements exchanged
  lb.set_up(false);
  SendResult r = a.eng->send_text(Dest::to_node(b.nid()), "survive restart");
  REQUIRE(r.ok);
  s.run(1000);
  CHECK_FALSE(mem.rows.empty());
  Json state = a.eng->save_state();
  // "Restart" a: new engine from the persisted rows.
  auto a2 = std::make_unique<MeshEngine>(a.id, ByteView(std::string_view("fresh entropy for the restarted engine")),
                                         Sim::default_config());
  a2->set_delegate(&a.rec);
  a2->tick(s.now());
  for (auto& kv : mem.rows) CHECK(a2->restore_packet(kv.second.first, kv.second.second));
  CHECK(a2->load_state(state));
  CHECK(a2->has_keys(b.nid()));  // from the stored announcement
  CHECK(a2->status(r.id) != MsgStatus::Unknown);
  CHECK(a2->stats().stored_messages == mem.rows.size());
  a.eng = std::move(a2);
  a.eng->attach_link(a.links[0]->id(), a.links[0].get());
  lb.set_up(true);
  CHECK(s.run_until([&] { return b.rec.got_text("survive restart"); }, 120000) != UINT64_MAX);
  CHECK(s.run_until([&] { return a.eng->status(r.id) == MsgStatus::Delivered; }, 120000) != UINT64_MAX);
  CHECK(b.rec.count_text("survive restart") == 1);
}

}  // TEST_SUITE
