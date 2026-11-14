// EmberMesh core — offline navigation math: great circle, Maidenhead, UTM/MGRS (WGS-84),
// DMS parsing/formatting, sun position / rise / set (NOAA algorithm) and moon phase.
#pragma once

#include <string>

namespace ember {
namespace nav {

constexpr double kEarthRadiusM = 6371008.8;

// ---- great circle (spherical earth, mean radius) ----
double distance_m(double lat1, double lon1, double lat2, double lon2);
double initial_bearing_deg(double lat1, double lon1, double lat2, double lon2);  // 0..360
void destination(double lat, double lon, double bearing_deg, double dist_m, double* lat2, double* lon2);
double normalize_lon(double lon);  // → [-180, 180)

// ---- Maidenhead locator ----
// pairs: 2..5 (4..10 characters). Returns "" for invalid input (lat must be in [-90, 90]).
std::string maidenhead_encode(double lat, double lon, int pairs = 3);
// Decodes 2/4/6/8/10-char locators (case-insensitive). Returns the centre of the square and its
// half-size in degrees. False on malformed input.
bool maidenhead_decode(const std::string& loc, double* lat, double* lon, double* half_lat = nullptr,
                       double* half_lon = nullptr);

// ---- UTM / MGRS (WGS-84) ----
struct Utm {
  int zone = 0;         // 1..60
  char band = 0;        // 'C'..'X' (MGRS latitude band)
  bool north = true;
  double easting = 0;   // m
  double northing = 0;  // m
};
// Valid for -80 <= lat <= 84 (UPS polar regions are not supported). Applies Norway/Svalbard zones.
bool latlon_to_utm(double lat, double lon, Utm* out);
// Forced-zone variant (used internally and for cross-zone work).
bool latlon_to_utm_zone(double lat, double lon, int zone, Utm* out);
bool utm_to_latlon(int zone, bool north, double easting, double northing, double* lat, double* lon);
// Parses "33N 500000 4649776" / "33 N 500000 4649776" / "33T 500000 4649776" (band letter → hemisphere).
bool parse_utm(const std::string& s, Utm* out);
// digits: 1..5 per coordinate (5 = 1 m). Output like "33T WN 12345 67890".
std::string utm_to_mgrs(const Utm& u, int digits = 5);
bool latlon_to_mgrs(double lat, double lon, int digits, std::string* out);
// Accepts spaces or none: "33TWN1234567890", "4Q FJ 12345 67890". Returns the SW corner + half
// the precision (i.e. the centre of the referenced square).
bool mgrs_to_latlon(const std::string& s, double* lat, double* lon);

// ---- DMS ----
std::string to_dms(double deg, bool is_lat, int sec_decimals = 1);  // 40°26'46.3"N
std::string to_dm(double deg, bool is_lat, int min_decimals = 3);   // 40°26.772'N
// Lenient parser: "-40.446", "40 26 46 N", "40°26'46\"N", "N40 26.77", "40d26m46s S".
bool parse_coordinate(const std::string& s, bool is_lat, double* out);

// ---- dates ----
bool parse_date(const std::string& s, int* y, int* m, int* d);  // YYYY-MM-DD, 1900..2200
double julian_day(int y, int m, int d, double hour_utc = 0);
int day_of_week(int y, int m, int d);  // 0 = Sunday

// ---- sun (NOAA solar calculator equations) ----
struct SunEvents {
  bool always_up = false;    // midnight sun
  bool always_down = false;  // polar night
  double sunrise_min = 0;    // minutes after local midnight (local = UTC + tz_hours); may be <0 or >1440
  double sunset_min = 0;
  double noon_min = 0;
  double civil_dawn_min = 0;  // NaN if no civil twilight event
  double civil_dusk_min = 0;
  double day_length_min = 0;
};
SunEvents sun_events(int y, int m, int d, double lat, double lon, double tz_hours);
// Sun position at a local time (minutes after local midnight). Elevation includes refraction.
void sun_position(int y, int m, int d, double local_min, double lat, double lon, double tz_hours, double* azimuth_deg,
                  double* elevation_deg);
std::string hhmm(double minutes);  // "06:42" (wraps into 0..24 h, "--:--" for NaN)

// ---- moon ----
struct MoonPhase {
  double age_days;      // 0 = new
  double illumination;  // 0..1
  std::string name;
  double days_to_full;
  double days_to_new;
};
MoonPhase moon_phase(double jd);

}  // namespace nav
}  // namespace ember
