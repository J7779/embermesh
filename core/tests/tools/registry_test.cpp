// Registry-wide tests: every tool has a valid schema, runs with its defaults, rejects bad input,
// and never emits NaN/inf no matter which in-range boundary values are fed to it.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

#include "doctest.h"
#include "tools/tool.h"

using namespace ember::tools;

namespace {
bool finite_outputs(const Result& r) {
  for (const auto& o : r.outputs) {
    if (o.value.find("nan") != std::string::npos || o.value.find("inf") != std::string::npos ||
        o.value.find("n/a") != std::string::npos)
      return false;
  }
  return true;
}
std::string num_str(double v) {
  char b[64];
  std::snprintf(b, sizeof b, "%.10g", v);
  return b;
}
bool has_required_without_default(const ToolDef& d) {
  for (const auto& f : d.inputs)
    if (f.required && !f.has_default) return true;
  return false;
}
}  // namespace

TEST_SUITE("tools") {

TEST_CASE("registry: ids unique, schema valid, categories covered") {
  auto tools = list_tools();
  CHECK(tools.size() >= 60);
  std::set<std::string> ids, cats;
  for (const auto& t : tools) {
    CAPTURE(t.id);
    CHECK(ids.insert(t.id).second);
    cats.insert(t.category);
    CHECK(validate_schema(t) == "");
    ToolDef f;
    CHECK(find_tool(t.id, &f));
    CHECK(f.title == t.title);
  }
  for (const char* c : {"antenna", "rf", "power", "navigation", "signal", "survival", "crypto"}) CHECK(cats.count(c));
  ToolDef f;
  CHECK_FALSE(find_tool("nope", &f));
}

TEST_CASE("registry: every tool runs with its defaults") {
  for (const auto& t : list_tools()) {
    CAPTURE(t.id);
    Result r = run_tool(t.id, {});
    if (has_required_without_default(t)) {
      CHECK_FALSE(r.ok());
      CHECK(r.error_code == "bad_params");
      CHECK(r.error.find("required") != std::string::npos);
      continue;
    }
    CHECK(r.error == "");
    CHECK(r.ok());
    CHECK((!r.outputs.empty() || !r.text.empty()));
    CHECK(finite_outputs(r));
  }
}

TEST_CASE("registry: unknown tool and garbage input") {
  Result r = run_tool("does_not_exist", {});
  CHECK(r.error_code == "not_found");
  for (const char* bad : {"abc", "nan", "inf", "-inf", "1e999", "0x10", "1,5", "--1", "1e", "", "   "}) {
    CAPTURE(bad);
    Result x = run_tool("dipole", {{"freq_mhz", bad}});
    if (std::string(bad).find_first_not_of(' ') == std::string::npos) {
      CHECK(x.ok());  // blank → default
    } else {
      CHECK_FALSE(x.ok());
      CHECK(x.error_code == "bad_params");
      CHECK(x.outputs.empty());
    }
  }
  CHECK(run_tool("dipole", {{"freq_mhz", "0"}}).error.find("at least 0.1") != std::string::npos);
  CHECK(run_tool("moxon", {{"wire_diam_mm", "0"}}).error.find("greater than 0") != std::string::npos);
  CHECK_FALSE(run_tool("dipole", {{"freq_mhz", "-146"}}).ok());
  CHECK_FALSE(run_tool("dipole", {{"freq_mhz", "1e9"}}).ok());
  CHECK(run_tool("dipole", {{"freq_mhz", " 146 "}}).ok());
  CHECK_FALSE(run_tool("ground_plane", {{"radials", "3.5"}}).ok());
  CHECK_FALSE(run_tool("yagi", {{"elements", "9"}}).ok());
  CHECK_FALSE(run_tool("random_wire", {{"band_40m", "maybe"}}).ok());
  CHECK(run_tool("random_wire", {{"band_40m", "false"}}).ok());
  std::string huge(5000, 'A');
  CHECK_FALSE(run_tool("nato", {{"text", huge}}).ok());
}

TEST_CASE("registry: boundary sweep never produces NaN/inf") {
  // For each numeric field, try min, max, just inside and just outside the range; all other
  // fields at defaults. Results must be either a clean error or finite outputs.
  for (const auto& t : list_tools()) {
    if (has_required_without_default(t)) continue;
    for (const auto& f : t.inputs) {
      if (f.type != "number") continue;
      CAPTURE(t.id);
      CAPTURE(f.key);
      double span = f.max - f.min;
      std::vector<double> vals = {f.min, f.max, f.min + span * 1e-6, f.max - span * 1e-6, f.min + span / 2};
      if (f.integer) for (auto& v : vals) v = std::round(v);
      for (double v : vals) {
        Result r = run_tool(t.id, {{f.key, num_str(v)}});
        if (r.ok()) CHECK(finite_outputs(r));
        else CHECK(r.error_code == "bad_params");
      }
      Result below = run_tool(t.id, {{f.key, num_str(f.min - std::max(1.0, std::fabs(f.min)))}});
      CHECK_FALSE(below.ok());
      Result above = run_tool(t.id, {{f.key, num_str(f.max + std::max(1.0, std::fabs(f.max)))}});
      CHECK_FALSE(above.ok());
    }
    // Every select option runs cleanly.
    for (const auto& f : t.inputs) {
      if (f.type != "select") continue;
      for (const auto& o : f.options) {
        CAPTURE(t.id);
        CAPTURE(o.value);
        Result r = run_tool(t.id, {{f.key, o.value}});
        if (r.ok()) CHECK(finite_outputs(r));
        else CHECK(r.error_code == "bad_params");
      }
    }
  }
}

}  // TEST_SUITE
