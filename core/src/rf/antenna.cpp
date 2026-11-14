#include "rf/antenna.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "rf/rf_math.h"

namespace ember {
namespace rf {

namespace {

double lam_m(double f_mhz) { return kC / (f_mhz * 1e6); }

std::string mm(double m) {
  char b[32];
  std::snprintf(b, sizeof b, "%.0f mm", m * 1000.0);
  return b;
}

const char* kScavenge =
    "Scavenged conductors that work: electrical house wire (strip or leave insulation), speaker "
    "wire split into two, fence wire, copper pipe, aluminium tube/curtain rods, steel tape measure, "
    "coat hangers. Insulators: plastic bottles, PVC offcuts, nylon rope, dry wood.";
const char* kTrim = "Cut every element a little long, then trim in small steps (about 1 % at a time) "
                    "for lowest SWR at the centre of the band you need. Shorter = higher frequency.";

}  // namespace

// ---------------------------------------------------------------------------------------------
Design dipole(double f, double k) {
  Design d;
  double half = lam_m(f) / 2.0 * k;
  d.dims = {{"total", "A: total length (tip to tip)", half}, {"leg", "B: each leg (from feed point)", half / 2.0}};
  d.values = {{"wavelength", "Free-space wavelength", lam_m(f), "m", 3},
              {"feed_z", "Feed impedance (in free space)", 73, "ohm", 0}};
  d.diagram =
      "        B                     B\n"
      "  o===========o  ||  o===========o\n"
      "  insulator   feed point (coax)  insulator\n"
      "  |<---------------- A ---------------->|\n"
      "                  coax: centre to one leg, braid to the other\n";
  d.notes = {"Feed with 50 ohm coax directly; a choke (a few turns of coax at the feed) keeps RF off the coax shield.",
             "At typical heights the feed impedance is 50-75 ohm, a good match to 50 ohm coax.",
             "Mount at least a half wavelength high if possible; height matters more than perfection.", kTrim,
             kScavenge};
  return d;
}

Design inverted_v(double f, double k, double apex, double angle) {
  Design d = dipole(f, k);
  double leg = d.dims[1].m;
  double half_angle = angle / 2.0 * kPi / 180.0;
  double drop = leg * std::cos(half_angle);
  double spread = leg * std::sin(half_angle);
  double end_h = apex - drop;
  if (end_h < 0.3) {
    d.error = "The wire ends would be below 0.3 m: raise the apex or use a wider included angle.";
    return d;
  }
  d.dims.push_back({"apex", "H: apex (feed point) height", apex});
  d.dims.push_back({"end_height", "E: height of each wire end", end_h});
  d.dims.push_back({"span", "S: ground distance between the two ends", 2.0 * spread});
  d.values = {{"included_angle", "Included angle at apex", angle, "deg", 0},
              {"feed_z", "Approx. feed impedance", angle >= 120 ? 60.0 : 50.0, "ohm", 0}};
  d.diagram =
      "                 o  <- feed point / support, height H\n"
      "               /   \\\n"
      "          B  /       \\  B\n"
      "           /  angle    \\\n"
      "         o               o   <- ends at height E (keep out of reach!)\n"
      "  _______|_______S_______|________ ground\n";
  d.notes = {"Needs only one tall support; the ends tie off to stakes, fences or bushes with rope.",
             "Lengths start at the flat-dipole value: an inverted V usually ends up 2-5 % shorter after trimming.",
             "Keep the included angle 90-120 degrees; narrower angles lower the feed impedance and the efficiency.",
             "The wire ends carry high RF voltage: keep them out of reach of people.", kTrim};
  return d;
}

Design ground_plane(double f, double k, double droop, int radials) {
  Design d;
  double lam = lam_m(f);
  double rad = lam / 4.0 * k;
  double radial = lam / 4.0 * 1.0;  // radials slightly longer than the radiator
  double r = droop * kPi / 180.0;
  // Rough feed impedance vs. droop: 0° ≈ 36 Ω, 30° ≈ 45 Ω, 45° ≈ 50 Ω, 90° ≈ 73 Ω (vertical dipole).
  double z;
  if (droop <= 30) z = 36 + (45 - 36) * droop / 30.0;
  else if (droop <= 45) z = 45 + (50 - 45) * (droop - 30) / 15.0;
  else z = 50 + (73 - 50) * (droop - 45) / 45.0;
  d.dims = {{"radiator", "A: vertical radiator", rad},
            {"radial", "B: each radial", radial},
            {"radial_drop", "C: vertical drop of each radial tip", radial * std::sin(r)},
            {"radial_reach", "D: horizontal reach of each radial", radial * std::cos(r)}};
  d.values = {{"feed_z", "Approx. feed impedance", z, "ohm", 0},
              {"radials", "Radials", static_cast<double>(radials), "", 0},
              {"droop", "Radial droop below horizontal", droop, "deg", 0}};
  d.diagram =
      "            |\n"
      "            |  A  (centre conductor)\n"
      "            |\n"
      "      ======O======   <- SO-239 / chassis socket or plate\n"
      "     /     coax     \\    B radials (braid), drooped\n"
      "    /                \\   C = drop, D = reach\n";
  d.notes = {"Radiator to the coax centre, all radials joined to the braid.",
             "Drooping the radials 30-45 degrees raises the feed impedance towards 50 ohm.",
             "3-4 radials are enough; they need not be exact, the radiator sets the frequency.",
             "A chassis-mount socket on a scrap of sheet metal or a tin lid makes a good hub.", kTrim,
             kScavenge};
  return d;
}

Design j_pole(double f, double k) {
  Design d;
  double lam = lam_m(f);
  d.dims = {{"long", "A: long element (overall)", 0.75 * lam * k},
            {"stub", "B: short element (matching stub)", 0.25 * lam * k},
            {"radiator", "R: radiating half-wave above the stub top", 0.5 * lam * k},
            {"feed_tap", "C: feed tap above the shorting bar (start)", 0.02 * lam},
            {"spacing", "D: centre-to-centre spacing", 0.0125 * lam}};
  d.values = {{"gain", "Approx. gain", 2.1, "dBi", 1}};
  d.diagram =
      "   |  ^\n"
      "   |  |\n"
      "   |  R (half wave)\n"
      "   |  |\n"
      "   |  v\n"
      "   |      |  ^\n"
      "   |      |  B (quarter-wave stub)\n"
      " A |      |  |\n"
      "   +-o--o-+  v   <- feed taps at height C (centre to long side, braid to short side)\n"
      "   |      |\n"
      "   +------+  <- shorting bar; D = spacing between the two sides\n"
      "   |  (mount here: the bar is at DC ground potential)\n";
  d.notes = {"Slide both feed taps up/down together for lowest SWR, then trim the long element for frequency.",
             "Copper pipe with a soldered U bar, aluminium tube, or 300 ohm twin-lead (as a roll-up J) all work.",
             "Put a choke (a few turns of coax) right below the feed point; J-poles are prone to feed-line radiation.",
             "Spacing is not critical; it sets the stub impedance, the feed tap absorbs the difference.", kScavenge};
  return d;
}

Design slim_jim(double f, double k) {
  Design d;
  double lam = lam_m(f);
  d.dims = {{"overall", "A: overall height", 0.75 * lam * k},
            {"radiator", "R: folded half-wave (top section)", 0.5 * lam * k},
            {"stub", "B: quarter-wave matching section", 0.25 * lam * k},
            {"gap", "G: gap in the inner side (start)", 0.012 * lam},
            {"feed_tap", "C: feed tap above the bottom bar (start)", 0.025 * lam},
            {"spacing", "D: spacing between the two sides", 0.0125 * lam}};
  d.values = {{"gain", "Approx. gain", 2.1, "dBi", 1}};
  d.diagram =
      "   +------+  <- closed top\n"
      "   |      |\n"
      "   |      |  R (folded half wave)\n"
      "   |      |\n"
      "   |      =  <- gap G in the inner side\n"
      " A |      |\n"
      "   |      |  B (matching section)\n"
      "   +-o--o-+  <- feed taps at height C\n"
      "   |      |\n"
      "   +------+  <- bottom bar; D = spacing\n";
  d.notes = {"Classic build: one length of 300/450 ohm ladder line or bent copper wire/tube.",
             "Adjust the feed tap for SWR first, then the overall length for frequency.",
             "Hang it from a tree with fishing line; run the coax away horizontally for at least a quarter wave if possible.",
             kTrim};
  return d;
}

Design five_eighths_vertical(double f, double k) {
  Design d;
  double lam = lam_m(f);
  d.dims = {{"radiator", "A: radiator (5/8 wave)", 0.625 * lam * k},
            {"radial", "B: each radial (4)", 0.25 * lam}};
  d.values = {{"gain", "Approx. gain over a 1/4 wave", 1.5, "dB", 1}};
  d.diagram =
      "        |\n"
      "        |  A\n"
      "        |\n"
      "       (O)  <- base loading coil (series inductor)\n"
      "    ====O====  radials B\n"
      "       coax\n";
  d.notes = {"A bare 5/8 wave has a capacitive feed impedance: it needs a series base loading coil to cancel the reactance.",
             "Start with a few turns of stiff wire and squeeze/stretch (or tap) the coil for lowest SWR.",
             "The extra gain over a quarter wave only appears with a good ground plane and a clear horizon.", kTrim};
  return d;
}

// ---------------------------------------------------------------------------------------------
Design yagi(double f, int n, double diam_mm) {
  Design d;
  double lam = lam_m(f);
  // P. Viezbicke, "Yagi Antenna Design", NBS Technical Note 688 (1976), Table 1. d/λ = 0.0085,
  // reflector spacing 0.2 λ. Lengths in wavelengths. Driven element: resonant dipole (start 0.47 λ).
  struct Row { int n; double boom; double dir_spacing; double gain_dbi; double refl; double dirs[4]; int ndir; };
  static const Row rows[] = {{3, 0.4, 0.20, 9.25, 0.482, {0.442, 0, 0, 0}, 1},
                             {5, 0.8, 0.20, 11.35, 0.482, {0.428, 0.424, 0.428, 0}, 3},
                             {6, 1.2, 0.25, 12.35, 0.482, {0.428, 0.420, 0.420, 0.428}, 4}};
  const Row* r = nullptr;
  for (const auto& x : rows)
    if (x.n == n) r = &x;
  if (!r) {
    d.error = "Supported element counts: 3, 5 or 6 (published NBS TN-688 designs).";
    return d;
  }
  d.dims.push_back({"reflector", "R: reflector", r->refl * lam});
  d.dims.push_back({"driven", "DE: driven element (start, then trim)", 0.47 * lam});
  for (int i = 0; i < r->ndir; ++i) {
    char key[8], label[48];
    std::snprintf(key, sizeof key, "d%d", i + 1);
    std::snprintf(label, sizeof label, "D%d: director %d", i + 1, i + 1);
    d.dims.push_back({key, label, r->dirs[i] * lam});
  }
  d.dims.push_back({"s_refl", "S1: reflector to driven element", 0.2 * lam});
  d.dims.push_back({"s_dir", "S2: spacing between driven/directors", r->dir_spacing * lam});
  d.dims.push_back({"boom", "Boom length (reflector to last director)", r->boom * lam});
  d.dims.push_back({"ideal_diam", "Element diameter used by the design (0.0085 lambda)", 0.0085 * lam});
  d.values = {{"gain_dbi", "Gain (NBS measured)", r->gain_dbi, "dBi", 2},
              {"gain_dbd", "Gain over a dipole", r->gain_dbi - 2.15, "dBd", 2}};
  std::string dia = "  R      DE";
  for (int i = 0; i < r->ndir; ++i) dia += "      D" + std::to_string(i + 1);
  dia += "\n  |      |";
  for (int i = 0; i < r->ndir; ++i) dia += "      | ";
  dia += "\n  +======+=";
  for (int i = 0; i < r->ndir; ++i) dia += "=====+=";
  dia += "==>  beam direction\n  |      |";
  for (int i = 0; i < r->ndir; ++i) dia += "      | ";
  dia += "\n  |<-S1->|<-S2->| ...   (all elements centred on the boom, in one plane)\n";
  d.diagram = dia;
  double ratio = diam_mm / 1000.0 / lam;
  if (ratio < 0.0085 / 2 || ratio > 0.0085 * 2) {
    char b[200];
    std::snprintf(b, sizeof b,
                  "Your element diameter (%.1f mm) differs a lot from the design value (%s). Thinner elements must be "
                  "slightly longer, thicker ones shorter: cut long and check performance.",
                  diam_mm, mm(0.0085 * lam).c_str());
    d.notes.push_back(b);
  }
  d.notes.push_back("A metal boom shortens the effective length of elements that pass through it; insulate the elements or use a wooden/PVC boom.");
  d.notes.push_back("The driven element of a Yagi has a low feed impedance (20-30 ohm): use a hairpin or gamma match, or a folded dipole, for a good 50 ohm match.");
  d.notes.push_back("Mount with elements horizontal for SSB/weak-signal, vertical for FM repeaters and most handheld traffic.");
  d.notes.push_back("Source: P. Viezbicke, Yagi Antenna Design, NBS Technical Note 688 (1976).");
  return d;
}

Design tape_yagi(double f) {
  Design d;
  // WB2HOL "Tape Measure Beam" (Joe Leggio, published for 146.565 MHz), dimensions in inches.
  const double f0 = 146.565, in = 0.0254;
  double s = f0 / f;
  d.dims = {{"reflector", "R: reflector (one piece)", 41.375 * in * s},
            {"driven_half", "DE: each driven-element half", 17.75 * in * s},
            {"director", "D: director (one piece)", 35.125 * in * s},
            {"boom_rd", "S1: boom section reflector to driven", 7.0 * in * s},
            {"boom_dd", "S2: boom section driven to director", 11.5 * in * s},
            {"hairpin", "H: hairpin wire length (formed into a U)", 4.5 * in * s},
            {"halves_gap", "G: gap between driven halves (start)", 1.0 * in * s}};
  d.values = {{"gain", "Approx. gain", 7, "dBi", 0}};
  d.diagram =
      "   R          DE (two halves)        D\n"
      "   |            |    |               |\n"
      "   |            |    |               |\n"
      "   +====S1======+-HH-+=====S2========+==>  beam\n"
      "   |            | ^  |               |\n"
      "   |            | hairpin U across the feed, coax centre to one half,\n"
      "   |            |      braid to the other (G = gap)\n";
  d.notes = {"Elements are cut from a 1 in (25 mm) steel tape measure; PVC pipe tees/crosses form the boom, hose clamps hold the elements.",
             "Cut the tape with tin snips and cover the sharp ends with tape. Sand the paint off where the coax attaches.",
             "Excellent for radio direction finding (fox hunting): the rear null is sharp. Flexible elements survive knocks.",
             "Source: WB2HOL tape measure beam (146.565 MHz design), scaled linearly to your frequency."};
  if (f > 300) {
    d.notes.push_back("At 70 cm the tape width is large relative to the wavelength: the scaled elements tend to come out "
                      "long. Cut as given and trim all elements by the same percentage while checking SWR/gain.");
  }
  return d;
}

Design moxon(double f, double wire_mm) {
  Design d;
  double lam = lam_m(f);
  double dl = wire_mm / 1000.0 / lam;
  if (dl < 1e-5 || dl > 1e-2) {
    d.error = "Wire diameter must be between 0.00001 and 0.01 wavelengths for the Moxon formulas.";
    return d;
  }
  // Moxon dimensions after L. B. Cebik, W4RNL (the regression commonly implemented in MoxGen), in wavelengths.
  double d1 = 0.4342945 * std::log(dl);
  double A = -0.0008571428571 * d1 * d1 - 0.009571428571 * d1 + 0.3398571429;
  double B = -0.002142857143 * d1 * d1 - 0.02035714286 * d1 + 0.008285714286;
  double C = 0.001809523381 * d1 * d1 + 0.01780952423 * d1 + 0.05164285714;
  double D = 0.001 * d1 + 0.07178571429;
  double E = B + C + D;
  d.dims = {{"A", "A: width (driver and reflector)", A * lam},
            {"B", "B: driver tail (each side)", B * lam},
            {"C", "C: gap between tails", C * lam},
            {"D", "D: reflector tail (each side)", D * lam},
            {"E", "E: depth (B + C + D)", E * lam}};
  d.values = {{"gain", "Approx. gain", 5.8, "dBi", 1}, {"feed_z", "Feed impedance", 50, "ohm", 0}};
  d.diagram =
      "   |<------------------ A ------------------>|\n"
      "   +--------------------o--------------------+   driver (feed in the centre)\n"
      "   |  B                                    B |\n"
      "   |                                         |\n"
      "   x  C (gap, insulated: fishing line)       x\n"
      "   |  D                                    D |\n"
      "   +-----------------------------------------+   reflector (continuous)\n"
      "   E = total depth                ==> beam direction: towards the driver\n";
  d.notes = {"Feeds 50 ohm coax directly; excellent front-to-back ratio, about 2 elements of Yagi gain.",
             "Spreaders: fibreglass rods, PVC, bamboo or dry wood in an X shape.",
             "The gap C is the critical dimension: keep it accurate.",
             "Source: after L. B. Cebik, W4RNL; the regression commonly implemented in MoxGen."};
  return d;
}

Design full_wave_loop(double f, int shape, double vf) {
  Design d;
  double lam = lam_m(f);
  double perim = 1005.0 / f * kMetersPerFoot;  // classic 1005/f ft driven-loop rule
  double refl = 1030.0 / f * kMetersPerFoot;
  double match = lam / 4.0 * vf;
  d.dims.push_back({"perimeter", "P: loop perimeter (driven)", perim});
  if (shape == 0) {
    d.dims.push_back({"side", "S: each side (square)", perim / 4.0});
    d.dims.push_back({"spreader", "X: spreader arm, centre to corner", perim / 4.0 / std::sqrt(2.0)});
    d.diagram =
        "        +-----S-----+\n"
        "        |           |\n"
        "        S   quad    S\n"
        "        |           |\n"
        "        +-----o-----+   <- feed in the centre of the bottom side\n"
        "              | 75 ohm quarter-wave match, then 50 ohm coax\n";
  } else {
    d.dims.push_back({"side", "S: each side (equilateral delta)", perim / 3.0});
    d.dims.push_back({"height", "Height of the triangle", perim / 3.0 * std::sqrt(3.0) / 2.0});
    d.diagram =
        "              /\\\n"
        "             /  \\\n"
        "          S /    \\ S\n"
        "           /      \\\n"
        "          /___o____\\   <- feed in the centre of the bottom side\n"
        "               S\n";
  }
  d.dims.push_back({"reflector", "Optional reflector loop perimeter", refl});
  d.dims.push_back({"match", "M: 75 ohm quarter-wave matching section", match});
  d.values = {{"feed_z", "Feed impedance (approx.)", 110, "ohm", 0}, {"gain", "Approx. gain", 3.1, "dBi", 1}};
  d.notes = {"Feed impedance is about 100-125 ohm: a quarter wave of 75 ohm coax (RG-6/RG-59 TV cable) transforms it to about 50 ohm.",
             "Feeding at the bottom centre gives horizontal polarisation; feeding at a side centre gives vertical.",
             "A horizontal full-wave loop near the ground is also a good quiet NVIS receiving/transmitting antenna.",
             "Source: the 1005/f (driven) and 1030/f (reflector) feet rules from ARRL quad literature.", kTrim};
  return d;
}

Design end_fed_half_wave(double f, double k) {
  Design d;
  double lam = lam_m(f);
  d.dims = {{"wire", "A: radiator wire", lam / 2.0 * k},
            {"counterpoise", "B: counterpoise (short wire at the transformer)", 0.05 * lam}};
  d.values = {{"feed_z", "Feed impedance at the end", 2500, "ohm", 0},
              {"ratio", "Transformer impedance ratio", 49, ":1", 0}};
  d.diagram =
      "  [49:1]===========================A=============================o  far end (high voltage!)\n"
      "    |  \\_ B counterpoise\n"
      "   coax to radio (add a choke a short distance down the coax)\n";
  d.notes = {"The end of a half-wave wire is about 2000-5000 ohm: use a 49:1 (or 64:1) unun, e.g. 2:14 turns on a type 43 ferrite toroid.",
             "A half wave on the lowest band is also resonant on even harmonics (40 m wire works 20/15/10 m).",
             "Easy to deploy from a single tree: throw a line over a branch, the transformer stays at ground level.",
             "Both ends carry high RF voltage; keep them out of reach.", kTrim};
  return d;
}

static const HfBand kHfBands[] = {{"160 m", 1.8, 2.0},     {"80 m", 3.5, 4.0},      {"60 m", 5.3, 5.41},
                                  {"40 m", 7.0, 7.3},      {"30 m", 10.1, 10.15},   {"20 m", 14.0, 14.35},
                                  {"17 m", 18.068, 18.168}, {"15 m", 21.0, 21.45},  {"12 m", 24.89, 24.99},
                                  {"10 m", 28.0, 29.7}};
const HfBand* hf_band_list(size_t* n) {
  *n = sizeof(kHfBands) / sizeof(kHfBands[0]);
  return kHfBands;
}

Design random_wire(double lo, double hi, unsigned mask, double vf) {
  Design d;
  if (hi <= lo) { d.error = "Maximum length must be larger than minimum length."; return d; }
  if (hi - lo > 200) { d.error = "Search range is limited to 200 m."; return d; }
  size_t nb;
  const HfBand* bands = hf_band_list(&nb);
  if ((mask & ((1u << nb) - 1)) == 0) { d.error = "Select at least one band."; return d; }
  // Forbidden zones: within ±4 % of any half-wave multiple on any selected band (very high impedance).
  struct Zone { double a, b; };
  std::vector<Zone> zones;
  for (size_t i = 0; i < nb; ++i) {
    if (!(mask & (1u << i))) continue;
    double h_short = kC / (bands[i].hi_mhz * 1e6) / 2.0 * vf;
    double h_long = kC / (bands[i].lo_mhz * 1e6) / 2.0 * vf;
    for (int n = 1; n * h_short * 0.96 <= hi; ++n) zones.push_back({n * h_short * 0.96, n * h_long * 1.04});
  }
  // Sweep in 5 cm steps; collect free windows.
  struct Win { double a, b; };
  std::vector<Win> wins;
  const double step = 0.05;
  double start = -1;
  int steps = static_cast<int>((hi - lo) / step) + 1;
  for (int s = 0; s <= steps; ++s) {
    double x = lo + s * step;
    bool bad = x > hi;
    for (const auto& z : zones)
      if (x >= z.a && x <= z.b) { bad = true; break; }
    if (!bad && start < 0) start = x;
    if ((bad || s == steps) && start >= 0) {
      double end = bad ? x - step : x;
      if (end - start >= 0.3) wins.push_back({start, std::min(end, hi)});
      start = -1;
    }
  }
  std::sort(wins.begin(), wins.end(), [](const Win& a, const Win& b) { return (a.b - a.a) > (b.b - b.a); });
  if (wins.size() > 8) wins.resize(8);
  std::sort(wins.begin(), wins.end(), [](const Win& a, const Win& b) { return a.a < b.a; });
  if (wins.empty()) {
    d.error = "No length in this range avoids every selected band; widen the range or drop a band.";
    return d;
  }
  std::string t = "Good lengths (centre of each window, with the usable window):\n";
  int i = 0;
  for (const auto& w : wins) {
    char key[16], label[96], line[128];
    std::snprintf(key, sizeof key, "good%d", ++i);
    std::snprintf(label, sizeof label, "Good length %d (window %.1f-%.1f m)", i, w.a, w.b);
    d.dims.push_back({key, label, (w.a + w.b) / 2.0});
    std::snprintf(line, sizeof line, "  %6.2f m  (%5.1f ft)   window %.2f - %.2f m\n", (w.a + w.b) / 2.0,
                  (w.a + w.b) / 2.0 / kMetersPerFoot, w.a, w.b);
    t += line;
  }
  t += "\n  [tuner/9:1 unun]==== random wire ========o\n        |\n      ground / counterpoise\n";
  d.diagram = t;
  d.notes = {"A random wire avoids lengths near half-wave multiples, where the end impedance is too high for most tuners and 9:1 ununs.",
             "Use with an antenna tuner or a 9:1 unun plus a counterpoise or ground rod; the coax shield becomes part of the antenna unless choked.",
             "Windows exclude +/-4 % around every half-wave multiple on the selected bands.",
             "Longer is generally better on the low bands; keep the far end high and clear."};
  return d;
}

Design nvis_dipole(double f, double k) {
  Design d = dipole(f, k);
  double lam = lam_m(f);
  d.dims.push_back({"h_min", "Minimum height (0.1 lambda)", 0.10 * lam});
  d.dims.push_back({"h_opt", "Good height (0.15 lambda)", 0.15 * lam});
  d.dims.push_back({"h_max", "Maximum useful height (0.25 lambda)", 0.25 * lam});
  d.dims.push_back({"reflector", "Optional ground reflector wire, 5 % longer, laid under it", d.dims[0].m * 1.05});
  d.diagram =
      "  o==============o||o==============o   flat dipole, height 0.1-0.25 lambda\n"
      "  |                                |\n"
      "  |  radiation goes straight up and |\n"
      "  |  falls back within ~0-400 km    |\n"
      "  .....reflector wire on the ground (optional).....\n";
  d.notes = {"NVIS (near-vertical incidence skywave) fills the skip zone: 0-400 km coverage over mountains and valleys.",
             "Use frequencies below the critical frequency: typically 5-8 MHz by day, 2-4 MHz at night.",
             "Low heights (a few metres at 40/60 m) are fine and even help; a wire on the ground below adds ~2-3 dB.",
             "Sources: US Army FM 24-18 and NVIS literature (Fiedler & Farmer)."};
  return d;
}

Design coaxial_collinear(double f, int sections, double vf) {
  Design d;
  double lam = lam_m(f);
  double sec = lam / 2.0 * vf;
  double whip = lam / 4.0 * 0.95;
  double sleeve = lam / 4.0 * 0.95;
  d.dims = {{"section", "S: each coax section (shield-to-centre transposed)", sec},
            {"top_whip", "W: top quarter-wave whip (centre conductor)", whip},
            {"sleeve", "Q: bottom quarter-wave decoupling sleeve/choke", sleeve},
            {"total", "Total radiating length", sections * sec + whip}};
  d.values = {{"sections", "Sections", static_cast<double>(sections), "", 0},
              {"gain_ideal", "Ideal gain (10 log N)", 10.0 * std::log10(static_cast<double>(sections)) + 2.15, "dBi", 1}};
  d.diagram =
      "   |   W (top whip, centre conductor only)\n"
      "  [=]  S  <- each joint: centre of one piece soldered to the braid of the next\n"
      "  [=]  S\n"
      "  [=]  S     (N sections)\n"
      "  [ ]  Q decoupling sleeve / choke\n"
      "   |   feed coax\n";
  d.notes = {"Velocity factor of the coax used for the sections sets their length: check the cable (solid PE 0.66, foam 0.78-0.85).",
             "Real coaxial collinears achieve 1-3 dB less than the ideal figure; more than ~8 sections rarely helps.",
             "Seal it in a PVC tube; water in the coax ruins it.", kTrim};
  return d;
}

Design coax_choke(double f, double form_mm, double od_mm, int turns) {
  Design d;
  const double inch = 25.4;
  double r_in = (form_mm + od_mm) / 2.0 / inch;   // mean coil radius
  double len_in = turns * od_mm / inch;           // close-wound
  double L_uh = r_in * r_in * turns * turns / (9.0 * r_in + 10.0 * len_in);  // Wheeler
  double X = 2.0 * kPi * f * 1e6 * L_uh * 1e-6;
  double coax_len = turns * kPi * (form_mm + od_mm) / 1000.0;
  d.dims = {{"coax_needed", "Coax needed for the coil (plus leads)", coax_len},
            {"coil_length", "Coil length (close-wound)", len_in * inch / 1000.0}};
  d.values = {{"inductance", "Inductance (Wheeler)", L_uh, "uH", 2},
              {"reactance", "Choking reactance at f", X, "ohm", 0}};
  d.diagram =
      "  antenna feed --(((((((((()))-- to radio\n"
      "                  N turns of the coax itself on a form (bottle, PVC pipe)\n";
  d.notes.push_back(X >= 500 ? "Reactance is at least 500 ohm: a useful choke at this frequency."
                             : "Reactance is below 500 ohm: add turns or use a larger form.");
  double lam = lam_m(f);
  if (coax_len > 0.25 * lam)
    d.notes.push_back("The coiled coax is longer than a quarter wave: self-resonance may be close to (or below) this frequency and the choke can misbehave. Prefer ferrite.");
  d.notes.push_back("Ferrite (type 31 or 43 clamp-on cores, or several turns through a type 31 toroid) chokes much better over a wide range than air coils.");
  d.notes.push_back("Place the choke at the antenna feed point.");
  return d;
}

Design coax_balun(double f, int kind, double vf, double sleeve_vf) {
  Design d;
  double lam = lam_m(f);
  if (kind == 0) {
    d.dims = {{"sleeve", "Q: quarter-wave sleeve length", lam / 4.0 * sleeve_vf}};
    d.diagram =
        "   to antenna halves\n"
        "     |      |\n"
        "  ===o======o  <- top: sleeve is OPEN (not connected) at the antenna end\n"
        "  |[ sleeve ]|  Q long, made of braid or foil over the coax jacket\n"
        "  |[        ]|\n"
        "  ===+======   <- bottom: sleeve soldered to the coax braid\n"
        "     | coax\n";
    d.notes = {"1:1 sleeve (bazooka) balun: the quarter-wave sleeve presents high impedance to common-mode current.",
               "Sleeve velocity factor depends on the gap dielectric: ~0.95 air-spaced; noticeably less over a PVC jacket. Check with an analyser if possible.",
               "Narrow-band: use one per band. For HF a ferrite choke is more practical."};
  } else {
    d.dims = {{"loop", "L: half-wave coax delay loop", lam / 2.0 * vf}};
    d.diagram =
        "   balanced 200 ohm antenna (e.g. folded dipole)\n"
        "     A                        B\n"
        "     |                        |\n"
        "     o---- centre of feed coax  o---- one end of loop centre\n"
        "     |____ loop (L) other end ___|\n"
        "     braids of feed coax and both loop ends joined together (not to the antenna)\n";
    d.notes = {"4:1 coax balun: the half-wave loop inverts the phase so the two antenna terminals are driven in antiphase.",
               "Transforms 50 ohm unbalanced to 200 ohm balanced: feeds folded dipoles and some Yagi drivers.",
               "Use the velocity factor of the actual coax used for the loop."};
  }
  d.values = {{"wavelength", "Free-space wavelength", lam, "m", 3}};
  return d;
}

Design cantenna(double f, double diam_mm) {
  Design d;
  double lam = lam_m(f);
  double D = diam_mm / 1000.0;
  double fc_te11 = 1.8412 * kC / (kPi * D) / 1e6;   // MHz
  double fc_tm01 = 2.4049 * kC / (kPi * D) / 1e6;
  if (f <= fc_te11 * 1.05) {
    char b[160];
    std::snprintf(b, sizeof b, "Can too narrow: TE11 cutoff is %.0f MHz. Use a wider can (more than %.0f mm).", fc_te11,
                  1.8412 * kC / (kPi * f * 1e6) * 1000 * 1.05);
    d.error = b;
    return d;
  }
  if (f >= fc_tm01) {
    char b[160];
    std::snprintf(b, sizeof b, "Can too wide: the TM01 mode starts at %.0f MHz. Use a can narrower than %.0f mm.", fc_tm01,
                  2.4049 * kC / (kPi * f * 1e6) * 1000);
    d.error = b;
    return d;
  }
  double lc = kC / (fc_te11 * 1e6);
  double lg = lam / std::sqrt(1.0 - (lam / lc) * (lam / lc));
  d.dims = {{"probe_pos", "P: probe distance from the closed end (lambda_g/4)", lg / 4.0},
            {"probe_len", "H: probe height inside the can (lambda/4)", lam / 4.0},
            {"min_can", "Minimum can length (3/4 lambda_g)", 0.75 * lg}};
  d.values = {{"cutoff", "TE11 cutoff frequency", fc_te11, "MHz", 0},
              {"guide_wl", "Guide wavelength", lg * 1000.0, "mm", 1},
              {"gain", "Approx. gain", 10, "dBi", 0}};
  d.diagram =
      "   closed end                          open end\n"
      "   |=====================================\n"
      "   |        |  H (probe: N-socket centre pin + wire)\n"
      "   |        |                              ==>  point at the other station\n"
      "   |=====================================\n"
      "   |<- P ->|\n";
  d.notes = {"Waveguide antenna for 2.4 GHz Wi-Fi: a straight-sided metal can (coffee, juice) with a chassis-mount N or SMA socket.",
             "Keep the coax short (LMR-400 or a short pigtail): coax loss at 2.4 GHz is large.",
             "Polarisation follows the probe: match the other end."};
  return d;
}

Design biquad(double f) {
  Design d;
  double lam = lam_m(f);
  d.dims = {{"side", "S: each side of the two squares (lambda/4)", lam / 4.0},
            {"wire", "Total wire length (8 sides)", 2.0 * lam},
            {"reflector", "R: square reflector side (about 1 lambda)", lam},
            {"spacing", "H: height of the element above the reflector (lambda/8)", lam / 8.0}};
  d.values = {{"gain", "Approx. gain", 10.5, "dBi", 1}};
  d.diagram =
      "      /\\    /\\\n"
      "     /  \\  /  \\      two squares of side S, joined at the centre (feed)\n"
      "     \\  /  \\  /      centre conductor to one corner pair, braid to the other\n"
      "      \\/    \\/\n"
      "   ===========  reflector R x R (PCB, tin sheet, old CD case with foil), H below\n";
  d.notes = {"Use 2.5 mm^2 (14 AWG) solid copper house wire; the feed passes through the reflector centre (copper tube for the braid).",
             "Gain about 10-11 dBi with a beamwidth of roughly 60 degrees: easier to aim than a cantenna.",
             "Polarisation: squares side by side horizontally = vertical polarisation."};
  return d;
}

Design lora_quarter_wave(double f, double k) {
  Design d = ground_plane(f, k, 45, 4);
  d.dims.push_back({"dipole_half", "Each half of a half-wave dipole alternative", lam_m(f) / 4.0 * 0.95});
  d.notes.insert(d.notes.begin(),
                 "For LoRa/Meshtastic: solder the radiator to the SMA/u.FL centre pin; a quarter wave of stiff wire straight from the board works but a proper ground plane is much better.");
  d.notes.insert(d.notes.begin() + 1,
                 "Never transmit without an antenna attached: it can destroy the LoRa module's output stage.");
  return d;
}

}  // namespace rf
}  // namespace ember
