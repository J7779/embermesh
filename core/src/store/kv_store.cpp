// EmberMesh store — KvStore implementation. Format and guarantees: store/kv_store.h, store/kv_format.h.
#include "store/kv_store.h"

#include <algorithm>
#include <cstring>

#include "store/kv_format.h"

namespace ember::store {

using fmt::kDataStart;
using fmt::kFrameHeader;
using fmt::kFrameOverhead;

namespace {

constexpr uint8_t kOpPut = 1, kOpDel = 2, kOpRange = 3;
constexpr size_t kWindow = 64 * 1024;
constexpr size_t kCacheMax = 1 << 20;  // largest decrypted frame kept in the read cache

std::string_view sv(ByteView b) {
  return std::string_view(reinterpret_cast<const char*>(b.data()), b.size());
}
ByteView bv(std::string_view s) { return ByteView(s); }

void put_le(uint8_t* p, uint64_t v, int n) {
  for (int i = 0; i < n; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
uint64_t get_le(const uint8_t* p, int n) {
  uint64_t v = 0;
  for (int i = 0; i < n; ++i) v |= uint64_t(p[i]) << (8 * i);
  return v;
}

uint64_t mix64(uint64_t z) {
  z += 0x9E3779B97F4A7C15ull;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

struct FrameHdr {
  uint8_t type = 0, flags = 0;
  uint32_t len = 0;
  uint64_t seq = 0;
  uint32_t hcrc = 0;
  uint8_t raw[kFrameHeader];
};

void encode_frame_header(uint8_t* h, uint8_t type, uint8_t flags, uint32_t len, uint64_t seq,
                         uint32_t seed) {
  h[0] = fmt::kFrameMagic0;
  h[1] = fmt::kFrameMagic1;
  h[2] = type;
  h[3] = flags;
  put_le(h + 4, len, 4);
  put_le(h + 8, seq, 8);
  put_le(h + 16, fmt::crc32(ByteView(h, 16), seed), 4);
}

bool parse_frame_header(const uint8_t* h, uint32_t seed, FrameHdr* out) {
  if (h[0] != fmt::kFrameMagic0 || h[1] != fmt::kFrameMagic1) return false;
  if (h[2] < fmt::kFrameBatch || h[2] > fmt::kFrameSnapCommit) return false;
  if (h[3] & ~fmt::kFrameFlagEncrypted) return false;
  uint32_t hcrc = static_cast<uint32_t>(get_le(h + 16, 4));
  if (fmt::crc32(ByteView(h, 16), seed) != hcrc) return false;
  out->type = h[2];
  out->flags = h[3];
  out->len = static_cast<uint32_t>(get_le(h + 4, 4));
  out->seq = get_le(h + 8, 8);
  out->hcrc = hcrc;
  std::memcpy(out->raw, h, kFrameHeader);
  return out->len <= fmt::kMaxFramePayload;
}

// Per-file parameters needed to build or open frames.
struct FileCtx {
  const uint8_t* salt;
  uint64_t gen;
  uint32_t seed;
  RecordCipher* cipher;
  const uint8_t* nonce_prefix;
  uint64_t* ctr;
};

Bytes frame_aad(const FileCtx& c, const uint8_t* hdr16, ByteView extra) {
  Bytes aad(16 + 8 + 16 + extra.size());
  std::memcpy(aad.data(), c.salt, 16);
  put_le(aad.data() + 16, c.gen, 8);
  std::memcpy(aad.data() + 24, hdr16, 16);
  if (!extra.empty()) std::memcpy(aad.data() + 40, extra.data(), extra.size());
  return aad;
}

// Extra AAD of version-2 snapshot frames (see kv_format.h).
Bytes snap_extra(uint64_t ordinal, bool commit, uint32_t running_crc) {
  Bytes e(commit ? 12 : 8);
  put_le(e.data(), ordinal, 8);
  if (commit) put_le(e.data() + 8, running_crc, 4);
  return e;
}

// Nonce counter of an encrypted payload (0 if too short).
uint64_t payload_ctr(ByteView payload) {
  return payload.size() >= RecordCipher::kNonceSize ? get_le(payload.data() + 16, 8) : 0;
}

bool build_frame(const FileCtx& c, uint8_t type, uint64_t seq, ByteView pt, Bytes* out,
                 ByteView extra = ByteView()) {
  uint64_t plen = c.cipher ? RecordCipher::kNonceSize + pt.size() + c.cipher->overhead() : pt.size();
  if (plen > fmt::kMaxFramePayload) return false;
  out->assign(kFrameHeader, 0);
  encode_frame_header(out->data(), type, c.cipher ? fmt::kFrameFlagEncrypted : 0,
                      static_cast<uint32_t>(plen), seq, c.seed);
  uint32_t hcrc = static_cast<uint32_t>(get_le(out->data() + 16, 4));
  if (c.cipher) {
    uint8_t nonce[RecordCipher::kNonceSize];
    std::memcpy(nonce, c.nonce_prefix, 16);
    put_le(nonce + 16, (*c.ctr)++, 8);
    Bytes aad = frame_aad(c, out->data(), extra);
    Bytes ct;
    if (!c.cipher->seal(ByteView(nonce, sizeof nonce), aad, pt, &ct)) return false;
    if (ct.size() != pt.size() + c.cipher->overhead()) return false;
    out->insert(out->end(), nonce, nonce + sizeof nonce);
    out->insert(out->end(), ct.begin(), ct.end());
  } else {
    out->insert(out->end(), pt.begin(), pt.end());
  }
  uint32_t fcrc = fmt::crc32(ByteView(out->data() + kFrameHeader, out->size() - kFrameHeader), hcrc);
  uint8_t t[4];
  put_le(t, fcrc, 4);
  out->insert(out->end(), t, t + 4);
  return true;
}

bool open_payload(const FileCtx& c, const uint8_t* hdr16, ByteView payload, Bytes* pt,
                  ByteView extra = ByteView()) {
  if (!c.cipher || payload.size() < RecordCipher::kNonceSize + c.cipher->overhead()) return false;
  Bytes aad = frame_aad(c, hdr16, extra);
  return c.cipher->open(payload.sub(0, RecordCipher::kNonceSize), aad,
                        payload.sub(RecordCipher::kNonceSize), pt);
}

bool make_keycheck(RecordCipher* c, const fmt::Header& h, const uint8_t* nonce, Bytes* out) {
  Bytes aad = fmt::header_aad(h);
  Bytes ct;
  ByteView pt(reinterpret_cast<const uint8_t*>(fmt::kKeyCheckPlain), 16);
  if (!c->seal(ByteView(nonce, RecordCipher::kNonceSize), aad, pt, &ct)) return false;
  out->assign(nonce, nonce + RecordCipher::kNonceSize);
  out->insert(out->end(), ct.begin(), ct.end());
  return out->size() <= fmt::kKeyCheckMax;
}

bool verify_keycheck(RecordCipher* c, const fmt::Header& h) {
  if (h.keycheck.size() < RecordCipher::kNonceSize) return false;
  ByteView kc(h.keycheck);
  Bytes pt;
  if (!c->open(kc.sub(0, RecordCipher::kNonceSize), fmt::header_aad(h),
               kc.sub(RecordCipher::kNonceSize), &pt))
    return false;
  return ByteView(pt) == ByteView(reinterpret_cast<const uint8_t*>(fmt::kKeyCheckPlain), 16);
}

// Buffered random-access reader used by recovery. Small reads (headers, the sync scan) go through
// a forward-moving 64 KiB window; payload reads that do not fit inside the current window are read
// directly so that checking a candidate frame never throws away the scan window (a hostile file
// with a forged header every few bytes must not cost a window refill per candidate).
// Every byte fetched from the file is charged to `work`; recovery gives up once that exceeds its
// budget (see kv_store.h, RecoveryReport::work_limit_hit).
class FileWindow {
 public:
  FileWindow(VfsFile* f, uint64_t size) : f_(f), size_(size) {}
  const uint8_t* peek(uint64_t off, size_t n, Status* st) {
    if (off >= buf_off_ && off + n <= buf_off_ + buf_.size()) return buf_.data() + (off - buf_off_);
    if (off > size_ || n > size_ - off) {
      *st = Status::Corruption;
      return nullptr;
    }
    uint64_t avail = size_ - off;
    size_t len = avail < kWindow ? static_cast<size_t>(avail) : kWindow;
    if (len < n) len = n;
    buf_.resize(len);
    work += len;
    Status s = read_exact(*f_, off, buf_.data(), len);
    if (s != Status::Ok) {
      buf_.clear();
      *st = s;
      return nullptr;
    }
    buf_off_ = off;
    return buf_.data();
  }
  Status read(uint64_t off, size_t n, uint8_t* dst) {
    if (off >= buf_off_ && off + n <= buf_off_ + buf_.size()) {
      if (n) std::memcpy(dst, buf_.data() + (off - buf_off_), n);
      return Status::Ok;
    }
    work += n;
    return read_exact(*f_, off, dst, n);
  }
  uint64_t work = 0;  // bytes fetched from the file so far

 private:
  VfsFile* f_;
  uint64_t size_;
  Bytes buf_;
  uint64_t buf_off_ = 0;
};

// Recovery work budget: one full pass for the genuine frames plus this many more bytes of
// candidate-frame verification (a few passes worth; honest damage needs a tiny fraction of it).
uint64_t recovery_budget(uint64_t size) { return 4 * size + (uint64_t(1) << 20); }

// Ok: valid frame at off (payload filled, CRC verified). Corruption: no valid frame. TooLarge:
// verifying this candidate would exceed the work budget. IoError.
Status try_frame(FileWindow& w, uint64_t off, uint64_t size, uint32_t seed, uint64_t budget,
                 FrameHdr* fh, Bytes* payload) {
  if (off > size || size - off < kFrameOverhead) return Status::Corruption;
  Status st = Status::Ok;
  const uint8_t* h = w.peek(off, kFrameHeader, &st);
  if (!h) return st;
  if (!parse_frame_header(h, seed, fh)) return Status::Corruption;
  if (fh->len > size - off - kFrameOverhead) return Status::Corruption;
  if (w.work + fh->len + 4 > budget) return Status::TooLarge;
  payload->resize(static_cast<size_t>(fh->len) + 4);
  st = w.read(off + kFrameHeader, payload->size(), payload->data());
  if (st != Status::Ok) return st;
  uint32_t stored = static_cast<uint32_t>(get_le(payload->data() + fh->len, 4));
  if (fmt::crc32(ByteView(payload->data(), fh->len), fh->hcrc) != stored) return Status::Corruption;
  payload->resize(fh->len);
  return Status::Ok;
}

// Scans forward from `from` for the next valid frame. Ok + *at, NotFound, TooLarge (work budget
// exhausted; *at = where the scan stopped), or IoError.
Status resync(FileWindow& w, uint64_t from, uint64_t size, uint32_t seed, uint64_t budget, uint64_t* at,
              FrameHdr* fh, Bytes* payload) {
  for (uint64_t p = from; p + kFrameOverhead <= size; ++p) {
    Status st = Status::Ok;
    const uint8_t* b = w.peek(p, 2, &st);
    if (!b) return st == Status::Corruption ? Status::NotFound : st;
    if (b[0] != fmt::kFrameMagic0 || b[1] != fmt::kFrameMagic1) continue;
    st = try_frame(w, p, size, seed, budget, fh, payload);
    if (st == Status::Ok || st == Status::TooLarge) {
      *at = p;
      return st;
    }
    if (st != Status::Corruption) return st;
  }
  return Status::NotFound;
}

bool read_str(ByteReader& r, size_t max, std::string_view* out) {
  uint64_t n = r.varint();
  if (!r.ok() || n > max) {
    r.fail();
    return false;
  }
  ByteView v = r.bytes(static_cast<size_t>(n));
  *out = sv(v);
  return r.ok();
}

// Overwrites (all or the first 4 KiB of) a file with zeros, syncs, truncates to 0, syncs.
Status scrub_file(Vfs& vfs, const std::string& path, bool full) {
  std::unique_ptr<VfsFile> f;
  Status s = vfs.open(path, OpenMode::ReadWrite, &f);
  if (s != Status::Ok) return s;
  uint64_t size = 0;
  s = f->size(&size);
  if (s != Status::Ok) return s;
  uint64_t end = full ? size : std::min<uint64_t>(size, 4096);
  Bytes zeros(static_cast<size_t>(std::min<uint64_t>(end, kWindow)), 0);
  Status first = Status::Ok;
  for (uint64_t off = 0; off < end; off += zeros.size()) {
    size_t n = static_cast<size_t>(std::min<uint64_t>(zeros.size(), end - off));
    s = f->write_at(off, ByteView(zeros.data(), n));
    if (s != Status::Ok) {
      first = s;
      break;
    }
  }
  s = f->sync();
  if (first == Status::Ok) first = s;
  if (full) {
    s = f->truncate(0);
    if (first == Status::Ok) first = s;
    s = f->sync();
    if (first == Status::Ok) first = s;
  }
  return first;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// WriteBatch
// ---------------------------------------------------------------------------------------------

void WriteBatch::header(uint8_t op, std::string_view ns, ByteView key) {
  if (ns.size() > kMaxNamespaceLen || key.size() > kMaxKeyLen) bad_name_ = true;
  ByteWriter w;
  w.u8(op).varint(ns.size()).str(ns).varint(key.size()).bytes(key);
  rep_.insert(rep_.end(), w.data().begin(), w.data().end());
  ++count_;
}

void WriteBatch::put(std::string_view ns, ByteView key, ByteView value) {
  header(kOpPut, ns, key);
  ByteWriter w;
  w.varint(value.size());
  rep_.insert(rep_.end(), w.data().begin(), w.data().end());
  rep_.insert(rep_.end(), value.begin(), value.end());
  if (value.size() > max_value_) max_value_ = value.size();
}

void WriteBatch::del(std::string_view ns, ByteView key) { header(kOpDel, ns, key); }

void WriteBatch::delete_range(std::string_view ns, ByteView lower, ByteView upper) {
  header(kOpRange, ns, lower);
  if (upper.size() > kMaxKeyLen) bad_name_ = true;
  ByteWriter w;
  w.u8(1).varint(upper.size()).bytes(upper);
  rep_.insert(rep_.end(), w.data().begin(), w.data().end());
}

void WriteBatch::delete_from(std::string_view ns, ByteView lower) {
  header(kOpRange, ns, lower);
  rep_.push_back(0);
}

void WriteBatch::drop_namespace(std::string_view ns) { delete_from(ns, ByteView()); }

void WriteBatch::clear() {
  rep_.clear();
  count_ = 0;
  max_value_ = 0;
  bad_name_ = false;
}

// ---------------------------------------------------------------------------------------------
// KvStore: lifecycle
// ---------------------------------------------------------------------------------------------

KvStore::KvStore(Vfs* vfs, std::string dir, Options opts)
    : vfs_(vfs), dir_(std::move(dir)), opts_(std::move(opts)) {}

KvStore::~KvStore() { close(); }

Status KvStore::open(Vfs* vfs, const std::string& dir, Options options,
                     std::unique_ptr<KvStore>* out, RecoveryReport* report) {
  out->reset();
  if (report) *report = RecoveryReport();
  if (!vfs) return Status::InvalidArgument;
  if (options.max_value_size > kMaxValueLen || options.max_batch_bytes > kMaxBatchBytes ||
      options.cipher_params.size() > kMaxCipherParams)
    return Status::InvalidArgument;
  if (options.cipher && (!options.entropy || options.cipher->overhead() > RecordCipher::kMaxOverhead))
    return Status::InvalidArgument;
  if (options.snapshot_chunk_bytes < 256) options.snapshot_chunk_bytes = 256;
  if (options.snapshot_chunk_bytes > (size_t(1) << 20)) options.snapshot_chunk_bytes = size_t(1) << 20;
  std::unique_ptr<KvStore> s(new KvStore(vfs, dir, std::move(options)));
  Status st = s->do_open();
  if (report) *report = s->report_;
  if (st != Status::Ok) {
    s->dirty_ = false;
    s->file_.reset();
    s->closed_ = true;
    return st;
  }
  *out = std::move(s);
  return Status::Ok;
}

void KvStore::fill_random(uint8_t* buf, size_t n) {
  if (opts_.entropy) {
    opts_.entropy(buf, n);
    return;
  }
  // Deterministic fallback (plaintext stores only): distinct per file/generation, not secret.
  uint64_t st = mix64(gen_ * 0x100000001B3ull ^ max_gen_ ^ (++det_counter_ << 32) ^ last_seq_ ^
                      (uint64_t(fmt::crc32(bv(dir_))) << 17));
  for (size_t i = 0; i < n; ++i) {
    if (i % 8 == 0) st = mix64(st);
    buf[i] = static_cast<uint8_t>(st >> (8 * (i % 8)));
  }
}

Status KvStore::do_open() {
  std::vector<std::string> names;
  Status st = vfs_->list(dir_, &names);
  if (st == Status::NotFound) {
    if (opts_.read_only || !opts_.create_if_missing) return Status::NotFound;
    st = vfs_->mkdir(dir_);
    if (st != Status::Ok) return st;
    size_t slash = dir_.find_last_of('/');
    std::string parent = slash == std::string::npos ? std::string(".")
                         : slash == 0               ? std::string("/")
                                                    : dir_.substr(0, slash);
    st = vfs_->sync_dir(parent);
    if (st != Status::Ok) return st;
    names.clear();
  } else if (st != Status::Ok) {
    return st;
  }

  std::vector<uint64_t> data_gens;
  std::vector<std::string> tmps;
  for (const auto& n : names) {
    char kind;
    uint64_t g;
    if (!fmt::parse_name(n, &kind, &g)) continue;
    if (g > max_gen_) max_gen_ = g;
    if (kind == 'd')
      data_gens.push_back(g);
    else
      tmps.push_back(n);
  }
  std::sort(data_gens.rbegin(), data_gens.rend());

  if (data_gens.empty()) {
    if (opts_.require_encryption && !opts_.cipher) return Status::Tampered;
    if (opts_.read_only || !opts_.create_if_missing) return Status::NotFound;
    for (const auto& t : tmps) {
      std::string p = path_join(dir_, t);
      scrub_file(*vfs_, p, true);
      if (vfs_->remove(p) == Status::Ok) ++report_.tmp_files_removed;
    }
    st = compact_impl(false, nullptr, opts_.cipher_params, false);
    if (st != Status::Ok) return st;
    report_.created = true;
    report_.generation = gen_;
    new_session_prefix();
    return Status::Ok;
  }
  if (opts_.error_if_exists) return Status::Exists;

  // Read every generation's header (newest first) and pick the newest usable one. Downgrade
  // protection: with a cipher only encrypted generations are usable; without one, the presence of
  // any encrypted generation makes the store Locked. A plaintext generation is therefore never
  // preferred over (or allowed to cause the deletion of) an encrypted one.
  fmt::Header hdr;
  bool found = false, any_enc = false, any_plain = false;
  uint32_t unreadable = 0;
  std::vector<uint64_t> skipped_plain;  // plaintext generations ignored because we have a cipher
  for (uint64_t g : data_gens) {
    std::string p = path_join(dir_, fmt::data_name(g));
    std::unique_ptr<VfsFile> f;
    st = vfs_->open(p, opts_.read_only ? OpenMode::ReadOnly : OpenMode::ReadWrite, &f);
    if (st == Status::NotFound || st == Status::InvalidArgument) continue;
    if (st != Status::Ok) return st;
    uint64_t size = 0;
    st = f->size(&size);
    if (st != Status::Ok) return st;
    Bytes buf(static_cast<size_t>(std::min<uint64_t>(size, kDataStart)));
    st = read_exact(*f, 0, buf.data(), buf.size());
    if (st != Status::Ok) return st;
    ByteView all(buf);
    fmt::Header ha, hb;
    bool a = fmt::decode_header(all.sub(0, fmt::kHeaderSize), &ha) && ha.generation == g;
    bool b = fmt::decode_header(all.sub(fmt::kHeaderSize, fmt::kHeaderSize), &hb) && hb.generation == g;
    if (!a && !b) {  // (a header naming another generation is not trusted either)
      if (!found) ++unreadable;
      continue;
    }
    const fmt::Header& hh = a ? ha : hb;
    bool enc = (hh.flags & fmt::kFileFlagEncrypted) != 0;
    (enc ? any_enc : any_plain) = true;
    if (found) continue;  // only classifying older generations
    if (opts_.cipher && !enc) {
      skipped_plain.push_back(g);
      continue;
    }
    hdr = hh;
    if (!a || !b) report_.header_copy_damaged = true;
    file_ = std::move(f);
    file_path_ = p;
    found = true;
  }
  report_.mixed_encryption = any_enc && any_plain;
  if (!opts_.cipher && any_enc) {
    file_.reset();
    return Status::Locked;
  }
  if (!found) {
    if (opts_.cipher && any_plain) return opts_.require_encryption ? Status::Tampered : Status::InvalidArgument;
    return Status::Corruption;
  }
  if (opts_.require_encryption && !(hdr.flags & fmt::kFileFlagEncrypted)) {
    file_.reset();
    return Status::Tampered;
  }
  if (unreadable) report_.corrupt_regions += unreadable;  // newer generation(s) lost entirely
  if (hdr.version < fmt::kFormatVersion || hdr.version > fmt::kFormatVersionMax) return Status::InvalidArgument;

  bool encrypted = (hdr.flags & fmt::kFileFlagEncrypted) != 0;
  if (encrypted && !opts_.cipher) return Status::Locked;
  if (!encrypted && opts_.cipher) return Status::InvalidArgument;
  if (encrypted && !verify_keycheck(opts_.cipher.get(), hdr)) return Status::WrongKey;

  ver_ = hdr.version;
  gen_ = hdr.generation;
  base_seq_ = hdr.base_seq;
  last_seq_ = base_seq_;
  std::memcpy(salt_, hdr.salt, 16);
  crc_seed_ = fmt::crc32(ByteView(salt_, 16));
  params_ = hdr.params;
  report_.generation = gen_;

  st = recover_frames();
  if (st != Status::Ok) return st;
  if (opts_.on_corruption == OnCorruption::Fail && !report_.clean()) return Status::Corruption;

  if (!opts_.read_only) {
    if (report_.header_copy_damaged) {
      Bytes h = fmt::encode_header(hdr);
      Bytes both = h;
      both.insert(both.end(), h.begin(), h.end());
      st = file_->write_at(0, both);
      if (st == Status::Ok) st = file_->sync();
      if (st != Status::Ok) return st;
    }
    uint64_t size = 0;
    st = file_->size(&size);
    if (st != Status::Ok) return st;
    if (size != file_end_) {
      st = file_->truncate(file_end_);
      if (st == Status::Ok) st = file_->sync();
      if (st != Status::Ok) return st;
    }
    bool removed = false;
    for (const auto& t : tmps) {
      std::string p = path_join(dir_, t);
      scrub_file(*vfs_, p, true);
      if (vfs_->remove(p) == Status::Ok) {
        ++report_.tmp_files_removed;
        removed = true;
      }
    }
    for (uint64_t g : data_gens) {
      bool skipped = std::find(skipped_plain.begin(), skipped_plain.end(), g) != skipped_plain.end();
      if (g >= gen_ && !skipped) continue;
      if (!report_.clean() || unreadable) {
        ++report_.old_generations_kept;
        continue;
      }
      std::string p = path_join(dir_, fmt::data_name(g));
      scrub_file(*vfs_, p, true);
      if (vfs_->remove(p) == Status::Ok) {
        ++report_.old_generations_removed;
        removed = true;
      }
    }
    if (removed) vfs_->sync_dir(dir_);  // best effort: leftovers are removed again next time
  }
  new_session_prefix();
  return Status::Ok;
}

Status KvStore::recover_frames() {
  uint64_t size = 0;
  Status st = file_->size(&size);
  if (st != Status::Ok) return st;
  FileWindow win(file_.get(), size);
  RecordCipher* cipher = opts_.cipher.get();
  FileCtx ctx{salt_, gen_, crc_seed_, cipher, nonce_prefix_, &nonce_ctr_};

  uint64_t off = kDataStart, good_end = kDataStart;
  uint64_t expected = base_seq_ + 1;
  bool in_snap = true;
  uint64_t snap_recs = 0, snap_chunks = 0;
  uint64_t next_ord = 0;   // next expected snapshot chunk ordinal
  uint32_t snap_rcrc = 0;  // running CRC over accepted chunk payloads (v2 commit AAD)
  FrameHdr fh;
  Bytes payload, pt;
  const uint64_t budget = recovery_budget(size);
  auto give_up = [&](uint64_t at) {
    report_.work_limit_hit = true;
    ++report_.corrupt_regions;
    report_.corrupt_bytes += size - at;
  };
  while (off < size) {
    st = try_frame(win, off, size, crc_seed_, budget, &fh, &payload);
    if (st == Status::TooLarge) {
      give_up(off);
      break;
    }
    if (st == Status::Corruption) {
      uint64_t next = 0;
      st = resync(win, off + 1, size, crc_seed_, budget, &next, &fh, &payload);
      if (st == Status::NotFound) {
        report_.torn_tail_bytes = size - off;
        break;
      }
      if (st == Status::TooLarge) {
        give_up(off);
        break;
      }
      if (st != Status::Ok) return st;
      ++report_.corrupt_regions;
      report_.corrupt_bytes += next - off;
      off = next;
    } else if (st != Status::Ok) {
      return st;
    }
    uint64_t frame_off = off;
    off += kFrameOverhead + fh.len;
    good_end = off;

    bool enc_frame = (fh.flags & fmt::kFrameFlagEncrypted) != 0;
    if (enc_frame != (cipher != nullptr)) {
      ++report_.rejected_frames;
      continue;
    }
    ByteView plain;
    uint64_t ord = 0;  // snapshot frame ordinal (encrypted stores)
    const bool snap_frame = fh.type == fmt::kFrameSnapChunk || fh.type == fmt::kFrameSnapCommit;
    if (cipher) {
      Bytes extra;
      if (snap_frame) {
        uint64_t c = payload_ctr(ByteView(payload));
        if (c == 0) {
          ++report_.rejected_frames;
          continue;
        }
        ord = c - 1;
        if (ver_ >= fmt::kFormatVersionBoundSnapshot)
          extra = snap_extra(ord, fh.type == fmt::kFrameSnapCommit, snap_rcrc);
      }
      if (!open_payload(ctx, fh.raw, payload, &pt, ByteView(extra))) {
        ++report_.rejected_frames;
        continue;
      }
      plain = ByteView(pt);
    } else {
      plain = ByteView(payload);
    }
    uint64_t n = 0;
    switch (fh.type) {
      case fmt::kFrameSnapChunk:
        if (!in_snap || fh.seq != base_seq_ || (cipher && ord < next_ord) ||
            !apply_payload(plain, fh.type, frame_off, fh.len, fh.seq, &n)) {
          ++report_.rejected_frames;  // (duplicated or out-of-order chunks included)
          continue;
        }
        if (cipher) {
          if (ord > next_ord) report_.snapshot_incomplete = true;  // chunk(s) missing
          next_ord = ord + 1;
          snap_rcrc = fmt::crc32(ByteView(payload), snap_rcrc);
        }
        snap_recs += n;
        ++snap_chunks;
        break;
      case fmt::kFrameSnapCommit: {
        ByteReader r(plain);
        uint64_t recs = r.varint(), chunks = r.varint();
        if (!in_snap || fh.seq != base_seq_ || !r.at_end()) {
          ++report_.rejected_frames;
          continue;
        }
        if (recs != snap_recs || chunks != snap_chunks) report_.snapshot_incomplete = true;
        if (cipher && (ord != next_ord || chunks != ord)) report_.snapshot_incomplete = true;
        in_snap = false;
        break;
      }
      case fmt::kFrameBatch:
        if (fh.seq < expected || !apply_payload(plain, fh.type, frame_off, fh.len, fh.seq, &n)) {
          ++report_.rejected_frames;
          continue;
        }
        if (in_snap) {  // commit frame lost
          in_snap = false;
          report_.snapshot_incomplete = true;
        }
        if (fh.seq > expected) report_.lost_ops += fh.seq - expected;
        expected = fh.seq + n;
        ++report_.batches_applied;
        break;
      default:
        ++report_.rejected_frames;
        continue;
    }
    ++report_.frames_ok;
  }
  if (in_snap) report_.snapshot_incomplete = true;
  report_.snapshot_records = snap_recs;
  file_end_ = good_end;
  last_seq_ = expected - 1 > base_seq_ ? expected - 1 : base_seq_;
  return Status::Ok;
}

void KvStore::new_session_prefix() {
  fill_random(nonce_prefix_, 16);
  nonce_ctr_ = 0;
}

Status KvStore::close() {
  if (closed_) return Status::Ok;
  Status s = Status::Ok;
  if (file_ && dirty_ && !broken_ && !opts_.read_only) s = file_->sync();
  file_.reset();
  closed_ = true;
  dirty_ = false;
  cache_valid_ = false;
  cache_pt_.clear();
  return s;
}

Status KvStore::wipe(WipeOptions o) {
  file_.reset();
  closed_ = true;
  dirty_ = false;
  index_.clear();
  cache_pt_.assign(cache_pt_.size(), 0);
  cache_pt_.clear();
  cache_valid_ = false;
  records_ = live_bytes_ = 0;
  return wipe(*vfs_, dir_, o);
}

Status KvStore::wipe(Vfs& vfs, const std::string& dir, WipeOptions o) {
  std::vector<std::string> names;
  Status st = vfs.list(dir, &names);
  if (st == Status::NotFound) return Status::Ok;
  if (st != Status::Ok) return st;
  // Data files first: their headers hold the salt / key check / cipher params.
  std::sort(names.begin(), names.end());
  Status first = Status::Ok;
  auto note = [&](Status s) {
    if (first == Status::Ok && s != Status::Ok && s != Status::NotFound) first = s;
  };
  // scrub_file() refuses symlinks and hard-linked files (the OS VFS opens neither for writing):
  // those are only unlinked, never overwritten, so a wipe cannot destroy files outside `dir`.
  auto scrub = [&](const std::string& n, bool full) {
    Status s = scrub_file(vfs, path_join(dir, n), full);
    if (s != Status::InvalidArgument) note(s);
  };
  for (const auto& n : names) scrub(n, false);
  if (o.full)
    for (const auto& n : names) scrub(n, true);
  for (const auto& n : names) note(vfs.remove(path_join(dir, n)));
  note(vfs.sync_dir(dir));
  note(vfs.remove(dir));
  return first;
}

Status KvStore::probe(Vfs& vfs, const std::string& dir, ProbeInfo* info) {
  *info = ProbeInfo();
  std::vector<std::string> names;
  Status st = vfs.list(dir, &names);
  if (st == Status::NotFound) return Status::Ok;
  if (st != Status::Ok) return st;
  std::vector<uint64_t> gens;
  for (const auto& n : names) {
    char kind;
    uint64_t g;
    if (fmt::parse_name(n, &kind, &g) && kind == 'd') gens.push_back(g);
  }
  if (gens.empty()) return Status::Ok;
  std::sort(gens.rbegin(), gens.rend());
  // Newest valid generation, but never report "plaintext" while an encrypted generation exists:
  // then the newest encrypted one is described and mixed_encryption is set (see open()).
  bool have_plain = false, have_enc = false;
  ProbeInfo plain_info, enc_info;
  for (uint64_t g : gens) {
    std::unique_ptr<VfsFile> f;
    st = vfs.open(path_join(dir, fmt::data_name(g)), OpenMode::ReadOnly, &f);
    if (st == Status::NotFound || st == Status::InvalidArgument) continue;
    if (st != Status::Ok) return st;
    uint64_t size = 0;
    st = f->size(&size);
    if (st != Status::Ok) return st;
    Bytes buf(static_cast<size_t>(std::min<uint64_t>(size, kDataStart)));
    st = read_exact(*f, 0, buf.data(), buf.size());
    if (st != Status::Ok) return st;
    fmt::Header h;
    ByteView all(buf);
    bool ok = (fmt::decode_header(all.sub(0, fmt::kHeaderSize), &h) && h.generation == g) ||
              (fmt::decode_header(all.sub(fmt::kHeaderSize, fmt::kHeaderSize), &h) && h.generation == g);
    if (!ok) continue;
    bool enc = (h.flags & fmt::kFileFlagEncrypted) != 0;
    if (enc ? have_enc : have_plain) continue;  // keep the newest of each kind
    ProbeInfo& pi = enc ? enc_info : plain_info;
    pi.exists = true;
    pi.encrypted = enc;
    pi.format_version = h.version;
    pi.generation = h.generation;
    pi.cipher_params = h.params;
    (enc ? have_enc : have_plain) = true;
  }
  if (!have_enc && !have_plain) return Status::Corruption;
  *info = have_enc ? enc_info : plain_info;
  info->mixed_encryption = have_enc && have_plain;
  return Status::Ok;
}

// ---------------------------------------------------------------------------------------------
// Index maintenance
// ---------------------------------------------------------------------------------------------

bool KvStore::apply_payload(ByteView pt, uint8_t type, uint64_t frame_off, uint32_t payload_len,
                            uint64_t seq, uint64_t* count) {
  if (pt.size() > UINT32_MAX) return false;
  struct Op {
    uint8_t kind;
    std::string_view ns, key, hi;
    bool has_hi;
    uint64_t seq;
    uint32_t voff, vlen, crc;
  };
  // Pass 1: validate everything; nothing is applied unless the whole payload is well formed.
  ByteReader r(pt);
  uint64_t n = r.varint();
  if (!r.ok() || n == 0 || n > pt.size()) return false;
  if (type == fmt::kFrameBatch && seq + n < seq) return false;
  std::vector<Op> ops;
  ops.reserve(static_cast<size_t>(n));
  for (uint64_t i = 0; i < n; ++i) {
    Op op{};
    if (type == fmt::kFrameBatch) {
      op.kind = r.u8();
      op.seq = seq + i;
    } else {
      op.kind = kOpPut;
    }
    if (!read_str(r, kMaxNamespaceLen, &op.ns) || !read_str(r, kMaxKeyLen, &op.key)) return false;
    if (type != fmt::kFrameBatch) {
      op.seq = r.varint();
      if (!r.ok() || op.seq > base_seq_) return false;
    }
    if (op.kind == kOpPut) {
      uint64_t vl = r.varint();
      if (!r.ok() || vl > kMaxValueLen) return false;
      op.voff = static_cast<uint32_t>(r.pos());
      ByteView v = r.bytes(static_cast<size_t>(vl));
      if (!r.ok()) return false;
      op.vlen = static_cast<uint32_t>(vl);
      op.crc = fmt::crc32(v);
    } else if (op.kind == kOpRange) {
      uint8_t hh = r.u8();
      if (!r.ok() || hh > 1) return false;
      op.has_hi = hh == 1;
      if (op.has_hi && !read_str(r, kMaxKeyLen, &op.hi)) return false;
    } else if (op.kind != kOpDel) {
      return false;
    }
    ops.push_back(op);
  }
  if (!r.at_end()) return false;
  // Pass 2: apply in order.
  for (const Op& op : ops) {
    switch (op.kind) {
      case kOpPut: {
        Loc l;
        l.frame_off = frame_off;
        l.seq = op.seq;
        l.payload_len = payload_len;
        l.val_off = op.voff;
        l.val_len = op.vlen;
        l.val_crc = op.crc;
        put_index(op.ns, op.key, l);
        break;
      }
      case kOpDel: erase_key(op.ns, op.key); break;
      case kOpRange: erase_range(op.ns, op.key, op.has_hi, op.hi); break;
    }
  }
  *count = n;
  return true;
}

void KvStore::put_index(std::string_view ns, std::string_view key, const Loc& loc) {
  auto nit = index_.find(ns);
  if (nit == index_.end()) nit = index_.emplace(std::string(ns), Ns()).first;
  Ns& n = nit->second;
  auto kit = n.keys.lower_bound(key);
  if (kit != n.keys.end() && kit->first == key) {
    n.st.value_bytes -= kit->second.val_len;
    live_bytes_ -= kit->second.val_len;
    kit->second = loc;
  } else {
    n.keys.emplace_hint(kit, std::string(key), loc);
    ++n.st.records;
    n.st.key_bytes += key.size();
    ++records_;
    live_bytes_ += key.size() + ns.size();
  }
  n.st.value_bytes += loc.val_len;
  live_bytes_ += loc.val_len;
}

void KvStore::erase_key(std::string_view ns, std::string_view key) {
  auto nit = index_.find(ns);
  if (nit == index_.end()) return;
  Ns& n = nit->second;
  auto kit = n.keys.find(key);
  if (kit == n.keys.end()) return;
  drop_entry(nit, kit);
}

void KvStore::drop_entry(NsMap::iterator nit, KeyMap::iterator kit) {
  Ns& n = nit->second;
  n.st.records--;
  n.st.key_bytes -= kit->first.size();
  n.st.value_bytes -= kit->second.val_len;
  --records_;
  live_bytes_ -= kit->first.size() + kit->second.val_len + nit->first.size();
  n.keys.erase(kit);
  if (n.keys.empty()) index_.erase(nit);
}

void KvStore::erase_range(std::string_view ns, std::string_view lo, bool has_hi, std::string_view hi) {
  auto nit = index_.find(ns);
  if (nit == index_.end()) return;
  if (has_hi && hi <= lo) return;
  Ns& n = nit->second;
  auto it = n.keys.lower_bound(lo);
  while (it != n.keys.end() && (!has_hi || std::string_view(it->first) < hi)) {
    n.st.records--;
    n.st.key_bytes -= it->first.size();
    n.st.value_bytes -= it->second.val_len;
    --records_;
    live_bytes_ -= it->first.size() + it->second.val_len + nit->first.size();
    it = n.keys.erase(it);
  }
  if (n.keys.empty()) index_.erase(nit);
}

uint64_t KvStore::live_estimate() const { return kDataStart + live_bytes_ + records_ * 24; }

// ---------------------------------------------------------------------------------------------
// Reads
// ---------------------------------------------------------------------------------------------

Status KvStore::frame_plaintext(const Loc& loc, const Bytes** out) {
  if (cache_valid_ && cache_off_ == loc.frame_off) {
    *out = &cache_pt_;
    return Status::Ok;
  }
  Bytes frame(kFrameOverhead + static_cast<size_t>(loc.payload_len));
  Status s = read_exact(*file_, loc.frame_off, frame.data(), frame.size());
  if (s != Status::Ok) return s;
  FrameHdr fh;
  if (!parse_frame_header(frame.data(), crc_seed_, &fh) || fh.len != loc.payload_len)
    return Status::Corruption;
  uint32_t stored = static_cast<uint32_t>(get_le(frame.data() + kFrameHeader + fh.len, 4));
  if (fmt::crc32(ByteView(frame.data() + kFrameHeader, fh.len), fh.hcrc) != stored)
    return Status::Corruption;
  FileCtx ctx{salt_, gen_, crc_seed_, opts_.cipher.get(), nonce_prefix_, &nonce_ctr_};
  Bytes pt;
  ByteView payload(frame.data() + kFrameHeader, fh.len);
  Bytes extra;
  if (fh.type == fmt::kFrameSnapChunk && ver_ >= fmt::kFormatVersionBoundSnapshot) {
    uint64_t c = payload_ctr(payload);
    if (c == 0) return Status::Corruption;
    extra = snap_extra(c - 1, false, 0);
  }
  if (!open_payload(ctx, fh.raw, payload, &pt, ByteView(extra))) return Status::Corruption;
  cache_pt_.swap(pt);
  cache_off_ = loc.frame_off;
  cache_valid_ = cache_pt_.size() <= kCacheMax;
  *out = &cache_pt_;
  return Status::Ok;
}

Status KvStore::read_value(const Loc& loc, Bytes* out) {
  if (!file_) return Status::Broken;
  if (opts_.cipher) {
    const Bytes* pt = nullptr;
    Status s = frame_plaintext(loc, &pt);
    if (s != Status::Ok) return s;
    if (static_cast<uint64_t>(loc.val_off) + loc.val_len > pt->size()) return Status::Corruption;
    out->assign(pt->begin() + loc.val_off, pt->begin() + loc.val_off + loc.val_len);
    if (!cache_valid_) cache_pt_.clear();
    return Status::Ok;
  }
  out->resize(loc.val_len);
  Status s = read_exact(*file_, loc.frame_off + kFrameHeader + loc.val_off, out->data(), out->size());
  if (s != Status::Ok) return s;
  if (fmt::crc32(ByteView(*out)) != loc.val_crc) return Status::Corruption;
  return Status::Ok;
}

Status KvStore::get(std::string_view ns, ByteView key, Bytes* value, uint64_t* seq) {
  value->clear();
  if (closed_) return Status::Broken;
  auto nit = index_.find(ns);
  if (nit == index_.end()) return Status::NotFound;
  auto kit = nit->second.keys.find(sv(key));
  if (kit == nit->second.keys.end()) return Status::NotFound;
  Status s = read_value(kit->second, value);
  if (s != Status::Ok) {
    value->clear();
    if (s == Status::Corruption) ++counters_.corrupt_reads;
    return s;
  }
  if (seq) *seq = kit->second.seq;
  return Status::Ok;
}

bool KvStore::contains(std::string_view ns, ByteView key) const {
  auto nit = index_.find(ns);
  return nit != index_.end() && nit->second.keys.count(sv(key)) != 0;
}

namespace {
// Effective [lo, hi) of a ScanSpec. has_hi=false means unbounded above.
void scan_bounds(const ScanSpec& spec, std::string* lo, bool* has_hi, std::string* hi) {
  *lo = std::string(sv(spec.lower));
  std::string pre(sv(spec.prefix));
  if (pre > *lo) *lo = pre;
  *has_hi = spec.has_upper;
  if (spec.has_upper) *hi = std::string(sv(spec.upper));
  if (!pre.empty()) {
    std::string succ = pre;
    while (!succ.empty() && static_cast<uint8_t>(succ.back()) == 0xFF) succ.pop_back();
    if (!succ.empty()) {
      succ.back() = static_cast<char>(static_cast<uint8_t>(succ.back()) + 1);
      if (!*has_hi || succ < *hi) *hi = succ;
      *has_hi = true;
    }
  }
}
}  // namespace

Status KvStore::scan(std::string_view ns, const ScanSpec& spec, const ScanVisitor& visit) {
  if (closed_) return Status::Broken;
  auto nit = index_.find(ns);
  if (nit == index_.end()) return Status::Ok;
  std::string lo, hi;
  bool has_hi;
  scan_bounds(spec, &lo, &has_hi, &hi);
  if (has_hi && hi <= lo) return Status::Ok;
  const KeyMap& km = nit->second.keys;
  ++scanning_;
  Status result = Status::Ok;
  size_t visited = 0;
  Bytes val;
  auto emit = [&](const KeyMap::value_type& e) -> bool {
    if (!spec.keys_only) {
      Status s = read_value(e.second, &val);
      if (s == Status::Corruption) {
        ++counters_.corrupt_reads;
        result = Status::Corruption;
        return true;  // skip, keep going
      }
      if (s != Status::Ok) {
        result = s;
        return false;
      }
    } else {
      val.clear();
    }
    ++visited;
    if (!visit(bv(e.first), ByteView(val), e.second.seq)) return false;
    return spec.limit == 0 || visited < spec.limit;
  };
  if (!spec.reverse) {
    for (auto it = km.lower_bound(lo); it != km.end(); ++it) {
      if (has_hi && std::string_view(it->first) >= std::string_view(hi)) break;
      if (!emit(*it)) break;
    }
  } else {
    auto it = has_hi ? km.lower_bound(hi) : km.end();
    while (it != km.begin()) {
      --it;
      if (std::string_view(it->first) < std::string_view(lo)) break;
      if (!emit(*it)) break;
    }
  }
  --scanning_;
  return result;
}

Status KvStore::scan(std::string_view ns, const ScanSpec& spec, std::vector<KvEntry>* out) {
  out->clear();
  return scan(ns, spec, [out](ByteView k, ByteView v, uint64_t seq) {
    KvEntry e;
    e.key = k.to_bytes();
    e.value = v.to_bytes();
    e.seq = seq;
    out->push_back(std::move(e));
    return true;
  });
}

uint64_t KvStore::count(std::string_view ns, const ScanSpec& spec) const {
  auto nit = index_.find(ns);
  if (nit == index_.end()) return 0;
  std::string lo, hi;
  bool has_hi;
  scan_bounds(spec, &lo, &has_hi, &hi);
  if (has_hi && hi <= lo) return 0;
  const KeyMap& km = nit->second.keys;
  auto a = km.lower_bound(lo);
  auto b = has_hi ? km.lower_bound(hi) : km.end();
  return static_cast<uint64_t>(std::distance(a, b));
}

// ---------------------------------------------------------------------------------------------
// Writes
// ---------------------------------------------------------------------------------------------

Status KvStore::write_check() const {
  if (closed_ || broken_) return Status::Broken;
  if (opts_.read_only) return Status::ReadOnly;
  if (scanning_) return Status::Busy;
  return Status::Ok;
}

Status KvStore::put(std::string_view ns, ByteView key, ByteView value, WriteOptions wo) {
  WriteBatch b;
  b.put(ns, key, value);
  return write(b, wo);
}

Status KvStore::del(std::string_view ns, ByteView key, WriteOptions wo) {
  WriteBatch b;
  b.del(ns, key);
  return write(b, wo);
}

Status KvStore::write(const WriteBatch& batch, WriteOptions wo, uint64_t* first_seq) {
  Status s = write_check();
  if (s != Status::Ok) return s;
  if (batch.bad_name_ || batch.max_value_ > opts_.max_value_size ||
      batch.rep_.size() > opts_.max_batch_bytes)
    return Status::TooLarge;
  if (batch.empty()) {
    if (first_seq) *first_seq = last_seq_ + 1;
    return Status::Ok;
  }
  uint64_t seq = last_seq_ + 1;
  ByteWriter w(batch.rep_.size() + 10);
  w.varint(batch.count_).bytes(ByteView(batch.rep_));
  Bytes pt = w.take();
  FileCtx ctx{salt_, gen_, crc_seed_, opts_.cipher.get(), nonce_prefix_, &nonce_ctr_};
  Bytes frame;
  if (!build_frame(ctx, fmt::kFrameBatch, seq, pt, &frame)) return Status::InvalidArgument;
  uint64_t off = file_end_;
  s = file_->write_at(off, frame);
  if (s != Status::Ok) {
    // Roll back the torn frame so the file stays a clean sequence of frames.
    if (file_->truncate(off) != Status::Ok) broken_ = true;
    return s;
  }
  file_end_ += frame.size();
  dirty_ = true;
  if (wo.sync) {
    s = file_->sync();
    if (s != Status::Ok) {
      broken_ = true;
      return s;
    }
    dirty_ = false;
  }
  uint64_t n = 0;
  uint32_t plen = static_cast<uint32_t>(frame.size() - kFrameOverhead);
  ByteView plain = opts_.cipher ? ByteView(pt) : ByteView(frame.data() + kFrameHeader, plen);
  if (!apply_payload(plain, fmt::kFrameBatch, off, plen, seq, &n)) {
    broken_ = true;  // cannot happen for a batch built by WriteBatch
    return Status::Corruption;
  }
  last_seq_ = seq + n - 1;
  if (first_seq) *first_seq = seq;
  maybe_auto_compact();
  return Status::Ok;
}

Status KvStore::sync() {
  Status s = write_check();
  if (s != Status::Ok) return s;
  if (!dirty_) return Status::Ok;
  s = file_->sync();
  if (s != Status::Ok) {
    broken_ = true;
    return s;
  }
  dirty_ = false;
  return Status::Ok;
}

void KvStore::maybe_auto_compact() {
  if (opts_.auto_compact_min_bytes == 0 || opts_.auto_compact_ratio == 0) return;
  if (file_end_ < opts_.auto_compact_min_bytes || file_end_ < next_auto_compact_) return;
  if (file_end_ <= live_estimate() * opts_.auto_compact_ratio) return;
  Status s = compact_impl(false, nullptr, params_, opts_.scrub_on_compact);
  if (s == Status::Ok) {
    ++counters_.auto_compactions;
  } else {
    next_auto_compact_ = file_end_ + file_end_ / 2;  // back off (e.g. disk full)
  }
}

Status KvStore::compact() {
  Status s = write_check();
  if (s != Status::Ok) return s;
  return compact_impl(false, nullptr, params_, opts_.scrub_on_compact);
}

Status KvStore::rekey(std::unique_ptr<RecordCipher> new_cipher, ByteView new_params) {
  Status s = write_check();
  if (s != Status::Ok) return s;
  if (new_params.size() > kMaxCipherParams) return Status::InvalidArgument;
  if (new_cipher && (!opts_.entropy || new_cipher->overhead() > RecordCipher::kMaxOverhead))
    return Status::InvalidArgument;
  return compact_impl(true, std::move(new_cipher), new_params.to_bytes(), true);
}

Status KvStore::compact_impl(bool change_cipher, std::unique_ptr<RecordCipher> new_cipher,
                             const Bytes& params, bool scrub_old) {
  RecordCipher* target = change_cipher ? new_cipher.get() : opts_.cipher.get();
  const uint64_t g2 = max_gen_ + 1;
  max_gen_ = g2;  // never reuse a generation number, even if this attempt fails
  const std::string tmp = path_join(dir_, fmt::tmp_name(g2));
  const std::string fin = path_join(dir_, fmt::data_name(g2));

  fmt::Header h;
  h.version = target ? fmt::kFormatVersionBoundSnapshot : fmt::kFormatVersion;
  h.flags = target ? fmt::kFileFlagEncrypted : 0;
  h.generation = g2;
  h.base_seq = last_seq_;
  fill_random(h.salt, 16);
  h.params = params;
  uint8_t prefix[16];
  fill_random(prefix, 16);
  uint64_t ctr = 0;
  if (target) {
    uint8_t nonce[RecordCipher::kNonceSize];
    std::memcpy(nonce, prefix, 16);
    put_le(nonce + 16, ctr++, 8);
    if (!make_keycheck(target, h, nonce, &h.keycheck)) return Status::InvalidArgument;
    if (!verify_keycheck(target, h)) return Status::InvalidArgument;  // cipher self-test
  }
  const uint32_t seed = fmt::crc32(ByteView(h.salt, 16));
  FileCtx ctx{h.salt, g2, seed, target, prefix, &ctr};

  std::unique_ptr<VfsFile> f;
  Status s = vfs_->open(tmp, OpenMode::CreateTruncate, &f);
  if (s != Status::Ok) return s;
  auto abandon = [&](Status e) {
    f.reset();
    scrub_file(*vfs_, tmp, true);  // may hold cleartext (re-key to plaintext); best effort
    vfs_->remove(tmp);
    return e;
  };
  Bytes hdr = fmt::encode_header(h);
  Bytes both = hdr;
  both.insert(both.end(), hdr.begin(), hdr.end());
  s = f->write_at(0, both);
  if (s != Status::Ok) return abandon(s);

  uint64_t off = kDataStart;
  std::vector<Loc> nl;
  nl.reserve(static_cast<size_t>(records_));
  std::vector<size_t> pend;  // indices into nl of records in the current chunk
  Bytes body, frame, val;
  uint64_t chunks = 0, written = 0, dropped = 0;
  uint32_t rcrc = 0;  // running CRC over chunk payloads (v2 commit AAD)
  auto flush_chunk = [&]() -> Status {
    if (pend.empty()) return Status::Ok;
    ByteWriter w(body.size() + 10);
    w.varint(pend.size());
    uint32_t hl = static_cast<uint32_t>(w.size());
    w.bytes(ByteView(body));
    Bytes pt = w.take();
    Bytes extra;
    if (target) {
      if (ctr != chunks + 1) return Status::InvalidArgument;  // ordinal == nonce counter - 1
      extra = snap_extra(chunks, false, 0);
    }
    if (!build_frame(ctx, fmt::kFrameSnapChunk, last_seq_, pt, &frame, ByteView(extra)))
      return Status::InvalidArgument;
    Status ws = f->write_at(off, frame);
    if (ws != Status::Ok) return ws;
    uint32_t plen = static_cast<uint32_t>(frame.size() - kFrameOverhead);
    rcrc = fmt::crc32(ByteView(frame.data() + kFrameHeader, plen), rcrc);
    for (size_t i : pend) {
      nl[i].frame_off = off;
      nl[i].payload_len = plen;
      nl[i].val_off += hl;
    }
    off += frame.size();
    ++chunks;
    body.clear();
    pend.clear();
    return Status::Ok;
  };
  for (const auto& nse : index_) {
    for (const auto& e : nse.second.keys) {
      Status rs = read_value(e.second, &val);
      if (rs == Status::Corruption) {
        nl.push_back(Loc());  // frame_off 0 marks "dropped"
        ++dropped;
        continue;
      }
      if (rs != Status::Ok) return abandon(rs);
      size_t rec = nse.first.size() + e.first.size() + val.size() + 32;
      if (!pend.empty() && body.size() + rec > opts_.snapshot_chunk_bytes) {
        s = flush_chunk();
        if (s != Status::Ok) return abandon(s);
      }
      ByteWriter w;
      w.varint(nse.first.size()).str(nse.first).varint(e.first.size()).str(e.first);
      w.varint(e.second.seq).varint(val.size());
      body.insert(body.end(), w.data().begin(), w.data().end());
      Loc l;
      l.seq = e.second.seq;
      l.val_off = static_cast<uint32_t>(body.size());
      l.val_len = static_cast<uint32_t>(val.size());
      l.val_crc = fmt::crc32(ByteView(val));
      body.insert(body.end(), val.begin(), val.end());
      nl.push_back(l);
      pend.push_back(nl.size() - 1);
      ++written;
    }
  }
  s = flush_chunk();
  if (s != Status::Ok) return abandon(s);
  {
    ByteWriter w;
    w.varint(written).varint(chunks);
    Bytes extra;
    if (target) {
      if (ctr != chunks + 1) return abandon(Status::InvalidArgument);
      extra = snap_extra(chunks, true, rcrc);
    }
    if (!build_frame(ctx, fmt::kFrameSnapCommit, last_seq_, ByteView(w.data()), &frame, ByteView(extra)))
      return abandon(Status::InvalidArgument);
    s = f->write_at(off, frame);
    if (s != Status::Ok) return abandon(s);
    off += frame.size();
  }
  s = f->sync();
  if (s != Status::Ok) return abandon(s);
  f.reset();  // must be closed before rename on Windows

  s = vfs_->rename(tmp, fin);
  if (s != Status::Ok) {
    scrub_file(*vfs_, tmp, true);
    vfs_->remove(tmp);
    return s;
  }
  s = vfs_->sync_dir(dir_);
  if (s != Status::Ok) {
    // The new generation may or may not be durable: writing more to the old file could be lost.
    broken_ = true;
    return s;
  }
  s = vfs_->open(fin, OpenMode::ReadWrite, &f);
  if (s != Status::Ok) {
    broken_ = true;
    return s;
  }

  // Switch to the new generation.
  const std::string old_path = file_path_;
  const bool had_old = file_ != nullptr;
  // A plaintext generation holds deleted / overwritten values in the clear: always scrub it.
  // An encrypted one only holds ciphertext; it is scrubbed on re-key or with scrub_on_compact.
  const bool old_plain = opts_.cipher == nullptr;
  file_ = std::move(f);  // closes the old file
  file_path_ = fin;
  gen_ = g2;
  ver_ = h.version;
  base_seq_ = last_seq_;
  std::memcpy(salt_, h.salt, 16);
  crc_seed_ = seed;
  params_ = params;
  std::memcpy(nonce_prefix_, prefix, 16);
  nonce_ctr_ = ctr;
  file_end_ = off;
  if (change_cipher) opts_.cipher = std::move(new_cipher);
  cache_valid_ = false;
  cache_pt_.clear();
  size_t i = 0;
  std::vector<std::pair<std::string, std::string>> lost;
  for (auto& nse : index_) {
    for (auto& e : nse.second.keys) {
      const Loc& l = nl[i++];
      if (l.frame_off == 0)
        lost.emplace_back(nse.first, e.first);
      else
        e.second = l;
    }
  }
  for (const auto& p : lost) erase_key(p.first, p.second);
  counters_.dropped_by_compaction += dropped;
  ++counters_.compactions;
  dirty_ = false;
  next_auto_compact_ = 0;
  if (had_old) {
    if (scrub_old || old_plain) scrub_file(*vfs_, old_path, true);
    vfs_->remove(old_path);
    vfs_->sync_dir(dir_);  // best effort; a leftover old generation is removed at next open
  }
  return Status::Ok;
}

// ---------------------------------------------------------------------------------------------
// Introspection
// ---------------------------------------------------------------------------------------------

NamespaceStats KvStore::namespace_stats(std::string_view ns) const {
  auto nit = index_.find(ns);
  return nit == index_.end() ? NamespaceStats() : nit->second.st;
}

std::vector<std::string> KvStore::namespaces() const {
  std::vector<std::string> out;
  for (const auto& kv : index_) out.push_back(kv.first);
  return out;
}

StoreStats KvStore::stats() const {
  StoreStats s = counters_;
  s.generation = gen_;
  s.last_seq = last_seq_;
  s.records = records_;
  s.namespaces = index_.size();
  s.live_bytes = live_bytes_;
  s.file_bytes = file_end_;
  s.encrypted = opts_.cipher != nullptr;
  s.read_only = opts_.read_only;
  s.broken = broken_;
  return s;
}

}  // namespace ember::store
