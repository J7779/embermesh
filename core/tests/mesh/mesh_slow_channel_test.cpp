// Broadcast convergence on one slow, shared, half-duplex channel with collisions (1200 bit/s
// sound-card modem style): a single Public message must reach everyone without user re-sends.
#include "doctest.h"
#include "mesh/sim.h"
#include "proto/packet.h"

using namespace sim;
using namespace ember::proto;

TEST_SUITE("mesh") {

TEST_CASE("slow shared channel: one broadcast reaches everyone by itself (2..5 nodes, collisions)") {
  for (int count : {2, 3, 5}) {
    for (uint64_t seed : {71u, 72u, 73u}) {
     for (bool lose_first : {false, true}) {
      CAPTURE(lose_first);
      CAPTURE(count);
      CAPTURE(seed);
      Sim s(seed);
      Medium& m = s.add_medium(afsk_caps());
      m.set_collisions(true);
      std::vector<Node*> n;
      for (int i = 0; i < count; ++i) {
        n.push_back(&s.add_node("a" + std::to_string(i)));
        s.join(*n.back(), m);
      }
      s.introduce_all();
      s.run(1000);  // send right away: start-up beacons / announcements are still in the air
      SendResult sent = n[0]->eng->send_text(Dest::broadcast(), "anyone there?");
      // A collision the modem's carrier sense could not prevent: the first on-air copy is lost
      // at every receiver (DCD latency / hidden terminal on a real audio channel).
      bool dropped = false;
      uint64_t first_tx = 0;
      s.rx_hook = [&](SimLink&, Bytes& f) {
        FrameHeader fh;
        ByteView body;
        PacketView pv;
        if (lose_first && parse_frame(f, fh, body) && fh.type == FrameType::Packet && parse_packet(body, pv) &&
            pv.h.origin == sent.id.origin && pv.h.pkt_id == sent.id.pkt_id) {
          if (!first_tx) first_tx = s.now();
          if (s.now() - first_tx < 500) { dropped = true; return false; }
        }
        return true;
      };
      uint64_t t = s.run_until([&] {
        for (int i = 1; i < count; ++i)
          if (!n[static_cast<size_t>(i)]->rec.got_text("anyone there?")) return false;
        return true;
      }, 10 * 60000);
      s.rx_hook = nullptr;
      if (lose_first) CHECK(dropped);
      MESSAGE(count << " nodes, seed " << seed << std::string(lose_first ? " (first copy lost)" : "") << ": reached all in " << (t == UINT64_MAX ? -1 : static_cast<long long>(t))
                    << " ms; collisions " << m.frames_collided);
      CHECK(t != UINT64_MAX);
      CHECK(t < 90000);  // without user action: rebroadcast / early beacon + reconciliation
      for (int i = 1; i < count; ++i) CHECK(n[static_cast<size_t>(i)]->rec.count_text("anyone there?") == 1);
     }
    }
  }
}

}  // TEST_SUITE
