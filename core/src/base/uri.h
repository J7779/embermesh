// EmberMesh core — `ember:` URIs (contact cards, channel invites, pairing blobs) and
// percent-encoding.
//
//   ember:<kind>?<key>=<value>&<key>=<value>...
//
// kind is [a-z0-9_-]{1,32}; keys are [a-z0-9_]{1,32}; values are percent-encoded UTF-8. The parser
// is strict, bounded (4096 bytes, 32 params) and accepts the scheme in any case.
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ember {

// Percent-encodes everything except unreserved characters (A-Z a-z 0-9 - . _ ~).
std::string percent_encode(std::string_view s);
// Strict: '%' must be followed by two hex digits; '+' is NOT treated as space. Returns false on
// malformed input.
bool percent_decode(std::string_view s, std::string& out);

struct EmberUri {
  static constexpr size_t kMaxLength = 4096;
  static constexpr size_t kMaxParams = 32;

  std::string kind;
  std::vector<std::pair<std::string, std::string>> params;

  EmberUri& add(std::string_view key, std::string_view value) {
    params.emplace_back(std::string(key), std::string(value));
    return *this;
  }
  // First value for key, or nullptr.
  const std::string* find(std::string_view key) const;
  std::string get(std::string_view key, std::string_view dflt = "") const {
    const std::string* v = find(key);
    return v ? *v : std::string(dflt);
  }

  std::string to_string() const;
  static bool parse(std::string_view text, EmberUri& out, std::string* error = nullptr);
};

}  // namespace ember
