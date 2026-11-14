// EmberMesh node — guides.*, tools.*, rf.bands and tools.qr bindings.
#include <cstdlib>

#include "node/node.h"
#include "node/node_guides.h"
#include "node/node_util.h"
#include "rf/bands.h"
#include "tools/qr.h"
#include "tools/tool.h"

namespace ember::node {

namespace {

Json guide_meta_json(const guides::GuideMeta& m) {
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

Json typed_default(const tools::Field& f) {
  if (f.type == "number") {
    char* end = nullptr;
    double v = std::strtod(f.def.c_str(), &end);
    if (end == f.def.c_str()) return Json(f.def);
    if (v == static_cast<double>(static_cast<int64_t>(v)) && std::fabs(v) < 9e15) return Json(static_cast<int64_t>(v));
    return Json(v);
  }
  if (f.type == "bool") return Json(f.def == "true" || f.def == "1" || f.def == "yes");
  return Json(f.def);
}

Json field_json(const tools::Field& f) {
  Json j = Json::object();
  j.set("key", f.key).set("label", f.label).set("type", f.type);
  if (!f.unit.empty()) j.set("unit", f.unit);
  if (f.has_default) j.set("default", typed_default(f));
  if (f.has_min) j.set("min", f.min);
  if (f.has_max) j.set("max", f.max);
  if (f.has_step) j.set("step", f.step);
  if (!f.options.empty()) {
    Json o = Json::array();
    for (auto& x : f.options) o.push(Json::object({{"value", x.value}, {"label", x.label}}));
    j.set("options", std::move(o));
  }
  if (!f.help.empty()) j.set("help", f.help);
  j.set("required", f.required).set("advanced", f.advanced);
  return j;
}

}  // namespace

RpcResult Node::m_guides_categories(Params&) {
  Json a = Json::array();
  for (auto& c : lib_.categories()) a.push(Json::object({{"id", c.id}, {"title", c.title}, {"count", c.count}}));
  return rpc_ok(Json::object({{"categories", std::move(a)}, {"total", lib_.total()}}));
}

RpcResult Node::m_guides_list(Params& p) {
  std::string cat, part;
  int64_t offset = 0, limit = 100;
  if (!p.str("category", &cat, 64, false) || !p.str("part_of", &part, 256, false) ||
      !p.integer("offset", &offset, 0, 0x7fffffff, false) || !p.integer("limit", &limit, 0, 10000, false))
    return p.fail();
  guides::GuideList l = lib_.list(cat, part, static_cast<uint32_t>(offset), static_cast<uint32_t>(limit));
  Json a = Json::array();
  for (auto& m : l.guides) a.push(guide_meta_json(m));
  return rpc_ok(Json::object({{"guides", std::move(a)}, {"total", l.total}}));
}

RpcResult Node::m_guides_get(Params& p) {
  std::string id;
  if (!p.str("id", &id, 256, true, false)) return p.fail();
  guides::Guide g;
  guides::GetStatus s = lib_.get(id, g);
  if (s == guides::GetStatus::kNotFound) return rpc_err("not_found", "no such guide");
  if (s != guides::GetStatus::kOk) return rpc_err("io_error", "guide data is corrupt");
  Json j = guide_meta_json(g.meta);
  j.set("markdown", g.markdown);
  if (!g.prev.empty()) j.set("prev", g.prev);
  if (!g.next.empty()) j.set("next", g.next);
  return rpc_ok(j);
}

RpcResult Node::m_guides_search(Params& p) { return guides_search_rpc(lib_, p); }
RpcResult Node::m_guides_related(Params& p) { return guides_related_rpc(lib_, p); }
RpcResult Node::m_guides_ask(Params& p) { return guides_ask_rpc(lib_, p); }
RpcResult Node::m_guides_info(Params& p) { return guides_info_rpc(lib_, p); }

RpcResult Node::m_tools_list(Params&) {
  Json a = Json::array();
  for (const tools::ToolDef& d : tools::list_tools()) {
    Json in = Json::array();
    for (auto& f : d.inputs) in.push(field_json(f));
    a.push(Json::object({{"id", d.id},
                         {"title", d.title},
                         {"category", d.category},
                         {"description", d.description},
                         {"inputs", std::move(in)}}));
  }
  return rpc_ok(Json::object({{"tools", std::move(a)}}));
}

RpcResult Node::m_tools_run(Params& p) {
  std::string id;
  const Json* in = nullptr;
  if (!p.str("id", &id, 64, true, false) || !p.object("inputs", &in, false)) return p.fail();
  tools::ToolDef def;
  if (!tools::find_tool(id, &def)) return rpc_err("not_found", "no such tool");
  tools::Inputs inputs;
  if (in) {
    for (const auto& kv : in->members()) {
      const Json& v = kv.second;
      if (v.is_null()) continue;
      if (v.is_string())
        inputs[kv.first] = v.as_string();
      else if (v.is_bool())
        inputs[kv.first] = v.as_bool() ? "true" : "false";
      else if (v.is_number())
        inputs[kv.first] = number_text(v.as_double());
      else
        return rpc_err("bad_params", "inputs." + kv.first + " must be a string, number or bool");
    }
  }
  // Tools that need randomness (one-time pads) get it from the node CSPRNG when the UI omits it.
  for (const auto& f : def.inputs) {
    if (f.key != "random_hex" || inputs.count(f.key)) continue;
    size_t letters = 250;
    auto g = inputs.find("groups");
    if (g != inputs.end()) letters = static_cast<size_t>(std::strtol(g->second.c_str(), nullptr, 10)) * 5;
    size_t bytes = letters * 256 / 234 + letters / 8 + 64;
    size_t cap = f.max_len / 2;
    if (bytes > cap) bytes = cap;
    inputs[f.key] = to_hex(ByteView(rng_.bytes(bytes)));
  }
  tools::Result r = tools::run_tool(id, inputs);
  if (!r.ok()) {
    if (r.error_code == "not_found") return rpc_err("not_found", r.error);
    return rpc_err("bad_params", r.error);
  }
  Json outs = Json::array();
  for (auto& o : r.outputs) {
    Json j = Json::object({{"key", o.key}, {"label", o.label}, {"value", o.value}});
    if (!o.unit.empty()) j.set("unit", o.unit);
    outs.push(std::move(j));
  }
  Json j = Json::object({{"outputs", std::move(outs)}});
  if (!r.text.empty()) j.set("text", r.text);
  if (!r.notes.empty()) {
    Json n = Json::array();
    for (auto& x : r.notes) n.push(x);
    j.set("notes", std::move(n));
  }
  return rpc_ok(j);
}

RpcResult Node::m_rf_bands(Params& p) {
  std::string region, service;
  if (!p.str("region", &region, 16, false) || !p.str("service", &service, 64, false)) return p.fail();
  Json a = Json::array();
  for (const rf::BandService& s : rf::band_services(region, service)) {
    Json ch = Json::array();
    for (const rf::BandChannel& c : s.channels) {
      Json cj = Json::object({{"name", c.name}, {"freq_hz", c.freq_hz}});
      if (c.bw_hz > 0) cj.set("bw_hz", c.bw_hz);
      if (!c.notes.empty()) cj.set("notes", c.notes);
      ch.push(std::move(cj));
    }
    Json sj = Json::object({{"id", s.id}, {"name", s.name}, {"region", s.region}, {"license", s.license}});
    if (s.max_power_w > 0) sj.set("max_power_w", s.max_power_w);
    sj.set("notes", s.notes).set("channels", std::move(ch));
    a.push(std::move(sj));
  }
  Json notes = Json::array();
  for (auto& n : rf::band_general_notes()) notes.push(n);
  Json regions = Json::array();
  for (auto& r : rf::band_regions()) regions.push(r);
  Json matched = Json::array();
  for (auto& r : rf::band_region_tags(region)) matched.push(r);
  return rpc_ok(Json::object({{"services", std::move(a)}, {"notes", std::move(notes)}, {"regions", std::move(regions)},
                              {"matched_regions", std::move(matched)}}));
}

RpcResult Node::m_tools_qr(Params& p) {
  std::string text, ecc_s = "M";
  if (!p.str("text", &text, 4096, true, false) || !p.str("ecc", &ecc_s, 4, false)) return p.fail();
  qr::Ecc ecc;
  if (!qr::parse_ecc(ecc_s, &ecc)) return rpc_err("bad_params", "ecc must be L, M, Q or H");
  qr::QrCode code;
  std::string err;
  if (!qr::encode_text(text, ecc, &code, &err)) return rpc_err("too_large", "text does not fit in a QR code: " + err);
  Json rows = Json::array();
  for (auto& r : code.rows()) rows.push(r);
  return rpc_ok(Json::object({{"size", code.size}, {"rows", std::move(rows)}, {"version", code.version}}));
}

}  // namespace ember::node
