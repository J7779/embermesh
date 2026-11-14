// Shared helpers for the guides tests: loads the fixture corpus in core/tests/data/guides/content
// (test code may do file I/O; the core may not).
#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "guides/library.h"
#include "guides/pack_builder.h"

namespace guides_test {

inline std::string fixture_dir() { return std::string(EMBER_TEST_DATA_DIR) + "/guides/content"; }

inline std::vector<ember::guides::SourceFile> fixture_files() {
  namespace fs = std::filesystem;
  std::vector<ember::guides::SourceFile> out;
  fs::path root = fixture_dir();
  for (auto& e : fs::recursive_directory_iterator(root)) {
    if (!e.is_regular_file()) continue;
    ember::guides::SourceFile f;
    f.path = fs::relative(e.path(), root).generic_string();
    std::ifstream in(e.path(), std::ios::binary);
    f.content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    out.push_back(std::move(f));
  }
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
  return out;
}

inline std::vector<ember::guides::SourceGuide> fixture_guides(size_t* errors = nullptr) {
  std::vector<ember::guides::SourceGuide> ok;
  std::vector<ember::guides::Diagnostic> diags;
  size_t e = ember::guides::validate_corpus(fixture_files(), ok, diags);
  if (errors) *errors = e;
  return ok;
}

inline const ember::Bytes& fixture_pack() {
  static const ember::Bytes pack = ember::guides::build_pack(fixture_guides());
  return pack;
}

// Minimal valid guide text for validator tests.
inline std::string guide_text(const std::string& id, const std::string& category, const std::string& extra = "",
                              const std::string& body = "# Title\n\nBody text.\n", const std::string& license = "CC0-1.0") {
  return "---\nid: " + id + "\ntitle: A Title\ncategory: " + category +
         "\ntags: [a, b]\nsummary: One sentence.\nsource: original\nsource_title: Test\nlicense: " + license +
         "\nattribution: Test\nretrieved: 2026-01-01\n" + extra + "---\n" + body;
}

// The committed static embedding model (models/<name>/*.emodel; the first one in path order).
// Empty if the repository has none (semantic tests then fail loudly, not silently pass).
inline const ember::Bytes& committed_model() {
  static const ember::Bytes model = [] {
    namespace fs = std::filesystem;
    std::vector<std::string> found;
    std::error_code ec;
    fs::path root = fs::path(EMBER_REPO_ROOT) / "models";
    for (auto it = fs::recursive_directory_iterator(root, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
      if (it->is_regular_file() && it->path().extension() == ".emodel") found.push_back(it->path().generic_string());
    std::sort(found.begin(), found.end());
    ember::Bytes b;
    if (found.empty()) return b;
    std::ifstream in(found.front(), std::ios::binary);
    b.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return b;
  }();
  return model;
}

inline std::vector<std::string> ids_of(const std::vector<ember::guides::SearchResult>& r) {
  std::vector<std::string> out;
  for (const auto& x : r) out.push_back(x.id);
  return out;
}

}  // namespace guides_test
