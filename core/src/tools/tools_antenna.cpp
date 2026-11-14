// Antenna designer tools (category "antenna"). Math lives in rf/antenna.cpp.
#include <cmath>
#include <cstdlib>

#include "rf/antenna.h"
#include "rf/rf_math.h"
#include "tools/tool_impl.h"

namespace ember {
namespace tools {
namespace detail {
namespace {

using rf::Design;

Field freq(double def, double lo, double hi) {
  return num("freq_mhz", "Frequency", "MHz").def(def).range(lo, hi).help("Centre of the frequency range you will use.");
}
Field end_factor() {
  return num("end_factor", "Shortening factor", "").def(0.95).range(0.80, 1.0).step(0.005).adv()
      .help("0.95 = bare thin wire (468/f ft rule); insulated wire ~0.93-0.94; thick tubing slightly lower.");
}

void emit(Ctx& c, const Design& d) {
  if (!d.ok()) { c.fail(d.error); return; }
  std::string table;
  for (const auto& x : d.dims) {
    c.out_len(x.key, x.label, x.m);
    table += "  " + pad_right(x.label, 58) + pad_right(fmt_metric_len(x.m), 12) + fmt_imperial_len(x.m) + "\n";
  }
  for (const auto& v : d.values) {
    c.out(v.key, v.label, v.v, v.unit, v.decimals);
  }
  c.r.text = d.diagram + "\nCut list:\n" + table;
  for (const auto& n : d.notes) c.note(n);
}

// ---- definitions ----
ToolDef d_dipole() {
  return {"dipole", "Half-wave dipole", "antenna",
          "Classic centre-fed wire dipole: total and per-leg lengths, build diagram and tips.",
          {freq(146.0, 0.1, 3000), end_factor()}};
}
void r_dipole(Ctx& c) {
  double f, k;
  if (!c.num("freq_mhz", &f) || !c.num("end_factor", &k)) return;
  emit(c, rf::dipole(f, k));
}

ToolDef d_inv_v() {
  return {"inverted_v", "Inverted-V dipole", "antenna",
          "Dipole hung from one central support: leg length, end heights and footprint.",
          {freq(7.1, 0.5, 1000),
           num("apex_m", "Apex height", "m").def(10).positive(200),
           num("angle_deg", "Included angle at apex", "deg").def(110).range(60, 170),
           end_factor()}};
}
void r_inv_v(Ctx& c) {
  double f, h, a, k;
  if (!c.num("freq_mhz", &f) || !c.num("apex_m", &h) || !c.num("angle_deg", &a) || !c.num("end_factor", &k)) return;
  emit(c, rf::inverted_v(f, k, h, a));
}

ToolDef d_gp() {
  return {"ground_plane", "Quarter-wave ground plane", "antenna",
          "Vertical quarter-wave with drooping radials; feed impedance estimate vs droop angle.",
          {freq(146.0, 1, 3000),
           num("droop_deg", "Radial droop below horizontal", "deg").def(45).range(0, 60),
           num("radials", "Number of radials", "").def(4).range(2, 16).whole(),
           end_factor()}};
}
void r_gp(Ctx& c) {
  double f, dr, k;
  int n;
  if (!c.num("freq_mhz", &f) || !c.num("droop_deg", &dr) || !c.integer("radials", &n) || !c.num("end_factor", &k)) return;
  emit(c, rf::ground_plane(f, k, dr, n));
}

ToolDef d_jpole() {
  return {"j_pole", "J-pole", "antenna", "End-fed half wave with quarter-wave matching stub (copper pipe, twin-lead).",
          {freq(146.0, 10, 1500), end_factor()}};
}
void r_jpole(Ctx& c) {
  double f, k;
  if (!c.num("freq_mhz", &f) || !c.num("end_factor", &k)) return;
  emit(c, rf::j_pole(f, k));
}

ToolDef d_slimjim() {
  return {"slim_jim", "Slim Jim", "antenna", "Folded J-pole variant, easy to make from ladder line.",
          {freq(146.0, 10, 1500), end_factor()}};
}
void r_slimjim(Ctx& c) {
  double f, k;
  if (!c.num("freq_mhz", &f) || !c.num("end_factor", &k)) return;
  emit(c, rf::slim_jim(f, k));
}

ToolDef d_58() {
  return {"vertical_5_8", "5/8-wave vertical", "antenna", "5/8-wave radiator with base loading coil and radials.",
          {freq(146.0, 10, 1500), end_factor()}};
}
void r_58(Ctx& c) {
  double f, k;
  if (!c.num("freq_mhz", &f) || !c.num("end_factor", &k)) return;
  emit(c, rf::five_eighths_vertical(f, k));
}

ToolDef d_yagi() {
  return {"yagi", "Yagi-Uda beam", "antenna",
          "3, 5 or 6 element Yagi from the NBS TN-688 published optimised designs.",
          {freq(146.0, 10, 3000),
           sel("elements", "Elements").opt("3", "3 elements (0.4 lambda boom, 7.1 dBd)")
               .opt("5", "5 elements (0.8 lambda boom, 9.2 dBd)").opt("6", "6 elements (1.2 lambda boom, 10.2 dBd)").def_s("3"),
           num("element_diam_mm", "Element diameter", "mm").def(10).positive(100).help("Used to warn when far from the design diameter.")}};
}
void r_yagi(Ctx& c) {
  double f, dm;
  std::string e;
  if (!c.num("freq_mhz", &f) || !c.choice("elements", &e) || !c.num("element_diam_mm", &dm)) return;
  emit(c, rf::yagi(f, std::atoi(e.c_str()), dm));
}

ToolDef d_tape() {
  return {"tape_yagi", "Tape-measure Yagi (2 m / 70 cm)", "antenna",
          "3-element handheld Yagi from a steel tape measure and PVC fittings (WB2HOL design).",
          {sel("band", "Band").opt("2m", "2 m (146.565 MHz)").opt("70cm", "70 cm (435 MHz)").opt("custom", "Custom frequency").def_s("2m"),
           num("freq_mhz", "Custom frequency", "MHz").def(146.565).range(140, 450).help("Only used with 'Custom frequency' (2 m or 70 cm band).")}};
}
void r_tape(Ctx& c) {
  std::string b;
  double f;
  if (!c.choice("band", &b) || !c.num("freq_mhz", &f)) return;
  if (b == "2m") f = 146.565;
  else if (b == "70cm") f = 435.0;
  else if (!((f >= 140 && f <= 150) || (f >= 420 && f <= 450))) {
    c.fail("Custom frequency must be in the 2 m (140-150 MHz) or 70 cm (420-450 MHz) range.");
    return;
  }
  emit(c, rf::tape_yagi(f));
}

ToolDef d_moxon() {
  return {"moxon", "Moxon rectangle", "antenna", "Two-element wire beam with excellent front-to-back, feeds 50 ohm directly.",
          {freq(146.0, 1, 3000), num("wire_diam_mm", "Wire / tube diameter", "mm").def(2).positive(50)}};
}
void r_moxon(Ctx& c) {
  double f, w;
  if (!c.num("freq_mhz", &f) || !c.num("wire_diam_mm", &w)) return;
  emit(c, rf::moxon(f, w));
}

ToolDef d_loop() {
  return {"loop", "Full-wave loop (quad / delta)", "antenna",
          "One-wavelength loop with 75 ohm quarter-wave match; quad (square) or delta (triangle).",
          {freq(14.2, 1, 3000),
           sel("shape", "Shape").opt("quad", "Quad (square)").opt("delta", "Delta (triangle)").def_s("quad"),
           num("match_vf", "Velocity factor of the 75 ohm match coax", "").def(0.66).range(0.5, 1.0).adv()
               .help("RG-59/RG-11 solid PE 0.66, foam RG-6 about 0.85.")}};
}
void r_loop(Ctx& c) {
  double f, vf;
  std::string s;
  if (!c.num("freq_mhz", &f) || !c.choice("shape", &s) || !c.num("match_vf", &vf)) return;
  emit(c, rf::full_wave_loop(f, s == "delta" ? 1 : 0, vf));
}

ToolDef d_efhw() {
  return {"efhw", "End-fed half wave (EFHW)", "antenna",
          "Half-wave wire fed at one end through a 49:1 unun; multiband on harmonics.",
          {freq(7.1, 1, 60), end_factor()}};
}
void r_efhw(Ctx& c) {
  double f, k;
  if (!c.num("freq_mhz", &f) || !c.num("end_factor", &k)) return;
  emit(c, rf::end_fed_half_wave(f, k));
}

ToolDef d_rw() {
  size_t n;
  const rf::HfBand* b = rf::hf_band_list(&n);
  ToolDef d{"random_wire", "Random-wire lengths to avoid", "antenna",
            "Finds wire lengths that avoid high-impedance half-wave multiples on the chosen HF bands.",
            {num("min_m", "Minimum length", "m").def(9).positive(500),
             num("max_m", "Maximum length", "m").def(40).positive(500),
             num("wire_vf", "Wire velocity factor", "").def(0.95).range(0.8, 1.0).adv()}};
  static const bool defaults[] = {false, true, false, true, false, true, false, true, false, true};
  for (size_t i = 0; i < n; ++i)
    d.inputs.push_back(flag("band_" + std::string(b[i].name).substr(0, std::string(b[i].name).find(' ')) + "m",
                            "Use " + std::string(b[i].name), defaults[i]));
  return d;
}
void r_rw(Ctx& c) {
  double lo, hi, vf;
  if (!c.num("min_m", &lo) || !c.num("max_m", &hi) || !c.num("wire_vf", &vf)) return;
  size_t n;
  const rf::HfBand* b = rf::hf_band_list(&n);
  unsigned mask = 0;
  for (size_t i = 0; i < n; ++i) {
    std::string key = "band_" + std::string(b[i].name).substr(0, std::string(b[i].name).find(' ')) + "m";
    bool on;
    if (!c.boolean(key.c_str(), &on)) return;
    if (on) mask |= 1u << i;
  }
  emit(c, rf::random_wire(lo, hi, mask, vf));
}

ToolDef d_nvis() {
  return {"nvis_dipole", "NVIS dipole", "antenna", "Low dipole for 0-400 km skywave coverage: length and height range.",
          {freq(5.35, 1.5, 15), end_factor()}};
}
void r_nvis(Ctx& c) {
  double f, k;
  if (!c.num("freq_mhz", &f) || !c.num("end_factor", &k)) return;
  emit(c, rf::nvis_dipole(f, k));
}

ToolDef d_coll() {
  return {"collinear", "Coaxial collinear (COCO)", "antenna",
          "Stacked half-wave coax sections for omnidirectional gain (VHF/UHF/LoRa).",
          {freq(868.0, 50, 3000), num("sections", "Coax sections", "").def(4).range(2, 16).whole(),
           num("coax_vf", "Velocity factor of the coax", "").def(0.66).range(0.5, 1.0).help("Solid PE 0.66; foam 0.78-0.85.")}};
}
void r_coll(Ctx& c) {
  double f, vf;
  int n;
  if (!c.num("freq_mhz", &f) || !c.integer("sections", &n) || !c.num("coax_vf", &vf)) return;
  emit(c, rf::coaxial_collinear(f, n, vf));
}

ToolDef d_choke() {
  return {"coax_choke", "Coax choke (ugly balun)", "antenna",
          "Air-wound coax choke: inductance and choking reactance at your frequency.",
          {freq(14.2, 1, 500), num("form_diam_mm", "Form diameter", "mm").def(110).positive(1000),
           num("coax_od_mm", "Coax outer diameter", "mm").def(5).positive(30),
           num("turns", "Turns", "").def(8).range(1, 60).whole()}};
}
void r_choke(Ctx& c) {
  double f, fd, od;
  int t;
  if (!c.num("freq_mhz", &f) || !c.num("form_diam_mm", &fd) || !c.num("coax_od_mm", &od) || !c.integer("turns", &t)) return;
  emit(c, rf::coax_choke(f, fd, od, t));
}

ToolDef d_balun() {
  return {"coax_balun", "Coax baluns (1:1 sleeve, 4:1 loop)", "antenna",
          "Quarter-wave sleeve (bazooka) 1:1 balun or half-wave coax 4:1 balun.",
          {freq(146.0, 1, 3000),
           sel("kind", "Type").opt("sleeve", "1:1 quarter-wave sleeve").opt("4to1", "4:1 half-wave coax loop").def_s("4to1"),
           num("coax_vf", "Coax velocity factor (4:1 loop)", "").def(0.66).range(0.5, 1.0),
           num("sleeve_vf", "Sleeve velocity factor (1:1)", "").def(0.95).range(0.5, 1.0).adv()}};
}
void r_balun(Ctx& c) {
  double f, vf, svf;
  std::string k;
  if (!c.num("freq_mhz", &f) || !c.choice("kind", &k) || !c.num("coax_vf", &vf) || !c.num("sleeve_vf", &svf)) return;
  emit(c, rf::coax_balun(f, k == "sleeve" ? 0 : 1, vf, svf));
}

ToolDef d_can() {
  return {"cantenna", "Cantenna (2.4 GHz waveguide)", "antenna",
          "Tin-can waveguide antenna: probe position from cutoff and guide wavelength.",
          {freq(2437, 1000, 6000), num("can_diam_mm", "Inside diameter of the can", "mm").def(83).positive(500)}};
}
void r_can(Ctx& c) {
  double f, d;
  if (!c.num("freq_mhz", &f) || !c.num("can_diam_mm", &d)) return;
  emit(c, rf::cantenna(f, d));
}

ToolDef d_biquad() {
  return {"biquad", "Biquad (2.4 GHz)", "antenna", "Double-square wire element over a reflector, ~10 dBi.",
          {freq(2437, 300, 6000)}};
}
void r_biquad(Ctx& c) {
  double f;
  if (!c.num("freq_mhz", &f)) return;
  emit(c, rf::biquad(f));
}

ToolDef d_lora() {
  return {"lora_antenna", "LoRa quarter-wave / ground plane", "antenna",
          "Quarter-wave whip and 4-radial ground plane for 915 / 868 / 433 MHz LoRa and Meshtastic.",
          {sel("band", "Band").opt("915", "915 MHz (US/AU)").opt("868", "868 MHz (EU)").opt("433", "433 MHz (EU)")
               .opt("923", "923 MHz (AS)").opt("custom", "Custom").def_s("915"),
           num("freq_mhz", "Custom frequency", "MHz").def(906.875).range(400, 1000).help("Only used with 'Custom'."),
           end_factor()}};
}
void r_lora(Ctx& c) {
  std::string b;
  double f, k;
  if (!c.choice("band", &b) || !c.num("freq_mhz", &f) || !c.num("end_factor", &k)) return;
  if (b == "915") f = 915.0;
  else if (b == "868") f = 868.0;
  else if (b == "433") f = 433.92;
  else if (b == "923") f = 923.0;
  emit(c, rf::lora_quarter_wave(f, k));
}

const Entry kEntries[] = {
    {d_dipole, r_dipole}, {d_inv_v, r_inv_v}, {d_gp, r_gp},       {d_jpole, r_jpole},   {d_slimjim, r_slimjim},
    {d_58, r_58},         {d_yagi, r_yagi},   {d_tape, r_tape},   {d_moxon, r_moxon},   {d_loop, r_loop},
    {d_efhw, r_efhw},     {d_rw, r_rw},       {d_nvis, r_nvis},   {d_coll, r_coll},     {d_choke, r_choke},
    {d_balun, r_balun},   {d_can, r_can},     {d_biquad, r_biquad}, {d_lora, r_lora},
};

}  // namespace

const Entry* antenna_tools(size_t* n) {
  *n = sizeof(kEntries) / sizeof(kEntries[0]);
  return kEntries;
}

}  // namespace detail
}  // namespace tools
}  // namespace ember
