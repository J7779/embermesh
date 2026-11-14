#include "host/http.h"

#include <cstring>
#include <ctime>

#include "host/util.h"

namespace ember_host {

const std::string* HttpRequest::header(std::string_view n) const {
  for (const auto& h : headers)
    if (h.first == n) return &h.second;
  return nullptr;
}

std::string HttpRequest::header_or(std::string_view n, const std::string& def) const {
  const std::string* v = header(n);
  return v ? *v : def;
}

static bool is_tchar(char c) {
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return true;
  return std::strchr("!#$%&'*+-.^_`|~", c) != nullptr && c != 0;
}

HttpParser::Result HttpParser::parse(std::string_view buf, size_t* consumed) {
  req_ = HttpRequest();
  status_ = 0;
  msg_.clear();
  // Ignore leading empty lines (RFC 9112 §2.2).
  size_t start = 0;
  while (start < buf.size() && (buf[start] == '\r' || buf[start] == '\n') && start < 8) ++start;
  std::string_view b = buf.substr(start);

  // Locate end of header block (CRLFCRLF, tolerate bare LF).
  size_t hdr_end = std::string_view::npos, body_start = 0;
  for (size_t i = 0; i < b.size(); ++i) {
    if (b[i] != '\n') continue;
    if (i + 1 < b.size() && b[i + 1] == '\n') {
      hdr_end = i;
      body_start = i + 2;
      break;
    }
    if (i + 2 < b.size() && b[i + 1] == '\r' && b[i + 2] == '\n') {
      hdr_end = i;
      body_start = i + 3;
      break;
    }
    if (i > limits_.max_header_bytes) break;
  }
  if (hdr_end == std::string_view::npos) {
    size_t first_nl = b.find('\n');
    if (first_nl == std::string_view::npos && b.size() > limits_.max_header_bytes) return fail(414, "request line too long");
    if (b.size() > limits_.max_header_bytes) return fail(431, "request headers too large");
    return Result::NeedMore;
  }
  if (body_start > limits_.max_header_bytes + 4) return fail(431, "request headers too large");

  // Split lines.
  std::vector<std::string_view> lines;
  {
    std::string_view h = b.substr(0, hdr_end);
    size_t s = 0;
    for (size_t i = 0; i <= h.size(); ++i) {
      if (i == h.size() || h[i] == '\n') {
        std::string_view line = h.substr(s, i - s);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        lines.push_back(line);
        s = i + 1;
      }
    }
  }
  if (lines.empty() || lines[0].empty()) return fail(400, "empty request line");

  // Request line: method SP target SP version
  std::string_view rl = lines[0];
  size_t sp1 = rl.find(' ');
  if (sp1 == std::string_view::npos || sp1 == 0) return fail(400, "malformed request line");
  size_t sp2 = rl.find(' ', sp1 + 1);
  if (sp2 == std::string_view::npos || sp2 == sp1 + 1) return fail(400, "malformed request line");
  if (rl.find(' ', sp2 + 1) != std::string_view::npos) return fail(400, "malformed request line");
  std::string_view method = rl.substr(0, sp1), target = rl.substr(sp1 + 1, sp2 - sp1 - 1), ver = rl.substr(sp2 + 1);
  for (char c : method)
    if (!is_tchar(c)) return fail(400, "bad method");
  if (method.size() > 16) return fail(501, "method not implemented");
  if (target.size() > 8192) return fail(414, "URI too long");
  for (char c : target)
    if (static_cast<unsigned char>(c) <= 0x20 || c == 0x7f) return fail(400, "bad request target");
  if (ver.size() != 8 || ver.substr(0, 5) != "HTTP/" || ver[6] != '.' || ver[5] < '0' || ver[5] > '9' || ver[7] < '0' ||
      ver[7] > '9')
    return fail(400, "bad HTTP version");
  if (ver[5] != '1') return fail(505, "HTTP version not supported");
  req_.method = std::string(method);
  req_.target = std::string(target);
  req_.version_minor = ver[7] - '0';

  // Headers.
  for (size_t i = 1; i < lines.size(); ++i) {
    std::string_view l = lines[i];
    if (l.empty()) return fail(400, "empty header line");
    if (l[0] == ' ' || l[0] == '\t') return fail(400, "obsolete header folding");
    size_t colon = l.find(':');
    if (colon == std::string_view::npos || colon == 0) return fail(400, "malformed header");
    std::string_view name = l.substr(0, colon);
    for (char c : name)
      if (!is_tchar(c)) return fail(400, "bad header name");
    std::string value = trim(l.substr(colon + 1));
    for (char c : value)
      if ((static_cast<unsigned char>(c) < 0x20 && c != '\t') || c == 0x7f) return fail(400, "bad header value");
    if (req_.headers.size() >= limits_.max_headers) return fail(431, "too many headers");
    req_.headers.emplace_back(to_lower(name), std::move(value));
  }

  if (req_.version_minor >= 1 && !req_.header("host")) return fail(400, "missing Host header");

  // Target forms.
  std::string t = req_.target;
  if (t == "*") {
    if (req_.method != "OPTIONS") return fail(400, "bad request target");
    req_.path = "*";
  } else {
    if (starts_with(t, "http://") || starts_with(t, "https://")) {
      size_t p = t.find('/', t.find("//") + 2);
      t = p == std::string::npos ? std::string("/") : t.substr(p);
    }
    if (t.empty() || t[0] != '/') return fail(400, "bad request target");
    size_t hash = t.find('#');
    if (hash != std::string::npos) t = t.substr(0, hash);
    size_t q = t.find('?');
    std::string raw_path = t.substr(0, q);
    if (q != std::string::npos) req_.query = t.substr(q + 1);
    if (!url_decode(raw_path, &req_.path, false)) return fail(400, "bad percent-encoding");
    if (req_.path.find('\0') != std::string::npos) return fail(400, "NUL in path");
  }

  // Connection semantics.
  std::string conn = req_.header_or("connection");
  if (req_.version_minor >= 1)
    req_.keep_alive = !header_has_token(conn, "close");
  else
    req_.keep_alive = header_has_token(conn, "keep-alive");

  // Body framing: Content-Length only (chunked request bodies are not needed by any client of ours).
  const std::string* te = req_.header("transfer-encoding");
  uint64_t clen = 0;
  bool have_cl = false;
  for (const auto& h : req_.headers) {
    if (h.first != "content-length") continue;
    uint64_t v;
    if (!parse_u64(h.second, &v)) return fail(400, "bad Content-Length");
    if (have_cl && v != clen) return fail(400, "conflicting Content-Length");
    clen = v;
    have_cl = true;
  }
  if (te) {
    if (have_cl) return fail(400, "both Transfer-Encoding and Content-Length");
    return fail(501, "Transfer-Encoding not supported");
  }
  if (clen > limits_.max_body) return fail(413, "request body too large");
  size_t total = start + body_start + size_t(clen);
  if (buf.size() < total) return Result::NeedMore;
  req_.body = std::string(buf.substr(start + body_start, size_t(clen)));
  *consumed = total;
  return Result::Done;
}

const char* http_reason(int s) {
  switch (s) {
    case 101: return "Switching Protocols";
    case 200: return "OK";
    case 204: return "No Content";
    case 206: return "Partial Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 304: return "Not Modified";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 411: return "Length Required";
    case 413: return "Content Too Large";
    case 414: return "URI Too Long";
    case 416: return "Range Not Satisfiable";
    case 426: return "Upgrade Required";
    case 429: return "Too Many Requests";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    case 505: return "HTTP Version Not Supported";
    default: return "Unknown";
  }
}

std::string http_date(int64_t unix_s) {
  std::time_t t = std::time_t(unix_s);
  std::tm g;
#ifdef _WIN32
  gmtime_s(&g, &t);
#else
  gmtime_r(&t, &g);
#endif
  char buf[64];
  std::strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S GMT", &g);
  return buf;
}

std::string http_response_head(int status, const std::vector<std::pair<std::string, std::string>>& headers,
                               int64_t content_length, bool keep_alive) {
  std::string h = format("HTTP/1.1 %d %s\r\n", status, http_reason(status));
  h += "Server: emberd\r\n";
  h += "Date: " + http_date(int64_t(unix_ms() / 1000)) + "\r\n";
  for (const auto& kv : headers) {
    h += kv.first;
    h += ": ";
    h += kv.second;
    h += "\r\n";
  }
  if (content_length >= 0) h += "Content-Length: " + std::to_string(content_length) + "\r\n";
  if (status != 101) h += keep_alive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
  h += "\r\n";
  return h;
}

std::string http_simple_response(int status, const std::string& content_type, const std::string& body,
                                 bool keep_alive, const std::vector<std::pair<std::string, std::string>>& extra) {
  std::vector<std::pair<std::string, std::string>> h = extra;
  if (!content_type.empty()) h.emplace_back("Content-Type", content_type);
  h.emplace_back("Cache-Control", "no-store");
  return http_response_head(status, h, int64_t(body.size()), keep_alive) + body;
}

std::string mime_type_for(std::string_view path) {
  size_t dot = path.rfind('.');
  size_t slash = path.find_last_of("/\\");
  if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) return "application/octet-stream";
  std::string e = to_lower(path.substr(dot + 1));
  struct M {
    const char* ext;
    const char* type;
  };
  static const M table[] = {
      {"html", "text/html; charset=utf-8"},
      {"htm", "text/html; charset=utf-8"},
      {"js", "text/javascript; charset=utf-8"},
      {"mjs", "text/javascript; charset=utf-8"},
      {"css", "text/css; charset=utf-8"},
      {"json", "application/json"},
      {"map", "application/json"},
      {"webmanifest", "application/manifest+json"},
      {"wasm", "application/wasm"},
      {"svg", "image/svg+xml"},
      {"png", "image/png"},
      {"jpg", "image/jpeg"},
      {"jpeg", "image/jpeg"},
      {"gif", "image/gif"},
      {"webp", "image/webp"},
      {"ico", "image/x-icon"},
      {"txt", "text/plain; charset=utf-8"},
      {"md", "text/markdown; charset=utf-8"},
      {"xml", "application/xml"},
      {"pdf", "application/pdf"},
      {"woff", "font/woff"},
      {"woff2", "font/woff2"},
      {"ttf", "font/ttf"},
      {"otf", "font/otf"},
      {"mp3", "audio/mpeg"},
      {"ogg", "audio/ogg"},
      {"wav", "audio/wav"},
      {"mp4", "video/mp4"},
      {"webm", "video/webm"},
      {"zip", "application/zip"},
      {"gz", "application/gzip"},
      {"tgz", "application/gzip"},
      {"tar", "application/x-tar"},
      {"apk", "application/vnd.android.package-archive"},
      {"pack", "application/octet-stream"},
      {"bin", "application/octet-stream"},
  };
  for (const auto& m : table)
    if (e == m.ext) return m.type;
  return "application/octet-stream";
}

int parse_range(std::string_view header, uint64_t size, uint64_t* start, uint64_t* len) {
  std::string h = trim(header);
  if (h.empty()) return 0;
  if (!starts_with(h, "bytes=")) return 0;  // unknown unit: ignore (RFC 9110 §14.2)
  std::string spec = trim(std::string_view(h).substr(6));
  if (spec.find(',') != std::string::npos) return 0;  // multi-range: serve the full entity instead
  size_t dash = spec.find('-');
  if (dash == std::string::npos) return 0;
  std::string a = trim(std::string_view(spec).substr(0, dash)), b = trim(std::string_view(spec).substr(dash + 1));
  uint64_t first = 0, last = 0;
  if (a.empty()) {
    // suffix range: last N bytes
    uint64_t n;
    if (!parse_u64(b, &n)) return 0;
    if (n == 0 || size == 0) return -1;
    if (n > size) n = size;
    *start = size - n;
    *len = n;
    return 1;
  }
  if (!parse_u64(a, &first)) return 0;
  if (b.empty()) {
    last = size ? size - 1 : 0;
  } else {
    if (!parse_u64(b, &last)) return 0;
    if (last < first) return 0;  // syntactically invalid -> ignore
  }
  if (first >= size) return -1;
  if (last >= size) last = size - 1;
  *start = first;
  *len = last - first + 1;
  return 1;
}

std::string resolve_static_path(const std::string& root, const std::string& url_path, bool* is_directory) {
  if (is_directory) *is_directory = false;
  if (root.empty() || url_path.empty() || url_path[0] != '/') return std::string();
  if (url_path.find('\0') != std::string::npos || url_path.find('\\') != std::string::npos) return std::string();
  std::vector<std::string> segs;
  for (const auto& s : split(url_path.substr(1), '/')) {
    if (s.empty() || s == ".") continue;
    if (s == "..") return std::string();
    if (s[0] == '.' ) return std::string();         // dot-files / dot-dirs are never served
    if (s.find(':') != std::string::npos) return std::string();  // drive letters / ADS on Windows
    for (char c : s)
      if (static_cast<unsigned char>(c) < 0x20) return std::string();
    segs.push_back(s);
  }
  // Symlinks below the root are never followed (no realpath(): it would touch every parent
  // directory of the root). The root itself may be a symlink.
  std::string p = root;
  for (const auto& s : segs) {
    p = path_join(p, s);
    if (is_symlink(p)) return std::string();
  }
  if (is_dir(p)) {
    if (is_directory) *is_directory = true;
    p = path_join(p, "index.html");
    if (is_symlink(p)) return std::string();
  }
  uint64_t sz;
  if (!is_file(p, &sz)) return std::string();
  return p;
}

std::string html_escape(std::string_view s) {
  std::string r;
  for (char c : s) {
    switch (c) {
      case '&': r += "&amp;"; break;
      case '<': r += "&lt;"; break;
      case '>': r += "&gt;"; break;
      case '"': r += "&quot;"; break;
      case '\'': r += "&#39;"; break;
      default: r += c;
    }
  }
  return r;
}

}  // namespace ember_host
