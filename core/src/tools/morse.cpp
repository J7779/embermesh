#include "tools/morse.h"

#include <cmath>
#include <cstring>

namespace ember {
namespace morse {

namespace {

struct Sym {
  char c;
  const char* code;
};
// ITU-R M.1677-1 plus common punctuation.
const Sym kTable[] = {
    {'A', ".-"},     {'B', "-..."},   {'C', "-.-."},   {'D', "-.."},    {'E', "."},      {'F', "..-."},
    {'G', "--."},    {'H', "...."},   {'I', ".."},     {'J', ".---"},   {'K', "-.-"},    {'L', ".-.."},
    {'M', "--"},     {'N', "-."},     {'O', "---"},    {'P', ".--."},   {'Q', "--.-"},   {'R', ".-."},
    {'S', "..."},    {'T', "-"},      {'U', "..-"},    {'V', "...-"},   {'W', ".--"},    {'X', "-..-"},
    {'Y', "-.--"},   {'Z', "--.."},   {'0', "-----"},  {'1', ".----"},  {'2', "..---"},  {'3', "...--"},
    {'4', "....-"},  {'5', "....."},  {'6', "-...."},  {'7', "--..."},  {'8', "---.."},  {'9', "----."},
    {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, {'\'', ".----."}, {'!', "-.-.--"}, {'/', "-..-."},
    {'(', "-.--."},  {')', "-.--.-"}, {'&', ".-..."},  {':', "---..."}, {';', "-.-.-."}, {'=', "-...-"},
    {'+', ".-.-."},  {'-', "-....-"}, {'_', "..--.-"}, {'"', ".-..-."}, {'$', "...-..-"}, {'@', ".--.-."},
};

const char* code_of(char c) {
  if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
  for (const auto& s : kTable)
    if (s.c == c) return s.code;
  return nullptr;
}

// Splits text into "units": each unit is a vector of codes sent without inter-letter gaps
// (a single char, or a <PROSIGN>), or a word break (empty vector).
struct Unit {
  std::string code;  // dots/dashes
  bool word_break = false;
};

std::vector<Unit> units(const std::string& text, std::string* skipped) {
  std::vector<Unit> u;
  bool pending_space = false;
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
      pending_space = !u.empty();
      continue;
    }
    std::string code;
    if (c == '<') {
      size_t j = text.find('>', i);
      if (j != std::string::npos && j - i <= 10 && j - i >= 2) {
        bool ok = true;
        for (size_t k = i + 1; k < j; ++k) {
          const char* cc = code_of(text[k]);
          if (!cc) { ok = false; break; }
          code += cc;
        }
        if (ok) {
          i = j;
        } else {
          code.clear();
        }
      }
      if (code.empty()) {
        if (skipped) *skipped += c;
        continue;
      }
    } else {
      const char* cc = code_of(c);
      if (!cc) {
        if (skipped) *skipped += c;
        continue;
      }
      code = cc;
    }
    if (pending_space) {
      u.push_back({"", true});
      pending_space = false;
    }
    u.push_back({code, false});
  }
  return u;
}

}  // namespace

std::string encode(const std::string& text, std::string* skipped) {
  std::string out;
  bool first = true;
  for (const auto& x : units(text, skipped)) {
    if (x.word_break) {
      out += " /";
      continue;
    }
    if (!first) out += ' ';
    out += x.code;
    first = false;
  }
  return out;
}

std::string decode(const std::string& code) {
  std::string out, cur;
  int spaces = 0;
  auto flush = [&]() {
    if (cur.empty()) return;
    char found = '?';
    for (const auto& s : kTable)
      if (cur == s.code) { found = s.c; break; }
    out += found;
    cur.clear();
  };
  for (char c : code) {
    if (c == '.' || c == '-' || c == '_') {
      if (spaces >= 3 && !out.empty()) out += ' ';
      spaces = 0;
      cur += c == '_' ? '-' : c;
    } else if (c == '/' || c == '|') {
      flush();
      if (!out.empty() && out.back() != ' ') out += ' ';
      spaces = 0;
    } else if (c == ' ' || c == '\t' || c == '\n') {
      flush();
      ++spaces;
    } else {
      flush();
      out += '?';
    }
  }
  flush();
  while (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}

std::vector<int> timing(const std::string& text, double wpm, double fw) {
  std::vector<int> p;
  if (!(wpm > 0)) return p;
  double dot = 1200.0 / wpm;
  double gap_char = 3 * dot, gap_word = 7 * dot;
  if (fw > 0 && fw < wpm) {
    // ARRL Farnsworth: total delay ta = (60·wpm − 37.2·fw) / (fw·wpm) seconds spread over 19 units.
    double ta = (60.0 * wpm - 37.2 * fw) / (fw * wpm) * 1000.0;
    double tf = ta / 19.0;
    gap_char = 3 * tf;
    gap_word = 7 * tf;
  }
  auto add = [&](bool on, double ms) {
    int v = static_cast<int>(std::lround(ms));
    if (p.empty() && !on) return;
    if (p.size() % 2 == (on ? 0u : 1u)) p.push_back(v);
    else p.back() += v;
  };
  bool need_gap = false;
  bool word = false;
  for (const auto& u : units(text, nullptr)) {
    if (u.word_break) { word = true; continue; }
    if (need_gap) add(false, word ? gap_word : gap_char);
    word = false;
    for (size_t i = 0; i < u.code.size(); ++i) {
      if (i) add(false, dot);
      add(true, u.code[i] == '.' ? dot : 3 * dot);
    }
    need_gap = true;
  }
  return p;
}

long total_ms(const std::vector<int>& p) {
  long t = 0;
  for (int v : p) t += v;
  return t;
}

std::string nato(const std::string& text) {
  static const char* words[26] = {"Alfa", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot", "Golf", "Hotel", "India",
                                  "Juliett", "Kilo", "Lima", "Mike", "November", "Oscar", "Papa", "Quebec", "Romeo",
                                  "Sierra", "Tango", "Uniform", "Victor", "Whiskey", "X-ray", "Yankee", "Zulu"};
  static const char* digits[10] = {"Zero", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Niner"};
  std::string out;
  for (char c : text) {
    const char* w = nullptr;
    std::string other;
    if (c >= 'a' && c <= 'z') w = words[c - 'a'];
    else if (c >= 'A' && c <= 'Z') w = words[c - 'A'];
    else if (c >= '0' && c <= '9') w = digits[c - '0'];
    else if (c == ' ') { if (!out.empty() && out.back() != '|') out += " |"; continue; }
    else if (c == '.') w = "Decimal";
    else if (c == '-') w = "Dash";
    else if (c == '/') w = "Slash";
    else continue;
    if (!out.empty()) out += ' ';
    out += w;
  }
  while (!out.empty() && (out.back() == '|' || out.back() == ' ')) out.pop_back();
  return out;
}

}  // namespace morse
}  // namespace ember
