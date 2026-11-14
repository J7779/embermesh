// Store VFS: MemVfs fault semantics, the OS VFS on a real temp directory, format helpers.
#include <cstdlib>

#include "ember/crc.h"
#include "store/kv_format.h"
#include "store_test_util.h"

using namespace store_test;

namespace {

std::unique_ptr<VfsFile> open_file(Vfs& vfs, const std::string& p, OpenMode m) {
  std::unique_ptr<VfsFile> f;
  Status s = vfs.open(p, m, &f);
  REQUIRE_MESSAGE(s == Status::Ok, status_name(s));
  return f;
}

Bytes read_all(VfsFile& f) {
  uint64_t n = 0;
  REQUIRE(f.size(&n) == Status::Ok);
  Bytes b(static_cast<size_t>(n));
  REQUIRE(read_exact(f, 0, b.data(), b.size()) == Status::Ok);
  return b;
}

// Exercises the generic Vfs contract; used for both MemVfs and the OS VFS.
void vfs_contract(Vfs& vfs, const std::string& dir) {
  REQUIRE(vfs.mkdir(dir) == Status::Ok);
  REQUIRE(vfs.mkdir(dir) == Status::Ok);  // idempotent
  std::vector<std::string> names;
  REQUIRE(vfs.list(dir, &names) == Status::Ok);
  CHECK(names.empty());
  std::string a = path_join(dir, "a.bin"), b = path_join(dir, "b.bin");
  std::unique_ptr<VfsFile> f;
  CHECK(vfs.open(a, OpenMode::ReadOnly, &f) == Status::NotFound);
  CHECK(vfs.open(a, OpenMode::ReadWrite, &f) == Status::NotFound);
  uint64_t sz = 0;
  CHECK(vfs.file_size(a, &sz) == Status::NotFound);
  {
    auto w = open_file(vfs, a, OpenMode::CreateReadWrite);
    CHECK(w->write_at(0, B("hello")) == Status::Ok);
    CHECK(w->append(B(" world")) == Status::Ok);
    CHECK(w->write_at(20, B("!")) == Status::Ok);  // gap is zero-filled
    CHECK(w->flush() == Status::Ok);
    CHECK(w->sync() == Status::Ok);
    CHECK(w->size(&sz) == Status::Ok);
    CHECK(sz == 21);
    Bytes all = read_all(*w);
    CHECK(S(Bytes(all.begin(), all.begin() + 11)) == "hello world");
    CHECK(all[15] == 0);
    CHECK(all[20] == '!');
    uint8_t buf[8];
    size_t got = 99;
    CHECK(w->read_at(100, buf, 8, &got) == Status::Ok);
    CHECK(got == 0);  // EOF
    CHECK(read_exact(*w, 18, buf, 8) == Status::Corruption);  // short file
    CHECK(w->truncate(5) == Status::Ok);
    CHECK(w->size(&sz) == Status::Ok);
    CHECK(sz == 5);
    CHECK(w->write_at(0, B("HE")) == Status::Ok);
    CHECK(S(read_all(*w)) == "HEllo");
  }
  CHECK(vfs.file_size(a, &sz) == Status::Ok);
  CHECK(sz == 5);
  {
    auto w = open_file(vfs, b, OpenMode::CreateTruncate);
    CHECK(w->append(B("bbb")) == Status::Ok);
    CHECK(w->sync() == Status::Ok);
  }
  {
    auto w = open_file(vfs, b, OpenMode::CreateReadWrite);  // keeps content
    CHECK(S(read_all(*w)) == "bbb");
  }
  CHECK(vfs.rename(a, b) == Status::Ok);  // atomic replace
  CHECK(vfs.sync_dir(dir) == Status::Ok);
  CHECK(vfs.file_size(a, &sz) == Status::NotFound);
  {
    auto r = open_file(vfs, b, OpenMode::ReadOnly);
    CHECK(S(read_all(*r)) == "HEllo");
  }
  {
    auto w = open_file(vfs, b, OpenMode::CreateTruncate);
    CHECK(w->size(&sz) == Status::Ok);
    CHECK(sz == 0);
  }
  REQUIRE(vfs.list(dir, &names) == Status::Ok);
  CHECK(names == std::vector<std::string>{"b.bin"});
  CHECK(vfs.rename(a, b) == Status::NotFound);
  CHECK(vfs.remove(b) == Status::Ok);
  CHECK(vfs.remove(b) == Status::NotFound);
  CHECK(vfs.remove(dir) == Status::Ok);  // empty directory
  CHECK(vfs.list(dir, &names) == Status::NotFound);
}

std::string temp_root() {
  const char* t = std::getenv("TMPDIR");
#ifdef _WIN32
  if (!t) t = std::getenv("TEMP");
#endif
  std::string r = t && *t ? t : "/tmp";
  return path_join(r, "ember_store_test");
}

}  // namespace

TEST_SUITE("store") {

TEST_CASE("vfs: helpers and format codecs") {
  CHECK(path_join("a", "b") == "a/b");
  CHECK(path_join("a/", "b") == "a/b");
  CHECK(path_join("", "b") == "b");
  CHECK(std::string(status_name(Status::NoSpace)) == "no_space");
  CHECK(std::string(status_name(Status::Broken)) == "broken");
  // Table CRC is bit-identical to the shared bitwise one.
  Rng r(3);
  for (size_t n : {0, 1, 7, 64, 1000}) {
    Bytes b = r.bytes(n);
    CHECK(fmt::crc32(ByteView(b)) == ember::crc32(ByteView(b)));
    CHECK(fmt::crc32(ByteView(b), 0x1234) == ember::crc32(ByteView(b), 0x1234));
  }
  CHECK(fmt::crc32(B("123456789")) == 0xCBF43926u);
  char kind;
  uint64_t g;
  CHECK(fmt::parse_name("data-1.ekv", &kind, &g));
  CHECK(kind == 'd');
  CHECK(g == 1);
  CHECK(fmt::parse_name("tmp-18446744073709551615.ekv", &kind, &g));
  CHECK(g == UINT64_MAX);
  CHECK(kind == 't');
  CHECK(fmt::parse_name(fmt::data_name(1234567), &kind, &g));
  CHECK(g == 1234567);
  for (const char* bad : {"data-.ekv", "data-01.ekv", "data-18446744073709551616.ekv", "data-1.ek",
                          "data-1a.ekv", "data--1.ekv", "xdata-1.ekv", "data-1.ekv.bak", "", "tmp-"})
    CHECK_FALSE(fmt::parse_name(bad, &kind, &g));
  // Header round trip and slot limits.
  fmt::Header h;
  h.generation = 9;
  h.base_seq = 77;
  h.flags = 1;
  h.params = Bytes(256, 7);
  h.keycheck = Bytes(128, 9);
  Bytes e = fmt::encode_header(h);
  CHECK(e.size() == fmt::kHeaderSize);
  fmt::Header d;
  CHECK(fmt::decode_header(ByteView(e), &d));
  CHECK(d.generation == 9);
  CHECK(d.base_seq == 77);
  CHECK(d.params == h.params);
  CHECK(d.keycheck == h.keycheck);
  for (size_t i = 0; i < e.size(); i += 13) {
    Bytes x = e;
    x[i] ^= 0x10;
    CHECK_FALSE(fmt::decode_header(ByteView(x), &d));
  }
  CHECK_FALSE(fmt::decode_header(ByteView(e).sub(0, 100), &d));
}

TEST_CASE("vfs: MemVfs satisfies the VFS contract") {
  MemVfs vfs;
  vfs_contract(vfs, "d");
  CHECK(vfs.mkdir("x/y") == Status::NotFound);  // parent missing
}

TEST_CASE("vfs: OS VFS satisfies the VFS contract and hosts a store") {
  auto vfs = new_os_vfs();
  std::string root = temp_root();
  KvStore::wipe(*vfs, path_join(root, "kv"));  // leftovers from an aborted run
  KvStore::wipe(*vfs, path_join(root, "contract"));
  REQUIRE(vfs->mkdir(root) == Status::Ok);
  vfs_contract(*vfs, path_join(root, "contract"));
  std::string dir = path_join(root, "kv");
  {
    auto db = open_ok(vfs.get(), dir, enc_opts("pw"));
    for (int i = 0; i < 200; ++i) CHECK(db->put("n", B("k" + std::to_string(i)), B("v" + std::to_string(i))) == Status::Ok);
    CHECK(db->compact() == Status::Ok);
    CHECK(db->put("n", B("late"), B("x")) == Status::Ok);
  }
  {
    RecoveryReport rep;
    auto db = open_ok(vfs.get(), dir, enc_opts("pw", 2), &rep);
    CHECK(rep.clean());
    CHECK(db->count("n") == 201);
    Bytes v;
    CHECK(db->get("n", B("k150"), &v) == Status::Ok);
    CHECK(S(v) == "v150");
    std::vector<std::string> names;
    CHECK(vfs->list(dir, &names) == Status::Ok);
    CHECK(names == std::vector<std::string>{"data-2.ekv"});
    CHECK(db->wipe() == Status::Ok);
  }
  std::vector<std::string> names;
  CHECK(vfs->list(dir, &names) == Status::NotFound);
  std::unique_ptr<VfsFile> f;
  CHECK(vfs->open(root, OpenMode::ReadOnly, &f) == Status::InvalidArgument);  // directory
  CHECK(vfs->remove(root) == Status::Ok);
}

TEST_CASE("vfs: MemVfs durability model") {
  MemVfs vfs;
  REQUIRE(vfs.mkdir("d") == Status::Ok);
  {
    auto f = open_file(vfs, "d/f", OpenMode::CreateReadWrite);
    CHECK(f->append(B("synced")) == Status::Ok);
    CHECK(f->sync() == Status::Ok);
    CHECK(f->append(B("-unsynced")) == Status::Ok);
  }
  // File created but directory entry never synced: lost on DropUnsynced.
  vfs.reboot(CrashMode::DropUnsynced);
  CHECK_FALSE(vfs.exists("d/f"));
  {
    auto f = open_file(vfs, "d/f", OpenMode::CreateReadWrite);
    CHECK(f->append(B("synced")) == Status::Ok);
    CHECK(f->sync() == Status::Ok);
    CHECK(vfs.sync_dir("d") == Status::Ok);
    CHECK(f->append(B("-unsynced")) == Status::Ok);
    CHECK(f->write_at(0, B("S")) == Status::Ok);  // overwrite of synced data, unsynced
  }
  vfs.reboot(CrashMode::DropUnsynced);
  CHECK(S(vfs.contents("d/f")) == "synced");
  {
    auto f = open_file(vfs, "d/f", OpenMode::ReadWrite);
    CHECK(f->truncate(2) == Status::Ok);  // unsynced truncate
    CHECK(vfs.rename("d/f", "d/g") == Status::Ok);  // unsynced rename
  }
  vfs.reboot(CrashMode::DropUnsynced);
  CHECK(S(vfs.contents("d/f")) == "synced");
  CHECK_FALSE(vfs.exists("d/g"));
  {
    auto f = open_file(vfs, "d/f", OpenMode::ReadWrite);
    CHECK(f->append(B("+more")) == Status::Ok);
    CHECK(vfs.rename("d/f", "d/g") == Status::Ok);
  }
  vfs.reboot(CrashMode::KeepAll);
  CHECK(S(vfs.contents("d/g")) == "synced+more");
  // Random: synced data always kept; result is synced + prefix of unsynced (+ maybe junk).
  for (uint64_t seed = 0; seed < 50; ++seed) {
    MemVfs v;
    v.mkdir("d");
    {
      auto f = open_file(v, "d/f", OpenMode::CreateReadWrite);
      f->append(B("base"));
      f->sync();
      v.sync_dir("d");
      f->append(B("0123456789"));
    }
    v.reboot(CrashMode::Random, seed);
    std::string c = S(v.contents("d/f"));
    REQUIRE(c.size() >= 4);
    CHECK(c.substr(0, 4) == "base");
  }
  // Handles from before a reboot are dead.
  std::unique_ptr<VfsFile> stale = open_file(vfs, "d/g", OpenMode::ReadWrite);
  vfs.reboot(CrashMode::KeepAll);
  CHECK(stale->append(B("x")) == Status::IoError);
}

TEST_CASE("vfs: MemVfs fault injection") {
  MemVfs vfs;
  vfs.mkdir("d");
  auto f = open_file(vfs, "d/f", OpenMode::CreateReadWrite);
  // Torn write at an exact byte.
  vfs.crash_after_bytes(3);
  CHECK(f->append(B("abcdef")) == Status::IoError);
  CHECK(vfs.crashed());
  CHECK(f->append(B("x")) == Status::IoError);
  uint8_t buf[4];
  size_t got;
  CHECK(f->read_at(0, buf, 4, &got) == Status::IoError);
  f.reset();
  vfs.reboot(CrashMode::KeepAll);
  CHECK(S(vfs.contents("d/f")) == "abc");
  // Crash at the n-th op: ops before succeed, that op does not happen.
  f = open_file(vfs, "d/f", OpenMode::ReadWrite);
  uint64_t before = vfs.op_count();
  vfs.crash_after_ops(2);
  CHECK(f->append(B("1")) == Status::Ok);
  CHECK(f->append(B("2")) == Status::Ok);
  CHECK(f->append(B("3")) == Status::IoError);
  CHECK(vfs.op_count() - before == 2);
  f.reset();
  vfs.reboot(CrashMode::KeepAll);
  CHECK(S(vfs.contents("d/f")) == "abc12");
  // Disk full writes what fits.
  vfs.set_capacity(vfs.used_bytes() + 4);
  f = open_file(vfs, "d/f", OpenMode::ReadWrite);
  CHECK(f->append(B("123456")) == Status::NoSpace);
  CHECK(S(vfs.contents("d/f")) == "abc121234");
  CHECK(f->write_at(0, B("ABC")) == Status::Ok);  // overwrite needs no space
  CHECK(f->truncate(3) == Status::Ok);
  CHECK(f->append(B("xyz")) == Status::Ok);
  vfs.set_capacity(UINT64_MAX);
  // Short reads, read failures, sync failures, bit flips.
  vfs.set_short_reads(1);
  CHECK(f->read_at(0, buf, 4, &got) == Status::Ok);
  CHECK(got == 1);
  Bytes all = read_all(*f);
  CHECK(S(all) == "ABCxyz");
  vfs.set_short_reads(0);
  vfs.set_fail_reads(true);
  CHECK(f->read_at(0, buf, 4, &got) == Status::IoError);
  vfs.set_fail_reads(false);
  vfs.set_fail_sync(true);
  CHECK(f->sync() == Status::IoError);
  CHECK(vfs.sync_dir("d") == Status::IoError);
  vfs.set_fail_sync(false);
  CHECK(vfs.flip_bit("d/f", 0));
  CHECK(vfs.contents("d/f")[0] == ('A' ^ 1));
  CHECK_FALSE(vfs.corrupt("d/f", 100, 1));
  CHECK_FALSE(vfs.corrupt("nope", 0, 1));
}

}  // TEST_SUITE
