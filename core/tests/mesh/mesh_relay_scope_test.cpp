// Internet relay sessions at the mesh level (docs/CONNECTIVITY.md §4.2, §4.7, §4.9): every session is
// its own scoped point-to-point link. Tests named after the review items they prove.
#include <algorithm>

#include "doctest.h"
#include "sim.h"

using namespace ember;
using namespace ember::mesh;
using namespace sim;

namespace {

LinkCaps session_caps() {
  LinkCaps c;
  c.mtu = 4000;
  c.bitrate_bps = 64000;
  c.broadcast = false;
  c.half_duplex = false;
  c.reliable = false;
  c.encryption_allowed = true;
  c.cost = 30;
  return c;
}

LinkCaps lan_link_caps() {
  LinkCaps c;
  c.mtu = 1400;
  c.bitrate_bps = 10000000;
  c.broadcast = true;
  c.half_duplex = false;
  c.cost = 1;
  return c;
}

void scope_peer(Node& n, SimLink& l, const Node& peer) {
  LinkScope s;
  s.restricted = true;
  s.relay_session = true;
  s.has_peer = true;
  s.peer = peer.nid();
  n.eng->set_link_scope(l.id(), s);
}

bool contains(const Bytes& hay, const uint8_t* needle, size_t n) {
  return std::search(hay.begin(), hay.end(), needle, needle + n) != hay.end();
}

}  // namespace

TEST_SUITE("mesh") {

TEST_CASE("relay H1: a contact never receives anything of my traffic with another contact") {
  Sim sim(31);
  Node& m = sim.add_node("m");
  Node& a = sim.add_node("a");
  Node& b = sim.add_node("b");
  sim.introduce(m, a);
  sim.introduce(m, b);  // a and b do not know each other
  Medium& ma = sim.add_medium(session_caps(), 40);
  Medium& mb = sim.add_medium(session_caps(), 40);
  SimLink& m_a = sim.join(m, ma);
  SimLink& a_m = sim.join(a, ma);
  SimLink& m_b = sim.join(m, mb);
  SimLink& b_m = sim.join(b, mb);
  scope_peer(m, m_a, a);
  scope_peer(a, a_m, m);
  scope_peer(m, m_b, b);
  scope_peer(b, b_m, m);
  std::vector<Bytes> on_ma;
  sim.tx_hook = [&](SimLink& from, const Bytes& f) {
    if (from.medium() == &ma) on_ma.push_back(f);
  };
  sim.run(5000);
  m.eng->send_text(Dest::to_node(b.nid()), "m to b, private");
  b.eng->send_text(Dest::to_node(m.nid()), "b to m, private");
  m.eng->send_text(Dest::broadcast(), "a public post by m");  // the Public channel never crosses a session
  m.eng->send_text(Dest::to_node(a.nid()), "m to a");
  sim.run(10 * 60000);
  CHECK(b.rec.got_text("m to b, private"));
  CHECK(m.rec.got_text("b to m, private"));
  CHECK(a.rec.got_text("m to a"));
  CHECK_FALSE(a.rec.got_text("a public post by m"));
  NodeId bid = b.nid();
  REQUIRE(!on_ma.empty());
  for (const Bytes& f : on_ma) CHECK_FALSE(contains(f, bid.data(), bid.size()));  // no packet to/from b, no b announce
  CHECK(a.eng->peer(b.nid()) == std::nullopt);  // a never even learned that b exists
}

TEST_CASE("relay H7: a session stays silent while its contact is heard directly elsewhere") {
  Sim sim(32);
  Node& m = sim.add_node("m");
  Node& a = sim.add_node("a");
  sim.introduce(m, a);
  Medium& lan = sim.add_medium(lan_link_caps());
  Medium& rel = sim.add_medium(session_caps(), 40);
  SimLink& m_lan = sim.join(m, lan);
  SimLink& a_lan = sim.join(a, lan);
  SimLink& m_rel = sim.join(m, rel);
  SimLink& a_rel = sim.join(a, rel);
  scope_peer(m, m_rel, a);
  scope_peer(a, a_rel, m);
  sim.run(20000);  // both heard on the LAN
  uint64_t before = m_rel.sent_frames + a_rel.sent_frames;
  m.eng->send_text(Dest::to_node(a.nid()), "over the lan");
  sim.run(5 * 60000);
  CHECK(a.rec.got_text("over the lan"));
  CHECK(m_rel.sent_frames + a_rel.sent_frames - before <= 2);  // at most frames queued before the LAN was heard
  // The LAN goes away: after the 3-minute rule the session takes over and delivers.
  m_lan.set_up(false);
  a_lan.set_up(false);
  sim.run(4 * 60000);
  m.eng->send_text(Dest::to_node(a.nid()), "over the relay");
  CHECK(sim.run_until([&] { return a.rec.got_text("over the relay"); }, 10 * 60000) != UINT64_MAX);
  CHECK(m_rel.sent_frames > 0);
}

TEST_CASE("relay M12: nothing received over a relay is put on an amateur-radio link") {
  Sim sim(33);
  MeshConfig mc = Sim::default_config();
  Node& m = sim.add_node("m", mc);
  Node& a = sim.add_node("a", mc);
  Node& r = sim.add_node("r", mc);
  sim.introduce_all();
  m.eng->set_profile("m", "N0CALL-1", "");
  r.eng->set_profile("r", "N0CALL-2", "");
  LinkCaps ham;
  ham.mtu = 200;
  ham.bitrate_bps = 1200;
  ham.half_duplex = true;
  ham.encryption_allowed = false;
  ham.requires_station_id = true;
  Medium& radio = sim.add_medium(ham);
  Medium& rel = sim.add_medium(session_caps(), 40);
  sim.join(m, radio);
  sim.join(r, radio);
  SimLink& m_rel = sim.join(m, rel);
  SimLink& a_rel = sim.join(a, rel);
  scope_peer(m, m_rel, a);
  scope_peer(a, a_rel, m);
  sim.run(10000);
  a.eng->sos_start("help from far away", std::nullopt, 60);  // signed broadcast, in scope on the session
  sim.run(10 * 60000);
  CHECK_FALSE(m.rec.sos.empty());   // m got it over the relay
  CHECK(r.rec.sos.empty());         // and never put it on the radio
  m.eng->sos_start("help from m", std::nullopt, 60);  // m's own SOS still goes on the radio
  CHECK(sim.run_until([&] { return !r.rec.sos.empty(); }, 10 * 60000) != UINT64_MAX);
}

TEST_CASE("relay M4: a session beacons about every 30 minutes, not like a LAN") {
  Sim sim(34);
  MeshConfig mc = Sim::default_config();
  mc.announce_interval_s = 1800;  // the node's default (beacon_interval_s 1800)
  Node& m = sim.add_node("m", mc);
  Node& a = sim.add_node("a", mc);
  sim.introduce(m, a);
  Medium& rel = sim.add_medium(session_caps(), 40);
  SimLink& m_rel = sim.join(m, rel);
  SimLink& a_rel = sim.join(a, rel);
  scope_peer(m, m_rel, a);
  scope_peer(a, a_rel, m);
  sim.run(10 * 60000);
  uint64_t t0 = m_rel.sent_frames + a_rel.sent_frames;
  sim.run(60 * 60000);  // one idle hour
  uint64_t idle = m_rel.sent_frames + a_rel.sent_frames - t0;
  MESSAGE("frames in one idle hour on one session (both ends): " << idle);
  // Per end and hour: 2 announcements (node default), each followed by one beacon, plus the 30-minute
  // beacons — about 6; both ends ≈ 12. A LAN-speed link would send ~240.
  CHECK(idle <= 14);
}

}  // TEST_SUITE
