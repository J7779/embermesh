// EmberMesh guides — ranked full-text search over a loaded GuideLibrary.
//
// Query pipeline:
//   1. tokenize (text.h); stop words dropped; each remaining word becomes a *clause*; the joined
//      form of a hyphenated compound ("j-pole" -> "jpole") is an optional clause.
//   2. each clause gets alternatives: the exact stem (weight 1); British/American spelling variants
//      (0.95) and a small table of lay synonyms ("baby" -> "infant", "kid" -> "child", 0.8); for the
//      final word when the query
//      does not end in whitespace, dictionary terms it is a prefix of (search-as-you-type); if a
//      word matches nothing, dictionary terms within Damerau-Levenshtein distance 1 (2 for long
//      words) sharing the first letter (typo tolerance).
//   3. scoring: per-field BM25 (title, tags, summary, body) with field weights; a document takes
//      the best alternative per clause; documents missing clauses are penalised (AND preference).
//   4. the top candidates (at least kMinRerank) are re-ranked by decompressing their bodies: bonus
//      for all terms in the title, for an exact title match, for the share of title words that are
//      query words (title precision), for query words covered by the
//      guide's own metadata (title + tags + summary: what the guide is *about*, not what it merely
//      mentions), for query phrases that appear verbatim in a tag or the title, and for terms
//      adjacent/near each other in the body (phrase preference); the best-covering window becomes
//      the snippet.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "guides/library.h"
#include "guides/pack_format.h"
#include "guides/text.h"

namespace ember::guides {

namespace {

constexpr size_t kMaxQueryBytes = 512;
constexpr size_t kMaxQueryTokens = 32;
constexpr size_t kMaxClauses = 12;
constexpr size_t kMaxPrefixAlts = 16;
constexpr size_t kMaxPrefixScan = 6000;
constexpr size_t kMaxSnippetTokens = 30000;
constexpr size_t kSnippetBytes = 220;
constexpr size_t kMinRerank = 30;  // candidates re-ranked regardless of `limit` (stable order)
// Per-query work budgets (security review H6: a search must never hold the node for seconds,
// whatever the pack contains). Real queries on the 864-guide corpus stay far below all of them:
//   * re-ranking reads at most the first 128 KiB of each body (guide files are <= 64 KiB) and at
//     most 16 MiB of compressed + decompressed body bytes per query (limit 200 on the real corpus:
//     ~5 MiB); candidates past the budget are ranked on metadata only;
//   * at most 4 M posting entries are decoded per query (real: ~30 alternatives x <= 864 docs);
//   * typo correction visits at most 300 000 dictionary terms per query (real dictionary: 54 508).
constexpr size_t kRerankBodyBytes = 128u << 10;
constexpr size_t kRerankBudget = 16u << 20;
constexpr uint64_t kMaxPostingEntries = 4u << 20;
constexpr size_t kMaxTypoVisits = 300000;

// Query-side word alternatives. Spelling variants are symmetric; synonyms map lay words to the
// words guides use (and back). Entries are normalized words (text.h), compared before stemming.
struct WordAlt { const char* a; const char* b; double w; };
constexpr WordAlt kWordAlts[] = {
    // British / American spelling
    {"labour", "labor", 0.95}, {"diarrhoea", "diarrhea", 0.95}, {"haemorrhage", "hemorrhage", 0.95},
    {"anaemia", "anemia", 0.95}, {"oedema", "edema", 0.95}, {"paediatric", "pediatric", 0.95},
    {"faeces", "feces", 0.95}, {"tumour", "tumor", 0.95}, {"centre", "center", 0.95}, {"metre", "meter", 0.95},
    {"litre", "liter", 0.95}, {"fibre", "fiber", 0.95}, {"mould", "mold", 0.95}, {"sulphur", "sulfur", 0.95},
    {"aluminium", "aluminum", 0.95}, {"colour", "color", 0.95}, {"defence", "defense", 0.95},
    {"aeroplane", "airplane", 0.95}, {"tyre", "tire", 0.95}, {"oesophagus", "esophagus", 0.95},
    {"haemorrhoids", "hemorrhoids", 0.95}, {"anaesthetic", "anesthetic", 0.95}, {"foetus", "fetus", 0.95},
    {"gaol", "jail", 0.9},
    // lay words <-> the words guides use
    {"baby", "infant", 0.8}, {"babies", "infants", 0.8}, {"baby", "newborn", 0.7}, {"kid", "child", 0.8},
    {"kids", "children", 0.8}, {"toddler", "child", 0.7}, {"fridge", "refrigerator", 0.8},
    {"fridge", "refrigeration", 0.7}, {"puke", "vomit", 0.8}, {"throwup", "vomit", 0.8}, {"poop", "feces", 0.7},
    {"broken", "fracture", 0.6}, {"torch", "flashlight", 0.8}, {"petrol", "gasoline", 0.9},
    {"paraffin", "kerosene", 0.8}, {"cellphone", "phone", 0.8}, {"mobile", "phone", 0.6},
    {"walkie", "handheld", 0.6}, {"faint", "syncope", 0.6}, {"fainted", "unconscious", 0.6},
    {"passed", "unconscious", 0.4}, {"fits", "seizure", 0.7}, {"jab", "injection", 0.7},
    {"sunstroke", "heatstroke", 0.8}};

constexpr double kK1 = 1.2;
constexpr double kFieldWeight[kFieldCount] = {4.0, 2.5, 2.0, 1.0};
constexpr double kFieldB[kFieldCount] = {0.4, 0.3, 0.5, 0.75};

struct Alt {
  std::string term;
  double weight;
  GuideLibrary::TermInfo info;
};

struct Clause {
  std::string word;  // normalized query word
  std::vector<Alt> alts;
  bool optional = false;   // joined compound: scores, but is not required
  uint32_t covers = 0;     // clauses (the compound's parts) a match of this clause also satisfies
  uint32_t begin = 0, end = 0;
};

// Optimal-string-alignment distance, returns > maxd as soon as it is certain.
int osa_distance(std::string_view a, std::string_view b, int maxd) {
  int n = static_cast<int>(a.size()), m = static_cast<int>(b.size());
  if (std::abs(n - m) > maxd) return maxd + 1;
  if (n > 64 || m > 64) return maxd + 1;
  int d0[65], d1[65], d2[65];  // rows i-2, i-1, i
  int* pp = d0; int* p = d1; int* c = d2;
  for (int j = 0; j <= m; ++j) p[j] = j;
  for (int i = 1; i <= n; ++i) {
    c[0] = i;
    int rowmin = c[0];
    for (int j = 1; j <= m; ++j) {
      int cost = a[static_cast<size_t>(i - 1)] == b[static_cast<size_t>(j - 1)] ? 0 : 1;
      int v = std::min({p[j] + 1, c[j - 1] + 1, p[j - 1] + cost});
      if (i > 1 && j > 1 && a[static_cast<size_t>(i - 1)] == b[static_cast<size_t>(j - 2)] &&
          a[static_cast<size_t>(i - 2)] == b[static_cast<size_t>(j - 1)])
        v = std::min(v, pp[j - 2] + 1);
      c[j] = v;
      rowmin = std::min(rowmin, v);
    }
    if (rowmin > maxd) return maxd + 1;
    int* t = pp; pp = p; p = c; c = t;
  }
  return p[m];
}

bool all_ascii_letters(std::string_view s) {
  for (char ch : s)
    if (ch < 'a' || ch > 'z') return false;
  return true;
}

int popcount16(uint32_t v) {
  int c = 0;
  while (v) { v &= v - 1; ++c; }
  return c;
}

}  // namespace

class Searcher {
 public:
  explicit Searcher(const GuideLibrary& lib) : lib_(lib) {}

  std::vector<SearchResult> run(std::string_view query, std::string_view category, uint32_t limit,
                                SearchMode mode = SearchMode::kKeyword, const HybridParams& hp = HybridParams()) {
    std::vector<SearchResult> out;
    if (!lib_.valid_ || lib_.n_docs_ == 0) return out;
    limit = std::max<uint32_t>(1, std::min<uint32_t>(limit, 200));
    if (query.size() > kMaxQueryBytes) query = query.substr(0, utf8_floor(query, kMaxQueryBytes));

    uint32_t lo = 0, hi = lib_.n_docs_;
    if (!category.empty()) {
      int c = lib_.find_category(category);
      if (c < 0) return out;
      size_t r = lib_.sec_[kSecCategories].off + static_cast<size_t>(c) * kCategoryRecordSize;
      lo = lib_.u32(r + 16);
      hi = lo + lib_.u32(r + 20);
      if (lo >= hi) return out;
    }

    const bool semantic = mode != SearchMode::kKeyword && lib_.sem_.available();
    if (!semantic) {
      if (!build_clauses(query)) return out;
      score(lo, hi);
      rank(limit, out);
      for (SearchResult& r : out) r.match = "words";
      return out;
    }
    hybrid(query, lo, hi, limit, mode == SearchMode::kSemantic, hp, out);
    return out;
  }

 private:
  // ---- query parsing ------------------------------------------------------------------------
  bool build_clauses(std::string_view query) {
    std::vector<Token> toks;
    tokenize(query, toks, kMaxQueryTokens);
    for (const Token& t : toks) {
      if (clauses_.size() >= kMaxClauses) break;
      if (is_stop_word(t.text)) continue;
      bool is_last = t.end == query.size();
      std::string s = stem(t.text);
      bool dup = false;
      for (const Clause& c : clauses_)
        if (c.word == t.text || (!c.alts.empty() && c.alts[0].term == s && c.alts[0].weight == 1.0)) dup = true;
      if (dup) continue;
      Clause c;
      c.word = t.text;
      c.optional = t.joined;
      c.begin = t.begin;
      c.end = t.end;
      if (t.joined)
        for (size_t k = 0; k < clauses_.size(); ++k)
          if (clauses_[k].begin >= t.begin && clauses_[k].end <= t.end) c.covers |= 1u << k;
      add_alternatives(c, t.text, s, is_last);
      clauses_.push_back(std::move(c));
    }
    // Adjacent words that the corpus writes as one ("snake bite" -> "snakebite", "fire starter").
    for (size_t k = 0, n = clauses_.size(); k + 1 < n && clauses_.size() < kMaxClauses; ++k) {
      const Clause& a = clauses_[k];
      const Clause& b = clauses_[k + 1];
      if (a.optional || b.optional || a.word.size() + b.word.size() > kMaxTokenBytes) continue;
      std::string joined = a.word + b.word;
      GuideLibrary::TermInfo info;
      std::string s = stem(joined);
      if (!lib_.find_term(s, info)) continue;
      Clause c;
      c.word = joined;
      c.optional = true;
      c.covers = (1u << k) | (1u << (k + 1));
      c.begin = a.begin;
      c.end = b.end;
      c.alts.push_back({s, 1.0, info});
      clauses_.push_back(std::move(c));
    }
    required_mask_ = 0;
    for (size_t i = 0; i < clauses_.size(); ++i)
      if (!clauses_[i].optional) required_mask_ |= 1u << i;
    n_required_ = popcount16(required_mask_);
    if (n_required_ == 0) {
      // Only optional clauses (cannot normally happen); treat them as required.
      for (size_t i = 0; i < clauses_.size(); ++i) { clauses_[i].optional = false; required_mask_ |= 1u << i; }
      n_required_ = popcount16(required_mask_);
    }
    bool any = false;
    for (const Clause& c : clauses_) any = any || !c.alts.empty();
    build_prefilter();
    return any;
  }

  void add_word_alts(Clause& c, const std::string& w) {
    for (const WordAlt& x : kWordAlts) {
      const char* other = w == x.a ? x.b : (w == x.b ? x.a : nullptr);
      if (!other) continue;
      std::string os = stem(other);
      bool have = false;
      for (const Alt& a : c.alts) have = have || a.term == os;
      GuideLibrary::TermInfo info;
      if (!have && !os.empty() && lib_.find_term(os, info)) c.alts.push_back({os, x.w, info});
    }
  }

  void add_alternatives(Clause& c, const std::string& w, const std::string& s, bool prefix) {
    GuideLibrary::TermInfo info;
    if (lib_.find_term(s, info)) c.alts.push_back({s, 1.0, info});
    if (w != s && lib_.find_term(w, info)) c.alts.push_back({w, 1.0, info});
    add_word_alts(c, w);
    if (prefix && w.size() >= 2) {
      struct Ctx { std::vector<Alt>* cand; const Clause* c; size_t seen; } ctx{&prefix_tmp_, &c, 0};
      prefix_tmp_.clear();
      auto v = [](void* p, std::string_view t, const GuideLibrary::TermInfo& i) {
        Ctx* x = static_cast<Ctx*>(p);
        if (++x->seen > kMaxPrefixScan) return false;
        for (const Alt& a : x->c->alts)
          if (a.term == t) return true;
        x->cand->push_back({std::string(t), 0.0, i});
        return true;
      };
      lib_.scan_prefix(w, v, &ctx);
      std::stable_sort(prefix_tmp_.begin(), prefix_tmp_.end(),
                       [](const Alt& a, const Alt& b) { return a.info.df > b.info.df; });
      if (prefix_tmp_.size() > kMaxPrefixAlts) prefix_tmp_.resize(kMaxPrefixAlts);
      double wgt = w.size() >= 4 ? 0.85 : (w.size() == 3 ? 0.7 : 0.55);
      for (Alt& a : prefix_tmp_) { a.weight = wgt; c.alts.push_back(std::move(a)); }
    }
    if (c.alts.empty() && s.size() >= 4 && all_ascii_letters(s)) {
      int maxd = s.size() >= 8 ? 2 : 1;
      struct Cand { std::string term; int d; GuideLibrary::TermInfo info; };
      std::vector<Cand> cands;
      struct Ctx { const std::string* s; int maxd; std::vector<Cand>* out; size_t* budget; } ctx{&s, maxd, &cands, &typo_budget_};
      auto v = [](void* p, std::string_view t, const GuideLibrary::TermInfo& i) {
        Ctx* x = static_cast<Ctx*>(p);
        if (*x->budget == 0) return false;
        --*x->budget;
        int d = osa_distance(*x->s, t, x->maxd);
        if (d <= x->maxd) x->out->push_back({std::string(t), d, i});
        return true;
      };
      lib_.scan_prefix(std::string_view(s).substr(0, 1), v, &ctx);
      std::stable_sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
        return a.d != b.d ? a.d < b.d : a.info.df > b.info.df;
      });
      for (size_t k = 0; k < cands.size() && k < 3; ++k)
        c.alts.push_back({cands[k].term, cands[k].d == 1 ? 0.7 : 0.45, cands[k].info});
    }
  }

  // ---- BM25 scoring ---------------------------------------------------------------------------
  void score(uint32_t lo, uint32_t hi) {
    const uint32_t n = lib_.n_docs_;
    total_.assign(n, 0.0f);
    cover_.assign(n, 0);
    best_.assign(n, 0.0f);
    std::vector<uint32_t> touched;
    for (size_t ci = 0; ci < clauses_.size(); ++ci) {
      touched.clear();
      for (const Alt& a : clauses_[ci].alts) {
        double idf = std::log(1.0 + (n - a.info.df + 0.5) / (a.info.df + 0.5));
        ByteReader r(lib_.postings(a.info));
        uint64_t doc = 0;
        for (uint32_t k = 0; k < a.info.df; ++k) {
          if (postings_budget_ == 0) break;
          --postings_budget_;
          uint64_t gap = r.varint();
          doc = k == 0 ? gap : doc + gap;
          uint8_t mask = r.u8();
          uint32_t tf[kFieldCount] = {0, 0, 0, 0};
          for (int f = 0; f < kFieldCount; ++f)
            if (mask & (1 << f)) tf[f] = static_cast<uint32_t>(std::min<uint64_t>(r.varint(), 1u << 20));
          if (!r.ok() || doc >= n || (k > 0 && gap == 0)) break;  // corrupt list: ignore the rest
          if (doc < lo) continue;
          if (doc >= hi) break;
          uint32_t d = static_cast<uint32_t>(doc);
          double s = 0;
          for (int f = 0; f < kFieldCount; ++f) {
            if (!tf[f]) continue;
            double norm = 1.0 - kFieldB[f] + kFieldB[f] * lib_.field_len(d, f) / lib_.avg_len_[f];
            s += kFieldWeight[f] * (tf[f] * (kK1 + 1.0)) / (tf[f] + kK1 * norm);
          }
          s *= idf * a.weight;
          if (best_[d] == 0.0f) touched.push_back(d);
          if (s > best_[d]) best_[d] = static_cast<float>(s);
        }
      }
      for (uint32_t d : touched) {
        total_[d] += best_[d];
        cover_[d] |= static_cast<uint16_t>((1u << ci) | clauses_[ci].covers);
        best_[d] = 0.0f;
      }
    }
    for (uint32_t d = lo; d < hi; ++d) {
      if (total_[d] <= 0) continue;
      int m = popcount16(cover_[d] & required_mask_);
      double f = 1.0;
      if (m < n_required_) {
        double frac = static_cast<double>(m) / n_required_;
        f = 0.5 * frac * std::sqrt(frac);
      }
      cand_.push_back({d, total_[d] * f});
    }
  }

  // ---- re-ranking and snippets ----------------------------------------------------------------
  struct Cand { uint32_t doc; double score; double meta_cov = 0; };

  int clause_of(std::string_view term) const {
    for (size_t ci = 0; ci < clauses_.size(); ++ci)
      for (const Alt& a : clauses_[ci].alts)
        if (a.term == term) return static_cast<int>(ci);
    return -1;
  }

  static uint32_t key2(std::string_view t) {
    return static_cast<uint32_t>(static_cast<uint8_t>(t[0])) << 8 | static_cast<uint8_t>(t[1]);
  }
  void build_prefilter() {
    first2_.assign(1024, 0);
    for (bool& b : one_) b = false;
    for (const Clause& c : clauses_)
      for (const Alt& a : c.alts) {
        if (a.term.size() >= 2) { uint32_t k = key2(a.term); first2_[k >> 6] |= 1ull << (k & 63); }
        else if (a.term.size() == 1) one_[static_cast<uint8_t>(a.term[0])] = true;
      }
  }
  bool maybe_match(std::string_view tok) const {
    // Cheap pre-filter before stemming: a matching token shares its first two bytes with an alt
    // (stems are prefixes of their words except for the final y -> i, which keeps two bytes).
    if (tok.size() >= 2) { uint32_t k = key2(tok); return (first2_[k >> 6] >> (k & 63)) & 1; }
    return tok.size() == 1 && one_[static_cast<uint8_t>(tok[0])];
  }

  int match_token(const std::string& normalized) const {
    if (!maybe_match(normalized)) return -1;
    if (is_stop_word(normalized)) return -1;
    int c = clause_of(stem(normalized));
    if (c < 0) c = clause_of(normalized);
    return c;
  }

  static void add_term(std::vector<std::string>& terms, std::string_view surface) {
    std::string t = utf8_lower(surface);
    for (const std::string& x : terms)
      if (x == t) return;
    if (terms.size() < 12) terms.push_back(std::move(t));
  }

  void rerank(Cand& c, SearchResult& r) {
    std::string_view title = lib_.doc_title(c.doc);
    // Title coverage.
    std::vector<Token> tt;
    tokenize(title, tt, 64);
    uint32_t tmask = 0;
    int title_words = 0;
    for (const Token& t : tt) {
      if (t.joined || is_stop_word(t.text)) continue;
      ++title_words;
      int ci = match_token(t.text);
      if (ci >= 0) { tmask |= (1u << ci) | clauses_[static_cast<size_t>(ci)].covers; add_term(r.terms, title.substr(t.begin, t.end - t.begin)); }
    }
    if ((tmask & required_mask_) == required_mask_) {
      c.score *= 1.4;
      if (title_words == n_required_) c.score *= 1.3;
    }
    // Title precision: share of the title's words that are query words ("Stroke" over "Heat
    // Stroke" for "stroke signs").
    if (title_words > 0 && tmask) {
      int hit = 0;
      for (const Token& t : tt)
        if (!t.joined && !is_stop_word(t.text) && match_token(t.text) >= 0) ++hit;
      c.score *= 1.0 + 0.35 * static_cast<double>(hit) / title_words;
    }

    // Metadata coverage: query words the title, tags or summary contain. A guide *about* the
    // query beats one that merely mentions its words in a long body.
    {
      uint32_t mmask = tmask;
      size_t rec = lib_.doc_rec(c.doc);
      std::string_view tags = lib_.str_at(rec + kDocTags);
      std::string_view sum = lib_.str_at(rec + kDocSummary);
      bool tag_phrase = false;
      for (std::string_view field : {tags, sum}) {
        std::vector<Token> ft;
        tokenize(field, ft, 512);
        int prev = -1;
        uint32_t prev_end = 0;
        for (const Token& t : ft) {
          if (t.joined) {
            int ci = clause_of(t.text);
            if (ci >= 0) mmask |= (1u << ci) | clauses_[static_cast<size_t>(ci)].covers;
            continue;
          }
          if (is_stop_word(t.text)) continue;
          int ci = match_token(t.text);
          if (ci >= 0) mmask |= (1u << ci) | clauses_[static_cast<size_t>(ci)].covers;
          // Two different required query words next to each other inside one tag ("not breathing",
          // "infant cpr"): the tag names the query's topic.
          if (field.data() == tags.data() && ci >= 0 && prev >= 0 && ci != prev &&
              ((required_mask_ >> ci) & 1) && ((required_mask_ >> prev) & 1) &&
              field.substr(prev_end, t.begin - prev_end).find('\n') == std::string_view::npos)
            tag_phrase = true;
          prev = ci;
          prev_end = t.end;
        }
      }
      int covered = popcount16(mmask & required_mask_);
      c.meta_cov = n_required_ > 0 ? static_cast<double>(covered) / n_required_ : 0.0;
      if (n_required_ > 0 && covered > 0) {
        double frac = static_cast<double>(covered) / n_required_;
        c.score *= 1.0 + 0.6 * frac * frac;
      }
      if (tag_phrase && n_required_ >= 2) c.score *= 1.15;
    }

    // Body: best window for the snippet and proximity bonus.
    std::string md;
    std::string plain;
    if (rerank_budget_ > 0) {
      size_t work = 0;
      bool ok = lib_.body_prefix(c.doc, std::min(kRerankBodyBytes, rerank_budget_), md, &work);
      rerank_budget_ = work >= rerank_budget_ ? 0 : rerank_budget_ - work;
      if (ok) plain = markdown_to_plain(md);
    }
    // Most bodies open with "# <title>"; don't waste the snippet repeating it.
    {
      size_t b = plain.find_first_not_of(" \n\t");
      if (b != std::string::npos) {
        size_t e = plain.find('\n', b);
        if (e == std::string::npos) e = plain.size();
        if (utf8_lower(std::string_view(plain).substr(b, e - b)) == utf8_lower(title)) plain.erase(0, e);
      }
    }
    std::vector<Token> toks;
    tokenize(plain, toks, kMaxSnippetTokens);
    struct M { uint32_t tok; int clause; };
    std::vector<M> ms;
    for (uint32_t i = 0; i < toks.size(); ++i) {
      if (toks[i].joined) {
        int ci = clause_of(toks[i].text);
        if (ci >= 0) ms.push_back({i, ci});
        continue;
      }
      int ci = match_token(toks[i].text);
      if (ci >= 0) ms.push_back({i, ci});
    }
    size_t best_lo = 0, best_hi = 0;
    int best_cnt = 0;
    size_t best_hits = 0;
    uint32_t min_span = UINT32_MAX;
    {
      int counts[kMaxClauses] = {0};
      int distinct = 0;
      size_t a = 0;
      const uint32_t kWindow = 24;
      for (size_t b = 0; b < ms.size(); ++b) {
        if (counts[ms[b].clause]++ == 0) ++distinct;
        while (ms[b].tok - ms[a].tok > kWindow) {
          if (--counts[ms[a].clause] == 0) --distinct;
          ++a;
        }
        // Most distinct query words first, then the densest window.
        size_t hits = b - a + 1;
        if (distinct > best_cnt || (distinct == best_cnt && hits > best_hits)) {
          best_cnt = distinct;
          best_hits = hits;
          best_lo = a;
          best_hi = b;
        }
      }
      // Smallest span covering all required clauses (phrase / proximity preference).
      std::fill(std::begin(counts), std::end(counts), 0);
      int req = 0;
      a = 0;
      for (size_t b = 0; b < ms.size(); ++b) {
        int cb = ms[b].clause;
        if ((required_mask_ >> cb) & 1) { if (counts[cb]++ == 0) ++req; }
        while (req == n_required_ && a <= b) {
          min_span = std::min(min_span, ms[b].tok - ms[a].tok);
          int ca = ms[a].clause;
          if ((required_mask_ >> ca) & 1) { if (--counts[ca] == 0) --req; }
          ++a;
        }
      }
    }
    if (n_required_ >= 2 && min_span != UINT32_MAX) {
      if (min_span <= static_cast<uint32_t>(n_required_)) c.score *= 1.25;      // adjacent: phrase
      else if (min_span <= 8) c.score *= 1.1;
    }

    if (best_cnt > 0) {
      uint32_t first_b = toks[ms[best_lo].tok].begin;
      uint32_t last_e = toks[ms[best_hi].tok].end;
      for (size_t k = best_lo; k <= best_hi; ++k) {
        const Token& t = toks[ms[k].tok];
        add_term(r.terms, std::string_view(plain).substr(t.begin, t.end - t.begin));
      }
      r.snippet = make_snippet(plain, first_b, last_e);
    } else {
      std::string_view sum = lib_.str_at(lib_.doc_rec(c.doc) + kDocSummary);
      r.snippet = make_snippet(sum, 0, 0);
      std::vector<Token> st;
      tokenize(sum, st, 256);
      for (const Token& t : st)
        if (!t.joined && match_token(t.text) >= 0) add_term(r.terms, sum.substr(t.begin, t.end - t.begin));
    }
    if (r.terms.empty())
      for (const Clause& cl : clauses_) if (!cl.optional) add_term(r.terms, cl.word);
  }

  static bool is_space(char ch) { return ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r'; }

  static std::string make_snippet(std::string_view text, uint32_t match_b, uint32_t match_e) {
    size_t n = text.size();
    size_t span = match_e > match_b ? match_e - match_b : 0;
    size_t lead = span >= kSnippetBytes ? 20 : std::min<size_t>(60, (kSnippetBytes - span) / 2);
    if (match_b > n) match_b = 0;
    size_t start = match_b > lead ? match_b - lead : 0;
    if (start > 0) {
      // Start at a word boundary, and after a line break if one precedes the match.
      while (start < match_b && !is_space(text[start - 1])) ++start;
      for (size_t j = match_b; j > start; --j)
        if (text[j - 1] == '\n') { start = j; break; }
    }
    start = utf8_floor(text, start);
    size_t end = std::min(n, start + kSnippetBytes);
    if (end < n) {
      size_t k = end, steps = 0;
      while (k > start && !is_space(text[k]) && steps < 40) { --k; ++steps; }
      if (k > start && is_space(text[k]) && k >= match_e) end = k;
      end = utf8_floor(text, end);
    }
    std::string out;
    if (start > 0) out += "\xE2\x80\xA6";  // …
    bool sp = false;
    for (size_t i = start; i < end; ++i) {
      char ch = text[i];
      if (is_space(ch)) { sp = !out.empty(); continue; }
      if (sp) { out.push_back(' '); sp = false; }
      out.push_back(ch);
    }
    if (end < n) {
      // Trim trailing whitespace already handled; append ellipsis.
      out += "\xE2\x80\xA6";
    }
    return out;
  }

  void rank(uint32_t limit, std::vector<SearchResult>& out) {
    auto better = [](const Cand& a, const Cand& b) { return a.score != b.score ? a.score > b.score : a.doc < b.doc; };
    size_t k = std::min<size_t>(cand_.size(), std::max<size_t>(static_cast<size_t>(limit) + 5, kMinRerank));
    std::partial_sort(cand_.begin(), cand_.begin() + static_cast<std::ptrdiff_t>(k), cand_.end(), better);
    cand_.resize(k);
    std::vector<SearchResult> res(k);
    for (size_t i = 0; i < k; ++i) rerank(cand_[i], res[i]);
    std::vector<size_t> order(k);
    for (size_t i = 0; i < k; ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return better(cand_[a], cand_[b]); });
    for (size_t i = 0; i < k && out.size() < limit; ++i) {
      size_t j = order[i];
      SearchResult& r = res[j];
      uint32_t d = cand_[j].doc;
      r.id = std::string(lib_.doc_id(d));
      r.title = std::string(lib_.doc_title(d));
      uint32_t cat = lib_.doc_category(d);
      if (cat < lib_.n_cats_)
        r.category = std::string(lib_.str_at(lib_.sec_[kSecCategories].off + static_cast<size_t>(cat) * kCategoryRecordSize));
      r.score = std::round(cand_[j].score * 1000.0) / 1000.0;
      out.push_back(std::move(r));
    }
  }

  // ---- hybrid: keyword + meaning ---------------------------------------------------------------
  // Each guide gets a keyword score kn (BM25 + re-rank, divided by the best one: 0..1) and a meaning
  // score sn (its best passage's cosine blended with its guide vector's cosine, rescaled so the
  // best guide is 1 and the 100th is 0). final = kn + lambda * sn, where lambda moves from
  // lambda_strong to lambda_weak as the keyword match weakens. Strength = the share of the query's
  // words that the best keyword guide carries in its title/tags/summary, times the share of query
  // words found exactly in the index (not only as typo/prefix guesses). So when the words match a
  // guide's own metadata strongly (names, part numbers, exact topics), meaning can only reorder
  // guides whose keyword scores are within lambda_strong of each other; when they don't, meaning
  // decides. Meaning-only guides enter the pool through the top `semantic_pool` of the meaning
  // ranking.
  void hybrid(std::string_view query, uint32_t lo, uint32_t hi, uint32_t limit, bool semantic_only,
              const HybridParams& hp, std::vector<SearchResult>& out) {
    const SemanticIndex& sem = lib_.sem_;
    const uint32_t n = lib_.n_docs_;
    const bool have_kw = build_clauses(query);
    std::vector<float> kn(n, 0.0f);
    std::vector<int32_t> rr(n, -1);  // index into kres of a re-ranked guide
    std::vector<SearchResult> kres;
    std::vector<Cand> kc;
    double strength = 0;
    if (have_kw && !semantic_only) {
      score(lo, hi);
      auto better = [](const Cand& a, const Cand& b) { return a.score != b.score ? a.score > b.score : a.doc < b.doc; };
      double raw_max = 0;
      for (const Cand& c : cand_) raw_max = std::max(raw_max, c.score);
      size_t k = std::min<size_t>(cand_.size(), std::max<size_t>(static_cast<size_t>(limit) + 5, kMinRerank));
      std::partial_sort(cand_.begin(), cand_.begin() + static_cast<std::ptrdiff_t>(k), cand_.end(), better);
      for (size_t i = k; i < cand_.size(); ++i)
        kn[cand_[i].doc] = raw_max > 0 ? static_cast<float>(0.5 * cand_[i].score / raw_max) : 0.0f;
      kc.assign(cand_.begin(), cand_.begin() + static_cast<std::ptrdiff_t>(k));
      kres.resize(k);
      double kmax = 0;
      for (size_t i = 0; i < k; ++i) { rerank(kc[i], kres[i]); kmax = std::max(kmax, kc[i].score); }
      size_t top = 0;
      for (size_t i = 0; i < k; ++i) {
        kn[kc[i].doc] = kmax > 0 ? static_cast<float>(kc[i].score / kmax) : 0.0f;
        rr[kc[i].doc] = static_cast<int32_t>(i);
        if (better(kc[i], kc[top])) top = i;
      }
      if (k > 0 && n_required_ > 0) {
        int exact = 0;
        for (size_t ci = 0; ci < clauses_.size(); ++ci) {
          if (clauses_[ci].optional) continue;
          bool ex = false;
          for (const Alt& a : clauses_[ci].alts) ex = ex || a.weight >= 0.9;
          exact += ex ? 1 : 0;
        }
        strength = kc[top].meta_cov * static_cast<double>(exact) / n_required_;
      }
    }
    // Meaning.
    std::vector<float> sd(n, 0.0f);
    std::vector<uint32_t> bestp(n, kNone);
    SemanticIndex::Query q;
    const bool have_sem = sem.prepare_text(query, q);
    std::vector<uint32_t> sem_order;
    double s_top = 0, s_floor = 0;
    if (have_sem) {
      const uint32_t plo = sem.first(lo), phi = sem.first(hi);
      std::vector<float> ps(phi > plo ? phi - plo : 0);
      if (!ps.empty()) sem.score_passages(q, plo, phi, ps.data());
      std::vector<float> gs;
      if (sem.has_guide_vectors()) { gs.assign(n, 0.0f); sem.score_guides(q, gs.data()); }
      for (uint32_t d = lo; d < hi; ++d) {
        uint32_t a = sem.first(d), b = sem.first(d + 1);
        float best = -2.0f, best_body = -2.0f;
        const bool meta_first = a < b && sem.passage(a).meta;
        for (uint32_t p = a; p < b; ++p) {
          float v = ps[p - plo];
          if (v > best) best = v;
          if (!(meta_first && p == a) && v > best_body) { best_body = v; bestp[d] = p; }
        }
        if (bestp[d] == kNone && a < b) bestp[d] = a;  // only the metadata passage: show the summary
        if (best < 0) best = 0;
        double g = gs.empty() ? best : std::max(0.0f, gs[d]);
        sd[d] = static_cast<float>((1.0 - hp.guide_weight) * best + hp.guide_weight * g);
        sem_order.push_back(d);
      }
      std::sort(sem_order.begin(), sem_order.end(), [&](uint32_t a, uint32_t b) { return sd[a] != sd[b] ? sd[a] > sd[b] : a < b; });
      if (!sem_order.empty()) {
        s_top = sd[sem_order[0]];
        s_floor = sd[sem_order[std::min<size_t>(sem_order.size() - 1, 100)]];
      }
    }
    auto sn = [&](uint32_t d) {
      if (!have_sem || s_top <= s_floor) return 0.0;
      double v = (sd[d] - s_floor) / (s_top - s_floor);
      return v < 0 ? 0.0 : (v > 1 ? 1.0 : v);
    };
    const double lambda = hp.lambda_strong + (hp.lambda_weak - hp.lambda_strong) * (1.0 - strength);
    // Candidate pool.
    std::vector<uint32_t> pool;
    std::vector<bool> in(n, false);
    for (const Cand& c : kc) if (!in[c.doc]) { in[c.doc] = true; pool.push_back(c.doc); }
    for (size_t i = 0; i < sem_order.size() && i < hp.semantic_pool; ++i)
      if (!in[sem_order[i]]) { in[sem_order[i]] = true; pool.push_back(sem_order[i]); }
    std::vector<uint32_t> sem_rank(n, UINT32_MAX);
    for (size_t i = 0; i < sem_order.size() && i < 200; ++i) sem_rank[sem_order[i]] = static_cast<uint32_t>(i);
    std::vector<double> fin(n, 0.0);
    for (uint32_t d : pool) fin[d] = semantic_only ? sd[d] : kn[d] + lambda * sn(d);
    std::sort(pool.begin(), pool.end(), [&](uint32_t a, uint32_t b) { return fin[a] != fin[b] ? fin[a] > fin[b] : a < b; });
    std::string md;
    for (size_t i = 0; i < pool.size() && out.size() < limit; ++i) {
      uint32_t d = pool[i];
      if (fin[d] <= 0) break;
      SearchResult r;
      // Guides found by meaning only skip the keyword re-rank (it would decompress and tokenize
      // the whole body just for a snippet): their snippet is the start of the best passage.
      if (rr[d] >= 0) r = std::move(kres[static_cast<size_t>(rr[d])]);
      r.id = std::string(lib_.doc_id(d));
      r.title = std::string(lib_.doc_title(d));
      uint32_t cat = lib_.doc_category(d);
      if (cat < lib_.n_cats_)
        r.category = std::string(lib_.str_at(lib_.sec_[kSecCategories].off + static_cast<size_t>(cat) * kCategoryRecordSize));
      r.score = std::round((semantic_only ? calibrate_similarity(sd[d]) : fin[d]) * 1000.0) / 1000.0;
      const bool words = kn[d] >= 0.3f;
      const bool meaning = have_sem && (sem_rank[d] < 20 || sn(d) >= 0.6);
      r.match = words && meaning ? "both" : (words || !meaning ? "words" : "meaning");
      if (bestp[d] != kNone) {
        PassageRec pr = sem.passage(bestp[d]);
        md.clear();
        if (!pr.meta && rerank_budget_ > 0) {  // shares the per-query body budget
          size_t work = 0;
          lib_.body_prefix(d, std::min<size_t>(static_cast<size_t>(pr.off) + pr.len, rerank_budget_), md, &work);
          rerank_budget_ = work >= rerank_budget_ ? 0 : rerank_budget_ - work;
        }
        r.passage = lib_.passage_text(d, bestp[d], md);
        r.has_passage = !r.passage.text.empty();
      }
      if (r.snippet.empty()) {
        if (r.has_passage) {
          r.snippet = r.passage.text.substr(0, utf8_floor(r.passage.text, kSnippetBytes));
          if (r.snippet.size() < r.passage.text.size()) {
            size_t sp = r.snippet.rfind(' ');
            if (sp != std::string::npos && sp > kSnippetBytes / 2) r.snippet.resize(sp);
            r.snippet += "\xE2\x80\xA6";
          }
        } else {
          r.snippet = make_snippet(lib_.str_at(lib_.doc_rec(d) + kDocSummary), 0, 0);
        }
        if (r.terms.empty())
          for (const Clause& cl : clauses_) if (!cl.optional) add_term(r.terms, cl.word);
      }
      out.push_back(std::move(r));
    }
  }

  const GuideLibrary& lib_;
  std::vector<Clause> clauses_;
  std::vector<Alt> prefix_tmp_;
  size_t rerank_budget_ = kRerankBudget;
  uint64_t postings_budget_ = kMaxPostingEntries;
  size_t typo_budget_ = kMaxTypoVisits;
  uint32_t required_mask_ = 0;
  int n_required_ = 0;
  std::vector<float> total_, best_;
  std::vector<uint16_t> cover_;
  std::vector<Cand> cand_;
  std::vector<uint64_t> first2_;
  bool one_[256] = {};
};

std::vector<SearchResult> GuideLibrary::search(std::string_view query, std::string_view category,
                                               uint32_t limit) const {
  return search(query, category, limit, SearchMode::kAuto);
}

std::vector<SearchResult> GuideLibrary::search(std::string_view query, std::string_view category, uint32_t limit,
                                               SearchMode mode, const HybridParams& params) const {
  Searcher s(*this);
  if (mode == SearchMode::kAuto) mode = sem_.available() ? SearchMode::kHybrid : SearchMode::kKeyword;
  return s.run(query, category, limit, mode, params);
}

}  // namespace ember::guides
