#include "framing/aprs.h"

#include <cmath>
#include <cstdio>

namespace ember::aprs {

namespace {

bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_alnum(char c) { return is_digit(c) || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
bool is_printable(char c) { return c >= 0x20 && c <= 0x7E; }

bool valid_id(std::string_view id) {
  if (id.empty() || id.size() > 5) return false;
  for (char c : id)
    if (!is_alnum(c)) return false;
  return true;
}

bool valid_addressee(std::string_view a) {
  if (a.empty() || a.size() > 9) return false;
  for (char c : a)
    if (!is_printable(c) || c == ':' || c == ' ') return false;
  return true;
}

// Message text: printable ASCII or bytes >= 0x80 (UTF-8), excluding | ~ {.
bool valid_text_byte(unsigned char c) {
  if (c >= 0x80) return true;
  return c >= 0x20 && c <= 0x7E && c != '|' && c != '~' && c != '{';
}

std::string_view trim_eol(std::string_view s) {
  while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.remove_suffix(1);
  return s;
}

void append_addressee(std::string_view addressee, std::string* out) {
  out->push_back(':');
  out->append(addressee.data(), addressee.size());
  out->append(9 - addressee.size(), ' ');
  out->push_back(':');
}

bool valid_symbol_table(char c) {
  return c == '/' || c == '\\' || is_digit(c) || (c >= 'A' && c <= 'Z');
}

bool valid_timestamp(std::string_view t) {
  if (t.size() != 7) return false;
  for (size_t i = 0; i < 6; ++i)
    if (!is_digit(t[i])) return false;
  return t[6] == 'z' || t[6] == '/' || t[6] == 'h';
}

// Parses "DDMM.hhN" (lat, deg_digits=2) or "DDDMM.hhE" (lon, deg_digits=3). Spaces may replace
// trailing digits (position ambiguity). Returns false on any malformation.
bool parse_coord(std::string_view s, size_t deg_digits, double* out, uint8_t* amb) {
  size_t n = deg_digits + 5;  // digits + '.' + 2 digits... plus hemisphere
  if (s.size() != n + 1) return false;
  if (s[deg_digits + 2] != '.') return false;
  char digits[8];
  size_t nd = 0;
  for (size_t i = 0; i < n; ++i)
    if (i != deg_digits + 2) digits[nd++] = s[i];
  // count trailing spaces (from the right), no space may precede a digit
  uint8_t spaces = 0;
  bool digit_seen = false;
  for (size_t k = nd; k-- > 0;) {
    char c = digits[k];
    if (c == ' ') {
      if (digit_seen) return false;
      ++spaces;
      digits[k] = '0';
    } else if (is_digit(c)) {
      digit_seen = true;
    } else {
      return false;
    }
  }
  if (spaces > 4) return false;
  int deg = 0;
  for (size_t k = 0; k < deg_digits; ++k) deg = deg * 10 + (digits[k] - '0');
  int mins = (digits[deg_digits] - '0') * 10 + (digits[deg_digits + 1] - '0');
  int hund = (digits[deg_digits + 2] - '0') * 10 + (digits[deg_digits + 3] - '0');
  if (mins >= 60) return false;
  double v = deg + (mins + hund / 100.0) / 60.0;
  char h = s[n];
  bool neg;
  if (deg_digits == 2) {
    if (h == 'N' || h == 'n') neg = false;
    else if (h == 'S' || h == 's') neg = true;
    else return false;
    if (v > 90.0) return false;
  } else {
    if (h == 'E' || h == 'e') neg = false;
    else if (h == 'W' || h == 'w') neg = true;
    else return false;
    if (v > 180.0) return false;
  }
  *out = neg ? -v : v;
  *amb = spaces;
  return true;
}

void format_coord(double v, bool is_lat, uint8_t ambiguity, std::string* out) {
  bool neg = v < 0;
  double a = std::fabs(v);
  if (!(a <= 180.0)) a = 180.0;  // also NaN: lround of a huge or NaN value is unspecified
  long total = std::lround(a * 6000.0);  // hundredths of a minute
  long deg = total / 6000;
  long rem = total % 6000;
  char buf[64];  // fits any long values (GCC -Wformat-truncation), not just valid coordinates
  if (is_lat) std::snprintf(buf, sizeof buf, "%02ld%02ld.%02ld%c", deg, rem / 100, rem % 100, neg ? 'S' : 'N');
  else std::snprintf(buf, sizeof buf, "%03ld%02ld.%02ld%c", deg, rem / 100, rem % 100, neg ? 'W' : 'E');
  std::string s(buf);
  // Blank `ambiguity` digits from the right: positions of hh and mm.
  static const int kPosLat[4] = {6, 5, 3, 2};
  static const int kPosLon[4] = {7, 6, 4, 3};
  for (uint8_t k = 0; k < ambiguity && k < 4; ++k) s[static_cast<size_t>(is_lat ? kPosLat[k] : kPosLon[k])] = ' ';
  out->append(s);
}

// Extracts "/A=nnnnnn" (feet, may be negative "-nnnnn") anywhere in the comment.
void parse_altitude_ext(std::string_view c, Position* p) {
  size_t at = c.find("/A=");
  if (at == std::string_view::npos || c.size() - at < 9) return;
  std::string_view v = c.substr(at + 3, 6);
  bool neg = v[0] == '-';
  int32_t val = 0;
  for (size_t i = neg ? 1 : 0; i < 6; ++i) {
    if (!is_digit(v[i])) return;
    val = val * 10 + (v[i] - '0');
  }
  p->has_altitude = true;
  p->altitude_ft = neg ? -val : val;
}

bool parse_uncompressed(std::string_view s, Position* p) {
  if (s.size() < 19) return false;
  uint8_t amb_lat = 0, amb_lon = 0;
  if (!parse_coord(s.substr(0, 8), 2, &p->lat, &amb_lat)) return false;
  p->symbol_table = s[8];
  if (!parse_coord(s.substr(9, 9), 3, &p->lon, &amb_lon)) return false;
  p->symbol_code = s[18];
  if (!valid_symbol_table(p->symbol_table) && !(p->symbol_table >= 'a' && p->symbol_table <= 'j')) return false;
  if (!is_printable(p->symbol_code)) return false;
  p->ambiguity = amb_lat;
  p->compressed = false;
  std::string_view c = s.substr(19);
  p->comment.assign(c.data(), c.size());
  // CSE/SPD data extension: "ddd/ddd" (not for weather '_' which reuses it for wind; harmless)
  if (c.size() >= 7 && c[3] == '/' && is_digit(c[0]) && is_digit(c[1]) && is_digit(c[2]) &&
      is_digit(c[4]) && is_digit(c[5]) && is_digit(c[6])) {
    int crs = (c[0] - '0') * 100 + (c[1] - '0') * 10 + (c[2] - '0');
    int spd = (c[4] - '0') * 100 + (c[5] - '0') * 10 + (c[6] - '0');
    if (crs <= 360) {
      p->has_course_speed = true;
      p->course_deg = static_cast<uint16_t>(crs);
      p->speed_knots = static_cast<uint16_t>(spd);
    }
  }
  parse_altitude_ext(c, p);
  return true;
}

bool is_b91(char c) { return c >= 33 && c <= 124; }

bool parse_compressed(std::string_view s, Position* p) {
  if (s.size() < 13) return false;
  char table = s[0];
  if (!(valid_symbol_table(table) || (table >= 'a' && table <= 'j'))) return false;
  uint32_t y = 0, x = 0;
  if (!base91_decode(s.substr(1, 4), &y) || !base91_decode(s.substr(5, 4), &x)) return false;
  p->lat = 90.0 - y / 380926.0;
  p->lon = -180.0 + x / 190463.0;
  if (p->lat < -90.0 || p->lat > 90.0 || p->lon < -180.0 || p->lon > 180.0) return false;
  // Overlay digits are sent as a-j in compressed format.
  p->symbol_table = (table >= 'a' && table <= 'j') ? static_cast<char>('0' + (table - 'a')) : table;
  p->symbol_code = s[9];
  if (!is_printable(p->symbol_code)) return false;
  char c = s[10], sp = s[11], t = s[12];
  if (c != ' ' && is_b91(c) && is_b91(sp) && is_b91(t)) {
    int ci = c - 33, si = sp - 33, ti = t - 33;
    if ((ti & 0x18) == 0x10) {  // GGA origin: cs is altitude
      p->has_altitude = true;
      p->altitude_ft = static_cast<int32_t>(std::pow(1.002, ci * 91 + si));  // truncated, as in APRS101
    } else if (ci <= 89) {
      p->has_course_speed = true;
      p->course_deg = static_cast<uint16_t>(ci * 4);
      p->speed_knots = static_cast<uint16_t>(std::lround(std::pow(1.08, si) - 1.0));
    }
  }
  p->compressed = true;
  p->ambiguity = 0;
  std::string_view cm = s.substr(13);
  p->comment.assign(cm.data(), cm.size());
  if (!p->has_altitude) parse_altitude_ext(cm, p);
  return true;
}

}  // namespace

// --- base91 -------------------------------------------------------------------------------

void base91_encode(uint32_t value, size_t digits, std::string* out) {
  char buf[8];
  size_t n = digits > 8 ? 8 : digits;
  for (size_t i = n; i-- > 0;) {
    buf[i] = static_cast<char>(33 + value % 91);
    value /= 91;
  }
  out->append(buf, n);
}

bool base91_decode(std::string_view s, uint32_t* value) {
  if (s.empty() || s.size() > 4) return false;
  uint32_t v = 0;
  for (char c : s) {
    if (!is_b91(c)) return false;
    v = v * 91 + static_cast<uint32_t>(c - 33);
  }
  *value = v;
  return true;
}

// --- messages -------------------------------------------------------------------------------

bool build_message(std::string_view addressee, std::string_view text, std::string_view id, std::string* out) {
  if (!valid_addressee(addressee) || text.size() > kMaxMessageText) return false;
  if (!id.empty() && !valid_id(id)) return false;
  for (char c : text)
    if (!valid_text_byte(static_cast<unsigned char>(c))) return false;
  std::string s;
  append_addressee(addressee, &s);
  s.append(text.data(), text.size());
  if (!id.empty()) {
    s.push_back('{');
    s.append(id.data(), id.size());
  }
  *out = std::move(s);
  return true;
}

static bool build_ackrej(const char* what, std::string_view addressee, std::string_view id, std::string* out) {
  if (!valid_addressee(addressee) || !valid_id(id)) return false;
  std::string s;
  append_addressee(addressee, &s);
  s.append(what);
  s.append(id.data(), id.size());
  *out = std::move(s);
  return true;
}

bool build_ack(std::string_view addressee, std::string_view id, std::string* out) {
  return build_ackrej("ack", addressee, id, out);
}
bool build_rej(std::string_view addressee, std::string_view id, std::string* out) {
  return build_ackrej("rej", addressee, id, out);
}

bool build_bulletin(char line, std::string_view text, std::string* out) {
  if (!(is_digit(line) || (line >= 'A' && line <= 'Z'))) return false;
  char addr[4] = {'B', 'L', 'N', line};
  return build_message(std::string_view(addr, 4), text, {}, out);
}

std::string sanitize_message_text(std::string_view text) {
  std::string out;
  size_t i = 0;
  while (i < text.size()) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    size_t len = 1;
    bool ok;
    if (c < 0x80) {
      ok = valid_text_byte(c);
    } else {
      // validate one UTF-8 sequence (structure only)
      if (c >= 0xC2 && c <= 0xDF) len = 2;
      else if (c >= 0xE0 && c <= 0xEF) len = 3;
      else if (c >= 0xF0 && c <= 0xF4) len = 4;
      else len = 0;
      ok = len != 0 && i + len <= text.size();
      for (size_t k = 1; ok && k < len; ++k)
        ok = (static_cast<unsigned char>(text[i + k]) & 0xC0) == 0x80;
      if (!ok) len = 1;
    }
    size_t add = ok ? len : 1;
    if (out.size() + add > kMaxMessageText) break;
    if (ok) out.append(text.data() + i, len);
    else out.push_back('?');
    i += len;
  }
  return out;
}

bool parse_message(std::string_view info, Message* out) {
  info = trim_eol(info);
  if (info.size() < 11 || info[0] != ':' || info[10] != ':') return false;
  std::string_view addr = info.substr(1, 9);
  while (!addr.empty() && addr.back() == ' ') addr.remove_suffix(1);
  if (addr.empty()) return false;
  for (char c : addr)
    if (!is_printable(c)) return false;
  Message m;
  m.addressee.assign(addr.data(), addr.size());
  std::string_view text = info.substr(11);
  if (text.size() >= 4 && (text.compare(0, 3, "ack") == 0 || text.compare(0, 3, "rej") == 0)) {
    std::string_view rest = text.substr(3);
    size_t brace = rest.find('}');  // tolerate "ackMM}AA" style
    std::string_view id = brace == std::string_view::npos ? rest : rest.substr(0, brace);
    while (!id.empty() && id.back() == ' ') id.remove_suffix(1);
    if (valid_id(id)) {
      m.kind = text[0] == 'a' ? MessageKind::Ack : MessageKind::Rej;
      m.id.assign(id.data(), id.size());
      if (brace != std::string_view::npos) {
        std::string_view ra = rest.substr(brace + 1);
        if (valid_id(ra)) m.reply_ack.assign(ra.data(), ra.size());
      }
      *out = std::move(m);
      return true;
    }
  }
  size_t lb = text.rfind('{');
  if (lb != std::string_view::npos) {
    std::string_view tail = text.substr(lb + 1);
    size_t rb = tail.find('}');
    std::string_view id = rb == std::string_view::npos ? tail : tail.substr(0, rb);
    std::string_view ra = rb == std::string_view::npos ? std::string_view() : tail.substr(rb + 1);
    if (valid_id(id) && (ra.empty() || valid_id(ra))) {
      m.id.assign(id.data(), id.size());
      m.reply_ack.assign(ra.data(), ra.size());
      text = text.substr(0, lb);
    }
  }
  m.text.assign(text.data(), text.size());
  *out = std::move(m);
  return true;
}

// --- positions -------------------------------------------------------------------------------

bool build_position(const Position& p, std::string* out) {
  if (!(p.lat >= -90.0 && p.lat <= 90.0 && p.lon >= -180.0 && p.lon <= 180.0)) return false;
  if (!valid_symbol_table(p.symbol_table) || !is_printable(p.symbol_code) || p.symbol_code == ' ') return false;
  if (p.ambiguity > 4) return false;
  for (char c : p.comment)
    if (c == '\r' || c == '\n' || c == 0) return false;
  std::string s;
  bool ts = valid_timestamp(p.timestamp);
  s.push_back(ts ? (p.messaging ? '@' : '/') : (p.messaging ? '=' : '!'));
  if (ts) s.append(p.timestamp);
  bool alt_in_comment = p.has_altitude;
  if (p.compressed) {
    // Truncation (not rounding) matches the APRS101 worked example and common encoders.
    double yv = std::floor(380926.0 * (90.0 - p.lat));
    double xv = std::floor(190463.0 * (180.0 + p.lon));
    uint32_t y = static_cast<uint32_t>(yv < 0 ? 0 : (yv > 68574960.0 ? 68574960.0 : yv));
    uint32_t x = static_cast<uint32_t>(xv < 0 ? 0 : (xv > 68574960.0 ? 68574960.0 : xv));
    char table = is_digit(p.symbol_table) ? static_cast<char>('a' + (p.symbol_table - '0')) : p.symbol_table;
    s.push_back(table);
    base91_encode(y, 4, &s);
    base91_encode(x, 4, &s);
    s.push_back(p.symbol_code);
    if (p.has_course_speed) {
      int crs = (p.course_deg % 360) / 4;
      int sp = static_cast<int>(std::lround(std::log(p.speed_knots + 1.0) / std::log(1.08)));
      if (sp > 89) sp = 89;
      s.push_back(static_cast<char>(33 + crs));
      s.push_back(static_cast<char>(33 + sp));
      s.push_back(static_cast<char>(33 + (0x20 | 0x18 | 0x06)));  // current fix, RMC, software
    } else if (p.has_altitude && p.altitude_ft >= 1) {
      long v = std::lround(std::log(static_cast<double>(p.altitude_ft)) / std::log(1.002));
      if (v < 0) v = 0;
      if (v > 91 * 91 - 1) v = 91 * 91 - 1;
      s.push_back(static_cast<char>(33 + v / 91));
      s.push_back(static_cast<char>(33 + v % 91));
      s.push_back(static_cast<char>(33 + (0x20 | 0x10 | 0x06)));  // current fix, GGA, software
      alt_in_comment = false;
    } else {
      s.append("  ");
      s.push_back(static_cast<char>(33 + (0x20 | 0x06)));
    }
  } else {
    format_coord(p.lat, true, p.ambiguity, &s);
    s.push_back(p.symbol_table);
    format_coord(p.lon, false, p.ambiguity, &s);
    s.push_back(p.symbol_code);
    if (p.has_course_speed) {
      char buf[16];
      unsigned crs = p.course_deg > 360 ? 0u : p.course_deg;
      unsigned spd = p.speed_knots > 999 ? 999u : p.speed_knots;
      std::snprintf(buf, sizeof buf, "%03u/%03u", crs, spd);
      s.append(buf);
    }
  }
  if (alt_in_comment) {
    char buf[16];
    int32_t a = p.altitude_ft;
    if (a > 999999) a = 999999;
    if (a < -99999) a = -99999;
    if (a < 0) std::snprintf(buf, sizeof buf, "/A=-%05d", static_cast<int>(-a));
    else std::snprintf(buf, sizeof buf, "/A=%06d", static_cast<int>(a));
    s.append(buf);
  }
  s.append(p.comment);
  if (s.size() > kMaxInfo) return false;
  *out = std::move(s);
  return true;
}

bool parse_position(std::string_view info, Position* out) {
  info = trim_eol(info);
  if (info.empty()) return false;
  Position p;
  char t = info[0];
  std::string_view s = info.substr(1);
  if (t == '/' || t == '@') {
    if (s.size() < 7 || !valid_timestamp(s.substr(0, 7))) return false;
    p.timestamp.assign(s.data(), 7);
    s = s.substr(7);
  } else if (t != '!' && t != '=') {
    return false;
  }
  p.messaging = (t == '=' || t == '@');
  if (s.empty()) return false;
  bool ok = (is_digit(s[0]) || s[0] == ' ') ? parse_uncompressed(s, &p) : parse_compressed(s, &p);
  if (!ok) return false;
  *out = std::move(p);
  return true;
}

bool parse_mic_e(std::string_view dest, std::string_view info, Position* out) {
  info = trim_eol(info);
  if (dest.size() != 6 || info.size() < 9) return false;
  char t = info[0];
  if (t != '`' && t != '\'' && t != 0x1C && t != 0x1D) return false;
  int digit[6];
  int msg_bits = 0;
  uint8_t amb = 0;
  bool north = false, lon_offset = false, west = false;
  for (int i = 0; i < 6; ++i) {
    char c = dest[static_cast<size_t>(i)];
    bool custom = false, std_bit = false, space = false;
    if (c >= '0' && c <= '9') digit[i] = c - '0';
    else if (c >= 'A' && c <= 'J') { digit[i] = c - 'A'; custom = true; }
    else if (c == 'K') { digit[i] = 0; custom = true; space = true; }
    else if (c == 'L') { digit[i] = 0; space = true; }
    else if (c >= 'P' && c <= 'Y') { digit[i] = c - 'P'; std_bit = true; }
    else if (c == 'Z') { digit[i] = 0; std_bit = true; space = true; }
    else return false;
    if (i >= 3 && custom) return false;  // A-K only allowed in the message bit positions
    if (i < 3 && (custom || std_bit)) msg_bits |= 1 << (2 - i);
    bool hi = (c >= 'P' && c <= 'Z');
    if (i == 3) north = hi;
    if (i == 4) lon_offset = hi;
    if (i == 5) west = hi;
    if (space) ++amb;
    else if (amb) return false;  // ambiguity spaces must be trailing
  }
  int lat_deg = digit[0] * 10 + digit[1];
  int lat_min = digit[2] * 10 + digit[3];
  int lat_hun = digit[4] * 10 + digit[5];
  if (lat_deg > 90 || lat_min >= 60) return false;
  double lat = lat_deg + (lat_min + lat_hun / 100.0) / 60.0;
  if (lat > 90.0) return false;

  auto b = [&](size_t i) { return static_cast<int>(static_cast<unsigned char>(info[i])); };
  for (size_t i = 1; i <= 6; ++i)
    if (b(i) < 28 || b(i) > 127) return false;
  int lon_deg = b(1) - 28;
  if (lon_offset) lon_deg += 100;
  if (lon_deg >= 180 && lon_deg <= 189) lon_deg -= 80;
  else if (lon_deg >= 190 && lon_deg <= 199) lon_deg -= 190;
  int lon_min = b(2) - 28;
  if (lon_min >= 60) lon_min -= 60;
  int lon_hun = b(3) - 28;
  if (lon_deg > 180 || lon_min >= 60 || lon_hun > 99) return false;
  double lon = lon_deg + (lon_min + lon_hun / 100.0) / 60.0;
  if (lon > 180.0) return false;

  int sp = b(4) - 28, dc = b(5) - 28, se = b(6) - 28;
  int speed = sp * 10 + dc / 10;
  int course = (dc % 10) * 100 + se;
  if (speed >= 800) speed -= 800;
  if (course >= 400) course -= 400;

  Position p;
  p.mic_e = true;
  p.lat = north ? lat : -lat;
  p.lon = west ? -lon : lon;
  p.ambiguity = amb;
  p.mic_e_message = static_cast<uint8_t>(msg_bits);
  p.symbol_code = info[7];
  p.symbol_table = info[8];
  if (!is_printable(p.symbol_code) || !valid_symbol_table(p.symbol_table)) return false;
  if (course >= 0 && course <= 360 && speed >= 0) {
    p.has_course_speed = true;
    p.course_deg = static_cast<uint16_t>(course);
    p.speed_knots = static_cast<uint16_t>(speed);
  }
  std::string_view c = info.substr(9);
  p.comment.assign(c.data(), c.size());
  // Optional altitude: 3 base-91 chars followed by '}', meters relative to -10000 m. It may be
  // preceded by a one-character radio type code ('>', ']', '`', '\'').
  for (size_t off = 0; off <= 1 && c.size() >= off + 4; ++off) {
    if (c[off + 3] != '}') continue;
    uint32_t v = 0;
    if (base91_decode(c.substr(off, 3), &v)) {
      p.has_altitude = true;
      p.altitude_ft = static_cast<int32_t>(std::lround((static_cast<double>(v) - 10000.0) * 3.28084));
      break;
    }
  }
  *out = std::move(p);
  return true;
}

// --- status -----------------------------------------------------------------------------------

bool build_status(std::string_view text, std::string* out) {
  if (text.size() + 1 > kMaxInfo) return false;
  for (char c : text)
    if (c == '\r' || c == '\n' || c == 0 || c == '|' || c == '~') return false;
  std::string s(">");
  s.append(text.data(), text.size());
  *out = std::move(s);
  return true;
}

bool parse_status(std::string_view info, std::string* out) {
  info = trim_eol(info);
  if (info.empty() || info[0] != '>') return false;
  out->assign(info.data() + 1, info.size() - 1);
  return true;
}

// --- dispatch -----------------------------------------------------------------------------

Packet parse(std::string_view dest_call, std::string_view info) {
  Packet pk;
  if (info.empty()) return pk;
  switch (info[0]) {
    case '!': case '=': case '/': case '@':
      if (parse_position(info, &pk.position)) pk.type = PacketType::Position;
      break;
    case '`': case '\'': case 0x1C: case 0x1D:
      if (parse_mic_e(dest_call, info, &pk.position)) pk.type = PacketType::Position;
      break;
    case ':':
      if (parse_message(info, &pk.message)) pk.type = PacketType::Message;
      break;
    case '>':
      if (parse_status(info, &pk.status)) pk.type = PacketType::Status;
      break;
    default:
      break;
  }
  return pk;
}

Packet parse(const ax25::Frame& f) {
  std::string_view info(reinterpret_cast<const char*>(f.info.data()), f.info.size());
  return parse(f.dst.call, info);
}

// --- TNC2 -------------------------------------------------------------------------------------

std::string format_tnc2(const ax25::Frame& f) {
  std::string s = f.src.to_string();
  s.push_back('>');
  s.append(f.dst.to_string());
  int last_h = -1;
  for (size_t i = 0; i < f.digis.size(); ++i)
    if (f.digis[i].flag) last_h = static_cast<int>(i);
  for (size_t i = 0; i < f.digis.size(); ++i) {
    s.push_back(',');
    s.append(f.digis[i].to_string());
    if (static_cast<int>(i) == last_h) s.push_back('*');
  }
  s.push_back(':');
  s.append(reinterpret_cast<const char*>(f.info.data()), f.info.size());
  return s;
}

bool parse_tnc2(std::string_view line, ax25::Frame* out) {
  line = trim_eol(line);
  size_t colon = line.find(':');
  if (colon == std::string_view::npos) return false;
  std::string_view header = line.substr(0, colon);
  std::string_view info = line.substr(colon + 1);
  if (info.size() > kMaxInfo) return false;
  size_t gt = header.find('>');
  if (gt == std::string_view::npos) return false;
  ax25::Frame f;
  if (!ax25::parse_address(header.substr(0, gt), &f.src) || f.src.flag) return false;
  std::string_view rest = header.substr(gt + 1);
  bool first = true;
  int last_h = -1;
  for (;;) {
    size_t comma = rest.find(',');
    std::string_view part = comma == std::string_view::npos ? rest : rest.substr(0, comma);
    ax25::Address a;
    if (!ax25::parse_address(part, &a)) return false;
    if (first) {
      if (a.flag) return false;
      f.dst = a;
      first = false;
    } else {
      if (f.digis.size() >= ax25::kMaxDigis) return false;
      if (a.flag) last_h = static_cast<int>(f.digis.size());
      f.digis.push_back(a);
    }
    if (comma == std::string_view::npos) break;
    rest = rest.substr(comma + 1);
  }
  for (int i = 0; i <= last_h; ++i) f.digis[static_cast<size_t>(i)].flag = true;
  f.control = ax25::kControlUI;
  f.pid = ax25::kPidNoLayer3;
  f.info.assign(info.begin(), info.end());
  f.set_command(true);
  *out = std::move(f);
  return true;
}

}  // namespace ember::aprs
