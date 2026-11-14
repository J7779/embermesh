#include "guides/passages.h"

#include <algorithm>
#include <string>

#include "guides/text.h"

namespace ember::guides {

namespace {

bool is_ws(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

std::string_view trim(std::string_view s) {
  while (!s.empty() && is_ws(s.front())) s.remove_prefix(1);
  while (!s.empty() && is_ws(s.back())) s.remove_suffix(1);
  return s;
}

bool starts_with(std::string_view s, std::string_view p) { return s.substr(0, p.size()) == p; }

bool boilerplate_heading(std::string_view h) {
  std::string l = ascii_lower(trim(h));
  while (!l.empty() && (l.back() == ':' || l.back() == '.')) l.pop_back();
  static constexpr std::string_view kSkip[] = {
      "sources", "source", "references", "reference list", "bibliography", "external links", "see also",
      "further reading", "notes", "notes and references", "licence", "license", "licensing", "attribution",
      "credits", "footnotes", "citations", "sources and page info", "works cited", "copyright",
  };
  for (std::string_view k : kSkip)
    if (l == k) return true;
  return false;
}

bool boilerplate_block(std::string_view b) {
  b = trim(b);
  if (b.size() >= 4 && starts_with(b, "_(") && b.substr(b.size() - 2) == ")_") return true;
  if (b.size() > 600) return false;
  static constexpr std::string_view kStarts[] = {
      "Source:", "Sources:", "**Source:**", "**Sources:**", "*Source:*", "Licence:", "License:", "Licence decision",
      "Licence evidence", "**Licence", "**License", "Attribution:", "**Attribution", "Text adapted from",
  };
  for (std::string_view k : kStarts)
    if (starts_with(b, k)) return true;
  return false;
}

size_t text_bytes(std::string_view s) {
  size_t n = 0;
  for (char c : s)
    if (!is_ws(c) && c != '#' && c != '*' && c != '-' && c != '|' && c != '_' && c != '>' && c != '`') ++n;
  return n;
}

// Is s[i] the end of a sentence ('.', '!' or '?' followed by whitespace)?
bool sentence_end(std::string_view s, size_t i) {
  return i + 1 < s.size() && (s[i] == '.' || s[i] == '!' || s[i] == '?') && is_ws(s[i + 1]);
}

class Splitter {
 public:
  Splitter(std::string_view md, const PassageOptions& o) : md_(md), o_(o) {}

  std::vector<PassageSpan> run() {
    bool fence = false, skipping = false;
    int skip_level = 0;
    size_t block_start = std::string_view::npos, block_end = 0;
    auto end_block = [&]() {
      if (block_start != std::string_view::npos && !skipping) add_block(block_start, block_end);
      block_start = std::string_view::npos;
    };
    size_t pos = 0;
    while (pos < md_.size()) {
      size_t nl = md_.find('\n', pos);
      size_t le = nl == std::string_view::npos ? md_.size() : nl;
      std::string_view line = md_.substr(pos, le - pos);
      std::string_view t = trim(line);
      size_t next = nl == std::string_view::npos ? md_.size() : nl + 1;
      bool fence_line = starts_with(t, "```") || starts_with(t, "~~~");
      if (!fence && !fence_line && t.empty()) {
        end_block();
      } else if (!fence && !fence_line && t[0] == '#') {
        size_t h = 0;
        while (h < t.size() && t[h] == '#') ++h;
        if (h <= 6 && (h == t.size() || t[h] == ' ' || t[h] == '\t')) {
          end_block();
          end_section();
          std::string_view text = trim(t.substr(h));
          while (!text.empty() && text.back() == '#') text.remove_suffix(1);
          text = trim(text);
          int level = static_cast<int>(h);
          if (skipping && level <= skip_level) skipping = false;
          if (!skipping && boilerplate_heading(text)) { skipping = true; skip_level = level; }
          if (level >= 2 && !text.empty()) {
            head_off_ = static_cast<uint32_t>(text.data() - md_.data());
            head_len_ = static_cast<uint32_t>(std::min<size_t>(text.size(), 200));
            head_len_ = static_cast<uint32_t>(utf8_floor(text, head_len_));
          } else {
            head_off_ = head_len_ = 0;
          }
          pos = next;
          continue;
        }
        if (block_start == std::string_view::npos) block_start = pos;
        block_end = le;
      } else {
        if (fence_line) fence = !fence;
        if (block_start == std::string_view::npos) block_start = pos;
        block_end = le;
      }
      pos = next;
    }
    end_block();
    end_section();
    return std::move(out_);
  }

 private:
  void add_block(size_t b, size_t e) {
    if (e <= b) return;
    if (boilerplate_block(md_.substr(b, e - b))) return;
    if (e - b > o_.max_bytes) {
      flush();
      split_long(b, e);
      return;
    }
    if (!has_cur_) { cs_ = b; ce_ = e; has_cur_ = true; return; }
    if (e - cs_ <= o_.max_bytes) { ce_ = e; return; }
    flush();
    cs_ = b;
    ce_ = e;
    has_cur_ = true;
  }

  void split_long(size_t b, size_t e) {
    size_t p = b;
    while (e - p > o_.max_bytes) {
      size_t lim = p + o_.max_bytes, half = p + o_.max_bytes / 2, cut = 0;
      for (size_t i = lim; i > half; --i)
        if (sentence_end(md_, i - 1)) { cut = i; break; }
      if (!cut)
        for (size_t i = lim; i > half; --i)
          if (md_[i - 1] == '\n') { cut = i; break; }
      if (!cut)
        for (size_t i = lim; i > half; --i)
          if (md_[i - 1] == ' ') { cut = i; break; }
      if (!cut) cut = utf8_floor(md_, lim);
      if (cut <= p) cut = lim;
      emit(p, cut);
      // Overlap: start the next piece at the last sentence of this one if it is short.
      size_t q = cut;
      size_t back = 0;
      for (size_t i = cut - 1; i > p + 1 && cut - i <= o_.overlap_bytes; --i)
        if (sentence_end(md_, i - 1)) { back = i; break; }
      if (back > p && back < cut) q = back;
      while (q < e && is_ws(md_[q])) ++q;
      if (q <= p) q = cut;
      p = q;
    }
    if (p < e) { cs_ = p; ce_ = e; has_cur_ = true; }
  }

  void flush() {
    if (has_cur_) emit(cs_, ce_);
    has_cur_ = false;
  }

  void end_section() {
    if (!has_cur_) { section_first_ = out_.size(); return; }
    if (ce_ - cs_ < o_.min_bytes && out_.size() > section_first_) {
      PassageSpan& last = out_.back();
      if (ce_ - last.off <= o_.max_bytes + o_.min_bytes) {
        last.len = static_cast<uint32_t>(ce_ - last.off);
        has_cur_ = false;
        section_first_ = out_.size();
        return;
      }
    }
    flush();
    section_first_ = out_.size();
  }

  void emit(size_t b, size_t e) {
    while (b < e && is_ws(md_[b])) ++b;
    while (e > b && is_ws(md_[e - 1])) --e;
    if (text_bytes(md_.substr(b, e - b)) < 24) return;
    PassageSpan s;
    s.off = static_cast<uint32_t>(b);
    s.len = static_cast<uint32_t>(e - b);
    s.head_off = head_off_;
    s.head_len = head_len_;
    out_.push_back(s);
  }

  std::string_view md_;
  PassageOptions o_;
  std::vector<PassageSpan> out_;
  size_t cs_ = 0, ce_ = 0, section_first_ = 0;
  bool has_cur_ = false;
  uint32_t head_off_ = 0, head_len_ = 0;
};

}  // namespace

std::vector<PassageSpan> split_passages(std::string_view md, const PassageOptions& opt) {
  PassageOptions o = opt;
  if (o.max_bytes < 64) o.max_bytes = 64;
  if (o.max_bytes > 16000) o.max_bytes = 16000;
  if (o.min_bytes > o.max_bytes / 2) o.min_bytes = o.max_bytes / 2;
  if (o.overlap_bytes > o.max_bytes / 2) o.overlap_bytes = o.max_bytes / 2;
  return Splitter(md, o).run();
}

}  // namespace ember::guides
