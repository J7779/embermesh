#include "host/util.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <direct.h>
#include <io.h>
#include <shlobj.h>
#include <sys/stat.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <stdlib.h>
#endif
#if (defined(__linux__) && !defined(__ANDROID__)) || (defined(__ANDROID__) && __ANDROID_API__ >= 28)
#include <sys/random.h>  // getrandom: Android from API 28; older Android reads /dev/urandom
#endif
#endif

namespace ember_host {

// ---- base64 --------------------------------------------------------------------------------------

static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const void* data, size_t len) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  std::string out;
  out.reserve((len + 2) / 3 * 4);
  size_t i = 0;
  for (; i + 3 <= len; i += 3) {
    uint32_t v = (uint32_t(p[i]) << 16) | (uint32_t(p[i + 1]) << 8) | p[i + 2];
    out += kB64[v >> 18];
    out += kB64[(v >> 12) & 63];
    out += kB64[(v >> 6) & 63];
    out += kB64[v & 63];
  }
  if (len - i == 1) {
    uint32_t v = uint32_t(p[i]) << 16;
    out += kB64[v >> 18];
    out += kB64[(v >> 12) & 63];
    out += "==";
  } else if (len - i == 2) {
    uint32_t v = (uint32_t(p[i]) << 16) | (uint32_t(p[i + 1]) << 8);
    out += kB64[v >> 18];
    out += kB64[(v >> 12) & 63];
    out += kB64[(v >> 6) & 63];
    out += '=';
  }
  return out;
}

static int b64v(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

bool base64_decode(std::string_view in, std::string* out, bool allow_unpadded) {
  std::string r;
  size_t n = in.size();
  if (n % 4 != 0) {
    if (!allow_unpadded || n % 4 == 1) return false;
  }
  size_t pad = 0;
  if (n >= 1 && in[n - 1] == '=') pad = 1;
  if (n >= 2 && in[n - 2] == '=') pad = 2;
  if (pad && n % 4 != 0) return false;
  size_t data_len = n - pad;
  r.reserve(data_len * 3 / 4);
  uint32_t acc = 0;
  int bits = 0;
  for (size_t i = 0; i < data_len; ++i) {
    int v = b64v(in[i]);
    if (v < 0) return false;
    acc = (acc << 6) | uint32_t(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      r += char((acc >> bits) & 0xFF);
    }
  }
  // Leftover bits must be zero (canonical encoding).
  if (bits > 0 && (acc & ((1u << bits) - 1)) != 0) return false;
  *out = std::move(r);
  return true;
}

std::string hex_encode(const void* data, size_t len) {
  static const char* h = "0123456789abcdef";
  const uint8_t* p = static_cast<const uint8_t*>(data);
  std::string s;
  s.reserve(len * 2);
  for (size_t i = 0; i < len; ++i) {
    s += h[p[i] >> 4];
    s += h[p[i] & 15];
  }
  return s;
}

// ---- SHA-1 (FIPS 180-4) --------------------------------------------------------------------------

static inline uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

Sha1::Sha1() {
  h_[0] = 0x67452301;
  h_[1] = 0xEFCDAB89;
  h_[2] = 0x98BADCFE;
  h_[3] = 0x10325476;
  h_[4] = 0xC3D2E1F0;
}

void Sha1::block(const uint8_t* p) {
  uint32_t w[80];
  for (int i = 0; i < 16; ++i)
    w[i] = (uint32_t(p[4 * i]) << 24) | (uint32_t(p[4 * i + 1]) << 16) | (uint32_t(p[4 * i + 2]) << 8) | p[4 * i + 3];
  for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4];
  for (int i = 0; i < 80; ++i) {
    uint32_t f, k;
    if (i < 20) {
      f = (b & c) | (~b & d);
      k = 0x5A827999;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDC;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6;
    }
    uint32_t t = rol(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = rol(b, 30);
    b = a;
    a = t;
  }
  h_[0] += a;
  h_[1] += b;
  h_[2] += c;
  h_[3] += d;
  h_[4] += e;
}

void Sha1::update(const void* data, size_t len) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  total_ += len;
  while (len > 0) {
    size_t take = 64 - buf_len_;
    if (take > len) take = len;
    std::memcpy(buf_ + buf_len_, p, take);
    buf_len_ += take;
    p += take;
    len -= take;
    if (buf_len_ == 64) {
      block(buf_);
      buf_len_ = 0;
    }
  }
}

void Sha1::finish(uint8_t out[20]) {
  uint64_t bits = total_ * 8;
  uint8_t pad = 0x80;
  update(&pad, 1);
  uint8_t zero = 0;
  while (buf_len_ != 56) update(&zero, 1);
  uint8_t lenb[8];
  for (int i = 0; i < 8; ++i) lenb[i] = uint8_t(bits >> (56 - 8 * i));
  update(lenb, 8);
  for (int i = 0; i < 5; ++i) {
    out[4 * i] = uint8_t(h_[i] >> 24);
    out[4 * i + 1] = uint8_t(h_[i] >> 16);
    out[4 * i + 2] = uint8_t(h_[i] >> 8);
    out[4 * i + 3] = uint8_t(h_[i]);
  }
}

std::string sha1_raw(std::string_view data) {
  Sha1 s;
  s.update(data.data(), data.size());
  uint8_t d[20];
  s.finish(d);
  return std::string(reinterpret_cast<char*>(d), 20);
}

std::string sha1_hex(std::string_view data) {
  std::string r = sha1_raw(data);
  return hex_encode(r.data(), r.size());
}

// ---- UTF-8 ---------------------------------------------------------------------------------------

bool utf8_valid(const void* data, size_t len) {
  const uint8_t* s = static_cast<const uint8_t*>(data);
  size_t i = 0;
  while (i < len) {
    uint8_t c = s[i];
    if (c < 0x80) {
      ++i;
      continue;
    }
    size_t n;
    uint32_t cp;
    if (c >= 0xC2 && c <= 0xDF) {
      n = 1;
      cp = c & 0x1F;
    } else if (c >= 0xE0 && c <= 0xEF) {
      n = 2;
      cp = c & 0x0F;
    } else if (c >= 0xF0 && c <= 0xF4) {
      n = 3;
      cp = c & 0x07;
    } else {
      return false;
    }
    if (len - i - 1 < n) return false;
    for (size_t k = 1; k <= n; ++k) {
      uint8_t cc = s[i + k];
      if ((cc & 0xC0) != 0x80) return false;
      cp = (cp << 6) | (cc & 0x3F);
    }
    if (n == 2 && cp < 0x800) return false;
    if (n == 3 && (cp < 0x10000 || cp > 0x10FFFF)) return false;
    if (cp >= 0xD800 && cp <= 0xDFFF) return false;
    i += n + 1;
  }
  return true;
}

// ---- randomness / time ---------------------------------------------------------------------------

bool random_bytes(void* out, size_t n) {
  if (n == 0) return true;
#ifdef _WIN32
  return BCryptGenRandom(nullptr, static_cast<PUCHAR>(out), ULONG(n), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
  arc4random_buf(out, n);
  return true;
#else
  uint8_t* p = static_cast<uint8_t*>(out);
  size_t got = 0;
#if (defined(__linux__) && !defined(__ANDROID__)) || (defined(__ANDROID__) && __ANDROID_API__ >= 28)
  while (got < n) {
    ssize_t r = getrandom(p + got, n - got, 0);
    if (r < 0) {
      if (errno == EINTR) continue;
      break;
    }
    got += size_t(r);
  }
  if (got == n) return true;
#endif
  int fd = ::open("/dev/urandom", O_RDONLY | O_CLOEXEC);
  if (fd < 0) return false;
  while (got < n) {
    ssize_t r = ::read(fd, p + got, n - got);
    if (r <= 0) {
      if (r < 0 && errno == EINTR) continue;
      ::close(fd);
      return false;
    }
    got += size_t(r);
  }
  ::close(fd);
  return true;
#endif
}

std::string random_hex(size_t nbytes) {
  std::vector<uint8_t> b(nbytes);
  if (!random_bytes(b.data(), b.size())) {
    // Never silently produce a predictable token.
    std::abort();
  }
  return hex_encode(b.data(), b.size());
}

uint64_t unix_ms() {
  using namespace std::chrono;
  return uint64_t(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

uint64_t steady_ms() {
  using namespace std::chrono;
  return uint64_t(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// ---- strings -------------------------------------------------------------------------------------

std::string to_lower(std::string_view s) {
  std::string r(s);
  for (auto& c : r)
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  return r;
}

std::string trim(std::string_view s) {
  size_t b = 0, e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
  return std::string(s.substr(b, e - b));
}

bool iequals(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    char x = a[i], y = b[i];
    if (x >= 'A' && x <= 'Z') x = char(x - 'A' + 'a');
    if (y >= 'A' && y <= 'Z') y = char(y - 'A' + 'a');
    if (x != y) return false;
  }
  return true;
}

bool starts_with(std::string_view s, std::string_view p) { return s.size() >= p.size() && s.substr(0, p.size()) == p; }
bool ends_with(std::string_view s, std::string_view p) {
  return s.size() >= p.size() && s.substr(s.size() - p.size()) == p;
}

std::vector<std::string> split(std::string_view s, char sep) {
  std::vector<std::string> r;
  size_t start = 0;
  for (size_t i = 0; i <= s.size(); ++i) {
    if (i == s.size() || s[i] == sep) {
      r.emplace_back(s.substr(start, i - start));
      start = i + 1;
    }
  }
  return r;
}

bool header_has_token(std::string_view list, std::string_view token) {
  for (const auto& part : split(list, ','))
    if (iequals(trim(part), token)) return true;
  return false;
}

bool url_decode(std::string_view in, std::string* out, bool plus_is_space) {
  std::string r;
  r.reserve(in.size());
  for (size_t i = 0; i < in.size(); ++i) {
    char c = in[i];
    if (c == '%') {
      if (i + 2 >= in.size()) return false;
      auto hv = [](char h) -> int {
        if (h >= '0' && h <= '9') return h - '0';
        if (h >= 'a' && h <= 'f') return h - 'a' + 10;
        if (h >= 'A' && h <= 'F') return h - 'A' + 10;
        return -1;
      };
      int a = hv(in[i + 1]), b = hv(in[i + 2]);
      if (a < 0 || b < 0) return false;
      r += char(a * 16 + b);
      i += 2;
    } else if (c == '+' && plus_is_space) {
      r += ' ';
    } else {
      r += c;
    }
  }
  *out = std::move(r);
  return true;
}

std::string query_param(std::string_view query, std::string_view key, bool* present) {
  if (present) *present = false;
  for (const auto& part : split(query, '&')) {
    size_t eq = part.find('=');
    std::string k = part.substr(0, eq), kd;
    if (!url_decode(k, &kd, true)) continue;
    if (kd != key) continue;
    std::string v = eq == std::string::npos ? std::string() : part.substr(eq + 1), vd;
    if (!url_decode(v, &vd, true)) continue;
    if (present) *present = true;
    return vd;
  }
  return std::string();
}

bool constant_time_equals(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  unsigned char d = 0;
  for (size_t i = 0; i < a.size(); ++i) d |= static_cast<unsigned char>(a[i] ^ b[i]);
  return d == 0;
}

bool parse_u64(std::string_view s, uint64_t* out) {
  if (s.empty() || s.size() > 20) return false;
  uint64_t v = 0;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
    uint64_t nv = v * 10 + uint64_t(c - '0');
    if (nv / 10 != v) return false;
    v = nv;
  }
  *out = v;
  return true;
}

std::string format(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char buf[512];
  va_list ap2;
  va_copy(ap2, ap);
  int n = std::vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  std::string r;
  if (n < 0) {
    va_end(ap2);
    return r;
  }
  if (size_t(n) < sizeof buf) {
    r.assign(buf, size_t(n));
  } else {
    r.resize(size_t(n) + 1);
    std::vsnprintf(&r[0], r.size(), fmt, ap2);
    r.resize(size_t(n));
  }
  va_end(ap2);
  return r;
}

// ---- files ---------------------------------------------------------------------------------------

char path_sep() {
#ifdef _WIN32
  return '\\';
#else
  return '/';
#endif
}

std::string path_join(const std::string& a, const std::string& b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  char last = a.back();
  if (last == '/' || last == '\\') return a + b;
  return a + path_sep() + b;
}

#ifdef _WIN32
static std::wstring widen(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
  std::wstring w(size_t(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), &w[0], n);
  return w;
}
static std::string narrow(const std::wstring& w) {
  if (w.empty()) return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
  std::string s(size_t(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), &s[0], n, nullptr, nullptr);
  return s;
}
#endif

std::FILE* fopen_utf8(const std::string& path, const char* mode) {
#ifdef _WIN32
  return _wfopen(widen(path).c_str(), widen(mode).c_str());
#else
  return std::fopen(path.c_str(), mode);
#endif
}

bool fseek64(std::FILE* f, uint64_t off) {
#ifdef _WIN32
  return _fseeki64(f, static_cast<long long>(off), SEEK_SET) == 0;
#else
  return fseeko(f, off_t(off), SEEK_SET) == 0;
#endif
}

bool read_file(const std::string& path, std::string* out, size_t max_bytes) {
#ifdef _WIN32
  FILE* f = _wfopen(widen(path).c_str(), L"rb");
#else
  FILE* f = std::fopen(path.c_str(), "rb");
#endif
  if (!f) return false;
  std::string r;
  char buf[65536];
  while (true) {
    size_t n = std::fread(buf, 1, sizeof buf, f);
    if (n == 0) break;
    if (r.size() + n > max_bytes) {
      std::fclose(f);
      return false;
    }
    r.append(buf, n);
  }
  bool err = std::ferror(f) != 0;
  std::fclose(f);
  if (err) return false;
  *out = std::move(r);
  return true;
}

bool write_file_atomic(const std::string& path, std::string_view data, bool private_mode) {
  std::string tmp = path + ".tmp" + random_hex(4);
#ifdef _WIN32
  FILE* f = _wfopen(widen(tmp).c_str(), L"wb");
  if (!f) return false;
#else
  int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, private_mode ? 0600 : 0644);
  if (fd < 0) return false;
  FILE* f = fdopen(fd, "wb");
  if (!f) {
    ::close(fd);
    return false;
  }
#endif
  bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
  ok = (std::fflush(f) == 0) && ok;
#ifndef _WIN32
  if (ok) ::fsync(fileno(f));
#endif
  ok = (std::fclose(f) == 0) && ok;
  if (!ok) {
    remove_file(tmp);
    return false;
  }
#ifdef _WIN32
  if (!MoveFileExW(widen(tmp).c_str(), widen(path).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    remove_file(tmp);
    return false;
  }
#else
  if (std::rename(tmp.c_str(), path.c_str()) != 0) {
    remove_file(tmp);
    return false;
  }
#endif
  return true;
}

bool is_dir(const std::string& path) {
#ifdef _WIN32
  DWORD a = GetFileAttributesW(widen(path).c_str());
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
  struct stat st;
  return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

bool is_file(const std::string& path, uint64_t* size, int64_t* mtime_s) {
#ifdef _WIN32
  struct _stat64 st;
  if (_wstat64(widen(path).c_str(), &st) != 0) return false;
  if (!(st.st_mode & _S_IFREG)) return false;
#else
  struct stat st;
  if (::stat(path.c_str(), &st) != 0) return false;
  if (!S_ISREG(st.st_mode)) return false;
#endif
  if (size) *size = uint64_t(st.st_size);
  if (mtime_s) *mtime_s = int64_t(st.st_mtime);
  return true;
}

// Creates `path` and only the missing ancestors: it never stats/opens ancestors that already
// exist (on some macOS setups touching e.g. ~/Documents can block on a permission prompt).
static bool mkdir_one(const std::string& d, bool* missing_parent) {
  *missing_parent = false;
#ifdef _WIN32
  if (CreateDirectoryW(widen(d).c_str(), nullptr)) return true;
  DWORD e = GetLastError();
  if (e == ERROR_ALREADY_EXISTS) return is_dir(d);
  if (e == ERROR_PATH_NOT_FOUND) *missing_parent = true;
  return false;
#else
  if (::mkdir(d.c_str(), 0755) == 0) return true;
  if (errno == EEXIST) return is_dir(d);
  if (errno == ENOENT) *missing_parent = true;
  return false;
#endif
}

bool make_dirs(const std::string& path_in) {
  std::string path = path_in;
  while (path.size() > 1 && (path.back() == '/' || path.back() == '\\')) path.pop_back();
  if (path.empty()) return false;
  bool missing = false;
  if (mkdir_one(path, &missing)) return true;
  if (!missing) return false;
  size_t s = path.find_last_of("/\\");
  if (s == std::string::npos || s == 0) return false;
  std::string parent = path.substr(0, s);
#ifdef _WIN32
  if (parent.size() == 2 && parent[1] == ':') return false;
#endif
  if (!make_dirs(parent)) return false;
  return mkdir_one(path, &missing);
}

bool is_symlink(const std::string& path) {
#ifdef _WIN32
  DWORD a = GetFileAttributesW(widen(path).c_str());
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_REPARSE_POINT);
#else
  struct stat st;
  return ::lstat(path.c_str(), &st) == 0 && S_ISLNK(st.st_mode);
#endif
}

bool remove_file(const std::string& path) {
#ifdef _WIN32
  return DeleteFileW(widen(path).c_str()) != 0;
#else
  return ::unlink(path.c_str()) == 0;
#endif
}

std::vector<std::string> list_dir(const std::string& path) {
  std::vector<std::string> r;
#ifdef _WIN32
  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW(widen(path_join(path, "*")).c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return r;
  do {
    std::string n = narrow(fd.cFileName);
    if (n != "." && n != "..") r.push_back(n);
  } while (FindNextFileW(h, &fd));
  FindClose(h);
#else
  DIR* d = ::opendir(path.c_str());
  if (!d) return r;
  while (struct dirent* e = ::readdir(d)) {
    std::string n = e->d_name;
    if (n != "." && n != "..") r.push_back(n);
  }
  ::closedir(d);
#endif
  return r;
}

std::string home_dir() {
#ifdef _WIN32
  const char* up = std::getenv("USERPROFILE");
  return up ? up : "C:\\";
#else
  const char* h = std::getenv("HOME");
  if (h && *h) return h;
  struct passwd* pw = getpwuid(getuid());
  return pw && pw->pw_dir ? pw->pw_dir : "/";
#endif
}

std::string executable_dir() {
  std::string p;
#ifdef _WIN32
  wchar_t buf[MAX_PATH * 2];
  DWORD n = GetModuleFileNameW(nullptr, buf, DWORD(sizeof buf / sizeof buf[0]));
  if (n > 0) p = narrow(std::wstring(buf, n));
#elif defined(__APPLE__)
  char buf[PATH_MAX];
  uint32_t sz = sizeof buf;
  if (_NSGetExecutablePath(buf, &sz) == 0) p = buf;  // no realpath(): it walks parent directories
#elif defined(__linux__)
  char buf[PATH_MAX];
  ssize_t n = ::readlink("/proc/self/exe", buf, sizeof buf - 1);
  if (n > 0) p.assign(buf, size_t(n));
#endif
  size_t s = p.find_last_of("/\\");
  return s == std::string::npos ? std::string(".") : p.substr(0, s);
}

std::string real_path(const std::string& path) {
#ifdef _WIN32
  wchar_t buf[MAX_PATH * 4];
  DWORD n = GetFullPathNameW(widen(path).c_str(), DWORD(sizeof buf / sizeof buf[0]), buf, nullptr);
  if (n == 0 || n >= sizeof buf / sizeof buf[0]) return std::string();
  std::string r = narrow(std::wstring(buf, n));
  if (GetFileAttributesW(buf) == INVALID_FILE_ATTRIBUTES) return std::string();
  return r;
#else
  char buf[PATH_MAX];
  if (!::realpath(path.c_str(), buf)) return std::string();
  return buf;
#endif
}

// ---- logging -------------------------------------------------------------------------------------

static std::atomic<int> g_level{int(LogLevel::Info)};
static std::mutex g_log_mu;
static std::function<void(LogLevel, const char*)>* g_sink = nullptr;

void set_log_level(LogLevel l) { g_level = int(l); }
LogLevel log_level() { return LogLevel(g_level.load()); }

LogLevel parse_log_level(std::string_view s, LogLevel def) {
  std::string l = to_lower(s);
  if (l == "debug" || l == "trace") return LogLevel::Debug;
  if (l == "info") return LogLevel::Info;
  if (l == "warn" || l == "warning") return LogLevel::Warn;
  if (l == "error") return LogLevel::Error;
  if (l == "off" || l == "none" || l == "quiet") return LogLevel::Off;
  return def;
}

void set_log_sink(std::function<void(LogLevel, const char*)> sink) {
  std::lock_guard<std::mutex> lk(g_log_mu);
  delete g_sink;
  g_sink = sink ? new std::function<void(LogLevel, const char*)>(std::move(sink)) : nullptr;
}

void log_msg(LogLevel l, const char* fmt, ...) {
  if (int(l) < g_level.load()) return;
  char msg[1024];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  std::lock_guard<std::mutex> lk(g_log_mu);
  if (g_sink) {
    (*g_sink)(l, msg);
    return;
  }
  static const char* names[] = {"debug", "info", "warn", "error", "off"};
  std::time_t t = std::time(nullptr);
  std::tm tmv;
#ifdef _WIN32
  localtime_s(&tmv, &t);
#else
  localtime_r(&t, &tmv);
#endif
  char ts[32];
  std::strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tmv);
  std::fprintf(stderr, "%s [%s] %s\n", ts, names[int(l)], msg);
  std::fflush(stderr);
}

// ---- memory-mapped files -----------------------------------------------------------------------

bool MappedFile::open(const std::string& path, std::string* err) {
  close();
#ifdef _WIN32
  int wl = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
  std::wstring w(size_t(wl > 0 ? wl : 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &w[0], wl);
  HANDLE f = CreateFileW(w.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
  if (f == INVALID_HANDLE_VALUE) {
    if (err) *err = "cannot open " + path;
    return false;
  }
  LARGE_INTEGER sz;
  if (!GetFileSizeEx(f, &sz) || sz.QuadPart <= 0) {
    CloseHandle(f);
    if (err) *err = "empty or unreadable " + path;
    return false;
  }
  HANDLE m = CreateFileMappingW(f, nullptr, PAGE_READONLY, 0, 0, nullptr);
  CloseHandle(f);
  if (!m) {
    if (err) *err = "cannot map " + path;
    return false;
  }
  void* p = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
  if (!p) {
    CloseHandle(m);
    if (err) *err = "cannot map " + path;
    return false;
  }
  map_ = m;
  ptr_ = p;
  size_ = size_t(sz.QuadPart);
  return true;
#else
  int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    if (err) *err = "cannot open " + path + ": " + std::strerror(errno);
    return false;
  }
  struct stat st;
  if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0) {
    ::close(fd);
    if (err) *err = "not a regular non-empty file: " + path;
    return false;
  }
  void* p = ::mmap(nullptr, size_t(st.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
  ::close(fd);  // the mapping keeps the file alive
  if (p == MAP_FAILED) {
    if (err) *err = "mmap " + path + ": " + std::strerror(errno);
    return false;
  }
  ptr_ = p;
  size_ = size_t(st.st_size);
  return true;
#endif
}

void MappedFile::close() {
  if (!ptr_) return;
#ifdef _WIN32
  UnmapViewOfFile(ptr_);
  CloseHandle(static_cast<HANDLE>(map_));
  map_ = nullptr;
#else
  ::munmap(ptr_, size_);
#endif
  ptr_ = nullptr;
  size_ = 0;
}

}  // namespace ember_host
