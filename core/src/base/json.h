// EmberMesh core — small JSON DOM, strict parser and serializer.
//
// API STABILITY: every core module (node, link, store, tools, capi ...) uses this header. Changes
// must be additive only.
//
//   Json j = Json::object();
//   j.set("name", "Ann").set("age", 41).set("tags", Json::array({"a", "b"}));
//   std::string text = j.dump();                       // {"name":"Ann","age":41,"tags":["a","b"]}
//
//   std::string err;
//   std::optional<Json> p = Json::parse(text, &err);    // strict RFC 8259; never throws
//   if (!p) { ... err ... }
//   std::string name = p->get_string("name", "anon");
//   int64_t age = p->get_int("age", 0);
//   for (const Json& t : p->get("tags").items()) { ... t.as_string() ... }
//   for (const auto& [k, v] : p->members()) { ... }      // insertion order is preserved
//
// Semantics:
//  - Integers that fit in int64 parse as Int, every other number as Double. Doubles that are not
//    finite serialize as null. get_int() accepts an integral Double within range.
//  - Strings are UTF-8 (may contain NUL via \u0000). The parser rejects invalid UTF-8, lone
//    surrogates, raw control characters and duplicate object keys. The serializer always emits valid
//    JSON: invalid UTF-8 in a string is replaced by U+FFFD; U+2028/U+2029 are escaped so the output
//    is also safe to embed in JavaScript source.
//  - Accessors never fail: a missing key / wrong type yields the supplied default or a shared null.
//  - Parser limits (depth, input size, element count, string length) are enforced for hostile input.
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ember {

struct JsonLimits {
  size_t max_depth = 64;              // nesting of arrays/objects
  size_t max_input = 16u << 20;       // bytes of input text
  size_t max_elements = 1u << 20;     // total values in the document
  size_t max_string = 8u << 20;       // bytes of one decoded string
};

class Json {
 public:
  enum class Type : uint8_t { Null, Bool, Int, Double, String, Array, Object };
  using Array = std::vector<Json>;
  using Member = std::pair<std::string, Json>;
  using Object = std::vector<Member>;

  // --- construction ---------------------------------------------------------------------------
  Json() noexcept : t_(Type::Null) { u_.i = 0; }
  Json(std::nullptr_t) noexcept : Json() {}  // NOLINT
  Json(bool b) noexcept : t_(Type::Bool) { u_.b = b; }  // NOLINT
  Json(int v) noexcept : t_(Type::Int) { u_.i = v; }  // NOLINT
  Json(unsigned v) noexcept : t_(Type::Int) { u_.i = v; }  // NOLINT
  Json(long v) noexcept : t_(Type::Int) { u_.i = v; }  // NOLINT
  Json(long long v) noexcept : t_(Type::Int) { u_.i = v; }  // NOLINT
  Json(unsigned long v) noexcept { set_u64(v); }  // NOLINT
  Json(unsigned long long v) noexcept { set_u64(v); }  // NOLINT
  Json(double v) noexcept : t_(Type::Double) { u_.d = v; }  // NOLINT
  Json(float v) noexcept : t_(Type::Double) { u_.d = v; }  // NOLINT
  Json(const char* s);  // NOLINT  (nullptr -> null)
  Json(std::string_view s);  // NOLINT
  Json(const std::string& s);  // NOLINT
  Json(std::string&& s);  // NOLINT
  Json(Array a);  // NOLINT
  Json(Object o);  // NOLINT

  static Json array() { return Json(Array{}); }
  static Json array(std::initializer_list<Json> items) { return Json(Array(items)); }
  static Json object() { return Json(Object{}); }
  // Json::object({{"a", 1}, {"b", "x"}})  (later duplicates replace earlier ones)
  static Json object(std::initializer_list<Member> members);

  Json(const Json& o);
  Json(Json&& o) noexcept;
  Json& operator=(const Json& o);
  Json& operator=(Json&& o) noexcept;
  ~Json() { destroy(); }

  // --- type queries ---------------------------------------------------------------------------
  Type type() const { return t_; }
  bool is_null() const { return t_ == Type::Null; }
  bool is_bool() const { return t_ == Type::Bool; }
  bool is_int() const { return t_ == Type::Int; }
  bool is_double() const { return t_ == Type::Double; }
  bool is_number() const { return t_ == Type::Int || t_ == Type::Double; }
  bool is_string() const { return t_ == Type::String; }
  bool is_array() const { return t_ == Type::Array; }
  bool is_object() const { return t_ == Type::Object; }
  static const char* type_name(Type t);

  // --- scalar access (never fails) ------------------------------------------------------------
  bool as_bool(bool dflt = false) const { return t_ == Type::Bool ? u_.b : dflt; }
  // Int, or a Double with an exact integral value representable in int64.
  int64_t as_int(int64_t dflt = 0) const;
  bool get_int_if(int64_t* out) const;  // true if as_int would not use the default
  double as_double(double dflt = 0) const;
  // Returns the string or an empty string for non-strings.
  const std::string& as_string() const;
  std::string as_string(std::string_view dflt) const;

  // --- arrays ---------------------------------------------------------------------------------
  size_t size() const;  // elements of array / members of object, else 0
  bool empty() const { return size() == 0; }
  const Json& operator[](size_t i) const;  // shared null when out of range / not an array
  const Json& at(size_t i) const { return (*this)[i]; }
  // Mutable array; converts null to an empty array. Non-array, non-null values are replaced.
  Json& push(Json v);
  // Iteration: empty for non-arrays.
  const Array& items() const;
  Array& items_mut();  // converts to array like push()

  // --- objects --------------------------------------------------------------------------------
  bool has(std::string_view key) const { return find(key) != nullptr; }
  const Json* find(std::string_view key) const;
  Json* find(std::string_view key);
  const Json& get(std::string_view key) const;  // shared null when missing
  const Json& operator[](std::string_view key) const { return get(key); }  // j["k"] and j[0] both work
  // Inserts or replaces (keeps the original position on replace). Converts null to an object.
  // Returns *this so calls can be chained.
  Json& set(std::string_view key, Json v);
  // Mutable member reference, inserting null if missing. Converts null/non-object to object.
  Json& at_key(std::string_view key);
  bool erase(std::string_view key);
  const Object& members() const;
  Object& members_mut();

  // Typed getters with defaults (wrong type -> default).
  std::string get_string(std::string_view key, std::string_view dflt = "") const;
  int64_t get_int(std::string_view key, int64_t dflt = 0) const;
  double get_double(std::string_view key, double dflt = 0) const;
  bool get_bool(std::string_view key, bool dflt = false) const;
  // Strict-ish typed lookups: true when present with the right type.
  bool get_string_if(std::string_view key, std::string* out) const;
  bool get_int_if(std::string_view key, int64_t* out) const;
  bool get_double_if(std::string_view key, double* out) const;
  bool get_bool_if(std::string_view key, bool* out) const;

  // --- comparison -----------------------------------------------------------------------------
  // Deep equality; objects compare member-by-member in order. Int 1 == Double 1.0.
  friend bool operator==(const Json& a, const Json& b);
  friend bool operator!=(const Json& a, const Json& b) { return !(a == b); }

  // --- text -----------------------------------------------------------------------------------
  // Compact when indent < 0; otherwise pretty-printed with `indent` spaces.
  std::string dump(int indent = -1) const;
  void dump_to(std::string& out, int indent = -1) const;

  static std::optional<Json> parse(std::string_view text, std::string* error = nullptr,
                                   const JsonLimits& limits = JsonLimits());
  static bool parse(std::string_view text, Json& out, std::string* error = nullptr,
                    const JsonLimits& limits = JsonLimits());

  // Append `s` as a JSON string literal (with quotes) to out.
  static void quote(std::string_view s, std::string& out);
  static std::string quote(std::string_view s) { std::string o; quote(s, o); return o; }

 private:
  void set_u64(unsigned long long v) noexcept;
  void destroy() noexcept;
  void copy_from(const Json& o);
  void move_from(Json& o) noexcept;
  void dump_impl(std::string& out, int indent, int level) const;

  Type t_;
  union U {
    bool b;
    int64_t i;
    double d;
    std::string* s;
    Array* a;
    Object* o;
  } u_;
};

// UTF-8 helpers used by the JSON code; exposed for other modules (text sanitising, truncation).
// True when `s` is well-formed UTF-8 (no overlongs, no surrogates, <= U+10FFFF).
bool utf8_valid(std::string_view s);
// Replaces every invalid sequence with U+FFFD.
std::string utf8_sanitize(std::string_view s);
// Longest prefix of `s` with at most `max_bytes` bytes that does not split a code point
// (assumes valid UTF-8; invalid input is truncated at byte level).
std::string_view utf8_truncate(std::string_view s, size_t max_bytes);

}  // namespace ember
