#include "modem/morse.h"

#include <cstring>

namespace ember {
namespace modem {
namespace morse {

namespace {
struct Ch {
  char c;
  const char* p;
};
const Ch kChars[] = {
    {'A', ".-"},     {'B', "-..."},   {'C', "-.-."},   {'D', "-.."},    {'E', "."},       {'F', "..-."},
    {'G', "--."},    {'H', "...."},   {'I', ".."},     {'J', ".---"},   {'K', "-.-"},     {'L', ".-.."},
    {'M', "--"},     {'N', "-."},     {'O', "---"},    {'P', ".--."},   {'Q', "--.-"},    {'R', ".-."},
    {'S', "..."},    {'T', "-"},      {'U', "..-"},    {'V', "...-"},   {'W', ".--"},     {'X', "-..-"},
    {'Y', "-.--"},   {'Z', "--.."},   {'0', "-----"},  {'1', ".----"},  {'2', "..---"},   {'3', "...--"},
    {'4', "....-"},  {'5', "....."},  {'6', "-...."},  {'7', "--..."},  {'8', "---.."},   {'9', "----."},
    {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, {'\'', ".----."}, {'!', "-.-.--"}, {'/', "-..-."},
    {'(', "-.--."},  {')', "-.--.-"}, {'&', ".-..."},  {':', "---..."}, {';', "-.-.-."},  {'=', "-...-"},
    {'+', ".-.-."},  {'-', "-....-"}, {'_', "..--.-"}, {'"', ".-..-."}, {'$', "...-..-"}, {'@', ".--.-."},
};
struct Pro {
  const char* name;
  const char* p;
};
// Order matters for decoding: the first entry with a pattern wins (aliases follow).
const Pro kPros[] = {
    {"SK", "...-.-"}, {"VE", "...-."},  {"KA", "-.-.-"},  {"HH", "........"}, {"SOS", "...---..."},
    {"BK", "-...-.-"}, {"CL", "-.-..-.."}, {"DO", "-..---"}, {"AR", ".-.-."},   {"BT", "-...-"},
    {"KN", "-.--."},  {"AS", ".-..."},  {"SN", "...-."},  {"CT", "-.-.-"},
};
}  // namespace

const char* pattern_for(char c) {
  if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  for (const Ch& e : kChars) if (e.c == c) return e.p;
  return nullptr;
}

const char* prosign_pattern(std::string_view name) {
  for (const Pro& p : kPros) {
    if (name.size() == std::strlen(p.name)) {
      bool eq = true;
      for (size_t i = 0; i < name.size(); ++i) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        if (c != p.name[i]) { eq = false; break; }
      }
      if (eq) return p.p;
    }
  }
  return nullptr;
}

std::string decode(std::string_view pat) {
  if (pat.empty() || pat.size() > 12) return "";
  for (const Ch& e : kChars) if (pat == e.p) return std::string(1, e.c);
  for (const Pro& p : kPros) if (pat == p.p) return std::string("<") + p.name + ">";
  return "";
}

bool encode(std::string_view text, std::string* out) {
  out->clear();
  bool any = false, gap = false;
  auto add = [&](const char* p) {
    if (gap && any) *out += " |";
    gap = false;
    *out += p;
    *out += '|';
    any = true;
  };
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') { gap = true; continue; }
    if (c == '<') {
      size_t e = text.find('>', i);
      if (e != std::string_view::npos && e - i <= 4) {
        const char* p = prosign_pattern(text.substr(i + 1, e - i - 1));
        if (p) { add(p); i = e; continue; }
      }
    }
    const char* p = pattern_for(c);
    if (p) add(p);
  }
  return any;
}

}  // namespace morse
}  // namespace modem
}  // namespace ember
