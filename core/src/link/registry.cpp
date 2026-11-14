// EmberMesh core / link — driver registry: create_link() and link_types_json() (ember/link.h).
#include <string>

#include "base/json.h"
#include "ember/link.h"
#include "link/drivers.h"

namespace ember {
namespace link {

const std::vector<const LinkTypeInfo*>& all_types() {
  // Immutable after first use (function-local static of constant data, no mutable global state).
  static const std::vector<const LinkTypeInfo*> types = {
      &raw_type(),   &stream_type(), &kiss_type(),    &audio_type(),
      &meshtastic_type(), &rnode_type(), &lora_at_type(), &rtl_tcp_type(),
  };
  return types;
}

const LinkTypeInfo* find_type(const std::string& type) {
  for (const LinkTypeInfo* t : all_types())
    if (type == t->type) return t;
  return nullptr;
}

}  // namespace link

std::unique_ptr<Link> create_link(const std::string& type, const std::string& config_json, LinkHost& host,
                                  std::string* error) {
  std::string dummy;
  if (!error) error = &dummy;
  const link::LinkTypeInfo* t = link::find_type(type);
  if (!t) {
    *error = "unknown link type '" + utf8_sanitize(utf8_truncate(type, 40)) + "'";
    return nullptr;
  }
  Json cfg;
  if (!link::parse_config(t->schema(), config_json, &cfg, error)) return nullptr;
  if (t->apply_defaults) {
    Json given;
    if (!Json::parse(config_json, given) || !given.is_object()) given = Json::object();
    t->apply_defaults(given, &cfg);
  }
  std::unique_ptr<Link> l = t->create(cfg, host, error);
  if (!l && error->empty()) *error = "invalid configuration";
  return l;
}

bool link::effective_config(const std::string& type, const std::string& config_json, Json* out, std::string* error) {
  const link::LinkTypeInfo* t = link::find_type(type);
  if (!t) {
    if (error) *error = "unknown link type";
    return false;
  }
  if (!link::parse_config(t->schema(), config_json, out, error)) return false;
  if (t->apply_defaults) {
    Json given;
    if (!Json::parse(config_json, given) || !given.is_object()) given = Json::object();
    t->apply_defaults(given, out);
  }
  return true;
}

std::string link_types_json() {
  Json a = Json::array();
  for (const link::LinkTypeInfo* t : link::all_types()) {
    Json kinds = Json::array();
    for (const char* k : t->endpoint_kinds) kinds.push(k);
    Json o = Json::object();
    o.set("type", t->type).set("title", t->title).set("description", t->description);
    o.set("endpoint_kinds", std::move(kinds)).set("config", link::schema_json(t->schema()));
    a.push(std::move(o));
  }
  return a.dump();
}

}  // namespace ember
