// guidepack — builds, validates and inspects EmberMesh guide packs. See README.md.
//
//   guidepack build    <content_dir> -o <guides.pack> [--model M] [--vector-bits 4|8|32] [--strict] [-v]
//   guidepack validate <content_dir> [--no-warnings]
//   guidepack stats    <pack>
//   guidepack search   <pack> <query...> [--category C] [--limit N] [--semantic|--no-semantic|--meaning-only]
//   guidepack ask      <pack> <question...> [--category C] [--limit N]
//   guidepack related  <pack> <id> [--limit N]
//   guidepack get      <pack> <id>
//   guidepack eval     <pack> --queries <tsv> [--thresholds <file>] [--split tune|test|all] [-v]
//
// Host tool: uses the shared core guides code for everything except file I/O.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "eval.h"
#include "guides/library.h"
#include "guides/pack_builder.h"

namespace fs = std::filesystem;
using namespace ember;
using namespace ember::guides;

namespace {

int usage() {
  std::fprintf(stderr,
               "usage:\n"
               "  guidepack build    <content_dir> -o <guides.pack> [--model <file.emodel>] [--vector-bits 4|8|32]\n"
               "                     [--passage-bytes N] [--strict] [-v]\n"
               "  guidepack validate <content_dir> [--no-warnings]\n"
               "  guidepack stats    <pack>\n"
               "  guidepack search   <pack> <query...> [--category C] [--limit N] [--semantic|--no-semantic|--meaning-only]\n"
               "  guidepack ask      <pack> <question...> [--category C] [--limit N]\n"
               "  guidepack related  <pack> <id> [--limit N]\n"
               "  guidepack get      <pack> <id>\n"
               "  guidepack eval     <pack> --queries <queries.tsv> [--thresholds <file>] [--split tune|test|all]\n"
               "                     [--modes keyword,semantic,hybrid] [--lambda-strong X] [--lambda-weak X]\n"
               "                     [--guide-weight X] [--pool N] [-v]\n");
  return 2;
}

bool read_file(const fs::path& p, std::string& out) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return !f.bad();
}

bool write_file_atomic(const fs::path& p, const Bytes& data) {
  std::error_code ec;
  if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
  fs::path tmp = p;
  tmp += ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!f) return false;
  }
  fs::rename(tmp, p, ec);
  return !ec;
}

// Collects every regular file under dir (relative paths with '/'), skipping ignored components.
bool collect(const fs::path& dir, std::vector<SourceFile>& files, std::string& err) {
  std::error_code ec;
  if (!fs::exists(dir, ec)) return true;  // missing corpus = empty corpus
  if (!fs::is_directory(dir, ec)) { err = dir.string() + " is not a directory"; return false; }
  std::vector<std::string> rels;
  // Relative paths are computed lexically (iterator paths are always `dir / ...`): never
  // canonicalise, so nothing above `dir` is ever opened or resolved.
  const std::string base = dir.generic_u8string();  // UTF-8 on Windows too (not the ANSI code page)
  for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
       !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
    std::string full = it->path().generic_u8string();
    if (full.compare(0, base.size(), base) != 0) continue;
    std::string rel = full.substr(base.size());
    while (!rel.empty() && rel[0] == '/') rel.erase(0, 1);
    if (rel.empty()) continue;
    if (path_ignored(rel)) {
      if (it->is_directory(ec)) it.disable_recursion_pending();
      continue;
    }
    if (it->is_regular_file(ec)) rels.push_back(rel);
  }
  if (ec) { err = "cannot walk " + dir.string() + ": " + ec.message(); return false; }
  std::sort(rels.begin(), rels.end());
  for (const std::string& rel : rels) {
    SourceFile f;
    f.path = rel;
    if (!read_file(dir / fs::u8path(rel), f.content)) { err = "cannot read " + (dir / fs::u8path(rel)).u8string(); return false; }
    files.push_back(std::move(f));
  }
  return true;
}

void print_diags(const fs::path& dir, const std::vector<Diagnostic>& diags, bool warnings, FILE* out) {
  for (const Diagnostic& d : diags) {
    if (!d.error && !warnings) continue;
    std::string file = (dir / d.file).generic_string();
    if (d.line > 0) std::fprintf(out, "%s:%d: %s: %s\n", file.c_str(), d.line, d.error ? "error" : "warning", d.message.c_str());
    else std::fprintf(out, "%s: %s: %s\n", file.c_str(), d.error ? "error" : "warning", d.message.c_str());
  }
}

double ms_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

bool load_pack(const char* path, Bytes& data, GuideLibrary& lib, double* load_ms) {
  std::string s;
  if (!read_file(path, s)) { std::fprintf(stderr, "guidepack: cannot read %s\n", path); return false; }
  data.assign(s.begin(), s.end());
  auto t0 = std::chrono::steady_clock::now();
  LoadStatus st = lib.load(ByteView(data), false);
  if (load_ms) *load_ms = ms_since(t0);
  if (st != LoadStatus::kOk) {
    std::fprintf(stderr, "guidepack: %s: invalid pack (%s)\n", path, load_status_name(st));
    return false;
  }
  return true;
}

int cmd_validate(int argc, char** argv) {
  if (argc < 1) return usage();
  fs::path dir = argv[0];
  bool warnings = true;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--no-warnings")) warnings = false;
    else return usage();
  }
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) { std::fprintf(stderr, "guidepack: %s is not a directory\n", dir.string().c_str()); return 1; }
  std::vector<SourceFile> files;
  std::string err;
  if (!collect(dir, files, err)) { std::fprintf(stderr, "guidepack: %s\n", err.c_str()); return 1; }
  std::vector<SourceGuide> ok;
  std::vector<Diagnostic> diags;
  size_t errors = validate_corpus(files, ok, diags);
  print_diags(dir, diags, warnings, stdout);
  size_t warns = 0;
  for (const Diagnostic& d : diags) warns += d.error ? 0 : 1;
  size_t md = 0;
  for (const SourceFile& f : files) md += (f.path.size() > 3 && f.path.compare(f.path.size() - 3, 3, ".md") == 0) ? 1 : 0;
  std::printf("%zu guide file(s): %zu valid, %zu error(s), %zu warning(s)\n", md, ok.size(), errors, warns);
  return errors ? 1 : 0;
}

int cmd_build(int argc, char** argv) {
  const char* dir_arg = nullptr;
  const char* out_arg = nullptr;
  bool strict = false, verbose = false;
  const char* model_arg = nullptr;
  SemanticBuildOptions sem;
  for (int i = 0; i < argc; ++i) {
    if (!std::strcmp(argv[i], "-o") && i + 1 < argc) out_arg = argv[++i];
    else if (!std::strcmp(argv[i], "--model") && i + 1 < argc) model_arg = argv[++i];
    else if (!std::strcmp(argv[i], "--vector-bits") && i + 1 < argc) sem.vector_bits = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    else if (!std::strcmp(argv[i], "--passage-bytes") && i + 1 < argc) sem.passages.max_bytes = std::strtoul(argv[++i], nullptr, 10);
    else if (!std::strcmp(argv[i], "--no-title-in-passages")) sem.title_in_passages = false;
    else if (!std::strcmp(argv[i], "--strict")) strict = true;
    else if (!std::strcmp(argv[i], "-v")) verbose = true;
    else if (!dir_arg && argv[i][0] != '-') dir_arg = argv[i];
    else return usage();
  }
  if (!dir_arg || !out_arg) return usage();
  fs::path dir = dir_arg;
  std::error_code ec;
  if (!fs::exists(dir, ec)) std::fprintf(stderr, "guidepack: warning: %s does not exist; building an empty pack\n", dir_arg);
  std::vector<SourceFile> files;
  std::string err;
  if (!collect(dir, files, err)) { std::fprintf(stderr, "guidepack: %s\n", err.c_str()); return 1; }
  auto t0 = std::chrono::steady_clock::now();
  std::vector<SourceGuide> ok;
  std::vector<Diagnostic> diags;
  size_t errors = validate_corpus(files, ok, diags);
  print_diags(dir, diags, verbose, stderr);
  if (errors) {
    std::fprintf(stderr, "guidepack: %zu error(s); %s\n", errors,
                 strict ? "not writing the pack (--strict)" : "files with errors are left out of the pack");
    if (strict) return 1;
  }
  std::string model_bytes;
  if (model_arg) {
    if (!read_file(model_arg, model_bytes)) { std::fprintf(stderr, "guidepack: cannot read model %s\n", model_arg); return 1; }
    sem.model = ByteView(std::string_view(model_bytes));
  }
  double validate_ms = ms_since(t0);
  SemanticBuildStats sst;
  std::string berr;
  Bytes pack = build_pack(ok, sem, &sst, &berr);
  if (pack.empty()) { std::fprintf(stderr, "guidepack: %s\n", berr.c_str()); return 1; }
  double build_ms = ms_since(t0);
  // Self-check before writing.
  GuideLibrary lib;
  LoadStatus st = lib.load(ByteView(pack), false);
  if (st != LoadStatus::kOk) { std::fprintf(stderr, "guidepack: internal error: built pack does not load (%s)\n", load_status_name(st)); return 1; }
  if (!write_file_atomic(out_arg, pack)) { std::fprintf(stderr, "guidepack: cannot write %s\n", out_arg); return 1; }
  uint64_t text = 0;
  for (const SourceGuide& g : ok) text += g.markdown.size();
  std::printf("guidepack: %zu guide(s), %u categor%s, %llu bytes of Markdown -> %s (%zu bytes) in %.0f ms\n", ok.size(),
              lib.stats().categories, lib.stats().categories == 1 ? "y" : "ies",
              static_cast<unsigned long long>(text), out_arg, pack.size(), build_ms);
  if (model_arg)
    std::printf("guidepack: semantic: %u passages, %llu tokens embedded, +%zu bytes (model %zu, passages %zu, "
                "vectors %zu, guide vectors %zu); validate+index %.0f ms, total %.0f ms; %s\n",
                sst.passages, static_cast<unsigned long long>(sst.tokens),
                sst.model_bytes + sst.passage_bytes + sst.vector_bytes + sst.guide_vector_bytes, sst.model_bytes,
                sst.passage_bytes, sst.vector_bytes, sst.guide_vector_bytes, validate_ms, build_ms,
                lib.semantic_available() ? "semantic search available" : lib.info().semantic.status.c_str());
  if (model_arg && !lib.semantic_available()) { std::fprintf(stderr, "guidepack: internal error: semantic section does not load\n"); return 1; }
  return 0;
}

int cmd_stats(int argc, char** argv) {
  if (argc != 1) return usage();
  Bytes data;
  GuideLibrary lib;
  double load_ms = 0;
  if (!load_pack(argv[0], data, lib, &load_ms)) return 1;
  auto t0 = std::chrono::steady_clock::now();
  std::string err;
  bool ok = lib.verify(&err);
  double verify_ms = ms_since(t0);
  PackStats s = lib.stats();
  std::printf("pack:        %s (%zu bytes)\n", argv[0], s.pack_bytes);
  std::printf("guides:      %u in %u categories, %u index terms\n", s.docs, s.categories, s.terms);
  static const char* names[8] = {"strings", "categories", "docs", "id index", "term blocks", "terms", "postings", "bodies"};
  for (int i = 0; i < 8; ++i) std::printf("  %-12s %10zu bytes\n", names[i], s.section_bytes[i]);
  if (s.body_bytes)
    std::printf("markdown:    %llu bytes, compressed to %zu (%.1f%%)\n", static_cast<unsigned long long>(s.body_bytes),
                s.section_bytes[7], 100.0 * static_cast<double>(s.section_bytes[7]) / static_cast<double>(s.body_bytes));
  if (s.semantic.available) {
    std::printf("semantic:    %s, %u dims, %u-bit weights, %u-bit vectors, %u passages, %zu bytes\n",
                s.semantic.model.c_str(), s.semantic.dims, s.semantic.bits, s.semantic.vector_bits, s.semantic.passages,
                s.semantic.bytes);
    std::printf("  %-12s %10zu bytes\n  %-12s %10zu bytes\n  %-12s %10zu bytes\n  %-12s %10zu bytes\n", "model",
                s.sem_model_bytes, "passages", s.sem_passage_bytes, "vectors", s.sem_vector_bytes, "guide vectors",
                s.sem_guide_bytes);
  } else {
    std::printf("semantic:    not available (%s)\n", s.semantic.status.c_str());
  }
  std::printf("load:        %.2f ms (zero-copy)\n", load_ms);
  std::printf("verify:      %s (%.1f ms)%s%s\n", ok ? "ok" : "FAILED", verify_ms, ok ? "" : ": ", err.c_str());
  std::printf("categories:\n");
  for (const CategoryInfo& c : lib.categories()) std::printf("  %-14s %5u  %s\n", c.id.c_str(), c.count, c.title.c_str());
  return ok ? 0 : 1;
}

int cmd_search(int argc, char** argv) {
  if (argc < 2) return usage();
  std::string query, category;
  uint32_t limit = 10;
  SearchMode mode = SearchMode::kAuto;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--category") && i + 1 < argc) category = argv[++i];
    else if (!std::strcmp(argv[i], "--limit") && i + 1 < argc) limit = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    else if (!std::strcmp(argv[i], "--semantic")) mode = SearchMode::kHybrid;
    else if (!std::strcmp(argv[i], "--no-semantic")) mode = SearchMode::kKeyword;
    else if (!std::strcmp(argv[i], "--meaning-only")) mode = SearchMode::kSemantic;
    else { if (!query.empty()) query.push_back(' '); query += argv[i]; }
  }
  Bytes data;
  GuideLibrary lib;
  if (!load_pack(argv[0], data, lib, nullptr)) return 1;
  auto t0 = std::chrono::steady_clock::now();
  std::vector<SearchResult> res = lib.search(query, category, limit, mode);
  double first_ms = ms_since(t0);
  // Repeat for a steadier timing figure.
  t0 = std::chrono::steady_clock::now();
  const int reps = 5;
  for (int i = 0; i < reps; ++i) lib.search(query, category, limit, mode);
  double avg_ms = ms_since(t0) / reps;
  std::printf("%zu result(s) for \"%s\" in %.2f ms (avg of %d repeats: %.2f ms)\n", res.size(), query.c_str(), first_ms, reps, avg_ms);
  for (size_t i = 0; i < res.size(); ++i) {
    const SearchResult& r = res[i];
    std::string terms;
    for (const std::string& t : r.terms) { if (!terms.empty()) terms += ", "; terms += t; }
    std::printf("%2zu. [%7.3f %-7s] %s — %s\n      %s\n      terms: %s\n", i + 1, r.score, r.match.c_str(), r.id.c_str(),
                r.title.c_str(), r.snippet.c_str(), terms.c_str());
    if (r.has_passage)
      std::printf("      passage%s%s: %s\n", r.passage.heading.empty() ? "" : " under ", r.passage.heading.c_str(),
                  r.passage.text.c_str());
  }
  return 0;
}

int cmd_ask(int argc, char** argv) {
  if (argc < 2) return usage();
  std::string q, category;
  uint32_t limit = 5;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--category") && i + 1 < argc) category = argv[++i];
    else if (!std::strcmp(argv[i], "--limit") && i + 1 < argc) limit = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    else { if (!q.empty()) q.push_back(' '); q += argv[i]; }
  }
  Bytes data;
  GuideLibrary lib;
  if (!load_pack(argv[0], data, lib, nullptr)) return 1;
  if (!lib.semantic_available()) { std::fprintf(stderr, "guidepack: this pack has no semantic section\n"); return 1; }
  auto t0 = std::chrono::steady_clock::now();
  AskResult a = lib.ask(q, category, limit);
  double ms = ms_since(t0);
  std::printf("%zu passage(s) for \"%s\" in %.2f ms%s\n", a.passages.size(), q.c_str(), ms, a.weak ? " (weak)" : "");
  for (size_t i = 0; i < a.passages.size(); ++i) {
    const AskPassage& p = a.passages[i];
    std::printf("%2zu. [%.3f] %s — %s%s%s\n      %s\n", i + 1, p.score, p.id.c_str(), p.title.c_str(),
                p.heading.empty() ? "" : " / ", p.heading.c_str(), p.text.c_str());
  }
  return 0;
}

int cmd_related(int argc, char** argv) {
  if (argc < 2) return usage();
  uint32_t limit = 8;
  for (int i = 2; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--limit") && i + 1 < argc) limit = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    else return usage();
  }
  Bytes data;
  GuideLibrary lib;
  if (!load_pack(argv[0], data, lib, nullptr)) return 1;
  auto t0 = std::chrono::steady_clock::now();
  RelatedList r = lib.related(argv[1], limit);
  double ms = ms_since(t0);
  std::printf("%zu related guide(s) for %s in %.2f ms%s\n", r.results.size(), argv[1], ms, r.weak ? " (weak)" : "");
  for (size_t i = 0; i < r.results.size(); ++i)
    std::printf("%2zu. [%.3f] %s — %s\n", i + 1, r.results[i].score, r.results[i].meta.id.c_str(), r.results[i].meta.title.c_str());
  return 0;
}

int cmd_get(int argc, char** argv) {
  if (argc != 2) return usage();
  Bytes data;
  GuideLibrary lib;
  if (!load_pack(argv[0], data, lib, nullptr)) return 1;
  Guide g;
  GetStatus st = lib.get(argv[1], g);
  if (st == GetStatus::kNotFound) { std::fprintf(stderr, "guidepack: no guide '%s'\n", argv[1]); return 1; }
  if (st == GetStatus::kCorrupt) { std::fprintf(stderr, "guidepack: guide '%s' is corrupt in this pack\n", argv[1]); return 1; }
  const GuideMeta& m = g.meta;
  std::string tags;
  for (const std::string& t : m.tags) { if (!tags.empty()) tags += ", "; tags += t; }
  std::printf("id:           %s\ntitle:        %s\ncategory:     %s\nsummary:      %s\ntags:         %s\n"
              "source:       %s\nsource_title: %s\nlicense:      %s\nattribution:  %s\nbytes:        %u\n",
              m.id.c_str(), m.title.c_str(), m.category.c_str(), m.summary.c_str(), tags.c_str(), m.source.c_str(),
              m.source_title.c_str(), m.license.c_str(), m.attribution.c_str(), m.bytes);
  if (!m.part_of.empty()) std::printf("part_of:      %s\n", m.part_of.c_str());
  if (m.has_order) std::printf("order:        %d\n", m.order);
  if (!g.prev.empty()) std::printf("prev:         %s\n", g.prev.c_str());
  if (!g.next.empty()) std::printf("next:         %s\n", g.next.c_str());
  std::printf("\n%s", g.markdown.c_str());
  if (!g.markdown.empty() && g.markdown.back() != '\n') std::printf("\n");
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) return usage();
  std::string cmd = argv[1];
  if (cmd == "build") return cmd_build(argc - 2, argv + 2);
  if (cmd == "validate") return cmd_validate(argc - 2, argv + 2);
  if (cmd == "stats") return cmd_stats(argc - 2, argv + 2);
  if (cmd == "search") return cmd_search(argc - 2, argv + 2);
  if (cmd == "get") return cmd_get(argc - 2, argv + 2);
  if (cmd == "ask") return cmd_ask(argc - 2, argv + 2);
  if (cmd == "related") return cmd_related(argc - 2, argv + 2);
  if (cmd == "eval") return cmd_eval(argc - 2, argv + 2);
  return usage();
}
