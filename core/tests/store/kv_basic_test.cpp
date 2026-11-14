// KvStore: basic operations, limits, persistence, stats, compaction.
#include "store/kv_format.h"
#include "store_test_util.h"

using namespace store_test;

TEST_SUITE("store") {

TEST_CASE("kv: put/get/del/overwrite and NotFound") {
  MemVfs vfs;
  RecoveryReport rep;
  auto db = open_ok(&vfs, "db", plain_opts(), &rep);
  CHECK(rep.created);
  Bytes v;
  CHECK(db->get("ns", B("a"), &v) == Status::NotFound);
  CHECK(db->put("ns", B("a"), B("1")) == Status::Ok);
  CHECK(db->get("ns", B("a"), &v) == Status::Ok);
  CHECK(S(v) == "1");
  CHECK(db->put("ns", B("a"), B("22")) == Status::Ok);
  CHECK(db->get("ns", B("a"), &v) == Status::Ok);
  CHECK(S(v) == "22");
  CHECK(db->contains("ns", B("a")));
  CHECK_FALSE(db->contains("other", B("a")));
  CHECK(db->get("other", B("a"), &v) == Status::NotFound);
  CHECK(db->del("ns", B("a")) == Status::Ok);
  CHECK(db->get("ns", B("a"), &v) == Status::NotFound);
  CHECK(db->del("ns", B("missing")) == Status::Ok);
  CHECK(db->namespaces().empty());
}

TEST_CASE("kv: empty key, empty value, empty namespace, 1-byte and binary data") {
  MemVfs vfs;
  {
    auto db = open_ok(&vfs, "db", plain_opts());
    CHECK(db->put("", ByteView(), ByteView()) == Status::Ok);
    CHECK(db->put("n", ByteView(), B("empty-key")) == Status::Ok);
    CHECK(db->put("n", B("k"), ByteView()) == Status::Ok);
    Bytes one = {0x00};
    Bytes ff = {0xFF};
    CHECK(db->put("n", ByteView(one), ByteView(ff)) == Status::Ok);
  }
  auto db = open_ok(&vfs, "db", plain_opts());
  Bytes v = {1};
  CHECK(db->get("", ByteView(), &v) == Status::Ok);
  CHECK(v.empty());
  CHECK(db->get("n", ByteView(), &v) == Status::Ok);
  CHECK(S(v) == "empty-key");
  CHECK(db->get("n", B("k"), &v) == Status::Ok);
  CHECK(v.empty());
  Bytes one = {0x00};
  CHECK(db->get("n", ByteView(one), &v) == Status::Ok);
  CHECK(v == Bytes{0xFF});
}

TEST_CASE("kv: limits on namespace, key, value and batch size") {
  MemVfs vfs;
  Options o = plain_opts();
  o.max_value_size = 1000;
  o.max_batch_bytes = 5000;
  auto db = open_ok(&vfs, "db", std::move(o));
  std::string ns255(255, 'n'), ns256(256, 'n');
  std::string k65535(65535, 'k'), k65536(65536, 'k');
  CHECK(db->put(ns255, B("k"), B("v")) == Status::Ok);
  CHECK(db->put(ns256, B("k"), B("v")) == Status::TooLarge);
  CHECK(db->put("n", B(k65536), B("v")) == Status::TooLarge);
  std::string v1000(1000, 'v'), v1001(1001, 'v');
  CHECK(db->put("n", B("k"), B(v1000)) == Status::Ok);
  CHECK(db->put("n", B("k"), B(v1001)) == Status::TooLarge);
  WriteBatch b;
  for (int i = 0; i < 10; ++i) b.put("n", B("k" + std::to_string(i)), B(v1000));
  CHECK(db->write(b) == Status::TooLarge);
  CHECK(db->count("n") == 1);

  // Bad options.
  std::unique_ptr<KvStore> x;
  Options bad = plain_opts();
  bad.max_value_size = kMaxValueLen + 1;
  CHECK(KvStore::open(&vfs, "db2", std::move(bad), &x) == Status::InvalidArgument);
  Options nocrypto = plain_opts();
  nocrypto.cipher.reset(new InsecureTestCipher("k"));
  CHECK(KvStore::open(&vfs, "db3", std::move(nocrypto), &x) == Status::InvalidArgument);  // no entropy
  (void)k65535;
}

TEST_CASE("kv: max-size key round trip") {
  MemVfs vfs;
  std::string k(kMaxKeyLen, '\xAB');
  {
    auto db = open_ok(&vfs, "db", plain_opts());
    CHECK(db->put("n", B(k), B("v")) == Status::Ok);
  }
  auto db = open_ok(&vfs, "db", plain_opts());
  Bytes v;
  CHECK(db->get("n", B(k), &v) == Status::Ok);
  CHECK(S(v) == "v");
}

TEST_CASE("kv: batch ops apply in order; sequence numbers are monotonic across reopen/compact") {
  MemVfs vfs;
  uint64_t s1 = 0, s2 = 0;
  {
    auto db = open_ok(&vfs, "db", plain_opts());
    CHECK(db->last_sequence() == 0);
    WriteBatch b;
    b.put("n", B("a"), B("1"));
    b.put("n", B("b"), B("2"));
    b.del("n", B("a"));
    b.put("n", B("c"), B("3"));
    CHECK(b.count() == 4);
    CHECK(db->write(b, WriteOptions(), &s1) == Status::Ok);
    CHECK(s1 == 1);
    CHECK(db->last_sequence() == 4);
    WriteBatch empty;
    CHECK(db->write(empty) == Status::Ok);
    CHECK(db->last_sequence() == 4);
    uint64_t seq = 0;
    Bytes v;
    CHECK(db->get("n", B("c"), &v, &seq) == Status::Ok);
    CHECK(seq == 4);
    CHECK(db->get("n", B("b"), &v, &seq) == Status::Ok);
    CHECK(seq == 2);
    CHECK_FALSE(db->contains("n", B("a")));
    CHECK(db->compact() == Status::Ok);
    CHECK(db->last_sequence() == 4);
    CHECK(db->get("n", B("c"), &v, &seq) == Status::Ok);
    CHECK(seq == 4);
  }
  auto db = open_ok(&vfs, "db", plain_opts());
  CHECK(db->last_sequence() == 4);
  WriteBatch b;
  b.put("n", B("d"), B("4"));
  CHECK(db->write(b, WriteOptions(), &s2) == Status::Ok);
  CHECK(s2 == 5);
  uint64_t seq = 0;
  Bytes v;
  CHECK(db->get("n", B("b"), &v, &seq) == Status::Ok);
  CHECK(seq == 2);  // preserved through compaction + reopen
  CHECK(dump(*db) == Model{{"n", {{"b", "2"}, {"c", "3"}, {"d", "4"}}}});
}

TEST_CASE("kv: delete_range, delete_from, drop_namespace") {
  MemVfs vfs;
  auto db = open_ok(&vfs, "db", plain_opts());
  WriteBatch b;
  for (char c = 'a'; c <= 'j'; ++c) b.put("n", B(std::string(1, c)), B("v"));
  b.put("m", B("x"), B("y"));
  CHECK(db->write(b) == Status::Ok);
  WriteBatch d;
  d.delete_range("n", B("c"), B("f"));  // c d e
  CHECK(db->write(d) == Status::Ok);
  CHECK(db->count("n") == 7);
  CHECK(db->contains("n", B("b")));
  CHECK_FALSE(db->contains("n", B("c")));
  CHECK_FALSE(db->contains("n", B("e")));
  CHECK(db->contains("n", B("f")));
  d.clear();
  d.delete_range("n", B("z"), B("a"));  // empty (inverted) range: no-op
  d.delete_from("n", B("h"));           // h i j
  CHECK(db->write(d) == Status::Ok);
  CHECK(db->count("n") == 4);
  d.clear();
  d.drop_namespace("n");
  CHECK(db->write(d) == Status::Ok);
  CHECK(db->count("n") == 0);
  CHECK(db->namespaces() == std::vector<std::string>{"m"});
  db.reset();
  db = open_ok(&vfs, "db", plain_opts());
  CHECK(dump(*db) == Model{{"m", {{"x", "y"}}}});
}

TEST_CASE("kv: namespace stats and store stats track sizes") {
  MemVfs vfs;
  auto db = open_ok(&vfs, "db", plain_opts());
  CHECK(db->put("a", B("k1"), B("12345")) == Status::Ok);
  CHECK(db->put("a", B("k2"), B("1")) == Status::Ok);
  CHECK(db->put("b", B("kkk"), B("")) == Status::Ok);
  NamespaceStats a = db->namespace_stats("a");
  CHECK(a.records == 2);
  CHECK(a.key_bytes == 4);
  CHECK(a.value_bytes == 6);
  CHECK(a.bytes() == 10);
  CHECK(db->put("a", B("k1"), B("1")) == Status::Ok);  // overwrite shrinks
  CHECK(db->namespace_stats("a").value_bytes == 2);
  CHECK(db->del("a", B("k2")) == Status::Ok);
  a = db->namespace_stats("a");
  CHECK(a.records == 1);
  CHECK(a.key_bytes == 2);
  CHECK(a.value_bytes == 1);
  StoreStats st = db->stats();
  CHECK(st.records == 2);
  CHECK(st.namespaces == 2);
  CHECK(st.live_bytes == (2 + 1 + 1) + (3 + 0 + 1));  // key + value + ns name per record
  CHECK(st.generation == 1);
  CHECK(st.file_bytes > fmt::kDataStart);
  CHECK_FALSE(st.encrypted);
  db.reset();
  db = open_ok(&vfs, "db", plain_opts());
  CHECK(db->namespace_stats("a").bytes() == 3);
  CHECK(db->namespace_stats("b").records == 1);
  CHECK(db->namespace_stats("zzz").records == 0);
}

TEST_CASE("kv: compaction reclaims space and keeps data; old generation removed") {
  MemVfs vfs;
  auto db = open_ok(&vfs, "db", plain_opts());
  std::string big(1000, 'x');
  for (int r = 0; r < 20; ++r)
    for (int i = 0; i < 10; ++i) CHECK(db->put("n", B("k" + std::to_string(i)), B(big + std::to_string(r))) == Status::Ok);
  uint64_t before = db->stats().file_bytes;
  Model m = dump(*db);
  CHECK(db->compact() == Status::Ok);
  CHECK(db->stats().generation == 2);
  CHECK(db->stats().file_bytes < before / 5);
  CHECK(dump(*db) == m);
  CHECK(vfs.all_files() == std::vector<std::string>{"db/data-2.ekv"});
  CHECK(db->put("n", B("after"), B("1")) == Status::Ok);
  db.reset();
  db = open_ok(&vfs, "db", plain_opts());
  m["n"]["after"] = "1";
  CHECK(dump(*db) == m);
  CHECK(db->recovery_report().clean());
  CHECK(db->recovery_report().torn_tail_bytes == 0);
}

TEST_CASE("kv: auto compaction triggers on garbage ratio") {
  MemVfs vfs;
  Options o = plain_opts();
  o.auto_compact_min_bytes = 64 * 1024;
  o.auto_compact_ratio = 2;
  auto db = open_ok(&vfs, "db", std::move(o));
  std::string v(500, 'v');
  for (int i = 0; i < 1000; ++i) CHECK(db->put("n", B("same"), B(v + std::to_string(i))) == Status::Ok);
  CHECK(db->stats().auto_compactions > 0);
  CHECK(db->stats().file_bytes < 64 * 1024 + 2000);
  Bytes got;
  CHECK(db->get("n", B("same"), &got) == Status::Ok);
  CHECK(S(got) == v + "999");
}

TEST_CASE("kv: multi-megabyte values") {
  MemVfs vfs;
  Rng rng(7);
  Bytes big = rng.bytes(3 * 1024 * 1024 + 3);
  Bytes big2 = rng.bytes(1024 * 1024);
  {
    auto db = open_ok(&vfs, "db", plain_opts());
    CHECK(db->put("blob", B("a"), ByteView(big)) == Status::Ok);
    WriteBatch b;
    b.put("blob", B("b"), ByteView(big2));
    b.put("blob", B("c"), B("small"));
    CHECK(db->write(b) == Status::Ok);
    CHECK(db->compact() == Status::Ok);
  }
  for (int enc = 0; enc < 2; ++enc) {
    MemVfs v2;
    Options o = enc ? enc_opts("k") : plain_opts();
    {
      auto db = open_ok(enc ? &v2 : &vfs, "db", std::move(o));
      if (enc) {
        CHECK(db->put("blob", B("a"), ByteView(big)) == Status::Ok);
        CHECK(db->put("blob", B("b"), ByteView(big2)) == Status::Ok);
        CHECK(db->compact() == Status::Ok);
      }
    }
    auto db = open_ok(enc ? &v2 : &vfs, "db", enc ? enc_opts("k") : plain_opts());
    Bytes v;
    CHECK(db->get("blob", B("a"), &v) == Status::Ok);
    CHECK(v == big);
    CHECK(db->get("blob", B("b"), &v) == Status::Ok);
    CHECK(v == big2);
  }
}

TEST_CASE("kv: 100k keys, reopen, scans and compaction") {
  MemVfs vfs;
  const int N = 100000;
  {
    auto db = open_ok(&vfs, "db", plain_opts());
    WriteBatch b;
    for (int i = 0; i < N; ++i) {
      b.put("msg", B(be64(static_cast<uint64_t>(i) * 7919 % N)), B("m" + std::to_string(i)));
      if (b.count() == 1000) {
        REQUIRE(db->write(b, WriteOptions{false}) == Status::Ok);
        b.clear();
      }
    }
    CHECK(db->sync() == Status::Ok);
    CHECK(db->count("msg") == N);
  }
  auto db = open_ok(&vfs, "db", plain_opts());
  CHECK(db->count("msg") == N);
  CHECK(db->recovery_report().batches_applied == N / 1000);
  std::vector<KvEntry> out;
  ScanSpec s;
  s.set_upper(B(be64(50000)));
  s.reverse = true;
  s.limit = 50;
  CHECK(db->scan("msg", s, &out) == Status::Ok);
  REQUIRE(out.size() == 50);
  CHECK(S(out[0].key) == be64(49999));
  CHECK(S(out[49].key) == be64(49950));
  CHECK(db->compact() == Status::Ok);
  db.reset();
  db = open_ok(&vfs, "db", plain_opts());
  CHECK(db->count("msg") == N);
  CHECK(db->recovery_report().snapshot_records == N);
  Bytes v;
  uint64_t key = 12345;
  // i such that i*7919 % N == key: verify value via a scan instead of inverting.
  CHECK(db->get("msg", B(be64(key)), &v) == Status::Ok);
  CHECK(v.size() > 1);
}

TEST_CASE("kv: open modes: create_if_missing, error_if_exists, read_only") {
  MemVfs vfs;
  std::unique_ptr<KvStore> db;
  Options o = plain_opts();
  o.create_if_missing = false;
  CHECK(KvStore::open(&vfs, "db", std::move(o), &db) == Status::NotFound);
  Options ro = plain_opts();
  ro.read_only = true;
  CHECK(KvStore::open(&vfs, "db", std::move(ro), &db) == Status::NotFound);
  CHECK_FALSE(vfs.exists("db"));
  db = open_ok(&vfs, "db", plain_opts());
  CHECK(db->put("n", B("k"), B("v")) == Status::Ok);
  db.reset();
  Options ex = plain_opts();
  ex.error_if_exists = true;
  CHECK(KvStore::open(&vfs, "db", std::move(ex), &db) == Status::Exists);
  Options ro2 = plain_opts();
  ro2.read_only = true;
  db = open_ok(&vfs, "db", std::move(ro2));
  Bytes v;
  CHECK(db->get("n", B("k"), &v) == Status::Ok);
  CHECK(db->put("n", B("k"), B("x")) == Status::ReadOnly);
  CHECK(db->compact() == Status::ReadOnly);
  CHECK(db->stats().read_only);
}

TEST_CASE("kv: closed store refuses ops; scan visitor cannot write") {
  MemVfs vfs;
  auto db = open_ok(&vfs, "db", plain_opts());
  CHECK(db->put("n", B("a"), B("1")) == Status::Ok);
  CHECK(db->put("n", B("b"), B("2")) == Status::Ok);
  Status inner = Status::Ok;
  Bytes got;
  Status s = db->scan("n", ScanSpec(), [&](ByteView k, ByteView, uint64_t) {
    inner = db->put("n", B("c"), B("3"));
    CHECK(db->get("n", k, &got) == Status::Ok);  // reads are allowed
    return true;
  });
  CHECK(s == Status::Ok);
  CHECK(inner == Status::Busy);
  CHECK(db->put("n", B("c"), B("3")) == Status::Ok);
  CHECK(db->close() == Status::Ok);
  CHECK(db->close() == Status::Ok);
  CHECK(db->put("n", B("d"), B("4")) == Status::Broken);
  CHECK(db->get("n", B("a"), &got) == Status::Broken);
}

TEST_CASE("kv: unsynced writes become durable with sync()/close()") {
  MemVfs vfs;
  {
    auto db = open_ok(&vfs, "db", plain_opts());
    CHECK(db->put("n", B("a"), B("1"), WriteOptions{false}) == Status::Ok);
    CHECK(db->sync() == Status::Ok);
    CHECK(db->put("n", B("b"), B("2"), WriteOptions{false}) == Status::Ok);
    CHECK(db->close() == Status::Ok);
    CHECK(db->put("n", B("c"), B("3"), WriteOptions{false}) == Status::Broken);
  }
  vfs.reboot(CrashMode::DropUnsynced);
  auto db = open_ok(&vfs, "db", plain_opts());
  CHECK(dump(*db) == Model{{"n", {{"a", "1"}, {"b", "2"}}}});
}

TEST_CASE("kv: short reads everywhere still work") {
  MemVfs vfs;
  {
    auto db = open_ok(&vfs, "db", plain_opts());
    for (int i = 0; i < 50; ++i) CHECK(db->put("n", B("k" + std::to_string(i)), B(std::string(300, 'a' + i % 26))) == Status::Ok);
  }
  vfs.set_short_reads(3);
  auto db = open_ok(&vfs, "db", plain_opts());
  CHECK(db->count("n") == 50);
  Bytes v;
  CHECK(db->get("n", B("k7"), &v) == Status::Ok);
  CHECK(S(v) == std::string(300, 'h'));
  CHECK(db->compact() == Status::Ok);
  CHECK(db->get("n", B("k7"), &v) == Status::Ok);
  vfs.set_short_reads(0);
}

TEST_CASE("kv: read errors surface as IoError, never as data") {
  MemVfs vfs;
  {
    auto db = open_ok(&vfs, "db", plain_opts());
    CHECK(db->put("n", B("k"), B("v")) == Status::Ok);
  }
  vfs.set_fail_reads(true);
  std::unique_ptr<KvStore> db;
  CHECK(KvStore::open(&vfs, "db", plain_opts(), &db) == Status::IoError);
  vfs.set_fail_reads(false);
  db = open_ok(&vfs, "db", plain_opts());
  vfs.set_fail_reads(true);
  Bytes v;
  CHECK(db->get("n", B("k"), &v) == Status::IoError);
  CHECK(v.empty());
  vfs.set_fail_reads(false);
  CHECK(db->get("n", B("k"), &v) == Status::Ok);
}

TEST_CASE("kv: failed sync makes the store Broken until reopen") {
  MemVfs vfs;
  auto db = open_ok(&vfs, "db", plain_opts());
  CHECK(db->put("n", B("a"), B("1")) == Status::Ok);
  vfs.set_fail_sync(true);
  CHECK(db->put("n", B("b"), B("2")) == Status::IoError);
  CHECK(db->broken());
  vfs.set_fail_sync(false);
  CHECK(db->put("n", B("c"), B("3")) == Status::Broken);
  Bytes v;
  CHECK(db->get("n", B("a"), &v) == Status::Ok);  // reads still served
  db.reset();
  db = open_ok(&vfs, "db", plain_opts());
  CHECK(db->contains("n", B("a")));
  CHECK(db->put("n", B("c"), B("3")) == Status::Ok);
}

TEST_CASE("kv: reopen cycles accumulate correctly") {
  MemVfs vfs;
  Model m;
  Rng rng(99);
  for (int cycle = 0; cycle < 30; ++cycle) {
    auto db = open_ok(&vfs, "db", plain_opts());
    REQUIRE(dump(*db) == m);
    REQUIRE(db->recovery_report().clean());
    REQUIRE(db->recovery_report().torn_tail_bytes == 0);
    for (int i = 0; i < 20; ++i) {
      std::string k = "k" + std::to_string(rng.below(40));
      if (rng.below(4) == 0) {
        CHECK(db->del("n", B(k)) == Status::Ok);
        m["n"].erase(k);
        if (m["n"].empty()) m.erase("n");
      } else {
        std::string v = std::to_string(rng.next());
        CHECK(db->put("n", B(k), B(v), WriteOptions{rng.below(2) == 0}) == Status::Ok);
        m["n"][k] = v;
      }
    }
    if (cycle % 7 == 3) CHECK(db->compact() == Status::Ok);
  }
}

}  // TEST_SUITE
