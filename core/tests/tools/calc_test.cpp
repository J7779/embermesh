// Hand-verified reference values for navigation, signal, crypto, power and survival tools.
// Sun and UTM/MGRS references were cross-checked with astral 3.x and the Python utm/mgrs packages.
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "doctest.h"
#include "tools/morse.h"
#include "tools/nav.h"
#include "tools/otp.h"
#include "tools/tool.h"

using namespace ember;
using namespace ember::tools;

namespace {
double outnum(const Result& r, const std::string& key) {
  const Output* o = r.find(key);
  if (!o) return std::nan("");
  return std::atof(o->value.c_str());
}
std::string outstr(const Result& r, const std::string& key) {
  const Output* o = r.find(key);
  return o ? o->value : "<missing>";
}
double hhmm_min(const std::string& s) { return std::atoi(s.substr(0, 2).c_str()) * 60 + std::atoi(s.substr(3, 2).c_str()); }
}  // namespace

TEST_SUITE("tools") {

// ---------------- navigation ----------------
TEST_CASE("nav: great circle distance and bearing") {
  // London → Paris ≈ 343.5 km (spherical), initial bearing ≈ 148°.
  double d = nav::distance_m(51.5074, -0.1278, 48.8566, 2.3522);
  CHECK(d / 1000 == doctest::Approx(343.5).epsilon(3e-3));
  CHECK(nav::initial_bearing_deg(51.5074, -0.1278, 48.8566, 2.3522) == doctest::Approx(148.1).epsilon(3e-3));
  // Quarter of the equator.
  CHECK(nav::distance_m(0, 0, 0, 90) == doctest::Approx(nav::kEarthRadiusM * 3.14159265358979 / 2));
  CHECK(nav::initial_bearing_deg(0, 0, 0, 90) == doctest::Approx(90));
  CHECK(nav::initial_bearing_deg(0, 0, 10, 0) == doctest::Approx(0));
  // Antimeridian: 179.9 E → 179.9 W is ~22 km, heading east.
  CHECK(nav::distance_m(0, 179.9, 0, -179.9) == doctest::Approx(22239).epsilon(1e-3));
  CHECK(nav::initial_bearing_deg(0, 179.9, 0, -179.9) == doctest::Approx(90));
  // Poles.
  CHECK(nav::distance_m(90, 0, -90, 0) == doctest::Approx(nav::kEarthRadiusM * 3.14159265358979));
  CHECK(nav::distance_m(90, 0, 90, 123) == doctest::Approx(0).epsilon(1e-6));
  CHECK(std::isfinite(nav::initial_bearing_deg(90, 0, 0, 0)));
}

TEST_CASE("nav: destination point round trips and wraps longitude") {
  double la, lo;
  nav::destination(0, 179.95, 90, 11119.5, &la, &lo);  // 0.1° east across the antimeridian
  CHECK(la == doctest::Approx(0).epsilon(1e-6));
  CHECK(lo == doctest::Approx(-179.95).epsilon(1e-5));
  nav::destination(40.7128, -74.0060, 45, 100000, &la, &lo);
  CHECK(nav::distance_m(40.7128, -74.0060, la, lo) == doctest::Approx(100000).epsilon(1e-6));
  CHECK(nav::initial_bearing_deg(40.7128, -74.0060, la, lo) == doctest::Approx(45).epsilon(1e-6));
  nav::destination(89.9, 0, 0, 50000, &la, &lo);  // over the pole
  CHECK(la < 90);
  CHECK(la > 89.4);
  CHECK(nav::normalize_lon(540) == doctest::Approx(-180));
  CHECK(nav::normalize_lon(-190) == doctest::Approx(170));
}

TEST_CASE("nav: Maidenhead known locators") {
  CHECK(nav::maidenhead_encode(41.714775, -72.727260, 3) == "FN31pr");  // W1AW, Newington CT
  CHECK(nav::maidenhead_encode(48.14666, 11.60833, 3) == "JN58td");     // Munich
  CHECK(nav::maidenhead_encode(-34.91, -56.21166, 3) == "GF15vc");      // Montevideo
  CHECK(nav::maidenhead_encode(38.92, -77.065, 2) == "FM18");
  CHECK(nav::maidenhead_encode(0, 0, 1) == "JJ");
  CHECK(nav::maidenhead_encode(90, 179.99999, 3) == "RR99xx");  // corner clamps
  CHECK(nav::maidenhead_encode(0, 180, 1) == "AJ");  // 180 E == 180 W
  CHECK(nav::maidenhead_encode(-90, -180, 3) == "AA00aa");
  CHECK(nav::maidenhead_encode(91, 0, 3) == "");
  CHECK(nav::maidenhead_encode(41.714775, -72.727260, 5).substr(0, 6) == "FN31pr");
  double la, lo, hla, hlo;
  REQUIRE(nav::maidenhead_decode("fn31PR", &la, &lo, &hla, &hlo));
  CHECK(la == doctest::Approx(41.72917).epsilon(1e-5));
  CHECK(lo == doctest::Approx(-72.70833).epsilon(1e-5));
  CHECK(hla == doctest::Approx(1.0 / 48));
  CHECK(nav::maidenhead_decode("JN58", &la, &lo));
  CHECK(la == doctest::Approx(48.5));
  CHECK(lo == doctest::Approx(11.0));
  for (const char* bad : {"", "F", "SN31", "FN3", "FNAA", "FN31zz", "FN31pr1", "FN31pr12zz", "FN31pr12xx99"}) {
    CAPTURE(bad);
    CHECK_FALSE(nav::maidenhead_decode(bad, &la, &lo));
  }
  // Round trip at 10 chars is within the square.
  for (double lat = -89.5; lat < 90; lat += 17.3)
    for (double lon = -179.5; lon < 180; lon += 23.7) {
      std::string l = nav::maidenhead_encode(lat, lon, 5);
      REQUIRE(nav::maidenhead_decode(l, &la, &lo, &hla, &hlo));
      CHECK(std::fabs(la - lat) <= hla + 1e-9);
      CHECK(std::fabs(lo - lon) <= hlo + 1e-9);
    }
}

TEST_CASE("nav: UTM and MGRS reference points") {
  struct V { double lat, lon; int zone; char band; double e, n; const char* mgrs; };
  const V vs[] = {
      {38.8895, -77.0352, 18, 'S', 323486.737, 4306483.048, "18S UJ 23486 06483"},
      {51.4769, -0.0005, 30, 'U', 708287.478, 5707127.328, "30U YC 08287 07127"},
      {-33.8568, 151.2153, 56, 'H', 334900.570, 6252288.753, "56H LH 34900 52288"},
      {60.0, 5.0, 32, 'V', 276979.926, 6658157.203, "32V KM 76979 58157"},       // Norway exception
      {78.22, 15.65, 33, 'X', 514813.527, 8683004.154, "33X WG 14813 83004"},     // Svalbard exception
      {-79.9, -179.9, 1, 'C', 443247.872, 1128161.372, "1C DM 43247 28161"},
      {83.9, 179.9, 60, 'X', 534390.831, 9317795.754, "60X WU 34390 17795"},
      {64.1466, -21.9426, 27, 'W', 454138.377, 7113689.870, "27W VM 54138 13689"},
      {-34.6037, -58.3816, 21, 'H', 373317.502, 6170036.171, "21H UB 73317 70036"},
  };
  for (const auto& v : vs) {
    CAPTURE(v.mgrs);
    nav::Utm u;
    REQUIRE(nav::latlon_to_utm(v.lat, v.lon, &u));
    CHECK(u.zone == v.zone);
    CHECK(u.band == v.band);
    CHECK(u.easting == doctest::Approx(v.e).epsilon(1e-8));
    CHECK(std::fabs(u.northing - v.n) < 0.01);
    CHECK(nav::utm_to_mgrs(u, 5) == v.mgrs);
    double la, lo;
    REQUIRE(nav::utm_to_latlon(u.zone, u.north, u.easting, u.northing, &la, &lo));
    CHECK(la == doctest::Approx(v.lat).epsilon(1e-9));
    CHECK(lo == doctest::Approx(v.lon).epsilon(1e-9));
    REQUIRE(nav::mgrs_to_latlon(v.mgrs, &la, &lo));
    CHECK(nav::distance_m(la, lo, v.lat, v.lon) < 2.0);  // 1 m square centre
  }
  // Equator point (0,0) at the zone edge: northing 0 / row letter A.
  std::string m;
  REQUIRE(nav::latlon_to_mgrs(0, 0, 5, &m));
  CHECK(m == "31N AA 66021 00000");
  // Lower precision truncates.
  REQUIRE(nav::latlon_to_mgrs(38.8895, -77.0352, 2, &m));
  CHECK(m == "18S UJ 23 06");
  double la, lo;
  REQUIRE(nav::mgrs_to_latlon("18SUJ2306", &la, &lo));
  CHECK(nav::distance_m(la, lo, 38.8895, -77.0352) < 1500);
  // Polar regions are UPS: refused.
  nav::Utm u;
  CHECK_FALSE(nav::latlon_to_utm(85, 0, &u));
  CHECK_FALSE(nav::latlon_to_utm(-80.5, 0, &u));
  for (const char* bad : {"", "18", "18S", "61S UJ 1 1", "18I UJ 1 1", "18S IJ 1 1", "18S UJ 123 45", "18S UJ 12a45", "18S UJ 1234567890123"})
    CHECK_FALSE(nav::mgrs_to_latlon(bad, &la, &lo));
  // UTM parsing.
  REQUIRE(nav::parse_utm("18N 323487 4306483", &u));
  CHECK(u.zone == 18);
  CHECK(u.north);
  REQUIRE(nav::parse_utm("56 S 334900 6252289", &u));
  CHECK_FALSE(u.north);
  REQUIRE(nav::parse_utm("33T 500000 4649776", &u));
  CHECK(u.north);
  CHECK_FALSE(nav::parse_utm("0N 500000 1", &u));
  CHECK_FALSE(nav::parse_utm("18N 50 4306483", &u));
  CHECK_FALSE(nav::parse_utm("18Q", &u));
  // Dense round trip.
  for (double lat = -79.5; lat <= 83.5; lat += 6.1)
    for (double lon = -179.7; lon < 180; lon += 13.9) {
      REQUIRE(nav::latlon_to_utm(lat, lon, &u));
      REQUIRE(nav::utm_to_latlon(u.zone, u.north, u.easting, u.northing, &la, &lo));
      CHECK(nav::distance_m(la, lo, lat, lon) < 0.01);
      std::string g = nav::utm_to_mgrs(u, 5);
      REQUIRE(nav::mgrs_to_latlon(g, &la, &lo));
      CHECK(nav::distance_m(la, lo, lat, lon) < 1.5);
    }
}

TEST_CASE("nav: DMS parse and format") {
  double v;
  CHECK(nav::parse_coordinate("40 26 46 N", true, &v));
  CHECK(v == doctest::Approx(40.446111).epsilon(1e-7));
  CHECK(nav::parse_coordinate("40\xC2\xB0" "26'46\"S", true, &v));
  CHECK(v == doctest::Approx(-40.446111).epsilon(1e-7));
  CHECK(nav::parse_coordinate("79d58m56s W", false, &v));
  CHECK(v == doctest::Approx(-79.982222).epsilon(1e-7));
  CHECK(nav::parse_coordinate("-33.8688", true, &v));
  CHECK(v == doctest::Approx(-33.8688));
  CHECK(nav::parse_coordinate("E 151 12.558", false, &v));
  CHECK(v == doctest::Approx(151.2093));
  CHECK(nav::parse_coordinate("180", false, &v));
  for (const char* bad : {"", "abc", "91", "-181", "40 61 0 N", "40 26 60", "200 E", "-40 S", "1.5 30", "40 26 46 12", "N S 40", "1..2"}) {
    CAPTURE(bad);
    bool lat = std::string(bad).find('E') == std::string::npos;
    CHECK_FALSE(nav::parse_coordinate(bad, lat, &v));
  }
  CHECK(nav::to_dms(40.446111, true) == "40\xC2\xB0" "26'46.0\"N");
  CHECK(nav::to_dms(-79.982222, false, 0) == "79\xC2\xB0" "58'56\"W");
  CHECK(nav::to_dm(-33.8688, true) == "33\xC2\xB0" "52.128'S");
  CHECK(nav::to_dms(59.9999999, true, 1) == "60\xC2\xB0" "00'00.0\"N");  // carry
}

TEST_CASE("nav: dates") {
  int y, m, d;
  CHECK(nav::parse_date("2024-02-29", &y, &m, &d));
  CHECK_FALSE(nav::parse_date("2023-02-29", &y, &m, &d));
  CHECK_FALSE(nav::parse_date("2023-13-01", &y, &m, &d));
  CHECK_FALSE(nav::parse_date("2023-1-01", &y, &m, &d));
  CHECK_FALSE(nav::parse_date("1800-01-01", &y, &m, &d));
  CHECK(nav::julian_day(2000, 1, 1, 12) == doctest::Approx(2451545.0));
  CHECK(nav::day_of_week(2026, 11, 13) == 5);  // Friday
}

TEST_CASE("nav: sunrise/sunset against astral references (within 2 minutes)") {
  struct V { double lat, lon; const char* date; double tz; const char *rise, *noon, *set; double az15, el15; };
  const V vs[] = {
      {51.5074, -0.1278, "2026-06-21", 1, "04:43", "13:02", "21:21", 230.01, 53.99},
      {40.7128, -74.0060, "2026-12-21", -5, "07:17", "11:54", "16:32", 222.96, 12.77},
      {-33.8688, 151.2093, "2026-03-20", 11, "06:58", "13:03", "19:07", 314.62, 46.53},
      {-0.1807, -78.4678, "2026-09-23", -5, "06:03", "12:06", "18:09", 269.72, 46.55},
      {61.2181, -149.9003, "2026-01-15", -9, "09:57", "13:09", "16:22", 205.83, 5.0},
      {35.6762, 139.6503, "2026-11-13", 9, "06:15", "11:26", "16:36", 232.86, 16.23},
  };
  for (const auto& v : vs) {
    CAPTURE(v.date);
    int y, m, d;
    REQUIRE(nav::parse_date(v.date, &y, &m, &d));
    nav::SunEvents e = nav::sun_events(y, m, d, v.lat, v.lon, v.tz);
    REQUIRE_FALSE(e.always_up);
    REQUIRE_FALSE(e.always_down);
    CHECK(std::fabs(e.sunrise_min - hhmm_min(v.rise)) <= 2.0);
    CHECK(std::fabs(e.noon_min - hhmm_min(v.noon)) <= 1.5);
    CHECK(std::fabs(e.sunset_min - hhmm_min(v.set)) <= 2.0);
    double az, el;
    nav::sun_position(y, m, d, 15 * 60, v.lat, v.lon, v.tz, &az, &el);
    CHECK(az == doctest::Approx(v.az15).epsilon(0.004));
    CHECK(std::fabs(el - v.el15) < 0.4);  // reference without refraction
  }
  // Polar day / night at Tromsø.
  nav::SunEvents s = nav::sun_events(2026, 6, 21, 69.65, 18.96, 2);
  CHECK(s.always_up);
  CHECK(std::isnan(s.sunrise_min));
  s = nav::sun_events(2026, 12, 21, 69.65, 18.96, 1);
  CHECK(s.always_down);
  CHECK(s.day_length_min == 0);
  // Poles do not produce NaN in positions.
  double az, el;
  nav::sun_position(2026, 3, 20, 720, 90, 0, 0, &az, &el);
  CHECK(std::isfinite(az));
  CHECK(std::isfinite(el));
  CHECK(nav::hhmm(-30) == "23:30");
  CHECK(nav::hhmm(std::nan("")) == "--:--");
}

TEST_CASE("nav: moon phase") {
  // New moon 2024-04-08 18:21 UTC (total solar eclipse); full moon 2024-04-23 23:49 UTC.
  nav::MoonPhase p = nav::moon_phase(nav::julian_day(2024, 4, 8, 18.35));
  CHECK((p.age_days < 0.75 || p.age_days > 29.53 - 0.75));
  CHECK(p.illumination < 0.02);
  p = nav::moon_phase(nav::julian_day(2024, 4, 23, 23.8));
  CHECK(std::fabs(p.age_days - 14.77) < 0.75);
  CHECK(p.name == "Full moon");
  CHECK(p.illumination > 0.98);
}

TEST_CASE("nav tools: end-to-end through the registry") {
  Result r = run_tool("maidenhead", {{"lat", "41 42 53.2 N"}, {"lon", "72 43 38.1 W"}});
  CHECK(outstr(r, "locator") == "FN31pr");
  r = run_tool("maidenhead", {{"mode", "from_locator"}, {"locator", "JN58td"}});
  CHECK(outnum(r, "lat") == doctest::Approx(48.146).epsilon(1e-3));
  r = run_tool("maidenhead", {{"mode", "from_locator"}, {"locator", "ZZ99"}});
  CHECK_FALSE(r.ok());
  r = run_tool("utm_mgrs", {{"mode", "mgrs"}});
  REQUIRE(r.ok());
  CHECK(outnum(r, "lat") == doctest::Approx(38.8895).epsilon(1e-4));
  r = run_tool("utm_mgrs", {{"mode", "utm"}});
  REQUIRE(r.ok());
  CHECK(outstr(r, "mgrs") == "18S UJ 23487 06483");
  r = run_tool("utm_mgrs", {{"lat", "89"}});
  CHECK(r.ok());  // lat/lon shown, UPS note
  CHECK(r.find("mgrs") == nullptr);
  r = run_tool("distance_bearing", {{"lat1", "95"}});
  CHECK_FALSE(r.ok());
  r = run_tool("sun", {{"date", "2026-02-30"}});
  CHECK_FALSE(r.ok());
  r = run_tool("sun", {{"time", "25:00"}});
  CHECK_FALSE(r.ok());
  r = run_tool("sun", {{"lat", "78.2"}, {"lon", "15.6"}, {"date", "2026-06-21"}});
  REQUIRE(r.ok());
  CHECK(outstr(r, "sunrise").find("midnight sun") != std::string::npos);
  r = run_tool("pace_count", {{"distance_m", "1000"}, {"paces_per_100m", "65"}});
  CHECK(outnum(r, "paces") == doctest::Approx(650));
  r = run_tool("map_scale", {{"map_cm", "4"}, {"scale", "25000"}});
  CHECK(outstr(r, "ground").rfind("1.00 km", 0) == 0);
}

// ---------------- signal ----------------
TEST_CASE("morse: encode, decode, timing") {
  CHECK(morse::encode("SOS") == "... --- ...");
  CHECK(morse::encode("sos help") == "... --- ... / .... . .-.. .--.");
  CHECK(morse::encode("<SOS>") == "...---...");
  CHECK(morse::encode("CQ <SK>") == "-.-. --.- / ...-.-");
  std::string skipped;
  CHECK(morse::encode("A~B", &skipped) == ".- -...");
  CHECK(skipped == "~");
  CHECK(morse::encode("") == "");
  CHECK(morse::decode("... --- ...") == "SOS");
  CHECK(morse::decode(".... . .-.. .-.. --- / .-- --- .-. .-.. -..") == "HELLO WORLD");
  CHECK(morse::decode(".-   -...") == "A B");
  CHECK(morse::decode("........") == "?");
  CHECK(morse::decode("") == "");
  CHECK(morse::decode(morse::encode("THE QUICK BROWN FOX 0123456789")) == "THE QUICK BROWN FOX 0123456789");
  // 20 WPM: dot = 60 ms.
  CHECK(morse::timing("E", 20) == std::vector<int>{60});
  CHECK(morse::timing("EE", 20) == std::vector<int>{60, 180, 60});
  CHECK(morse::timing("E E", 20) == std::vector<int>{60, 420, 60});
  CHECK(morse::timing("A", 20) == std::vector<int>{60, 60, 180});
  CHECK(morse::total_ms(morse::timing("PARIS", 20)) + 420 == 3000);  // 50 units = 1 word per 3 s
  // Farnsworth stretches only the gaps.
  auto f = morse::timing("EE", 20, 10);
  REQUIRE(f.size() == 3);
  CHECK(f[0] == 60);
  CHECK(f[1] > 180);
  CHECK(morse::timing("SOS", 0).empty());
  CHECK(morse::nato("Ab 9") == "Alfa Bravo | Niner");
}

TEST_CASE("signal tools: tables") {
  Result r = run_tool("ctcss_dcs", {{"table", "ctcss"}});
  CHECK(outnum(r, "count") == 50);
  CHECK(r.text.find("100.0 Hz") != std::string::npos);
  r = run_tool("ctcss_dcs", {{"table", "frs"}});
  CHECK(outnum(r, "count") == 38);
  CHECK(r.text.find("38: 250.3 Hz") != std::string::npos);
  r = run_tool("ctcss_dcs", {{"table", "dcs"}});
  CHECK(outnum(r, "count") == 104);
  CHECK(r.text.find("D023N") != std::string::npos);
  CHECK(r.text.find("D754N") != std::string::npos);
  r = run_tool("morse", {{"text", "SOS"}, {"wpm", "12"}});
  CHECK(outstr(r, "pattern_ms").rfind("100,100,100,100,100,300", 0) == 0);
  r = run_tool("morse", {{"mode", "decode"}, {"text", "-- . ... ...."}});
  CHECK(outstr(r, "text") == "MESH");
  r = run_tool("qcodes", {{"filter", "qth"}});
  CHECK(outnum(r, "matches") == 1);
  r = run_tool("rst", {{"r", "5"}, {"s", "9"}, {"t", "9"}});
  CHECK(outstr(r, "report") == "599");
  CHECK_FALSE(run_tool("rst", {{"r", "6"}}).ok());
}

// ---------------- crypto ----------------
TEST_CASE("otp: classic example and round trip") {
  std::string c, p;
  REQUIRE(otp::encrypt("HELLO", "XMCKL", &c));
  CHECK(c == "EQNVZ");
  REQUIRE(otp::decrypt("EQNVZ", "XMCKL", &p));
  CHECK(p == "HELLO");
  CHECK_FALSE(otp::encrypt("HELLO", "XMCK", &c));
  CHECK(otp::normalize_plaintext("Meet at 7, ok?") == "MEETATSEVENOK");
  CHECK(otp::letters_only("01 abcde FGHIJ") == "ABCDEFGHIJ");
  CHECK(otp::groups("ABCDEFGHIJKL", 5) == "ABCDE FGHIJ KL");
  CHECK(otp::groups("ABCDEFGHIJKL", 5, 2) == "ABCDE FGHIJ\nKL");
  std::string pad;
  size_t used;
  uint8_t rnd[] = {0, 25, 26, 233, 234, 255, 1};
  REQUIRE(otp::pad_letters(ByteView(rnd, sizeof rnd), 5, &pad, &used));
  CHECK(pad == "AZAZB");
  CHECK(used == 7);
  CHECK_FALSE(otp::pad_letters(ByteView(rnd, sizeof rnd), 6, &pad));
}

TEST_CASE("otp tools: sheet, encode, decode") {
  std::string hex;
  for (int i = 0; i < 300; ++i) {
    char b[3];
    std::snprintf(b, sizeof b, "%02x", (i * 151 + 7) & 0xff);
    hex += b;
  }
  Result r = run_tool("otp_sheet", {{"random_hex", hex}, {"groups", "20"}});
  REQUIRE(r.ok());
  std::string pad = outstr(r, "pad");
  CHECK(pad.size() == 100);
  CHECK(r.text.find("ONE-TIME PAD") != std::string::npos);
  CHECK_FALSE(run_tool("otp_sheet", {{"random_hex", "zz"}}).ok());
  CHECK_FALSE(run_tool("otp_sheet", {{"random_hex", "00ff"}, {"groups", "5"}}).ok());
  r = run_tool("otp_encode", {{"plaintext", "Need water at grid 12"}, {"pad", pad}});
  REQUIRE(r.ok());
  std::string ct = outstr(r, "ciphertext");
  r = run_tool("otp_decode", {{"ciphertext", ct}, {"pad", pad}});
  REQUIRE(r.ok());
  CHECK(outstr(r, "plaintext") == "NEEDWATERATGRIDONETWO");
  CHECK_FALSE(run_tool("otp_encode", {{"plaintext", "!!!"}, {"pad", pad}}).ok());
  CHECK_FALSE(run_tool("otp_encode", {{"plaintext", "hello"}, {"pad", "AB"}}).ok());
}

// ---------------- power ----------------
TEST_CASE("power tools") {
  Result r = run_tool("battery_runtime", {{"capacity_mah", "3000"}, {"rx_ma", "15"}, {"tx_ma", "115"}, {"tx_duty_pct", "10"},
                                          {"usable_pct", "100"}});
  CHECK(outnum(r, "avg_ma") == doctest::Approx(25.0));
  CHECK(outnum(r, "hours") == doctest::Approx(120.0));
  CHECK_FALSE(run_tool("battery_runtime", {{"rx_ma", "0"}, {"tx_ma", "0"}}).ok());
  r = run_tool("ohms_law", {{"v", "12"}, {"i", "2"}});
  CHECK(outnum(r, "r") == doctest::Approx(6));
  CHECK(outnum(r, "p") == doctest::Approx(24));
  r = run_tool("ohms_law", {{"v", "0"}, {"i", "0"}, {"r", "50"}, {"p", "100"}});
  CHECK(outnum(r, "v") == doctest::Approx(70.711).epsilon(1e-4));
  CHECK_FALSE(run_tool("ohms_law", {{"v", "1"}, {"i", "1"}, {"r", "1"}}).ok());
  r = run_tool("wire_drop", {{"awg", "14"}, {"current_a", "10"}, {"length_m", "5"}, {"voltage_v", "12"}});
  CHECK(outnum(r, "diameter") == doctest::Approx(1.628).epsilon(1e-3));
  CHECK(outnum(r, "drop_v") == doctest::Approx(0.828).epsilon(5e-3));
  CHECK(outnum(r, "ampacity") == 15);
  CHECK(outstr(r, "min_awg_3pct") == "10 AWG");
  r = run_tool("solar_sizing", {{"daily_wh", "60"}, {"sun_hours", "4"}, {"system_eff_pct", "75"}, {"autonomy_days", "2"},
                                {"battery_v", "12"}, {"dod_pct", "50"}});
  CHECK(outnum(r, "panel_w") == 20);
  CHECK(outnum(r, "battery_wh") == 240);
  CHECK(outnum(r, "battery_ah") == 20);
  r = run_tool("battery_pack", {{"series", "4"}, {"parallel", "3"}, {"chemistry", "lifepo4"}, {"cell_mah", "3000"}});
  CHECK(outnum(r, "nominal_v") == doctest::Approx(12.8));
  CHECK(outnum(r, "capacity_ah") == doctest::Approx(9));
  CHECK(outnum(r, "full_v") == doctest::Approx(14.6));
}

// ---------------- survival ----------------
TEST_CASE("survival tools: EPA bleach figures and others") {
  Result r = run_tool("water_disinfection", {{"volume_l", "3.785411784"}, {"strength_pct", "6"}});
  CHECK(outnum(r, "drops") == 8);
  r = run_tool("water_disinfection", {{"volume_l", "3.785411784"}, {"strength_pct", "8.25"}});
  CHECK(outnum(r, "drops") == 6);
  r = run_tool("water_disinfection", {{"volume_l", "3.785411784"}, {"strength_pct", "6"}, {"cloudy", "true"}});
  CHECK(outnum(r, "drops") == 16);
  r = run_tool("water_disinfection", {{"method", "iodine"}, {"volume_l", "0.946352946"}});
  CHECK(outnum(r, "drops") == 5);
  r = run_tool("water_disinfection", {{"method", "boil"}, {"altitude_m", "2500"}});
  CHECK(outnum(r, "boil_min") == 3);
  bool cites = false;
  for (const auto& n : run_tool("water_disinfection", {}).notes) cites |= n.find("EPA") != std::string::npos;
  CHECK(cites);
  r = run_tool("fallout_decay", {{"rate", "100"}, {"t_meas_h", "1"}, {"t_future_h", "7"}, {"threshold", "1"}});
  CHECK(outnum(r, "rate_future") == doctest::Approx(9.69).epsilon(1e-3));  // ~1/10 at 7 h
  CHECK(outnum(r, "time_to_threshold_h") == doctest::Approx(46.4).epsilon(2e-3));  // 100^(1/1.2)
  r = run_tool("wind_chill", {{"temp", "-10"}, {"wind", "30"}});
  CHECK(outnum(r, "wind_chill_c") == doctest::Approx(-19.5).epsilon(1e-2));
  CHECK_FALSE(run_tool("wind_chill", {{"temp", "20"}}).ok());
  r = run_tool("heat_index", {{"units", "imperial"}, {"temp", "90"}, {"rh", "70"}});
  CHECK(std::fabs(outnum(r, "heat_index_f") - 106) < 1.0);  // NWS table value
  r = run_tool("heat_index", {{"units", "imperial"}, {"temp", "70"}, {"rh", "50"}});
  CHECK(outnum(r, "heat_index_f") < 75);
  r = run_tool("unit_convert", {{"value", "212"}, {"from", "f"}, {"to", "c"}});
  CHECK(outnum(r, "result") == doctest::Approx(100));
  r = run_tool("unit_convert", {{"value", "1"}, {"from", "gal"}, {"to", "l"}});
  CHECK(outnum(r, "result") == doctest::Approx(3.785411784));
  CHECK_FALSE(run_tool("unit_convert", {{"from", "kg"}, {"to", "l"}}).ok());
  CHECK_FALSE(run_tool("unit_convert", {{"value", "-500"}, {"from", "c"}, {"to", "k"}}).ok());
  r = run_tool("rations", {{"people", "4"}, {"days", "14"}, {"water_stock_l", "100"}});
  CHECK(outnum(r, "water_total_l") == doctest::Approx(4 * 14 * 3.8));
  CHECK(outnum(r, "water_days") == doctest::Approx(100 / (4 * 3.8)).epsilon(1e-2));
}

// ---------------- antenna + rf via registry ----------------
TEST_CASE("antenna / rf tools: dimensions in metric and imperial") {
  Result r = run_tool("dipole", {{"freq_mhz", "146"}});
  REQUIRE(r.ok());
  CHECK(outstr(r, "total") == "0.975 m (3 ft 2 3/8 in)");
  CHECK(r.text.find("Cut list") != std::string::npos);
  CHECK(r.text.find("o===========o") != std::string::npos);
  r = run_tool("fspl", {{"freq_mhz", "433"}, {"distance_km", "1"}});
  CHECK(outnum(r, "fspl") == doctest::Approx(85.18).epsilon(1e-3));
  r = run_tool("link_budget", {});
  REQUIRE(r.ok());
  CHECK(r.find("max_range"));
  r = run_tool("link_budget", {{"preset", "custom"}, {"rx_sens_dbm", "0"}, {"tx_power_dbm", "0"}});
  REQUIRE(r.ok());
  CHECK(r.find("max_range") == nullptr);  // negative budget handled, no NaN
  r = run_tool("swr", {{"from", "swr"}, {"value", "0.5"}});
  CHECK_FALSE(r.ok());
  r = run_tool("swr", {{"from", "impedance"}, {"r_ohm", "75"}});
  CHECK(outnum(r, "swr") == doctest::Approx(1.5));
  r = run_tool("coax_loss", {{"cable", "ladder450"}, {"freq_mhz", "1000"}});
  CHECK_FALSE(r.ok());
  r = run_tool("power_convert", {{"value", "5"}, {"unit", "w"}});
  CHECK(outnum(r, "dbm") == doctest::Approx(36.99).epsilon(1e-3));
  CHECK_FALSE(run_tool("power_convert", {{"value", "0"}, {"unit", "w"}}).ok());
  r = run_tool("relay_coverage", {});
  REQUIRE(r.ok());
  CHECK(outnum(r, "coverage_km") > 1);
  CHECK_FALSE(run_tool("relay_coverage", {{"clutter_m", "20"}, {"relay_height_m", "10"}}).ok());
  r = run_tool("lora_airtime", {{"sf", "12"}, {"bw", "125000"}, {"payload", "10"}, {"preamble", "8"}});
  CHECK(outnum(r, "airtime_ms") == doctest::Approx(991.2).epsilon(1e-4));
  r = run_tool("tape_yagi", {{"band", "custom"}, {"freq_mhz", "300"}});
  CHECK_FALSE(r.ok());
  r = run_tool("cantenna", {{"can_diam_mm", "50"}});
  CHECK_FALSE(r.ok());
  CHECK(r.error.find("narrow") != std::string::npos);
}

}  // TEST_SUITE
