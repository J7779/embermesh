// KvStore: corruption injection (bit rot, garbage, truncation, header loss) and disk-full handling.
#include "store/kv_format.h"
#include "store_test_util.h"

using namespace store_test;

namespace {

std::string data_file(MemVfs& vfs) {
  for (const auto& f : vfs.all_files())
    if (f.compare(0, 8, "db/data-") == 0) return f;
  return std::string();
}

// Builds a store with unique keys (no overwrites/deletes): snapshot chunks from a compaction,
// then a log of batches. Returns key -> value.
std::map<std::string, std::string> build_unique(MemVfs& vfs, bool enc, int n_before = 30, int n_after = 30) {
  std::map<std::string, std::string> m;
  Options o = enc ? enc_opts("k", 5) : plain_opts();
  o.snapshot_chunk_bytes = 256;
  o.entropy = test_entropy(77);
  auto db = open_ok(&vfs, "db", std::move(o));
  int id = 0;
  auto batch = [&](int n) {
    for (int i = 0; i < n; ++i) {
      WriteBatch b;
      for (int j = 0; j < 1 + i % 3; ++j) {
        std::string k = "key" + std::to_string(id), v = "value-" + std::to_string(id) + std::string(id % 50, '#');
        ++id;
        b.put(id % 2 ? "a" : "b", B(k), B(v));
        m[k] = v;
      }
      REQUIRE(db->write(b) == Status::Ok);
    }
  };
  batch(n_before);
  REQUIRE(db->compact() == Status::Ok);
  batch(n_after);
  return m;
}

// Checks that every visible record is genuine; returns the number of records found.
// (Aggregates into a few assertions: this runs thousands of times.)
size_t check_genuine(KvStore& db, const std::map<std::string, std::string>& m) {
  size_t found = 0;
  bool scans_ok = true, values_ok = true, gets_ok = true;
  for (const char* ns : {"a", "b"}) {
    std::vector<KvEntry> v;
    Status s = db.scan(ns, ScanSpec(), &v);
    scans_ok &= (s == Status::Ok || s == Status::Corruption);
    for (const auto& e : v) {
      auto it = m.find(S(e.key));
      values_ok &= it != m.end() && it->second == S(e.value);
      ++found;
    }
  }
  Bytes v;
  for (const auto& kv : m) {
    for (const char* ns : {"a", "b"}) {  // a key lives in one namespace; check both
      Status g = db.get(ns, B(kv.first), &v);
      gets_ok &= (g == Status::Ok || g == Status::NotFound || g == Status::Corruption);
      if (g == Status::Ok) values_ok &= S(v) == kv.second;
    }
  }
  CHECK(scans_ok);
  CHECK(gets_ok);
  REQUIRE(values_ok);
  return found;
}

void flip_campaign(bool soak) {
  for (int enc = 0; enc < 2; ++enc) {
    CAPTURE(enc);
    MemVfs base;
    auto m = build_unique(base, enc != 0, 20, 20);
    std::string path = data_file(base);
    Bytes orig = base.contents(path);
    size_t lost_runs = 0, runs = 0;
    for (size_t off = 0; off < orig.size(); off += soak ? 1 : (off < fmt::kDataStart ? 5 : (off < 1100 ? 1 : 5))) {
      CAPTURE(off);
      MemVfs vfs;
      vfs.mkdir("db");
      vfs.set_contents(path, ByteView(orig));
      vfs.corrupt(path, off, static_cast<uint8_t>(soak ? 0xFF : 1u << (off % 8)));
      RecoveryReport rep;
      std::unique_ptr<KvStore> db;
      Status s = KvStore::open(&vfs, "db", enc ? enc_opts("k", 9) : plain_opts(), &db, &rep);
      REQUIRE_MESSAGE(s == Status::Ok, status_name(s));
      size_t found = check_genuine(*db, m);
      bool lossless = rep.clean() && rep.torn_tail_bytes == 0;
      if (lossless) CHECK(found == m.size());
      if (found < m.size()) {
        ++lost_runs;
        CHECK_FALSE(lossless);  // every loss is reported
      }
      if (off < fmt::kDataStart) CHECK(found == m.size());  // one header copy always survives
      ++runs;
      // The store stays usable and the damage does not grow.
      REQUIRE(db->put("a", B("new"), B("x")) == Status::Ok);
      db.reset();
      db = open_ok(&vfs, "db", enc ? enc_opts("k", 10) : plain_opts());
      CHECK(db->contains("a", B("new")));
    }
    MESSAGE("enc=" << enc << " flips=" << runs << " runs with reported loss=" << lost_runs);
    CHECK(lost_runs > 0);
  }
}

}  // namespace

TEST_SUITE("store") {

TEST_CASE("corruption: single byte flip anywhere never yields wrong values") { flip_campaign(false); }

TEST_CASE("corruption soak: every byte inverted" * doctest::skip()) { flip_campaign(true); }

TEST_CASE("corruption: random garbage, zeroed ranges, truncation and junk tails") {
  for (int enc = 0; enc < 2; ++enc) {
    MemVfs base;
    auto m = build_unique(base, enc != 0);
    std::string path = data_file(base);
    Bytes orig = base.contents(path);
    Rng r(1234 + enc);
    for (int iter = 0; iter < 300; ++iter) {
      CAPTURE(iter);
      Bytes d = orig;
      int kind = static_cast<int>(r.below(5));
      size_t a = r.below(d.size()), len = 1 + r.below(200);
      switch (kind) {
        case 0:  // random overwrite
          for (size_t i = a; i < d.size() && i < a + len; ++i) d[i] = static_cast<uint8_t>(r.next());
          break;
        case 1:  // zero-fill
          for (size_t i = a; i < d.size() && i < a + len; ++i) d[i] = 0;
          break;
        case 2:  // truncate
          d.resize(a);
          break;
        case 3: {  // junk appended
          Bytes j = r.bytes(len);
          d.insert(d.end(), j.begin(), j.end());
          break;
        }
        case 4:  // several scattered flips
          for (int i = 0; i < 20; ++i) d[r.below(d.size())] ^= static_cast<uint8_t>(1 + r.below(255));
          break;
      }
      MemVfs vfs;
      vfs.mkdir("db");
      vfs.set_contents(path, ByteView(d));
      std::unique_ptr<KvStore> db;
      Status s = KvStore::open(&vfs, "db", enc ? enc_opts("k", 9) : plain_opts(), &db);
      if (s == Status::Corruption) continue;  // both header copies gone: refused, never crashes
      REQUIRE_MESSAGE(s == Status::Ok, status_name(s));
      check_genuine(*db, m);
      CHECK(db->put("a", B("new"), B("x")) == Status::Ok);
    }
  }
}

TEST_CASE("corruption: history-membership invariant with overwrites and deletes") {
  MemVfs base;
  std::map<std::string, std::set<std::string>> hist;
  {
    auto db = open_ok(&base, "db", plain_opts());
    Rng r(5);
    for (int i = 0; i < 200; ++i) {
      std::string k = "k" + std::to_string(r.below(20));
      if (r.below(4) == 0) {
        REQUIRE(db->del("n", B(k)) == Status::Ok);
      } else {
        std::string v = "v" + std::to_string(i);
        REQUIRE(db->put("n", B(k), B(v)) == Status::Ok);
        hist[k].insert(v);
      }
      if (i == 120) REQUIRE(db->compact() == Status::Ok);
    }
  }
  std::string path = data_file(base);
  Bytes orig = base.contents(path);
  Rng r(6);
  for (int iter = 0; iter < 400; ++iter) {
    MemVfs vfs;
    vfs.mkdir("db");
    vfs.set_contents(path, ByteView(orig));
    for (int f = 0; f < 3; ++f) vfs.corrupt(path, fmt::kDataStart + r.below(orig.size() - fmt::kDataStart), static_cast<uint8_t>(1 + r.below(255)));
    std::unique_ptr<KvStore> db;
    REQUIRE(KvStore::open(&vfs, "db", plain_opts(), &db) == Status::Ok);
    std::vector<KvEntry> v;
    db->scan("n", ScanSpec(), &v);
    for (const auto& e : v) {
      REQUIRE(hist.count(S(e.key)));
      REQUIRE(hist[S(e.key)].count(S(e.value)));
    }
  }
}

TEST_CASE("corruption: bit rot after open is caught on read") {
  for (int enc = 0; enc < 2; ++enc) {
    MemVfs vfs;
    auto db = open_ok(&vfs, "db", enc ? enc_opts("k") : plain_opts());
    REQUIRE(db->put("n", B("a"), B("hello world value")) == Status::Ok);
    REQUIRE(db->put("n", B("b"), B("second")) == Status::Ok);
    std::string path = data_file(vfs);
    Bytes d = vfs.contents(path);
    // Flip the last payload byte of the first batch frame (inside the value or its tag).
    uint64_t first = fmt::kDataStart;
    while (true) {  // skip snapshot frames
      uint32_t len = static_cast<uint32_t>(d[first + 4] | d[first + 5] << 8 | d[first + 6] << 16 | uint32_t(d[first + 7]) << 24);
      if (d[first + 2] == fmt::kFrameBatch) {
        vfs.corrupt(path, first + fmt::kFrameHeader + len - 2, 0x40);
        break;
      }
      first += fmt::kFrameOverhead + len;
    }
    Bytes v;
    CHECK(db->get("n", B("a"), &v) == Status::Corruption);
    CHECK(v.empty());
    CHECK(db->get("n", B("b"), &v) == Status::Ok);
    std::vector<KvEntry> out;
    CHECK(db->scan("n", ScanSpec(), &out) == Status::Corruption);
    CHECK(out.size() == 1);
    CHECK(db->stats().corrupt_reads == 2);
    // Compaction drops the unreadable record rather than copying garbage.
    CHECK(db->compact() == Status::Ok);
    CHECK(db->stats().dropped_by_compaction == 1);
    CHECK_FALSE(db->contains("n", B("a")));
    CHECK(db->get("n", B("b"), &v) == Status::Ok);
  }
}

TEST_CASE("corruption: both headers destroyed; older generation fallback; OnCorruption::Fail") {
  MemVfs vfs;
  {
    auto db = open_ok(&vfs, "db", plain_opts());
    REQUIRE(db->put("n", B("a"), B("1")) == Status::Ok);
  }
  Bytes gen1 = vfs.contents("db/data-1.ekv");
  {
    auto db = open_ok(&vfs, "db", plain_opts());
    REQUIRE(db->put("n", B("b"), B("2")) == Status::Ok);
    REQUIRE(db->compact() == Status::Ok);
  }
  REQUIRE(vfs.exists("db/data-2.ekv"));
  vfs.corrupt("db/data-2.ekv", 100, 1);
  vfs.corrupt("db/data-2.ekv", 612, 1);
  std::unique_ptr<KvStore> db;
  ProbeInfo pi;
  CHECK(KvStore::probe(vfs, "db", &pi) == Status::Corruption);
  CHECK(KvStore::open(&vfs, "db", plain_opts(), &db) == Status::Corruption);
  // An older generation is left over (e.g. its removal never became durable): use it.
  vfs.set_contents("db/data-1.ekv", ByteView(gen1));
  RecoveryReport rep;
  db = open_ok(&vfs, "db", plain_opts(), &rep);
  CHECK_FALSE(rep.clean());
  CHECK(rep.generation == 1);
  CHECK(dump(*db) == Model{{"n", {{"a", "1"}}}});
  CHECK(db->compact() == Status::Ok);
  CHECK(db->stats().generation == 3);  // never reuses the damaged generation's number
  CHECK(vfs.exists("db/data-2.ekv"));  // damaged newer file is not overwritten
  db.reset();

  // OnCorruption::Fail: torn tail is fine, mid-file damage is refused.
  MemVfs v2;
  {
    auto d2 = open_ok(&v2, "db", plain_opts());
    for (int i = 0; i < 5; ++i) REQUIRE(d2->put("n", B("k" + std::to_string(i)), B("v")) == Status::Ok);
  }
  Bytes d = v2.contents("db/data-1.ekv");
  v2.set_contents("db/data-1.ekv", ByteView(d.data(), d.size() - 3));
  Options f = plain_opts();
  f.on_corruption = OnCorruption::Fail;
  db = open_ok(&v2, "db", std::move(f), &rep);
  CHECK(rep.torn_tail_bytes > 0);
  CHECK(rep.clean());
  CHECK(db->count("n") == 4);
  db.reset();
  v2.corrupt("db/data-1.ekv", fmt::kDataStart + 40, 0xFF);  // early frame
  Options f2 = plain_opts();
  f2.on_corruption = OnCorruption::Fail;
  CHECK(KvStore::open(&v2, "db", std::move(f2), &db) == Status::Corruption);
  db = open_ok(&v2, "db", plain_opts(), &rep);
  CHECK(rep.corrupt_regions == 1);
  CHECK(rep.lost_ops >= 1);
}

TEST_CASE("corruption: forged frame hidden inside a value is not resurrected by resync") {
  // Attacker-controlled value contains a complete valid frame taken from another store.
  MemVfs evil;
  Options eo = plain_opts();
  eo.entropy = test_entropy(1);
  {
    auto db = open_ok(&evil, "db", std::move(eo));
    REQUIRE(db->put("n", B("evil"), B("1")) == Status::Ok);
  }
  Bytes ed = evil.contents("db/data-1.ekv");
  Bytes forged(ed.end() - 50, ed.end());  // tail of file ends with the batch frame
  size_t flen = 0;
  for (size_t i = 0; i + 8 < ed.size(); ++i)
    if (ed[i] == 'E' && ed[i + 1] == 0x4B && ed[i + 2] == fmt::kFrameBatch) {
      flen = ed.size() - i;
      forged.assign(ed.begin() + static_cast<std::ptrdiff_t>(i), ed.end());
    }
  REQUIRE(flen > 0);
  MemVfs vfs;
  Options o = plain_opts();
  o.entropy = test_entropy(2);
  {
    auto db = open_ok(&vfs, "db", std::move(o));
    REQUIRE(db->put("n", B("carrier"), ByteView(forged)) == Status::Ok);
    REQUIRE(db->put("n", B("after"), B("x")) == Status::Ok);
  }
  // Damage the carrier frame's header so recovery must resync through the value bytes.
  Bytes d = vfs.contents("db/data-1.ekv");
  size_t carrier = 0;
  for (size_t i = fmt::kDataStart; i + 3 < d.size(); ++i)
    if (d[i] == 'E' && d[i + 1] == 0x4B && d[i + 2] == fmt::kFrameBatch) {
      carrier = i;
      break;
    }
  vfs.corrupt("db/data-1.ekv", carrier + 9, 0x01);
  RecoveryReport rep;
  auto db = open_ok(&vfs, "db", plain_opts(), &rep);
  CHECK_FALSE(db->contains("n", B("evil")));
  CHECK_FALSE(db->contains("n", B("carrier")));
  CHECK(db->contains("n", B("after")));
  CHECK(rep.corrupt_regions == 1);
}

TEST_CASE("disk full: put fails cleanly at every byte, store stays usable") {
  MemVfs probe;
  { open_ok(&probe, "db", plain_opts()); }
  uint64_t base = probe.used_bytes();
  int checked = 0;
  for (uint64_t cap = base; cap < base + 1500; cap += 7) {
    MemVfs vfs;
    vfs.set_capacity(cap);
    Model m;
    std::unique_ptr<KvStore> db;
    Status s = KvStore::open(&vfs, "db", plain_opts(), &db);
    REQUIRE(s == Status::Ok);
    int i = 0;
    for (;; ++i) {
      std::string k = "k" + std::to_string(i % 13), v(i % 40, 'v');
      WriteBatch b;
      b.put("n", B(k), B(v));
      b.put("o", B(k), B(v));
      s = db->write(b);
      if (s != Status::Ok) break;
      m["n"][k] = v;
      m["o"][k] = v;
    }
    REQUIRE(s == Status::NoSpace);
    CHECK_FALSE(db->broken());
    CHECK(dump(*db) == m);
    // Compaction cannot fit a second copy either: unchanged, tmp removed.
    Status c = db->compact();
    CHECK((c == Status::NoSpace || c == Status::Ok));
    CHECK(dump(*db) == m);
    // Space freed elsewhere -> writes work again.
    vfs.set_capacity(UINT64_MAX);
    REQUIRE(db->put("n", B("later"), B("ok")) == Status::Ok);
    m["n"]["later"] = "ok";
    db.reset();
    vfs.reboot(CrashMode::DropUnsynced);
    RecoveryReport rep;
    db = open_ok(&vfs, "db", plain_opts(), &rep);
    CHECK(rep.clean());
    CHECK(dump(*db) == m);
    for (const auto& f : vfs.all_files()) CHECK(f.find("tmp-") == std::string::npos);
    ++checked;
  }
  CHECK(checked > 200);
}

TEST_CASE("disk full: during compaction, auto compaction and creation") {
  MemVfs vfs;
  Options o = plain_opts();
  auto db = open_ok(&vfs, "db", std::move(o));
  for (int i = 0; i < 100; ++i) REQUIRE(db->put("n", B("k" + std::to_string(i)), B(std::string(100, 'x'))) == Status::Ok);
  Model m = dump(*db);
  uint64_t used = vfs.used_bytes();
  for (uint64_t extra = 0; extra < used; extra += 97) {
    vfs.set_capacity(used + extra);
    Status c = db->compact();
    REQUIRE((c == Status::NoSpace || c == Status::Ok));
    CHECK_FALSE(db->broken());
    CHECK(dump(*db) == m);
    used = vfs.used_bytes();
  }
  vfs.set_capacity(UINT64_MAX);
  db.reset();
  db = open_ok(&vfs, "db", plain_opts());
  CHECK(dump(*db) == m);
  db.reset();

  // Auto compaction failing for lack of space does not fail the write that triggered it.
  MemVfs v2;
  Options ao = plain_opts();
  ao.auto_compact_min_bytes = 8192;
  auto d2 = open_ok(&v2, "db", std::move(ao));
  std::string v(200, 'q');
  for (int i = 0; i < 30; ++i) REQUIRE(d2->put("n", B("same"), B(v)) == Status::Ok);
  v2.set_capacity(v2.used_bytes() + 600);
  int ok = 0;
  for (int i = 0; i < 40; ++i) {
    Status s = d2->put("n", B("same"), B(v + std::to_string(i)));
    if (s == Status::Ok) ++ok;
    else CHECK(s == Status::NoSpace);
  }
  CHECK(ok >= 2);
  CHECK_FALSE(d2->broken());
  d2.reset();

  // Creation with no space at all.
  MemVfs v3;
  v3.set_capacity(100);
  std::unique_ptr<KvStore> d3;
  CHECK(KvStore::open(&v3, "db", plain_opts(), &d3) == Status::NoSpace);
  v3.set_capacity(UINT64_MAX);
  d3 = open_ok(&v3, "db", plain_opts());
  CHECK(d3->put("n", B("k"), B("v")) == Status::Ok);
}

}  // TEST_SUITE
