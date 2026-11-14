// Mesh engine: basic two/three node behaviour on clean links.
#include "doctest.h"
#include "mesh/sim.h"

using namespace sim;

TEST_SUITE("mesh") {

TEST_CASE("two nodes: DM is end-to-end encrypted, delivered and acknowledged") {
  Sim s(1);
  Node& a = s.add_node("alice");
  Node& b = s.add_node("bob");
  Medium& lan = s.add_medium(lan_caps());
  SimLink& la = s.join(a, lan);
  s.join(b, lan);
  la.log_frames = true;
  s.introduce(a, b);
  s.run(2000);
  SendResult r = a.eng->send_text(Dest::to_node(b.nid()), "hello bob, meet at the well");
  REQUIRE(r.ok);
  CHECK_FALSE(r.waiting_for_key);
  CHECK(s.run_until([&] { return b.rec.got_text("hello bob, meet at the well"); }, 5000) != UINT64_MAX);
  auto& t = b.rec.texts.back();
  CHECK(t.rx.from == a.nid());
  CHECK(t.rx.encrypted);
  CHECK(t.rx.authenticated);
  CHECK(t.rx.hops == 0);
  CHECK(t.rx.id == r.id);
  CHECK(s.run_until([&] { return a.rec.status_of(r.id) == MsgStatus::Delivered; }, 5000) != UINT64_MAX);
  CHECK(a.eng->status(r.id) == MsgStatus::Delivered);
  CHECK(a.eng->stats().acks_received == 1);
  // The plaintext never appears on the air.
  for (const Bytes& f : la.sent_log) {
    std::string sf(f.begin(), f.end());
    CHECK(sf.find("meet at the well") == std::string::npos);
  }
  // Exactly one delivery even after more time passes (retries / sync must not duplicate).
  s.run(60000);
  CHECK(b.rec.count_text("hello bob, meet at the well") == 1);
  // Delivered copies are purged from both stores (the ack replaces them).
  bool a_has = false;
  for (auto w : a.eng->stats().per_link) (void)w;
  CHECK(a.eng->stats().stored_messages <= 4);  // announces + ack
  (void)a_has;
}

TEST_CASE("two nodes: keys learnt from announcements, message waits for the key") {
  Sim s(2);
  Node& a = s.add_node("alice");
  Node& b = s.add_node("bob");
  Medium& lan = s.add_medium(lan_caps());
  s.join(a, lan);
  s.join(b, lan);
  CHECK(a.eng->send_text(Dest::to_node(b.nid()), "too early").error == "not_started");
  s.step();
  // Not introduced and no announcement yet: the DM is held and a WHOIS goes out.
  SendResult r = a.eng->send_text(Dest::to_node(b.nid()), "are you there?");
  REQUIRE(r.ok);
  CHECK(r.waiting_for_key);
  CHECK(a.eng->status(r.id) == MsgStatus::Queued);
  REQUIRE(s.run_until([&] { return b.rec.got_text("are you there?"); }, 20000) != UINT64_MAX);
  CHECK(b.rec.texts.back().rx.id == r.id);  // same id the app was given
  CHECK(s.run_until([&] { return a.rec.status_of(r.id) == MsgStatus::Delivered; }, 10000) != UINT64_MAX);
  auto pb = a.eng->peer(b.nid());
  REQUIRE(pb);
  CHECK(pb->has_keys);
  CHECK(pb->name == "bob");
  CHECK(pb->direct);
  CHECK(pb->hops == 0);
}

TEST_CASE("public channel is signed cleartext; open and private channels") {
  Sim s(3);
  Node& a = s.add_node("a");
  Node& b = s.add_node("b");
  Node& c = s.add_node("c");
  Medium& lan = s.add_medium(lan_caps());
  s.join(a, lan);
  s.join(b, lan);
  s.join(c, lan);
  s.run(3000);  // announcements
  a.eng->send_text(Dest::broadcast(), "public hello");
  CHECK(s.run_until([&] { return b.rec.got_text("public hello") && c.rec.got_text("public hello"); }, 5000) != UINT64_MAX);
  CHECK(b.rec.texts.back().rx.authenticated);
  CHECK_FALSE(b.rec.texts.back().rx.encrypted);

  proto::ChannelKey priv = proto::channel_from_key("camp", crypto::Key32{9, 9, 9});
  a.eng->join_channel(priv);
  b.eng->join_channel(priv);
  a.eng->send_text(Dest::to_channel(priv.id), "members only");
  CHECK(s.run_until([&] { return b.rec.got_text("members only"); }, 5000) != UINT64_MAX);
  CHECK(b.rec.texts.back().rx.encrypted);
  CHECK(b.rec.texts.back().rx.authenticated);
  CHECK(b.rec.texts.back().rx.channel_name == "camp");
  s.run(3000);
  CHECK_FALSE(c.rec.got_text("members only"));  // c relays but cannot read

  proto::ChannelKey open = proto::open_channel("Market");
  a.eng->join_channel(open);
  c.eng->join_channel(open);
  a.eng->send_text(Dest::to_channel(open.id), "selling eggs");
  CHECK(s.run_until([&] { return c.rec.got_text("selling eggs"); }, 5000) != UINT64_MAX);
  CHECK_FALSE(c.rec.texts.back().rx.encrypted);
  s.run(2000);
  CHECK_FALSE(b.rec.got_text("selling eggs"));  // not joined
  SendResult bad = a.eng->send_text(Dest::to_channel(12345), "x");
  CHECK_FALSE(bad.ok);
  CHECK(bad.error == "unknown_channel");
}

TEST_CASE("ping/pong, read receipts, sos, check-in, bulletin, netplan, position, file, app") {
  Sim s(4);
  Node& a = s.add_node("a");
  Node& b = s.add_node("b");
  Medium& lan = s.add_medium(lan_caps());
  s.join(a, lan);
  s.join(b, lan);
  s.introduce(a, b);
  s.run(2000);

  REQUIRE(a.eng->ping(b.nid()));
  CHECK(s.run_until([&] { return !a.rec.pongs.empty(); }, 5000) != UINT64_MAX);
  CHECK(a.rec.pongs[0].first == b.nid());

  SendResult m = a.eng->send_text(Dest::to_node(b.nid()), "read me");
  s.run_until([&] { return b.rec.got_text("read me"); }, 5000);
  b.eng->send_read_receipt(a.nid(), {m.id.pkt_id});
  CHECK(s.run_until([&] { return a.rec.status_of(m.id) == MsgStatus::Read; }, 5000) != UINT64_MAX);
  REQUIRE(a.rec.reads.size() == 1);

  a.eng->sos_start("trapped under rubble", Position::from_degrees(40.1, -70.2), 30);
  CHECK(a.eng->sos_active());
  CHECK(s.run_until([&] { return !b.rec.sos.empty(); }, 5000) != UINT64_MAX);
  CHECK(b.rec.sos[0].second.text == "trapped under rubble");
  CHECK(b.rec.sos[0].second.active);
  CHECK(b.rec.sos[0].first.prio == Prio::Emergency);
  s.run(31000);
  CHECK(b.rec.sos.size() >= 2);  // repeating beacon
  b.eng->sos_ack(a.nid(), "on our way");
  CHECK(s.run_until([&] { return !a.rec.sos_acks.empty(); }, 5000) != UINT64_MAX);
  CHECK(a.rec.sos_acks[0].second.text == "on our way");
  CHECK(a.rec.sos_acks[0].first.authenticated);
  a.eng->sos_stop();
  CHECK_FALSE(a.eng->sos_active());
  CHECK(s.run_until([&] { return !b.rec.sos.back().second.active; }, 5000) != UINT64_MAX);
  size_t sos_count = b.rec.sos.size();
  s.run(65000);
  CHECK(b.rec.sos.size() == sos_count);  // stopped

  proto::CheckinBody ci;
  ci.status = proto::CheckinStatus::NeedHelp;
  ci.note = "out of water";
  a.eng->send_checkin(ci);
  CHECK(s.run_until([&] { return !b.rec.checkins.empty(); }, 5000) != UINT64_MAX);
  CHECK(b.rec.checkins[0].second.status == proto::CheckinStatus::NeedHelp);

  proto::BulletinBody bl;
  bl.category = proto::BulletinCategory::Water;
  bl.title = "Clean well";
  bl.text = "Behind the school";
  SendResult br = a.eng->post_bulletin(bl, 3600);
  CHECK(s.run_until([&] { return !b.rec.bulletins.empty(); }, 5000) != UINT64_MAX);
  CHECK(b.rec.bulletins[0].second.title == "Clean well");
  CHECK(b.rec.bulletins[0].first.expires_ms - b.rec.bulletins[0].first.created_ms <= 3600 * 1000 + 60000);
  a.eng->delete_bulletin(br.id.pkt_id);
  CHECK(s.run_until([&] { return b.rec.bulletins.size() == 2; }, 5000) != UINT64_MAX);
  CHECK(b.rec.bulletins[1].second.is_delete);
  CHECK(b.rec.bulletins[1].second.delete_pkt_id == br.id.pkt_id);

  proto::NetPlanBody np;
  np.entries.push_back({"Camp net", 146520000, "voice", "", "hourly", ""});
  a.eng->set_netplan(np, 600);
  CHECK(s.run_until([&] { return !b.rec.netplans.empty(); }, 5000) != UINT64_MAX);
  CHECK(b.rec.netplans[0].second.entries[0].freq_hz == 146520000);

  a.eng->send_position(Dest::broadcast(), Position::from_degrees(10.5, 20.25));
  CHECK(s.run_until([&] { return !b.rec.positions.empty(); }, 5000) != UINT64_MAX);
  auto pa = b.eng->peer(a.nid());
  REQUIRE(pa);
  REQUIRE(pa->position);
  CHECK(pa->position->lat() == doctest::Approx(10.5));

  Bytes file(3000);
  for (size_t i = 0; i < file.size(); ++i) file[i] = static_cast<uint8_t>(i * 13);
  SendResult fr = a.eng->send_file(Dest::to_node(b.nid()), "map.bin", "application/octet-stream", file);
  REQUIRE(fr.ok);
  CHECK(s.run_until([&] { return !b.rec.files.empty(); }, 10000) != UINT64_MAX);
  CHECK(b.rec.files[0].data == file);
  CHECK(b.rec.files[0].name == "map.bin");
  CHECK(b.rec.files[0].rx.id == fr.id);
  CHECK(s.run_until([&] { return a.rec.status_of(fr.id) == MsgStatus::Delivered; }, 10000) != UINT64_MAX);
  SendResult big = a.eng->send_file(Dest::to_node(b.nid()), "big", "x", Bytes(a.eng->config().max_file_bytes + 1));
  CHECK_FALSE(big.ok);
  CHECK(big.error == "too_large");
}

TEST_CASE("relay through a middle node; hop counts; peers table") {
  Sim s(5);
  Node& a = s.add_node("a");
  Node& m = s.add_node("m");
  Node& b = s.add_node("b");
  Medium& r = s.add_medium(lora_caps());
  s.join(a, r);
  s.join(m, r);
  s.join(b, r);
  r.connect(a, b, false);  // a and b cannot hear each other
  s.run(15000);
  SendResult res = a.eng->send_text(Dest::to_node(b.nid()), "via m");
  REQUIRE(res.ok);
  CHECK(s.run_until([&] { return b.rec.got_text("via m"); }, 60000) != UINT64_MAX);
  CHECK(b.rec.texts.back().rx.hops == 1);
  CHECK(s.run_until([&] { return a.rec.status_of(res.id) == MsgStatus::Delivered; }, 60000) != UINT64_MAX);
  auto pb = a.eng->peer(b.nid());
  REQUIRE(pb);
  CHECK(pb->hops == 1);
  CHECK_FALSE(pb->direct);
  auto pm = a.eng->peer(m.nid());
  REQUIRE(pm);
  CHECK(pm->direct);
  REQUIRE_FALSE(pm->links.empty());
  CHECK(pm->links[0].has_snr);
  CHECK(m.eng->stats().relayed > 0);
}

}  // TEST_SUITE
