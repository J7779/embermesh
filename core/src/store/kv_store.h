// EmberMesh store — crash-safe, optionally encrypted, namespaced ordered key/value store.
//
// This is the persistence layer for the message database, contacts, settings, DTN store and
// identity storage. It is deliberately simple and boringly robust:
//
//   * Namespaces (short strings, e.g. "msg", "contacts") each hold an ordered map of binary keys
//     to binary values, ordered by unsigned lexicographic byte comparison (memcmp order). Encode
//     numbers big-endian in keys so they sort numerically (e.g. 8-byte BE timestamp + id).
//   * Writes are batches: every op in a batch becomes visible and durable together or not at all.
//     Each op gets a sequence number; sequence numbers strictly increase across batches,
//     restarts and compactions (but a batch lost in a crash may have its numbers reused).
//   * The index (keys + 40-byte locators) lives in RAM; values stay on disk and are read on demand.
//     Rough RAM cost per record: ~90 bytes + key length (std::map node + std::string + locator),
//     so 300k records with 24-byte keys ≈ 35 MB on a 64-bit phone, less on 32-bit.
//
// ON-DISK FORMAT (one active file per store directory: "<dir>/data-<generation>.ekv")
//   [0..512)    file header, copy A  ┐ magic "EMBERKV\0", version (1 plaintext, 2 encrypted; both read), flags (encrypted), generation,
//   [512..1024) file header, copy B  ┘ base sequence, 16-byte salt, cipher params, key check, CRC32
//   [1024..)    frames, appended:
//       u8 'E',0x4B | u8 type | u8 flags | u32le payload_len | u64le seq | u32le header_crc
//       payload | u32le frame_crc            (CRCs are CRC-32 seeded from the file salt)
//     type 2 = snapshot chunk (records written by compaction), type 3 = snapshot commit,
//     type 1 = batch (one WriteBatch). Encrypted payloads are nonce(24) || AEAD ciphertext.
//   Compaction / re-key writes "tmp-<g+1>.ekv" (snapshot of live data), syncs it, renames it to
//   "data-<g+1>.ekv", syncs the directory, then removes the old generation. Recovery opens the
//   highest generation with a valid header; leftover tmp files and older generations are removed.
//
// GUARANTEES
//   * A batch written with WriteOptions::sync (default), or followed by a successful sync() /
//     compact() / rekey(), survives any crash. A crash at any byte boundary recovers to a prefix
//     of the batch sequence: never a partial batch.
//   * A torn tail (incomplete last frame) is normal after a crash; it is discarded and reported in
//     RecoveryReport::torn_tail_bytes. Mid-file damage is detected by CRC (and AEAD when
//     encrypted); with OnCorruption::Salvage (default) every frame that still verifies is kept, the
//     damaged ranges are reported, and the store opens. Corruption of the very last frame is
//     indistinguishable from a torn write and is reported as a torn tail.
//   * Reads never return unverified bytes: plaintext values carry a CRC-32 checked on every read;
//     encrypted values are authenticated on every read. A failed check returns Status::Corruption.
//   * Hostile files never crash the store: every length is bounded by the file size and by hard
//     caps before allocation.
//
// LIMITS / CAVEATS
//   * Single process, single thread. Two KvStore instances on one directory corrupt each other.
//   * Space: compaction needs free space for a full copy of the live data; deleting needs a few
//     bytes of log space. On NoSpace the store stays usable (the failed batch is rolled back).
//   * Any I/O failure that leaves the file state unknown (failed sync, failed rollback, failed
//     directory sync after a compaction rename) puts the store in Status::Broken: reads keep
//     working from the index, writes fail; close and reopen to recover.
//   * Salvage after mid-file corruption can resurrect a state that never existed as a whole (e.g. a
//     later batch survives while an earlier one was lost); every surviving value was genuinely
//     written, but callers needing cross-record invariants should check RecoveryReport::clean().
//   * Recovery work is bounded: open() reads at most ~4 x file size + 1 MiB while hunting for
//     frames (forged frame headers cannot make it quadratic); past that bound the rest of the file
//     is treated as corrupt (RecoveryReport::work_limit_hit).
//   * Downgrade protection: with a cipher only encrypted generations are ever opened; without one,
//     any encrypted generation makes open() return Locked and probe() report encrypted (plus
//     mixed_encryption when plaintext generations coexist). Nothing encrypted is deleted in favour
//     of a plaintext file. An attacker who can delete files can still delete the whole encrypted
//     store: only the caller can know that it should exist — pass Options::require_encryption.
//   * Snapshot chunks of encrypted stores (format v2) are bound to their ordinal and the commit to
//     the whole chunk sequence; plaintext stores have CRCs only and no integrity against an
//     attacker who can write the files (by design: encryption is the integrity mechanism).
//   * Cleartext residue: deletes are logical until the next compaction. Compaction always
//     overwrites a *plaintext* old generation before unlinking it (an encrypted one only with
//     scrub_on_compact; re-key always does), abandoned tmp files are overwritten too. What the
//     file system makes unavoidable remains: copy-on-write / log-structured file systems and flash
//     translation layers (APFS, F2FS, SSD/eMMC wear levelling, snapshots, backups) may keep old
//     blocks that an overwrite cannot reach. Real protection is encryption + forgetting the key.
//   * The cipher params blob is opaque and only CRC-protected: whoever parses it (the node's KDF
//     parameters) must cap the cost it may demand before deriving a key.
//   * The OS VFS never opens symlinks, nor hard-linked files for writing, so truncation, scrub and
//     wipe() cannot be redirected to files outside the store directory.
//   * Encrypted stores leak: number and sizes of batch frames (≈ per-write sizes), sequence
//     numbers (write counts), generation number, total size, and the cipher params blob (e.g. the
//     KDF salt) — never key, value or namespace bytes. Truncation of the newest frames by an
//     attacker (rollback) is indistinguishable from a crash.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ember/bytes.h"
#include "store/cipher.h"
#include "store/status.h"
#include "store/vfs.h"

namespace ember::store {

// Hard limits (independent of Options; Options may only lower the value/batch limits).
constexpr size_t kMaxNamespaceLen = 255;
constexpr size_t kMaxKeyLen = 65535;
constexpr size_t kMaxValueLen = size_t(64) << 20;       // 64 MiB
constexpr size_t kMaxBatchBytes = size_t(128) << 20;    // 128 MiB encoded
constexpr size_t kMaxCipherParams = 256;

// A group of writes applied atomically, in order. Cheap to build; reuse with clear().
class WriteBatch {
 public:
  // Inserts or overwrites key in namespace ns.
  void put(std::string_view ns, ByteView key, ByteView value);
  // Removes key (no-op if absent).
  void del(std::string_view ns, ByteView key);
  // Removes every key k with lower <= k < upper.
  void delete_range(std::string_view ns, ByteView lower, ByteView upper);
  // Removes every key k >= lower.
  void delete_from(std::string_view ns, ByteView lower);
  // Removes every key of the namespace.
  void drop_namespace(std::string_view ns);

  void clear();
  size_t count() const { return count_; }
  bool empty() const { return count_ == 0; }
  // Encoded size; KvStore rejects batches over Options::max_batch_bytes.
  size_t byte_size() const { return rep_.size(); }

 private:
  friend class KvStore;
  void header(uint8_t op, std::string_view ns, ByteView key);
  Bytes rep_;
  uint32_t count_ = 0;
  size_t max_value_ = 0;
  bool bad_name_ = false;  // namespace or key over the hard limit
};

struct WriteOptions {
  // true: the batch is fsync'ed before write() returns (durable on Ok).
  // false: written to the OS only; durable after the next successful sync()/compact()/close().
  bool sync = true;
};

// Key range for scans. Bounds are on raw keys: lower inclusive, upper exclusive. The prefix (if
// non-empty) further restricts to keys starting with it. Forward scans yield ascending keys
// starting at the low end; reverse scans yield descending keys starting at the high end.
// "Latest 50 messages before timestamp X":  ScanSpec s; s.set_upper(be64(X)); s.reverse = true;
//                                            s.limit = 50;
struct ScanSpec {
  Bytes lower;             // empty = from the first key
  Bytes upper;             // used only if has_upper
  bool has_upper = false;
  Bytes prefix;
  bool reverse = false;
  size_t limit = 0;        // 0 = unlimited
  bool keys_only = false;  // visitor gets an empty value; no disk reads

  ScanSpec& set_lower(ByteView k) { lower = k.to_bytes(); return *this; }
  ScanSpec& set_upper(ByteView k) { upper = k.to_bytes(); has_upper = true; return *this; }
  ScanSpec& set_prefix(ByteView p) { prefix = p.to_bytes(); return *this; }
  static ScanSpec all() { return ScanSpec(); }
  static ScanSpec with_prefix(ByteView p) { ScanSpec s; s.prefix = p.to_bytes(); return s; }
};

struct KvEntry {
  Bytes key;
  Bytes value;
  uint64_t seq = 0;
};

// Return false to stop the scan early. key/value views are valid only during the call.
// The visitor may call get()/scan(); it must not write (writes return Status::Busy).
using ScanVisitor = std::function<bool(ByteView key, ByteView value, uint64_t seq)>;

struct NamespaceStats {
  uint64_t records = 0;
  uint64_t key_bytes = 0;
  uint64_t value_bytes = 0;
  // Logical size used for quotas: keys + values.
  uint64_t bytes() const { return key_bytes + value_bytes; }
};

struct StoreStats {
  uint64_t generation = 0;
  uint64_t last_seq = 0;
  uint64_t records = 0;
  uint64_t namespaces = 0;
  uint64_t live_bytes = 0;     // keys + values + namespace names of live records
  uint64_t file_bytes = 0;     // size of the active data file
  uint64_t compactions = 0;    // since open (explicit + automatic + rekey)
  uint64_t auto_compactions = 0;
  uint64_t corrupt_reads = 0;  // get/scan reads that failed verification since open
  uint64_t dropped_by_compaction = 0;  // unreadable records dropped while compacting
  bool encrypted = false;
  bool read_only = false;
  bool broken = false;
};

// What open() found and did. "Lost" counts are best effort.
struct RecoveryReport {
  uint64_t generation = 0;
  bool created = false;               // a new empty store was created
  uint64_t frames_ok = 0;
  uint64_t batches_applied = 0;
  uint64_t snapshot_records = 0;
  uint64_t torn_tail_bytes = 0;       // incomplete tail discarded (normal after a crash)
  uint64_t corrupt_regions = 0;       // damaged byte ranges skipped in the middle of the file
  uint64_t corrupt_bytes = 0;
  uint64_t rejected_frames = 0;       // checksum-valid frames that failed decrypt/decode/sequence
  uint64_t lost_ops = 0;              // sequence numbers missing between surviving batches
  bool snapshot_incomplete = false;   // snapshot chunks missing (records from compaction lost)
  bool header_copy_damaged = false;   // one header copy failed its CRC (the other was used)
  uint32_t tmp_files_removed = 0;
  uint32_t old_generations_removed = 0;
  uint32_t old_generations_kept = 0;  // kept because the newest file was damaged
  // Generations with different encryption were found (e.g. a plaintext data file planted next to
  // an encrypted store, or an interrupted re-key). With a cipher, plaintext generations are never
  // used (and are scrubbed + removed after a clean open); without one, open() returns Locked as
  // soon as any encrypted generation exists. The node should surface this to the user.
  bool mixed_encryption = false;
  // Recovery hit its work bound (hostile file with many forged frame headers): the rest of the
  // file from the point reached is treated as corrupt (counted in corrupt_regions/corrupt_bytes).
  bool work_limit_hit = false;
  // True if nothing was lost except a torn tail.
  bool clean() const {
    return corrupt_regions == 0 && rejected_frames == 0 && lost_ops == 0 && !snapshot_incomplete;
  }
};

// What probe() can tell without any key.
struct ProbeInfo {
  bool exists = false;       // a store with a valid header is present
  bool encrypted = false;    // a cipher (passphrase) is needed to open it
  uint32_t format_version = 0;
  uint64_t generation = 0;
  Bytes cipher_params;       // opaque blob stored by Options::cipher_params / rekey (e.g. KDF salt)
  // Valid generations with and without encryption coexist. `encrypted` is then true and
  // cipher_params/generation describe the newest *encrypted* generation: a store is never reported
  // as plaintext while an encrypted generation is present (downgrade protection).
  bool mixed_encryption = false;
};

enum class OnCorruption : uint8_t {
  Salvage,  // keep every verifiable record, report the rest (default)
  Fail,     // open() returns Status::Corruption if anything but a torn tail was lost
};

struct WipeOptions {
  // false: only the headers/first 4 KiB of each file are overwritten before removal. For an
  // encrypted store this destroys the salt/params and key check (crypto-shred) in milliseconds.
  // true: then every byte of every file is overwritten too.
  bool full = true;
};

struct Options {
  bool create_if_missing = true;
  bool error_if_exists = false;
  bool read_only = false;  // no writes, no recovery side effects (torn tail kept, files kept)
  OnCorruption on_corruption = OnCorruption::Salvage;

  // Encryption at rest. Required to open an encrypted store; when creating a store with a cipher
  // it is encrypted from the first byte. `entropy` must also be set (nonces, salts).
  std::unique_ptr<RecordCipher> cipher;
  // Opaque public blob stored in the header when the store is created (e.g. Argon2 salt + cost).
  // Readable via probe() before the key is known. <= kMaxCipherParams bytes.
  Bytes cipher_params;
  // Fills buf with n random bytes (platform CSPRNG via the node). Required with a cipher; without
  // one it is optional (salts are then derived deterministically).
  std::function<void(uint8_t* buf, size_t n)> entropy;
  // The caller knows (from state kept outside the store, e.g. platform secure storage or app
  // config) that this store must be encrypted. Then open() returns Status::Tampered instead of
  // opening or creating a plaintext store, and never creates a fresh store without a cipher.
  bool require_encryption = false;

  size_t max_value_size = size_t(16) << 20;   // <= kMaxValueLen
  size_t max_batch_bytes = size_t(64) << 20;  // <= kMaxBatchBytes

  // Automatic compaction after a write when file_bytes >= auto_compact_min_bytes and
  // file_bytes > auto_compact_ratio * (live data + overhead). 0 disables.
  uint64_t auto_compact_min_bytes = uint64_t(4) << 20;
  uint32_t auto_compact_ratio = 2;
  // Overwrite old generation files before deleting them after compaction (always done by rekey).
  bool scrub_on_compact = false;
  // Target plaintext size of compaction snapshot chunks (a damaged chunk loses its records).
  size_t snapshot_chunk_bytes = 4096;
};

class KvStore {
 public:
  // Opens (or creates) the store in directory `dir`. `vfs` must outlive the store. On success
  // *out is set; *report (optional) describes recovery. Errors: NotFound (missing and
  // !create_if_missing, or read_only), Exists, Locked, WrongKey, InvalidArgument (bad options or
  // cipher given for a plaintext store — use rekey() to encrypt), Corruption (no usable header,
  // or OnCorruption::Fail), Tampered (require_encryption and no encrypted store), IoError/NoSpace.
  static Status open(Vfs* vfs, const std::string& dir, Options options,
                     std::unique_ptr<KvStore>* out, RecoveryReport* report = nullptr);

  // Read-only inspection: does `dir` hold a store, is it encrypted, which cipher params apply.
  // Never modifies anything. Ok with exists=false if there is no store; Corruption if store files
  // exist but no header verifies.
  static Status probe(Vfs& vfs, const std::string& dir, ProbeInfo* info);

  // Best-effort secure destruction of every file in `dir` and the directory itself: all headers
  // first (fast crypto-shred), then (full) all content, then truncate + remove + directory sync.
  // Continues past errors and returns the first one. On flash media overwriting cannot guarantee
  // the old cells are erased; the real protection is encryption + forgetting the key.
  static Status wipe(Vfs& vfs, const std::string& dir, WipeOptions opts = WipeOptions());

  ~KvStore();  // close() if not closed (errors ignored)
  KvStore(const KvStore&) = delete;
  KvStore& operator=(const KvStore&) = delete;

  // --- reads ------------------------------------------------------------------------------------
  // NotFound if absent; Corruption if the stored value fails verification. *seq (optional) is the
  // sequence number of the op that wrote the value.
  Status get(std::string_view ns, ByteView key, Bytes* value, uint64_t* seq = nullptr);
  bool contains(std::string_view ns, ByteView key) const;
  // Visits matching entries in order. Records that fail verification are skipped and the scan
  // continues; the return value is then Status::Corruption (everything visited was verified).
  Status scan(std::string_view ns, const ScanSpec& spec, const ScanVisitor& visit);
  // Convenience: collects into *out (cleared first).
  Status scan(std::string_view ns, const ScanSpec& spec, std::vector<KvEntry>* out);
  // Number of keys matching spec (ignores limit/reverse; no disk reads).
  uint64_t count(std::string_view ns, const ScanSpec& spec = ScanSpec()) const;

  // --- writes -----------------------------------------------------------------------------------
  Status put(std::string_view ns, ByteView key, ByteView value, WriteOptions wo = WriteOptions());
  Status del(std::string_view ns, ByteView key, WriteOptions wo = WriteOptions());
  // Applies the whole batch atomically. *first_seq (optional) receives the first op's sequence
  // number (op i gets first_seq + i). An empty batch is a no-op returning Ok. Errors: TooLarge,
  // InvalidArgument, ReadOnly, Busy, Broken, NoSpace (rolled back, store still usable), IoError
  // (store becomes Broken; the batch may or may not be present after reopen).
  Status write(const WriteBatch& batch, WriteOptions wo = WriteOptions(), uint64_t* first_seq = nullptr);
  // Makes every earlier write durable.
  Status sync();

  // Rewrites live data into a new generation file and drops the old one (reclaims space from
  // overwritten/deleted records). Crash-safe at every step. NoSpace leaves the store unchanged.
  Status compact();
  // Atomically replaces the encryption: rewrites everything under `new_cipher` (nullptr = decrypt
  // to plaintext) with new cipher params, then scrubs the old file. A crash leaves either the old
  // store (old key) or the new one (new key) — probe() tells which params apply.
  Status rekey(std::unique_ptr<RecordCipher> new_cipher, ByteView new_cipher_params);

  // Syncs and releases the files. Further calls return Broken. Idempotent.
  Status close();
  // Closes the store and wipe()s its directory.
  Status wipe(WipeOptions opts = WipeOptions());

  // --- introspection ----------------------------------------------------------------------------
  uint64_t last_sequence() const { return last_seq_; }
  NamespaceStats namespace_stats(std::string_view ns) const;
  std::vector<std::string> namespaces() const;  // sorted, non-empty namespaces only
  StoreStats stats() const;
  const RecoveryReport& recovery_report() const { return report_; }
  bool broken() const { return broken_; }

 private:
  struct Loc {
    uint64_t frame_off = 0;   // offset of the frame header in the active file
    uint64_t seq = 0;         // sequence number of the op that wrote this value
    uint32_t payload_len = 0; // on-disk payload length of the frame
    uint32_t val_off = 0;     // value offset within the frame's plaintext payload
    uint32_t val_len = 0;
    uint32_t val_crc = 0;     // CRC-32 of the value (plaintext stores)
  };
  using KeyMap = std::map<std::string, Loc, std::less<>>;
  struct Ns {
    KeyMap keys;
    NamespaceStats st;
  };
  using NsMap = std::map<std::string, Ns, std::less<>>;

  KvStore(Vfs* vfs, std::string dir, Options opts);

  Status do_open();
  Status recover_frames();
  // Writes a new generation holding the live data (optionally under a different cipher), makes it
  // the active file and removes the old one. Also used to create a fresh store.
  Status compact_impl(bool change_cipher, std::unique_ptr<RecordCipher> new_cipher,
                      const Bytes& params, bool scrub_old);
  Status write_check() const;
  Status read_value(const Loc& loc, Bytes* out);
  Status frame_plaintext(const Loc& loc, const Bytes** pt);
  // Validates the whole payload, then applies it to the index. False (nothing applied) if malformed.
  bool apply_payload(ByteView pt, uint8_t type, uint64_t frame_off, uint32_t payload_len,
                     uint64_t seq, uint64_t* count);
  void put_index(std::string_view ns, std::string_view key, const Loc& loc);
  void erase_key(std::string_view ns, std::string_view key);
  void drop_entry(NsMap::iterator nit, KeyMap::iterator kit);
  void erase_range(std::string_view ns, std::string_view lo, bool has_hi, std::string_view hi);
  void maybe_auto_compact();
  uint64_t live_estimate() const;
  void new_session_prefix();
  void fill_random(uint8_t* buf, size_t n);

  Vfs* vfs_;
  std::string dir_;
  Options opts_;
  std::unique_ptr<VfsFile> file_;
  std::string file_path_;
  uint64_t gen_ = 0;
  uint32_t ver_ = 1;      // format version of the active file
  uint64_t max_gen_ = 0;  // highest generation number seen in the directory (never reused)
  uint64_t base_seq_ = 0;
  uint64_t last_seq_ = 0;
  uint64_t file_end_ = 0;
  uint8_t salt_[16] = {};
  uint32_t crc_seed_ = 0;
  Bytes params_;
  uint8_t nonce_prefix_[16] = {};
  uint64_t nonce_ctr_ = 0;
  bool closed_ = false, broken_ = false, dirty_ = false;
  int scanning_ = 0;
  NsMap index_;
  uint64_t records_ = 0, live_bytes_ = 0;
  // One-frame plaintext cache for encrypted reads.
  uint64_t cache_off_ = 0;
  bool cache_valid_ = false;
  Bytes cache_pt_;
  uint64_t next_auto_compact_ = 0;
  StoreStats counters_;
  RecoveryReport report_;
  uint64_t det_counter_ = 0;  // deterministic salt source when no entropy is injected
};

}  // namespace ember::store
