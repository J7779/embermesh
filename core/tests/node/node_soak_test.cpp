// Node: long soak — skipped by default; run with:  ember_tests -ts=node -tc="soak*" --no-skip
// 8 nodes on two lossy media joined by gateways, random partitions, 20 virtual minutes of traffic,
// one node restarted from disk halfway. Every DM must arrive exactly once; memory stays bounded.
#include <map>

#include "node/node_harness.h"

using namespace nt;

TEST_SUITE("node") {

TEST_CASE("soak: lossy partitioned mesh, restarts, exactly-once delivery" * doctest::skip()) {
  Net net(500);
  int m1 = net.add_medium(0.2, 20), m2 = net.add_medium(0.2, 40);
  std::vector<std::unique_ptr<TNode>> nodes;
  std::string dir = temp_dir("soak");
  for (int i = 0; i < 8; ++i) nodes.push_back(std::make_unique<TNode>(i == 0 ? dir : "", 500 + static_cast<uint64_t>(i)));
  std::map<TNode*, std::vector<uint32_t>> links;
  for (int i = 0; i < 8; ++i) {
    if (i < 5) links[nodes[i].get()].push_back(net.join(*nodes[i], m1));
    if (i >= 3) links[nodes[i].get()].push_back(net.join(*nodes[i], m2));
  }
  for (int i = 0; i < 8; ++i)
    for (int j = i + 1; j < 8; ++j) introduce(*nodes[i], *nodes[j]);
  Rng r(500);
  struct Sent {
    std::string from, to, text;  // node ids (node 0 is replaced by a restarted instance)
  };
  std::vector<Sent> sent;
  for (int minute = 0; minute < 20; ++minute) {
    for (int k = 0; k < 6; ++k) {
      TNode* f = nodes[r.below(8)].get();
      TNode* t = nodes[r.below(8)].get();
      if (f == t) continue;
      std::string text = "soak " + std::to_string(minute) + "/" + std::to_string(k);
      f->ok("messages.send", Json::object({{"conv", "dm:" + t->id}, {"text", text}}));
      sent.push_back({f->id, t->id, text});
    }
    TNode* iso = nodes[r.below(8)].get();
    net.isolate(*iso);
    net.run(30000, 50);
    net.isolate(*iso, false);
    if (minute == 10) {  // restart node 0 from disk
      std::vector<uint32_t> ids = links[nodes[0].get()];
      std::string id0 = nodes[0]->id;
      net.forget(*nodes[0]);
      nodes[0].reset();
      nodes[0] = std::make_unique<TNode>(dir, 999);
      for (uint32_t id : ids) {
        net.attach(*nodes[0], id, m1);
        nodes[0]->ok("links.endpoint_state", Json::object({{"id", id}, {"connected", true}}));
      }
      REQUIRE(nodes[0]->id == id0);  // same identity from disk
    }
    net.run(30000, 50);
  }
  net.run(30 * 60000, 100);
  size_t ok = 0;
  for (auto& s : sent) {
    TNode* to = nullptr;
    for (auto& n : nodes)
      if (n->id == s.to) to = n.get();
    REQUIRE(to);
    int copies = 0;
    for (auto& m : to->messages("dm:" + s.from, 1000)) copies += m.get_string("text") == s.text;
    CHECK(copies <= 1);
    ok += copies == 1;
  }
  MESSAGE("soak: " << ok << " / " << sent.size() << " DMs delivered exactly once");
  CHECK(ok >= sent.size() * 9 / 10);
}

}  // TEST_SUITE
