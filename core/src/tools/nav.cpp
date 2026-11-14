#include "tools/nav.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace ember {
namespace nav {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
double rad(double d) { return d * kDeg; }
double deg(double r) { return r / kDeg; }
double fmod360(double x) {
  double r = std::fmod(x, 360.0);
  return r < 0 ? r + 360.0 : r;
}
double clamp1(double x) { return x > 1 ? 1 : (x < -1 ? -1 : x); }
}  // namespace

// ---- great circle ----------------------------------------------------------------------------
double normalize_lon(double lon) {
  double r = std::fmod(lon + 180.0, 360.0);
  if (r < 0) r += 360.0;
  return r - 180.0;
}

double distance_m(double lat1, double lon1, double lat2, double lon2) {
  double p1 = rad(lat1), p2 = rad(lat2), dp = rad(lat2 - lat1), dl = rad(lon2 - lon1);
  double a = std::sin(dp / 2) * std::sin(dp / 2) + std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
  if (a > 1) a = 1;
  return 2.0 * kEarthRadiusM * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}

double initial_bearing_deg(double lat1, double lon1, double lat2, double lon2) {
  double p1 = rad(lat1), p2 = rad(lat2), dl = rad(lon2 - lon1);
  double y = std::sin(dl) * std::cos(p2);
  double x = std::cos(p1) * std::sin(p2) - std::sin(p1) * std::cos(p2) * std::cos(dl);
  return fmod360(deg(std::atan2(y, x)));
}

void destination(double lat, double lon, double brg, double dist, double* lat2, double* lon2) {
  double d = dist / kEarthRadiusM, t = rad(brg), p1 = rad(lat), l1 = rad(lon);
  double p2 = std::asin(clamp1(std::sin(p1) * std::cos(d) + std::cos(p1) * std::sin(d) * std::cos(t)));
  double l2 = l1 + std::atan2(std::sin(t) * std::sin(d) * std::cos(p1), std::cos(d) - std::sin(p1) * std::sin(p2));
  *lat2 = deg(p2);
  *lon2 = normalize_lon(deg(l2));
}

// ---- Maidenhead ------------------------------------------------------------------------------
std::string maidenhead_encode(double lat, double lon, int pairs) {
  if (!(lat >= -90 && lat <= 90) || !std::isfinite(lon) || pairs < 1 || pairs > 5) return "";
  double x = normalize_lon(lon) + 180.0;
  double y = lat + 90.0;
  if (y >= 180.0) y = 180.0 - 1e-9;
  if (x >= 360.0) x = 360.0 - 1e-9;
  static const double dlon[5] = {20.0, 2.0, 2.0 / 24, 2.0 / 240, 2.0 / 5760};
  static const double dlat[5] = {10.0, 1.0, 1.0 / 24, 1.0 / 240, 1.0 / 5760};
  static const int base[5] = {18, 10, 24, 10, 24};
  std::string s;
  for (int i = 0; i < pairs; ++i) {
    int a = static_cast<int>(std::floor(x / dlon[i] + 1e-9));
    int b = static_cast<int>(std::floor(y / dlat[i] + 1e-9));
    if (a >= base[i]) a = base[i] - 1;
    if (b >= base[i]) b = base[i] - 1;
    if (a < 0) a = 0;
    if (b < 0) b = 0;
    x -= a * dlon[i];
    y -= b * dlat[i];
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    char ca, cb;
    if (i == 0) { ca = static_cast<char>('A' + a); cb = static_cast<char>('A' + b); }
    else if (i % 2 == 1) { ca = static_cast<char>('0' + a); cb = static_cast<char>('0' + b); }
    else { ca = static_cast<char>('a' + a); cb = static_cast<char>('a' + b); }
    s += ca;
    s += cb;
  }
  return s;
}

bool maidenhead_decode(const std::string& in, double* lat, double* lon, double* hlat, double* hlon) {
  std::string s;
  for (char c : in)
    if (c != ' ') s += c;
  if (s.empty() || s.size() % 2 || s.size() > 10) return false;
  static const double dlon[5] = {20.0, 2.0, 2.0 / 24, 2.0 / 240, 2.0 / 5760};
  static const double dlat[5] = {10.0, 1.0, 1.0 / 24, 1.0 / 240, 1.0 / 5760};
  double x = 0, y = 0;
  int pairs = static_cast<int>(s.size() / 2);
  for (int i = 0; i < pairs; ++i) {
    char a = s[static_cast<size_t>(2 * i)], b = s[static_cast<size_t>(2 * i + 1)];
    int va, vb;
    if (i % 2 == 1) {
      if (a < '0' || a > '9' || b < '0' || b > '9') return false;
      va = a - '0';
      vb = b - '0';
    } else {
      if (a >= 'a' && a <= 'z') a = static_cast<char>(a - 32);
      if (b >= 'a' && b <= 'z') b = static_cast<char>(b - 32);
      int lim = i == 0 ? 18 : 24;
      va = a - 'A';
      vb = b - 'A';
      if (va < 0 || va >= lim || vb < 0 || vb >= lim) return false;
    }
    x += va * dlon[i];
    y += vb * dlat[i];
  }
  double hx = dlon[pairs - 1] / 2, hy = dlat[pairs - 1] / 2;
  *lon = x + hx - 180.0;
  *lat = y + hy - 90.0;
  if (hlat) *hlat = hy;
  if (hlon) *hlon = hx;
  return true;
}

// ---- UTM (Krüger series, WGS-84) ---------------------------------------------------------------
namespace {
constexpr double kA = 6378137.0;
constexpr double kF = 1.0 / 298.257223563;
constexpr double kK0 = 0.9996;
struct Kruger {
  double n, A, alpha[3], beta[3], delta[3];
  Kruger() {
    n = kF / (2 - kF);
    double n2 = n * n, n3 = n2 * n;
    A = kA / (1 + n) * (1 + n2 / 4 + n2 * n2 / 64);
    alpha[0] = n / 2 - 2 * n2 / 3 + 5 * n3 / 16;
    alpha[1] = 13 * n2 / 48 - 3 * n3 / 5;
    alpha[2] = 61 * n3 / 240;
    beta[0] = n / 2 - 2 * n2 / 3 + 37 * n3 / 96;
    beta[1] = n2 / 48 + n3 / 15;
    beta[2] = 17 * n3 / 480;
    delta[0] = 2 * n - 2 * n2 / 3 - 2 * n3;
    delta[1] = 7 * n2 / 3 - 8 * n3 / 5;
    delta[2] = 56 * n3 / 15;
  }
};
const char kBands[] = "CDEFGHJKLMNPQRSTUVWX";

char lat_band(double lat) {
  if (lat < -80 || lat > 84) return 0;
  int i = static_cast<int>(std::floor((lat + 80) / 8));
  if (i > 19) i = 19;  // X spans 72..84
  return kBands[i];
}
int utm_zone(double lat, double lon) {
  double L = normalize_lon(lon);
  int z = static_cast<int>(std::floor((L + 180) / 6)) + 1;
  if (z > 60) z = 60;
  if (lat >= 56 && lat < 64 && L >= 3 && L < 12) z = 32;
  if (lat >= 72 && lat <= 84) {
    if (L >= 0 && L < 9) z = 31;
    else if (L >= 9 && L < 21) z = 33;
    else if (L >= 21 && L < 33) z = 35;
    else if (L >= 33 && L < 42) z = 37;
  }
  return z;
}
}  // namespace

bool latlon_to_utm_zone(double lat, double lon, int zone, Utm* out) {
  if (!(lat >= -80.0 && lat <= 84.0) || zone < 1 || zone > 60 || !std::isfinite(lon)) return false;
  static const Kruger K;
  double lon0 = (zone - 1) * 6 - 180 + 3;
  double dl = rad(normalize_lon(lon - lon0));
  if (std::fabs(dl) > rad(30)) return false;  // far outside the zone: series diverges
  double phi = rad(lat);
  double c = 2 * std::sqrt(K.n) / (1 + K.n);
  double t = std::sinh(std::atanh(std::sin(phi)) - c * std::atanh(c * std::sin(phi)));
  double xi = std::atan2(t, std::cos(dl));
  double eta = std::atanh(std::sin(dl) / std::sqrt(1 + t * t));
  double E = eta, N = xi;
  for (int j = 1; j <= 3; ++j) {
    E += K.alpha[j - 1] * std::cos(2 * j * xi) * std::sinh(2 * j * eta);
    N += K.alpha[j - 1] * std::sin(2 * j * xi) * std::cosh(2 * j * eta);
  }
  out->zone = zone;
  out->band = lat_band(lat);
  out->north = lat >= 0;
  out->easting = 500000.0 + kK0 * K.A * E;
  out->northing = kK0 * K.A * N + (lat < 0 ? 10000000.0 : 0.0);
  return true;
}

bool latlon_to_utm(double lat, double lon, Utm* out) {
  if (!(lat >= -80.0 && lat <= 84.0) || !std::isfinite(lon)) return false;
  return latlon_to_utm_zone(lat, lon, utm_zone(lat, lon), out);
}

bool utm_to_latlon(int zone, bool north, double e, double n, double* lat, double* lon) {
  if (zone < 1 || zone > 60 || !std::isfinite(e) || !std::isfinite(n)) return false;
  if (e < 100000 || e > 900000 || n < 0 || n > 10000000) return false;
  static const Kruger K;
  double xi = (n - (north ? 0 : 10000000.0)) / (kK0 * K.A);
  double eta = (e - 500000.0) / (kK0 * K.A);
  double xp = xi, ep = eta;
  for (int j = 1; j <= 3; ++j) {
    xp -= K.beta[j - 1] * std::sin(2 * j * xi) * std::cosh(2 * j * eta);
    ep -= K.beta[j - 1] * std::cos(2 * j * xi) * std::sinh(2 * j * eta);
  }
  double chi = std::asin(clamp1(std::sin(xp) / std::cosh(ep)));
  double phi = chi;
  for (int j = 1; j <= 3; ++j) phi += K.delta[j - 1] * std::sin(2 * j * chi);
  double lon0 = (zone - 1) * 6 - 180 + 3;
  *lat = deg(phi);
  *lon = normalize_lon(lon0 + deg(std::atan2(std::sinh(ep), std::cos(xp))));
  return true;
}

namespace {
// Tokenise into alnum runs.
std::vector<std::string> tokens(const std::string& s) {
  std::vector<std::string> t;
  std::string cur;
  for (char c : s) {
    bool an = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '.';
    if (an) cur += c;
    else if (!cur.empty()) { t.push_back(cur); cur.clear(); }
  }
  if (!cur.empty()) t.push_back(cur);
  return t;
}
bool all_digits(const std::string& s, bool allow_dot = false) {
  if (s.empty()) return false;
  int dots = 0;
  for (char c : s) {
    if (c == '.' && allow_dot && ++dots == 1) continue;
    if (c < '0' || c > '9') return false;
  }
  return true;
}
char upc(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c; }
}  // namespace

bool parse_utm(const std::string& s, Utm* out) {
  std::vector<std::string> t = tokens(s);
  if (t.empty() || t.size() > 4 || s.size() > 100) return false;
  // Split a leading "33N"/"33T" token.
  std::string zs = t[0], hem;
  size_t i = 0;
  while (i < zs.size() && zs[i] >= '0' && zs[i] <= '9') ++i;
  if (i == 0 || i > 2) return false;
  hem = zs.substr(i);
  zs = zs.substr(0, i);
  size_t k = 1;
  if (hem.empty() && t.size() == 4) hem = t[k++];
  if (hem.size() != 1 || t.size() - k != 2) return false;
  if (!all_digits(t[k], true) || !all_digits(t[k + 1], true)) return false;
  int zone = std::atoi(zs.c_str());
  char h = upc(hem[0]);
  bool north;
  if (h == 'N') north = true;
  else if (h == 'S') north = false;  // ambiguity: "S" is treated as southern hemisphere, not band S
  else if (h >= 'C' && h <= 'X' && h != 'I' && h != 'O') north = h >= 'N';
  else return false;
  double e = std::atof(t[k].c_str()), n = std::atof(t[k + 1].c_str());
  if (zone < 1 || zone > 60) return false;
  out->zone = zone;
  out->north = north;
  out->easting = e;
  out->northing = n;
  out->band = (h == 'N' || h == 'S') ? 0 : h;
  return e >= 100000 && e <= 900000 && n >= 0 && n <= 10000000;
}

std::string utm_to_mgrs(const Utm& u, int digits) {
  if (digits < 1 || digits > 5 || u.zone < 1 || u.zone > 60 || !u.band) return "";
  static const char* cols[3] = {"STUVWXYZ", "ABCDEFGH", "JKLMNPQR"};  // index zone % 3
  static const char rows[] = "ABCDEFGHJKLMNPQRSTUV";
  int ci = static_cast<int>(std::floor(u.easting / 100000.0)) - 1;
  if (ci < 0 || ci > 7) return "";
  int ri = static_cast<int>(std::fmod(std::floor(u.northing / 100000.0), 20.0));
  if (u.zone % 2 == 0) ri = (ri + 5) % 20;
  double e = std::fmod(u.easting, 100000.0), n = std::fmod(u.northing, 100000.0);
  double div = std::pow(10.0, 5 - digits);
  long ee = static_cast<long>(std::floor(e / div + 1e-6)), nn = static_cast<long>(std::floor(n / div + 1e-6));
  long lim = static_cast<long>(std::pow(10.0, digits));
  if (ee >= lim) ee = lim - 1;
  if (nn >= lim) nn = lim - 1;
  char b[96];  // room for any int/long values (GCC -Wformat-truncation)
  std::snprintf(b, sizeof b, "%d%c %c%c %0*ld %0*ld", u.zone, u.band, cols[u.zone % 3][ci], rows[ri], digits, ee, digits, nn);
  return b;
}

bool latlon_to_mgrs(double lat, double lon, int digits, std::string* out) {
  Utm u;
  if (!latlon_to_utm(lat, lon, &u)) return false;
  *out = utm_to_mgrs(u, digits);
  return !out->empty();
}

bool mgrs_to_latlon(const std::string& in, double* lat, double* lon) {
  std::string s;
  for (char c : in) {
    if (c == ' ' || c == '\t') continue;
    s += upc(c);
  }
  if (s.size() < 5 || s.size() > 15 + 2) return false;
  size_t i = 0;
  while (i < s.size() && i < 2 && s[i] >= '0' && s[i] <= '9') ++i;
  if (i == 0) return false;
  int zone = std::atoi(s.substr(0, i).c_str());
  if (zone < 1 || zone > 60 || i + 3 > s.size()) return false;
  char band = s[i], c1 = s[i + 1], c2 = s[i + 2];
  const char* bp = nullptr;
  for (const char* p = kBands; *p; ++p)
    if (*p == band) bp = p;
  if (!bp) return false;
  std::string digs = s.substr(i + 3);
  if (digs.size() % 2 || digs.size() > 10 || (!digs.empty() && !all_digits(digs))) return false;
  int nd = static_cast<int>(digs.size() / 2);
  static const char* cols[3] = {"STUVWXYZ", "ABCDEFGH", "JKLMNPQR"};
  static const char rows[] = "ABCDEFGHJKLMNPQRSTUV";
  const char* cs = cols[zone % 3];
  int ci = -1, ri = -1;
  for (int k = 0; k < 8; ++k)
    if (cs[k] == c1) ci = k;
  for (int k = 0; k < 20; ++k)
    if (rows[k] == c2) ri = k;
  if (ci < 0 || ri < 0) return false;
  if (zone % 2 == 0) ri = (ri + 15) % 20;
  double scale = std::pow(10.0, 5 - nd);
  double e_in = nd ? std::atof(digs.substr(0, static_cast<size_t>(nd)).c_str()) * scale : 0;
  double n_in = nd ? std::atof(digs.substr(static_cast<size_t>(nd)).c_str()) * scale : 0;
  double half = nd ? scale / 2 : 50000.0;
  double e = (ci + 1) * 100000.0 + e_in + half;
  double n = ri * 100000.0 + n_in + half;
  // Latitude band → minimum northing, then add 2000 km cycles.
  int bi = static_cast<int>(bp - kBands);
  double band_lat = -80.0 + 8.0 * bi;
  bool north = band >= 'N';
  Utm ref;
  double lon0 = (zone - 1) * 6 - 180 + 3;
  if (!latlon_to_utm_zone(band_lat, lon0, zone, &ref)) return false;
  double min_n = ref.northing - 100000.0;
  if (min_n < 0) min_n = 0;
  int guard = 0;
  while (n < min_n && guard++ < 10) n += 2000000.0;
  if (n > 10000000.0) return false;
  if (!utm_to_latlon(zone, north, e, n, lat, lon)) return false;
  // Sanity: the result must fall in (or within 0.5 deg of) the stated band.
  double top = band == 'X' ? 84.0 : band_lat + 8.0;
  return *lat >= band_lat - 0.5 && *lat <= top + 0.5;
}

// ---- DMS ---------------------------------------------------------------------------------------
static std::string hemi(double v, bool is_lat) { return is_lat ? (v < 0 ? "S" : "N") : (v < 0 ? "W" : "E"); }

std::string to_dms(double v, bool is_lat, int sd) {
  double a = std::fabs(v);
  double scale = std::pow(10.0, sd);
  double total = std::round(a * 3600.0 * scale) / scale;
  int d = static_cast<int>(total / 3600.0);
  int m = static_cast<int>((total - d * 3600.0) / 60.0);
  double s = total - d * 3600.0 - m * 60.0;
  if (s < 0) s = 0;
  char b[64];
  std::snprintf(b, sizeof b, "%d\xC2\xB0%02d'%0*.*f\"%s", d, m, sd ? sd + 3 : 2, sd, s, hemi(v, is_lat).c_str());
  return b;
}

std::string to_dm(double v, bool is_lat, int md) {
  double a = std::fabs(v);
  double scale = std::pow(10.0, md);
  double total = std::round(a * 60.0 * scale) / scale;
  int d = static_cast<int>(total / 60.0);
  double m = total - d * 60.0;
  if (m < 0) m = 0;
  char b[64];
  std::snprintf(b, sizeof b, "%d\xC2\xB0%0*.*f'%s", d, md ? md + 3 : 2, md, m, hemi(v, is_lat).c_str());
  return b;
}

bool parse_coordinate(const std::string& s, bool is_lat, double* out) {
  if (s.empty() || s.size() > 64) return false;
  std::vector<std::string> nums;
  int sign = 1, hemis = 0;
  bool neg_seen = false;
  std::string cur;
  auto flush = [&]() { if (!cur.empty()) { nums.push_back(cur); cur.clear(); } };
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if ((c >= '0' && c <= '9') || c == '.') { cur += c; continue; }
    bool prev_digit = i > 0 && ((s[i - 1] >= '0' && s[i - 1] <= '9') || s[i - 1] == '.');
    flush();
    if (c == '-') {
      if (neg_seen || !nums.empty()) return false;
      neg_seen = true;
      sign = -1;
    } else if (c == '+') {
      if (!nums.empty()) return false;
    } else if ((c == 'd' || c == 'm' || c == 's') && prev_digit) {
      // unit markers
    } else if (c == 'N' || c == 'n' || c == 'S' || c == 'E' || c == 'e' || c == 'W' || c == 'w' || c == 's') {
      char h = upc(c);
      bool lat_h = h == 'N' || h == 'S';
      if (lat_h != is_lat) return false;
      if (++hemis > 1) return false;
      if (h == 'S' || h == 'W') sign = -sign;
    } else if (c == ' ' || c == ',' || c == '\'' || c == '"' || c == ':' || static_cast<unsigned char>(c) >= 0x80) {
      // separators, including UTF-8 degree/prime symbols
    } else {
      return false;
    }
  }
  flush();
  if (nums.empty() || nums.size() > 3) return false;
  if (hemis && neg_seen) return false;
  double parts[3] = {0, 0, 0};
  for (size_t i = 0; i < nums.size(); ++i) {
    const std::string& n = nums[i];
    int dots = 0;
    for (char c : n) dots += c == '.';
    if (dots > 1 || n == ".") return false;
    if (dots && i + 1 < nums.size()) return false;  // only the last component may be fractional
    parts[i] = std::atof(n.c_str());
  }
  if (parts[1] >= 60 || parts[2] >= 60) return false;
  double v = parts[0] + parts[1] / 60.0 + parts[2] / 3600.0;
  if (v > (is_lat ? 90.0 : 180.0)) return false;
  *out = sign * v;
  return true;
}

// ---- dates -------------------------------------------------------------------------------------
static bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

bool parse_date(const std::string& s, int* y, int* m, int* d) {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
  for (size_t i : {0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u})
    if (s[i] < '0' || s[i] > '9') return false;
  int yy = std::atoi(s.substr(0, 4).c_str()), mm = std::atoi(s.substr(5, 2).c_str()), dd = std::atoi(s.substr(8, 2).c_str());
  static const int dim[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (yy < 1900 || yy > 2200 || mm < 1 || mm > 12) return false;
  int maxd = dim[mm - 1] + (mm == 2 && leap(yy) ? 1 : 0);
  if (dd < 1 || dd > maxd) return false;
  *y = yy;
  *m = mm;
  *d = dd;
  return true;
}

double julian_day(int y, int m, int d, double hour) {
  if (m <= 2) { y -= 1; m += 12; }
  int A = y / 100;
  int B = 2 - A + A / 4;
  return std::floor(365.25 * (y + 4716)) + std::floor(30.6001 * (m + 1)) + d + B - 1524.5 + hour / 24.0;
}

int day_of_week(int y, int m, int d) {
  long jd = static_cast<long>(std::floor(julian_day(y, m, d) + 0.5));
  return static_cast<int>((jd + 1) % 7);
}

// ---- sun -------------------------------------------------------------------------------------
namespace {
struct SunCalc {
  double decl;    // degrees
  double eqtime;  // minutes
};
SunCalc sun_at(double jd) {
  double t = (jd - 2451545.0) / 36525.0;
  double L0 = fmod360(280.46646 + t * (36000.76983 + t * 0.0003032));
  double M = 357.52911 + t * (35999.05029 - 0.0001537 * t);
  double e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
  double C = std::sin(rad(M)) * (1.914602 - t * (0.004817 + 0.000014 * t)) + std::sin(rad(2 * M)) * (0.019993 - 0.000101 * t) +
             std::sin(rad(3 * M)) * 0.000289;
  double true_long = L0 + C;
  double omega = 125.04 - 1934.136 * t;
  double app_long = true_long - 0.00569 - 0.00478 * std::sin(rad(omega));
  double obl0 = 23.0 + (26.0 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60.0) / 60.0;
  double obl = obl0 + 0.00256 * std::cos(rad(omega));
  double decl = deg(std::asin(std::sin(rad(obl)) * std::sin(rad(app_long))));
  double y = std::tan(rad(obl / 2));
  y *= y;
  double eq = y * std::sin(2 * rad(L0)) - 2 * e * std::sin(rad(M)) + 4 * e * y * std::sin(rad(M)) * std::cos(2 * rad(L0)) -
              0.5 * y * y * std::sin(4 * rad(L0)) - 1.25 * e * e * std::sin(2 * rad(M));
  return {decl, 4.0 * deg(eq)};
}
// Hour angle (degrees) for the sun at a given zenith; NaN if never reached.
double hour_angle(double lat, double decl, double zenith) {
  double c = std::cos(rad(zenith)) / (std::cos(rad(lat)) * std::cos(rad(decl))) - std::tan(rad(lat)) * std::tan(rad(decl));
  if (c > 1 || c < -1) return std::nan("");
  return deg(std::acos(c));
}
// Iteratively solve an event time (local minutes) for a given zenith; sign -1 = morning, +1 = evening.
double event_min(double jd0, double lat, double lon, double tz, double zenith, int sign, bool* never) {
  double t_min = 720.0 + sign * 360.0;  // first guess 06:00 / 18:00 local
  double result = std::nan("");
  for (int it = 0; it < 3; ++it) {
    double jd = jd0 + (t_min - tz * 60.0) / 1440.0;
    SunCalc s = sun_at(jd);
    double ha = hour_angle(lat, s.decl, zenith);
    if (std::isnan(ha)) { *never = true; return std::nan(""); }
    double noon = 720.0 - 4.0 * lon - s.eqtime + tz * 60.0;
    result = noon + sign * 4.0 * ha;
    t_min = result;
  }
  *never = false;
  return result;
}
}  // namespace

SunEvents sun_events(int y, int m, int d, double lat, double lon, double tz) {
  SunEvents ev;
  double jd0 = julian_day(y, m, d, 0);  // 0h UTC of the date
  // Solar noon (iterate once on its own time).
  double noon = 720.0 - 4.0 * lon + tz * 60.0;
  for (int it = 0; it < 2; ++it) {
    SunCalc s = sun_at(jd0 + (noon - tz * 60.0) / 1440.0);
    noon = 720.0 - 4.0 * lon - s.eqtime + tz * 60.0;
  }
  ev.noon_min = noon;
  bool nr = false, ns = false;
  ev.sunrise_min = event_min(jd0, lat, lon, tz, 90.833, -1, &nr);
  ev.sunset_min = event_min(jd0, lat, lon, tz, 90.833, +1, &ns);
  if (nr || ns) {
    SunCalc s = sun_at(jd0 + (noon - tz * 60.0) / 1440.0);
    // Sun elevation at noon decides between polar day and night.
    double elev_noon = 90.0 - std::fabs(lat - s.decl);
    if (elev_noon > 0) ev.always_up = true;
    else ev.always_down = true;
    ev.day_length_min = ev.always_up ? 1440 : 0;
    ev.sunrise_min = ev.sunset_min = std::nan("");
  } else {
    ev.day_length_min = ev.sunset_min - ev.sunrise_min;
  }
  bool n1, n2;
  ev.civil_dawn_min = event_min(jd0, lat, lon, tz, 96.0, -1, &n1);
  ev.civil_dusk_min = event_min(jd0, lat, lon, tz, 96.0, +1, &n2);
  return ev;
}

void sun_position(int y, int m, int d, double local_min, double lat, double lon, double tz, double* az, double* el) {
  double jd = julian_day(y, m, d, 0) + (local_min - tz * 60.0) / 1440.0;
  SunCalc s = sun_at(jd);
  double tst = std::fmod(local_min + s.eqtime + 4.0 * lon - 60.0 * tz, 1440.0);
  if (tst < 0) tst += 1440.0;
  double ha = tst / 4.0 < 0 ? tst / 4.0 + 180.0 : tst / 4.0 - 180.0;
  double cz = std::sin(rad(lat)) * std::sin(rad(s.decl)) + std::cos(rad(lat)) * std::cos(rad(s.decl)) * std::cos(rad(ha));
  double zen = deg(std::acos(clamp1(cz)));
  double elev = 90.0 - zen;
  // Refraction (NOAA approximation), degrees.
  double r = 0;
  if (elev <= 85.0) {
    double te = std::tan(rad(elev));
    if (elev > 5.0) r = 58.1 / te - 0.07 / (te * te * te) + 0.000086 / (te * te * te * te * te);
    else if (elev > -0.575) r = 1735.0 + elev * (-518.2 + elev * (103.4 + elev * (-12.79 + elev * 0.711)));
    else r = -20.774 / te;
    r /= 3600.0;
  }
  *el = elev + r;
  double denom = std::cos(rad(lat)) * std::sin(rad(zen));
  double a;
  if (std::fabs(denom) > 1e-9) {
    double ca = (std::sin(rad(lat)) * std::cos(rad(zen)) - std::sin(rad(s.decl))) / denom;
    a = deg(std::acos(clamp1(ca)));
    a = ha > 0 ? fmod360(a + 180.0) : fmod360(540.0 - a);
  } else {
    a = lat > 0 ? 180.0 : 0.0;
  }
  *az = a;
}

std::string hhmm(double minutes) {
  if (!std::isfinite(minutes)) return "--:--";
  long t = std::lround(minutes);
  t %= 1440;
  if (t < 0) t += 1440;
  char b[8];
  std::snprintf(b, sizeof b, "%02ld:%02ld", t / 60, t % 60);
  return b;
}

// ---- moon -------------------------------------------------------------------------------------
MoonPhase moon_phase(double jd) {
  const double synodic = 29.530588853;
  const double ref_new = 2451550.25972;  // 2000-01-06 18:14 UTC new moon
  double age = std::fmod(jd - ref_new, synodic);
  if (age < 0) age += synodic;
  MoonPhase p;
  p.age_days = age;
  p.illumination = (1.0 - std::cos(2.0 * kPi * age / synodic)) / 2.0;
  double f = age / synodic;
  static const char* names[8] = {"New moon", "Waxing crescent", "First quarter", "Waxing gibbous",
                                 "Full moon", "Waning gibbous", "Last quarter", "Waning crescent"};
  int idx = static_cast<int>(std::floor(f * 8.0 + 0.5)) % 8;
  p.name = names[idx];
  double half = synodic / 2.0;
  p.days_to_full = age <= half ? half - age : synodic - age + half;
  p.days_to_new = synodic - age;
  return p;
}

}  // namespace nav
}  // namespace ember
