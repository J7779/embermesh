// guidepack eval: MRR and recall@k of keyword / meaning / hybrid ranking on a labelled query set,
// per query type and split, plus the extractive answer hit rate of `ask`, query latency, and
// threshold checks for CI. Exit status: 0 pass, 1 a threshold failed, 2 usage or input error.
#include "eval.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "guides/library.h"

using namespace ember;
using namespace ember::guides;

namespace {

struct Query {
  std::string split, type, text;
  std::set<std::string> relevant;
  int line = 0;
};

struct Stat {
  double rr = 0;
  int hit1 = 0, hit5 = 0, hit10 = 0, n = 0;
  void add(int rank) {
    ++n;
    if (rank > 0) rr += 1.0 / rank;
    hit1 += rank == 1;
    hit5 += rank > 0 && rank <= 5;
    hit10 += rank > 0 && rank <= 10;
  }
  double mrr() const { return n ? rr / n : 0; }
  double r(int k) const { return n ? static_cast<double>(k == 1 ? hit1 : k == 5 ? hit5 : hit10) / n : 0; }
};

bool load_queries(const char* path, std::vector<Query>& out, std::string& err) {
  std::ifstream in(path, std::ios::binary);
  if (!in) { err = std::string("cannot read ") + path; return false; }
  std::string line;
  int ln = 0;
  while (std::getline(in, line)) {
    ++ln;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    std::vector<std::string> f;
    size_t p = 0;
    while (true) {
      size_t t = line.find('\t', p);
      f.push_back(line.substr(p, t == std::string::npos ? std::string::npos : t - p));
      if (t == std::string::npos) break;
      p = t + 1;
    }
    if (f.size() != 4) { err = std::string(path) + ":" + std::to_string(ln) + ": expected 4 tab-separated fields"; return false; }
    Query q;
    q.split = f[0];
    q.type = f[1];
    q.text = f[2];
    q.line = ln;
    std::istringstream ss(f[3]);
    std::string id;
    while (ss >> id) q.relevant.insert(id);
    if (q.relevant.empty()) { err = std::string(path) + ":" + std::to_string(ln) + ": no relevant guides"; return false; }
    out.push_back(std::move(q));
  }
  return true;
}

const char* mode_name(SearchMode m) {
  return m == SearchMode::kKeyword ? "keyword" : m == SearchMode::kSemantic ? "semantic" : "hybrid";
}

}  // namespace

int cmd_eval(int argc, char** argv) {
  if (argc < 1) { std::fprintf(stderr, "usage: guidepack eval <pack> --queries <tsv> [...] (see guidepack --help)\n"); return 2; }
  const char* pack_path = argv[0];
  const char* qpath = nullptr;
  const char* tpath = nullptr;
  std::string split = "all";
  std::vector<SearchMode> modes = {SearchMode::kKeyword, SearchMode::kSemantic, SearchMode::kHybrid};
  HybridParams hp;
  bool verbose = false, do_ask = true;
  for (int i = 1; i < argc; ++i) {
    auto num = [&](double& v) { if (i + 1 < argc) v = std::strtod(argv[++i], nullptr); };
    if (!std::strcmp(argv[i], "--queries") && i + 1 < argc) qpath = argv[++i];
    else if (!std::strcmp(argv[i], "--thresholds") && i + 1 < argc) tpath = argv[++i];
    else if (!std::strcmp(argv[i], "--split") && i + 1 < argc) split = argv[++i];
    else if (!std::strcmp(argv[i], "--lambda-strong")) num(hp.lambda_strong);
    else if (!std::strcmp(argv[i], "--lambda-weak")) num(hp.lambda_weak);
    else if (!std::strcmp(argv[i], "--guide-weight")) num(hp.guide_weight);
    else if (!std::strcmp(argv[i], "--pool") && i + 1 < argc) hp.semantic_pool = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    else if (!std::strcmp(argv[i], "--no-ask")) do_ask = false;
    else if (!std::strcmp(argv[i], "--modes") && i + 1 < argc) {
      modes.clear();
      std::string m = argv[++i];
      if (m.find("keyword") != std::string::npos) modes.push_back(SearchMode::kKeyword);
      if (m.find("semantic") != std::string::npos) modes.push_back(SearchMode::kSemantic);
      if (m.find("hybrid") != std::string::npos) modes.push_back(SearchMode::kHybrid);
    } else if (!std::strcmp(argv[i], "-v")) verbose = true;
    else { std::fprintf(stderr, "guidepack eval: unknown option %s\n", argv[i]); return 2; }
  }
  if (!qpath) { std::fprintf(stderr, "guidepack eval: --queries is required\n"); return 2; }
  std::vector<Query> qs;
  std::string err;
  if (!load_queries(qpath, qs, err)) { std::fprintf(stderr, "guidepack eval: %s\n", err.c_str()); return 2; }

  std::string bytes;
  {
    std::ifstream f(pack_path, std::ios::binary);
    if (!f) { std::fprintf(stderr, "guidepack eval: cannot read %s\n", pack_path); return 2; }
    bytes.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  }
  GuideLibrary lib;
  LoadStatus st = lib.load(ByteView(std::string_view(bytes)), false);
  if (st != LoadStatus::kOk) { std::fprintf(stderr, "guidepack eval: invalid pack (%s)\n", load_status_name(st)); return 2; }
  const bool sem = lib.semantic_available();
  if (!sem) {
    std::fprintf(stderr, "guidepack eval: note: no semantic section (%s); semantic/hybrid fall back to keyword\n",
                 lib.info().semantic.status.c_str());
  }
  // Unknown relevant ids are input errors (a renamed guide must not silently lower the score).
  for (const Query& q : qs)
    for (const std::string& id : q.relevant) {
      GuideMeta m;
      if (!lib.meta(id, m)) {
        std::fprintf(stderr, "%s:%d: relevant guide '%s' is not in the pack\n", qpath, q.line, id.c_str());
        return 2;
      }
    }

  std::map<std::string, Stat> stats;  // key: mode|split|type
  std::map<std::string, double> ms;   // per mode total
  std::map<std::string, int> runs;
  std::vector<std::vector<int>> ranks(qs.size(), std::vector<int>(3, 0));
  Stat ask_all, ask_by[2];
  std::map<std::string, Stat> ask_type;
  for (size_t qi = 0; qi < qs.size(); ++qi) {
    const Query& q = qs[qi];
    if (split != "all" && q.split != split) continue;
    std::string line;
    for (SearchMode m : modes) {
      auto t0 = std::chrono::steady_clock::now();
      std::vector<SearchResult> r = lib.search(q.text, "", 20, m, hp);
      ms[mode_name(m)] += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      runs[mode_name(m)]++;
      int rank = 0;
      for (size_t i = 0; i < r.size(); ++i)
        if (q.relevant.count(r[i].id)) { rank = static_cast<int>(i) + 1; break; }
      ranks[qi][m == SearchMode::kKeyword ? 0 : m == SearchMode::kSemantic ? 1 : 2] = rank;
      for (const std::string& sp : {std::string("all"), q.split})
        for (const std::string& ty : {std::string("all"), q.type}) stats[std::string(mode_name(m)) + "|" + sp + "|" + ty].add(rank);
      line += std::string(" ") + mode_name(m) + "=" + (rank ? std::to_string(rank) : "-");
      if (verbose && rank != 1 && !r.empty())
        line += "(" + r[0].id + ")";
    }
    if (do_ask && sem) {
      auto t0 = std::chrono::steady_clock::now();
      AskResult a = lib.ask(q.text, "", 5);
      ms["ask"] += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      runs["ask"]++;
      int rank = 0;
      for (size_t i = 0; i < a.passages.size(); ++i)
        if (q.relevant.count(a.passages[i].id)) { rank = static_cast<int>(i) + 1; break; }
      ask_all.add(rank);
      ask_type[q.type].add(rank);
      ask_type["split:" + q.split].add(rank);
      line += " ask=" + (rank ? std::to_string(rank) : std::string("-")) + (a.weak ? "w" : "") +
              (a.passages.empty() ? "" : "(" + std::to_string(a.passages[0].score).substr(0, 5) + ")");
    }
    if (verbose) std::printf("%-5s %-6s %-50s%s\n", q.split.c_str(), q.type.c_str(), q.text.c_str(), line.c_str());
  }

  std::set<std::string> types, splits;
  for (const Query& q : qs) { types.insert(q.type); splits.insert(q.split); }
  std::printf("pack: %s  semantic: %s", pack_path, sem ? lib.info().semantic.model.c_str() : "none");
  if (sem)
    std::printf(" (%u dims, %u-bit weights, %u-bit vectors, %u passages)", lib.info().semantic.dims, lib.info().semantic.bits,
                lib.info().semantic.vector_bits, lib.info().semantic.passages);
  std::printf("\nqueries: %zu (split %s); metrics: MRR@20, R@k = share of queries with a relevant guide in the top k\n",
              qs.size(), split.c_str());
  std::printf("%-9s %-6s %-6s %5s %6s %6s %6s %6s\n", "mode", "split", "type", "n", "MRR", "R@1", "R@5", "R@10");
  std::vector<std::string> sp_list = {"all"};
  for (const std::string& s : splits) sp_list.push_back(s);
  std::vector<std::string> ty_list = {"all"};
  for (const std::string& t : types) ty_list.push_back(t);
  for (SearchMode m : modes)
    for (const std::string& sp : sp_list)
      for (const std::string& ty : ty_list) {
        if (sp != "all" && ty != "all") continue;
        auto it = stats.find(std::string(mode_name(m)) + "|" + sp + "|" + ty);
        if (it == stats.end() || it->second.n == 0) continue;
        const Stat& s = it->second;
        std::printf("%-9s %-6s %-6s %5d %6.3f %6.3f %6.3f %6.3f\n", mode_name(m), sp.c_str(), ty.c_str(), s.n, s.mrr(), s.r(1),
                    s.r(5), s.r(10));
      }
  if (ask_all.n) {
    std::printf("ask (a relevant guide's passage in the top k passages):\n");
    std::printf("%-16s %5s %6s %6s %6s\n", "type", "n", "MRR", "@1", "@5");
    std::printf("%-16s %5d %6.3f %6.3f %6.3f\n", "all", ask_all.n, ask_all.mrr(), ask_all.r(1), ask_all.r(5));
    for (auto& kv : ask_type)
      std::printf("%-16s %5d %6.3f %6.3f %6.3f\n", kv.first.c_str(), kv.second.n, kv.second.mrr(), kv.second.r(1), kv.second.r(5));
  }
  std::printf("latency (mean per query, this machine):");
  for (auto& kv : ms) std::printf("  %s %.2f ms", kv.first.c_str(), kv.second / std::max(1, runs[kv.first]));
  std::printf("\n");

  // Regression guard: exact-term queries must not rank worse with hybrid than with keywords.
  int worse = 0, guard_n = 0;
  bool have_kw = std::find(modes.begin(), modes.end(), SearchMode::kKeyword) != modes.end();
  bool have_hy = std::find(modes.begin(), modes.end(), SearchMode::kHybrid) != modes.end();
  if (have_kw && have_hy)
    for (size_t qi = 0; qi < qs.size(); ++qi) {
      if (qs[qi].type != "exact" || (split != "all" && qs[qi].split != split)) continue;
      ++guard_n;
      int k = ranks[qi][0] ? ranks[qi][0] : 99, h = ranks[qi][2] ? ranks[qi][2] : 99;
      if (h > k) {
        ++worse;
        std::printf("exact-term regression: \"%s\" keyword rank %d, hybrid rank %d\n", qs[qi].text.c_str(), ranks[qi][0], ranks[qi][2]);
      }
    }
  if (guard_n) std::printf("exact-term guard: %d of %d exact queries rank worse with hybrid than keyword\n", worse, guard_n);

  if (!tpath) return 0;
  std::ifstream tf(tpath, std::ios::binary);
  if (!tf) { std::fprintf(stderr, "guidepack eval: cannot read %s\n", tpath); return 2; }
  std::string line;
  int fails = 0, checks = 0;
  while (std::getline(tf, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    size_t hash = line.find('#');
    if (hash != std::string::npos) line.resize(hash);
    std::istringstream ss(line);
    std::string a, b, c, d;
    double v = 0;
    if (!(ss >> a)) continue;
    if (a == "require") {
      ss >> b;
      ++checks;
      if (b != "semantic") { std::fprintf(stderr, "guidepack eval: unknown requirement '%s'\n", b.c_str()); return 2; }
      if (sem) std::printf("ok   require semantic\n");
      else { ++fails; std::printf("FAIL require semantic: the pack has no usable semantic section\n"); }
      continue;
    }
    if (a == "guard") {
      ss >> b;
      ++checks;
      if (b != "exact") { std::fprintf(stderr, "guidepack eval: unknown guard '%s'\n", b.c_str()); return 2; }
      if (!sem) { std::printf("SKIP guard exact (no semantic section)\n"); continue; }
      if (worse) { ++fails; std::printf("FAIL guard exact: %d regression(s)\n", worse); }
      else std::printf("ok   guard exact\n");
      continue;
    }
    bool parsed = a == "ask" ? static_cast<bool>(ss >> b >> c >> v) : static_cast<bool>(ss >> b >> c >> d >> v);
    if (!parsed) { std::fprintf(stderr, "guidepack eval: bad threshold line: %s\n", line.c_str()); return 2; }
    ++checks;
    double got = 0;
    if (a == "ask") {
      const Stat* s = b == "all" ? &ask_all : (ask_type.count(b) ? &ask_type[b] : nullptr);
      if (!sem) { std::printf("SKIP ask %s (no semantic section)\n", b.c_str()); continue; }
      if (!s) { std::fprintf(stderr, "guidepack eval: no ask data for '%s'\n", b.c_str()); return 2; }
      got = c == "mrr" ? s->mrr() : c == "r1" ? s->r(1) : s->r(5);
    } else {
      auto it = stats.find(a + "|" + b + "|" + c);
      if (it == stats.end()) { std::fprintf(stderr, "guidepack eval: no data for %s %s %s\n", a.c_str(), b.c_str(), c.c_str()); return 2; }
      const Stat& s = it->second;
      got = d == "mrr" ? s.mrr() : d == "r1" ? s.r(1) : d == "r5" ? s.r(5) : s.r(10);
      if (a != "keyword" && !sem) { std::printf("SKIP %s %s %s %s (no semantic section)\n", a.c_str(), b.c_str(), c.c_str(), d.c_str()); continue; }
    }
    bool ok = got + 1e-9 >= v;
    if (!ok) ++fails;
    std::printf("%s %s %s %s %s: %.3f (min %.3f)\n", ok ? "ok  " : "FAIL", a.c_str(), b.c_str(), c.c_str(), d.c_str(), got, v);
  }
  std::printf("thresholds: %d check(s), %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
