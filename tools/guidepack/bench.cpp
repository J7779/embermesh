// guides_bench — latency and memory of guide search on a real pack, for native and WebAssembly
// builds (tools/guidepack/README.md, "Performance"). Standalone (no std::filesystem), so it also
// builds with Emscripten and runs under node:
//
//   guides_bench <guides.pack> <queries.tsv> [repeats]
//
// Prints: load time and heap held after load (counted by a replacement operator new), then per
// mode the mean and 90th-percentile latency over every query of the query set, plus the cost of
// the query embedding and of one full passage scan on their own.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <new>
#include <string>
#include <vector>

#include "guides/library.h"

namespace {
size_t g_live = 0, g_peak = 0;
}  // namespace

#if defined(__EMSCRIPTEN__)
// WebAssembly: the allocator's own statistics (live bytes; the peak is sampled, not exact).
#include <malloc.h>
namespace {
struct HeapSampler {
  static size_t live() { return static_cast<size_t>(mallinfo().uordblks); }
};
}  // namespace
#define EMBER_LIVE() (g_live = HeapSampler::live(), g_peak = std::max(g_peak, g_live), g_live)
#else
#define EMBER_LIVE() (g_live)
// Native: heap accounting by a replacement operator new; every allocation carries its size in a
// 16-byte prefix.
void* operator new(size_t n) {
  void* p = std::malloc(n + 16);
  if (!p) std::abort();
  std::memcpy(p, &n, sizeof n);
  g_live += n;
  g_peak = std::max(g_peak, g_live);
  return static_cast<char*>(p) + 16;
}
void operator delete(void* p) noexcept {
  if (!p) return;
  char* b = static_cast<char*>(p) - 16;
  size_t n;
  std::memcpy(&n, b, sizeof n);
  g_live -= n;
  std::free(b);
}
void operator delete(void* p, size_t) noexcept { operator delete(p); }
void* operator new[](size_t n) { return operator new(n); }
void operator delete[](void* p) noexcept { operator delete(p); }
void operator delete[](void* p, size_t) noexcept { operator delete(p); }
#endif

using namespace ember;
using namespace ember::guides;

namespace {
double now_ms() {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: guides_bench <guides.pack> <queries.tsv> [repeats]\n"); return 2; }
  int repeats = argc > 3 ? std::atoi(argv[3]) : 3;
  if (repeats < 1) repeats = 1;
  std::vector<std::string> queries;
  {
    std::ifstream q(argv[2], std::ios::binary);
    std::string line;
    while (std::getline(q, line)) {
      if (line.empty() || line[0] == '#') continue;
      size_t a = line.find('\t'), b = line.find('\t', a + 1), c = line.find('\t', b + 1);
      if (c != std::string::npos) queries.push_back(line.substr(b + 1, c - b - 1));
    }
  }
  std::vector<uint8_t>* data = new std::vector<uint8_t>();
  {
    std::ifstream f(argv[1], std::ios::binary);
    if (!f) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    data->assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  }
  const size_t before = EMBER_LIVE();
  g_peak = g_live;
  GuideLibrary* lib = new GuideLibrary();
  double t0 = now_ms();
  LoadStatus st = lib->load(ByteView(*data), false);
  double load_ms = now_ms() - t0;
  if (st != LoadStatus::kOk) { std::fprintf(stderr, "invalid pack (%s)\n", load_status_name(st)); return 2; }
  std::printf("pack %zu bytes, %u guides, semantic %s (%u passages, %u dims)\n", data->size(), lib->total(),
              lib->semantic_available() ? "yes" : "no", lib->semantic().passages(), lib->semantic().dims());
  EMBER_LIVE();
  std::printf("load: %.2f ms; heap held after load %zu bytes, peak during load %zu bytes (zero-copy)\n", load_ms,
              g_live - before, g_peak - before);

  struct Mode { const char* name; int kind; };
  const Mode modes[] = {{"keyword", 0}, {"semantic", 1}, {"hybrid", 2}, {"ask", 3}};
  for (const Mode& m : modes) {
    if (m.kind && !lib->semantic_available()) continue;
    std::vector<double> t;
    size_t peak = 0;
    for (int r = 0; r < repeats; ++r)
      for (const std::string& q : queries) {
        size_t base = EMBER_LIVE();
        g_peak = g_live;
        double a = now_ms();
        if (m.kind == 3) lib->ask(q, "", 5);
        else lib->search(q, "", 20, m.kind == 0 ? SearchMode::kKeyword : m.kind == 1 ? SearchMode::kSemantic : SearchMode::kHybrid);
        t.push_back(now_ms() - a);
        EMBER_LIVE();
        peak = std::max(peak, g_peak - base);
      }
    std::sort(t.begin(), t.end());
    double sum = 0;
    for (double x : t) sum += x;
    std::printf("%-8s mean %7.2f ms  p50 %7.2f  p90 %7.2f  max %7.2f  (n=%zu, peak heap per query %zu KiB)\n", m.name,
                sum / t.size(), t[t.size() / 2], t[t.size() * 9 / 10], t.back(), t.size(), peak / 1024);
  }
  if (lib->semantic_available()) {
    const SemanticIndex& s = lib->semantic();
    SemanticIndex::Query q;
    const int n = 200;
    double a = now_ms();
    for (int i = 0; i < n; ++i) s.prepare_text(queries[static_cast<size_t>(i) % queries.size()], q);
    double embed = (now_ms() - a) / n;
    std::vector<float> out(s.passages());
    s.prepare_text("how do i make water safe to drink", q);
    a = now_ms();
    const int scans = 20;
    for (int i = 0; i < scans; ++i) s.score_passages(q, 0, s.passages(), out.data());
    double scan = (now_ms() - a) / scans;
    std::printf("query embedding %.3f ms; full scan of %u passages %.2f ms (%.1f ns/passage)\n", embed, s.passages(), scan,
                scan * 1e6 / s.passages());
  }
  delete lib;
  delete data;
  return 0;
}
