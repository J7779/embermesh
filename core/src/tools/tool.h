// EmberMesh core — schema-driven calculator/utility registry (mirrors RPC tools.list / tools.run).
//
// Binding notes for the RPC layer:
//  * tools.list → list_tools(); serialise each Field exactly as docs/RPC_API.md "Field schema".
//    Field::def is a string; emit it as a JSON number for type "number", bool for "bool", string
//    otherwise (has_default == false → omit "default"). has_min/has_max/has_step gate min/max/step.
//  * tools.run → run_tool(id, inputs). Convert the JSON inputs object to strings first (numbers via
//    shortest round-trip formatting, bools as "true"/"false"). Result::error_code is "" on success,
//    "not_found" for an unknown id, "bad_params" for invalid input; map to the RPC error envelope.
//    On success emit {outputs:[{key,label,value,unit?}], text?, notes?}.
#pragma once

#include <map>
#include <string>
#include <vector>

namespace ember {
namespace tools {

struct Option {
  std::string value;
  std::string label;
};

struct Field {
  std::string key;
  std::string label;
  std::string type = "number";  // number | text | select | bool | textarea | password
  std::string unit;
  bool has_default = false;
  std::string def;              // default, as text
  bool has_min = false;
  double min = 0;
  bool min_exclusive = false;   // internal: value must be > min (not part of the RPC schema)
  bool has_max = false;
  double max = 0;
  bool has_step = false;
  double step = 0;
  std::vector<Option> options;
  std::string help;
  bool required = false;
  bool advanced = false;
  bool integer = false;         // internal: must be a whole number
  size_t max_len = 4096;        // internal: text length cap
};

struct ToolDef {
  std::string id;
  std::string title;
  std::string category;  // antenna | rf | power | navigation | signal | survival | crypto
  std::string description;
  std::vector<Field> inputs;
};

struct Output {
  std::string key;
  std::string label;
  std::string value;
  std::string unit;
};

struct Result {
  std::vector<Output> outputs;
  std::string text;
  std::vector<std::string> notes;
  std::string error;       // human-readable, empty on success
  std::string error_code;  // "", "bad_params", "not_found"
  bool ok() const { return error.empty(); }
  const Output* find(const std::string& key) const {
    for (const auto& o : outputs)
      if (o.key == key) return &o;
    return nullptr;
  }
};

using Inputs = std::map<std::string, std::string>;

std::vector<ToolDef> list_tools();
bool find_tool(const std::string& id, ToolDef* out);
Result run_tool(const std::string& id, const Inputs& inputs);

// Validates a ToolDef's own schema (used by tests): unique keys, select defaults among options,
// defaults inside min/max, known type and category. Returns "" if fine, else a message.
std::string validate_schema(const ToolDef& def);

}  // namespace tools
}  // namespace ember
