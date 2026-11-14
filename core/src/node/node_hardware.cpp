// EmberMesh node — `rf.hardware` (docs/RPC_API.md §6 "What you need"): hardware per connection method.
#include "node/node.h"
#include "rf/hardware.h"

namespace ember::node {

RpcResult Node::m_rf_hardware(Params& p) {
  std::string platform, method;
  if (!p.str("platform", &platform, 16, false) || !p.str("method", &method, 64, false)) return p.fail();
  if (!platform.empty() && rf::platform_endpoint_kinds(platform).empty())
    return rpc_err("bad_params", "platform must be android, ios, web or desktop");
  Json a = Json::array();
  for (const rf::HardwareMethod& m : rf::hardware_methods(platform, method)) {
    Json plats = Json::array(), items = Json::array(), notes = Json::object();
    for (const auto& x : m.platforms) plats.push(x);
    if (!m.note_android.empty()) notes.set("android", m.note_android);
    if (!m.note_ios.empty()) notes.set("ios", m.note_ios);
    if (!m.note_web.empty()) notes.set("web", m.note_web);
    if (!m.note_desktop.empty()) notes.set("desktop", m.note_desktop);
    for (const rf::HardwareItem& i : m.items) {
      Json alt = Json::array();
      for (const auto& x : i.alternatives) alt.push(x);
      items.push(Json::object({{"name", i.name}, {"why", i.why}, {"required", i.required}, {"alternatives", std::move(alt)}}));
    }
    Json o = Json::object();
    o.set("id", m.id).set("title", m.title).set("what_it_connects", m.what_it_connects).set("link_type", m.link_type);
    if (!m.preset.empty()) o.set("preset", m.preset);
    o.set("platforms", std::move(plats)).set("platform_notes", std::move(notes)).set("items", std::move(items));
    o.set("steps_summary", m.steps_summary).set("manual_chapter", m.manual_chapter).set("proven", m.proven);
    a.push(std::move(o));
  }
  return rpc_ok(Json::object({{"methods", std::move(a)}}));
}

}  // namespace ember::node
