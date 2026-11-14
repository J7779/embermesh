// KvStore: ordered scans with binary keys and a seeded randomized model test against std::map.
#include "store_test_util.h"

using namespace store_test;

namespace {

std::vector<std::string> ref_scan(const std::map<std::string, std::string>& m, const ScanSpec& s) {
  std::vector<std::string> out;
  std::string lo(s.lower.begin(), s.lower.end()), hi(s.upper.begin(), s.upper.end());
  std::string pre(s.prefix.begin(), s.prefix.end());
  for (const auto& kv : m) {
    const std::string& k = kv.first;
    if (k < lo) continue;
    if (s.has_upper && !(k < hi)) continue;
    if (k.compare(0, pre.size(), pre) != 0) continue;
    out.push_back(k);
  }
  if (s.reverse) std::reverse(out.begin(), out.end());
  if (s.limit && out.size() > s.limit) out.resize(s.limit);
  return out;
}

std::vector<std::string> db_scan(KvStore& db, const std::string& ns, const ScanSpec& s,
                                 const std::map<std::string, std::string>* check_values = nullptr) {
  std::vector<std::string> out;
  Status st = db.scan(ns, s, [&](ByteView k, ByteView v, uint64_t) {
    out.push_back(k.to_string());
    if (check_values && !s.keys_only) {
      auto it = check_values->find(k.to_string());
      CHECK(it != check_values->end());
      if (it != check_values->end()) CHECK(it->second == v.to_string());
    }
    if (s.keys_only) CHECK(v.empty());
    return true;
  });
  CHECK(st == Status::Ok);
  return out;
}

std::string rand_key(Rng& r) {
  // Small alphabet incl. 0x00 and 0xFF so prefixes/bounds collide often.
  static const char alpha[] = {'\x00', '\x01', 'a', 'b', '\x7f', '\x80', '\xfe', '\xff'};
  std::string k;
  size_t n = r.below(5);
  for (size_t i = 0; i < n; ++i) k.push_back(alpha[r.below(sizeof alpha)]);
  return k;
}

ScanSpec rand_spec(Rng& r) {
  ScanSpec s;
  if (r.below(2)) s.set_lower(B(rand_key(r)));
  if (r.below(2)) s.set_upper(B(rand_key(r)));
  if (r.below(3) == 0) s.set_prefix(B(rand_key(r)));
  s.reverse = r.below(2) == 1;
  if (r.below(2)) s.limit = 1 + r.below(6);
  s.keys_only = r.below(4) == 0;
  return s;
}

}  // namespace

TEST_SUITE("store") {

TEST_CASE("scan: ordering is unsigned bytewise with binary keys") {
  MemVfs vfs;
  auto db = open_ok(&vfs, "db", plain_opts());
  std::vector<std::string> keys = {std::string(), std::string(1, '\0'), std::string("\0\0", 2),
                                   std::string(1, '\x01'), "a", "ab", "b", std::string(1, '\x7f'),
                                   std::string(1, '\x80'), std::string("\xff", 1),
                                   std::string("\xff\xff", 2), std::string("\xff\x00", 2)};
  for (const auto& k : keys) CHECK(db->put("n", B(k), B("v" + k)) == Status::Ok);
  std::vector<std::string> sorted = keys;
  std::sort(sorted.begin(), sorted.end());
  CHECK(db_scan(*db, "n", ScanSpec()) == sorted);
  ScanSpec rev;
  rev.reverse = true;
  std::vector<std::string> rs(sorted.rbegin(), sorted.rend());
  CHECK(db_scan(*db, "n", rev) == rs);
  // 0x80 sorts after 0x7f (unsigned), and "\xff" prefix covers "\xff", "\xff\x00", "\xff\xff".
  CHECK(db_scan(*db, "n", ScanSpec::with_prefix(B("\xff"))) ==
        std::vector<std::string>{"\xff", std::string("\xff\x00", 2), "\xff\xff"});
  ScanSpec pf = ScanSpec::with_prefix(B("\xff\xff"));
  CHECK(db_scan(*db, "n", pf) == std::vector<std::string>{"\xff\xff"});
  ScanSpec z = ScanSpec::with_prefix(ByteView(reinterpret_cast<const uint8_t*>("\0"), 1));
  CHECK(db_scan(*db, "n", z) == std::vector<std::string>{std::string(1, '\0'), std::string("\0\0", 2)});
}

TEST_CASE("scan: latest N before timestamp, limits, bounds, early stop") {
  MemVfs vfs;
  auto db = open_ok(&vfs, "db", plain_opts());
  WriteBatch b;
  for (uint64_t t = 1000; t < 1200; ++t) b.put("msg", B(be64(t) + "id"), B("body" + std::to_string(t)));
  b.put("msgx", B(be64(1100)), B("other ns"));
  CHECK(db->write(b) == Status::Ok);
  ScanSpec s;
  s.set_upper(B(be64(1150)));
  s.reverse = true;
  s.limit = 50;
  std::vector<KvEntry> out;
  CHECK(db->scan("msg", s, &out) == Status::Ok);
  REQUIRE(out.size() == 50);
  CHECK(S(out.front().key) == be64(1149) + "id");
  CHECK(S(out.back().key) == be64(1100) + "id");
  CHECK(S(out.front().value) == "body1149");
  // Forward with lower bound + limit (oldest-first eviction).
  ScanSpec f;
  f.set_lower(B(be64(1195)));
  CHECK(db->scan("msg", f, &out) == Status::Ok);
  CHECK(out.size() == 5);
  // Empty / inverted range.
  ScanSpec inv;
  inv.set_lower(B("z"));
  inv.set_upper(B("a"));
  CHECK(db->scan("msg", inv, &out) == Status::Ok);
  CHECK(out.empty());
  CHECK(db->count("msg", inv) == 0);
  // Early stop from visitor.
  int seen = 0;
  CHECK(db->scan("msg", ScanSpec(), [&](ByteView, ByteView, uint64_t) { return ++seen < 3; }) == Status::Ok);
  CHECK(seen == 3);
  // Unknown namespace.
  CHECK(db->scan("nope", ScanSpec(), &out) == Status::Ok);
  CHECK(out.empty());
  CHECK(db->count("msg") == 200);
  CHECK(db->count("msg", ScanSpec::with_prefix(B(be64(1100).substr(0, 7)))) == 176);  // 0x400..0x4FF ∩ [1000,1200)
}

TEST_CASE("model: seeded randomized ops vs std::map with reopen/compact/crash") {
  for (uint64_t seed = 1; seed <= 6; ++seed) {
    CAPTURE(seed);
    Rng r(seed * 7777);
    MemVfs vfs;
    bool enc = seed % 2 == 0;
    auto opts = [&]() { return enc ? enc_opts("model-key", seed) : plain_opts(); };
    std::map<std::string, std::map<std::string, std::string>> m;
    auto db = open_ok(&vfs, "db", opts());
    const char* nss[] = {"a", "b", ""};
    for (int step = 0; step < 1500; ++step) {
      std::string ns = nss[r.below(3)];
      uint64_t op = r.below(100);
      if (op < 45) {
        std::string k = rand_key(r), v = r.below(10) == 0 ? std::string(r.below(3000), 'q') : std::to_string(r.next());
        REQUIRE(db->put(ns, B(k), B(v)) == Status::Ok);
        m[ns][k] = v;
      } else if (op < 60) {
        std::string k = rand_key(r);
        REQUIRE(db->del(ns, B(k)) == Status::Ok);
        m[ns].erase(k);
      } else if (op < 68) {
        WriteBatch b;
        auto mm = m;
        size_t n = 1 + r.below(8);
        for (size_t i = 0; i < n; ++i) {
          std::string bns = nss[r.below(3)], k = rand_key(r);
          uint64_t kind = r.below(10);
          if (kind < 6) {
            std::string v = std::to_string(r.next());
            b.put(bns, B(k), B(v));
            mm[bns][k] = v;
          } else if (kind < 8) {
            b.del(bns, B(k));
            mm[bns].erase(k);
          } else if (kind < 9) {
            std::string hi = rand_key(r);
            b.delete_range(bns, B(k), B(hi));
            auto& t = mm[bns];
            for (auto it = t.lower_bound(k); it != t.end() && it->first < hi;) it = t.erase(it);
          } else {
            b.delete_from(bns, B(k));
            auto& t = mm[bns];
            t.erase(t.lower_bound(k), t.end());
          }
        }
        REQUIRE(db->write(b, WriteOptions{r.below(2) == 0}) == Status::Ok);
        m = mm;
      } else if (op < 88) {
        ScanSpec s = rand_spec(r);
        CAPTURE(step);
        REQUIRE(db_scan(*db, ns, s, &m[ns]) == ref_scan(m[ns], s));
        ScanSpec c = s;
        c.limit = 0;
        c.reverse = false;
        REQUIRE(db->count(ns, s) == ref_scan(m[ns], c).size());
      } else if (op < 93) {
        std::string k = rand_key(r);
        Bytes v;
        Status st = db->get(ns, B(k), &v);
        auto it = m[ns].find(k);
        if (it == m[ns].end()) {
          REQUIRE(st == Status::NotFound);
        } else {
          REQUIRE(st == Status::Ok);
          REQUIRE(S(v) == it->second);
        }
      } else if (op < 96) {
        db.reset();
        db = open_ok(&vfs, "db", opts());
        REQUIRE(db->recovery_report().clean());
      } else if (op < 98) {
        REQUIRE(db->compact() == Status::Ok);
      } else {
        // Clean crash: everything was synced or not; sync first, then crash dropping nothing durable.
        REQUIRE(db->sync() == Status::Ok);
        vfs.crash();
        db.reset();
        vfs.reboot(CrashMode::DropUnsynced);
        db = open_ok(&vfs, "db", opts());
      }
      for (auto it = m.begin(); it != m.end();) it = it->second.empty() ? m.erase(it) : std::next(it);
    }
    REQUIRE(dump(*db) == m);
    uint64_t total = 0;
    for (const auto& ns : m) {
      NamespaceStats st = db->namespace_stats(ns.first);
      uint64_t kb = 0, vb = 0;
      for (const auto& kv : ns.second) {
        kb += kv.first.size();
        vb += kv.second.size();
      }
      CHECK(st.records == ns.second.size());
      CHECK(st.key_bytes == kb);
      CHECK(st.value_bytes == vb);
      total += ns.second.size();
    }
    CHECK(db->stats().records == total);
  }
}

}  // TEST_SUITE
