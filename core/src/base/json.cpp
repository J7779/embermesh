#include "base/json.h"

#include <algorithm>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace ember {

namespace {

const Json& shared_null() {
  static const Json n;
  return n;
}
const std::string& shared_empty_string() {
  static const std::string s;
  return s;
}
const Json::Array& shared_empty_array() {
  static const Json::Array a;
  return a;
}
const Json::Object& shared_empty_object() {
  static const Json::Object o;
  return o;
}

char locale_decimal_point() {
  const std::lconv* lc = std::localeconv();
  if (lc && lc->decimal_point && lc->decimal_point[0]) return lc->decimal_point[0];
  return '.';
}

// Locale-independent strtod over a bounded ASCII buffer already validated against JSON grammar.
bool parse_double(std::string_view text, double* out) {
  char buf[4100];
  if (text.size() >= sizeof(buf)) return false;
  char dp = locale_decimal_point();
  for (size_t i = 0; i < text.size(); ++i) buf[i] = (text[i] == '.') ? dp : text[i];
  buf[text.size()] = 0;
  char* end = nullptr;
  double v = std::strtod(buf, &end);
  if (end != buf + text.size()) return false;
  if (!std::isfinite(v)) return false;
  *out = v;
  return true;
}

// Decodes one UTF-8 code point at s[i]; returns its byte length (1-4) or 0 if invalid.
size_t utf8_decode(std::string_view s, size_t i, uint32_t* cp) {
  const auto* p = reinterpret_cast<const unsigned char*>(s.data());
  size_t n = s.size();
  unsigned char c = p[i];
  if (c < 0x80) { *cp = c; return 1; }
  size_t len;
  uint32_t v;
  if (c >= 0xC2 && c <= 0xDF) { len = 2; v = c & 0x1F; }
  else if (c >= 0xE0 && c <= 0xEF) { len = 3; v = c & 0x0F; }
  else if (c >= 0xF0 && c <= 0xF4) { len = 4; v = c & 0x07; }
  else return 0;
  if (n - i < len) return 0;
  for (size_t k = 1; k < len; ++k) {
    unsigned char cc = p[i + k];
    if ((cc & 0xC0) != 0x80) return 0;
    v = (v << 6) | (cc & 0x3F);
  }
  if (len == 3 && (v < 0x800 || (v >= 0xD800 && v <= 0xDFFF))) return 0;
  if (len == 4 && (v < 0x10000 || v > 0x10FFFF)) return 0;
  *cp = v;
  return len;
}

void utf8_append(std::string& out, uint32_t cp) {
  if (cp < 0x80) {
    out.push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

// ---------------------------------------------------------------------------------------------
class Parser {
 public:
  Parser(std::string_view s, const JsonLimits& lim) : s_(s), lim_(lim) {}

  bool run(Json& out, std::string* err) {
    if (s_.size() > lim_.max_input) return fail("input too large", err);
    skip_ws();
    if (!value(out, 0)) return fail(nullptr, err);
    skip_ws();
    if (pos_ != s_.size()) { error_ = "trailing characters"; return fail(nullptr, err); }
    return true;
  }

 private:
  bool fail(const char* msg, std::string* err) {
    if (msg) error_ = msg;
    if (err) {
      *err = error_.empty() ? "parse error" : error_;
      *err += " at offset ";
      *err += std::to_string(pos_);
    }
    return false;
  }
  bool err(const char* m) { if (error_.empty()) error_ = m; return false; }

  void skip_ws() {
    while (pos_ < s_.size()) {
      char c = s_[pos_];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
      else break;
    }
  }

  bool lit(const char* word) {
    size_t n = std::strlen(word);
    if (s_.size() - pos_ < n || s_.compare(pos_, n, word) != 0) return err("invalid literal");
    pos_ += n;
    return true;
  }

  bool value(Json& out, size_t depth) {
    if (++elements_ > lim_.max_elements) return err("too many elements");
    if (pos_ >= s_.size()) return err("unexpected end of input");
    char c = s_[pos_];
    switch (c) {
      case 'n': if (!lit("null")) return false; out = Json(); return true;
      case 't': if (!lit("true")) return false; out = Json(true); return true;
      case 'f': if (!lit("false")) return false; out = Json(false); return true;
      case '"': {
        std::string str;
        if (!string(str)) return false;
        out = Json(std::move(str));
        return true;
      }
      case '[': return array(out, depth + 1);
      case '{': return object(out, depth + 1);
      default:
        if (c == '-' || (c >= '0' && c <= '9')) return number(out);
        return err("unexpected character");
    }
  }

  bool number(Json& out) {
    size_t start = pos_;
    bool neg = false;
    if (s_[pos_] == '-') { neg = true; ++pos_; }
    if (pos_ >= s_.size()) return err("bad number");
    if (s_[pos_] == '0') {
      ++pos_;
    } else if (s_[pos_] >= '1' && s_[pos_] <= '9') {
      while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
    } else {
      return err("bad number");
    }
    size_t int_end = pos_;
    bool is_float = false;
    if (pos_ < s_.size() && s_[pos_] == '.') {
      ++pos_;
      size_t d = pos_;
      while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
      if (pos_ == d) return err("bad number");
      is_float = true;
    }
    if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
      size_t d = pos_;
      while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
      if (pos_ == d) return err("bad number");
      is_float = true;
    }
    std::string_view text = s_.substr(start, pos_ - start);
    if (text.size() > 4096) return err("number too long");
    if (!is_float) {
      // Exact integer path with overflow detection; falls back to double when out of range.
      uint64_t mag = 0;
      bool overflow = false;
      for (size_t i = start + (neg ? 1 : 0); i < int_end; ++i) {
        uint64_t dgt = static_cast<uint64_t>(s_[i] - '0');
        if (mag > (UINT64_MAX - dgt) / 10) { overflow = true; break; }
        mag = mag * 10 + dgt;
      }
      if (!overflow) {
        if (!neg && mag <= static_cast<uint64_t>(INT64_MAX)) {
          out = Json(static_cast<long long>(mag));
          return true;
        }
        if (neg && mag <= static_cast<uint64_t>(INT64_MAX) + 1) {
          out = Json(mag == static_cast<uint64_t>(INT64_MAX) + 1
                         ? std::numeric_limits<long long>::min()
                         : -static_cast<long long>(mag));
          return true;
        }
      }
    }
    double d;
    if (!parse_double(text, &d)) return err("number out of range");
    out = Json(d);
    return true;
  }

  bool hex4(uint32_t* v) {
    if (s_.size() - pos_ < 4) return err("bad \\u escape");
    uint32_t r = 0;
    for (int i = 0; i < 4; ++i) {
      char c = s_[pos_++];
      r <<= 4;
      if (c >= '0' && c <= '9') r |= static_cast<uint32_t>(c - '0');
      else if (c >= 'a' && c <= 'f') r |= static_cast<uint32_t>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') r |= static_cast<uint32_t>(c - 'A' + 10);
      else return err("bad \\u escape");
    }
    *v = r;
    return true;
  }

  bool string(std::string& out) {
    ++pos_;  // opening quote
    for (;;) {
      if (pos_ >= s_.size()) return err("unterminated string");
      if (out.size() > lim_.max_string) return err("string too long");
      unsigned char c = static_cast<unsigned char>(s_[pos_]);
      if (c == '"') { ++pos_; return true; }
      if (c < 0x20) return err("control character in string");
      if (c == '\\') {
        ++pos_;
        if (pos_ >= s_.size()) return err("unterminated string");
        char e = s_[pos_++];
        switch (e) {
          case '"': out.push_back('"'); break;
          case '\\': out.push_back('\\'); break;
          case '/': out.push_back('/'); break;
          case 'b': out.push_back('\b'); break;
          case 'f': out.push_back('\f'); break;
          case 'n': out.push_back('\n'); break;
          case 'r': out.push_back('\r'); break;
          case 't': out.push_back('\t'); break;
          case 'u': {
            uint32_t cp;
            if (!hex4(&cp)) return false;
            if (cp >= 0xDC00 && cp <= 0xDFFF) return err("lone low surrogate");
            if (cp >= 0xD800 && cp <= 0xDBFF) {
              if (s_.size() - pos_ < 6 || s_[pos_] != '\\' || s_[pos_ + 1] != 'u')
                return err("lone high surrogate");
              pos_ += 2;
              uint32_t lo;
              if (!hex4(&lo)) return false;
              if (lo < 0xDC00 || lo > 0xDFFF) return err("bad surrogate pair");
              cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            }
            utf8_append(out, cp);
            break;
          }
          default: return err("bad escape");
        }
        continue;
      }
      if (c < 0x80) { out.push_back(static_cast<char>(c)); ++pos_; continue; }
      uint32_t cp;
      size_t n = utf8_decode(s_, pos_, &cp);
      if (!n) return err("invalid UTF-8");
      out.append(s_.data() + pos_, n);
      pos_ += n;
    }
  }

  bool array(Json& out, size_t depth) {
    if (depth > lim_.max_depth) return err("nesting too deep");
    ++pos_;
    Json::Array arr;
    skip_ws();
    if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; out = Json(std::move(arr)); return true; }
    for (;;) {
      skip_ws();
      Json v;
      if (!value(v, depth)) return false;
      arr.push_back(std::move(v));
      skip_ws();
      if (pos_ >= s_.size()) return err("unterminated array");
      char c = s_[pos_++];
      if (c == ',') continue;
      if (c == ']') break;
      --pos_;
      return err("expected , or ]");
    }
    out = Json(std::move(arr));
    return true;
  }

  bool object(Json& out, size_t depth) {
    if (depth > lim_.max_depth) return err("nesting too deep");
    ++pos_;
    Json::Object obj;
    skip_ws();
    if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; out = Json(std::move(obj)); return true; }
    for (;;) {
      skip_ws();
      if (pos_ >= s_.size() || s_[pos_] != '"') return err("expected string key");
      std::string key;
      if (!string(key)) return false;
      skip_ws();
      if (pos_ >= s_.size() || s_[pos_] != ':') return err("expected :");
      ++pos_;
      skip_ws();
      Json v;
      if (!value(v, depth)) return false;
      if (obj.size() < 16) {
        for (const auto& m : obj)
          if (m.first == key) return err("duplicate key");
      }
      obj.emplace_back(std::move(key), std::move(v));
      skip_ws();
      if (pos_ >= s_.size()) return err("unterminated object");
      char c = s_[pos_++];
      if (c == ',') continue;
      if (c == '}') break;
      --pos_;
      return err("expected , or }");
    }
    if (obj.size() > 16) {
      std::vector<const std::string*> keys;
      keys.reserve(obj.size());
      for (const auto& m : obj) keys.push_back(&m.first);
      std::sort(keys.begin(), keys.end(),
                [](const std::string* a, const std::string* b) { return *a < *b; });
      for (size_t i = 1; i < keys.size(); ++i)
        if (*keys[i] == *keys[i - 1]) return err("duplicate key");
    }
    out = Json(std::move(obj));
    return true;
  }

  std::string_view s_;
  const JsonLimits& lim_;
  size_t pos_ = 0;
  size_t elements_ = 0;
  std::string error_;
};

void format_double(double d, std::string& out) {
  if (!std::isfinite(d)) { out += "null"; return; }
  char buf[40];
  char dp = locale_decimal_point();
  for (int prec = 15; prec <= 17; ++prec) {
    std::snprintf(buf, sizeof buf, "%.*g", prec, d);
    if (prec == 17) break;
    char* end = nullptr;
    if (std::strtod(buf, &end) == d) break;
  }
  bool needs_frac = true;
  for (char* p = buf; *p; ++p) {
    if (*p == dp && dp != '.') *p = '.';
    if (*p == '.' || *p == 'e' || *p == 'E') needs_frac = false;
  }
  out += buf;
  if (needs_frac) out += ".0";
}

void newline(std::string& out, int indent, int level) {
  if (indent < 0) return;
  out.push_back('\n');
  out.append(static_cast<size_t>(indent) * static_cast<size_t>(level), ' ');
}

}  // namespace

// --- UTF-8 helpers ------------------------------------------------------------------------------

bool utf8_valid(std::string_view s) {
  size_t i = 0;
  while (i < s.size()) {
    uint32_t cp;
    size_t n = utf8_decode(s, i, &cp);
    if (!n) return false;
    i += n;
  }
  return true;
}

std::string utf8_sanitize(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  size_t i = 0;
  while (i < s.size()) {
    uint32_t cp;
    size_t n = utf8_decode(s, i, &cp);
    if (!n) { out += "\xEF\xBF\xBD"; ++i; continue; }
    out.append(s.data() + i, n);
    i += n;
  }
  return out;
}

std::string_view utf8_truncate(std::string_view s, size_t max_bytes) {
  if (s.size() <= max_bytes) return s;
  size_t n = max_bytes;
  // Back up over continuation bytes so we do not split a sequence.
  size_t back = 0;
  while (n > 0 && back < 3 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) { --n; ++back; }
  if ((static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) n = max_bytes;  // invalid input
  return s.substr(0, n);
}

// --- Json ---------------------------------------------------------------------------------------

Json::Json(const char* s) {
  if (!s) { t_ = Type::Null; u_.i = 0; return; }
  t_ = Type::String;
  u_.s = new std::string(s);
}
Json::Json(std::string_view s) : t_(Type::String) { u_.s = new std::string(s); }
Json::Json(const std::string& s) : t_(Type::String) { u_.s = new std::string(s); }
Json::Json(std::string&& s) : t_(Type::String) { u_.s = new std::string(std::move(s)); }
Json::Json(Array a) : t_(Type::Array) { u_.a = new Array(std::move(a)); }
Json::Json(Object o) : t_(Type::Object) { u_.o = new Object(std::move(o)); }

Json Json::object(std::initializer_list<Member> members) {
  Json j = object();
  for (const auto& m : members) j.set(m.first, m.second);
  return j;
}

void Json::set_u64(unsigned long long v) noexcept {
  if (v <= static_cast<unsigned long long>(INT64_MAX)) {
    t_ = Type::Int;
    u_.i = static_cast<int64_t>(v);
  } else {
    t_ = Type::Double;
    u_.d = static_cast<double>(v);
  }
}

void Json::destroy() noexcept {
  switch (t_) {
    case Type::String: delete u_.s; break;
    case Type::Array: delete u_.a; break;
    case Type::Object: delete u_.o; break;
    default: break;
  }
  t_ = Type::Null;
  u_.i = 0;
}

void Json::copy_from(const Json& o) {
  t_ = o.t_;
  switch (o.t_) {
    case Type::String: u_.s = new std::string(*o.u_.s); break;
    case Type::Array: u_.a = new Array(*o.u_.a); break;
    case Type::Object: u_.o = new Object(*o.u_.o); break;
    default: u_ = o.u_; break;
  }
}

void Json::move_from(Json& o) noexcept {
  t_ = o.t_;
  u_ = o.u_;
  o.t_ = Type::Null;
  o.u_.i = 0;
}

Json::Json(const Json& o) { copy_from(o); }
Json::Json(Json&& o) noexcept { move_from(o); }
Json& Json::operator=(const Json& o) {
  if (this != &o) {
    Json tmp(o);
    destroy();
    move_from(tmp);
  }
  return *this;
}
Json& Json::operator=(Json&& o) noexcept {
  if (this != &o) {
    // `o` may be owned by *this (e.g. j = std::move(j["child"])): detach it first.
    Json tmp(std::move(o));
    destroy();
    move_from(tmp);
  }
  return *this;
}

const char* Json::type_name(Type t) {
  switch (t) {
    case Type::Null: return "null";
    case Type::Bool: return "bool";
    case Type::Int: return "int";
    case Type::Double: return "double";
    case Type::String: return "string";
    case Type::Array: return "array";
    case Type::Object: return "object";
  }
  return "?";
}

bool Json::get_int_if(int64_t* out) const {
  if (t_ == Type::Int) { *out = u_.i; return true; }
  if (t_ == Type::Double) {
    double d = u_.d;
    if (std::isfinite(d) && d == std::floor(d) && d >= -9223372036854775808.0 &&
        d < 9223372036854775808.0) {
      *out = static_cast<int64_t>(d);
      return true;
    }
  }
  return false;
}

int64_t Json::as_int(int64_t dflt) const {
  int64_t v;
  return get_int_if(&v) ? v : dflt;
}

double Json::as_double(double dflt) const {
  if (t_ == Type::Double) return u_.d;
  if (t_ == Type::Int) return static_cast<double>(u_.i);
  return dflt;
}

const std::string& Json::as_string() const {
  return t_ == Type::String ? *u_.s : shared_empty_string();
}
std::string Json::as_string(std::string_view dflt) const {
  return t_ == Type::String ? *u_.s : std::string(dflt);
}

size_t Json::size() const {
  if (t_ == Type::Array) return u_.a->size();
  if (t_ == Type::Object) return u_.o->size();
  return 0;
}

const Json& Json::operator[](size_t i) const {
  if (t_ != Type::Array || i >= u_.a->size()) return shared_null();
  return (*u_.a)[i];
}

Json::Array& Json::items_mut() {
  if (t_ != Type::Array) { destroy(); t_ = Type::Array; u_.a = new Array(); }
  return *u_.a;
}
Json& Json::push(Json v) {
  items_mut().push_back(std::move(v));
  return *this;
}
const Json::Array& Json::items() const {
  return t_ == Type::Array ? *u_.a : shared_empty_array();
}

const Json* Json::find(std::string_view key) const {
  if (t_ != Type::Object) return nullptr;
  for (const auto& m : *u_.o)
    if (m.first == key) return &m.second;
  return nullptr;
}
Json* Json::find(std::string_view key) {
  if (t_ != Type::Object) return nullptr;
  for (auto& m : *u_.o)
    if (m.first == key) return &m.second;
  return nullptr;
}
const Json& Json::get(std::string_view key) const {
  const Json* j = find(key);
  return j ? *j : shared_null();
}
Json::Object& Json::members_mut() {
  if (t_ != Type::Object) { destroy(); t_ = Type::Object; u_.o = new Object(); }
  return *u_.o;
}
const Json::Object& Json::members() const {
  return t_ == Type::Object ? *u_.o : shared_empty_object();
}
Json& Json::set(std::string_view key, Json v) {
  Object& o = members_mut();
  for (auto& m : o) {
    if (m.first == key) { m.second = std::move(v); return *this; }
  }
  o.emplace_back(std::string(key), std::move(v));
  return *this;
}
Json& Json::at_key(std::string_view key) {
  Object& o = members_mut();
  for (auto& m : o)
    if (m.first == key) return m.second;
  o.emplace_back(std::string(key), Json());
  return o.back().second;
}
bool Json::erase(std::string_view key) {
  if (t_ != Type::Object) return false;
  Object& o = *u_.o;
  for (size_t i = 0; i < o.size(); ++i) {
    if (o[i].first == key) { o.erase(o.begin() + static_cast<std::ptrdiff_t>(i)); return true; }
  }
  return false;
}

std::string Json::get_string(std::string_view key, std::string_view dflt) const {
  const Json* j = find(key);
  return (j && j->is_string()) ? *j->u_.s : std::string(dflt);
}
int64_t Json::get_int(std::string_view key, int64_t dflt) const {
  const Json* j = find(key);
  return j ? j->as_int(dflt) : dflt;
}
double Json::get_double(std::string_view key, double dflt) const {
  const Json* j = find(key);
  return j ? j->as_double(dflt) : dflt;
}
bool Json::get_bool(std::string_view key, bool dflt) const {
  const Json* j = find(key);
  return j ? j->as_bool(dflt) : dflt;
}
bool Json::get_string_if(std::string_view key, std::string* out) const {
  const Json* j = find(key);
  if (!j || !j->is_string()) return false;
  *out = *j->u_.s;
  return true;
}
bool Json::get_int_if(std::string_view key, int64_t* out) const {
  const Json* j = find(key);
  return j && j->get_int_if(out);
}
bool Json::get_double_if(std::string_view key, double* out) const {
  const Json* j = find(key);
  if (!j || !j->is_number()) return false;
  *out = j->as_double();
  return true;
}
bool Json::get_bool_if(std::string_view key, bool* out) const {
  const Json* j = find(key);
  if (!j || !j->is_bool()) return false;
  *out = j->u_.b;
  return true;
}

bool operator==(const Json& a, const Json& b) {
  if (a.is_number() && b.is_number()) {
    if (a.is_int() && b.is_int()) return a.u_.i == b.u_.i;
    return a.as_double() == b.as_double();
  }
  if (a.t_ != b.t_) return false;
  switch (a.t_) {
    case Json::Type::Null: return true;
    case Json::Type::Bool: return a.u_.b == b.u_.b;
    case Json::Type::String: return *a.u_.s == *b.u_.s;
    case Json::Type::Array: return *a.u_.a == *b.u_.a;
    case Json::Type::Object: {
      const auto& x = *a.u_.o;
      const auto& y = *b.u_.o;
      if (x.size() != y.size()) return false;
      for (size_t i = 0; i < x.size(); ++i)
        if (x[i].first != y[i].first || !(x[i].second == y[i].second)) return false;
      return true;
    }
    default: return false;
  }
}

void Json::quote(std::string_view s, std::string& out) {
  static const char* hex = "0123456789abcdef";
  out.push_back('"');
  size_t i = 0;
  while (i < s.size()) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) {
      switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
          if (c < 0x20 || c == 0x7F) {
            out += "\\u00";
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 15]);
          } else {
            out.push_back(static_cast<char>(c));
          }
      }
      ++i;
      continue;
    }
    uint32_t cp;
    size_t n = utf8_decode(s, i, &cp);
    if (!n) { out += "\xEF\xBF\xBD"; ++i; continue; }
    if (cp == 0x2028) out += "\\u2028";
    else if (cp == 0x2029) out += "\\u2029";
    else out.append(s.data() + i, n);
    i += n;
  }
  out.push_back('"');
}

void Json::dump_impl(std::string& out, int indent, int level) const {
  switch (t_) {
    case Type::Null: out += "null"; break;
    case Type::Bool: out += u_.b ? "true" : "false"; break;
    case Type::Int: out += std::to_string(static_cast<long long>(u_.i)); break;
    case Type::Double: format_double(u_.d, out); break;
    case Type::String: quote(*u_.s, out); break;
    case Type::Array: {
      if (u_.a->empty()) { out += "[]"; break; }
      out.push_back('[');
      bool first = true;
      for (const Json& v : *u_.a) {
        if (!first) out.push_back(',');
        first = false;
        newline(out, indent, level + 1);
        v.dump_impl(out, indent, level + 1);
      }
      newline(out, indent, level);
      out.push_back(']');
      break;
    }
    case Type::Object: {
      if (u_.o->empty()) { out += "{}"; break; }
      out.push_back('{');
      bool first = true;
      for (const auto& m : *u_.o) {
        if (!first) out.push_back(',');
        first = false;
        newline(out, indent, level + 1);
        quote(m.first, out);
        out.push_back(':');
        if (indent >= 0) out.push_back(' ');
        m.second.dump_impl(out, indent, level + 1);
      }
      newline(out, indent, level);
      out.push_back('}');
      break;
    }
  }
}

void Json::dump_to(std::string& out, int indent) const { dump_impl(out, indent, 0); }
std::string Json::dump(int indent) const {
  std::string out;
  dump_impl(out, indent, 0);
  return out;
}

bool Json::parse(std::string_view text, Json& out, std::string* error, const JsonLimits& limits) {
  Parser p(text, limits);
  Json tmp;
  if (!p.run(tmp, error)) return false;
  out = std::move(tmp);
  return true;
}

std::optional<Json> Json::parse(std::string_view text, std::string* error,
                                const JsonLimits& limits) {
  Json j;
  if (!parse(text, j, error, limits)) return std::nullopt;
  return j;
}

}  // namespace ember
