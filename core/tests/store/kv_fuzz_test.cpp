// KvStore: seeded mutation fuzzing of real store data files (security review: "store mutation
// fuzzing not covered"). For each seed a random workload writes a plaintext or encrypted store and
// records every value ever written per key; then the data file is mutated (bit flips, byte sets,
// truncation, insert/delete, splices, duplicated / dropped / reordered frames, zeroed ranges,
// forged frame headers with a valid header CRC, header field edits with the header CRC repaired)
// and reopened through a read-counting VFS. Invariants:
//   * open() terminates within a deterministic work bound (bytes read from the VFS, mutating ops);
//   * open() returns Ok or one of the documented failure statuses;
//   * every value returned by get()/scan() is one that was written for that key at some point;
//   * a store that opened read-write still accepts a write and reopens with it.
// Fast version runs in the default suite; the long soak:  ./ember_tests -ts=store -tc="fuzz soak*" --no-skip
#include <algorithm>
#include <set>

#include "store/kv_format.h"
#include "store_test_util.h"

using namespace store_test;

namespace {

using History = std::map<std::string, std::map<std::string, std::set<std::string>>>;

void put_le(uint8_t* p, uint64_t v, int n) {
  for (int i = 0; i < n; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
uint32_t get_le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }

struct Span {
  size_t off, len;
};

// Best-effort frame walk of an undamaged file.
std::vector<Span> frames_of(const Bytes& d) {
  std::vector<Span> out;
  size_t off = fmt::kDataStart;
  while (off + fmt::kFrameOverhead <= d.size() && d[off] == fmt::kFrameMagic0 && d[off + 1] == fmt::kFrameMagic1) {
    size_t len = get_le32(&d[off + 4]);
    if (len > d.size() - off - fmt::kFrameOverhead) break;
    out.push_back(Span{off, fmt::kFrameOverhead + len});
    off += fmt::kFrameOverhead + len;
  }
  return out;
}

std::string data_path(MemVfs& v) {
  std::string p;
  uint64_t best = 0;
  for (const auto& f : v.all_files()) {
    char kind;
    uint64_t g;
    if (f.compare(0, 3, "db/") == 0 && fmt::parse_name(f.substr(3), &kind, &g) && kind == 'd' && g >= best) {
      best = g;
      p = f;
    }
  }
  return p;
}

Options opts_for(bool enc, uint64_t seed) {
  Options o = enc ? enc_opts("fuzz-key", seed) : plain_opts();
  o.snapshot_chunk_bytes = 256;
  return o;
}

// Random workload; returns every value ever written per (ns, key).
History workload(MemVfs& vfs, bool enc, Rng& r) {
  History h;
  auto db = open_ok(&vfs, "db", opts_for(enc, r.next()));
  int ops = 40 + static_cast<int>(r.below(80));
  for (int i = 0; i < ops; ++i) {
    std::string ns = r.below(4) ? "a" : "b";
    std::string k = "k" + std::to_string(r.below(30));
    uint64_t what = r.below(20);
    if (what < 12) {
      std::string v = "v" + std::to_string(i) + "-" + std::string(r.below(3) ? r.below(40) : r.below(900), char('a' + r.below(26)));
      CHECK(db->put(ns, B(k), B(v), WriteOptions{r.below(2) == 0}) == Status::Ok);
      h[ns][k].insert(v);
    } else if (what < 15) {
      CHECK(db->del(ns, B(k)) == Status::Ok);
    } else if (what < 18) {
      WriteBatch b;
      int n = 1 + static_cast<int>(r.below(5));
      for (int j = 0; j < n; ++j) {
        std::string kk = "k" + std::to_string(r.below(30));
        std::string v = "b" + std::to_string(i) + "." + std::to_string(j);
        b.put(ns, B(kk), B(v));
        h[ns][kk].insert(v);
      }
      if (r.below(3) == 0) b.delete_range(ns, B("k1"), B("k2"));
      CHECK(db->write(b) == Status::Ok);
    } else if (what == 18) {
      CHECK(db->compact() == Status::Ok);
    } else {
      CHECK(db->sync() == Status::Ok);
    }
  }
  CHECK(db->close() == Status::Ok);
  return h;
}

void fix_header_crc(Bytes& d, size_t at) {
  if (d.size() < at + fmt::kHeaderSize) return;
  put_le(&d[at + 508], fmt::crc32(ByteView(d.data() + at, 508)), 4);
}

void mutate(Bytes& d, Rng& r) {
  const size_t hs = fmt::kHeaderSize;
  int rounds = 1 + static_cast<int>(r.below(3));
  for (int k = 0; k < rounds && !d.empty(); ++k) {
    auto pos = [&]() { return static_cast<size_t>(r.below(d.size())); };
    auto data_pos = [&]() {
      return d.size() > fmt::kDataStart ? fmt::kDataStart + static_cast<size_t>(r.below(d.size() - fmt::kDataStart)) : pos();
    };
    switch (r.below(14)) {
      case 0: d[data_pos()] ^= static_cast<uint8_t>(1u << r.below(8)); break;
      case 1: d[pos()] = static_cast<uint8_t>(r.next()); break;
      case 2: d.resize(static_cast<size_t>(r.below(d.size() + 1))); break;
      case 3: {  // insert junk
        size_t at = data_pos(), n = 1 + static_cast<size_t>(r.below(64));
        Bytes j = r.bytes(n);
        d.insert(d.begin() + static_cast<std::ptrdiff_t>(at), j.begin(), j.end());
        break;
      }
      case 4: {  // delete a range
        size_t at = data_pos(), n = std::min<size_t>(d.size() - at, 1 + r.below(200));
        d.erase(d.begin() + static_cast<std::ptrdiff_t>(at), d.begin() + static_cast<std::ptrdiff_t>(at + n));
        break;
      }
      case 5: {  // zero a range
        size_t at = data_pos(), n = std::min<size_t>(d.size() - at, 1 + r.below(300));
        std::fill(d.begin() + static_cast<std::ptrdiff_t>(at), d.begin() + static_cast<std::ptrdiff_t>(at + n), 0);
        break;
      }
      case 6: {  // splice a copy of another region
        size_t from = pos(), n = std::min<size_t>(d.size() - from, 1 + r.below(400));
        Bytes c(d.begin() + static_cast<std::ptrdiff_t>(from), d.begin() + static_cast<std::ptrdiff_t>(from + n));
        size_t at = data_pos();
        d.insert(d.begin() + static_cast<std::ptrdiff_t>(at), c.begin(), c.end());
        break;
      }
      case 7: case 8: {  // frame-level: duplicate, drop or swap whole frames
        auto fr = frames_of(d);
        if (fr.size() < 2) break;
        size_t a = static_cast<size_t>(r.below(fr.size())), b = static_cast<size_t>(r.below(fr.size()));
        Bytes fa(d.begin() + static_cast<std::ptrdiff_t>(fr[a].off), d.begin() + static_cast<std::ptrdiff_t>(fr[a].off + fr[a].len));
        uint64_t op = r.below(3);
        Bytes out(d.begin(), d.begin() + fmt::kDataStart);
        for (size_t i = 0; i < fr.size(); ++i) {
          const Span& s = (op == 2 && i == a) ? fr[b] : (op == 2 && i == b) ? fr[a] : fr[i];
          if (op == 1 && i == a) continue;
          out.insert(out.end(), d.begin() + static_cast<std::ptrdiff_t>(s.off), d.begin() + static_cast<std::ptrdiff_t>(s.off + s.len));
          if (op == 0 && i == b) out.insert(out.end(), fa.begin(), fa.end());
        }
        size_t tail = fr.back().off + fr.back().len;
        out.insert(out.end(), d.begin() + static_cast<std::ptrdiff_t>(tail), d.end());
        d.swap(out);
        break;
      }
      case 9: case 10: {  // forged frame headers (valid header CRC, payload CRC wrong)
        fmt::Header h;
        if (!fmt::decode_header(ByteView(d), &h)) break;
        uint32_t seed = fmt::crc32(ByteView(h.salt, 16));
        bool big = r.below(4) == 0;
        if (big) d.resize(d.size() + 64 * 1024 + static_cast<size_t>(r.below(384 * 1024)), 0);
        int n = 1 + static_cast<int>(r.below(big ? 20000 : 8));
        for (int i = 0; i < n; ++i) {
          size_t at = data_pos();
          if (at + fmt::kFrameOverhead > d.size()) break;
          uint8_t* p = &d[at];
          p[0] = fmt::kFrameMagic0;
          p[1] = fmt::kFrameMagic1;
          p[2] = static_cast<uint8_t>(1 + r.below(3));
          p[3] = static_cast<uint8_t>(r.below(2));
          uint64_t room = d.size() - at - fmt::kFrameOverhead;
          put_le(p + 4, r.below(3) ? room : r.below(room + 1), 4);
          put_le(p + 8, r.below(2) ? r.below(400) : r.next(), 8);
          put_le(p + 16, fmt::crc32(ByteView(p, 16), seed), 4);
        }
        break;
      }
      case 11: case 12: {  // header field edits with a valid CRC (copy A, B or both)
        if (d.size() < fmt::kDataStart) break;
        static const size_t fields[] = {8, 12, 16, 24, 32, 48, 306};
        size_t f = fields[r.below(7)];
        uint64_t v = r.below(2) ? r.below(4) : r.next();
        for (size_t at : {size_t(0), hs}) {
          if (r.below(3) == 0) continue;
          put_le(&d[at + f], v, f == 48 || f == 306 ? 2 : (f == 8 || f == 12) ? 4 : 8);
          fix_header_crc(d, at);
        }
        break;
      }
      default:  // smash one header copy
        d[static_cast<size_t>(r.below(std::min<size_t>(d.size(), fmt::kDataStart)))] ^= 0xFF;
        break;
    }
  }
}

struct FuzzStats {
  uint64_t runs = 0, opened = 0, values_checked = 0, max_read_ratio_x100 = 0;
};

void check_values(KvStore& db, const History& hist) {
  Model got = dump(db);
  for (const auto& ns : got)
    for (const auto& kv : ns.second) {
      auto a = hist.find(ns.first);
      REQUIRE_MESSAGE(a != hist.end(), "unknown namespace " << ns.first);
      auto b = a->second.find(kv.first);
      REQUIRE_MESSAGE(b != a->second.end(), "unknown key " << kv.first);
      REQUIRE_MESSAGE(b->second.count(kv.second) == 1, "wrong value for " << kv.first);
    }
  for (const auto& ns : hist)
    for (const auto& kv : ns.second) {
      Bytes v;
      Status s = db.get(ns.first, B(kv.first), &v);
      REQUIRE((s == Status::Ok || s == Status::NotFound || s == Status::Corruption));
      if (s == Status::Ok) REQUIRE(kv.second.count(S(v)) == 1);
    }
}

void run_campaign(uint64_t seeds, uint64_t mutations_per_seed, uint64_t base_seed, FuzzStats* st) {
  for (uint64_t sd = 0; sd < seeds; ++sd) {
    Rng r(base_seed * 1000003 + sd);
    bool enc = (sd & 1) != 0;
    MemVfs src;
    History hist = workload(src, enc, r);
    std::string path = data_path(src);
    REQUIRE(!path.empty());
    const Bytes orig = src.contents(path);
    for (uint64_t m = 0; m < mutations_per_seed; ++m) {
      Bytes d = orig;
      mutate(d, r);
      MemVfs mem;
      mem.mkdir("db");
      mem.set_contents(path, ByteView(d));
      if (r.below(8) == 0) mem.set_contents("db/tmp-99.ekv", ByteView(d));  // leftovers
      CountingVfs v(&mem);
      bool ro = r.below(3) == 0;
      Options o = opts_for(enc, r.next());
      o.read_only = ro;
      if (r.below(10) == 0) o.on_corruption = OnCorruption::Fail;
      std::unique_ptr<KvStore> db;
      RecoveryReport rep;
      uint64_t ops0 = mem.op_count();
      Status s = KvStore::open(&v, "db", std::move(o), &db, &rep);
      ++st->runs;
      // Work bound: a few passes over the file, independent of its contents.
      uint64_t total = d.size() * (mem.exists("db/tmp-99.ekv") ? 2 : 1);
      CAPTURE(sd);
      CAPTURE(m);
      REQUIRE(v.read_bytes <= 8 * total + (uint64_t(2) << 20));
      REQUIRE(v.reads <= 64 + total / 64);  // window refills + direct payload reads, never per byte
      REQUIRE(mem.op_count() - ops0 <= 64);
      if (total) st->max_read_ratio_x100 = std::max<uint64_t>(st->max_read_ratio_x100, v.read_bytes * 100 / total);
      REQUIRE_MESSAGE((s == Status::Ok || s == Status::Corruption || s == Status::InvalidArgument ||
                       s == Status::Locked || s == Status::WrongKey || s == Status::NotFound),
                      status_name(s));
      if (s != Status::Ok) continue;
      ++st->opened;
      check_values(*db, hist);
      st->values_checked += db->stats().records;
      if (!ro && r.below(3) == 0) {
        Status ws = db->put("a", B("k0"), B("after-fuzz"));
        if (ws == Status::Ok) {
          if (r.below(2)) {
            Status cs = db->compact();
            REQUIRE((cs == Status::Ok || cs == Status::InvalidArgument));
          }
          db.reset();
          hist["a"]["k0"].insert("after-fuzz");
          std::unique_ptr<KvStore> db2;
          Status s2 = KvStore::open(&mem, "db", opts_for(enc, r.next()), &db2);
          REQUIRE_MESSAGE(s2 == Status::Ok, status_name(s2));
          Bytes val;
          REQUIRE(db2->get("a", B("k0"), &val) == Status::Ok);
          CHECK(S(val) == "after-fuzz");
          check_values(*db2, hist);
          hist["a"]["k0"].erase("after-fuzz");
        }
      }
    }
  }
}

}  // namespace

TEST_SUITE("store") {

TEST_CASE("fuzz: mutated data files never hang open and never return wrong data") {
  FuzzStats st;
  run_campaign(6, 160, 1, &st);
  MESSAGE("store fuzz: " << st.runs << " opens, " << st.opened << " ok, " << st.values_checked
                         << " records checked, max read/size " << st.max_read_ratio_x100 / 100.0);
  CHECK(st.opened > st.runs / 10);
}

TEST_CASE("fuzz soak: mutated data files, many seeds" * doctest::skip()) {
  FuzzStats st;
  run_campaign(60, 3000, 7, &st);
  MESSAGE("store fuzz soak: " << st.runs << " opens, " << st.opened << " ok, " << st.values_checked
                              << " records checked, max read/size " << st.max_read_ratio_x100 / 100.0);
}

}  // TEST_SUITE
