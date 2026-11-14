// Host-layer utilities: codecs (base64, hex, SHA-1), UTF-8 validation, CSPRNG, clocks, strings,
// files and logging. Portable (POSIX + Win32).
#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ember_host {

// ---- codecs --------------------------------------------------------------------------------------
std::string base64_encode(const void* data, size_t len);
inline std::string base64_encode(std::string_view s) { return base64_encode(s.data(), s.size()); }
// Strict standard-alphabet decoder; padding required except when `allow_unpadded`. Whitespace is not
// accepted. Returns false on any malformed input.
bool base64_decode(std::string_view in, std::string* out, bool allow_unpadded = false);

std::string hex_encode(const void* data, size_t len);

struct Sha1 {
  Sha1();
  void update(const void* data, size_t len);
  void finish(uint8_t out[20]);

 private:
  void block(const uint8_t* p);
  uint32_t h_[5];
  uint64_t total_ = 0;
  uint8_t buf_[64];
  size_t buf_len_ = 0;
};
std::string sha1_raw(std::string_view data);  // 20 raw bytes
std::string sha1_hex(std::string_view data);

// Validates UTF-8 per RFC 3629 (no overlongs, no surrogates, <= U+10FFFF).
bool utf8_valid(const void* data, size_t len);
inline bool utf8_valid(std::string_view s) { return utf8_valid(s.data(), s.size()); }

// ---- randomness / time ---------------------------------------------------------------------------
// Fills from the OS CSPRNG (arc4random_buf / getrandom / /dev/urandom / BCryptGenRandom).
bool random_bytes(void* out, size_t n);
std::string random_hex(size_t nbytes);
uint64_t unix_ms();    // wall clock
uint64_t steady_ms();  // monotonic

// ---- strings -------------------------------------------------------------------------------------
std::string to_lower(std::string_view s);
std::string trim(std::string_view s);
bool iequals(std::string_view a, std::string_view b);
bool starts_with(std::string_view s, std::string_view prefix);
bool ends_with(std::string_view s, std::string_view suffix);
std::vector<std::string> split(std::string_view s, char sep);
// True if comma-separated header value `list` contains `token` (case-insensitive).
bool header_has_token(std::string_view list, std::string_view token);
// Percent-decodes; returns false on malformed escapes. `plus_is_space` for query strings.
bool url_decode(std::string_view in, std::string* out, bool plus_is_space = false);
// Value of `key` in a raw query string "a=1&b=2" (decoded), or "" if absent.
std::string query_param(std::string_view query, std::string_view key, bool* present = nullptr);
bool constant_time_equals(std::string_view a, std::string_view b);
bool parse_u64(std::string_view s, uint64_t* out);  // decimal digits only, no overflow
std::string format(const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

// ---- files ---------------------------------------------------------------------------------------
bool read_file(const std::string& path, std::string* out, size_t max_bytes = size_t(64) << 20);
// fopen with a UTF-8 path on every platform (wide API on Windows) and 64-bit seeking.
std::FILE* fopen_utf8(const std::string& path, const char* mode);
bool fseek64(std::FILE* f, uint64_t off);
// Writes via temp file + rename so a crash never leaves a half-written file.
bool write_file_atomic(const std::string& path, std::string_view data, bool private_mode = false);
bool make_dirs(const std::string& path);  // touches only missing levels
bool is_symlink(const std::string& path);  // lstat (POSIX) / reparse point (Windows)
bool is_dir(const std::string& path);
bool is_file(const std::string& path, uint64_t* size = nullptr, int64_t* mtime_s = nullptr);
bool remove_file(const std::string& path);
std::vector<std::string> list_dir(const std::string& path);  // names only, no "." / ".."
std::string path_join(const std::string& a, const std::string& b);
std::string home_dir();
std::string executable_dir();
// Resolves symlinks / relative parts. Returns "" if the path does not exist. Walks every parent
// directory, so the runtime never calls it (tests only).
std::string real_path(const std::string& path);
char path_sep();

// Read-only memory map of a whole file (POSIX mmap / Win32 file mapping). Used for the guide pack
// (tens of MB): pages are loaded on demand and shared with the page cache instead of copied to RAM.
// The file must not be modified in place while mapped (replace it by rename instead).
class MappedFile {
 public:
  MappedFile() = default;
  ~MappedFile() { close(); }
  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;
  bool open(const std::string& path, std::string* err = nullptr);
  void close();
  const uint8_t* data() const { return static_cast<const uint8_t*>(ptr_); }
  size_t size() const { return size_; }
  bool is_open() const { return ptr_ != nullptr; }

 private:
  void* ptr_ = nullptr;
  size_t size_ = 0;
#ifdef _WIN32
  void* map_ = nullptr;
#endif
};

// ---- logging -------------------------------------------------------------------------------------
enum class LogLevel : int { Debug = 0, Info = 1, Warn = 2, Error = 3, Off = 4 };
void set_log_level(LogLevel l);
LogLevel log_level();
LogLevel parse_log_level(std::string_view s, LogLevel def = LogLevel::Info);
// Optional sink (default: stderr with timestamp). Called from any thread; must be thread-safe.
void set_log_sink(std::function<void(LogLevel, const char* line)> sink);
void log_msg(LogLevel l, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

#define EH_LOGD(...) ::ember_host::log_msg(::ember_host::LogLevel::Debug, __VA_ARGS__)
#define EH_LOGI(...) ::ember_host::log_msg(::ember_host::LogLevel::Info, __VA_ARGS__)
#define EH_LOGW(...) ::ember_host::log_msg(::ember_host::LogLevel::Warn, __VA_ARGS__)
#define EH_LOGE(...) ::ember_host::log_msg(::ember_host::LogLevel::Error, __VA_ARGS__)

}  // namespace ember_host
