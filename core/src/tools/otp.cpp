#include "tools/otp.h"

namespace ember {
namespace otp {

bool pad_letters(ByteView random, size_t letters, std::string* out, size_t* used) {
  out->clear();
  size_t i = 0;
  for (; i < random.size() && out->size() < letters; ++i) {
    uint8_t b = random[i];
    if (b >= 234) continue;  // 234 = 9 * 26: keep the distribution exactly uniform
    out->push_back(static_cast<char>('A' + b % 26));
  }
  if (used) *used = i;
  return out->size() == letters;
}

std::string normalize_plaintext(const std::string& text) {
  static const char* digits[10] = {"ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE"};
  std::string o;
  for (char c : text) {
    if (c >= 'a' && c <= 'z') o += static_cast<char>(c - 32);
    else if (c >= 'A' && c <= 'Z') o += c;
    else if (c >= '0' && c <= '9') o += digits[c - '0'];
  }
  return o;
}

std::string letters_only(const std::string& text) {
  std::string o;
  for (char c : text) {
    if (c >= 'a' && c <= 'z') o += static_cast<char>(c - 32);
    else if (c >= 'A' && c <= 'Z') o += c;
  }
  return o;
}

static bool combine(const std::string& a, const std::string& k, int sign, std::string* out) {
  if (k.size() < a.size()) return false;
  out->clear();
  out->reserve(a.size());
  for (size_t i = 0; i < a.size(); ++i) {
    int x = a[i] - 'A', y = k[i] - 'A';
    if (x < 0 || x > 25 || y < 0 || y > 25) return false;
    out->push_back(static_cast<char>('A' + ((x + sign * y) % 26 + 26) % 26));
  }
  return true;
}

bool encrypt(const std::string& p, const std::string& k, std::string* out) { return combine(p, k, +1, out); }
bool decrypt(const std::string& c, const std::string& k, std::string* out) { return combine(c, k, -1, out); }

std::string groups(const std::string& s, size_t g, size_t per_line) {
  if (g == 0) g = 5;
  std::string o;
  size_t n = 0;
  for (size_t i = 0; i < s.size(); i += g) {
    if (i) o += (per_line && n % per_line == 0) ? '\n' : ' ';
    o += s.substr(i, g);
    ++n;
  }
  return o;
}

}  // namespace otp
}  // namespace ember
