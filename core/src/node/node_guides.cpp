#include "node/node_guides.h"

#include <string>

namespace ember::node {

Json guide_meta_to_json(const guides::GuideMeta& m) {
  Json j = Json::object();
  j.set("id", m.id).set("title", m.title).set("category", m.category).set("summary", m.summary);
  Json tags = Json::array();
  for (auto& t : m.tags) tags.push(t);
  j.set("tags", std::move(tags));
  j.set("source", m.source).set("source_title", m.source_title).set("license", m.license);
  j.set("attribution", m.attribution).set("bytes", m.bytes);
  if (!m.part_of.empty()) j.set("part_of", m.part_of);
  if (m.has_order) j.set("order", m.order);
  return j;
}

RpcResult guides_search_rpc(const guides::GuideLibrary& lib, Params& p) {
  std::string q, cat;
  int64_t limit = 20;
  bool semantic = true;
  if (!p.str("query", &q, 512, true) || !p.str("category", &cat, 64, false) ||
      !p.integer("limit", &limit, 1, 200, false) || !p.boolean("semantic", &semantic))
    return p.fail();
  Json a = Json::array();
  guides::SearchMode mode = semantic ? guides::SearchMode::kAuto : guides::SearchMode::kKeyword;
  for (auto& r : lib.search(q, cat, static_cast<uint32_t>(limit), mode)) {
    Json t = Json::array();
    for (auto& x : r.terms) t.push(x);
    Json o = Json::object({{"id", r.id},
                           {"title", r.title},
                           {"category", r.category},
                           {"score", r.score},
                           {"snippet", r.snippet},
                           {"terms", std::move(t)},
                           {"match", r.match}});
    if (r.has_passage) {
      Json ps = Json::object({{"text", r.passage.text}});
      if (!r.passage.heading.empty()) ps.set("heading", r.passage.heading);
      o.set("passage", std::move(ps));
    }
    a.push(std::move(o));
  }
  return rpc_ok(Json::object({{"results", std::move(a)}}));
}

RpcResult guides_related_rpc(const guides::GuideLibrary& lib, Params& p) {
  std::string id;
  int64_t limit = 8;
  if (!p.str("id", &id, 256, true, false) || !p.integer("limit", &limit, 1, 50, false)) return p.fail();
  guides::RelatedList r = lib.related(id, static_cast<uint32_t>(limit));
  Json a = Json::array();
  for (auto& x : r.results) {
    Json j = guide_meta_to_json(x.meta);
    j.set("score", x.score);
    a.push(std::move(j));
  }
  Json out = Json::object({{"results", std::move(a)}});
  if (r.weak) out.set("weak", true);
  return rpc_ok(std::move(out));
}

RpcResult guides_ask_rpc(const guides::GuideLibrary& lib, Params& p) {
  std::string q, cat;
  int64_t limit = 5;
  if (!p.str("question", &q, 512, true) || !p.str("category", &cat, 64, false) ||
      !p.integer("limit", &limit, 1, 20, false))
    return p.fail();
  guides::AskResult r = lib.ask(q, cat, static_cast<uint32_t>(limit));
  Json a = Json::array();
  for (auto& x : r.passages) {
    Json j = Json::object({{"id", x.id}, {"title", x.title}, {"category", x.category}});
    if (!x.heading.empty()) j.set("heading", x.heading);
    j.set("text", x.text).set("score", x.score);
    a.push(std::move(j));
  }
  Json out = Json::object({{"passages", std::move(a)}});
  if (r.weak) out.set("weak", true);
  return rpc_ok(std::move(out));
}

RpcResult guides_info_rpc(const guides::GuideLibrary& lib, Params&) {
  guides::LibraryInfo li = lib.info();
  Json sem = Json::object({{"available", li.semantic.available}});
  if (li.semantic.available) {
    sem.set("model", li.semantic.model);
    sem.set("dims", li.semantic.dims);
    sem.set("bits", li.semantic.bits);
    sem.set("passages", li.semantic.passages);
  }
  Json lic = Json::array();
  for (auto& l : li.licences) lic.push(Json::object({{"name", l.first}, {"count", l.second}}));
  return rpc_ok(Json::object({{"guides", li.guides},
                              {"categories", li.categories},
                              {"bytes", static_cast<int64_t>(li.bytes)},
                              {"semantic", std::move(sem)},
                              {"licences", std::move(lic)}}));
}

}  // namespace ember::node
