// EmberMesh store — in-memory, deterministic, fault-injecting VFS.
//
// Models a disk with a volatile cache, strictly following the contract in store/vfs.h:
//   * each file has its *visible* bytes and its *durable* bytes; sync() makes visible durable;
//   * the directory has a visible and a durable name table; sync_dir() makes it durable;
//   * crash()/reboot() throw away whatever the chosen CrashMode says did not reach the platter.
//
// Fault injection (all deterministic; nothing reads the clock or OS randomness):
//   * crash_after_ops(n)   — the n-th mutating operation (0-based) and everything after it fails
//                            with IoError ("power cut"); the operation itself does not happen.
//                            Mutating ops: write_at/append, sync, truncate, rename, remove,
//                            sync_dir, mkdir, open that creates or truncates a file.
//   * crash_after_bytes(n) — power cut once n more bytes have been written in total: the write in
//                            flight is torn at that exact byte (its first bytes land, the rest do not).
//   * set_capacity(bytes)  — disk full: a write that would grow the total over capacity writes what
//                            fits, then fails with NoSpace (like a real ENOSPC mid-write).
//   * set_short_reads(k)   — every read returns at most k bytes.
//   * set_fail_reads/set_fail_sync — reads / syncs fail with IoError.
//   * corrupt(path, off, xor) / flip_bit — bit rot, applied to both visible and durable bytes.
//
// After a crash, destroy every object that holds files from this VFS (the store), call reboot(),
// then reopen. Handles opened before reboot() fail with IoError.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "store/vfs.h"

namespace ember::store {

enum class CrashMode : uint8_t {
  DropUnsynced,  // only synced data and synced directory entries survive (strict POSIX worst case)
  KeepAll,       // everything written survived (lucky: the cache got flushed)
  Random,        // per file: synced data + a random-length prefix of the unsynced appended bytes,
                 // sometimes followed by a few junk/zero bytes (torn tail); the directory table
                 // is either all-visible or all-durable. Seeded by reboot()'s seed.
};

class MemVfs : public Vfs {
 public:
  MemVfs();
  ~MemVfs() override;
  MemVfs(const MemVfs&) = delete;
  MemVfs& operator=(const MemVfs&) = delete;

  // --- Vfs ---------------------------------------------------------------------------------------
  Status open(const std::string& path, OpenMode mode, std::unique_ptr<VfsFile>* out) override;
  Status rename(const std::string& from, const std::string& to) override;
  Status remove(const std::string& path) override;
  Status list(const std::string& dir, std::vector<std::string>* names) override;
  Status file_size(const std::string& path, uint64_t* out) override;
  Status mkdir(const std::string& path) override;
  Status sync_dir(const std::string& dir) override;

  // --- fault injection ---------------------------------------------------------------------------
  // n < 0 disables. Counted from the moment of the call.
  void crash_after_ops(int64_t n);
  void crash_after_bytes(int64_t n);
  void set_capacity(uint64_t bytes) { capacity_ = bytes; }  // UINT64_MAX = unlimited
  void set_short_reads(size_t max_per_read) { short_read_max_ = max_per_read; }  // 0 = off
  void set_fail_reads(bool on) { fail_reads_ = on; }
  void set_fail_sync(bool on) { fail_sync_ = on; }
  // Immediately enters the crashed state (every later op fails until reboot()).
  void crash() { crashed_ = true; }
  bool crashed() const { return crashed_; }

  // Applies `mode` to decide what survived, clears the crashed state and all crash triggers,
  // and invalidates open handles. Capacity / short-read / fail flags are kept.
  void reboot(CrashMode mode, uint64_t seed = 0);

  // --- inspection / corruption (bypass faults; do not count as ops) ------------------------------
  uint64_t op_count() const { return ops_; }            // mutating ops so far
  uint64_t bytes_written() const { return written_; }   // bytes written so far
  uint64_t used_bytes() const;                          // sum of visible file sizes
  bool exists(const std::string& path) const;
  // Visible content of a file (empty if missing).
  Bytes contents(const std::string& path) const;
  // Replaces a file's content (visible and durable); creates it durably if missing.
  void set_contents(const std::string& path, ByteView data);
  // XORs `mask` into the byte at `offset` (visible and durable). False if out of range.
  bool corrupt(const std::string& path, uint64_t offset, uint8_t mask);
  bool flip_bit(const std::string& path, uint64_t bit_index) {
    return corrupt(path, bit_index / 8, static_cast<uint8_t>(1u << (bit_index % 8)));
  }
  // Every visible file path, sorted.
  std::vector<std::string> all_files() const;

 private:
  friend class MemFile;
  struct Inode {
    Bytes data;           // visible content
    // Durable content == data[0, clean_len) + saved_tail. Appends past clean_len cost nothing;
    // overwrites/truncates below clean_len move the old durable bytes into saved_tail.
    size_t clean_len = 0;
    Bytes saved_tail;
    void preserve_from(size_t off);
    void make_durable() { saved_tail.clear(); clean_len = data.size(); }
    Bytes durable() const;
  };

  bool begin_op();  // false (and maybe enters crashed state) if the op must fail
  Status do_write(uint32_t ino, uint64_t off, ByteView data);
  Status do_read(uint32_t ino, uint64_t off, uint8_t* dst, size_t n, size_t* got);
  Status do_sync(uint32_t ino);
  Status do_truncate(uint32_t ino, uint64_t size);
  Inode* inode(uint32_t ino);
  static std::string parent_of(const std::string& path);

  std::map<uint32_t, Inode> inodes_;
  std::map<std::string, uint32_t> names_;          // visible directory table
  std::map<std::string, uint32_t> durable_names_;  // what survives DropUnsynced
  std::set<std::string> dirs_;                     // directories (always durable)
  uint32_t next_ino_ = 1;
  uint64_t epoch_ = 0;

  uint64_t ops_ = 0, written_ = 0;
  int64_t crash_ops_budget_ = -1, crash_bytes_budget_ = -1;
  uint64_t capacity_ = UINT64_MAX;
  size_t short_read_max_ = 0;
  bool fail_reads_ = false, fail_sync_ = false, crashed_ = false;
};

}  // namespace ember::store
