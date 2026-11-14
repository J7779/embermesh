// Internal helpers for tool implementations (not part of the public tools API).
#pragma once

#include <cstddef>
#include <string>

#include "tools/tool.h"

namespace ember {
namespace tools {
namespace detail {

// Fluent Field builder.
struct FB {
  Field f;
  FB& def(double v);
  FB& def_s(const std::string& v) { f.has_default = true; f.def = v; return *this; }
  FB& range(double lo, double hi) { f.has_min = true; f.min = lo; f.has_max = true; f.max = hi; return *this; }
  FB& positive(double hi) { f.has_min = true; f.min = 0; f.min_exclusive = true; f.has_max = true; f.max = hi; return *this; }
  FB& step(double s) { f.has_step = true; f.step = s; return *this; }
  FB& whole() { f.integer = true; f.has_step = true; f.step = 1; return *this; }
  FB& help(const std::string& h) { f.help = h; return *this; }
  FB& req() { f.required = true; return *this; }
  FB& adv() { f.advanced = true; return *this; }
  FB& opt(const std::string& v, const std::string& l) { f.options.push_back({v, l}); return *this; }
  FB& maxlen(size_t n) { f.max_len = n; return *this; }
  operator Field() const { return f; }  // NOLINT(implicit)
};
FB num(const std::string& key, const std::string& label, const std::string& unit = "");
FB txt(const std::string& key, const std::string& label, const std::string& type = "text");
FB sel(const std::string& key, const std::string& label);
FB flag(const std::string& key, const std::string& label, bool def);

// Per-run context: typed, validated access to inputs plus result building.
class Ctx {
 public:
  Ctx(const ToolDef& d, const Inputs& in) : def_(d), in_(in) {}

  bool num(const char* key, double* out);
  bool integer(const char* key, int* out);
  bool text(const char* key, std::string* out);   // text / textarea / password
  bool choice(const char* key, std::string* out); // select (validated against options)
  bool boolean(const char* key, bool* out);
  bool fail(const std::string& msg) {
    if (r.error.empty()) r.error = msg;
    return false;
  }
  bool failed() const { return !r.error.empty(); }

  void out(const std::string& key, const std::string& label, double v, const std::string& unit, int decimals);
  void out_s(const std::string& key, const std::string& label, const std::string& v, const std::string& unit = "");
  void out_len(const std::string& key, const std::string& label, double metres);
  void note(const std::string& s) { r.notes.push_back(s); }

  Result r;

 private:
  const Field* field(const char* key);
  bool raw(const char* key, const Field** f, std::string* s);
  const ToolDef& def_;
  const Inputs& in_;
};

struct Entry {
  ToolDef (*def)();
  void (*run)(Ctx&);
};

// Category tables (each defined in its tools_<category>.cpp).
const Entry* antenna_tools(size_t* n);
const Entry* rf_tools(size_t* n);
const Entry* power_tools(size_t* n);
const Entry* navigation_tools(size_t* n);
const Entry* signal_tools(size_t* n);
const Entry* survival_tools(size_t* n);
const Entry* crypto_tools(size_t* n);

// Formatting helpers (locale-independent printf).
std::string fmt(double v, int decimals);
std::string fmt_g(double v, int sig);           // significant digits, plain notation where sensible
std::string fmt_metric_len(double m);           // "977 mm" / "1.234 m" / "12.35 km"
std::string fmt_imperial_len(double m);         // "38 1/2 in" / "3 ft 2 1/2 in" / "1.25 mi"
std::string fmt_len(double m);                  // "0.977 m (3 ft 2 1/2 in)"
std::string pad_right(const std::string& s, size_t w);
bool parse_number(const std::string& s, double* out);

}  // namespace detail
}  // namespace tools
}  // namespace ember
