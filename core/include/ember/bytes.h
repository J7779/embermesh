// EmberMesh core — byte buffer primitives shared by every module. Header-only.
//
// Everything that parses data from the outside world (radio, serial, files) must go through
// ByteReader or do its own explicit bounds checks: the core is compiled without exceptions and
// must never read out of bounds on malformed input.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace ember {

using Bytes = std::vector<uint8_t>;

// Non-owning view of bytes (like std::span<const uint8_t>, which is C++20).
class ByteView {
 public:
  constexpr ByteView() = default;
  constexpr ByteView(const uint8_t* p, size_t n) : p_(p), n_(p ? n : 0) {}
  ByteView(const Bytes& b) : p_(b.data()), n_(b.size()) {}  // NOLINT(implicit)
  explicit ByteView(std::string_view s)
      : p_(reinterpret_cast<const uint8_t*>(s.data())), n_(s.size()) {}

  constexpr const uint8_t* data() const { return p_; }
  constexpr size_t size() const { return n_; }
  constexpr bool empty() const { return n_ == 0; }
  constexpr const uint8_t* begin() const { return p_; }
  constexpr const uint8_t* end() const { return p_ + n_; }
  // Unchecked; callers must have validated i < size().
  constexpr uint8_t operator[](size_t i) const { return p_[i]; }

  // Clamped sub-view: never reads out of range, returns a shorter/empty view instead.
  constexpr ByteView sub(size_t off, size_t len = static_cast<size_t>(-1)) const {
    if (off >= n_) return ByteView();
    size_t avail = n_ - off;
    return ByteView(p_ + off, len < avail ? len : avail);
  }

  Bytes to_bytes() const { return Bytes(p_, p_ + n_); }
  std::string to_string() const { return std::string(reinterpret_cast<const char*>(p_), n_); }

  friend bool operator==(ByteView a, ByteView b) {
    return a.n_ == b.n_ && (a.n_ == 0 || std::memcmp(a.p_, b.p_, a.n_) == 0);
  }
  friend bool operator!=(ByteView a, ByteView b) { return !(a == b); }

 private:
  const uint8_t* p_ = nullptr;
  size_t n_ = 0;
};

// Bounds-checked sequential reader with a sticky failure flag: after any failed read every
// subsequent read returns zero/empty and ok() is false. Check ok() once at the end of parsing.
class ByteReader {
 public:
  explicit ByteReader(ByteView v) : v_(v) {}

  bool ok() const { return ok_; }
  size_t pos() const { return pos_; }
  size_t remaining() const { return ok_ ? v_.size() - pos_ : 0; }
  bool at_end() const { return ok_ && pos_ == v_.size(); }
  void fail() { ok_ = false; }

  uint8_t u8() { return static_cast<uint8_t>(be(1)); }
  uint16_t u16be() { return static_cast<uint16_t>(be(2)); }
  uint32_t u32be() { return static_cast<uint32_t>(be(4)); }
  uint64_t u64be() { return be(8); }
  uint16_t u16le() { return static_cast<uint16_t>(le(2)); }
  uint32_t u32le() { return static_cast<uint32_t>(le(4)); }
  uint64_t u64le() { return le(8); }

  // Unsigned LEB128, at most 10 bytes; fails on truncation, overflow and overlong (non-minimal)
  // encodings such as 80 00 for 0, so every value has exactly one accepted encoding (security
  // review L9: these bytes are hashed, signed and deduplicated). ByteWriter::varint is minimal.
  uint64_t varint() {
    uint64_t out = 0;
    for (int shift = 0; shift < 70; shift += 7) {
      if (!need(1)) return 0;
      uint8_t b = v_[pos_++];
      if (shift == 63 && (b & 0x7E)) { ok_ = false; return 0; }
      if (shift > 0 && b == 0) { ok_ = false; return 0; }  // overlong: final byte adds nothing
      out |= static_cast<uint64_t>(b & 0x7F) << shift;
      if (!(b & 0x80)) return out;
    }
    ok_ = false;
    return 0;
  }

  // Returns a view of the next n bytes, or an empty view (and fails) if fewer remain.
  ByteView bytes(size_t n) {
    if (!need(n)) return ByteView();
    ByteView out = v_.sub(pos_, n);
    pos_ += n;
    return out;
  }
  bool copy(uint8_t* out, size_t n) {
    if (!need(n)) return false;
    if (n) std::memcpy(out, v_.data() + pos_, n);
    pos_ += n;
    return true;
  }
  ByteView rest() { return bytes(remaining()); }
  bool skip(size_t n) {
    if (!need(n)) return false;
    pos_ += n;
    return true;
  }

 private:
  bool need(size_t n) {
    if (!ok_ || n > v_.size() - pos_) { ok_ = false; return false; }
    return true;
  }
  uint64_t be(size_t n) {
    if (!need(n)) return 0;
    uint64_t out = 0;
    for (size_t i = 0; i < n; ++i) out = (out << 8) | v_[pos_ + i];
    pos_ += n;
    return out;
  }
  uint64_t le(size_t n) {
    if (!need(n)) return 0;
    uint64_t out = 0;
    for (size_t i = 0; i < n; ++i) out |= static_cast<uint64_t>(v_[pos_ + i]) << (8 * i);
    pos_ += n;
    return out;
  }

  ByteView v_;
  size_t pos_ = 0;
  bool ok_ = true;
};

class ByteWriter {
 public:
  ByteWriter() = default;
  explicit ByteWriter(size_t reserve) { buf_.reserve(reserve); }

  ByteWriter& u8(uint8_t v) { buf_.push_back(v); return *this; }
  ByteWriter& u16be(uint16_t v) { return be(v, 2); }
  ByteWriter& u32be(uint32_t v) { return be(v, 4); }
  ByteWriter& u64be(uint64_t v) { return be(v, 8); }
  ByteWriter& u16le(uint16_t v) { return le(v, 2); }
  ByteWriter& u32le(uint32_t v) { return le(v, 4); }
  ByteWriter& u64le(uint64_t v) { return le(v, 8); }
  ByteWriter& varint(uint64_t v) {
    while (v >= 0x80) { buf_.push_back(static_cast<uint8_t>(v) | 0x80); v >>= 7; }
    buf_.push_back(static_cast<uint8_t>(v));
    return *this;
  }
  ByteWriter& bytes(ByteView v) { buf_.insert(buf_.end(), v.begin(), v.end()); return *this; }
  ByteWriter& bytes(const uint8_t* p, size_t n) { return bytes(ByteView(p, n)); }
  ByteWriter& str(std::string_view s) { return bytes(ByteView(s)); }

  size_t size() const { return buf_.size(); }
  const Bytes& data() const { return buf_; }
  Bytes take() { return std::move(buf_); }

 private:
  ByteWriter& be(uint64_t v, int n) {
    for (int i = n - 1; i >= 0; --i) buf_.push_back(static_cast<uint8_t>(v >> (8 * i)));
    return *this;
  }
  ByteWriter& le(uint64_t v, int n) {
    for (int i = 0; i < n; ++i) buf_.push_back(static_cast<uint8_t>(v >> (8 * i)));
    return *this;
  }
  Bytes buf_;
};

inline std::string to_hex(ByteView v) {
  static const char* k = "0123456789abcdef";
  std::string out;
  out.reserve(v.size() * 2);
  for (uint8_t b : v) { out.push_back(k[b >> 4]); out.push_back(k[b & 15]); }
  return out;
}

// Strict hex decode (upper or lower case, even length, no separators). Returns false on any error.
inline bool from_hex(std::string_view s, Bytes& out) {
  auto nib = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  out.clear();
  if (s.size() % 2) return false;
  out.reserve(s.size() / 2);
  for (size_t i = 0; i < s.size(); i += 2) {
    int hi = nib(s[i]), lo = nib(s[i + 1]);
    if (hi < 0 || lo < 0) { out.clear(); return false; }
    out.push_back(static_cast<uint8_t>(hi << 4 | lo));
  }
  return true;
}

// Constant-time comparison for secrets / MACs.
inline bool ct_equal(ByteView a, ByteView b) {
  if (a.size() != b.size()) return false;
  uint8_t d = 0;
  for (size_t i = 0; i < a.size(); ++i) d |= static_cast<uint8_t>(a[i] ^ b[i]);
  return d == 0;
}

}  // namespace ember
