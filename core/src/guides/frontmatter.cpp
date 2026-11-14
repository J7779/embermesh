#include "guides/frontmatter.h"

#include "guides/text.h"

namespace ember::guides {

const FmValue* FrontMatter::find(std::string_view key) const {
  for (const FmValue& v : values)
    if (v.key == key) return &v;
  return nullptr;
}

std::string FrontMatter::get(std::string_view key) const {
  const FmValue* v = find(key);
  if (!v) return std::string();
  if (!v->is_list) return v->scalar;
  std::string out;
  for (size_t i = 0; i < v->list.size(); ++i) {
    if (i) out += ", ";
    out += v->list[i];
  }
  return out;
}

namespace {

struct Line {
  std::string_view text;  // without line terminator
  size_t offset;          // offset of the line start
  size_t next;            // offset just past the terminator
};

bool is_blank(std::string_view s) {
  for (char c : s)
    if (c != ' ' && c != '\t') return false;
  return true;
}

std::string_view trim(std::string_view s) {
  size_t b = 0, e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
  return s.substr(b, e - b);
}

size_t indent_of(std::string_view s) {
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
  return i;
}

bool is_fence(std::string_view s, bool closing) {
  std::string_view t = trim(s);
  return t == "---" || (closing && t == "...");
}

// Strips a trailing " # comment" from a plain scalar.
std::string_view strip_comment(std::string_view s) {
  for (size_t i = 0; i < s.size(); ++i)
    if (s[i] == '#' && (i == 0 || s[i - 1] == ' ' || s[i - 1] == '\t')) return trim(s.substr(0, i));
  return trim(s);
}

int hexval(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Parses a quoted scalar starting at s[i] (the quote). On success sets out and i (past the closing
// quote) and returns true.
bool parse_quoted(std::string_view s, size_t& i, std::string& out, std::string& err) {
  char q = s[i++];
  out.clear();
  while (i < s.size()) {
    char c = s[i];
    if (q == '\'' && c == '\'') {
      if (i + 1 < s.size() && s[i + 1] == '\'') { out.push_back('\''); i += 2; continue; }
      ++i;
      return true;
    }
    if (q == '"' && c == '"') { ++i; return true; }
    if (q == '"' && c == '\\') {
      if (i + 1 >= s.size()) break;
      char e = s[i + 1];
      i += 2;
      switch (e) {
        case 'n': out.push_back('\n'); break;
        case 't': out.push_back('\t'); break;
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case '0': out.push_back(' '); break;
        case 'u': {
          uint32_t cp = 0;
          bool ok = i + 4 <= s.size();
          for (size_t k = 0; ok && k < 4; ++k) {
            int h = hexval(s[i + k]);
            if (h < 0) ok = false; else cp = cp * 16 + static_cast<uint32_t>(h);
          }
          if (!ok || (cp >= 0xD800 && cp <= 0xDFFF) || cp == 0) { err = "invalid \\u escape"; return false; }
          utf8_append(out, cp);
          i += 4;
          break;
        }
        default: err = std::string("unknown escape \\") + e; return false;
      }
      continue;
    }
    out.push_back(c);
    ++i;
  }
  err = "unterminated quoted string";
  return false;
}

// Parses a single scalar token (quoted or plain); used for flow-list items and block-list items.
bool parse_scalar(std::string_view raw, std::string& out, std::string& err) {
  std::string_view t = trim(raw);
  if (!t.empty() && (t[0] == '"' || t[0] == '\'')) {
    size_t i = 0;
    if (!parse_quoted(t, i, out, err)) return false;
    std::string_view rest = strip_comment(t.substr(i));
    if (!rest.empty()) { err = "unexpected text after quoted string"; return false; }
    return true;
  }
  out = std::string(strip_comment(t));
  return true;
}

bool parse_flow_list(std::string_view t, std::vector<std::string>& out, std::string& err) {
  // t starts with '['
  size_t i = 1;
  out.clear();
  for (;;) {
    while (i < t.size() && (t[i] == ' ' || t[i] == '\t')) ++i;
    if (i >= t.size()) { err = "unterminated list (missing ']')"; return false; }
    if (t[i] == ']') { ++i; break; }
    std::string item;
    if (t[i] == '"' || t[i] == '\'') {
      if (!parse_quoted(t, i, item, err)) return false;
    } else {
      size_t b = i;
      while (i < t.size() && t[i] != ',' && t[i] != ']') ++i;
      item = std::string(trim(t.substr(b, i - b)));
    }
    while (i < t.size() && (t[i] == ' ' || t[i] == '\t')) ++i;
    if (i >= t.size()) { err = "unterminated list (missing ']')"; return false; }
    if (!item.empty()) out.push_back(item);
    if (t[i] == ',') { ++i; continue; }
    if (t[i] == ']') { ++i; break; }
    err = "expected ',' or ']' in list";
    return false;
  }
  std::string_view rest = strip_comment(t.substr(i));
  if (!rest.empty()) { err = "unexpected text after list"; return false; }
  return true;
}

}  // namespace

FrontMatter parse_front_matter(std::string_view text) {
  FrontMatter fm;
  size_t start = 0;
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF)
    start = 3;

  std::vector<Line> lines;
  for (size_t pos = start; pos < text.size();) {
    size_t nl = text.find('\n', pos);
    size_t end = nl == std::string_view::npos ? text.size() : nl;
    size_t next = nl == std::string_view::npos ? text.size() : nl + 1;
    std::string_view l = text.substr(pos, end - pos);
    if (!l.empty() && l.back() == '\r') l.remove_suffix(1);
    lines.push_back({l, pos, next});
    pos = next;
  }

  fm.body_offset = start;
  fm.body_line = 1;
  if (lines.empty() || !is_fence(lines[0].text, false)) return fm;
  fm.present = true;

  auto err = [&](size_t li, std::string msg) { fm.errors.push_back({static_cast<int>(li + 1), std::move(msg)}); };

  size_t li = 1;
  size_t close = lines.size();
  for (size_t k = 1; k < lines.size(); ++k)
    if (is_fence(lines[k].text, true) && indent_of(lines[k].text) == 0) { close = k; break; }
  if (close == lines.size()) {
    err(0, "unterminated front matter (no closing '---')");
    fm.body_offset = text.size();
    fm.body_line = static_cast<int>(lines.size()) + 1;
    // Still parse what is there so that more errors can be reported.
  } else {
    fm.terminated = true;
    fm.body_offset = lines[close].next;
    fm.body_line = static_cast<int>(close) + 2;
  }

  while (li < close) {
    std::string_view l = lines[li].text;
    std::string_view t = trim(l);
    if (t.empty() || t[0] == '#') { ++li; continue; }
    if (indent_of(l) > 0) { err(li, "unexpected indented line"); ++li; continue; }
    size_t colon = std::string_view::npos;
    for (size_t i = 0; i < l.size(); ++i) {
      char c = l[i];
      if (c == ':') { colon = i; break; }
      bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
      if (!ok) break;
    }
    if (colon == std::string_view::npos || colon == 0 ||
        (colon + 1 < l.size() && l[colon + 1] != ' ' && l[colon + 1] != '\t')) {
      err(li, "expected 'key: value'");
      ++li;
      continue;
    }
    FmValue v;
    v.key = std::string(l.substr(0, colon));
    v.line = static_cast<int>(li + 1);
    std::string_view rest = trim(l.substr(colon + 1));
    size_t key_line = li;
    ++li;
    std::string e;
    bool ok = true;
    if (!rest.empty() && rest[0] == '[') {
      v.is_list = true;
      ok = parse_flow_list(rest, v.list, e);
    } else if (!rest.empty() && (rest[0] == '"' || rest[0] == '\'')) {
      ok = parse_scalar(rest, v.scalar, e);
    } else if (!rest.empty() && (rest[0] == '>' || rest[0] == '|') && strip_comment(rest.substr(1)).find_first_not_of("+-0123456789") == std::string_view::npos) {
      bool literal = rest[0] == '|';
      std::string acc;
      while (li < close && (is_blank(lines[li].text) || indent_of(lines[li].text) > 0)) {
        std::string_view piece = trim(lines[li].text);
        if (literal) { if (!acc.empty()) acc.push_back('\n'); acc.append(piece.data(), piece.size()); }
        else if (piece.empty()) { acc.push_back('\n'); }
        else { if (!acc.empty() && acc.back() != '\n') acc.push_back(' '); acc.append(piece.data(), piece.size()); }
        ++li;
      }
      while (!acc.empty() && (acc.back() == '\n' || acc.back() == ' ')) acc.pop_back();
      v.scalar = acc;
    } else if (rest.empty() || rest[0] == '#') {
      // Block list or empty value.
      bool any = false;
      while (li < close) {
        std::string_view bl = lines[li].text;
        std::string_view bt = trim(bl);
        if (bt.empty()) { ++li; continue; }
        if (bt[0] == '-' && (bt.size() == 1 || bt[1] == ' ' || bt[1] == '\t')) {
          std::string item;
          if (!parse_scalar(bt.substr(1), item, e)) { err(li, e); e.clear(); }
          else if (!item.empty()) v.list.push_back(item);
          any = true;
          ++li;
          continue;
        }
        break;
      }
      v.is_list = any;
    } else {
      std::string acc(strip_comment(rest));
      // Plain scalar continued on indented lines.
      while (li < close && !is_blank(lines[li].text) && indent_of(lines[li].text) > 0) {
        std::string_view piece = strip_comment(lines[li].text);
        if (!piece.empty()) { acc.push_back(' '); acc.append(piece.data(), piece.size()); }
        ++li;
      }
      v.scalar = acc;
    }
    if (!ok) { err(key_line, v.key + ": " + e); continue; }
    if (fm.find(v.key)) { err(key_line, "duplicate key '" + v.key + "'"); continue; }
    fm.values.push_back(std::move(v));
  }
  return fm;
}

}  // namespace ember::guides
