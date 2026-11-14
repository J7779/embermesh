#include "rf/bands.h"

#include <cstdio>

namespace ember {
namespace rf {

namespace {

constexpr double kMHz = 1e6;
constexpr double kKHz = 1e3;

std::string chname(const char* prefix, int n) {
  char b[32];
  std::snprintf(b, sizeof b, "%s%d", prefix, n);
  return b;
}

char upper(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c; }
std::string up(const std::string& s) {
  std::string o = s;
  for (auto& c : o) c = upper(c);
  return o;
}

bool region_has(const std::string& list, const std::string& code) {
  size_t i = 0;
  while (i <= list.size()) {
    size_t j = list.find(',', i);
    if (j == std::string::npos) j = list.size();
    if (list.compare(i, j - i, code) == 0) return true;
    i = j + 1;
  }
  return false;
}

bool in_list(const char* const* list, const std::string& code) {
  for (; *list; ++list)
    if (code == *list) return true;
  return false;
}

// ISO 3166-1 alpha-2 codes grouped by the data set that applies to them.
// CEPT harmonised short-range / PMR446 / CB rules: EU + EEA + Switzerland.
const char* const kCept[] = {"AT", "BE", "BG", "HR", "CY", "CZ", "DK", "EE", "FI", "FR", "DE", "GR", "HU", "IE",
                             "IT", "LV", "LT", "LU", "MT", "NL", "PL", "PT", "RO", "SK", "SI", "ES", "SE", "NO",
                             "IS", "LI", "CH", nullptr};
// Countries using the generic AS923 LoRa plan.
const char* const kAs923[] = {"SG", "TH", "MY", "VN", "PH", "HK", "TW", "ID", "BN", "KH", "LA", "MM", nullptr};
// IARU Region 2: the Americas.
const char* const kR2[] = {"US", "CA", "MX", "BR", "AR", "CL", "CO", "PE", "VE", "EC", "BO", "PY", "UY", "GY",
                           "SR", "GT", "HN", "SV", "NI", "CR", "PA", "CU", "DO", "HT", "JM", "TT", "BS", "BB",
                           "BZ", "PR", "VI", "GL", nullptr};
// IARU Region 3: Asia-Pacific.
const char* const kR3[] = {"AU", "NZ", "JP", "KR", "IN", "AS", "CN", "PK", "BD", "LK", "NP", "FJ", "PG",
                           "SG", "TH", "MY", "VN", "PH", "HK", "TW", "ID", "BN", "KH", "LA", "MM", "MN", nullptr};
// IARU Region 1: Europe, Africa, Middle East, northern Asia (most remaining codes we know).
const char* const kR1[] = {"EU", "UK", "GB", "ZA", "RU", "UA", "BY", "TR", "IL", "EG", "MA", "DZ", "TN", "NG",
                           "KE", "GH", "ET", "TZ", "UG", "NA", "BW", "ZW", "ZM", "MZ", "AO", "SN", "CM", "SA",
                           "AE", "QA", "KW", "OM", "JO", "LB", "IQ", "IR", "KZ", "UZ", "GE", "AM", "AZ", "RS",
                           "BA", "ME", "MK", "AL", "MD", nullptr};

// Region tags a code matches: the code itself, the data set(s) that apply to it, its IARU region
// and INTL. Unknown codes (e.g. "OTHER") match INTL plus the band plans of all three IARU regions.
std::vector<std::string> tags_for(const std::string& r) {
  std::vector<std::string> t;
  auto add = [&](const std::string& x) {
    for (const auto& y : t)
      if (y == x) return;
    t.push_back(x);
  };
  if (r.empty()) return t;
  add(r);
  if (r == "GB") add("UK");
  if (in_list(kCept, r)) add("EU");
  if (in_list(kAs923, r)) add("AS");
  if (r == "PR" || r == "VI" || r == "GU") add("US");
  bool known = true;
  if (r == "R1" || r == "R2" || r == "R3") {
  } else if (in_list(kR2, r)) {
    add("R2");
  } else if (in_list(kR3, r) || r == "GU") {
    add("R3");
  } else if (in_list(kR1, r) || in_list(kCept, r)) {
    add("R1");
  } else if (r != "INTL") {
    known = false;
  }
  if (!known) {
    add("R1");
    add("R2");
    add("R3");
  }
  add("INTL");
  return t;
}

void add_frs_gmrs(std::vector<BandService>& v) {
  BandService frs{"frs", "FRS (Family Radio Service)", "US", "Licence-free", 2.0,
                  "22 shared channels, 12.5 kHz NBFM, fixed antenna radios. Channels 8-14 limited to 0.5 W ERP. "
                  "Ch 20 (462.675) is widely used as an emergency/traveller channel; ch 3 is used by community "
                  "preparedness nets (e.g. the 'Channel 3 Project').", {}};
  BandService gmrs{"gmrs", "GMRS (General Mobile Radio Service)", "US", "FCC licence required (no exam; covers the family)", 50.0,
                   "Same 462/467 MHz channels as FRS; up to 50 W on 15-22 and repeaters, 5 W on 1-7, 0.5 W on 8-14. "
                   "Repeaters listen on 467.xxx (+5 MHz) and transmit on the 462.xxx main channel.", {}};
  for (int n = 1; n <= 22; ++n) {
    double f;
    if (n <= 7) f = 462.5625 + 0.025 * (n - 1);
    else if (n <= 14) f = 467.5625 + 0.025 * (n - 8);
    else f = 462.5500 + 0.025 * (n - 15);
    std::string note = (n >= 8 && n <= 14) ? "0.5 W ERP max" : "";
    if (n == 20) note = "Common emergency / traveller assistance channel";
    frs.channels.push_back({chname("Ch ", n), f * kMHz, 12500, note});
    gmrs.channels.push_back({chname("Ch ", n), f * kMHz, n >= 8 && n <= 14 ? 12500.0 : 20000.0, note});
  }
  for (int n = 15; n <= 22; ++n) {
    double in = 467.5500 + 0.025 * (n - 15);
    gmrs.channels.push_back({chname("Repeater input for ch ", n), in * kMHz, 20000, "Repeater input (+5 MHz)"});
  }
  v.push_back(frs);
  v.push_back(gmrs);
}

void add_murs(std::vector<BandService>& v) {
  v.push_back({"murs", "MURS (Multi-Use Radio Service)", "US", "Licence-free", 2.0,
               "Five VHF channels; VHF carries better than FRS through woods and over terrain. External antennas allowed.",
               {{"Ch 1", 151.820 * kMHz, 11250, ""},
                {"Ch 2", 151.880 * kMHz, 11250, ""},
                {"Ch 3", 151.940 * kMHz, 11250, ""},
                {"Ch 4 (Blue Dot)", 154.570 * kMHz, 20000, ""},
                {"Ch 5 (Green Dot)", 154.600 * kMHz, 20000, ""}}});
}

void add_cb(std::vector<BandService>& v) {
  static const double f40[40] = {26.965, 26.975, 26.985, 27.005, 27.015, 27.025, 27.035, 27.055, 27.065, 27.075,
                                 27.085, 27.105, 27.115, 27.125, 27.135, 27.155, 27.165, 27.175, 27.185, 27.205,
                                 27.215, 27.225, 27.255, 27.235, 27.245, 27.265, 27.275, 27.285, 27.295, 27.305,
                                 27.315, 27.325, 27.335, 27.345, 27.355, 27.365, 27.375, 27.385, 27.395, 27.405};
  BandService us{"cb_us", "CB radio 27 MHz (40 channels)", "US,CA,AU", "Licence-free", 4.0,
                 "4 W AM carrier, 12 W PEP SSB (US). Ch 9 = emergency, ch 19 = highway/truckers. "
                 "SSB users congregate on ch 36-40 (LSB on 38). Range: a few km ground wave, much further on skip.",
                 {}};
  BandService eu{"cb_eu", "CB radio 27 MHz CEPT (40 channels)", "EU,UK", "Licence-free in most CEPT countries", 4.0,
                 "CEPT harmonised: 4 W FM/AM, 12 W PEP SSB. Some countries add extra channels (e.g. 80-channel "
                 "Germany). Ch 9 emergency, ch 19 road traffic.",
                 {}};
  for (int n = 1; n <= 40; ++n) {
    std::string note = n == 9 ? "Emergency" : (n == 19 ? "Road / highway" : "");
    us.channels.push_back({chname("Ch ", n), f40[n - 1] * kMHz, 10000, note});
    eu.channels.push_back({chname("Ch ", n), f40[n - 1] * kMHz, 10000, note});
  }
  v.push_back(us);
  v.push_back(eu);
  BandService uk{"cb_uk", "UK CB 27/81 (40 channels, FM)", "UK", "Licence-free", 4.0,
                 "UK-specific 27.60125-27.99125 MHz channels, FM only, alongside the CEPT channels. Ch 9 emergency, "
                 "ch 14 traditional calling channel.",
                 {}};
  for (int n = 1; n <= 40; ++n) {
    std::string note = n == 9 ? "Emergency" : (n == 14 ? "Calling" : "");
    uk.channels.push_back({chname("Ch ", n), (27.60125 + 0.01 * (n - 1)) * kMHz, 10000, note});
  }
  v.push_back(uk);
}

void add_pmr_lpd(std::vector<BandService>& v) {
  BandService pmr{"pmr446", "PMR446", "EU,UK", "Licence-free", 0.5,
                  "0.5 W ERP, integral (non-detachable) antenna, 12.5 kHz analogue FM. Ch 1-8 are the original "
                  "set every radio has; 9-16 were added in 2018. No repeaters.", {}};
  for (int n = 1; n <= 16; ++n)
    pmr.channels.push_back({chname("Ch ", n), (446.00625 + 0.0125 * (n - 1)) * kMHz, 12500, ""});
  v.push_back(pmr);
  BandService lpd{"lpd433", "LPD433", "EU", "Licence-free in many European countries", 0.01,
                  "69 channels at 25 kHz spacing, 10 mW ERP. Shares the 433 MHz ISM band with amateur 70 cm and "
                  "telemetry devices; many dual-band handhelds include it.", {}};
  for (int n = 1; n <= 69; ++n)
    lpd.channels.push_back({chname("Ch ", n), (433.075 + 0.025 * (n - 1)) * kMHz, 25000, ""});
  v.push_back(lpd);
}

void add_uhf_cb(std::vector<BandService>& v) {
  BandService s{"uhf_cb_au", "UHF CB 477 MHz (80 channels)", "AU,NZ", "Licence-free (class licence)", 5.0,
                "5 W FM. Ch 5/35 = emergency (duplex: ch 5 is the repeater output, ch 35 its input); do not use them "
                "for chat. Ch 11 = calling channel. Ch 40 = highway / road (AU). Ch 1-8 and 41-48 are repeater "
                "outputs, ch 31-38 and 71-78 their inputs (+750 kHz). Ch 22/23 are data/telemetry only (no voice).",
                {}};
  for (int n = 1; n <= 80; ++n) {
    double f = n <= 40 ? 476.425 + 0.025 * (n - 1) : 476.4375 + 0.025 * (n - 41);
    std::string note;
    if (n == 5) note = "EMERGENCY (repeater output)";
    else if (n == 35) note = "EMERGENCY (repeater input)";
    else if (n == 11) note = "Calling channel";
    else if (n == 40) note = "Highway / road (AU)";
    else if (n == 22 || n == 23) note = "Data / telemetry only";
    else if ((n >= 1 && n <= 8) || (n >= 41 && n <= 48)) note = "Repeater output";
    else if ((n >= 31 && n <= 38) || (n >= 71 && n <= 78)) note = "Repeater input";
    s.channels.push_back({chname("Ch ", n), f * kMHz, 12500, note});
  }
  v.push_back(s);
}

void add_marine(std::vector<BandService>& v) {
  v.push_back({"marine_vhf", "Marine VHF (key channels)", "INTL",
               "Ship station / operator licence in most countries (US: not required for recreational vessels in domestic waters)",
               25.0,
               "25 W maximum, 1 W for short range/harbour. Ch 16 is monitored for distress (MAYDAY), urgency (PAN-PAN) "
               "and safety; switch to a working channel after first contact.",
               {{"Ch 16", 156.800 * kMHz, 25000, "DISTRESS, safety and calling"},
                {"Ch 70", 156.525 * kMHz, 25000, "DSC digital distress/calling only (no voice)"},
                {"Ch 06", 156.300 * kMHz, 25000, "Intership safety; SAR coordination"},
                {"Ch 09", 156.450 * kMHz, 25000, "Secondary calling (US)"},
                {"Ch 13", 156.650 * kMHz, 25000, "Bridge-to-bridge navigation safety, 1 W"},
                {"Ch 22A", 157.100 * kMHz, 25000, "US Coast Guard liaison"},
                {"Ch 12", 156.600 * kMHz, 25000, "Port operations"},
                {"Ch 68", 156.425 * kMHz, 25000, "Non-commercial working (US)"},
                {"Ch 72", 156.625 * kMHz, 25000, "Intership"},
                {"Ch 77", 156.875 * kMHz, 25000, "Intership"},
                {"AIS 1", 161.975 * kMHz, 25000, "AIS data (receive with an SDR to see ship positions)"},
                {"AIS 2", 162.025 * kMHz, 25000, "AIS data"}}});
}

void add_ham(std::vector<BandService>& v) {
  const char* lic = "Amateur licence required (exam). Station ID required; encryption not allowed";
  v.push_back({"ham_2m_r2", "Amateur 2 m (144-148 MHz), Americas", "US,CA,R2", lic, 0,
               "FM simplex and repeaters. Local band plans vary; listen before transmitting.",
               {{"National simplex calling", 146.520 * kMHz, 16000, "FM"},
                {"Simplex", 146.550 * kMHz, 16000, "FM"},
                {"Simplex", 146.580 * kMHz, 16000, "FM"},
                {"APRS", 144.390 * kMHz, 16000, "1200 bd AFSK packet (North America)"},
                {"SSB/CW calling", 144.200 * kMHz, 2700, "USB"},
                {"Packet", 145.010 * kMHz, 16000, "Common 1200 bd packet/BBS frequency"},
                {"ISS APRS digipeater", 145.825 * kMHz, 16000, "Worldwide"},
                {"ISS voice downlink", 145.800 * kMHz, 16000, "Worldwide (receive)"}}});
  v.push_back({"ham_2m_r1", "Amateur 2 m (144-146 MHz), Europe / IARU Region 1", "EU,UK,R1", lic, 0,
               "IARU Region 1 band plan. FM simplex channels at 12.5 kHz around 145.2-145.6 MHz.",
               {{"FM calling", 145.500 * kMHz, 16000, "FM simplex calling"},
                {"APRS", 144.800 * kMHz, 16000, "1200 bd AFSK packet (Region 1)"},
                {"SSB calling", 144.300 * kMHz, 2700, "USB"},
                {"ISS APRS digipeater", 145.825 * kMHz, 16000, "Worldwide"},
                {"ISS voice downlink", 145.800 * kMHz, 16000, "Worldwide (receive)"}}});
  v.push_back({"ham_2m_r3", "Amateur 2 m / 70 cm, Asia-Pacific / IARU Region 3", "R3", lic, 0,
               "2 m is 144-148 MHz in most of Region 3 (144-146 MHz in some countries); 70 cm is 430-440 MHz. "
               "Calling and APRS frequencies differ per country: listen first and ask local operators. The "
               "satellite frequencies below work everywhere.",
               {{"ISS APRS digipeater", 145.825 * kMHz, 16000, "Worldwide"},
                {"ISS voice downlink", 145.800 * kMHz, 16000, "Worldwide (receive)"}}});
  v.push_back({"ham_2m_br", "Amateur 2 m (144-148 MHz), Brazil", "BR", lic, 0,
               "IARU Region 2 band plan (LABRE). APRS in Brazil uses 145.570 MHz, not the North American 144.390.",
               {{"APRS", 145.570 * kMHz, 16000, "1200 bd AFSK packet (Brazil)"},
                {"ISS APRS digipeater", 145.825 * kMHz, 16000, "Worldwide"}}});
  v.push_back({"ham_2m_au", "Amateur 2 m (144-148 MHz), Australia", "AU", lic, 0, "WIA band plan.",
               {{"APRS", 145.175 * kMHz, 16000, "1200 bd AFSK packet (Australia)"},
                {"ISS APRS digipeater", 145.825 * kMHz, 16000, "Worldwide"}}});
  v.push_back({"ham_70cm_r2", "Amateur 70 cm (420-450 MHz), Americas", "US,CA,R2", lic, 0,
               "US allocation 420-450 MHz; FM simplex and repeaters, ATV, satellites.",
               {{"National simplex calling", 446.000 * kMHz, 16000, "FM"},
                {"SSB/CW calling", 432.100 * kMHz, 2700, "USB"}}});
  v.push_back({"ham_70cm_r1", "Amateur 70 cm (430-440 MHz), Europe / IARU Region 1", "EU,UK,R1", lic, 0,
               "Shares 433.05-434.79 MHz with ISM/LPD devices.",
               {{"FM calling", 433.500 * kMHz, 16000, "FM simplex calling"},
                {"SSB calling", 432.200 * kMHz, 2700, "USB"},
                {"LoRa APRS", 433.775 * kMHz, 125000, "Community convention: LoRa SF12/125 kHz APRS"}}});
  v.push_back({"ham_6m", "Amateur 6 m (50-54 MHz)", "US,CA,R2", lic, 0,
               "'Magic band': mostly local, with sporadic-E openings of 1000-2000 km in summer.",
               {{"FM simplex calling", 52.525 * kMHz, 16000, "FM (US)"},
                {"SSB calling", 50.125 * kMHz, 2700, "USB (US)"},
                {"FT8", 50.313 * kMHz, 3000, "USB dial, worldwide"}}});
  v.push_back({"ham_10m", "Amateur 10 m (28.0-29.7 MHz)", "INTL", lic, 0,
               "Open worldwide near solar maximum; ground wave tens of km otherwise. FM allowed 29.5-29.7 MHz.",
               {{"FM simplex calling", 29.600 * kMHz, 16000, "FM"},
                {"SSB calling", 28.400 * kMHz, 2700, "USB (customary)"},
                {"QRP CW", 28.060 * kMHz, 500, "CW"},
                {"FT8", 28.074 * kMHz, 3000, "USB dial"}}});
  v.push_back({"ham_hf_emergency", "Amateur HF emergency centres of activity", "INTL", lic, 0,
               "IARU Region 1 'Centre of Activity' frequencies for emergency traffic; keep them clear and listen "
               "first. Use +/- a few kHz if occupied. Most nets: LSB below 10 MHz, USB above.",
               {{"80 m", 3.760 * kMHz, 2700, "LSB"},
                {"40 m", 7.110 * kMHz, 2700, "LSB"},
                {"20 m", 14.300 * kMHz, 2700, "USB; also Maritime Mobile Service Net"},
                {"17 m", 18.160 * kMHz, 2700, "USB"},
                {"15 m", 21.360 * kMHz, 2700, "USB"},
                {"Hurricane Watch Net", 14.325 * kMHz, 2700, "USB (Americas, during hurricanes)"}}});
  v.push_back({"ham_60m_us", "Amateur 60 m channels (US)", "US", lic, 100,
               "Five channelised USB frequencies (dial shown), 100 W PEP ERP. Interoperable with US government "
               "SHARES emergency nets. NVIS workhorse by day.",
               {{"Ch 1", 5330.5 * kKHz, 2800, "USB dial"},
                {"Ch 2", 5346.5 * kKHz, 2800, "USB dial"},
                {"Ch 3", 5357.0 * kKHz, 2800, "USB dial"},
                {"Ch 4", 5371.5 * kKHz, 2800, "USB dial"},
                {"Ch 5", 5403.5 * kKHz, 2800, "USB dial"}}});
  v.push_back({"ham_hf_digital", "Amateur HF digital / QRP calling frequencies", "INTL", lic, 0,
               "Dial frequencies (USB) of common weak-signal modes, useful to find activity and propagation.",
               {{"FT8 80 m", 3.573 * kMHz, 3000, "USB dial"},
                {"FT8 40 m", 7.074 * kMHz, 3000, "USB dial"},
                {"FT8 30 m", 10.136 * kMHz, 3000, "USB dial"},
                {"FT8 20 m", 14.074 * kMHz, 3000, "USB dial"},
                {"FT8 17 m", 18.100 * kMHz, 3000, "USB dial"},
                {"FT8 15 m", 21.074 * kMHz, 3000, "USB dial"},
                {"JS8Call 40 m", 7.078 * kMHz, 3000, "USB dial, keyboard chat + store-and-forward"},
                {"JS8Call 20 m", 14.078 * kMHz, 3000, "USB dial"},
                {"QRP CW 80 m", 3.560 * kMHz, 500, "CW"},
                {"QRP CW 40 m", 7.030 * kMHz, 500, "CW"},
                {"QRP CW 20 m", 14.060 * kMHz, 500, "CW"},
                {"QRP CW 15 m", 21.060 * kMHz, 500, "CW"}}});
}

void add_weather_time_aviation(std::vector<BandService>& v) {
  BandService wx{"noaa_wx", "NOAA Weather Radio / Weatheradio Canada", "US,CA", "Receive only", 0,
                 "Continuous weather and hazard broadcasts on seven VHF channels; most areas hear one or two. "
                 "Many handhelds and scanners have a WX button.", {}};
  for (int n = 1; n <= 7; ++n) wx.channels.push_back({chname("WX", n), (162.400 + 0.025 * (n - 1)) * kMHz, 25000, ""});
  // Conventional WX channel numbering is not in frequency order; label with the frequency instead.
  for (auto& c : wx.channels) {
    char b[32];
    std::snprintf(b, sizeof b, "%.3f MHz", c.freq_hz / kMHz);
    c.name = b;
  }
  v.push_back(wx);
  v.push_back({"time_stations", "Time and standard-frequency stations", "INTL", "Receive only", 0,
               "Exact time (UTC) and a frequency reference with any shortwave/longwave receiver: set clocks, "
               "calibrate radios, check propagation (which ones you hear tells you which bands are open).",
               {{"WWV (Fort Collins, USA)", 2.5 * kMHz, 0, "AM voice time; also 5, 10, 15, 20, 25 MHz"},
                {"WWV", 5.0 * kMHz, 0, ""},
                {"WWV", 10.0 * kMHz, 0, "Most reliable WWV frequency"},
                {"WWV", 15.0 * kMHz, 0, ""},
                {"WWV", 20.0 * kMHz, 0, ""},
                {"WWV", 25.0 * kMHz, 0, ""},
                {"WWVH (Hawaii, USA)", 2.5 * kMHz, 0, "Also 5, 10, 15 MHz (female voice)"},
                {"WWVH", 5.0 * kMHz, 0, ""},
                {"WWVH", 10.0 * kMHz, 0, ""},
                {"WWVH", 15.0 * kMHz, 0, ""},
                {"CHU (Ottawa, Canada)", 3.330 * kMHz, 0, "USB-compatible AM; also 7.850, 14.670 MHz"},
                {"CHU", 7.850 * kMHz, 0, ""},
                {"CHU", 14.670 * kMHz, 0, ""},
                {"BPM (China)", 10.0 * kMHz, 0, "Also 2.5, 5, 15 MHz"},
                {"RWM (Russia)", 9.996 * kMHz, 0, "CW time signals; also 4.996, 14.996 MHz"},
                {"WWVB (USA)", 60 * kKHz, 0, "Longwave time code for radio clocks"},
                {"DCF77 (Germany)", 77.5 * kKHz, 0, "Longwave time code"},
                {"MSF (UK)", 60 * kKHz, 0, "Longwave time code"},
                {"JJY (Japan)", 40 * kKHz, 0, "Longwave time code; also 60 kHz"}}});
  v.push_back({"aviation_emergency", "Aviation and distress frequencies", "INTL",
               "Aircraft/ship station licence to transmit; anyone may listen", 0,
               "121.5 MHz is monitored by airliners and ATC worldwide (AM). Emergency beacons (ELT/EPIRB/PLB) "
               "transmit digitally on 406 MHz to satellites; older ones also on 121.5 MHz.",
               {{"Aeronautical emergency (guard)", 121.500 * kMHz, 25000, "AM voice; also homing signal of beacons"},
                {"Military emergency", 243.000 * kMHz, 25000, "AM voice"},
                {"SAR on-scene", 123.100 * kMHz, 25000, "Search-and-rescue coordination (AM)"},
                {"Air-to-air (North America)", 122.750 * kMHz, 25000, "AM"},
                {"COSPAS-SARSAT beacons", 406.0 * kMHz, 100000, "406.0-406.1 MHz, digital bursts (receive only)"},
                {"HF maritime distress", 2182 * kKHz, 2800, "USB voice"},
                {"HF maritime distress 4 MHz", 4125 * kKHz, 2800, "USB voice (GMDSS)"},
                {"HF maritime distress 6 MHz", 6215 * kKHz, 2800, "USB voice (GMDSS)"},
                {"HF maritime distress 8 MHz", 8291 * kKHz, 2800, "USB voice (GMDSS)"},
                {"HF maritime distress 12 MHz", 12290 * kKHz, 2800, "USB voice (GMDSS)"},
                {"HF maritime distress 16 MHz", 16420 * kKHz, 2800, "USB voice (GMDSS)"}}});
}

void add_ism(std::vector<BandService>& v) {
  BandService us{"ism_us915", "ISM 902-928 MHz (LoRa / Meshtastic)", "US,CA", "Licence-free (FCC Part 15)", 1.0,
                 "Up to 1 W conducted with up to 6 dBi antenna (4 W EIRP) for frequency-hopping or wideband digital "
                 "systems; no duty-cycle limit. LoRaWAN US915 uses 64 x 125 kHz uplink channels from 902.3 MHz "
                 "(200 kHz steps).",
                 {{"Meshtastic LongFast default", 906.875 * kMHz, 250000, "US default slot (SF11/250 kHz)"},
                  {"LoRaWAN US915 ch 0", 902.3 * kMHz, 125000, "first uplink channel"},
                  {"LoRaWAN US915 ch 8 (sub-band 2)", 903.9 * kMHz, 125000, "TTN / most gateways use sub-band 2 (903.9-905.3)"}}};
  v.push_back(us);
  v.push_back({"ism_eu868", "SRD 863-870 MHz (LoRa / Meshtastic)", "EU,UK", "Licence-free (ERC Rec 70-03 / EN 300 220)", 0.5,
               "Sub-band limits: 868.0-868.6 MHz 25 mW ERP, 1 % duty cycle; 868.7-869.2 MHz 25 mW, 0.1 %; "
               "869.4-869.65 MHz 500 mW ERP, 10 % duty cycle (the Meshtastic slot); 869.7-870.0 MHz 25 mW, 1 %. "
               "Duty cycle is per device per hour (1 % = 36 s/hour).",
               {{"Meshtastic default", 869.525 * kMHz, 250000, "869.4-869.65 MHz sub-band: 500 mW ERP, 10 % duty"},
                {"LoRaWAN EU868 ch 1", 868.1 * kMHz, 125000, "1 % duty"},
                {"LoRaWAN EU868 ch 2", 868.3 * kMHz, 125000, "1 % duty"},
                {"LoRaWAN EU868 ch 3", 868.5 * kMHz, 125000, "1 % duty"}}});
  v.push_back({"ism_eu433", "ISM 433.05-434.79 MHz", "EU,UK", "Licence-free", 0.01,
               "10 mW ERP with at most 10 % duty cycle. Shared with LPD433 voice radios and amateur 70 cm users.",
               {{"Band centre", 433.92 * kMHz, 0, "Common for sensors, remote controls"},
                {"LoRa APRS (amateur)", 433.775 * kMHz, 125000, "Amateur licence required"}}});
  v.push_back({"ism_au915", "LIPD 915-928 MHz (LoRa / Meshtastic)", "AU,NZ", "Licence-free (AU: LIPD class licence; NZ: GURL)", 1.0,
               "AU: up to 1 W EIRP for frequency-hopping / digital modulation systems. LoRaWAN AU915 mirrors US915 "
               "channel layout shifted to 915.2 MHz. New Zealand uses the same 915-928 MHz band (Meshtastic "
               "region ANZ); check local power limits.",
               {{"LoRaWAN AU915 ch 0", 915.2 * kMHz, 125000, ""}}});
  v.push_back({"ism_br915", "ISM 902-907.5 / 915-928 MHz (Brazil)", "BR", "Licence-free (Anatel, certified equipment)", 0,
               "Brazil allows spread-spectrum / digital devices in 902-907.5 MHz and 915-928 MHz (not 907.5-915). "
               "LoRaWAN in Brazil uses the AU915 plan; set Meshtastic / LoRa devices to a 915-928 MHz channel.",
               {{"LoRaWAN AU915 ch 0", 915.2 * kMHz, 125000, "AU915 plan, used in Brazil"},
                {"LoRaWAN AU915 ch 8", 916.8 * kMHz, 125000, "sub-band 2 (TTN default)"}}});
  v.push_back({"ism_za868", "SRD 863-870 MHz (LoRa / Meshtastic), South Africa", "ZA", "Licence-free (ICASA, type-approved equipment)", 0,
               "South Africa uses the 868 MHz short-range-device band and the LoRaWAN EU868 channel plan "
               "(Meshtastic region EU_868). Duty-cycle and power limits per sub-band follow the European pattern; "
               "keep transmissions short.",
               {{"LoRaWAN EU868 ch 1", 868.1 * kMHz, 125000, ""},
                {"LoRaWAN EU868 ch 2", 868.3 * kMHz, 125000, ""},
                {"LoRaWAN EU868 ch 3", 868.5 * kMHz, 125000, ""}}});
  v.push_back({"ism_as923", "AS923 (920-925 MHz)", "AS", "Licence-free, rules per country", 0,
               "Used across much of Asia (e.g. Singapore, Thailand, Malaysia, Japan variants). Power and "
               "listen-before-talk rules differ per country; many limit to about 16 dBm EIRP.",
               {{"LoRaWAN AS923 ch 1", 923.2 * kMHz, 125000, "default"},
                {"LoRaWAN AS923 ch 2", 923.4 * kMHz, 125000, "default"}}});
  v.push_back({"ism_in865", "IN865 (865-867 MHz)", "IN", "Licence-free (de-licensed band)", 1.0,
               "Up to 1 W transmitter power (4 W ERP) in the de-licensed 865-867 MHz band.",
               {{"LoRaWAN IN865 ch 1", 865.0625 * kMHz, 125000, "default"},
                {"LoRaWAN IN865 ch 2", 865.4025 * kMHz, 125000, "default"},
                {"LoRaWAN IN865 ch 3", 865.985 * kMHz, 125000, "default"}}});
  v.push_back({"ism_jp920", "JP 920 MHz (ARIB STD-T108)", "JP", "Licence-free (certified equipment)", 0.02,
               "About 920-928 MHz, typically 20 mW; carrier sense / listen-before-talk required. Only use "
               "radios certified for Japan (giteki mark).",
               {{"LoRaWAN AS923 ch 1", 923.2 * kMHz, 125000, "default (AS923-1 as used in Japan)"},
                {"LoRaWAN AS923 ch 2", 923.4 * kMHz, 125000, "default"}}});
  v.push_back({"ism_kr920", "KR 920 MHz", "KR", "Licence-free (certified equipment)", 0,
               "About 920.9-923.3 MHz; listen-before-talk required; low power limits.",
               {{"LoRaWAN KR920 ch 1", 922.1 * kMHz, 125000, "default"},
                {"LoRaWAN KR920 ch 2", 922.3 * kMHz, 125000, "default"},
                {"LoRaWAN KR920 ch 3", 922.5 * kMHz, 125000, "default"}}});
  v.push_back({"ism_2g4", "2.4 GHz ISM (Wi-Fi, Bluetooth, LoRa 2.4)", "INTL", "Licence-free", 0,
               "2400-2483.5 MHz. Power limits vary: typically 100 mW EIRP in Europe, up to 1 W conducted (+ antenna "
               "gain rules) in the US. Wi-Fi channels 1, 6 and 11 do not overlap.",
               {{"Wi-Fi ch 1", 2412 * kMHz, 20e6, ""}, {"Wi-Fi ch 6", 2437 * kMHz, 20e6, ""}, {"Wi-Fi ch 11", 2462 * kMHz, 20e6, ""}}});
}

std::vector<BandService> all_services() {
  std::vector<BandService> v;
  add_frs_gmrs(v);
  add_murs(v);
  add_cb(v);
  add_pmr_lpd(v);
  add_uhf_cb(v);
  add_marine(v);
  add_ham(v);
  add_weather_time_aviation(v);
  add_ism(v);
  return v;
}

}  // namespace

std::vector<std::string> band_region_tags(const std::string& region) { return tags_for(up(region)); }

std::vector<BandService> band_services(const std::string& region_in, const std::string& service) {
  std::vector<BandService> all = all_services();
  std::vector<std::string> tags = tags_for(up(region_in));
  std::vector<BandService> out;
  for (auto& s : all) {
    if (!service.empty() && s.id != service) continue;
    if (!tags.empty()) {
      bool hit = false;
      for (const auto& t : tags) hit = hit || region_has(s.region, t);
      if (!hit) continue;
    }
    out.push_back(std::move(s));
  }
  return out;
}

std::vector<std::string> band_regions() {
  return {"US", "CA", "EU", "UK", "AU", "NZ", "JP", "KR", "IN", "AS", "BR", "ZA", "R1", "R2", "R3", "INTL", "OTHER"};
}

std::vector<std::string> band_general_notes() {
  return {
      "In a genuine emergency where life is in immediate danger, most jurisdictions permit any means of "
      "radio communication to summon help (for example US 47 CFR 97.403 for amateur stations, and the "
      "distress provisions of the ITU Radio Regulations). This is general information, not legal advice; "
      "return to normal rules as soon as the emergency is over.",
      "Rules differ per country and change over time: check the local regulator before relying on a power "
      "or duty-cycle figure. Frequencies listed are channel centres (or dial frequencies where noted).",
      "Receive-only use (listening) is generally unrestricted; transmitting needs the licence noted per service."};
}

}  // namespace rf
}  // namespace ember
