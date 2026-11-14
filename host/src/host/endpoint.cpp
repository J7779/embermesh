#include "host/endpoint.h"

#include <cstdlib>

#include "host/util.h"

namespace ember_host {

std::string cfg_str(const Json& cfg, const char* key, const std::string& def) {
  const Json& v = cfg.get(key);
  if (v.is_string()) return v.as_string();
  if (v.is_number()) {
    Json n = v;
    return n.dump();
  }
  if (v.is_bool()) return v.as_bool() ? "true" : "false";
  return def;
}

double cfg_num(const Json& cfg, const char* key, double def) {
  const Json& v = cfg.get(key);
  if (v.is_number()) return v.as_number();
  if (v.is_string()) {
    std::string s = trim(v.as_string());
    if (s.empty()) return def;
    char* end = nullptr;
    double d = std::strtod(s.c_str(), &end);
    if (end && *end == 0) return d;
  }
  if (v.is_bool()) return v.as_bool() ? 1 : 0;
  return def;
}

int64_t cfg_int(const Json& cfg, const char* key, int64_t def) {
  double d = cfg_num(cfg, key, double(def));
  if (!(d >= -9.2e18 && d <= 9.2e18)) return def;
  return int64_t(d);
}

bool cfg_bool(const Json& cfg, const char* key, bool def) {
  const Json& v = cfg.get(key);
  if (v.is_bool()) return v.as_bool();
  if (v.is_number()) return v.as_number() != 0;
  if (v.is_string()) {
    std::string s = to_lower(trim(v.as_string()));
    if (s == "true" || s == "1" || s == "yes" || s == "on") return true;
    if (s == "false" || s == "0" || s == "no" || s == "off") return false;
  }
  return def;
}

Json cfg_with_defaults(const Json& fields, const Json& cfg) {
  Json out = cfg.is_object() ? cfg : Json::object();
  for (const auto& f : fields.elements()) {
    const std::string& k = f.get("key").as_string();
    if (k.empty() || out.has(k)) continue;
    if (f.has("default")) out.set(k, f.get("default"));
  }
  return out;
}

EndpointRegistry make_default_registry() {
  EndpointRegistry r;
  register_net_endpoints(r);
  auto unavailable = [&r](const char* kind, const char* title, const char* why) {
    EndpointKind k;
    k.kind = kind;
    k.title = title;
    k.available = false;
    k.unavailable_reason = why;
    r.add(std::move(k));
  };
  (void)unavailable;
#ifdef EMBER_HOST_HAVE_SERIAL
  register_serial_endpoints(r);
#else
  unavailable("serial", "Serial port", "this build has no serial support (EMBER_HOST_SERIAL=OFF)");
#endif
#ifdef EMBER_HOST_HAVE_AUDIO
  register_audio_endpoints(r);
#if !defined(_WIN32)
  register_audio_pipe_endpoints(r);
#endif
#else
  unavailable("audio", "Sound card", "this build has no audio support (EMBER_HOST_AUDIO=OFF)");
#endif
  register_sdr_endpoints(r);
  return r;
}

}  // namespace ember_host
