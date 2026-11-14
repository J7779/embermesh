// HTTP/1.1 request parsing, response formatting, MIME types, Range parsing and safe static-file path
// resolution. Pure functions (no sockets) so they are unit-testable; the server is in server.h.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ember_host {

struct HttpRequest {
  std::string method;   // "GET"
  std::string target;   // raw request-target "/a%20b?x=1"
  std::string path;     // percent-decoded path "/a b" (no query)
  std::string query;    // raw query "x=1"
  int version_minor = 1;  // HTTP/1.<n>
  std::vector<std::pair<std::string, std::string>> headers;  // names lower-cased
  std::string body;
  bool keep_alive = true;

  const std::string* header(std::string_view lower_name) const;
  std::string header_or(std::string_view lower_name, const std::string& def = std::string()) const;
};

struct HttpLimits {
  size_t max_header_bytes = 16 * 1024;  // request line + headers
  size_t max_headers = 64;
  size_t max_body = 1 << 20;
};

class HttpParser {
 public:
  enum class Result { NeedMore, Done, Error };
  explicit HttpParser(HttpLimits limits = HttpLimits()) : limits_(limits) {}

  // Parses one request from the front of `buf`. On Done, *consumed = bytes used (pipelining: the
  // rest belongs to the next request). On Error, error_status() is the HTTP status to answer with
  // (400, 413, 414, 431, 501, 505) and the connection must be closed afterwards.
  Result parse(std::string_view buf, size_t* consumed);
  const HttpRequest& request() const { return req_; }
  HttpRequest& request() { return req_; }
  int error_status() const { return status_; }
  const std::string& error_message() const { return msg_; }

 private:
  Result fail(int status, const char* msg) {
    status_ = status;
    msg_ = msg;
    return Result::Error;
  }
  HttpLimits limits_;
  HttpRequest req_;
  int status_ = 0;
  std::string msg_;
};

const char* http_reason(int status);
// "Status line + headers + CRLF". Adds Content-Length unless `content_length` < 0.
std::string http_response_head(int status, const std::vector<std::pair<std::string, std::string>>& headers,
                               int64_t content_length, bool keep_alive);
std::string http_simple_response(int status, const std::string& content_type, const std::string& body,
                                 bool keep_alive, const std::vector<std::pair<std::string, std::string>>& extra = {});

std::string mime_type_for(std::string_view path);  // by extension; "application/octet-stream" default

// Single "bytes=" range against `size`. Returns 1 on a satisfiable range (start/len set), 0 when the
// header is absent or should be ignored (serve 200), -1 when unsatisfiable (answer 416).
int parse_range(std::string_view header, uint64_t size, uint64_t* start, uint64_t* len);

// Maps a decoded URL path to a file under `root`, refusing traversal ("..", NUL, backslashes,
// drive letters, dot-files) and symlinks that escape the root. Directories resolve to index.html.
// Returns "" if not servable.
std::string resolve_static_path(const std::string& root, const std::string& url_path, bool* is_directory = nullptr);

std::string http_date(int64_t unix_s);
std::string html_escape(std::string_view s);

}  // namespace ember_host
