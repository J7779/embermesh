#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>

#include "tools/tool_impl.h"

namespace ember {
namespace tools {

using namespace detail;

namespace detail {

// ---- formatting ---------------------------------------------------------------------------------
std::string fmt(double v, int d) {
  if (!std::isfinite(v)) return "n/a";
  char b[64];
  std::snprintf(b, sizeof b, "%.*f", d < 0 ? 0 : (d > 9 ? 9 : d), v);
  std::string s = b;
  // "-0", "-0.00" → "0"
  bool allzero = true;
  for (char c : s)
    if (c != '-' && c != '0' && c != '.') { allzero = false; break; }
  if (allzero && !s.empty() && s[0] == '-') s.erase(0, 1);
  return s;
}

std::string fmt_g(double v, int sig) {
  if (!std::isfinite(v)) return "n/a";
  if (v == 0) return "0";
  double a = std::fabs(v);
  if (a >= 1e9 || a < 1e-4) {
    char b[64];
    std::snprintf(b, sizeof b, "%.*e", sig - 1, v);
    return b;
  }
  int mag = static_cast<int>(std::floor(std::log10(a)));
  int dec = sig - 1 - mag;
  if (dec < 0) dec = 0;
  return fmt(v, dec);
}

std::string fmt_metric_len(double m) {
  double a = std::fabs(m);
  if (a < 0.1) return fmt(m * 1000.0, a < 0.01 ? 1 : 0) + " mm";
  if (a < 1000.0) return fmt(m, 3) + " m";
  return fmt(m / 1000.0, 2) + " km";
}

std::string fmt_imperial_len(double m) {
  double inches = m / 0.0254;
  if (std::fabs(inches) >= 5280.0 * 12.0 / 4.0) return fmt(inches / (5280.0 * 12.0), 2) + " mi";
  bool neg = inches < 0;
  long eighths = std::lround(std::fabs(inches) * 8.0);
  long ft = eighths / 96;
  long rem = eighths % 96;
  long whole_in = rem / 8, frac = rem % 8;
  static const char* fr[8] = {"", " 1/8", " 1/4", " 3/8", " 1/2", " 5/8", " 3/4", " 7/8"};
  std::string s = neg ? "-" : "";
  if (ft > 0) {
    s += std::to_string(ft) + " ft";
    if (whole_in || frac) s += " " + std::to_string(whole_in) + fr[frac] + " in";
  } else {
    if (whole_in == 0 && frac) s += std::string(fr[frac]).substr(1) + " in";
    else s += std::to_string(whole_in) + fr[frac] + " in";
  }
  return s;
}

std::string fmt_len(double m) { return fmt_metric_len(m) + " (" + fmt_imperial_len(m) + ")"; }

std::string pad_right(const std::string& s, size_t w) {
  if (s.size() >= w) return s;
  return s + std::string(w - s.size(), ' ');
}

bool parse_number(const std::string& in, double* out) {
  size_t a = 0, b = in.size();
  while (a < b && (in[a] == ' ' || in[a] == '\t')) ++a;
  while (b > a && (in[b - 1] == ' ' || in[b - 1] == '\t')) --b;
  if (a == b || b - a > 64) return false;
  std::string s = in.substr(a, b - a);
  for (char c : s) {
    bool okc = (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E';
    if (!okc) return false;  // rejects "nan", "inf", hex, commas
  }
  char* end = nullptr;
  double v = std::strtod(s.c_str(), &end);
  if (!end || *end != '\0' || !std::isfinite(v)) return false;
  *out = v;
  return true;
}

// ---- field builders -------------------------------------------------------------------------------
FB& FB::def(double v) {
  f.has_default = true;
  f.def = fmt_g(v, 12);
  // trim trailing zeros of a fixed representation
  if (f.def.find('.') != std::string::npos && f.def.find('e') == std::string::npos) {
    while (!f.def.empty() && f.def.back() == '0') f.def.pop_back();
    if (!f.def.empty() && f.def.back() == '.') f.def.pop_back();
  }
  return *this;
}
FB num(const std::string& key, const std::string& label, const std::string& unit) {
  FB b;
  b.f.key = key;
  b.f.label = label;
  b.f.type = "number";
  b.f.unit = unit;
  return b;
}
FB txt(const std::string& key, const std::string& label, const std::string& type) {
  FB b;
  b.f.key = key;
  b.f.label = label;
  b.f.type = type;
  return b;
}
FB sel(const std::string& key, const std::string& label) {
  FB b;
  b.f.key = key;
  b.f.label = label;
  b.f.type = "select";
  return b;
}
FB flag(const std::string& key, const std::string& label, bool d) {
  FB b;
  b.f.key = key;
  b.f.label = label;
  b.f.type = "bool";
  b.f.has_default = true;
  b.f.def = d ? "true" : "false";
  return b;
}

// ---- Ctx ----------------------------------------------------------------------------------------
const Field* Ctx::field(const char* key) {
  for (const auto& f : def_.inputs)
    if (f.key == key) return &f;
  return nullptr;
}

bool Ctx::raw(const char* key, const Field** fo, std::string* s) {
  const Field* f = field(key);
  if (!f) return fail(std::string("internal: unknown field ") + key);
  *fo = f;
  auto it = in_.find(key);
  bool present = it != in_.end();
  bool blank = true;
  if (present)
    for (char c : it->second)
      if (c != ' ' && c != '\t') { blank = false; break; }
  if (present && !blank) {
    *s = it->second;
    return true;
  }
  if (f->has_default) {
    *s = f->def;
    return true;
  }
  return fail(f->label + ": required");
}

bool Ctx::num(const char* key, double* out) {
  const Field* f;
  std::string s;
  if (!raw(key, &f, &s)) return false;
  double v;
  if (!parse_number(s, &v)) return fail(f->label + ": not a number");
  if (f->integer && v != std::floor(v)) return fail(f->label + ": must be a whole number");
  if (f->has_min) {
    if (f->min_exclusive && v <= f->min) return fail(f->label + ": must be greater than " + fmt_g(f->min, 6));
    if (!f->min_exclusive && v < f->min) return fail(f->label + ": must be at least " + fmt_g(f->min, 6));
  }
  if (f->has_max && v > f->max) return fail(f->label + ": must be at most " + fmt_g(f->max, 6));
  *out = v;
  return true;
}

bool Ctx::integer(const char* key, int* out) {
  double v;
  if (!num(key, &v)) return false;
  if (v != std::floor(v) || std::fabs(v) > 2e9) return fail(field(key)->label + ": must be a whole number");
  *out = static_cast<int>(v);
  return true;
}

bool Ctx::text(const char* key, std::string* out) {
  const Field* f;
  std::string s;
  if (!raw(key, &f, &s)) return false;
  if (s.size() > f->max_len) return fail(f->label + ": too long (max " + std::to_string(f->max_len) + " characters)");
  *out = s;
  return true;
}

bool Ctx::choice(const char* key, std::string* out) {
  const Field* f;
  std::string s;
  if (!raw(key, &f, &s)) return false;
  for (const auto& o : f->options)
    if (o.value == s) { *out = s; return true; }
  return fail(f->label + ": unknown option '" + s.substr(0, 40) + "'");
}

bool Ctx::boolean(const char* key, bool* out) {
  const Field* f;
  std::string s;
  if (!raw(key, &f, &s)) return false;
  if (s == "true" || s == "1" || s == "yes" || s == "on") { *out = true; return true; }
  if (s == "false" || s == "0" || s == "no" || s == "off") { *out = false; return true; }
  return fail(f->label + ": must be true or false");
}

void Ctx::out(const std::string& key, const std::string& label, double v, const std::string& unit, int decimals) {
  if (!std::isfinite(v)) {
    fail("internal: " + label + " is not finite");
    return;
  }
  r.outputs.push_back({key, label, fmt(v, decimals), unit});
}
void Ctx::out_s(const std::string& key, const std::string& label, const std::string& v, const std::string& unit) {
  r.outputs.push_back({key, label, v, unit});
}
void Ctx::out_len(const std::string& key, const std::string& label, double m) {
  if (!std::isfinite(m)) {
    fail("internal: " + label + " is not finite");
    return;
  }
  r.outputs.push_back({key, label, fmt_len(m), ""});
}

}  // namespace detail

// ---- registry -----------------------------------------------------------------------------------
namespace {
template <typename F>
void each_entry(F fn) {
  const Entry* (*tables[])(size_t*) = {antenna_tools, rf_tools, power_tools, navigation_tools,
                                        signal_tools, survival_tools, crypto_tools};
  for (auto t : tables) {
    size_t n = 0;
    const Entry* e = t(&n);
    for (size_t i = 0; i < n; ++i)
      if (!fn(e[i])) return;
  }
}
}  // namespace

std::vector<ToolDef> list_tools() {
  std::vector<ToolDef> v;
  each_entry([&](const Entry& e) { v.push_back(e.def()); return true; });
  return v;
}

bool find_tool(const std::string& id, ToolDef* out) {
  bool found = false;
  each_entry([&](const Entry& e) {
    ToolDef d = e.def();
    if (d.id == id) { *out = std::move(d); found = true; return false; }
    return true;
  });
  return found;
}

Result run_tool(const std::string& id, const Inputs& inputs) {
  Result res;
  bool found = false;
  each_entry([&](const Entry& e) {
    ToolDef d = e.def();
    if (d.id != id) return true;
    found = true;
    Ctx c(d, inputs);
    e.run(c);
    res = std::move(c.r);
    return false;
  });
  if (!found) {
    res.error = "unknown tool '" + id.substr(0, 64) + "'";
    res.error_code = "not_found";
  } else if (!res.error.empty()) {
    res.outputs.clear();
    res.text.clear();
    res.notes.clear();
    res.error_code = "bad_params";
  }
  return res;
}

std::string validate_schema(const ToolDef& d) {
  static const char* cats[] = {"antenna", "rf", "power", "navigation", "signal", "survival", "crypto"};
  static const char* types[] = {"number", "text", "select", "bool", "textarea", "password"};
  bool cat_ok = false;
  for (auto c : cats) cat_ok |= d.category == c;
  if (!cat_ok) return d.id + ": bad category '" + d.category + "'";
  if (d.id.empty() || d.title.empty() || d.description.empty()) return d.id + ": missing id/title/description";
  std::set<std::string> keys;
  for (const auto& f : d.inputs) {
    if (f.key.empty() || f.label.empty()) return d.id + ": field without key/label";
    if (!keys.insert(f.key).second) return d.id + ": duplicate key " + f.key;
    bool type_ok = false;
    for (auto t : types) type_ok |= f.type == t;
    if (!type_ok) return d.id + "." + f.key + ": bad type " + f.type;
    if (f.type == "select") {
      if (f.options.empty()) return d.id + "." + f.key + ": select without options";
      if (f.has_default) {
        bool hit = false;
        for (const auto& o : f.options) hit |= o.value == f.def;
        if (!hit) return d.id + "." + f.key + ": default not among options";
      }
    }
    if (f.type == "number" && f.has_default) {
      double v;
      if (!parse_number(f.def, &v)) return d.id + "." + f.key + ": default not numeric";
      if (f.has_min && (v < f.min || (f.min_exclusive && v <= f.min))) return d.id + "." + f.key + ": default below min";
      if (f.has_max && v > f.max) return d.id + "." + f.key + ": default above max";
    }
    if (f.type == "number" && (!f.has_min || !f.has_max)) return d.id + "." + f.key + ": number without range";
    if (f.type == "bool" && f.has_default && f.def != "true" && f.def != "false") return d.id + "." + f.key + ": bad bool default";
  }
  return "";
}

}  // namespace tools
}  // namespace ember
