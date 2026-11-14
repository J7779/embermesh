#include "guides/text.h"

#include <algorithm>
#include <cstring>

namespace ember::guides {

// ---------------------------------------------------------------------------------------------
// UTF-8

bool utf8_valid(std::string_view s, size_t* bad_offset) {
  const auto* p = reinterpret_cast<const unsigned char*>(s.data());
  size_t n = s.size(), i = 0;
  while (i < n) {
    unsigned c = p[i];
    if (c < 0x80) { ++i; continue; }
    size_t len;
    uint32_t cp, min;
    if ((c & 0xE0) == 0xC0) { len = 2; cp = c & 0x1F; min = 0x80; }
    else if ((c & 0xF0) == 0xE0) { len = 3; cp = c & 0x0F; min = 0x800; }
    else if ((c & 0xF8) == 0xF0) { len = 4; cp = c & 0x07; min = 0x10000; }
    else { if (bad_offset) *bad_offset = i; return false; }
    if (len > n - i) { if (bad_offset) *bad_offset = i; return false; }
    for (size_t k = 1; k < len; ++k) {
      if ((p[i + k] & 0xC0) != 0x80) { if (bad_offset) *bad_offset = i; return false; }
      cp = (cp << 6) | (p[i + k] & 0x3F);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
      if (bad_offset) *bad_offset = i;
      return false;
    }
    i += len;
  }
  return true;
}

uint32_t utf8_next(std::string_view s, size_t& i) {
  const auto* p = reinterpret_cast<const unsigned char*>(s.data());
  size_t n = s.size();
  if (i >= n) return 0;
  unsigned c = p[i];
  if (c < 0x80) { ++i; return c; }
  size_t len;
  uint32_t cp, min;
  if ((c & 0xE0) == 0xC0) { len = 2; cp = c & 0x1F; min = 0x80; }
  else if ((c & 0xF0) == 0xE0) { len = 3; cp = c & 0x0F; min = 0x800; }
  else if ((c & 0xF8) == 0xF0) { len = 4; cp = c & 0x07; min = 0x10000; }
  else { ++i; return 0xFFFD; }
  if (len > n - i) { ++i; return 0xFFFD; }
  for (size_t k = 1; k < len; ++k) {
    if ((p[i + k] & 0xC0) != 0x80) { ++i; return 0xFFFD; }
    cp = (cp << 6) | (p[i + k] & 0x3F);
  }
  if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { ++i; return 0xFFFD; }
  i += len;
  return cp;
}

void utf8_append(std::string& out, uint32_t cp) {
  if (cp < 0x80) {
    out.push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

size_t utf8_floor(std::string_view s, size_t n) {
  if (n >= s.size()) return s.size();
  // Step back over continuation bytes so that s[n] starts a sequence.
  size_t k = n;
  int steps = 0;
  while (k > 0 && steps < 4 && (static_cast<unsigned char>(s[k]) & 0xC0) == 0x80) { --k; ++steps; }
  return k;
}

std::string ascii_lower(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

std::string utf8_lower(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  size_t i = 0;
  while (i < s.size()) {
    size_t start = i;
    uint32_t cp = utf8_next(s, i);
    if (cp == 0xFFFD && i - start != 3) continue;  // drop invalid bytes: output is always valid UTF-8
    uint32_t lc = cp;
    if (cp >= 'A' && cp <= 'Z') lc = cp + 32;
    else if (cp >= 0xC0 && cp <= 0xDE && cp != 0xD7) lc = cp + 0x20;
    else if ((cp >= 0x100 && cp <= 0x137) || (cp >= 0x14A && cp <= 0x177)) lc = cp | 1;
    else if ((cp >= 0x139 && cp <= 0x148) || (cp >= 0x179 && cp <= 0x17E)) lc = (cp & 1) ? cp + 1 : cp;
    else if (cp == 0x178) lc = 0xFF;
    else if (cp >= 0x391 && cp <= 0x3A9 && cp != 0x3A2) lc = cp + 0x20;
    else if (cp >= 0x410 && cp <= 0x42F) lc = cp + 0x20;
    else if (cp >= 0x400 && cp <= 0x40F) lc = cp + 0x50;
    utf8_append(out, lc);
  }
  return out;
}

// ---------------------------------------------------------------------------------------------
// Character classes and folding

namespace {

enum class Cls { kSep, kWord, kCjk, kApos, kHyphen, kDot, kComma, kIgnore };

// U+00C0..U+00FF and U+0100..U+017F folded to ASCII base letters ('-' = special/separator).
constexpr const char* kLatin1 =
    "aaaaaa-ceeeeiiii" "dnooooo-ouuuuy--"   // C0..DF (C6, D7, DE, DF special)
    "aaaaaa-ceeeeiiii" "dnooooo-ouuuuy-y";  // E0..FF (E6, F7, FE special)
constexpr const char* kLatinExtA =
    "aaaaaaccccccccdd" "ddeeeeeeeeeegggg" "gggghhhhiiiiiiii" "iiiijjkkklllllll"
    "lllnnnnnnnnnoooo" "oooorrrrrrssssss" "ssttttttuuuuuuuu" "uuuuwwyyyzzzzzzs";

bool is_cjk(uint32_t cp) {
  return (cp >= 0x3040 && cp <= 0x30FF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
         (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0xAC00 && cp <= 0xD7AF) ||
         (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0x20000 && cp <= 0x2FFFF);
}

// Classifies cp; for word characters appends the folded form to `fold`.
Cls classify(uint32_t cp, std::string* fold) {
  if (cp < 0x80) {
    if (cp >= 'a' && cp <= 'z') { if (fold) fold->push_back(static_cast<char>(cp)); return Cls::kWord; }
    if (cp >= 'A' && cp <= 'Z') { if (fold) fold->push_back(static_cast<char>(cp + 32)); return Cls::kWord; }
    if (cp >= '0' && cp <= '9') { if (fold) fold->push_back(static_cast<char>(cp)); return Cls::kWord; }
    if (cp == '\'') return Cls::kApos;
    if (cp == '-') return Cls::kHyphen;
    if (cp == '.') return Cls::kDot;
    if (cp == ',') return Cls::kComma;
    return Cls::kSep;
  }
  if (cp < 0xC0) return Cls::kSep;  // Latin-1 punctuation, NBSP, C1 controls
  if (cp <= 0xFF) {
    char c = kLatin1[cp - 0xC0];
    if (c != '-') { if (fold) fold->push_back(c); return Cls::kWord; }
    switch (cp) {
      case 0xC6: case 0xE6: if (fold) fold->append("ae"); return Cls::kWord;
      case 0xDE: case 0xFE: if (fold) fold->append("th"); return Cls::kWord;
      case 0xDF: if (fold) fold->append("ss"); return Cls::kWord;
      default: return Cls::kSep;  // × ÷
    }
  }
  if (cp <= 0x17F) {
    if (cp == 0x152 || cp == 0x153) { if (fold) fold->append("oe"); return Cls::kWord; }
    if (fold) fold->push_back(kLatinExtA[cp - 0x100]);
    return Cls::kWord;
  }
  if (cp >= 0x300 && cp <= 0x36F) return Cls::kIgnore;  // combining diacritics (decomposed accents)
  if (cp == 0x2018 || cp == 0x2019 || cp == 0x02BC) return Cls::kApos;
  if (cp == 0x2010 || cp == 0x2011) return Cls::kHyphen;
  if (cp == 0x200B || cp == 0x200C || cp == 0x200D || cp == 0x00AD) return Cls::kIgnore;
  if ((cp >= 0x2000 && cp <= 0x2BFF) || (cp >= 0x3000 && cp <= 0x303F) ||
      (cp >= 0xE000 && cp <= 0xF8FF) || (cp >= 0xFE30 && cp <= 0xFE4F) ||
      (cp >= 0xFFF0 && cp <= 0xFFFF) || cp == 0xFEFF || (cp >= 0x1F000 && cp <= 0x1FAFF))
    return Cls::kSep;
  if (cp >= 0xFF00 && cp <= 0xFFEF) {  // fullwidth forms
    if (cp >= 0xFF10 && cp <= 0xFF19) { if (fold) fold->push_back(static_cast<char>('0' + cp - 0xFF10)); return Cls::kWord; }
    if (cp >= 0xFF21 && cp <= 0xFF3A) { if (fold) fold->push_back(static_cast<char>('a' + cp - 0xFF21)); return Cls::kWord; }
    if (cp >= 0xFF41 && cp <= 0xFF5A) { if (fold) fold->push_back(static_cast<char>('a' + cp - 0xFF41)); return Cls::kWord; }
    if (cp >= 0xFF66) { if (fold) utf8_append(*fold, cp); return Cls::kCjk; }  // halfwidth kana etc.
    return Cls::kSep;
  }
  if (is_cjk(cp)) { if (fold) utf8_append(*fold, cp); return Cls::kCjk; }
  // Simple case folding for Greek and Cyrillic capitals.
  uint32_t lc = cp;
  if (cp >= 0x391 && cp <= 0x3A9 && cp != 0x3A2) lc = cp + 0x20;
  else if (cp >= 0x410 && cp <= 0x42F) lc = cp + 0x20;
  else if (cp >= 0x400 && cp <= 0x40F) lc = cp + 0x50;
  if (fold) utf8_append(*fold, lc);
  return Cls::kWord;
}

bool is_digit_char(char c) { return c >= '0' && c <= '9'; }

struct Part {
  std::string text;
  uint32_t begin, end;
  bool overflow;
};

bool keep_token(std::string_view t) {
  if (t.empty() || t.size() > kMaxTokenBytes) return false;
  if (t.size() == 1) return is_digit_char(t[0]);  // single ASCII letters carry no meaning
  return true;
}

// The word being built. Fixed buffer: no allocation per character.
struct Cur {
  static constexpr size_t kCap = kMaxTokenBytes + 16;
  char buf[kCap];
  size_t len = 0;
  uint32_t begin = 0, end = 0;
  bool overflow = false;
  bool empty() const { return len == 0 && !overflow; }
  std::string_view view() const { return std::string_view(buf, len); }
  void push(char c) { if (len < kCap) buf[len++] = c; else overflow = true; }
  void append(std::string_view s) {
    if (s.size() > kCap - len) { overflow = true; return; }
    std::memcpy(buf + len, s.data(), s.size());
    len += s.size();
  }
  void reset() { len = 0; overflow = false; }
};

}  // namespace

void tokenize(std::string_view text, std::vector<Token>& out, size_t max_tokens) {
  std::vector<Part> parts;  // only used for hyphenated compounds
  Cur cur;
  bool in_word = false;
  if (max_tokens == 0 || max_tokens > 64) out.reserve(out.size() + std::min<size_t>(text.size() / 6 + 1, max_tokens ? max_tokens : SIZE_MAX));

  auto full = [&]() { return max_tokens && out.size() >= max_tokens; };
  auto emit = [&](std::string_view t, uint32_t b, uint32_t e, bool joined) {
    if (full() || !keep_token(t)) return;
    out.emplace_back();
    Token& tok = out.back();
    tok.text.assign(t.data(), t.size());
    tok.begin = b;
    tok.end = e;
    tok.joined = joined;
  };
  auto end_part = [&]() {  // hyphen inside a word
    if (!cur.empty()) parts.push_back({std::string(cur.view()), cur.begin, cur.end, cur.overflow});
    cur.reset();
  };
  auto end_word = [&]() {
    in_word = false;
    if (parts.empty()) {
      if (!cur.overflow && cur.len) emit(cur.view(), cur.begin, cur.end, false);
      cur.reset();
      return;
    }
    end_part();
    for (const Part& p : parts)
      if (!p.overflow) emit(p.text, p.begin, p.end, false);
    if (parts.size() > 1 && parts.size() <= 8) {
      std::string joined;
      bool ok = true;
      for (const Part& p : parts) { joined += p.text; ok = ok && !p.overflow; }
      if (ok && joined.size() <= kMaxTokenBytes) emit(joined, parts.front().begin, parts.back().end, true);
    }
    parts.clear();
  };
  auto start_char = [&](uint32_t b) {
    if (!in_word) in_word = true;
    if (cur.empty()) cur.begin = b;
  };

  std::string piece;
  size_t i = 0;
  while (i < text.size() && !full()) {
    // Fast path: ASCII letters and digits (the vast majority of the corpus).
    char ch = text[i];
    bool lower = ch >= 'a' && ch <= 'z', upper = ch >= 'A' && ch <= 'Z', digit = ch >= '0' && ch <= '9';
    if (lower || upper || digit) {
      uint32_t b = static_cast<uint32_t>(i);
      start_char(b);
      cur.push(upper ? static_cast<char>(ch + 32) : ch);
      cur.end = b + 1;
      ++i;
      continue;
    }
    if (static_cast<unsigned char>(ch) < 0x80 && ch != '\'' && ch != '-' && ch != '.' && ch != ',') {
      if (in_word) end_word();  // ASCII separator
      ++i;
      continue;
    }
    size_t start = i;
    uint32_t cp = utf8_next(text, i);
    piece.clear();
    Cls c = classify(cp, &piece);
    switch (c) {
      case Cls::kWord:
        start_char(static_cast<uint32_t>(start));
        cur.append(piece);
        cur.end = static_cast<uint32_t>(i);
        break;
      case Cls::kCjk:
        end_word();
        emit(piece, static_cast<uint32_t>(start), static_cast<uint32_t>(i), false);
        break;
      case Cls::kIgnore:
        break;
      case Cls::kApos: case Cls::kHyphen: case Cls::kDot: case Cls::kComma: {
        if (!in_word) break;
        size_t j = i;
        uint32_t ncp = utf8_next(text, j);
        std::string npiece;
        Cls nc = (i < text.size()) ? classify(ncp, &npiece) : Cls::kSep;
        if (nc != Cls::kWord) { end_word(); break; }
        bool prev_digit = cur.len > 0 && is_digit_char(cur.buf[cur.len - 1]);
        bool next_digit = npiece.size() == 1 && is_digit_char(npiece[0]);
        if (c == Cls::kApos) {
          if (npiece == "s") {  // possessive 's: drop when it ends the word
            size_t k = j;
            std::string after;
            Cls ac = (j < text.size()) ? classify(utf8_next(text, k), &after) : Cls::kSep;
            if (ac != Cls::kWord && ac != Cls::kCjk) { i = j; end_word(); break; }
          }
          // otherwise drop the apostrophe and continue the word ("won't" -> "wont")
        } else if (c == Cls::kHyphen) {
          end_part();
        } else if (c == Cls::kDot) {
          if (prev_digit && next_digit) cur.push('.'); else end_word();
        } else {  // comma: digit grouping "1,000"
          if (!(prev_digit && next_digit)) end_word();
        }
        break;
      }
      case Cls::kSep:
        end_word();
        break;
    }
  }
  if (!full()) end_word();
}

// ---------------------------------------------------------------------------------------------
// Stop words (sorted; normalized form, apostrophes removed). Deliberately omits words that are
// also survival nouns: well, can, shed, shell, ill, will, use, fire, ...

namespace {
constexpr std::string_view kStopWords[] = {
    "a", "about", "above", "after", "again", "against", "all", "am", "an", "and", "any", "are",
    "arent", "as", "at", "be", "because", "been", "before", "being", "below", "between", "both",
    "but", "by", "cant", "could", "couldnt", "did", "didnt", "do", "does", "doesnt", "doing",
    "dont", "during", "each", "few", "for", "from", "further", "had", "hadnt", "has", "hasnt",
    "have", "havent", "having", "he", "hed", "hes", "her", "here", "heres", "hers", "herself",
    "him", "himself", "his", "how", "hows", "i", "if", "im", "in", "into", "is", "isnt", "it",
    "its", "itself", "ive", "just", "me", "more", "most", "mustnt", "my", "myself", "nor", "of",
    "on", "once", "only", "or", "other", "ought", "our", "ours", "ourselves", "own", "same",
    "shant", "she", "shes", "should", "shouldnt", "so", "some", "such", "than", "that", "thats",
    "the", "their", "theirs", "them", "themselves", "then", "there", "theres", "these", "they",
    "theyd", "theyll", "theyre", "theyve", "this", "those", "through", "to", "too", "until",
    "very", "was", "wasnt", "we", "were", "werent", "weve", "what", "whats", "when", "whens",
    "where", "wheres", "which", "while", "who", "whom", "whos", "why", "whys", "with", "wont",
    "would", "wouldnt", "you", "youd", "youll", "your", "youre", "yours", "yourself",
    "yourselves", "youve",
};
}  // namespace

bool is_stop_word(std::string_view w) {
  const auto* b = std::begin(kStopWords);
  const auto* e = std::end(kStopWords);
  const auto* it = std::lower_bound(b, e, w);
  return it != e && *it == w;
}

// ---------------------------------------------------------------------------------------------
// Porter step 1 (a, b, c)

namespace {
class Porter {
 public:
  explicit Porter(std::string& b) : b_(b), k_(static_cast<int>(b.size()) - 1) {}
  void run() {
    step1ab();
    step1c();
    b_.resize(static_cast<size_t>(k_ + 1));
  }

 private:
  bool cons(int i) const {
    switch (b_[static_cast<size_t>(i)]) {
      case 'a': case 'e': case 'i': case 'o': case 'u': return false;
      case 'y': return i == 0 ? true : !cons(i - 1);
      default: return true;
    }
  }
  int m() const {
    int n = 0, i = 0;
    for (;;) { if (i > j_) return n; if (!cons(i)) break; ++i; }
    ++i;
    for (;;) {
      for (;;) { if (i > j_) return n; if (cons(i)) break; ++i; }
      ++i; ++n;
      for (;;) { if (i > j_) return n; if (!cons(i)) break; ++i; }
      ++i;
    }
  }
  bool vowel_in_stem() const {
    for (int i = 0; i <= j_; ++i) if (!cons(i)) return true;
    return false;
  }
  bool doublec(int j) const { return j >= 1 && b_[static_cast<size_t>(j)] == b_[static_cast<size_t>(j - 1)] && cons(j); }
  bool cvc(int i) const {
    if (i < 2 || !cons(i) || cons(i - 1) || !cons(i - 2)) return false;
    char ch = b_[static_cast<size_t>(i)];
    return ch != 'w' && ch != 'x' && ch != 'y';
  }
  bool ends(std::string_view s) {
    int len = static_cast<int>(s.size());
    if (len > k_ + 1) return false;
    if (b_.compare(static_cast<size_t>(k_ - len + 1), static_cast<size_t>(len), s.data(), s.size()) != 0) return false;
    j_ = k_ - len;
    return true;
  }
  void setto(std::string_view s) {
    b_.resize(static_cast<size_t>(j_ + 1));
    b_.append(s.data(), s.size());
    k_ = j_ + static_cast<int>(s.size());
  }
  void step1ab() {
    if (b_[static_cast<size_t>(k_)] == 's') {
      if (ends("sses")) k_ -= 2;
      else if (ends("ies")) setto("i");
      else if (b_[static_cast<size_t>(k_ - 1)] != 's') --k_;
    }
    b_.resize(static_cast<size_t>(k_ + 1));
    if (ends("eed")) {
      if (m() > 0) --k_;
    } else if ((ends("ed") || ends("ing")) && vowel_in_stem()) {
      k_ = j_;
      b_.resize(static_cast<size_t>(k_ + 1));
      if (ends("at")) setto("ate");
      else if (ends("bl")) setto("ble");
      else if (ends("iz")) setto("ize");
      else if (doublec(k_)) {
        --k_;
        char ch = b_[static_cast<size_t>(k_)];
        if (ch == 'l' || ch == 's' || ch == 'z') ++k_;
      } else {
        j_ = k_;
        if (m() == 1 && cvc(k_)) setto("e");
      }
    }
    b_.resize(static_cast<size_t>(k_ + 1));
  }
  void step1c() {
    if (ends("y") && vowel_in_stem()) b_[static_cast<size_t>(k_)] = 'i';
  }

  std::string& b_;
  int k_;
  int j_ = 0;
};
}  // namespace

std::string stem(std::string_view w) {
  std::string s(w);
  if (s.size() <= 3) return s;
  for (char c : s)
    if (c < 'a' || c > 'z') return s;
  Porter p(s);
  p.run();
  // A few derivational endings common in the corpus, so that nouns meet their verbs:
  // purification/purify -> purifi, sterilization -> sterilize, dehydration -> dehydrate,
  // disinfection -> disinfect. Guards keep short words ("station", "section") intact.
  struct Rule { std::string_view suffix, repl; size_t min_stem; };
  static constexpr Rule kRules[] = {
      {"ification", "ifi", 2}, {"ization", "ize", 3}, {"ation", "ate", 4}, {"ection", "ect", 2},
  };
  for (const Rule& r : kRules) {
    if (s.size() >= r.suffix.size() + r.min_stem &&
        s.compare(s.size() - r.suffix.size(), r.suffix.size(), r.suffix.data(), r.suffix.size()) == 0) {
      s.resize(s.size() - r.suffix.size());
      s.append(r.repl.data(), r.repl.size());
      break;
    }
  }
  return s;
}

std::string index_term(std::string_view normalized) {
  if (is_stop_word(normalized)) return std::string();
  return stem(normalized);
}

// ---------------------------------------------------------------------------------------------
// Markdown -> plain text

namespace {

bool is_alnum(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
         (static_cast<unsigned char>(c) >= 0x80);
}

std::string_view ltrim(std::string_view s) {
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
  return s.substr(i);
}

bool is_rule_or_table_sep(std::string_view t) {
  // "---", "***", "___", "|---|:--:|", "- - -"
  if (t.empty()) return false;
  char f = t[0];
  if (f != '-' && f != '*' && f != '_' && f != '=' && f != '|' && f != ':') return false;  // fast reject
  int dashes = 0, other = 0;
  for (char c : t) {
    if (c == '-' || c == '*' || c == '_' || c == '=') ++dashes;
    else if (c == '|' || c == ':' || c == ' ' || c == '\t') {}
    else ++other;
  }
  return other == 0 && dashes >= 3;
}

void inline_plain(std::string_view s, std::string& out) {
  // Copies runs of ordinary characters in one append; only markup characters are handled singly.
  size_t i = 0, n = s.size(), run = 0;
  auto flush = [&](size_t upto) { if (upto > run) out.append(s.data() + run, upto - run); };
  while (i < n) {
    char c = s[i];
    switch (c) {
      case '\\':
        if (i + 1 < n) { flush(i); out.push_back(s[i + 1]); i += 2; run = i; continue; }
        break;
      case '!':
        if (i + 1 < n && s[i + 1] == '[') { flush(i); ++i; run = i; continue; }
        break;
      case '[':
        flush(i); ++i; run = i; continue;
      case ']': {
        flush(i);
        if (i + 1 < n && s[i + 1] == '(') {
          size_t close = s.find(')', i + 2);
          i = (close == std::string_view::npos) ? n : close + 1;
        } else {
          ++i;
        }
        run = i;
        continue;
      }
      case '<':
        if (s.substr(i + 1, 4) == "http" || s.substr(i + 1, 7) == "mailto:") {
          flush(i);
          size_t close = s.find('>', i);
          i = (close == std::string_view::npos) ? n : close + 1;
          run = i;
          continue;
        }
        break;
      case 'h': case 'w':
        if ((i == 0 || !is_alnum(s[i - 1])) &&
            (s.substr(i, 7) == "http://" || s.substr(i, 8) == "https://" || s.substr(i, 4) == "www.")) {
          flush(i);
          while (i < n && s[i] != ' ' && s[i] != '\t' && s[i] != ')') ++i;
          run = i;
          continue;
        }
        break;
      case '*': case '`': case '~':
        flush(i); ++i; run = i; continue;
      case '|':
        flush(i); out.push_back(' '); ++i; run = i; continue;
      case '_': {
        bool prev = i > 0 && is_alnum(s[i - 1]);
        bool next = i + 1 < n && is_alnum(s[i + 1]);
        if (!(prev && next)) { flush(i); ++i; run = i; continue; }
        break;
      }
      default:
        break;
    }
    ++i;
  }
  flush(n);
}

}  // namespace

std::string markdown_to_plain(std::string_view md) {
  std::string out;
  out.reserve(md.size());
  bool fence = false;
  size_t pos = 0;
  while (pos <= md.size()) {
    size_t nl = md.find('\n', pos);
    if (nl == std::string_view::npos) nl = md.size();
    std::string_view line = md.substr(pos, nl - pos);
    pos = nl + 1;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    std::string_view t = ltrim(line);
    if (t.substr(0, 3) == "```" || t.substr(0, 3) == "~~~") { fence = !fence; continue; }
    if (fence) { out.append(line.data(), line.size()); out.push_back('\n'); continue; }
    if (is_rule_or_table_sep(t)) continue;
    while (!t.empty() && t[0] == '>') t = ltrim(t.substr(1));
    if (!t.empty() && t[0] == '#') {
      size_t h = 0;
      while (h < t.size() && t[h] == '#') ++h;
      if (h <= 6 && (h == t.size() || t[h] == ' ' || t[h] == '\t')) t = ltrim(t.substr(h));
      while (!t.empty() && (t.back() == '#' || t.back() == ' ')) t.remove_suffix(1);
    } else if (t.size() >= 2 && (t[0] == '-' || t[0] == '*' || t[0] == '+') && (t[1] == ' ' || t[1] == '\t')) {
      t = ltrim(t.substr(2));
    } else {
      size_t d = 0;
      while (d < t.size() && d < 9 && t[d] >= '0' && t[d] <= '9') ++d;
      if (d > 0 && d + 1 < t.size() && (t[d] == '.' || t[d] == ')') && (t[d + 1] == ' ' || t[d + 1] == '\t'))
        t = ltrim(t.substr(d + 2));
    }
    if (t.size() >= 4 && t[0] == '[' && (t[1] == ' ' || t[1] == 'x' || t[1] == 'X') && t[2] == ']' && t[3] == ' ')
      t = ltrim(t.substr(4));
    inline_plain(t, out);
    out.push_back('\n');
    if (nl == md.size()) break;
  }
  return out;
}

}  // namespace ember::guides
