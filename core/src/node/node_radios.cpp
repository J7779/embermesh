// EmberMesh node — `rf.radios` (docs/RPC_API.md §6): the offline radio profile database.
#include "node/node.h"
#include "rf/radios.h"

namespace ember::node {

RpcResult Node::m_rf_radios(Params& p) {
  std::string query;
  if (!p.str("query", &query, 64, false)) return p.fail();
  Json a = Json::array();
  for (const rf::RadioProfile& r : rf::radio_profiles(query)) {
    Json bands = Json::array(), power = Json::array(), settings = Json::array();
    for (const auto& b : r.bands) bands.push(b);
    for (double w : r.power_w) power.push(w);
    for (const auto& s : r.settings) settings.push(Json::object({{"label", s.label}, {"value", s.value}, {"how", s.how}}));
    Json cfg;
    if (!Json::parse(r.link_config_json, cfg) || !cfg.is_object()) cfg = Json::object();
    Json link = Json::object({{"type", r.link_type}, {"config", std::move(cfg)}});
    Json rec = Json::object({{"link", std::move(link)}});
    if (!r.preset.empty()) rec.set("preset", r.preset);
    Json o = Json::object();
    o.set("id", r.id).set("maker", r.maker).set("model", r.model).set("kind", r.kind);
    o.set("bands", std::move(bands)).set("connector", r.connector).set("vox", r.vox);
    o.set("data_port", r.data_port).set("builtin_tnc", r.builtin_tnc).set("bluetooth", r.bluetooth);
    o.set("power_w", std::move(power)).set("verified", r.verified).set("settings", std::move(settings));
    o.set("cable_notes", r.cable_notes).set("notes", r.notes).set("recommended", std::move(rec));
    a.push(std::move(o));
  }
  return rpc_ok(Json::object({{"radios", std::move(a)}}));
}

}  // namespace ember::node
