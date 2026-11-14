// KvStore: regression tests for the security review findings (M8, L8 store side).
//   * forged frame headers must not make recovery quadratic (bounded work on open);
//   * a planted plaintext generation must never downgrade an encrypted store;
//   * snapshot chunks are bound to their ordinal (swap / reorder / drop are detected);
//   * scrubbing never follows symlinks or hard links out of the store directory;
//   * compaction / aborted re-key do not leave old cleartext in removed files.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

#include "store/kv_format.h"
#include "store_test_util.h"

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#define EMBER_STORE_POSIX_LINKS 1
#endif

using namespace store_test;

namespace {

struct Frm {
  uint64_t off;
  uint8_t type;
  uint32_t len;
};

std::vector<Frm> walk(const Bytes& d) {
  std::vector<Frm> out;
  uint64_t off = fmt::kDataStart;
  while (off + fmt::kFrameOverhead <= d.size()) {
    uint32_t len = static_cast<uint32_t>(d[off + 4] | d[off + 5] << 8 | d[off + 6] << 16 | uint32_t(d[off + 7]) << 24);
    out.push_back(Frm{off, d[off + 2], len});
    off += fmt::kFrameOverhead + len;
  }
  return out;
}

Bytes frame_bytes(const Bytes& d, const Frm& f) {
  return Bytes(d.begin() + static_cast<std::ptrdiff_t>(f.off),
               d.begin() + static_cast<std::ptrdiff_t>(f.off + fmt::kFrameOverhead + f.len));
}

std::string data_file(MemVfs& v, const std::string& dir) {
  std::string p;
  for (const auto& f : v.all_files())
    if (f.compare(0, dir.size() + 6, dir + "/data-") == 0) p = f;  // highest sorts last for same width
  return p;
}

void ple(uint8_t* p, uint64_t v, int n) {
  for (int i = 0; i < n; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}

bool has(const Bytes& hay, const std::string& needle) {
  return std::search(hay.begin(), hay.end(), needle.begin(), needle.end()) != hay.end();
}

double secs_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// Every value the store returns must be one of the values written for that key.
void check_subset(KvStore& db, const Model& truth) {
  Model got = dump(db);
  for (const auto& ns : got)
    for (const auto& kv : ns.second) {
      auto a = truth.find(ns.first);
      REQUIRE(a != truth.end());
      auto b = a->second.find(kv.first);
      REQUIRE(b != a->second.end());
      CHECK(b->second == kv.second);
    }
}


// Builds a version-1 encrypted store file exactly as the pre-review writer did (snapshot AAD
// without ordinal / running CRC), so the migration path stays tested.
Bytes legacy_v1_file(const std::string& key, uint64_t gen, const std::vector<std::pair<std::string, std::string>>& recs,
                     size_t per_chunk, std::vector<Bytes>* chunks_out = nullptr, Bytes* commit_out = nullptr) {
  InsecureTestCipher c(key);
  fmt::Header h;
  h.version = 1;
  h.flags = fmt::kFileFlagEncrypted;
  h.generation = gen;
  h.base_seq = recs.size();
  for (int i = 0; i < 16; ++i) h.salt[i] = static_cast<uint8_t>(0x30 + i);
  h.params = Bytes(key.begin(), key.end());
  uint8_t prefix[16];
  for (int i = 0; i < 16; ++i) prefix[i] = static_cast<uint8_t>(0xA0 + i);
  uint64_t ctr = 0;
  auto nonce = [&]() {
    Bytes n(prefix, prefix + 16);
    for (int i = 0; i < 8; ++i) n.push_back(static_cast<uint8_t>(ctr >> (8 * i)));
    ++ctr;
    return n;
  };
  {
    Bytes n = nonce(), ct;
    REQUIRE(c.seal(ByteView(n), ByteView(fmt::header_aad(h)), B(fmt::kKeyCheckPlain).sub(0, 16), &ct));
    h.keycheck = n;
    h.keycheck.insert(h.keycheck.end(), ct.begin(), ct.end());
  }
  Bytes out = fmt::encode_header(h);
  Bytes hb = out;
  out.insert(out.end(), hb.begin(), hb.end());
  uint32_t seed = fmt::crc32(ByteView(h.salt, 16));
  auto frame = [&](uint8_t type, ByteView pt) {
    Bytes f(20, 0);
    f[0] = 'E'; f[1] = 0x4B; f[2] = type; f[3] = fmt::kFrameFlagEncrypted;
    ple(&f[4], 24 + pt.size() + c.overhead(), 4);
    ple(&f[8], h.base_seq, 8);
    uint32_t hcrc = fmt::crc32(ByteView(f.data(), 16), seed);
    ple(&f[16], hcrc, 4);
    Bytes aad(h.salt, h.salt + 16);
    for (int i = 0; i < 8; ++i) aad.push_back(static_cast<uint8_t>(gen >> (8 * i)));
    aad.insert(aad.end(), f.begin(), f.begin() + 16);
    Bytes n = nonce(), ct;
    REQUIRE(c.seal(ByteView(n), ByteView(aad), pt, &ct));
    f.insert(f.end(), n.begin(), n.end());
    f.insert(f.end(), ct.begin(), ct.end());
    uint8_t t[4];
    ple(t, fmt::crc32(ByteView(f.data() + 20, f.size() - 20), hcrc), 4);
    f.insert(f.end(), t, t + 4);
    return f;
  };
  size_t nchunks = 0;
  for (size_t i = 0; i < recs.size(); i += per_chunk) {
    size_t n = std::min(per_chunk, recs.size() - i);
    ByteWriter w;
    w.varint(n);
    for (size_t k = i; k < i + n; ++k)
      w.varint(1).str("n").varint(recs[k].first.size()).str(recs[k].first).varint(k + 1)
          .varint(recs[k].second.size()).str(recs[k].second);
    Bytes f = frame(fmt::kFrameSnapChunk, ByteView(w.data()));
    if (chunks_out) chunks_out->push_back(f);
    out.insert(out.end(), f.begin(), f.end());
    ++nchunks;
  }
  ByteWriter w;
  w.varint(recs.size()).varint(nchunks);
  Bytes f = frame(fmt::kFrameSnapCommit, ByteView(w.data()));
  if (commit_out) *commit_out = f;
  out.insert(out.end(), f.begin(), f.end());
  return out;
}
}  // namespace

TEST_SUITE("store") {

// M8: frame headers with a valid header CRC every 20 bytes, each declaring a payload that runs to
// the end of the file with a wrong payload CRC. Recovery used to CRC ~size/2 bytes per candidate.
TEST_CASE("security: forged frame headers cannot make recovery quadratic") {
  for (size_t total : {size_t(256) << 10, size_t(2) << 20}) {
    CAPTURE(total);
    MemVfs mem;
    CountingVfs v(&mem);
    {
      auto db = open_ok(&v, "s", plain_opts());
      CHECK(db->put("ns", B("k"), B("v")) == Status::Ok);
    }
    std::string path = data_file(mem, "s");
    Bytes f = mem.contents(path);
    size_t genuine_end = f.size();
    fmt::Header h;
    REQUIRE(fmt::decode_header(ByteView(f), &h));
    uint32_t seed = fmt::crc32(ByteView(h.salt, 16));
    f.resize(genuine_end + total, 0);
    size_t n = 0;
    for (size_t off = genuine_end; off + 24 <= f.size(); off += 20, ++n) {
      uint8_t* p = &f[off];
      p[0] = 'E'; p[1] = 0x4B; p[2] = 1; p[3] = 0;
      ple(p + 4, f.size() - off - 24, 4);
      ple(p + 8, 2, 8);
      ple(p + 16, fmt::crc32(ByteView(p, 16), seed), 4);
    }
    mem.set_contents(path, ByteView(f));
    v.reads = v.read_bytes = 0;
    auto t0 = std::chrono::steady_clock::now();
    std::unique_ptr<KvStore> db;
    RecoveryReport rep;
    Status st = KvStore::open(&v, "s", plain_opts(), &db, &rep);
    double t = secs_since(t0);
    MESSAGE("forged headers: " << n << ", open " << std::string(status_name(st)) << " in " << t << " s, read "
                               << v.read_bytes << " bytes in " << v.reads << " reads");
    REQUIRE(st == Status::Ok);
    CHECK_FALSE(rep.clean());
    // Deterministic work bound: a small constant number of passes over the file.
    CHECK(v.read_bytes <= 8 * f.size() + (size_t(1) << 20));
    CHECK(t < 10.0);  // generous: sanitizers, slow CI
    Bytes val;
    CHECK(db->get("ns", B("k"), &val) == Status::Ok);  // the genuine frame before the junk survives
    CHECK(S(val) == "v");
  }
}

// M8: a plaintext data-<g+1>.ekv planted next to an encrypted store must not be preferred, and the
// encrypted generation must not be deleted in its favour.
TEST_CASE("security: planted plaintext generation never downgrades an encrypted store") {
  MemVfs vfs;
  Model truth;
  {
    auto db = open_ok(&vfs, "db", enc_opts("pw"));
    for (int i = 0; i < 20; ++i) {
      std::string k = "k" + std::to_string(i), val = "secret-" + std::to_string(i);
      CHECK(db->put("n", B(k), B(val)) == Status::Ok);
      truth["n"][k] = val;
    }
    CHECK(db->compact() == Status::Ok);
  }
  std::string enc_path = data_file(vfs, "db");
  Bytes enc_before = vfs.contents(enc_path);
  fmt::Header eh;
  REQUIRE(fmt::decode_header(ByteView(enc_before), &eh));
  // Attacker's plaintext store, relabelled as the next generation.
  MemVfs other;
  {
    auto db = open_ok(&other, "db", plain_opts());
    CHECK(db->put("n", B("k0"), B("attacker")) == Status::Ok);
  }
  Bytes planted = other.contents(data_file(other, "db"));
  fmt::Header ph;
  REQUIRE(fmt::decode_header(ByteView(planted), &ph));
  ph.generation = eh.generation + 1;
  Bytes hb = fmt::encode_header(ph);
  std::copy(hb.begin(), hb.end(), planted.begin());
  std::copy(hb.begin(), hb.end(), planted.begin() + fmt::kHeaderSize);
  std::string planted_path = "db/" + fmt::data_name(eh.generation + 1);
  vfs.set_contents(planted_path, ByteView(planted));

  ProbeInfo pi;
  REQUIRE(KvStore::probe(vfs, "db", &pi) == Status::Ok);
  CHECK(pi.exists);
  CHECK(pi.encrypted);  // the node then asks for the passphrase instead of opening in the clear
  CHECK(pi.mixed_encryption);
  CHECK(S(pi.cipher_params) == "pw");

  {  // no key: refuse, touch nothing
    std::unique_ptr<KvStore> db;
    RecoveryReport rep;
    CHECK(KvStore::open(&vfs, "db", plain_opts(), &db, &rep) == Status::Locked);
    CHECK(rep.mixed_encryption);
    CHECK(vfs.contents(enc_path) == enc_before);
    CHECK(vfs.exists(planted_path));
  }
  {  // read-only with the key: the encrypted generation is used, nothing removed
    Options o = enc_opts("pw", 2);
    o.read_only = true;
    RecoveryReport rep;
    auto db = open_ok(&vfs, "db", std::move(o), &rep);
    CHECK(rep.mixed_encryption);
    CHECK(dump(*db) == truth);
    CHECK(vfs.exists(planted_path));
  }
  {  // with the key: encrypted data, planted cleartext generation discarded
    RecoveryReport rep;
    auto db = open_ok(&vfs, "db", enc_opts("pw", 3), &rep);
    CHECK(rep.mixed_encryption);
    CHECK(dump(*db) == truth);
    CHECK(db->stats().encrypted);
    CHECK(vfs.exists(enc_path));
    CHECK_FALSE(vfs.exists(planted_path));
    CHECK(db->put("n", B("new"), B("x")) == Status::Ok);
    CHECK(db->compact() == Status::Ok);  // next generation is above the planted number
  }
  {
    ProbeInfo p2;
    REQUIRE(KvStore::probe(vfs, "db", &p2) == Status::Ok);
    CHECK(p2.encrypted);
    CHECK_FALSE(p2.mixed_encryption);
    CHECK(p2.generation > eh.generation + 1);
  }
}

TEST_CASE("security: require_encryption refuses plaintext and missing stores (Status::Tampered)") {
  MemVfs vfs;
  {
    auto db = open_ok(&vfs, "db", plain_opts());
    CHECK(db->put("n", B("k"), B("v")) == Status::Ok);
  }
  std::unique_ptr<KvStore> db;
  Options o = plain_opts();
  o.require_encryption = true;
  CHECK(KvStore::open(&vfs, "db", std::move(o), &db) == Status::Tampered);
  Options o2 = enc_opts("pw");
  o2.require_encryption = true;
  CHECK(KvStore::open(&vfs, "db", std::move(o2), &db) == Status::Tampered);
  CHECK(vfs.exists("db/data-1.ekv"));  // nothing deleted
  // Store directory gone entirely: no silent fresh plaintext store either.
  MemVfs empty;
  Options o3 = plain_opts();
  o3.require_encryption = true;
  CHECK(KvStore::open(&empty, "db", std::move(o3), &db) == Status::Tampered);
  // With a cipher a fresh encrypted store may be created.
  Options o4 = enc_opts("pw");
  o4.require_encryption = true;
  CHECK(KvStore::open(&empty, "db", std::move(o4), &db) == Status::Ok);
  CHECK(std::string(status_name(Status::Tampered)) == "tampered");
}

// M8: snapshot chunks of an encrypted store are bound to their position.
TEST_CASE("security: snapshot chunk swap, reorder, drop are detected") {
  MemVfs base;
  Model truth;
  {
    Options o = enc_opts("pw");
    o.snapshot_chunk_bytes = 256;
    auto db = open_ok(&base, "db", std::move(o));
    WriteBatch b;
    for (int i = 0; i < 300; ++i) {
      char k[8], val[24];
      std::snprintf(k, sizeof k, "k%03d", i);
      std::snprintf(val, sizeof val, "value-%013d", i);
      b.put("n", B(k), B(val));
      truth["n"][k] = val;
    }
    CHECK(db->write(b) == Status::Ok);
    CHECK(db->compact() == Status::Ok);
  }
  std::string path = data_file(base, "db");
  Bytes d = base.contents(path);
  auto fr = walk(d);
  std::vector<Bytes> chunks;
  Bytes commit;
  for (const auto& f : fr) {
    if (f.type == fmt::kFrameSnapChunk) chunks.push_back(frame_bytes(d, f));
    if (f.type == fmt::kFrameSnapCommit) commit = frame_bytes(d, f);
  }
  REQUIRE(chunks.size() > 6);
  REQUIRE(!commit.empty());
  REQUIRE(chunks[0].size() == chunks[1].size());  // same record count: the old count check misses swaps

  auto rebuild = [&](const std::vector<Bytes>& cs, bool with_commit) {
    Bytes out(d.begin(), d.begin() + fmt::kDataStart);
    for (const auto& c : cs) out.insert(out.end(), c.begin(), c.end());
    if (with_commit) out.insert(out.end(), commit.begin(), commit.end());
    return out;
  };
  struct Case {
    const char* name;
    std::vector<Bytes> cs;
    bool commit;
  };
  std::vector<Case> cases;
  {
    auto c = chunks; c[1] = c[0]; cases.push_back({"swap B for a copy of A", c, true});
  }
  {
    auto c = chunks; std::swap(c[0], c[1]); cases.push_back({"reorder", c, true});
  }
  {
    auto c = chunks; c.insert(c.begin() + 3, c[2]); cases.push_back({"duplicate", c, true});
  }
  {
    auto c = chunks; c.erase(c.begin() + 3); cases.push_back({"drop middle", c, true});
  }
  {
    auto c = chunks; c.pop_back(); cases.push_back({"drop last chunk", c, true});
  }
  {
    auto c = chunks; c.resize(c.size() / 2); cases.push_back({"drop tail and commit", c, false});
  }
  for (const auto& tc : cases) {
    CAPTURE(tc.name);
    MemVfs v;
    v.mkdir("db");
    v.set_contents(path, ByteView(rebuild(tc.cs, tc.commit)));
    RecoveryReport rep;
    Options o = enc_opts("pw", 9);
    o.read_only = true;
    auto db = open_ok(&v, "db", std::move(o), &rep);
    CHECK_FALSE(rep.clean());
    check_subset(*db, truth);
  }
  {  // untouched file is clean
    MemVfs v;
    v.mkdir("db");
    v.set_contents(path, ByteView(d));
    RecoveryReport rep;
    auto db = open_ok(&v, "db", enc_opts("pw", 9), &rep);
    CHECK(rep.clean());
    CHECK(dump(*db) == truth);
  }
}

TEST_CASE("security: version-1 encrypted stores still open, keep ordinal checks, migrate on compaction") {
  std::vector<std::pair<std::string, std::string>> recs;
  Model truth;
  for (int i = 0; i < 40; ++i) {
    recs.emplace_back("key" + std::to_string(100 + i), "val" + std::to_string(i));
    truth["n"][recs.back().first] = recs.back().second;
  }
  std::vector<Bytes> chunks;
  Bytes commit;
  Bytes file = legacy_v1_file("pw", 1, recs, 4, &chunks, &commit);
  {
    MemVfs v;
    v.mkdir("db");
    v.set_contents("db/data-1.ekv", ByteView(file));
    ProbeInfo pi;
    REQUIRE(KvStore::probe(v, "db", &pi) == Status::Ok);
    CHECK(pi.format_version == 1);
    RecoveryReport rep;
    auto db = open_ok(&v, "db", enc_opts("pw", 4), &rep);
    CHECK(rep.clean());
    CHECK(dump(*db) == truth);
    CHECK(db->put("n", B("after"), B("x")) == Status::Ok);
    CHECK(db->compact() == Status::Ok);
    db.reset();
    REQUIRE(KvStore::probe(v, "db", &pi) == Status::Ok);
    CHECK(pi.format_version == fmt::kFormatVersionBoundSnapshot);
    RecoveryReport rep2;
    db = open_ok(&v, "db", enc_opts("pw", 5), &rep2);
    CHECK(rep2.clean());
    truth["n"]["after"] = "x";
    CHECK(dump(*db) == truth);
    truth["n"].erase("after");
  }
  {  // v1 chunk swap is still caught through the nonce ordinals
    Bytes f2(file.begin(), file.begin() + fmt::kDataStart);
    std::vector<Bytes> cs = chunks;
    cs[1] = cs[0];
    for (auto& c : cs) f2.insert(f2.end(), c.begin(), c.end());
    f2.insert(f2.end(), commit.begin(), commit.end());
    MemVfs v;
    v.mkdir("db");
    v.set_contents("db/data-1.ekv", ByteView(f2));
    RecoveryReport rep;
    Options o = enc_opts("pw", 4);
    o.read_only = true;
    auto db = open_ok(&v, "db", std::move(o), &rep);
    CHECK_FALSE(rep.clean());
    check_subset(*db, truth);
  }
}

// L8 / M8: removed generations and abandoned tmp files must not carry old cleartext.
TEST_CASE("security: compaction and aborted re-key leave no cleartext in removed files") {
  {
    MemVfs mem;
    CountingVfs v(&mem);
    int leaks = 0;
    v.on_remove = [&](const std::string&, const Bytes& c) { leaks += has(c, "TOPSECRET"); };
    auto db = open_ok(&v, "db", plain_opts());
    CHECK(db->put("n", B("k"), B("TOPSECRET")) == Status::Ok);
    CHECK(db->del("n", B("k")) == Status::Ok);
    CHECK(db->compact() == Status::Ok);
    CHECK(leaks == 0);
  }
  {
    MemVfs mem;
    CountingVfs v(&mem);
    int leaks = 0;
    v.on_remove = [&](const std::string&, const Bytes& c) { leaks += has(c, "TOPSECRET"); };
    auto db = open_ok(&v, "db", enc_opts("pw"));
    CHECK(db->put("n", B("k"), B("TOPSECRET" + std::string(2000, 'x'))) == Status::Ok);
    mem.set_capacity(mem.used_bytes() + fmt::kDataStart + 300);
    CHECK(db->rekey(nullptr, ByteView()) == Status::NoSpace);  // decrypting rekey dies mid tmp file
    CHECK(leaks == 0);
    mem.set_capacity(UINT64_MAX);
    Bytes val;
    CHECK(db->get("n", B("k"), &val) == Status::Ok);
  }
}

#ifdef EMBER_STORE_POSIX_LINKS
std::string sec_root() {
  const char* t = std::getenv("TMPDIR");
  std::string r = t && *t ? t : "/tmp";
  return path_join(r, "ember_store_sec_test");
}

TEST_CASE("security: scrubbing never follows symlinks or hard links") {
  auto vfs = new_os_vfs();
  std::string root = sec_root();
  std::string dir = path_join(root, "db");
  std::string victim = path_join(root, "victim.txt"), victim2 = path_join(root, "victim2.txt");
  KvStore::wipe(*vfs, dir);
  ::unlink(victim.c_str());
  ::unlink(victim2.c_str());
  REQUIRE(vfs->mkdir(root) == Status::Ok);
  const std::string secret = "precious data outside the store";
  auto write_victim = [&](const std::string& p) {
    std::unique_ptr<VfsFile> f;
    REQUIRE(vfs->open(p, OpenMode::CreateTruncate, &f) == Status::Ok);
    REQUIRE(f->write_at(0, B(secret)) == Status::Ok);
  };
  auto victim_ok = [&](const std::string& p) {
    Bytes b;
    return read_file(*vfs, p, &b, 1 << 20) == Status::Ok && S(b) == secret;
  };
  write_victim(victim);
  write_victim(victim2);
  {
    auto db = open_ok(vfs.get(), dir, plain_opts());
    CHECK(db->put("n", B("k"), B("v")) == Status::Ok);
  }
  REQUIRE(::symlink(victim.c_str(), path_join(dir, "tmp-50.ekv").c_str()) == 0);
  REQUIRE(::link(victim2.c_str(), path_join(dir, "tmp-51.ekv").c_str()) == 0);
  REQUIRE(::symlink(victim.c_str(), path_join(dir, "data-0.ekv").c_str()) == 0);
  {
    RecoveryReport rep;
    auto db = open_ok(vfs.get(), dir, plain_opts(), &rep);  // removes leftover tmp files
    CHECK(victim_ok(victim));
    CHECK(victim_ok(victim2));
    CHECK(db->compact() == Status::Ok);
  }
  REQUIRE(::symlink(victim.c_str(), path_join(dir, "zz-link").c_str()) == 0);
  REQUIRE(::link(victim2.c_str(), path_join(dir, "zz-hard").c_str()) == 0);
  KvStore::wipe(*vfs, dir);
  CHECK(victim_ok(victim));
  CHECK(victim_ok(victim2));
  std::vector<std::string> names;
  CHECK(vfs->list(dir, &names) == Status::NotFound);
  // The OS VFS refuses to open symlinks at all, and hard-linked files for writing.
  REQUIRE(vfs->mkdir(dir) == Status::Ok);
  REQUIRE(::symlink(victim.c_str(), path_join(dir, "l").c_str()) == 0);
  REQUIRE(::link(victim2.c_str(), path_join(dir, "h").c_str()) == 0);
  std::unique_ptr<VfsFile> f;
  CHECK(vfs->open(path_join(dir, "l"), OpenMode::ReadOnly, &f) == Status::InvalidArgument);
  CHECK(vfs->open(path_join(dir, "l"), OpenMode::CreateTruncate, &f) == Status::InvalidArgument);
  CHECK(vfs->open(path_join(dir, "h"), OpenMode::ReadWrite, &f) == Status::InvalidArgument);
  CHECK(vfs->open(path_join(dir, "h"), OpenMode::ReadOnly, &f) == Status::Ok);
  f.reset();
  CHECK(victim_ok(victim));
  CHECK(victim_ok(victim2));
  KvStore::wipe(*vfs, dir);
  ::unlink(victim.c_str());
  ::unlink(victim2.c_str());
  vfs->remove(root);
}
#endif

}  // TEST_SUITE
