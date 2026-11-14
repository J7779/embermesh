// Navigation tools (category "navigation").
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "tools/nav.h"
#include "tools/tool_impl.h"

namespace ember {
namespace tools {
namespace detail {
namespace {

Field lat_f(const char* key, const char* label, const char* def) {
  return txt(key, label).def_s(def).maxlen(64).help("Decimal (-33.86) or DMS (33 51 35 S).");
}
bool get_coord(Ctx& c, const char* key, bool is_lat, double* out) {
  std::string s;
  if (!c.text(key, &s)) return false;
  if (!nav::parse_coordinate(s, is_lat, out)) {
    return c.fail(std::string(is_lat ? "Latitude" : "Longitude") + " '" + s.substr(0, 30) + "' is not valid" +
                  (is_lat ? " (-90..90, N/S)" : " (-180..180, E/W)"));
  }
  return true;
}
const char* compass16(double b) {
  static const char* n[16] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  int i = static_cast<int>(std::floor(b / 22.5 + 0.5)) % 16;
  return n[i];
}
void out_latlon(Ctx& c, const std::string& prefix, double lat, double lon) {
  c.out(prefix + "lat", "Latitude", lat, "deg", 6);
  c.out(prefix + "lon", "Longitude", lon, "deg", 6);
  c.out_s(prefix + "dms", "DMS", nav::to_dms(lat, true) + " " + nav::to_dms(lon, false));
}

// ---- distance / bearing ----
ToolDef d_dist() {
  return {"distance_bearing", "Distance and bearing", "navigation",
          "Great-circle distance, initial/final bearing between two points (true north).",
          {lat_f("lat1", "From latitude", "51.5074"), lat_f("lon1", "From longitude", "-0.1278"),
           lat_f("lat2", "To latitude", "48.8566"), lat_f("lon2", "To longitude", "2.3522")}};
}
void r_dist(Ctx& c) {
  double a, b, x, y;
  if (!get_coord(c, "lat1", true, &a) || !get_coord(c, "lon1", false, &b) || !get_coord(c, "lat2", true, &x) ||
      !get_coord(c, "lon2", false, &y))
    return;
  double d = nav::distance_m(a, b, x, y);
  double brg = nav::initial_bearing_deg(a, b, x, y);
  double back = nav::initial_bearing_deg(x, y, a, b);
  double fin = std::fmod(back + 180.0, 360.0);
  c.out("distance_km", "Distance", d / 1000, "km", 3);
  c.out("distance_mi", "Distance", d / 1609.344, "mi", 3);
  c.out("distance_nmi", "Distance", d / 1852, "nmi", 3);
  if (d < 0.01) {
    c.note("Points coincide: bearing undefined.");
    return;
  }
  c.out("bearing", "Initial bearing (true)", brg, "deg", 1);
  c.out_s("bearing_compass", "Initial direction", compass16(brg));
  c.out("final_bearing", "Final bearing (true)", fin, "deg", 1);
  c.out("back_bearing", "Bearing back to start", back, "deg", 1);
  c.note("Spherical earth (mean radius 6371 km): distances within ~0.5 %. Bearings are TRUE: correct for local magnetic declination when using a compass.");
}

// ---- destination ----
ToolDef d_dest() {
  return {"destination", "Destination point", "navigation", "Position reached after travelling a distance on a true bearing.",
          {lat_f("lat", "Start latitude", "40.7128"), lat_f("lon", "Start longitude", "-74.0060"),
           num("bearing", "Bearing (true)", "deg").def(90).range(0, 360), num("distance_km", "Distance", "km").def(10).range(0, 20038)}};
}
void r_dest(Ctx& c) {
  double a, b, brg, d;
  if (!get_coord(c, "lat", true, &a) || !get_coord(c, "lon", false, &b) || !c.num("bearing", &brg) || !c.num("distance_km", &d)) return;
  double x, y;
  nav::destination(a, b, brg, d * 1000, &x, &y);
  out_latlon(c, "", x, y);
  c.out_s("locator", "Maidenhead", nav::maidenhead_encode(x, y, 3));
}

// ---- Maidenhead ----
ToolDef d_mh() {
  return {"maidenhead", "Maidenhead grid locator", "navigation", "Convert latitude/longitude to a Maidenhead locator and back.",
          {sel("mode", "Mode").opt("to_locator", "Lat/lon -> locator").opt("from_locator", "Locator -> lat/lon").def_s("to_locator"),
           lat_f("lat", "Latitude", "41.714775"), lat_f("lon", "Longitude", "-72.727260"),
           txt("locator", "Locator").def_s("FN31pr").maxlen(12),
           sel("precision", "Precision").opt("2", "4 characters (~110 km)").opt("3", "6 characters (~5 km)")
               .opt("4", "8 characters (~500 m)").opt("5", "10 characters (~20 m)").def_s("3")}};
}
void r_mh(Ctx& c) {
  std::string mode, loc, prec;
  if (!c.choice("mode", &mode) || !c.choice("precision", &prec)) return;
  if (mode == "to_locator") {
    double a, b;
    if (!get_coord(c, "lat", true, &a) || !get_coord(c, "lon", false, &b)) return;
    c.out_s("locator", "Locator", nav::maidenhead_encode(a, b, prec[0] - '0'));
  } else {
    if (!c.text("locator", &loc)) return;
    double a, b, ha, hb;
    if (!nav::maidenhead_decode(loc, &a, &b, &ha, &hb)) { c.fail("Locator '" + loc.substr(0, 12) + "' is not valid (e.g. FN31pr)"); return; }
    out_latlon(c, "", a, b);
    c.out("half_lat", "Square half-height", ha, "deg", 5);
    c.out("half_lon", "Square half-width", hb, "deg", 5);
    c.note("Coordinates are the centre of the square.");
  }
}

// ---- UTM / MGRS ----
ToolDef d_utm() {
  return {"utm_mgrs", "UTM / MGRS converter", "navigation", "Convert between lat/lon, UTM and MGRS (WGS-84).",
          {sel("mode", "Convert from").opt("latlon", "Lat/lon").opt("utm", "UTM").opt("mgrs", "MGRS").def_s("latlon"),
           lat_f("lat", "Latitude", "38.8895"), lat_f("lon", "Longitude", "-77.0352"),
           txt("utm", "UTM (zone hemisphere easting northing)").def_s("18N 323487 4306483").maxlen(64),
           txt("mgrs", "MGRS").def_s("18S UJ 23486 06483").maxlen(32),
           sel("digits", "MGRS precision").opt("5", "1 m").opt("4", "10 m").opt("3", "100 m").opt("2", "1 km").opt("1", "10 km").def_s("5")}};
}
void r_utm(Ctx& c) {
  std::string mode, ds;
  if (!c.choice("mode", &mode) || !c.choice("digits", &ds)) return;
  double lat, lon;
  if (mode == "latlon") {
    if (!get_coord(c, "lat", true, &lat) || !get_coord(c, "lon", false, &lon)) return;
  } else if (mode == "utm") {
    std::string s;
    nav::Utm u;
    if (!c.text("utm", &s)) return;
    if (!nav::parse_utm(s, &u)) { c.fail("UTM must look like '18N 323487 4306483' (zone, N/S or band letter, easting, northing)"); return; }
    if (!nav::utm_to_latlon(u.zone, u.north, u.easting, u.northing, &lat, &lon)) { c.fail("UTM coordinate out of range"); return; }
  } else {
    std::string s;
    if (!c.text("mgrs", &s)) return;
    if (!nav::mgrs_to_latlon(s, &lat, &lon)) { c.fail("MGRS must look like '18S UJ 23486 06483'"); return; }
  }
  out_latlon(c, "", lat, lon);
  nav::Utm u;
  if (!nav::latlon_to_utm(lat, lon, &u)) {
    c.note("UTM/MGRS cover 80 S to 84 N only; polar regions use UPS (not supported).");
    return;
  }
  char b[64];
  std::snprintf(b, sizeof b, "%d%c %.0f %.0f", u.zone, u.north ? 'N' : 'S', std::floor(u.easting), std::floor(u.northing));
  c.out_s("utm", "UTM", b);
  c.out_s("mgrs", "MGRS", nav::utm_to_mgrs(u, ds[0] - '0'));
  c.out("zone", "UTM zone", u.zone, "", 0);
  c.out_s("band", "Latitude band", std::string(1, u.band));
}

// ---- DMS ----
ToolDef d_dms() {
  return {"dms", "Decimal <-> degrees/minutes/seconds", "navigation", "Convert a coordinate between decimal degrees, DMS and DM formats.",
          {txt("value", "Coordinate").def_s("40 26 46 N").maxlen(64),
           sel("axis", "Axis").opt("lat", "Latitude").opt("lon", "Longitude").def_s("lat")}};
}
void r_dms(Ctx& c) {
  std::string ax;
  double v;
  if (!c.choice("axis", &ax)) return;
  if (!get_coord(c, "value", ax == "lat", &v)) return;
  c.out("decimal", "Decimal degrees", v, "deg", 6);
  c.out_s("dms", "Degrees, minutes, seconds", nav::to_dms(v, ax == "lat"));
  c.out_s("dm", "Degrees, decimal minutes", nav::to_dm(v, ax == "lat"));
}

// ---- sun ----
bool parse_hhmm(const std::string& s, double* min) {
  if (s.size() != 5 || s[2] != ':') return false;
  for (int i : {0, 1, 3, 4})
    if (s[static_cast<size_t>(i)] < '0' || s[static_cast<size_t>(i)] > '9') return false;
  int h = std::atoi(s.substr(0, 2).c_str()), m = std::atoi(s.substr(3, 2).c_str());
  if (h > 23 || m > 59) return false;
  *min = h * 60 + m;
  return true;
}
ToolDef d_sun() {
  return {"sun", "Sunrise / sunset / sun position", "navigation",
          "Sunrise, sunset, solar noon, civil twilight and sun azimuth/elevation for a date and place (NOAA algorithm).",
          {txt("date", "Date (YYYY-MM-DD)").def_s("2026-06-21").maxlen(10), lat_f("lat", "Latitude", "51.4769"),
           lat_f("lon", "Longitude", "-0.0005"), num("tz_hours", "Time zone offset from UTC", "h").def(1).range(-14, 14).step(0.25),
           txt("time", "Local time for sun position (HH:MM)").def_s("12:00").maxlen(5)}};
}
void r_sun(Ctx& c) {
  std::string ds, ts;
  int y, m, d;
  double lat, lon, tz, tmin;
  if (!c.text("date", &ds) || !get_coord(c, "lat", true, &lat) || !get_coord(c, "lon", false, &lon) || !c.num("tz_hours", &tz) ||
      !c.text("time", &ts))
    return;
  if (!nav::parse_date(ds, &y, &m, &d)) { c.fail("Date must be YYYY-MM-DD (1900-2200)"); return; }
  if (!parse_hhmm(ts, &tmin)) { c.fail("Time must be HH:MM (24 h)"); return; }
  nav::SunEvents e = nav::sun_events(y, m, d, lat, lon, tz);
  if (e.always_up) c.out_s("sunrise", "Sunrise", "none (midnight sun)");
  else if (e.always_down) c.out_s("sunrise", "Sunrise", "none (polar night)");
  else {
    c.out_s("sunrise", "Sunrise", nav::hhmm(e.sunrise_min));
    c.out_s("sunset", "Sunset", nav::hhmm(e.sunset_min));
  }
  c.out_s("solar_noon", "Solar noon", nav::hhmm(e.noon_min));
  c.out_s("civil_dawn", "Civil dawn", nav::hhmm(e.civil_dawn_min));
  c.out_s("civil_dusk", "Civil dusk", nav::hhmm(e.civil_dusk_min));
  c.out("day_length_h", "Day length", e.day_length_min / 60.0, "h", 2);
  double az, el;
  nav::sun_position(y, m, d, tmin, lat, lon, tz, &az, &el);
  c.out("azimuth", "Sun azimuth at " + ts, az, "deg", 1);
  c.out("elevation", "Sun elevation at " + ts, el, "deg", 1);
  c.note("Times are local (UTC + offset) and accurate to about a minute; terrain and refraction can shift them a few minutes.");
  c.note("Without a compass: at solar noon the sun is due south (northern hemisphere) or due north (southern).");
}

// ---- moon ----
ToolDef d_moon() {
  return {"moon_phase", "Moon phase", "navigation", "Moon age, illumination and days to the next full/new moon for a date.",
          {txt("date", "Date (YYYY-MM-DD)").def_s("2026-01-01").maxlen(10),
           num("hour_utc", "Hour (UTC)", "h").def(12).range(0, 24)}};
}
void r_moon(Ctx& c) {
  std::string ds;
  double h;
  int y, m, d;
  if (!c.text("date", &ds) || !c.num("hour_utc", &h)) return;
  if (!nav::parse_date(ds, &y, &m, &d)) { c.fail("Date must be YYYY-MM-DD (1900-2200)"); return; }
  nav::MoonPhase p = nav::moon_phase(nav::julian_day(y, m, d, h));
  c.out_s("phase", "Phase", p.name);
  c.out("age_days", "Age", p.age_days, "days", 1);
  c.out("illumination", "Illuminated", p.illumination * 100, "%", 0);
  c.out("days_to_full", "Days to full moon", p.days_to_full, "days", 1);
  c.out("days_to_new", "Days to new moon", p.days_to_new, "days", 1);
  c.note("Mean-phase approximation: event times can be off by up to about half a day.");
  c.note("A bright moon helps night travel (and night spotting); full moon rises at sunset, new moon nights are darkest.");
}

// ---- pace count ----
ToolDef d_pace() {
  return {"pace_count", "Pace count", "navigation", "Convert between distance and paces using your measured pace count.",
          {num("paces_per_100m", "Your paces per 100 m", "paces").def(65).range(20, 300)
               .help("Count double steps (left foot) over a measured 100 m; add ~10-20 % for hills, mud or heavy loads."),
           sel("mode", "Mode").opt("to_paces", "Distance -> paces").opt("to_distance", "Paces -> distance").def_s("to_paces"),
           num("distance_m", "Distance", "m").def(1000).range(0, 1e6), num("paces", "Paces counted", "").def(650).range(0, 1e7),
           num("terrain_pct", "Terrain/load correction", "%").def(0).range(0, 100)}};
}
void r_pace(Ctx& c) {
  double p100, dist, paces, corr;
  std::string mode;
  if (!c.num("paces_per_100m", &p100) || !c.choice("mode", &mode) || !c.num("distance_m", &dist) || !c.num("paces", &paces) ||
      !c.num("terrain_pct", &corr))
    return;
  double eff = p100 * (1 + corr / 100.0);
  if (mode == "to_paces") {
    c.out("paces", "Paces", dist / 100.0 * eff, "", 0);
    c.out("beads", "Ranger-bead count (1 bead per 100 m)", std::floor(dist / 100.0), "", 0);
  } else {
    c.out_len("distance", "Distance", paces / eff * 100.0);
  }
  c.note("Ranger beads: slide one lower bead every 100 m, one upper bead every km.");
}

// ---- map scale ----
ToolDef d_scale() {
  return {"map_scale", "Map scale", "navigation", "Convert a measurement on a map to ground distance and back.",
          {num("scale", "Scale 1 :", "").def(25000).range(100, 1e8),
           sel("mode", "Mode").opt("to_ground", "Map -> ground").opt("to_map", "Ground -> map").def_s("to_ground"),
           num("map_cm", "Distance on map", "cm").def(4).range(0, 1e4), num("ground_km", "Ground distance", "km").def(1).range(0, 1e5)}};
}
void r_scale(Ctx& c) {
  double s, mc, gk;
  std::string mode;
  if (!c.num("scale", &s) || !c.choice("mode", &mode) || !c.num("map_cm", &mc) || !c.num("ground_km", &gk)) return;
  if (mode == "to_ground") c.out_len("ground", "Ground distance", mc / 100.0 * s);
  else {
    c.out("map_cm", "Distance on map", gk * 1000 * 100 / s, "cm", 2);
    c.out("map_in", "Distance on map", gk * 1000 / s / 0.0254, "in", 2);
  }
  c.out_len("per_cm", "1 cm on the map is", s / 100.0);
  c.note("Map distances are horizontal: steep ground is longer on foot. Measure curvy routes with a string.");
}

const Entry kEntries[] = {{d_dist, r_dist}, {d_dest, r_dest}, {d_mh, r_mh},     {d_utm, r_utm},   {d_dms, r_dms},
                          {d_sun, r_sun},   {d_moon, r_moon}, {d_pace, r_pace}, {d_scale, r_scale}};

}  // namespace

const Entry* navigation_tools(size_t* n) {
  *n = sizeof(kEntries) / sizeof(kEntries[0]);
  return kEntries;
}

}  // namespace detail
}  // namespace tools
}  // namespace ember
