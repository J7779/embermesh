// SPDX-License-Identifier: MIT
// mDNS / DNS-SD codec and agent: round trips, every malformed shape we can think of, random garbage
// and mutations (must never crash, hang or read out of bounds — run under ASan in CI), and two
// agents discovering each other on this machine when multicast is available.
#ifdef EMBER_HOST_HAVE_MDNS
#include <cstring>
#include <random>

#include "doctest.h"
#ifndef _WIN32
#include <unistd.h>
#endif
#include "host/mdns.h"
#include "host/util.h"
#include "test_util.h"

using namespace ember_host;

namespace {
std::string bytes(std::initializer_list<int> b) {
  std::string s;
  for (int x : b) s.push_back(char(x));
  return s;
}
bool parse_str(const std::string& s, mdns::Message* m, std::string* err = nullptr) {
  return mdns::parse(reinterpret_cast<const uint8_t*>(s.data()), s.size(), m, err);
}
std::string sample_response() {
  mdns::Builder b(0, 0x8400);
  b.ptr("_embermesh._tcp.local", "EmberMesh-0a1b2c3d._embermesh._tcp.local", 120);
  b.srv("EmberMesh-0a1b2c3d._embermesh._tcp.local", "EmberMesh-0a1b2c3d.local", 47475, 120);
  b.txt("EmberMesh-0a1b2c3d._embermesh._tcp.local", {"ember=1", "v=1", "link=stream", "t=0a1b2c3d"}, 120);
  b.begin_additional();
  uint8_t ip[4] = {192, 0, 2, 7};
  b.a("EmberMesh-0a1b2c3d.local", ip, 120);
  uint8_t ll[16] = {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7};
  b.aaaa("EmberMesh-0a1b2c3d.local", ll, 120);
  return b.finish();
}
}  // namespace

TEST_SUITE("mdns") {
  TEST_CASE("builder output parses back with every field") {
    std::string pkt = sample_response();
    mdns::Message m;
    REQUIRE(parse_str(pkt, &m));
    CHECK(m.response());
    REQUIRE(m.records.size() == 5);
    CHECK(m.records[0].type == mdns::kPTR);
    CHECK(m.records[0].target == "EmberMesh-0a1b2c3d._embermesh._tcp.local");
    CHECK(m.records[1].type == mdns::kSRV);
    CHECK(m.records[1].port == 47475);
    CHECK(m.records[1].target == "EmberMesh-0a1b2c3d.local");
    CHECK(m.records[1].cache_flush);
    CHECK(m.records[2].txt_valid);
    CHECK(m.records[2].txt == std::vector<std::string>({"ember=1", "v=1", "link=stream", "t=0a1b2c3d"}));
    CHECK(m.records[3].addr_len == 4);
    CHECK(m.records[4].addr_len == 16);
    mdns::Builder q;
    q.question("_embermesh._tcp.local", mdns::kPTR, true);
    REQUIRE(parse_str(q.finish(), &m));
    REQUIRE(m.questions.size() == 1);
    CHECK(m.questions[0].unicast_response);
    CHECK(m.questions[0].type == mdns::kPTR);
    CHECK(mdns::name_equal("_EmberMesh._TCP.Local", "_embermesh._tcp.local"));
    CHECK_FALSE(mdns::name_equal("a.local", "b.local"));
  }

  TEST_CASE("compressed names, and every malformed shape is refused") {
    // header: 1 question, 1 answer; question name "x.local" at offset 12; answer name = pointer.
    std::string hdr = bytes({0, 0, 0x84, 0, 0, 1, 0, 1, 0, 0, 0, 0});
    std::string qname = bytes({1, 'x', 5, 'l', 'o', 'c', 'a', 'l', 0});
    std::string q = qname + bytes({0, 12, 0, 1});
    std::string a = bytes({0xC0, 12, 0, 1, 0, 1, 0, 0, 0, 120, 0, 4, 192, 0, 2, 1});
    mdns::Message m;
    std::string err;
    REQUIRE(parse_str(hdr + q + a, &m, &err));
    CHECK(m.records[0].name == "x.local");

    // pointer to itself (loop) / forward pointer
    std::string self_ptr = bytes({0, 0, 0x84, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0xC0, 12, 0, 1, 0, 1});
    CHECK_FALSE(parse_str(self_ptr, &m, &err));
    CHECK(err.find("backwards") != std::string::npos);
    std::string fwd = bytes({0, 0, 0x84, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0xC0, 20, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0});
    CHECK_FALSE(parse_str(fwd, &m));
    // two pointers chasing each other (A -> B -> A)
    std::string chase = hdr.substr(0, 4) + bytes({0, 2, 0, 0, 0, 0, 0, 0}) + bytes({0xC0, 14, 0, 1, 0, 1, 0xC0, 12, 0, 1, 0, 1});
    CHECK_FALSE(parse_str(chase, &m));
    // deep chain of backward pointers: question i = label + pointer to question i-1's name, so
    // question 17 needs 17 jumps (> kMaxJumps) and the message is refused.
    {
      std::string p = bytes({0, 0, 0x84, 0, 0, 41, 0, 0, 0, 0, 0, 0});
      std::vector<size_t> starts;
      starts.push_back(p.size());
      p += bytes({1, 'a', 0, 0, 1, 0, 1});
      for (int i = 1; i < 41; ++i) {
        size_t prev = starts.back();
        starts.push_back(p.size());
        p += bytes({1, 'b', int(0xC0 | (prev >> 8)), int(prev & 0xff), 0, 1, 0, 1});
      }
      CHECK_FALSE(parse_str(p, &m, &err));
      CHECK(err.find("too many compression pointers") != std::string::npos);
      // 16 jumps are fine
      std::string ok = p.substr(0, starts[17]);
      ok[5] = 17;
      CHECK(parse_str(ok, &m, &err));
    }
    // label runs past the end, truncated pointer, reserved label type
    CHECK_FALSE(parse_str(hdr.substr(0, 4) + bytes({0, 1, 0, 0, 0, 0, 0, 0}) + bytes({10, 'a'}), &m));
    CHECK_FALSE(parse_str(hdr.substr(0, 4) + bytes({0, 1, 0, 0, 0, 0, 0, 0}) + bytes({0xC0}), &m));
    CHECK_FALSE(parse_str(hdr.substr(0, 4) + bytes({0, 1, 0, 0, 0, 0, 0, 0}) + bytes({0x40, 0, 0, 1, 0, 1}), &m));
    // name longer than 255 bytes
    {
      std::string n;
      for (int i = 0; i < 5; ++i) n += std::string(1, char(63)) + std::string(63, 'a');
      n.push_back('\0');
      CHECK_FALSE(parse_str(hdr.substr(0, 4) + bytes({0, 1, 0, 0, 0, 0, 0, 0}) + n + bytes({0, 1, 0, 1}), &m, &err));
      CHECK(err.find("too long") != std::string::npos);
    }
    // invalid UTF-8 in a name
    CHECK_FALSE(parse_str(hdr.substr(0, 4) + bytes({0, 1, 0, 0, 0, 0, 0, 0}) + bytes({2, 0xC3, 0x28, 0, 0, 1, 0, 1}), &m));
    // too many entries claimed / short header / truncated question
    CHECK_FALSE(parse_str(bytes({0, 0, 0x84, 0, 0xff, 0xff, 0, 0, 0, 0, 0, 0}), &m));
    CHECK_FALSE(parse_str(bytes({0, 0, 0x84}), &m));
    CHECK_FALSE(parse_str(hdr.substr(0, 4) + bytes({0, 1, 0, 0, 0, 0, 0, 0}) + qname + bytes({0, 12}), &m));
    // record data beyond the packet; A of the wrong size; SRV too short; trailing bytes in PTR
    std::string one_rr = bytes({0, 0, 0x84, 0, 0, 0, 0, 1, 0, 0, 0, 0}) + qname;
    CHECK_FALSE(parse_str(one_rr + bytes({0, 1, 0, 1, 0, 0, 0, 1, 0, 50, 1, 2}), &m));
    CHECK_FALSE(parse_str(one_rr + bytes({0, 1, 0, 1, 0, 0, 0, 1, 0, 3, 1, 2, 3}), &m));
    CHECK_FALSE(parse_str(one_rr + bytes({0, 33, 0, 1, 0, 0, 0, 1, 0, 3, 1, 2, 3}), &m));
    CHECK_FALSE(parse_str(one_rr + bytes({0, 12, 0, 1, 0, 0, 0, 1, 0, 3, 0, 9, 9}), &m));
    // TXT: string past the record, oversized record, invalid UTF-8 kept out (record parsed, invalid)
    CHECK_FALSE(parse_str(one_rr + bytes({0, 16, 0, 1, 0, 0, 0, 1, 0, 2, 5, 'a'}), &m));
    {
      std::string big = one_rr + bytes({0, 16, 0, 1, 0, 0, 0, 1, 0x05, 0x15});  // 1301 bytes
      for (int i = 0; i < 1301; ++i) big.push_back('\0');
      CHECK_FALSE(parse_str(big, &m));
    }
    REQUIRE(parse_str(one_rr + bytes({0, 16, 0, 1, 0, 0, 0, 1, 0, 3, 2, 0xC3, 0x28}), &m));
    CHECK_FALSE(m.records[0].txt_valid);
    CHECK(m.records[0].txt.empty());
    // oversized datagram
    std::string huge(mdns::kMaxPacket + 1, '\0');
    CHECK_FALSE(parse_str(huge, &m));
  }

  TEST_CASE("random garbage and mutations of a valid packet never crash") {
    std::mt19937 rng(12345);
    std::string base = sample_response();
    mdns::Message m;
    size_t ok = 0;
    for (int i = 0; i < 20000; ++i) {
      std::string p = base;
      int kind = int(rng() % 4);
      if (kind == 0) {  // flip bytes
        for (int k = 0; k < 1 + int(rng() % 8); ++k) p[rng() % p.size()] = char(rng());
      } else if (kind == 1) {  // truncate
        p.resize(rng() % p.size());
      } else if (kind == 2) {  // pure garbage
        p.resize(rng() % 600);
        for (auto& c : p) c = char(rng());
      } else {  // splice compression pointers at random places
        size_t at = 12 + rng() % (p.size() - 12);
        p[at] = char(0xC0 | (rng() % 4));
        if (at + 1 < p.size()) p[at + 1] = char(rng());
      }
      ok += parse_str(p, &m);
      CHECK(m.records.size() <= 3 * mdns::kMaxEntries);
    }
    MESSAGE("mutated packets that still parsed: " << ok << " of 20000");
  }

  TEST_CASE("two agents on this machine discover each other (needs multicast)") {
    Loop loop;
    loop.bind_thread();
    MdnsAgent a(loop), b(loop);
    std::string svc = "_embermesh-test" + random_hex(3) + "._tcp";
    std::vector<NearbyPeer> seen_by_a, seen_by_b;
    a.on_found = [&](const NearbyPeer& p) { seen_by_a.push_back(p); };
    b.on_found = [&](const NearbyPeer& p) { seen_by_b.push_back(p); };
    MdnsAgent::Options oa, ob;
    oa.service = ob.service = svc;
    oa.token = "00000001";
    ob.token = "ffff0002";
    oa.port = 40001;
    ob.port = 40002;
    for (const auto& n : local_networks())
      if (!n.v6) oa.ipv4.push_back(n.bind), ob.ipv4.push_back(n.bind);
    if (local_networks().empty()) {
      MESSAGE("no local network (private IPv4 / IPv6 link-local) here: skipping");
      return;
    }
    std::string err;
    if (!a.start(oa, &err) || !b.start(ob, &err)) {
      MESSAGE("mDNS sockets unavailable here (" << err << "): skipping");
      return;
    }
    bool found = test::run_until(loop, [&] { return !seen_by_a.empty() && !seen_by_b.empty(); }, 8000);
    if (!found && a.stats().rx_packets == 0) {
      MESSAGE("no multicast traffic received on 224.0.0.251/ff02::fb:5353 here: skipping");
      return;
    }
    REQUIRE(found);
    CHECK(seen_by_a.back().token == "ffff0002");
    CHECK(seen_by_a.back().port == 40002);
    CHECK(seen_by_b.back().token == "00000001");
    CHECK(!seen_by_a.back().addrs.empty());
    // Nothing identifying is advertised: the host name is EmberMesh-<token>.local.
    char hn[256] = {0};
#ifndef _WIN32
    gethostname(hn, sizeof hn - 1);
#endif
    std::string host = to_lower(hn);
    host = host.substr(0, host.find('.'));
    if (host.size() >= 3) CHECK(to_lower(seen_by_a.back().instance).find(host) == std::string::npos);
    // A hostile sender on the LAN: garbage datagrams, then a flood of fake instances. The agent
    // counts and drops the garbage, keeps at most 64 instances, and keeps working.
    {
      std::string e;
      sock_t h = udp_socket(4, &e);
      REQUIRE(h != kInvalidSock);
      REQUIRE(udp_bind(h, "0.0.0.0", 5353, true, &e));
      udp_set_multicast_ttl_loop(h, 1, true);
      SockAddr g;
      resolve_numeric("224.0.0.251", 5353, &g, 4);
      std::mt19937 rng(7);
      uint64_t bad0 = a.stats().rx_bad;
      for (int i = 0; i < 150; ++i) {
        std::string junk(12 + rng() % 400, '\0');
        for (auto& c : junk) c = char(rng());
        junk[4] = 0;  // keep the counts small enough that some reach the parser body
        junk[5] = char(1 + rng() % 4);
        udp_sendto(h, junk.data(), junk.size(), g);
        if (i % 30 == 0) test::run_for(loop, 5);
      }
      // The garbage exhausted this second's budget for our address; a real flood ends too.
      test::run_for(loop, 1100);
      for (int pkt = 0; pkt < 10; ++pkt) {
        mdns::Builder fb(0, 0x8400);
        for (int k = 0; k < 8; ++k) {
          std::string tok = format("%08x", 0x10000000u + unsigned(pkt * 8 + k));
          std::string inst = "EmberMesh-" + tok + "." + svc + ".local";
          fb.ptr(svc + ".local", inst, 120);
          fb.srv(inst, "EmberMesh-" + tok + ".local", 9, 120);
          fb.txt(inst, {"ember=1", "v=1", "link=stream", "t=" + tok}, 120);
          uint8_t ip[4] = {192, 0, 2, uint8_t(1 + k)};
          fb.a("EmberMesh-" + tok + ".local", ip, 120);
        }
        std::string fp = fb.finish();
        udp_sendto(h, fp.data(), fp.size(), g);
        test::run_for(loop, 5);
      }
      test::run_for(loop, 300);
      sock_close(h);
      CHECK(a.stats().rx_dropped_budget > 0);  // the burst hit the per-source budget
      CHECK(a.stats().rx_bad > bad0);
      CHECK(a.peers().size() <= 64);
      MESSAGE("rx_packets " << a.stats().rx_packets << " budget drops " << a.stats().rx_dropped_budget);
      MESSAGE("after the flood: " << a.peers().size() << " instances kept, " << a.stats().rx_bad - bad0 << " garbage datagrams refused");
    }
    // Goodbye: stopping b makes a forget it.
    std::vector<std::string> lost;
    a.on_lost = [&](const std::string& t) { lost.push_back(t); };
    test::run_for(loop, 1100);  // fresh budget window for the goodbye
    b.stop();
    CHECK(test::run_until(loop, [&] { return !lost.empty(); }, 5000));
    a.stop();
  }
}
#endif
