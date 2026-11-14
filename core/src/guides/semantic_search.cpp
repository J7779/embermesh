// EmberMesh guides — guides.related and guides.ask (docs/RPC_API.md §7): meaning-based lookups
// over the pack's passage and guide vectors (guides/semantic.h). Extractive only: every text
// returned is quoted from a guide.
#include <algorithm>
#include <cmath>

#include "guides/library.h"
#include "guides/pack_format.h"
#include "guides/text.h"

namespace ember::guides {

namespace {
constexpr double kAskKeywordBoost = 0.4;   // guides the keyword search also finds (tuned on the tune split)
constexpr uint32_t kAskPerGuide = 2;       // at most this many passages from one guide
constexpr uint32_t kRelatedSameBook = 2;   // at most this many chapters of the same book
// Guide-to-guide cosines of whole-guide vectors run higher than question-to-passage ones (long
// texts average towards the corpus mean), so related() uses its own linear calibration: 0.60 (guides
// that merely share a register) -> 0, 0.97 (near duplicates, sibling chapters) -> 1.
constexpr double kRelatedLow = 0.60, kRelatedHigh = 0.97;
double calibrate_related(double c) {
  double s = (c - kRelatedLow) / (kRelatedHigh - kRelatedLow);
  return s < 0 ? 0 : (s > 1 ? 1 : s);
}
}  // namespace

RelatedList GuideLibrary::related(std::string_view id, uint32_t limit) const {
  RelatedList out;
  limit = std::max<uint32_t>(1, std::min<uint32_t>(limit, 50));
  uint32_t doc;
  if (!sem_.has_guide_vectors() || !find_doc(id, doc)) return out;
  SemanticIndex::Query q;
  if (!sem_.prepare_guide(doc, q)) return out;
  std::vector<float> s(n_docs_, 0.0f);
  sem_.score_guides(q, s.data());
  std::vector<uint32_t> order;
  order.reserve(n_docs_);
  for (uint32_t d = 0; d < n_docs_; ++d)
    if (d != doc) order.push_back(d);
  std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return s[a] != s[b] ? s[a] > s[b] : a < b; });
  std::string_view book = str_at(doc_rec(doc) + kDocPartOf);
  uint32_t same_book = 0;
  for (uint32_t d : order) {
    if (out.results.size() >= limit) break;
    if (!book.empty() && str_at(doc_rec(d) + kDocPartOf) == book && ++same_book > kRelatedSameBook) continue;
    RelatedResult r;
    fill_meta(d, r.meta);
    r.score = std::round(calibrate_related(s[d]) * 1000.0) / 1000.0;
    out.results.push_back(std::move(r));
  }
  out.weak = out.results.empty() || out.results.front().score < kWeakScore;
  return out;
}

AskResult GuideLibrary::ask(std::string_view question, std::string_view category, uint32_t limit) const {
  AskResult out;
  limit = std::max<uint32_t>(1, std::min<uint32_t>(limit, 20));
  if (!semantic_available()) { out.weak = true; return out; }
  uint32_t lo = 0, hi = n_docs_;
  if (!category.empty()) {
    int c = find_category(category);
    if (c < 0) { out.weak = true; return out; }
    size_t r = sec_[kSecCategories].off + static_cast<size_t>(c) * kCategoryRecordSize;
    lo = u32(r + 16);
    hi = lo + u32(r + 20);
  }
  SemanticIndex::Query q;
  if (question.size() > 512) question = question.substr(0, utf8_floor(question, 512));
  if (lo >= hi || !sem_.prepare_text(question, q)) { out.weak = true; return out; }
  const uint32_t plo = sem_.first(lo), phi = sem_.first(hi);
  if (phi <= plo) { out.weak = true; return out; }
  std::vector<float> ps(phi - plo);
  sem_.score_passages(q, plo, phi, ps.data());
  // Keyword evidence per guide (a small boost, so wording that matches breaks ties).
  std::vector<float> kn(n_docs_, 0.0f);
  {
    std::vector<SearchResult> kw = search(question, category, 50, SearchMode::kKeyword);
    double top = kw.empty() ? 0 : kw.front().score;
    for (const SearchResult& r : kw) {
      uint32_t d;
      if (top > 0 && find_doc(r.id, d)) kn[d] = static_cast<float>(r.score / top);
    }
  }
  struct C { uint32_t p, doc; float cos; double score; };
  auto better = [](const C& x, const C& y) { return x.score != y.score ? x.score > y.score : x.p < y.p; };
  // Keep only the best kPool candidates (a heap whose top is the worst kept one).
  constexpr size_t kPool = 400;
  std::vector<C> cand;
  cand.reserve(kPool + 1);
  for (uint32_t d = lo; d < hi; ++d) {
    uint32_t a = sem_.first(d), b = sem_.first(d + 1);
    for (uint32_t p = a; p < b; ++p) {
      float v = ps[p - plo];
      if (v <= 0 || (p == a && sem_.passage(p).meta)) continue;
      C c{p, d, v, v + kAskKeywordBoost * kn[d]};
      if (cand.size() < kPool) {
        cand.push_back(c);
        std::push_heap(cand.begin(), cand.end(), better);
      } else if (better(c, cand.front())) {
        std::pop_heap(cand.begin(), cand.end(), better);
        cand.back() = c;
        std::push_heap(cand.begin(), cand.end(), better);
      }
    }
  }
  std::sort(cand.begin(), cand.end(), better);
  const size_t pool = cand.size();
  std::vector<uint32_t> per_guide(n_docs_, 0);
  std::string md;
  size_t budget = 16u << 20;
  for (size_t i = 0; i < pool && out.passages.size() < limit; ++i) {
    const C& c = cand[i];
    if (per_guide[c.doc] >= kAskPerGuide) continue;
    PassageRec pr = sem_.passage(c.p);
    md.clear();
    // Work budget (like search re-ranking): a hostile pack could point passages at the end of
    // huge bodies; real passages need a few KiB each.
    if (budget == 0) break;
    size_t work = 0;
    bool ok = body_prefix(c.doc, std::min<size_t>(static_cast<size_t>(pr.off) + pr.len, budget), md, &work);
    budget = work >= budget ? 0 : budget - work;
    if (!ok) continue;
    PassageText t = passage_text(c.doc, c.p, md);
    if (t.text.empty()) continue;
    ++per_guide[c.doc];
    AskPassage a;
    a.id = std::string(doc_id(c.doc));
    a.title = std::string(doc_title(c.doc));
    uint32_t cat = doc_category(c.doc);
    if (cat < n_cats_) a.category = std::string(str_at(sec_[kSecCategories].off + static_cast<size_t>(cat) * kCategoryRecordSize));
    a.heading = std::move(t.heading);
    a.text = std::move(t.text);
    a.score = std::round(calibrate_similarity(c.cos) * 1000.0) / 1000.0;
    out.passages.push_back(std::move(a));
  }
  double best = 0;
  for (const AskPassage& a : out.passages) best = std::max(best, a.score);
  out.weak = best < kWeakScore;
  return out;
}

}  // namespace ember::guides
