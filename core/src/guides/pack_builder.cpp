#include "guides/pack_builder.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <map>

#include "guides/deflate.h"
#include "guides/embed.h"
#include "guides/frontmatter.h"
#include "guides/pack_format.h"
#include "guides/text.h"

namespace ember::guides {

// ---------------------------------------------------------------------------------------------
// Categories

namespace {
struct CatDef {
  const char* id;
  const char* title;
};
constexpr CatDef kCategories[] = {
    {"medical", "Medical & First Aid"},     {"hygiene", "Hygiene & Sanitation"},
    {"water", "Water"},                     {"food", "Food & Foraging"},
    {"agriculture", "Agriculture"},         {"shelter", "Shelter"},
    {"fire", "Fire"},                       {"navigation", "Navigation"},
    {"signaling", "Signaling"},             {"survival", "Survival Skills"},
    {"radio", "Radio"},                     {"electronics", "Electronics"},
    {"power", "Power & Energy"},            {"repair", "Repair & Tools"},
    {"weather", "Weather"},                 {"hazards", "Hazards & Disasters"},
    {"security", "Security"},               {"community", "Community"},
    {"reference", "Reference"},             {"embermesh", "EmberMesh Manuals"},
};
constexpr int kNumCategories = static_cast<int>(sizeof kCategories / sizeof kCategories[0]);
}  // namespace

int category_rank(std::string_view id) {
  for (int i = 0; i < kNumCategories; ++i)
    if (id == kCategories[i].id) return i;
  return kNumCategories;
}

bool category_known(std::string_view id) { return category_rank(id) < kNumCategories; }

std::string category_title(std::string_view id) {
  int r = category_rank(id);
  if (r < kNumCategories) return kCategories[r].title;
  std::string t;
  bool up = true;
  for (char c : id) {
    if (c == '-' || c == '_') { t.push_back(' '); up = true; continue; }
    t.push_back(up && c >= 'a' && c <= 'z' ? static_cast<char>(c - 32) : c);
    up = false;
  }
  return t;
}

bool path_ignored(std::string_view p) {
  size_t i = 0;
  while (i <= p.size()) {
    size_t slash = p.find('/', i);
    if (slash == std::string_view::npos) slash = p.size();
    if (slash > i && (p[i] == '_' || p[i] == '.')) return true;
    i = slash + 1;
  }
  return false;
}

// ---------------------------------------------------------------------------------------------
// Licences

bool license_allowed(std::string_view license, std::string* why) {
  auto no = [&](const char* m) { if (why) *why = m; return false; };
  std::vector<std::string> tok;
  std::string cur;
  for (char c : license) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      cur.push_back(c >= 'a' && c <= 'z' ? static_cast<char>(c - 32) : c);
    } else if (!cur.empty()) {
      tok.push_back(cur);
      cur.clear();
    }
  }
  if (!cur.empty()) tok.push_back(cur);
  if (tok.empty()) return no("empty licence");
  auto has = [&](std::string_view t) { return std::find(tok.begin(), tok.end(), t) != tok.end(); };
  auto has_seq = [&](std::initializer_list<std::string_view> seq) {
    for (size_t i = 0; i + seq.size() <= tok.size(); ++i) {
      size_t k = 0;
      for (std::string_view s : seq) { if (tok[i + k] != s) break; ++k; }
      if (k == seq.size()) return true;
    }
    return false;
  };
  for (const std::string& t : tok) {
    if (t == "NC" || t.rfind("NONCOMMERCIAL", 0) == 0) return no("non-commercial (NC) licences are not allowed");
    if (t == "ND" || t.rfind("NODERIV", 0) == 0) return no("no-derivatives (ND) licences are not allowed");
  }
  if (has_seq({"NON", "COMMERCIAL"})) return no("non-commercial (NC) licences are not allowed");
  if (has_seq({"NO", "DERIVATIVES"}) || has_seq({"NO", "DERIVS"}) || has_seq({"NO", "DERIVATIVE"}))
    return no("no-derivatives (ND) licences are not allowed");
  if (has_seq({"ALL", "RIGHTS", "RESERVED"})) return no("'all rights reserved' is not redistributable");
  if (has("PROPRIETARY") || has_seq({"FAIR", "USE"}) || has("UNKNOWN") || has("TBD") || has("TODO"))
    return no("licence does not permit redistribution");
  if (has_seq({"PUBLIC", "DOMAIN"}) || has("CC0") || has("PD") || has_seq({"CC", "BY"}) ||
      has_seq({"CC", "ZERO"}) || has_seq({"CREATIVE", "COMMONS", "ATTRIBUTION"}) ||
      has_seq({"CREATIVE", "COMMONS", "ZERO"}) || has_seq({"OPEN", "GOVERNMENT", "LICENCE"}) ||
      has_seq({"OPEN", "GOVERNMENT", "LICENSE"}) || has("OGL") || has("MIT") || has("BSD") ||
      has("APACHE") || has("ISC") || has("GFDL") || has("UNLICENSE") || has("WTFPL") || has("GPL") ||
      has("LGPL") || has("ODC") || has("ODBL") || has("PDDL"))
    return true;
  return no("licence not in the allow-list (public domain, CC0, CC BY, CC BY-SA, ...)");
}

// ---------------------------------------------------------------------------------------------
// File validation

namespace {

constexpr std::string_view kKnownKeys[] = {
    "id", "title", "category", "tags", "summary", "source", "source_title", "license",
    "attribution", "retrieved", "part_of", "order", "authors", "author", "notes", "updated", "version",
};

constexpr std::string_view kHtmlTags[] = {
    "a", "abbr", "audio", "b", "big", "blockquote", "body", "br", "canvas", "center", "code",
    "dd", "del", "details", "div", "dl", "dt", "em", "embed", "figcaption", "figure", "font",
    "form", "h1", "h2", "h3", "h4", "h5", "h6", "head", "hr", "html", "i", "iframe", "img", "input",
    "ins", "kbd", "li", "link", "mark", "meta", "nav", "object", "ol", "p", "picture", "pre", "s",
    "script", "section", "small", "source", "span", "strike", "strong", "style", "sub", "summary",
    "sup", "svg", "table", "tbody", "td", "tfoot", "th", "thead", "tr", "tt", "u", "ul", "video",
};

int line_of(std::string_view text, size_t off) {
  int line = 1;
  for (size_t i = 0; i < off && i < text.size(); ++i)
    if (text[i] == '\n') ++line;
  return line;
}

bool parse_int(std::string_view s, int32_t& out) {
  if (s.empty() || s.size() > 10) return false;
  size_t i = 0;
  bool neg = false;
  if (s[0] == '-' || s[0] == '+') { neg = s[0] == '-'; i = 1; }
  if (i == s.size()) return false;
  int64_t v = 0;
  for (; i < s.size(); ++i) {
    if (s[i] < '0' || s[i] > '9') return false;
    v = v * 10 + (s[i] - '0');
  }
  if (neg) v = -v;
  if (v < INT32_MIN || v > INT32_MAX) return false;
  out = static_cast<int32_t>(v);
  return true;
}

bool is_date(std::string_view s) {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
  for (size_t i = 0; i < 10; ++i)
    if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9')) return false;
  return true;
}

// Removes inline code spans from a line.
std::string strip_inline_code(std::string_view l) {
  std::string out;
  size_t i = 0;
  while (i < l.size()) {
    if (l[i] == '`') {
      size_t run = 0;
      while (i + run < l.size() && l[i + run] == '`') ++run;
      std::string_view fence = l.substr(i, run);
      size_t close = l.find(fence, i + run);
      if (close == std::string_view::npos) { out.append(l.substr(i)); break; }
      i = close + run;
      out.push_back(' ');
      continue;
    }
    out.push_back(l[i++]);
  }
  return out;
}

// Returns the raw HTML tag found in the line, or "".
std::string find_html(std::string_view l) {
  if (l.find("<!--") != std::string_view::npos) return "<!--";
  for (size_t i = 0; i + 1 < l.size(); ++i) {
    if (l[i] != '<') continue;
    size_t j = i + 1;
    if (l[j] == '/') ++j;
    size_t b = j;
    while (j < l.size() && ((l[j] >= 'a' && l[j] <= 'z') || (l[j] >= 'A' && l[j] <= 'Z') || (l[j] >= '0' && l[j] <= '9'))) ++j;
    if (j == b) continue;
    if (j < l.size() && l[j] != '>' && l[j] != ' ' && l[j] != '/' && l[j] != '\t') continue;
    if (j == l.size()) continue;
    std::string name = ascii_lower(l.substr(b, j - b));
    for (std::string_view t : kHtmlTags)
      if (name == t) return std::string(l.substr(i, std::min<size_t>(j - i + 1, 24)));
  }
  return std::string();
}

}  // namespace

bool parse_guide_file(const SourceFile& file, SourceGuide& out, std::vector<Diagnostic>& diags,
                      const ValidateOptions& opt) {
  size_t errors = 0;
  auto diag = [&](int line, bool error, std::string msg) {
    diags.push_back({file.path, line, error, std::move(msg)});
    if (error) ++errors;
  };
  const std::string& text = file.content;
  out = SourceGuide();

  if (text.size() > opt.max_bytes)
    diag(0, true, "file is " + std::to_string(text.size()) + " bytes; limit is " + std::to_string(opt.max_bytes) +
                      " (split long works into chapters with part_of/order)");
  else if (text.size() > opt.warn_bytes)
    diag(0, false, "file is " + std::to_string(text.size()) + " bytes; keep guides under ~60 KB");

  size_t bad;
  if (!utf8_valid(text, &bad)) {
    diag(line_of(text, bad), true, "invalid UTF-8 at byte " + std::to_string(bad));
    return false;
  }
  for (size_t i = 0; i < text.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') {
      diag(line_of(text, i), true, "control character 0x" + to_hex(ByteView(&c, 1)) + " in file");
      break;
    }
  }

  FrontMatter fm = parse_front_matter(text);
  if (!fm.present) {
    diag(1, true, "missing front matter (file must start with a '---' line)");
    return false;
  }
  for (const FmDiag& e : fm.errors) diag(e.line, true, e.message);
  for (const FmValue& v : fm.values) {
    bool known = false;
    for (std::string_view k : kKnownKeys) known = known || v.key == k;
    if (!known) diag(v.line, false, "unknown front-matter key '" + v.key + "'");
  }

  auto scalar = [&](const char* key, bool required) -> std::string {
    const FmValue* v = fm.find(key);
    if (!v) {
      if (required) diag(1, true, std::string("missing required field '") + key + "'");
      return std::string();
    }
    if (v->is_list) {
      diag(v->line, true, std::string("field '") + key + "' must be a single value, not a list");
      return std::string();
    }
    if (required && v->scalar.empty()) diag(v->line, true, std::string("field '") + key + "' is empty");
    return v->scalar;
  };
  auto line_of_key = [&](const char* key) { const FmValue* v = fm.find(key); return v ? v->line : 1; };

  GuideMeta& m = out.meta;
  m.id = scalar("id", true);
  m.title = scalar("title", true);
  m.category = scalar("category", true);
  m.summary = scalar("summary", true);
  m.source = scalar("source", true);
  m.license = scalar("license", true);
  m.attribution = scalar("attribution", true);
  m.source_title = scalar("source_title", false);
  std::string retrieved = scalar("retrieved", false);
  m.part_of = scalar("part_of", false);
  std::string order = scalar("order", false);

  if (const FmValue* t = fm.find("tags")) {
    if (t->is_list) {
      m.tags = t->list;
    } else if (!t->scalar.empty()) {
      diag(t->line, false, "tags should be a list: [a, b, c]");
      std::string cur;
      for (char c : t->scalar + ",") {
        if (c == ',') { std::string_view s = cur; while (!s.empty() && s.front() == ' ') s.remove_prefix(1); while (!s.empty() && s.back() == ' ') s.remove_suffix(1); if (!s.empty()) m.tags.emplace_back(s); cur.clear(); }
        else cur.push_back(c);
      }
    }
    for (std::string& tag : m.tags)
      for (char& c : tag) if (c == '\n') c = ' ';
  } else {
    diag(1, false, "missing 'tags'");
  }
  if (!fm.find("source_title")) diag(1, false, "missing 'source_title'");
  if (!fm.find("retrieved")) diag(1, false, "missing 'retrieved'");
  else if (!retrieved.empty() && !is_date(retrieved)) diag(line_of_key("retrieved"), false, "retrieved should be YYYY-MM-DD");

  // Path / id / category consistency.
  std::string_view path = file.path;
  std::string expect_id;
  if (path.size() < 3 || path.substr(path.size() - 3) != ".md") {
    diag(0, true, "guide files must have the .md extension");
  } else {
    expect_id = std::string(path.substr(0, path.size() - 3));
  }
  size_t slash = path.find('/');
  std::string dir = slash == std::string_view::npos ? std::string() : std::string(path.substr(0, slash));
  if (dir.empty()) diag(0, true, "guide must live in a category directory (<category>/<slug>.md)");
  if (!m.id.empty() && !expect_id.empty() && m.id != expect_id)
    diag(line_of_key("id"), true, "id '" + m.id + "' must equal the path without extension ('" + expect_id + "')");
  if (!m.id.empty()) {
    for (char c : m.id) {
      bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '/' || c == '.';
      if (!ok) { diag(line_of_key("id"), false, "id should use only lowercase a-z, 0-9, '-', '_', '/'"); break; }
    }
  }
  if (!m.category.empty() && !dir.empty() && m.category != dir)
    diag(line_of_key("category"), true, "category '" + m.category + "' must equal the directory name ('" + dir + "')");
  if (!m.category.empty() && !category_known(m.category))
    diag(line_of_key("category"), false, "unknown category '" + m.category + "' (see ARCHITECTURE.md §7)");

  std::string why;
  if (!m.license.empty() && !license_allowed(m.license, &why))
    diag(line_of_key("license"), true, "licence '" + m.license + "' rejected: " + why);

  if (!order.empty()) {
    if (!parse_int(order, m.order)) diag(line_of_key("order"), true, "order must be an integer");
    else m.has_order = true;
  }
  if (!m.part_of.empty()) {
    if (!m.category.empty() && m.part_of.rfind(m.category + "/", 0) != 0)
      diag(line_of_key("part_of"), false, "part_of should start with '" + m.category + "/'");
    if (!m.has_order) diag(line_of_key("part_of"), false, "part_of without 'order'; chapters will sort by title");
  } else if (m.has_order) {
    diag(line_of_key("order"), false, "order without part_of has no effect");
  }
  if (m.title.size() > 200) diag(line_of_key("title"), false, "title is very long");
  if (m.summary.size() > 400) diag(line_of_key("summary"), false, "summary is very long (keep it to one sentence)");
  {
    // The reader rejects packs with any metadata string over kMaxMetaBytes (hostile-pack limit).
    size_t tags_len = 0;
    for (size_t i = 0; i < m.tags.size(); ++i) tags_len += m.tags[i].size() + (i ? 1 : 0);
    const std::pair<const char*, size_t> lens[] = {
        {"id", m.id.size()},          {"title", m.title.size()},     {"summary", m.summary.size()},
        {"tags", tags_len},           {"source", m.source.size()},   {"source_title", m.source_title.size()},
        {"license", m.license.size()}, {"attribution", m.attribution.size()}, {"part_of", m.part_of.size()}};
    for (const auto& l : lens)
      if (l.second > kMaxMetaBytes)
        diag(line_of_key(l.first), true,
             std::string("field '") + l.first + "' is longer than " + std::to_string(kMaxMetaBytes) + " bytes");
  }

  // Body checks.
  if (!fm.terminated) return false;
  std::string_view body = std::string_view(text).substr(std::min(fm.body_offset, text.size()));
  bool blank = true;
  for (char c : body)
    if (c != ' ' && c != '\n' && c != '\r' && c != '\t') { blank = false; break; }
  if (blank) diag(fm.body_line, true, "empty body");
  bool fence = false;
  int line = fm.body_line;
  int html_reported = 0, img_reported = 0;
  for (size_t pos = 0; pos <= body.size();) {
    size_t nl = body.find('\n', pos);
    if (nl == std::string_view::npos) nl = body.size();
    std::string_view l = body.substr(pos, nl - pos);
    size_t k = 0;
    while (k < l.size() && (l[k] == ' ' || l[k] == '\t')) ++k;
    std::string_view t = l.substr(k);
    if (t.substr(0, 3) == "```" || t.substr(0, 3) == "~~~") {
      fence = !fence;
    } else if (!fence) {
      std::string s = strip_inline_code(l);
      std::string tag = find_html(s);
      if (!tag.empty() && html_reported++ < 5) diag(line, true, "raw HTML is not allowed (" + tag + ")");
      if (s.find("![") != std::string::npos && s.find("](", s.find("![")) != std::string::npos && img_reported++ < 5)
        diag(line, true, "images are not allowed; describe the diagram in words or ASCII art");
    }
    ++line;
    if (nl == body.size()) break;
    pos = nl + 1;
  }
  if (fence) diag(line - 1, false, "unterminated code fence");

  // Normalise line endings in the stored body.
  out.markdown.reserve(body.size());
  for (size_t i = 0; i < body.size(); ++i) {
    if (body[i] == '\r' && i + 1 < body.size() && body[i + 1] == '\n') continue;
    out.markdown.push_back(body[i]);
  }
  while (!out.markdown.empty() && out.markdown.front() == '\n') out.markdown.erase(out.markdown.begin());
  return errors == 0;
}

size_t validate_corpus(const std::vector<SourceFile>& files, std::vector<SourceGuide>& ok,
                       std::vector<Diagnostic>& diags, const ValidateOptions& opt) {
  size_t before = 0;
  for (const Diagnostic& d : diags) before += d.error ? 1 : 0;
  std::vector<const SourceFile*> sorted;
  for (const SourceFile& f : files) sorted.push_back(&f);
  std::sort(sorted.begin(), sorted.end(), [](const SourceFile* a, const SourceFile* b) { return a->path < b->path; });
  std::map<std::string, std::string> ids;  // lowercased id -> path
  std::map<std::pair<std::string, int32_t>, std::string> chapters;
  for (const SourceFile* f : sorted) {
    if (path_ignored(f->path)) continue;
    if (f->path.size() < 3 || f->path.compare(f->path.size() - 3, 3, ".md") != 0) {
      diags.push_back({f->path, 0, false, "not a Markdown file; ignored (images and other assets are not allowed)"});
      continue;
    }
    SourceGuide g;
    if (!parse_guide_file(*f, g, diags, opt)) continue;
    std::string key = ascii_lower(g.meta.id);
    auto it = ids.find(key);
    if (it != ids.end()) {
      diags.push_back({f->path, 1, true, "duplicate id '" + g.meta.id + "' (also in " + it->second + ")"});
      continue;
    }
    ids[key] = f->path;
    if (!g.meta.part_of.empty() && g.meta.has_order) {
      auto k = std::make_pair(g.meta.part_of, g.meta.order);
      auto c = chapters.find(k);
      if (c != chapters.end())
        diags.push_back({f->path, 1, false, "same part_of/order as " + c->second});
      else
        chapters[k] = f->path;
    }
    ok.push_back(std::move(g));
  }
  size_t after = 0;
  for (const Diagnostic& d : diags) after += d.error ? 1 : 0;
  return after - before;
}

// ---------------------------------------------------------------------------------------------
// Pack building

namespace {

class StringPool {
 public:
  // Returns {offset, length}.
  std::pair<uint32_t, uint32_t> add(std::string_view s) {
    if (s.empty()) return {0, 0};
    auto it = index_.find(std::string(s));
    if (it != index_.end()) return {it->second, static_cast<uint32_t>(s.size())};
    uint32_t off = static_cast<uint32_t>(data_.size());
    data_.append(s.data(), s.size());
    index_.emplace(std::string(s), off);
    return {off, static_cast<uint32_t>(s.size())};
  }
  const std::string& data() const { return data_; }

 private:
  std::string data_;
  std::map<std::string, uint32_t> index_;
};

void put32(Bytes& b, size_t off, uint32_t v) {
  for (int i = 0; i < 4; ++i) b[off + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
}
void put64(Bytes& b, size_t off, uint64_t v) {
  put32(b, off, static_cast<uint32_t>(v));
  put32(b, off + 4, static_cast<uint32_t>(v >> 32));
}
uint32_t get32(const Bytes& b, size_t off) {
  return static_cast<uint32_t>(b[off]) | static_cast<uint32_t>(b[off + 1]) << 8 |
         static_cast<uint32_t>(b[off + 2]) << 16 | static_cast<uint32_t>(b[off + 3]) << 24;
}

std::string group_key(const GuideMeta& m) {
  if (!m.part_of.empty()) {
    std::string_view p = m.part_of;
    size_t slash = p.rfind('/');
    std::string s(slash == std::string_view::npos ? p : p.substr(slash + 1));
    for (char& c : s) if (c == '-' || c == '_') c = ' ';
    return ascii_lower(s);
  }
  return ascii_lower(m.title);
}

struct Posting {
  uint32_t doc;
  std::array<uint32_t, kFieldCount> tf;
};

void put16(Bytes& b, uint16_t v) { b.push_back(static_cast<uint8_t>(v)); b.push_back(static_cast<uint8_t>(v >> 8)); }
void app32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i))); }

// Semantic sections (pack_format.h): passages of every guide, their vectors and one vector per
// guide, all computed with the same integer embedding code the apps run (guides/embed.cpp).
void append_semantic(Bytes& out, const std::vector<SourceGuide>& docs, const EmbedModel& model,
                     const SemanticBuildOptions& sem, SemanticBuildStats* stats) {
  const uint32_t n = static_cast<uint32_t>(docs.size());
  const uint32_t D = model.dims();
  Bytes firsts, recs, vecs, norms, gvecs, gnorms;
  uint32_t P = 0;
  uint64_t tokens = 0;
  std::vector<int64_t> acc(D), gacc(D);
  std::vector<uint32_t> ids;
  for (uint32_t d = 0; d < n && P < kMaxPassages; ++d) {
    const SourceGuide& g = docs[d];
    app32(firsts, P);
    std::fill(gacc.begin(), gacc.end(), 0);
    auto add = [&](std::string_view text, const PassageSpan& span) {
      std::fill(acc.begin(), acc.end(), 0);
      ids.clear();
      model.tokenize(text, ids, kMaxEmbedTokens);
      for (uint32_t id : ids) model.add_row(id, acc.data());
      tokens += ids.size();
      for (uint32_t k = 0; k < D; ++k) gacc[k] += acc[k];
      app32(recs, span.off);
      put16(recs, static_cast<uint16_t>(span.len));
      put16(recs, static_cast<uint16_t>(span.head_len));
      app32(recs, span.head_off);
      encode_vector(acc.data(), D, sem.vector_bits, vecs, norms);
      ++P;
    };
    // Passage 0: what the guide is about (title, tags, summary).
    std::string meta = g.meta.title + "\n";
    for (size_t i = 0; i < g.meta.tags.size(); ++i) meta += (i ? ", " : "") + g.meta.tags[i];
    meta += "\n" + g.meta.summary;
    add(meta, PassageSpan());
    for (const PassageSpan& s : split_passages(g.markdown, sem.passages)) {
      if (P >= kMaxPassages) break;
      if (s.len > 0xFFFF || s.head_len > 0xFFFF) continue;
      std::string text;
      if (sem.title_in_passages) text = g.meta.title + "\n";
      if (s.head_len) text.append(g.markdown, s.head_off, s.head_len).push_back('\n');
      text += markdown_to_plain(std::string_view(g.markdown).substr(s.off, s.len));
      add(text, s);
    }
    encode_vector(gacc.data(), D, sem.vector_bits, gvecs, gnorms);
  }
  for (uint32_t d = static_cast<uint32_t>(firsts.size() / 4); d <= n; ++d) app32(firsts, P);
  while (gnorms.size() < static_cast<size_t>(n) * (sem.vector_bits == 4 ? 2 : 4)) {  // only if capped
    std::vector<int64_t> z(D, 0);
    encode_vector(z.data(), D, sem.vector_bits, gvecs, gnorms);
  }
  Bytes sp;
  app32(sp, P); app32(sp, n); app32(sp, kSemFlagMetaPassage); app32(sp, 0);
  sp.insert(sp.end(), firsts.begin(), firsts.end());
  sp.insert(sp.end(), recs.begin(), recs.end());
  auto vec_section = [&](uint32_t count, const Bytes& v, const Bytes& nb) {
    Bytes s;
    app32(s, count); app32(s, D); app32(s, sem.vector_bits); app32(s, sem.vector_bits == 4 ? 2 : 4);
    s.insert(s.end(), v.begin(), v.end());
    s.insert(s.end(), nb.begin(), nb.end());
    return s;
  };
  Bytes sv = vec_section(P, vecs, norms), sg = vec_section(n, gvecs, gnorms);
  struct Sec { uint32_t tag; ByteView data; };
  const Sec secs[] = {{kTagSemModel, sem.model}, {kTagSemPassages, ByteView(sp)}, {kTagSemVectors, ByteView(sv)},
                      {kTagSemGuides, ByteView(sg)}};
  const uint32_t count = static_cast<uint32_t>(sizeof secs / sizeof secs[0]);
  while (out.size() % 4) out.push_back(0);
  const size_t dir = out.size();
  out.resize(dir + 16 + count * kExtEntrySize, 0);
  std::memcpy(out.data() + dir, kExtMagic, sizeof kExtMagic);
  put32(out, dir + 8, count);
  for (uint32_t i = 0; i < count; ++i) {
    while (out.size() % 4) out.push_back(0);
    size_t e = dir + 16 + i * kExtEntrySize;
    put32(out, e, secs[i].tag);
    put32(out, e + 4, kSemVersion);
    put32(out, e + 8, static_cast<uint32_t>(out.size()));
    put32(out, e + 12, static_cast<uint32_t>(secs[i].data.size()));
    put32(out, e + 16, crc32_fast(secs[i].data));
    out.insert(out.end(), secs[i].data.begin(), secs[i].data.end());
  }
  put32(out, dir + 12, crc32_fast(ByteView(out.data() + dir + 16, count * kExtEntrySize)));
  put32(out, kHdrExtDir, static_cast<uint32_t>(dir));
  if (stats) {
    stats->passages = P;
    stats->tokens = tokens;
    stats->model_bytes = sem.model.size();
    stats->passage_bytes = sp.size();
    stats->vector_bytes = sv.size();
    stats->guide_vector_bytes = sg.size();
  }
}

}  // namespace

Bytes build_pack(const std::vector<SourceGuide>& input) { return build_pack(input, SemanticBuildOptions()); }

Bytes build_pack(const std::vector<SourceGuide>& input, const SemanticBuildOptions& sem, SemanticBuildStats* stats,
                 std::string* error) {
  if (stats) *stats = SemanticBuildStats();
  EmbedModel model;
  if (!sem.model.empty()) {
    ModelStatus ms = model.load(sem.model);
    if (ms != ModelStatus::kOk) {
      if (error) *error = std::string("cannot load the embedding model: ") + model_status_name(ms);
      return Bytes();
    }
    if (sem.vector_bits != 4 && sem.vector_bits != 8 && sem.vector_bits != 32) {
      if (error) *error = "vector_bits must be 4, 8 or 32";
      return Bytes();
    }
  }
  // Deduplicate ids (first wins) and fix up sizes.
  std::vector<SourceGuide> g;
  {
    std::map<std::string, bool> seen;
    for (const SourceGuide& s : input) {
      if (s.meta.id.empty() || seen.count(s.meta.id)) continue;
      seen[s.meta.id] = true;
      g.push_back(s);
      g.back().meta.bytes = static_cast<uint32_t>(g.back().markdown.size());
      if (g.back().meta.category.empty()) g.back().meta.category = "reference";
    }
  }
  // Categories in canonical order.
  std::vector<std::string> cats;
  for (const SourceGuide& s : g)
    if (std::find(cats.begin(), cats.end(), s.meta.category) == cats.end()) cats.push_back(s.meta.category);
  std::sort(cats.begin(), cats.end(), [](const std::string& a, const std::string& b) {
    int ra = category_rank(a), rb = category_rank(b);
    return ra != rb ? ra < rb : a < b;
  });
  auto cat_index = [&](const std::string& c) {
    return static_cast<uint32_t>(std::find(cats.begin(), cats.end(), c) - cats.begin());
  };
  // List order.
  std::vector<std::string> gkeys(g.size()), tkeys(g.size());
  for (size_t i = 0; i < g.size(); ++i) { gkeys[i] = group_key(g[i].meta); tkeys[i] = ascii_lower(g[i].meta.title); }
  std::vector<size_t> order(g.size());
  for (size_t i = 0; i < g.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    const GuideMeta& x = g[a].meta;
    const GuideMeta& y = g[b].meta;
    uint32_t cx = cat_index(x.category), cy = cat_index(y.category);
    if (cx != cy) return cx < cy;
    if (gkeys[a] != gkeys[b]) return gkeys[a] < gkeys[b];
    if (x.part_of != y.part_of) return x.part_of < y.part_of;
    int64_t ox = x.has_order ? x.order : INT64_MAX, oy = y.has_order ? y.order : INT64_MAX;
    if (ox != oy) return ox < oy;
    if (tkeys[a] != tkeys[b]) return tkeys[a] < tkeys[b];
    return x.id < y.id;
  });
  std::vector<SourceGuide> docs;
  docs.reserve(g.size());
  for (size_t i : order) docs.push_back(std::move(g[i]));
  const uint32_t n = static_cast<uint32_t>(docs.size());

  // prev/next within each book (by order, then title, then id).
  std::vector<uint32_t> prev(n, kNone), next(n, kNone);
  {
    std::map<std::string, std::vector<uint32_t>> books;
    for (uint32_t d = 0; d < n; ++d)
      if (!docs[d].meta.part_of.empty()) books[docs[d].meta.part_of].push_back(d);
    for (auto& kv : books) {
      std::vector<uint32_t>& v = kv.second;
      std::stable_sort(v.begin(), v.end(), [&](uint32_t a, uint32_t b) {
        const GuideMeta& x = docs[a].meta;
        const GuideMeta& y = docs[b].meta;
        int64_t ox = x.has_order ? x.order : INT64_MAX, oy = y.has_order ? y.order : INT64_MAX;
        if (ox != oy) return ox < oy;
        std::string tx = ascii_lower(x.title), ty = ascii_lower(y.title);
        if (tx != ty) return tx < ty;
        return x.id < y.id;
      });
      for (size_t i = 0; i + 1 < v.size(); ++i) { next[v[i]] = v[i + 1]; prev[v[i + 1]] = v[i]; }
    }
  }

  // Strings, docs, bodies, index.
  StringPool pool;
  Bytes cat_sec(cats.size() * kCategoryRecordSize, 0);
  Bytes doc_sec(static_cast<size_t>(n) * kDocRecordSize, 0);
  Bytes bodies;
  std::map<std::string, std::vector<Posting>> index;
  uint64_t field_total[kFieldCount] = {0, 0, 0, 0};

  for (size_t c = 0; c < cats.size(); ++c) {
    size_t r = c * kCategoryRecordSize;
    auto id = pool.add(cats[c]);
    auto title = pool.add(category_title(cats[c]));
    put32(cat_sec, r, id.first); put32(cat_sec, r + 4, id.second);
    put32(cat_sec, r + 8, title.first); put32(cat_sec, r + 12, title.second);
    uint32_t first = kNone, count = 0;
    for (uint32_t d = 0; d < n; ++d)
      if (docs[d].meta.category == cats[c]) { if (first == kNone) first = d; ++count; }
    put32(cat_sec, r + 16, first == kNone ? 0 : first);
    put32(cat_sec, r + 20, count);
  }

  std::vector<Token> toks;
  for (uint32_t d = 0; d < n; ++d) {
    const GuideMeta& m = docs[d].meta;
    size_t r = static_cast<size_t>(d) * kDocRecordSize;
    auto ref = [&](size_t off, std::string_view s) {
      auto p = pool.add(s);
      put32(doc_sec, r + off, p.first);
      put32(doc_sec, r + off + 4, p.second);
    };
    std::string tags;
    for (size_t i = 0; i < m.tags.size(); ++i) { if (i) tags.push_back('\n'); tags += m.tags[i]; }
    ref(kDocId, m.id);
    ref(kDocTitle, m.title);
    ref(kDocSummary, m.summary);
    ref(kDocTags, tags);
    ref(kDocSource, m.source);
    ref(kDocSourceTitle, m.source_title);
    ref(kDocLicense, m.license);
    ref(kDocAttribution, m.attribution);
    ref(kDocPartOf, m.part_of);
    put32(doc_sec, r + kDocCategory, cat_index(m.category));
    put32(doc_sec, r + kDocFlags, m.has_order ? kDocFlagHasOrder : 0);
    put32(doc_sec, r + kDocOrder, static_cast<uint32_t>(m.order));
    const std::string& md = docs[d].markdown;
    put32(doc_sec, r + kDocBytes, static_cast<uint32_t>(md.size()));
    Bytes z = deflate_raw(ByteView(md));
    put32(doc_sec, r + kDocBodyOff, static_cast<uint32_t>(bodies.size()));
    put32(doc_sec, r + kDocBodyLen, static_cast<uint32_t>(z.size()));
    put32(doc_sec, r + kDocBodyCrc, crc32_fast(ByteView(md)));
    bodies.insert(bodies.end(), z.begin(), z.end());
    put32(doc_sec, r + kDocPrev, prev[d]);
    put32(doc_sec, r + kDocNext, next[d]);

    std::map<std::string, std::array<uint32_t, kFieldCount>> local;
    std::string tag_text;
    for (const std::string& t : m.tags) { tag_text += t; tag_text.push_back(' '); }
    std::string plain = markdown_to_plain(md);
    std::string_view fields[kFieldCount] = {m.title, tag_text, m.summary, plain};
    for (int f = 0; f < kFieldCount; ++f) {
      toks.clear();
      tokenize(fields[f], toks);
      uint32_t len = 0;
      for (const Token& t : toks) {
        std::string term = index_term(t.text);
        if (term.empty() || term.size() > kMaxTermBytes) continue;
        auto& a = local[term];
        if (a[static_cast<size_t>(f)] < (1u << 20)) a[static_cast<size_t>(f)]++;
        ++len;
      }
      put32(doc_sec, r + kDocFieldLens + 4 * static_cast<size_t>(f), std::min<uint32_t>(len, 1u << 24));
      field_total[f] += std::min<uint32_t>(len, 1u << 24);
    }
    for (auto& kv : local) index[kv.first].push_back({d, kv.second});
  }

  // ID index.
  std::vector<uint32_t> by_id(n);
  for (uint32_t d = 0; d < n; ++d) by_id[d] = d;
  std::sort(by_id.begin(), by_id.end(), [&](uint32_t a, uint32_t b) { return docs[a].meta.id < docs[b].meta.id; });
  Bytes id_sec(static_cast<size_t>(n) * 4, 0);
  for (uint32_t i = 0; i < n; ++i) put32(id_sec, 4 * static_cast<size_t>(i), by_id[i]);

  // Dictionary + postings.
  ByteWriter terms, blocks, posts;
  uint32_t t = 0;
  std::string prev_term;
  for (auto& kv : index) {
    const std::string& term = kv.first;
    ByteWriter pl;
    uint32_t last = 0;
    for (size_t k = 0; k < kv.second.size(); ++k) {
      const Posting& p = kv.second[k];
      pl.varint(k == 0 ? p.doc : p.doc - last);
      last = p.doc;
      uint8_t mask = 0;
      for (int f = 0; f < kFieldCount; ++f) if (p.tf[static_cast<size_t>(f)]) mask |= static_cast<uint8_t>(1 << f);
      pl.u8(mask);
      for (int f = 0; f < kFieldCount; ++f) if (p.tf[static_cast<size_t>(f)]) pl.varint(p.tf[static_cast<size_t>(f)]);
    }
    if (t % kTermBlockSize == 0) {
      blocks.u32le(static_cast<uint32_t>(terms.size()));
      blocks.u32le(static_cast<uint32_t>(posts.size()));
      terms.varint(term.size()).str(term);
    } else {
      size_t shared = 0;
      while (shared < term.size() && shared < prev_term.size() && term[shared] == prev_term[shared]) ++shared;
      terms.varint(shared).varint(term.size() - shared).str(std::string_view(term).substr(shared));
    }
    terms.varint(kv.second.size()).varint(pl.size());
    posts.bytes(ByteView(pl.data()));
    prev_term = term;
    ++t;
  }

  // Assemble.
  const std::string& strings = pool.data();
  ByteView secs[kSectionCount] = {
      ByteView(std::string_view(strings)), ByteView(cat_sec), ByteView(doc_sec), ByteView(id_sec),
      ByteView(blocks.data()), ByteView(terms.data()), ByteView(posts.data()), ByteView(bodies),
  };
  Bytes out(kHeaderSize, 0);
  std::memcpy(out.data(), kPackMagic, sizeof kPackMagic);
  put32(out, kHdrVersion, kPackVersion);
  put32(out, kHdrHeaderSize, static_cast<uint32_t>(kHeaderSize));
  put32(out, kHdrFlags, 0);
  put32(out, kHdrDocCount, n);
  put32(out, kHdrCategoryCount, static_cast<uint32_t>(cats.size()));
  put32(out, kHdrTermCount, t);
  put32(out, kHdrTermBlockSize, kTermBlockSize);
  put32(out, kHdrSectionCount, kSectionCount);
  for (int f = 0; f < kFieldCount; ++f) put64(out, kHdrFieldTotals + 8 * static_cast<size_t>(f), field_total[f]);
  for (uint32_t s = 0; s < kSectionCount; ++s) {
    size_t e = kHdrSections + 12 * s;
    put32(out, e, static_cast<uint32_t>(out.size()));
    put32(out, e + 4, static_cast<uint32_t>(secs[s].size()));
    put32(out, e + 8, crc32_fast(secs[s]));
    out.insert(out.end(), secs[s].begin(), secs[s].end());
  }
  if (model.loaded()) append_semantic(out, docs, model, sem, stats);
  put32(out, kHdrCrc, crc32_fast(ByteView(out.data(), kHdrCrc)));
  return out;
}

bool repair_pack_checksums(Bytes& p) {
  if (p.size() < kHeaderSize) return false;
  for (uint32_t s = 0; s < kSectionCount; ++s) {
    size_t e = kHdrSections + 12 * s;
    uint32_t off = get32(p, e), len = get32(p, e + 4);
    if (off > p.size() || len > p.size() - off) return false;
    put32(p, e + 8, crc32_fast(ByteView(p.data() + off, len)));
  }
  put32(p, kHdrCrc, crc32_fast(ByteView(p.data(), kHdrCrc)));
  return true;
}

}  // namespace ember::guides
