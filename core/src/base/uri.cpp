#include "base/uri.h"

namespace ember {

namespace {
bool unreserved(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
         c == '.' || c == '_' || c == '~';
}
int hexval(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
bool valid_token(std::string_view s, bool allow_dash) {
  if (s.empty() || s.size() > 32) return false;
  for (char c : s) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || (allow_dash && c == '-');
    if (!ok) return false;
  }
  return true;
}
bool set_err(std::string* e, const char* m) {
  if (e) *e = m;
  return false;
}
}  // namespace

std::string percent_encode(std::string_view s) {
  static const char* hex = "0123456789ABCDEF";
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (unreserved(c)) {
      out.push_back(c);
    } else {
      unsigned char u = static_cast<unsigned char>(c);
      out.push_back('%');
      out.push_back(hex[u >> 4]);
      out.push_back(hex[u & 15]);
    }
  }
  return out;
}

bool percent_decode(std::string_view s, std::string& out) {
  out.clear();
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (c == '%') {
      if (s.size() - i < 3) { out.clear(); return false; }
      int h = hexval(s[i + 1]), l = hexval(s[i + 2]);
      if (h < 0 || l < 0) { out.clear(); return false; }
      out.push_back(static_cast<char>(h << 4 | l));
      i += 2;
    } else {
      out.push_back(c);
    }
  }
  return true;
}

const std::string* EmberUri::find(std::string_view key) const {
  for (const auto& p : params)
    if (p.first == key) return &p.second;
  return nullptr;
}

std::string EmberUri::to_string() const {
  std::string out = "ember:" + kind;
  char sep = '?';
  for (const auto& p : params) {
    out.push_back(sep);
    sep = '&';
    out += p.first;
    out.push_back('=');
    out += percent_encode(p.second);
  }
  return out;
}

bool EmberUri::parse(std::string_view text, EmberUri& out, std::string* error) {
  out = EmberUri();
  // Trim surrounding ASCII whitespace (pasted text).
  while (!text.empty() && (text.front() == ' ' || text.front() == '\n' || text.front() == '\r' || text.front() == '\t'))
    text.remove_prefix(1);
  while (!text.empty() && (text.back() == ' ' || text.back() == '\n' || text.back() == '\r' || text.back() == '\t'))
    text.remove_suffix(1);
  if (text.size() > kMaxLength) return set_err(error, "uri too long");
  if (text.size() < 6) return set_err(error, "not an ember: uri");
  for (size_t i = 0; i < 6; ++i) {
    char c = text[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (c != "ember:"[i]) return set_err(error, "not an ember: uri");
  }
  text.remove_prefix(6);
  // Tolerate "ember://kind".
  if (text.size() >= 2 && text[0] == '/' && text[1] == '/') text.remove_prefix(2);
  size_t q = text.find('?');
  std::string_view kind = text.substr(0, q);
  if (!valid_token(kind, true)) return set_err(error, "bad uri kind");
  out.kind = std::string(kind);
  if (q == std::string_view::npos) return true;
  std::string_view rest = text.substr(q + 1);
  while (!rest.empty()) {
    size_t amp = rest.find('&');
    std::string_view pair = rest.substr(0, amp);
    rest = amp == std::string_view::npos ? std::string_view() : rest.substr(amp + 1);
    if (pair.empty()) continue;
    size_t eq = pair.find('=');
    std::string_view k = pair.substr(0, eq);
    std::string_view v = eq == std::string_view::npos ? std::string_view() : pair.substr(eq + 1);
    if (!valid_token(k, false)) { out = EmberUri(); return set_err(error, "bad uri parameter name"); }
    std::string dv;
    if (!percent_decode(v, dv)) { out = EmberUri(); return set_err(error, "bad percent-encoding"); }
    if (out.params.size() >= kMaxParams) { out = EmberUri(); return set_err(error, "too many parameters"); }
    out.params.emplace_back(std::string(k), std::move(dv));
  }
  return true;
}

}  // namespace ember
