#include "host/json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ember_host {

namespace {

const Json& null_json() {
  static const Json n;
  return n;
}

struct Parser {
  const char* p;
  const char* end;
  int max_depth;
  std::string err;

  bool fail(const char* m) {
    if (err.empty()) err = m;
    return false;
  }
  void ws() {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
  }
  static int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }
  bool hex4(uint32_t* v) {
    if (end - p < 4) return fail("truncated \\u escape");
    uint32_t r = 0;
    for (int i = 0; i < 4; ++i) {
      int h = hexv(p[i]);
      if (h < 0) return fail("bad \\u escape");
      r = (r << 4) | uint32_t(h);
    }
    p += 4;
    *v = r;
    return true;
  }
  static void put_utf8(std::string& s, uint32_t cp) {
    if (cp < 0x80) {
      s += char(cp);
    } else if (cp < 0x800) {
      s += char(0xC0 | (cp >> 6));
      s += char(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      s += char(0xE0 | (cp >> 12));
      s += char(0x80 | ((cp >> 6) & 0x3F));
      s += char(0x80 | (cp & 0x3F));
    } else {
      s += char(0xF0 | (cp >> 18));
      s += char(0x80 | ((cp >> 12) & 0x3F));
      s += char(0x80 | ((cp >> 6) & 0x3F));
      s += char(0x80 | (cp & 0x3F));
    }
  }
  bool str(std::string* out) {
    // at opening quote
    ++p;
    std::string s;
    while (true) {
      if (p >= end) return fail("unterminated string");
      char c = *p++;
      if (c == '"') break;
      if (static_cast<unsigned char>(c) < 0x20) return fail("control character in string");
      if (c != '\\') {
        s += c;
        continue;
      }
      if (p >= end) return fail("unterminated escape");
      char e = *p++;
      switch (e) {
        case '"': s += '"'; break;
        case '\\': s += '\\'; break;
        case '/': s += '/'; break;
        case 'b': s += '\b'; break;
        case 'f': s += '\f'; break;
        case 'n': s += '\n'; break;
        case 'r': s += '\r'; break;
        case 't': s += '\t'; break;
        case 'u': {
          uint32_t cp;
          if (!hex4(&cp)) return false;
          if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (end - p < 6 || p[0] != '\\' || p[1] != 'u') return fail("lone high surrogate");
            p += 2;
            uint32_t lo;
            if (!hex4(&lo)) return false;
            if (lo < 0xDC00 || lo > 0xDFFF) return fail("bad low surrogate");
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
          } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            return fail("lone low surrogate");
          }
          put_utf8(s, cp);
          break;
        }
        default:
          return fail("bad escape");
      }
    }
    *out = std::move(s);
    return true;
  }
  bool num(Json* out) {
    const char* start = p;
    if (p < end && *p == '-') ++p;
    if (p >= end) return fail("bad number");
    if (*p == '0') {
      ++p;
    } else if (*p >= '1' && *p <= '9') {
      while (p < end && *p >= '0' && *p <= '9') ++p;
    } else {
      return fail("bad number");
    }
    if (p < end && *p == '.') {
      ++p;
      if (p >= end || !(*p >= '0' && *p <= '9')) return fail("bad number");
      while (p < end && *p >= '0' && *p <= '9') ++p;
    }
    if (p < end && (*p == 'e' || *p == 'E')) {
      ++p;
      if (p < end && (*p == '+' || *p == '-')) ++p;
      if (p >= end || !(*p >= '0' && *p <= '9')) return fail("bad number");
      while (p < end && *p >= '0' && *p <= '9') ++p;
    }
    size_t n = size_t(p - start);
    if (n > 64) return fail("number too long");
    char buf[80];
    std::memcpy(buf, start, n);
    buf[n] = 0;
    double v = std::strtod(buf, nullptr);
    if (!std::isfinite(v)) return fail("number out of range");
    *out = Json(v);
    return true;
  }
  bool lit(const char* word, size_t n) {
    if (size_t(end - p) < n || std::memcmp(p, word, n) != 0) return fail("bad literal");
    p += n;
    return true;
  }
  bool value(Json* out, int depth) {
    if (depth > max_depth) return fail("nesting too deep");
    ws();
    if (p >= end) return fail("unexpected end");
    char c = *p;
    if (c == '{') {
      ++p;
      *out = Json::object();
      ws();
      if (p < end && *p == '}') {
        ++p;
        return true;
      }
      while (true) {
        ws();
        if (p >= end || *p != '"') return fail("expected key");
        std::string k;
        if (!str(&k)) return false;
        ws();
        if (p >= end || *p != ':') return fail("expected ':'");
        ++p;
        Json v;
        if (!value(&v, depth + 1)) return false;
        out->set(k, std::move(v));
        ws();
        if (p < end && *p == ',') {
          ++p;
          continue;
        }
        if (p < end && *p == '}') {
          ++p;
          return true;
        }
        return fail("expected ',' or '}'");
      }
    }
    if (c == '[') {
      ++p;
      *out = Json::array();
      ws();
      if (p < end && *p == ']') {
        ++p;
        return true;
      }
      while (true) {
        Json v;
        if (!value(&v, depth + 1)) return false;
        out->push(std::move(v));
        ws();
        if (p < end && *p == ',') {
          ++p;
          continue;
        }
        if (p < end && *p == ']') {
          ++p;
          return true;
        }
        return fail("expected ',' or ']'");
      }
    }
    if (c == '"') {
      std::string s;
      if (!str(&s)) return false;
      *out = Json(std::move(s));
      return true;
    }
    if (c == 't') {
      if (!lit("true", 4)) return false;
      *out = Json(true);
      return true;
    }
    if (c == 'f') {
      if (!lit("false", 5)) return false;
      *out = Json(false);
      return true;
    }
    if (c == 'n') {
      if (!lit("null", 4)) return false;
      *out = Json();
      return true;
    }
    return num(out);
  }
};

}  // namespace

bool Json::parse(std::string_view text, Json* out, std::string* error, int max_depth) {
  Parser ps{text.data(), text.data() + text.size(), max_depth, {}};
  Json v;
  bool ok = ps.value(&v, 0);
  if (ok) {
    ps.ws();
    if (ps.p != ps.end) ok = ps.fail("trailing characters");
  }
  if (!ok) {
    if (error) *error = ps.err;
    return false;
  }
  *out = std::move(v);
  return true;
}

Json Json::parse_or_null(std::string_view text) {
  Json j;
  if (!parse(text, &j)) return Json();
  return j;
}

int64_t Json::as_int(int64_t def) const {
  if (type_ != Type::Number) return def;
  if (!(n_ >= -9.2e18 && n_ <= 9.2e18)) return def;
  return int64_t(n_);
}

const std::string& Json::as_string() const {
  static const std::string empty;
  return type_ == Type::String ? s_ : empty;
}

const Json& Json::get(std::string_view key) const {
  if (type_ != Type::Object) return null_json();
  for (const auto& kv : obj_)
    if (kv.first == key) return kv.second;
  return null_json();
}

bool Json::has(std::string_view key) const {
  if (type_ != Type::Object) return false;
  for (const auto& kv : obj_)
    if (kv.first == key) return true;
  return false;
}

Json& Json::set(std::string_view key, Json value) {
  if (type_ != Type::Object) {
    *this = object();
  }
  for (auto& kv : obj_) {
    if (kv.first == key) {
      kv.second = std::move(value);
      return kv.second;
    }
  }
  obj_.emplace_back(std::string(key), std::move(value));
  return obj_.back().second;
}

bool Json::erase(std::string_view key) {
  if (type_ != Type::Object) return false;
  for (auto it = obj_.begin(); it != obj_.end(); ++it) {
    if (it->first == key) {
      obj_.erase(it);
      return true;
    }
  }
  return false;
}

Json& Json::push(Json value) {
  if (type_ != Type::Array) *this = array();
  arr_.push_back(std::move(value));
  return arr_.back();
}

const Json& Json::at(size_t i) const {
  if (type_ != Type::Array || i >= arr_.size()) return null_json();
  return arr_[i];
}

void json_escape_to(std::string& out, std::string_view s) {
  static const char* hex = "0123456789abcdef";
  out += '"';
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      default:
        if (c < 0x20) {
          out += "\\u00";
          out += hex[c >> 4];
          out += hex[c & 15];
        } else {
          out += char(c);
        }
    }
  }
  out += '"';
}

std::string json_quote(std::string_view s) {
  std::string o;
  json_escape_to(o, s);
  return o;
}

void Json::dump_to(std::string& out) const {
  switch (type_) {
    case Type::Null: out += "null"; break;
    case Type::Bool: out += b_ ? "true" : "false"; break;
    case Type::Number: {
      if (!std::isfinite(n_)) {
        out += "null";
        break;
      }
      char buf[40];
      if (n_ == std::floor(n_) && std::fabs(n_) < 9007199254740992.0) {
        std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(n_));
      } else {
        std::snprintf(buf, sizeof buf, "%.17g", n_);
      }
      out += buf;
      break;
    }
    case Type::String: json_escape_to(out, s_); break;
    case Type::Array: {
      out += '[';
      bool first = true;
      for (const auto& v : arr_) {
        if (!first) out += ',';
        first = false;
        v.dump_to(out);
      }
      out += ']';
      break;
    }
    case Type::Object: {
      out += '{';
      bool first = true;
      for (const auto& kv : obj_) {
        if (!first) out += ',';
        first = false;
        json_escape_to(out, kv.first);
        out += ':';
        kv.second.dump_to(out);
      }
      out += '}';
      break;
    }
  }
}

std::string Json::dump() const {
  std::string s;
  dump_to(s);
  return s;
}

bool Json::operator==(const Json& o) const {
  if (type_ != o.type_) return false;
  switch (type_) {
    case Type::Null: return true;
    case Type::Bool: return b_ == o.b_;
    case Type::Number: return n_ == o.n_;
    case Type::String: return s_ == o.s_;
    case Type::Array: return arr_ == o.arr_;
    case Type::Object: return obj_ == o.obj_;
  }
  return false;
}

}  // namespace ember_host
