// KvStore: encryption at rest, key checks, re-keying, tamper detection, nonce uniqueness, probe, wipe.
#include <set>

#include "store/kv_format.h"
#include "store_test_util.h"

using namespace store_test;

namespace {

bool contains_bytes(const Bytes& hay, const std::string& needle) {
  return std::search(hay.begin(), hay.end(), needle.begin(), needle.end()) != hay.end();
}

bool any_file_contains(MemVfs& vfs, const std::string& needle) {
  for (const auto& f : vfs.all_files())
    if (contains_bytes(vfs.contents(f), needle)) return true;
  return false;
}

struct RawFrame {
  uint64_t off;
  uint8_t type, flags;
  uint32_t len;
};

// Test-side walk of a data file's frames (assumes an undamaged file).
std::vector<RawFrame> frames_of(const Bytes& d) {
  std::vector<RawFrame> out;
  uint64_t off = fmt::kDataStart;
  while (off + fmt::kFrameOverhead <= d.size()) {
    uint32_t len = static_cast<uint32_t>(d[off + 4] | d[off + 5] << 8 | d[off + 6] << 16 | uint32_t(d[off + 7]) << 24);
    out.push_back(RawFrame{off, d[off + 2], d[off + 3], len});
    off += fmt::kFrameOverhead + len;
  }
  return out;
}

std::string only_data_file(MemVfs& vfs) {
  std::string p;
  for (const auto& f : vfs.all_files())
    if (f.compare(0, 8, "db/data-") == 0) {
      CHECK(p.empty());
      p = f;
    }
  return p;
}

}  // namespace

TEST_SUITE("store") {

TEST_CASE("crypto: round trip; Locked / WrongKey / InvalidArgument") {
  MemVfs vfs;
  {
    auto db = open_ok(&vfs, "db", enc_opts("correct horse"));
    CHECK(db->stats().encrypted);
    CHECK(db->put("n", B("k"), B("v")) == Status::Ok);
  }
  std::unique_ptr<KvStore> db;
  CHECK(KvStore::open(&vfs, "db", plain_opts(), &db) == Status::Locked);
  CHECK(KvStore::open(&vfs, "db", enc_opts("wrong"), &db) == Status::WrongKey);
  db = open_ok(&vfs, "db", enc_opts("correct horse", 2));
  Bytes v;
  CHECK(db->get("n", B("k"), &v) == Status::Ok);
  CHECK(S(v) == "v");
  db.reset();
  MemVfs pv;
  { open_ok(&pv, "db", plain_opts()); }
  CHECK(KvStore::open(&pv, "db", enc_opts("x"), &db) == Status::InvalidArgument);
}

TEST_CASE("crypto: no key, value or namespace bytes reach the disk in the clear") {
  MemVfs vfs;
  const std::string kns = "SECRETNAMESPACE", kkey = "SECRETKEY-ALICE", kval = "SECRETVALUE-meet at the bridge";
  {
    auto db = open_ok(&vfs, "db", enc_opts("pw"));
    CHECK(db->put(kns, B(kkey), B(kval)) == Status::Ok);
    WriteBatch b;
    b.put(kns, B(kkey + "2"), B(kval + "2"));
    b.del(kns, B("SECRETDELETED"));
    b.delete_range(kns, B("SECRETLOW"), B("SECRETHIGH"));
    CHECK(db->write(b) == Status::Ok);
    for (const auto& n : {kns, kkey, kval, std::string("SECRETDELETED"), std::string("SECRETLOW"), std::string("SECRET")})
      CHECK_FALSE(any_file_contains(vfs, n));
    CHECK(db->compact() == Status::Ok);
    CHECK_FALSE(any_file_contains(vfs, "SECRET"));
  }
  CHECK_FALSE(any_file_contains(vfs, "SECRET"));
  // Sanity: the same data in a plaintext store is visible (the search works).
  MemVfs pv;
  {
    auto db = open_ok(&pv, "db", plain_opts());
    CHECK(db->put(kns, B(kkey), B(kval)) == Status::Ok);
  }
  CHECK(any_file_contains(pv, kkey));
}

TEST_CASE("crypto: probe works before the key is known") {
  MemVfs vfs;
  ProbeInfo pi;
  CHECK(KvStore::probe(vfs, "db", &pi) == Status::Ok);
  CHECK_FALSE(pi.exists);
  vfs.mkdir("db");
  CHECK(KvStore::probe(vfs, "db", &pi) == Status::Ok);
  CHECK_FALSE(pi.exists);
  {
    Options o = enc_opts("pw");
    o.cipher_params = Bytes{'a', 'r', 'g', 'o', 'n', 0, 1, 2};
    auto db = open_ok(&vfs, "db", std::move(o));
  }
  uint64_t ops = vfs.op_count();
  CHECK(KvStore::probe(vfs, "db", &pi) == Status::Ok);
  CHECK(vfs.op_count() == ops);  // read-only
  CHECK(pi.exists);
  CHECK(pi.encrypted);
  CHECK(pi.format_version == fmt::kFormatVersionBoundSnapshot);  // encrypted stores are written as v2
  CHECK(pi.generation == 1);
  CHECK(pi.cipher_params == Bytes{'a', 'r', 'g', 'o', 'n', 0, 1, 2});
  MemVfs pv;
  { open_ok(&pv, "p", plain_opts()); }
  CHECK(KvStore::probe(pv, "p", &pi) == Status::Ok);
  CHECK(pi.exists);
  CHECK_FALSE(pi.encrypted);
  CHECK(pi.format_version == fmt::kFormatVersion);  // plaintext stores stay v1 (older builds read them)
}

TEST_CASE("crypto: rekey changes the key atomically; to and from plaintext") {
  MemVfs vfs;
  Model m;
  {
    auto db = open_ok(&vfs, "db", enc_opts("old"));
    for (int i = 0; i < 50; ++i) {
      CHECK(db->put("n", B("k" + std::to_string(i)), B("v" + std::to_string(i))) == Status::Ok);
      m["n"]["k" + std::to_string(i)] = "v" + std::to_string(i);
    }
    std::unique_ptr<RecordCipher> c(new InsecureTestCipher("new"));
    CHECK(db->rekey(std::move(c), B("new")) == Status::Ok);
    CHECK(dump(*db) == m);
    CHECK(db->put("n", B("after"), B("rekey")) == Status::Ok);
    m["n"]["after"] = "rekey";
  }
  CHECK(vfs.all_files() == std::vector<std::string>{"db/data-2.ekv"});
  ProbeInfo pi;
  CHECK(KvStore::probe(vfs, "db", &pi) == Status::Ok);
  CHECK(S(pi.cipher_params) == "new");
  std::unique_ptr<KvStore> db;
  CHECK(KvStore::open(&vfs, "db", enc_opts("old"), &db) == Status::WrongKey);
  db = open_ok(&vfs, "db", enc_opts("new", 3));
  CHECK(dump(*db) == m);
  // Decrypt to plaintext.
  CHECK(db->rekey(nullptr, ByteView()) == Status::Ok);
  CHECK_FALSE(db->stats().encrypted);
  db.reset();
  CHECK(KvStore::probe(vfs, "db", &pi) == Status::Ok);
  CHECK_FALSE(pi.encrypted);
  db = open_ok(&vfs, "db", plain_opts());
  CHECK(dump(*db) == m);
  CHECK(any_file_contains(vfs, "after"));
  // Encrypt a plaintext store (needs entropy).
  std::unique_ptr<RecordCipher> c(new InsecureTestCipher("third"));
  CHECK(db->rekey(std::move(c), B("third")) == Status::InvalidArgument);  // plain_opts has no entropy
  db.reset();
  Options o = plain_opts();
  o.entropy = test_entropy(8);
  db = open_ok(&vfs, "db", std::move(o));
  c.reset(new InsecureTestCipher("third"));
  CHECK(db->rekey(std::move(c), B("third")) == Status::Ok);
  db.reset();
  CHECK_FALSE(any_file_contains(vfs, "after"));
  db = open_ok(&vfs, "db", enc_opts("third", 4));
  CHECK(dump(*db) == m);
}

TEST_CASE("crypto: rekey interrupted at every operation leaves exactly one working key") {
  uint64_t rekey_ops = 0;
  {
    MemVfs vfs;
    Options o = enc_opts("old");
    o.snapshot_chunk_bytes = 300;
    auto db = open_ok(&vfs, "db", std::move(o));
    for (int i = 0; i < 40; ++i) REQUIRE(db->put("n", B("k" + std::to_string(i)), B(std::string(50, 'x'))) == Status::Ok);
    uint64_t before = vfs.op_count();
    std::unique_ptr<RecordCipher> c(new InsecureTestCipher("new"));
    REQUIRE(db->rekey(std::move(c), B("new")) == Status::Ok);
    rekey_ops = vfs.op_count() - before;
  }
  REQUIRE(rekey_ops > 5);
  int old_wins = 0, new_wins = 0;
  for (uint64_t k = 0; k < rekey_ops; ++k) {
    for (int mode = 0; mode < 3; ++mode) {
      MemVfs vfs;
      Model m;
      {
        Options o = enc_opts("old", 50 + k);
        o.snapshot_chunk_bytes = 300;
        auto db = open_ok(&vfs, "db", std::move(o));
        for (int i = 0; i < 40; ++i) {
          REQUIRE(db->put("n", B("k" + std::to_string(i)), B(std::string(50, 'x'))) == Status::Ok);
          m["n"]["k" + std::to_string(i)] = std::string(50, 'x');
        }
        vfs.crash_after_ops(static_cast<int64_t>(k));
        std::unique_ptr<RecordCipher> c(new InsecureTestCipher("new"));
        db->rekey(std::move(c), B("new"));  // may even return Ok if only the best-effort cleanup was cut
      }
      vfs.reboot(static_cast<CrashMode>(mode), k);
      ProbeInfo pi;
      REQUIRE(KvStore::probe(vfs, "db", &pi) == Status::Ok);
      std::string key = S(pi.cipher_params), other = key == "old" ? "new" : "old";
      (key == "old" ? old_wins : new_wins)++;
      std::unique_ptr<KvStore> db;
      CHECK(KvStore::open(&vfs, "db", enc_opts(other, 7), &db) == Status::WrongKey);
      RecoveryReport rep;
      db = open_ok(&vfs, "db", enc_opts(key, 8), &rep);
      CHECK(rep.clean());
      CHECK(dump(*db) == m);
      db.reset();
      // Leftovers (tmp file or the old generation) are gone after a clean open.
      CHECK(vfs.all_files().size() == 1);
    }
  }
  CHECK(old_wins > 0);
  CHECK(new_wins > 0);
}

TEST_CASE("crypto: tampering with ciphertext (CRC recomputed) is rejected by authentication") {
  MemVfs vfs;
  {
    auto db = open_ok(&vfs, "db", enc_opts("pw"));
    CHECK(db->put("n", B("a"), B("first")) == Status::Ok);
    CHECK(db->put("n", B("b"), B("second")) == Status::Ok);
    CHECK(db->put("n", B("c"), B("third")) == Status::Ok);
  }
  std::string path = only_data_file(vfs);
  Bytes d = vfs.contents(path);
  fmt::Header h;
  REQUIRE(fmt::decode_header(ByteView(d), &h));
  uint32_t seed = fmt::crc32(ByteView(h.salt, 16));
  auto fr = frames_of(d);
  REQUIRE(fr.size() == 4);  // commit + 3 batches
  const RawFrame& f = fr[2];  // batch "b"
  CHECK(f.type == fmt::kFrameBatch);
  CHECK(f.flags == fmt::kFrameFlagEncrypted);
  d[f.off + fmt::kFrameHeader + 30] ^= 0x01;  // inside ciphertext
  uint32_t hcrc = fmt::crc32(ByteView(d.data() + f.off, 16), seed);
  uint32_t fcrc = fmt::crc32(ByteView(d.data() + f.off + fmt::kFrameHeader, f.len), hcrc);
  for (int i = 0; i < 4; ++i) d[f.off + fmt::kFrameHeader + f.len + i] = static_cast<uint8_t>(fcrc >> (8 * i));
  vfs.set_contents(path, ByteView(d));
  RecoveryReport rep;
  auto db = open_ok(&vfs, "db", enc_opts("pw", 2), &rep);
  CHECK(rep.rejected_frames == 1);
  CHECK(rep.lost_ops == 1);
  CHECK_FALSE(rep.clean());
  CHECK(dump(*db) == Model{{"n", {{"a", "first"}, {"c", "third"}}}});
}

TEST_CASE("crypto: frames spliced from another store file are rejected") {
  MemVfs a, b;
  {
    auto da = open_ok(&a, "db", enc_opts("pw", 1));
    CHECK(da->put("n", B("a"), B("1")) == Status::Ok);
    auto dbb = open_ok(&b, "db", enc_opts("pw", 2));  // same key, different file salt
    CHECK(dbb->put("n", B("evil"), B("1")) == Status::Ok);
  }
  Bytes da = a.contents("db/data-1.ekv"), dbb = b.contents("db/data-1.ekv");
  auto fb = frames_of(dbb);
  const RawFrame& f = fb.back();
  // Re-seal nothing: copy the frame bytes and fix the CRCs for A's salt (attacker can read it).
  Bytes frame(dbb.begin() + static_cast<std::ptrdiff_t>(f.off), dbb.begin() + static_cast<std::ptrdiff_t>(f.off + fmt::kFrameOverhead + f.len));
  fmt::Header ha;
  REQUIRE(fmt::decode_header(ByteView(da), &ha));
  uint32_t seed = fmt::crc32(ByteView(ha.salt, 16));
  frame[8] = 2;  // seq 2 == next expected in A
  for (int i = 9; i < 16; ++i) frame[i] = 0;
  uint32_t hcrc = fmt::crc32(ByteView(frame.data(), 16), seed);
  for (int i = 0; i < 4; ++i) frame[16 + i] = static_cast<uint8_t>(hcrc >> (8 * i));
  uint32_t fcrc = fmt::crc32(ByteView(frame.data() + 20, f.len), hcrc);
  for (int i = 0; i < 4; ++i) frame[20 + f.len + i] = static_cast<uint8_t>(fcrc >> (8 * i));
  da.insert(da.end(), frame.begin(), frame.end());
  a.set_contents("db/data-1.ekv", ByteView(da));
  RecoveryReport rep;
  auto db = open_ok(&a, "db", enc_opts("pw", 3), &rep);
  CHECK(rep.rejected_frames == 1);
  CHECK_FALSE(db->contains("n", B("evil")));
  CHECK(db->contains("n", B("a")));
}

TEST_CASE("crypto: nonces never repeat across writes, reopens and compactions") {
  MemVfs vfs;
  std::set<Bytes> nonces;
  size_t frames = 0;
  uint64_t seed = 100;
  for (int cycle = 0; cycle < 6; ++cycle) {
    {
      auto db = open_ok(&vfs, "db", enc_opts("pw", ++seed));
      for (int i = 0; i < 10; ++i) CHECK(db->put("n", B("k" + std::to_string(i % 4)), B("v")) == Status::Ok);
      if (cycle % 2) CHECK(db->compact() == Status::Ok);
      for (int i = 0; i < 3; ++i) CHECK(db->put("n", B("x"), B("y")) == Status::Ok);
    }
    // Collect nonces from every frame of the current file plus the key check.
    std::string path = only_data_file(vfs);
    Bytes d = vfs.contents(path);
    fmt::Header h;
    REQUIRE(fmt::decode_header(ByteView(d), &h));
    nonces.insert(Bytes(h.keycheck.begin(), h.keycheck.begin() + 24));
    for (const auto& f : frames_of(d)) {
      nonces.insert(Bytes(d.begin() + static_cast<std::ptrdiff_t>(f.off + 20), d.begin() + static_cast<std::ptrdiff_t>(f.off + 44)));
      ++frames;
    }
  }
  CHECK(nonces.size() > 40);  // sessions/compactions keep producing fresh nonces
  // Strict check within the final file.
  Bytes d = vfs.contents(only_data_file(vfs));
  std::set<Bytes> local;
  auto fr = frames_of(d);
  for (const auto& f : fr) local.insert(Bytes(d.begin() + static_cast<std::ptrdiff_t>(f.off + 20), d.begin() + static_cast<std::ptrdiff_t>(f.off + 44)));
  CHECK(local.size() == fr.size());
  (void)frames;
}

TEST_CASE("crypto: reads use the one-frame cache for snapshot chunks") {
  MemVfs vfs;
  Options o = enc_opts("pw");
  o.snapshot_chunk_bytes = 4096;
  auto* cipher = new InsecureTestCipher("pw");
  o.cipher.reset(cipher);
  auto db = open_ok(&vfs, "db", std::move(o));
  WriteBatch b;
  for (int i = 0; i < 100; ++i) b.put("n", B(be64(i)), B("v"));
  CHECK(db->write(b) == Status::Ok);
  CHECK(db->compact() == Status::Ok);
  uint64_t before = cipher->opens();
  std::vector<KvEntry> out;
  CHECK(db->scan("n", ScanSpec(), &out) == Status::Ok);
  CHECK(out.size() == 100);
  CHECK(cipher->opens() - before <= 3);  // ~1 decrypt per 4 KiB chunk, not per record
}

TEST_CASE("wipe: removes everything; headers go first so an interrupted wipe still shreds") {
  for (int enc = 0; enc < 2; ++enc) {
    MemVfs vfs;
    {
      auto db = open_ok(&vfs, "db", enc ? enc_opts("pw") : plain_opts());
      for (int i = 0; i < 100; ++i) CHECK(db->put("n", B("k" + std::to_string(i)), B("secret")) == Status::Ok);
      CHECK(db->wipe() == Status::Ok);
      CHECK(db->put("n", B("k"), B("v")) == Status::Broken);
    }
    CHECK_FALSE(vfs.exists("db"));
    CHECK(vfs.all_files().empty());
    ProbeInfo pi;
    CHECK(KvStore::probe(vfs, "db", &pi) == Status::Ok);
    CHECK_FALSE(pi.exists);
    CHECK(KvStore::wipe(vfs, "db") == Status::Ok);  // idempotent
  }
  // Interrupt a wipe of a directory with a leftover tmp file and two generations.
  uint64_t total_ops = 0;
  auto setup = [](MemVfs& vfs) {
    auto db = open_ok(&vfs, "db", enc_opts("pw"));
    for (int i = 0; i < 50; ++i) REQUIRE(db->put("n", B("k" + std::to_string(i)), B("secret")) == Status::Ok);
    db.reset();
    Bytes g1 = vfs.contents("db/data-1.ekv");
    vfs.set_contents("db/tmp-7.ekv", ByteView(g1));
    vfs.set_contents("db/data-0.ekv", ByteView(g1));
  };
  {
    MemVfs vfs;
    setup(vfs);
    uint64_t before = vfs.op_count();
    CHECK(KvStore::wipe(vfs, "db") == Status::Ok);
    total_ops = vfs.op_count() - before;
  }
  for (uint64_t k = 0; k < total_ops; ++k) {
    MemVfs vfs;
    setup(vfs);
    vfs.crash_after_ops(static_cast<int64_t>(k));
    CHECK(KvStore::wipe(vfs, "db") != Status::Ok);
    vfs.reboot(CrashMode::KeepAll);
    ProbeInfo pi;
    Status ps = KvStore::probe(vfs, "db", &pi);
    if (k >= 6) {  // 3 files x (write + sync): every header is destroyed
      CHECK((ps == Status::Corruption || !pi.exists));
      for (const auto& f : vfs.all_files()) {
        Bytes c = vfs.contents(f);
        for (size_t i = 0; i < c.size() && i < 4096; ++i) REQUIRE(c[i] == 0);
      }
    }
  }
  // Quick wipe.
  MemVfs vfs;
  setup(vfs);
  WipeOptions q;
  q.full = false;
  CHECK(KvStore::wipe(vfs, "db", q) == Status::Ok);
  CHECK_FALSE(vfs.exists("db"));
}

}  // TEST_SUITE
