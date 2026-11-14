// KvStore crash-consistency harness.
//
// A scripted workload (batches with and without sync, explicit syncs, compactions, automatic
// compactions, and — for the encrypted variant — a re-key) is first run without faults to count
// its mutating VFS operations and written bytes. It is then re-run once per crash point:
//   * a power cut before every single mutating op (write/sync/truncate/rename/remove/sync_dir/
//     create), each under three crash models (drop unsynced, keep all, seeded random prefix+junk);
//   * a power cut tearing the write stream at sampled byte offsets (every byte in the soak test);
//   * a second power cut during the recovery of the first (double crash).
// After each crash the store is reopened and must satisfy:
//   (1) open succeeds and the recovery report is clean (a crash never looks like corruption);
//   (2) the content equals the model state after exactly j batches, for some j between the number
//       of batches acknowledged as durable and the number attempted (no partial batch, no loss of
//       acknowledged-synced batches);
//   (3) the store is usable: a new synced write survives a clean reopen.
#include "store_test_util.h"

using namespace store_test;

namespace {

struct Op {
  int kind;  // 0 put, 1 del, 2 range, 3 drop ns
  std::string ns, k, v;
};

struct Step {
  enum Kind { kBatch, kSync, kCompact, kRekey } kind = kBatch;
  std::vector<Op> ops;
  bool sync = true;
};

Step ctl(Step::Kind k) {
  Step s;
  s.kind = k;
  return s;
}

struct Workload {
  std::vector<Step> steps;
  std::vector<Model> states;  // states[i] = content after i successful batches
};

void apply_ops(Model& m, const std::vector<Op>& ops) {
  for (const auto& o : ops) {
    auto& t = m[o.ns];
    switch (o.kind) {
      case 0: t[o.k] = o.v; break;
      case 1: t.erase(o.k); break;
      case 2:
        for (auto it = t.lower_bound(o.k); it != t.end() && it->first < o.v;) it = t.erase(it);
        break;
      case 3: t.clear(); break;
    }
    if (t.empty()) m.erase(o.ns);
  }
}

Workload make_workload(uint64_t seed, bool enc, int batches) {
  Rng r(seed);
  Workload w;
  w.states.push_back(Model());
  const char* nss[] = {"msg", "cfg", "dtn"};
  for (int i = 0; i < batches; ++i) {
    Step s;
    s.kind = Step::kBatch;
    s.sync = r.below(3) != 0;
    size_t n = 1 + r.below(4);
    for (size_t j = 0; j < n; ++j) {
      Op o;
      o.ns = nss[r.below(3)];
      o.k = "k" + std::to_string(r.below(25));
      uint64_t kind = r.below(20);
      if (kind < 14) {
        o.kind = 0;
        o.v = std::to_string(i) + ":" + std::string(r.below(7) == 0 ? 900 : r.below(60), 'a' + i % 26);
      } else if (kind < 18) {
        o.kind = 1;
      } else if (kind < 19) {
        o.kind = 2;
        o.v = "k" + std::to_string(r.below(25));
      } else {
        o.kind = 3;
      }
      s.ops.push_back(o);
    }
    w.steps.push_back(s);
    Model m = w.states.back();
    apply_ops(m, s.ops);
    w.states.push_back(m);
    if (i % 9 == 4) w.steps.push_back(ctl(Step::kSync));
    if (i % 13 == 7) w.steps.push_back(ctl(Step::kCompact));
    if (enc && i == batches / 2) w.steps.push_back(ctl(Step::kRekey));
  }
  return w;
}

uint64_t g_entropy_seed = 1000;

Options opts_for(bool enc, const std::string& key) {
  Options o = enc ? enc_opts(key, ++g_entropy_seed) : plain_opts();
  o.auto_compact_min_bytes = 3000;  // exercise automatic compaction inside write()
  o.auto_compact_ratio = 2;
  o.snapshot_chunk_bytes = 300;     // several snapshot chunks per compaction
  return o;
}

struct RunResult {
  size_t floor = 0;      // batches acknowledged as durable
  size_t attempted = 0;  // batches that may be present
  bool completed = false;
};

RunResult run(MemVfs& vfs, const Workload& w, bool enc) {
  RunResult r;
  std::unique_ptr<KvStore> db;
  if (KvStore::open(&vfs, "db", opts_for(enc, "key1"), &db) != Status::Ok) return r;
  size_t applied = 0;
  for (const Step& s : w.steps) {
    Status st = Status::Ok;
    switch (s.kind) {
      case Step::kBatch: {
        WriteBatch b;
        for (const auto& o : s.ops) {
          if (o.kind == 0) b.put(o.ns, B(o.k), B(o.v));
          if (o.kind == 1) b.del(o.ns, B(o.k));
          if (o.kind == 2) b.delete_range(o.ns, B(o.k), B(o.v));
          if (o.kind == 3) b.drop_namespace(o.ns);
        }
        st = db->write(b, WriteOptions{s.sync});
        if (st != Status::Ok) {
          r.attempted = applied + 1;
          return r;
        }
        ++applied;
        if (s.sync) r.floor = applied;
        break;
      }
      case Step::kSync: st = db->sync(); break;
      case Step::kCompact: st = db->compact(); break;
      case Step::kRekey: {
        std::unique_ptr<RecordCipher> c(new InsecureTestCipher("key2"));
        st = db->rekey(std::move(c), B("key2"));
        break;
      }
    }
    if (st != Status::Ok) {
      r.attempted = applied;
      return r;
    }
    if (s.kind != Step::kBatch) r.floor = applied;
    r.attempted = applied;
  }
  r.completed = true;
  return r;
}

// Opens after a crash with whichever key the directory says applies.
std::unique_ptr<KvStore> reopen(MemVfs& vfs, bool enc, RecoveryReport* rep, const std::string& ctx) {
  std::string key = "key1";
  ProbeInfo pi;
  REQUIRE(KvStore::probe(vfs, "db", &pi) == Status::Ok);
  if (pi.exists) {
    CHECK(pi.encrypted == enc);
    if (enc) key = S(pi.cipher_params);
  }
  std::unique_ptr<KvStore> db;
  Status s = KvStore::open(&vfs, "db", opts_for(enc, key), &db, rep);
  INFO(ctx);
  REQUIRE_MESSAGE(s == Status::Ok, status_name(s));
  if (enc && pi.exists) {  // the other key must not open it
    std::unique_ptr<KvStore> other;
    CHECK(KvStore::open(&vfs, "db", opts_for(enc, key == "key1" ? "key2" : "key1"), &other) ==
          Status::WrongKey);
  }
  return db;
}

// Checks invariants (1)-(3). Returns the recovered batch count j.
size_t verify(MemVfs& vfs, const Workload& w, const RunResult& r, bool enc, const std::string& ctx) {
  INFO(ctx);
  RecoveryReport rep;
  auto db = reopen(vfs, enc, &rep, ctx);
  CHECK_MESSAGE(rep.clean(), "regions=" << rep.corrupt_regions << " rejected=" << rep.rejected_frames
                                        << " lost=" << rep.lost_ops << " snap=" << rep.snapshot_incomplete);
  Model got = dump(*db);
  size_t j = SIZE_MAX;
  for (size_t i = r.attempted + 1; i-- > r.floor;) {
    if (i < w.states.size() && w.states[i] == got) {
      j = i;
      break;
    }
  }
  REQUIRE_MESSAGE(j != SIZE_MAX, "floor=" << r.floor << " attempted=" << r.attempted);
  // (3) usable
  REQUIRE(db->put("post", B("crash"), B("ok")) == Status::Ok);
  db.reset();
  db = reopen(vfs, enc, &rep, ctx + " (post)");
  CHECK(rep.clean());
  CHECK(rep.torn_tail_bytes == 0);
  Model want = w.states[j];
  want["post"]["crash"] = "ok";
  CHECK(dump(*db) == want);
  return j;
}

struct Totals {
  uint64_t op_points = 0, byte_points = 0, double_points = 0;
};

void crash_campaign(uint64_t seed, bool enc, int batches, uint64_t byte_stride, uint64_t double_stride,
                    Totals* tot) {
  Workload w = make_workload(seed, enc, batches);
  uint64_t n_ops, n_bytes;
  {
    MemVfs vfs;
    RunResult r = run(vfs, w, enc);
    REQUIRE(r.completed);
    REQUIRE(r.floor == static_cast<size_t>(batches));
    n_ops = vfs.op_count();
    n_bytes = vfs.bytes_written();
    std::unique_ptr<KvStore> db;
    vfs.reboot(CrashMode::DropUnsynced);
    auto d = reopen(vfs, enc, nullptr, "baseline");
    CHECK(dump(*d) == w.states.back());
    CHECK(d->stats().compactions == 0);
  }
  MESSAGE("workload seed=" << seed << " enc=" << enc << ": " << n_ops << " ops, " << n_bytes << " bytes");
  const CrashMode modes[] = {CrashMode::DropUnsynced, CrashMode::KeepAll, CrashMode::Random};
  for (uint64_t k = 0; k <= n_ops; ++k) {
    for (int mi = 0; mi < 3; ++mi) {
      MemVfs vfs;
      vfs.crash_after_ops(static_cast<int64_t>(k));
      RunResult r = run(vfs, w, enc);
      if (k == n_ops) CHECK(r.completed);  // (k < n_ops may still complete: crash in close()/cleanup)
      vfs.reboot(modes[mi], seed * 1000003 + k);
      verify(vfs, w, r, enc, "op crash k=" + std::to_string(k) + " mode=" + std::to_string(mi));
      ++tot->op_points;
    }
    if (double_stride && k % double_stride == 0) {
      for (int64_t m = 0; m < 8; ++m) {
        MemVfs vfs;
        vfs.crash_after_ops(static_cast<int64_t>(k));
        RunResult r = run(vfs, w, enc);
        vfs.reboot(CrashMode::Random, k * 31 + static_cast<uint64_t>(m));
        vfs.crash_after_ops(m);  // second power cut during recovery
        {
          std::unique_ptr<KvStore> db;
          std::string key = "key1";
          ProbeInfo pi;
          if (KvStore::probe(vfs, "db", &pi) == Status::Ok && pi.exists && enc) key = S(pi.cipher_params);
          KvStore::open(&vfs, "db", opts_for(enc, key), &db);
        }
        vfs.reboot(CrashMode::Random, k * 77 + static_cast<uint64_t>(m));
        verify(vfs, w, r, enc, "double crash k=" + std::to_string(k) + " m=" + std::to_string(m));
        ++tot->double_points;
      }
    }
  }
  for (uint64_t b = 0; b <= n_bytes; b += byte_stride) {
    for (int mi = 1; mi < 3; ++mi) {
      MemVfs vfs;
      vfs.crash_after_bytes(static_cast<int64_t>(b));
      RunResult r = run(vfs, w, enc);
      vfs.reboot(modes[mi], b * 13 + seed);
      verify(vfs, w, r, enc, "byte crash b=" + std::to_string(b) + " mode=" + std::to_string(mi));
      ++tot->byte_points;
    }
  }
}

}  // namespace

TEST_SUITE("store") {

TEST_CASE("crash: every op and sampled byte offsets, plaintext") {
  Totals t;
  crash_campaign(42, false, 36, 37, 6, &t);
  MESSAGE("crash points: ops=" << t.op_points << " bytes=" << t.byte_points << " double=" << t.double_points);
  CHECK(t.op_points > 100);
}

TEST_CASE("crash: every op and sampled byte offsets, encrypted with re-key") {
  Totals t;
  crash_campaign(4242, true, 30, 67, 9, &t);
  MESSAGE("crash points: ops=" << t.op_points << " bytes=" << t.byte_points << " double=" << t.double_points);
  CHECK(t.op_points > 100);
}

TEST_CASE("crash soak: every byte offset, many seeds" * doctest::skip()) {
  Totals t;
  for (uint64_t seed = 1; seed <= 6; ++seed) {
    crash_campaign(seed, seed % 2 == 0, 60, 1, 1, &t);
  }
  MESSAGE("soak crash points: ops=" << t.op_points << " bytes=" << t.byte_points << " double=" << t.double_points);
}

}  // TEST_SUITE
