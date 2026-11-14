// Small JSON value type for the host layer (the host talks to the core only through ember.h, so it
// cannot reuse core/base/json). Ordered objects, double numbers, depth/size-limited parser.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ember_host {

class Json {
 public:
  enum class Type : uint8_t { Null, Bool, Number, String, Array, Object };

  Json() = default;
  Json(std::nullptr_t) {}
  Json(bool b) : type_(Type::Bool), b_(b) {}
  Json(int v) : type_(Type::Number), n_(v) {}
  Json(unsigned v) : type_(Type::Number), n_(v) {}
  Json(long v) : type_(Type::Number), n_(double(v)) {}
  Json(unsigned long v) : type_(Type::Number), n_(double(v)) {}
  Json(long long v) : type_(Type::Number), n_(double(v)) {}
  Json(unsigned long long v) : type_(Type::Number), n_(double(v)) {}
  Json(double v) : type_(Type::Number), n_(v) {}
  Json(const char* s) : type_(Type::String), s_(s ? s : "") {}
  Json(std::string s) : type_(Type::String), s_(std::move(s)) {}
  Json(std::string_view s) : type_(Type::String), s_(s) {}

  static Json object() { Json j; j.type_ = Type::Object; return j; }
  static Json array() { Json j; j.type_ = Type::Array; return j; }

  // Parses `text`. Returns false (and sets *error) on malformed input, depth > max_depth, or trailing
  // garbage. Never throws.
  static bool parse(std::string_view text, Json* out, std::string* error = nullptr, int max_depth = 64);
  // Convenience: returns Null on error.
  static Json parse_or_null(std::string_view text);

  Type type() const { return type_; }
  bool is_null() const { return type_ == Type::Null; }
  bool is_bool() const { return type_ == Type::Bool; }
  bool is_number() const { return type_ == Type::Number; }
  bool is_string() const { return type_ == Type::String; }
  bool is_array() const { return type_ == Type::Array; }
  bool is_object() const { return type_ == Type::Object; }

  bool as_bool(bool def = false) const { return type_ == Type::Bool ? b_ : def; }
  double as_number(double def = 0) const { return type_ == Type::Number ? n_ : def; }
  int64_t as_int(int64_t def = 0) const;
  const std::string& as_string() const;  // "" if not a string
  std::string as_string(const std::string& def) const { return type_ == Type::String ? s_ : def; }

  // Object access. get() returns a shared Null for missing keys / non-objects.
  const Json& get(std::string_view key) const;
  const Json& operator[](std::string_view key) const { return get(key); }
  bool has(std::string_view key) const;
  Json& set(std::string_view key, Json value);  // replaces existing key; turns Null into Object
  bool erase(std::string_view key);
  const std::vector<std::pair<std::string, Json>>& items() const { return obj_; }

  // Array access.
  const std::vector<Json>& elements() const { return arr_; }
  std::vector<Json>& elements() { return arr_; }
  Json& push(Json value);  // turns Null into Array
  size_t size() const { return type_ == Type::Array ? arr_.size() : type_ == Type::Object ? obj_.size() : 0; }
  const Json& at(size_t i) const;

  std::string dump() const;
  void dump_to(std::string& out) const;

  bool operator==(const Json& o) const;
  bool operator!=(const Json& o) const { return !(*this == o); }

 private:
  Type type_ = Type::Null;
  bool b_ = false;
  double n_ = 0;
  std::string s_;
  std::vector<Json> arr_;
  std::vector<std::pair<std::string, Json>> obj_;
};

// Appends `s` as a quoted, escaped JSON string.
void json_escape_to(std::string& out, std::string_view s);
std::string json_quote(std::string_view s);

}  // namespace ember_host
