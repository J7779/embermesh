// Shared helpers for the store tests.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "doctest.h"
#include "store/kv_store.h"
#include "store/mem_vfs.h"
#include "store/test_cipher.h"

namespace store_test {

using namespace ember;
using namespace ember::store;

inline ByteView B(const std::string& s) { return ByteView(std::string_view(s)); }
inline std::string S(const Bytes& b) { return std::string(b.begin(), b.end()); }

// Deterministic PRNG (splitmix64).
struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed) {}
  uint64_t next() {
    uint64_t z = (s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }
  uint64_t below(uint64_t n) { return n ? next() % n : 0; }
  Bytes bytes(size_t n) {
    Bytes b(n);
    for (auto& x : b) x = static_cast<uint8_t>(next());
    return b;
  }
};

// Seeded entropy source for Options::entropy.
inline std::function<void(uint8_t*, size_t)> test_entropy(uint64_t seed) {
  auto rng = std::make_shared<Rng>(seed);
  return [rng](uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; ++i) p[i] = static_cast<uint8_t>(rng->next());
  };
}

inline Options plain_opts() {
  Options o;
  o.auto_compact_min_bytes = 0;
  return o;
}

inline Options enc_opts(const std::string& key, uint64_t seed = 1) {
  Options o;
  o.auto_compact_min_bytes = 0;
  o.cipher.reset(new InsecureTestCipher(key));
  o.cipher_params = Bytes(key.begin(), key.end());
  o.entropy = test_entropy(seed);
  return o;
}

inline std::unique_ptr<KvStore> open_ok(Vfs* vfs, const std::string& dir, Options o,
                                        RecoveryReport* rep = nullptr) {
  std::unique_ptr<KvStore> db;
  Status s = KvStore::open(vfs, dir, std::move(o), &db, rep);
  REQUIRE_MESSAGE(s == Status::Ok, status_name(s));
  return db;
}

using Model = std::map<std::string, std::map<std::string, std::string>>;  // ns -> key -> value

// Full logical content of the store, read through scans.
inline Model dump(KvStore& db) {
  Model m;
  for (const auto& ns : db.namespaces()) {
    std::vector<KvEntry> v;
    Status s = db.scan(ns, ScanSpec(), &v);
    CHECK(s == Status::Ok);
    for (auto& e : v) m[ns][S(e.key)] = S(e.value);
  }
  return m;
}

inline std::string be64(uint64_t v) {
  std::string s(8, '\0');
  for (int i = 0; i < 8; ++i) s[i] = static_cast<char>(v >> (56 - 8 * i));
  return s;
}

// Vfs decorator that counts read calls/bytes and lets a test inspect a file's last content just
// before it is removed (residue checks). Forwards everything to `inner`.
class CountingVfs : public Vfs {
 public:
  explicit CountingVfs(MemVfs* inner) : in_(inner) {}
  uint64_t reads = 0, read_bytes = 0;
  std::function<void(const std::string& path, const Bytes& content)> on_remove;

  Status open(const std::string& path, OpenMode mode, std::unique_ptr<VfsFile>* out) override {
    std::unique_ptr<VfsFile> f;
    Status s = in_->open(path, mode, &f);
    if (s == Status::Ok) out->reset(new File(this, std::move(f)));
    return s;
  }
  Status rename(const std::string& a, const std::string& b) override { return in_->rename(a, b); }
  Status remove(const std::string& p) override {
    if (on_remove && in_->exists(p)) on_remove(p, in_->contents(p));
    return in_->remove(p);
  }
  Status list(const std::string& d, std::vector<std::string>* n) override { return in_->list(d, n); }
  Status file_size(const std::string& p, uint64_t* o) override { return in_->file_size(p, o); }
  Status mkdir(const std::string& p) override { return in_->mkdir(p); }
  Status sync_dir(const std::string& d) override { return in_->sync_dir(d); }

 private:
  struct File : VfsFile {
    File(CountingVfs* v, std::unique_ptr<VfsFile> f) : v_(v), f_(std::move(f)) {}
    Status read_at(uint64_t off, uint8_t* dst, size_t n, size_t* got) override {
      Status s = f_->read_at(off, dst, n, got);
      ++v_->reads;
      v_->read_bytes += *got;
      return s;
    }
    Status write_at(uint64_t off, ByteView d) override { return f_->write_at(off, d); }
    Status append(ByteView d) override { return f_->append(d); }
    Status flush() override { return f_->flush(); }
    Status sync() override { return f_->sync(); }
    Status size(uint64_t* o) override { return f_->size(o); }
    Status truncate(uint64_t n) override { return f_->truncate(n); }
    CountingVfs* v_;
    std::unique_ptr<VfsFile> f_;
  };
  MemVfs* in_;
};

}  // namespace store_test
