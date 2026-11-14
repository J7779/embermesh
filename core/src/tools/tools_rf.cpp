// RF / link calculators (category "rf"). Math lives in rf/rf_math.cpp.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "rf/rf_math.h"
#include "tools/tool_impl.h"

namespace ember {
namespace tools {
namespace detail {
namespace {

Field freq(double def, double lo = 0.001, double hi = 300000) {
  return num("freq_mhz", "Frequency", "MHz").def(def).range(lo, hi);
}

FB preset_field() {
  FB f = sel("preset", "Receiver sensitivity preset");
  for (const auto& p : rf::sensitivity_presets()) f.opt(p.id, p.label);
  f.opt("custom", "Custom (enter below)");
  f.def_s("lora_sf11_250");
  return f;
}
FB env_field() {
  return sel("environment", "Environment").opt("free", "Free space / clear line of sight (n=2)")
      .opt("open", "Open rural, some obstacles (n=2.5)").opt("suburban", "Suburban / light woods (n=3)")
      .opt("urban", "Urban / dense forest (n=3.5)").def_s("open")
      .help("Rough log-distance model beyond 100 m; real terrain varies a lot.");
}
double env_exponent(const std::string& e) {
  if (e == "free") return 2.0;
  if (e == "open") return 2.5;
  if (e == "suburban") return 3.0;
  return 3.5;
}
bool sensitivity(Ctx& c, double* out, std::string* label) {
  std::string p;
  double custom;
  if (!c.choice("preset", &p) || !c.num("rx_sens_dbm", &custom)) return false;  // validate both always
  if (p == "custom") {
    *label = "custom";
    *out = custom;
    return true;
  }
  rf::SensitivityPreset sp;
  if (!rf::find_sensitivity_preset(p, &sp)) return c.fail("unknown preset");
  *out = sp.sensitivity_dbm;
  *label = sp.label;
  c.note("Sensitivity preset: " + sp.label + " = " + fmt(sp.sensitivity_dbm, 1) + " dBm. " + sp.notes);
  return true;
}
std::string dist_str(double m) {
  if (m < 1000) return fmt(m, 0) + " m (" + fmt(m / 0.3048, 0) + " ft)";
  return fmt(m / 1000.0, 2) + " km (" + fmt(m / 1609.344, 2) + " mi)";
}

// ---- wavelength ----
ToolDef d_wl() {
  return {"wavelength", "Wavelength", "rf", "Free-space wavelength and common fractions, optionally scaled by velocity factor.",
          {freq(146.0), num("vf", "Velocity factor", "").def(1.0).range(0.1, 1.0).help("1.0 for free space; coax 0.66-0.88.")}};
}
void r_wl(Ctx& c) {
  double f, vf;
  if (!c.num("freq_mhz", &f) || !c.num("vf", &vf)) return;
  double l = rf::wavelength_m(f * 1e6) * vf;
  c.out_len("wavelength", "Wavelength (x VF)", l);
  c.out_len("half", "1/2 wave", l / 2);
  c.out_len("quarter", "1/4 wave", l / 4);
  c.out_len("five_eighths", "5/8 wave", l * 0.625);
  c.out_len("eighth", "1/8 wave", l / 8);
}

// ---- FSPL ----
ToolDef d_fspl() {
  return {"fspl", "Free-space path loss", "rf", "Path loss between isotropic antennas in free space (Friis).",
          {freq(433.0), num("distance_km", "Distance", "km").def(1).positive(400000)}};
}
void r_fspl(Ctx& c) {
  double f, d;
  if (!c.num("freq_mhz", &f) || !c.num("distance_km", &d)) return;
  c.out("fspl", "Free-space path loss", rf::fspl_db(d * 1000, f * 1e6), "dB", 2);
  c.note("Real paths add obstruction, foliage and fading losses: plan for 10-30 dB extra on ground paths.");
}

// ---- link budget ----
ToolDef d_lb() {
  return {"link_budget", "Link budget and range", "rf",
          "TX power, antenna gains, feedline losses and receiver sensitivity -> margin and estimated range.",
          {freq(906.875, 0.1, 100000),
           num("tx_power_dbm", "TX power", "dBm").def(22).range(-30, 70).help("30 dBm = 1 W, 37 dBm = 5 W, 22 dBm = typical LoRa module."),
           num("tx_gain_dbi", "TX antenna gain", "dBi").def(2.15).range(-30, 40),
           num("tx_loss_db", "TX feedline + connector loss", "dB").def(0.5).range(0, 50),
           num("rx_gain_dbi", "RX antenna gain", "dBi").def(2.15).range(-30, 40),
           num("rx_loss_db", "RX feedline + connector loss", "dB").def(0.5).range(0, 50),
           preset_field(),
           num("rx_sens_dbm", "Custom RX sensitivity", "dBm").def(-120).range(-160, 0).adv(),
           num("fade_margin_db", "Fade margin to keep in reserve", "dB").def(10).range(0, 50),
           env_field(),
           num("distance_km", "Evaluate at distance (0 = skip)", "km").def(0).range(0, 100000),
           num("tx_height_m", "TX antenna height (0 = ignore horizon)", "m").def(0).range(0, 20000),
           num("rx_height_m", "RX antenna height", "m").def(0).range(0, 20000)}};
}
void r_lb(Ctx& c) {
  rf::LinkBudgetIn in{};
  double f, dist;
  std::string env, slabel;
  if (!c.num("freq_mhz", &f) || !c.num("tx_power_dbm", &in.tx_power_dbm) || !c.num("tx_gain_dbi", &in.tx_gain_dbi) ||
      !c.num("tx_loss_db", &in.tx_feed_loss_db) || !c.num("rx_gain_dbi", &in.rx_gain_dbi) ||
      !c.num("rx_loss_db", &in.rx_feed_loss_db) || !sensitivity(c, &in.rx_sensitivity_dbm, &slabel) ||
      !c.num("fade_margin_db", &in.fade_margin_db) || !c.choice("environment", &env) || !c.num("distance_km", &dist) ||
      !c.num("tx_height_m", &in.tx_height_m) || !c.num("rx_height_m", &in.rx_height_m))
    return;
  in.freq_hz = f * 1e6;
  in.distance_m = dist * 1000;
  in.path_exponent = env_exponent(env);
  rf::LinkBudgetOut o = rf::link_budget(in);
  c.out("eirp_dbm", "EIRP", o.eirp_dbm, "dBm", 1);
  c.out("eirp_w", "EIRP", rf::dbm_to_w(o.eirp_dbm), "W", 3);
  c.out("sensitivity", "RX sensitivity", in.rx_sensitivity_dbm, "dBm", 1);
  c.out("max_path_loss", "Allowed path loss (after fade margin)", o.max_path_loss_db, "dB", 1);
  if (o.max_path_loss_db <= 0) {
    c.note("The link budget is negative: no range is possible with this margin.");
  } else {
    c.out_s("max_range", "Estimated max range (path-loss model)", dist_str(o.max_range_m));
    c.out_s("fs_range", "Free-space max range (ideal)", dist_str(rf::fspl_distance_m(o.max_path_loss_db, in.freq_hz)));
  }
  if (o.horizon_m > 0) {
    c.out_s("horizon", "Radio horizon (line of sight)", dist_str(o.horizon_m));
    c.out_s("effective_range", "Expected range (min of both)", dist_str(o.effective_range_m));
  }
  if (in.distance_m > 0) {
    c.out("path_loss", "Path loss at distance", o.path_loss_db, "dB", 1);
    c.out("rx_level", "Received level at distance", o.rx_level_dbm, "dBm", 1);
    c.out("margin", "Margin above sensitivity", o.margin_db, "dB", 1);
    c.note(o.margin_db >= in.fade_margin_db ? "Margin covers the fade margin: link should be reliable."
                                            : (o.margin_db > 0 ? "Positive but thin margin: expect drop-outs."
                                                               : "Below sensitivity: link unlikely."));
  }
  c.note("Range is dominated by antenna height and terrain at VHF/UHF: getting an antenna 10 m higher often beats 10x the power.");
}

// ---- horizon ----
ToolDef d_hor() {
  return {"radio_horizon", "Radio horizon / line of sight", "rf",
          "Maximum line-of-sight distance between two antennas (4/3 earth radius, standard refraction).",
          {num("h1_m", "Antenna 1 height", "m").def(10).range(0, 40000), num("h2_m", "Antenna 2 height", "m").def(1.5).range(0, 40000),
           num("k", "Effective earth radius factor", "").def(4.0 / 3.0).range(0.5, 4).adv().help("4/3 typical; 1 = geometric horizon.")}};
}
void r_hor(Ctx& c) {
  double h1, h2, k;
  if (!c.num("h1_m", &h1) || !c.num("h2_m", &h2) || !c.num("k", &k)) return;
  double a = rf::radio_horizon_km(h1, k), b = rf::radio_horizon_km(h2, k);
  c.out_s("horizon1", "Horizon of antenna 1", dist_str(a * 1000));
  c.out_s("horizon2", "Horizon of antenna 2", dist_str(b * 1000));
  c.out_s("los", "Max line-of-sight distance", dist_str((a + b) * 1000));
  c.out("los_km", "Max line-of-sight distance", a + b, "km", 2);
  c.note("Heights are above the average terrain between the stations; hills in between block the path.");
}

// ---- fresnel ----
ToolDef d_fr() {
  return {"fresnel", "Fresnel zone and earth bulge", "rf",
          "First Fresnel zone radius, 60 % clearance and earth bulge at a point along the path.",
          {freq(915), num("distance_km", "Path length", "km").def(10).positive(2000),
           num("point_frac", "Point along the path (0-1)", "").def(0.5).range(0, 1).step(0.05),
           num("k", "Effective earth radius factor", "").def(4.0 / 3.0).range(0.5, 4).adv()}};
}
void r_fr(Ctx& c) {
  double f, d, p, k;
  if (!c.num("freq_mhz", &f) || !c.num("distance_km", &d) || !c.num("point_frac", &p) || !c.num("k", &k)) return;
  double d1 = d * 1000 * p, d2 = d * 1000 * (1 - p);
  double fz = rf::fresnel_radius_m(d1, d2, f * 1e6);
  double bulge = rf::earth_bulge_m(d1, d2, k);
  c.out("f1_radius", "1st Fresnel zone radius", fz, "m", 2);
  c.out("f1_60", "60 % clearance needed", 0.6 * fz, "m", 2);
  c.out("bulge", "Earth bulge at point", bulge, "m", 2);
  c.out("clearance", "Required line height above terrain (bulge + 60 % F1)", bulge + 0.6 * fz, "m", 2);
  c.note("Keep at least 60 % of the first Fresnel zone free of obstacles (trees, roofs, ridges) for near free-space loss.");
}

// ---- ERP / EIRP ----
ToolDef d_erp() {
  return {"erp_eirp", "ERP / EIRP", "rf", "Effective radiated power from transmitter power, feedline loss and antenna gain.",
          {num("tx_w", "Transmitter power", "W").def(5).positive(100000), num("loss_db", "Feedline loss", "dB").def(1).range(0, 100),
           num("gain_dbi", "Antenna gain", "dBi").def(2.15).range(-30, 60)}};
}
void r_erp(Ctx& c) {
  double w, l, g;
  if (!c.num("tx_w", &w) || !c.num("loss_db", &l) || !c.num("gain_dbi", &g)) return;
  double eirp = rf::w_to_dbm(w) - l + g;
  c.out("eirp_dbm", "EIRP", eirp, "dBm", 2);
  c.out("eirp_w", "EIRP", rf::dbm_to_w(eirp), "W", 3);
  c.out("erp_dbm", "ERP (vs dipole)", eirp - 2.15, "dBm", 2);
  c.out("erp_w", "ERP", rf::dbm_to_w(eirp - 2.15), "W", 3);
}

// ---- dBm/W/V ----
ToolDef d_dbm() {
  return {"power_convert", "dBm / W / V converter", "rf", "Convert between dBm, dBW, watts, milliwatts, volts and microvolts (RMS).",
          {num("value", "Value", "").def(30).range(-1e6, 1e9),
           sel("unit", "Unit").opt("dbm", "dBm").opt("dbw", "dBW").opt("w", "W").opt("mw", "mW").opt("v", "V RMS").opt("uv", "uV RMS").def_s("dbm"),
           num("z_ohm", "Impedance", "ohm").def(50).positive(10000)}};
}
void r_dbm(Ctx& c) {
  double v, z;
  std::string u;
  if (!c.num("value", &v) || !c.choice("unit", &u) || !c.num("z_ohm", &z)) return;
  double dbm;
  if (u == "dbm") dbm = v;
  else if (u == "dbw") dbm = v + 30;
  else {
    if (v <= 0) { c.fail("Value: must be greater than 0 for linear units"); return; }
    if (u == "w") dbm = rf::w_to_dbm(v);
    else if (u == "mw") dbm = rf::w_to_dbm(v / 1000);
    else if (u == "v") dbm = rf::vrms_to_dbm(v, z);
    else dbm = rf::vrms_to_dbm(v * 1e-6, z);
  }
  if (dbm < -250 || dbm > 250) { c.fail("Value out of range"); return; }
  double w = rf::dbm_to_w(dbm);
  c.out("dbm", "Power", dbm, "dBm", 2);
  c.out("dbw", "Power", dbm - 30, "dBW", 2);
  c.out_s("w", "Power", fmt_g(w, 4), "W");
  c.out_s("mw", "Power", fmt_g(w * 1000, 4), "mW");
  c.out_s("v", "Voltage across Z", fmt_g(rf::dbm_to_vrms(dbm, z), 4), "V RMS");
  c.out_s("uv", "Voltage across Z", fmt_g(rf::dbm_to_uv(dbm, z), 4), "uV RMS");
}

// ---- SWR ----
ToolDef d_swr() {
  return {"swr", "SWR / return loss / mismatch", "rf",
          "Convert SWR, return loss, reflection coefficient, forward/reflected power or a load impedance.",
          {sel("from", "Known quantity").opt("swr", "SWR").opt("rl", "Return loss (dB)").opt("gamma", "Reflection coefficient |G|")
               .opt("power", "Forward + reflected power").opt("impedance", "Load impedance R + jX").def_s("swr"),
           num("value", "SWR / return loss / |G|", "").def(1.5).range(0, 1000),
           num("fwd_w", "Forward power", "W").def(100).positive(1e6),
           num("ref_w", "Reflected power", "W").def(4).range(0, 1e6),
           num("r_ohm", "Load resistance R", "ohm").def(75).positive(1e5),
           num("x_ohm", "Load reactance X", "ohm").def(0).range(-1e5, 1e5),
           num("z0_ohm", "Line impedance Z0", "ohm").def(50).positive(1000)}};
}
void r_swr(Ctx& c) {
  std::string from;
  double v, fw, rw, r, x, z0;
  if (!c.choice("from", &from) || !c.num("value", &v) || !c.num("fwd_w", &fw) || !c.num("ref_w", &rw) ||
      !c.num("r_ohm", &r) || !c.num("x_ohm", &x) || !c.num("z0_ohm", &z0))
    return;
  rf::SwrInfo s;
  if (from == "swr") {
    if (v < 1) { c.fail("SWR must be at least 1"); return; }
    s = rf::swr_from_swr(v);
  } else if (from == "rl") {
    if (v <= 0) { c.fail("Return loss must be greater than 0 dB"); return; }
    s = rf::swr_from_return_loss(v);
  } else if (from == "gamma") {
    if (v >= 1) { c.fail("|G| must be below 1"); return; }
    s = rf::swr_from_gamma(v);
  } else if (from == "power") {
    if (rw >= fw) { c.fail("Reflected power must be less than forward power"); return; }
    s = rf::swr_from_powers(fw, rw);
  } else {
    s = rf::swr_from_impedance(r, x, z0);
  }
  c.out("swr", "SWR", s.swr, ":1", 2);
  c.out("return_loss", "Return loss", s.return_loss_db, "dB", 2);
  c.out("gamma", "Reflection coefficient |G|", s.gamma, "", 4);
  c.out("mismatch_loss", "Mismatch loss", s.mismatch_loss_db, "dB", 3);
  c.out("reflected_pct", "Reflected power", s.reflected_pct, "%", 2);
  c.out("delivered_pct", "Delivered power", 100 - s.reflected_pct, "%", 2);
  if (s.swr > 3) c.note("SWR above 3:1: many solid-state radios fold back power; fix the antenna or use a tuner.");
}

// ---- coax loss ----
ToolDef d_coax() {
  FB t = sel("cable", "Cable");
  size_t n;
  const rf::CoaxType* ct = rf::coax_types(&n);
  for (size_t i = 0; i < n; ++i) t.opt(ct[i].id, ct[i].name);
  t.def_s("rg58");
  return {"coax_loss", "Coax / feedline loss", "rf",
          "Matched loss of common cables at any frequency and length, plus extra loss from SWR.",
          {t, freq(146, 0.1, 6000), num("length_m", "Length", "m").def(15).positive(10000),
           num("swr", "SWR at the antenna", ":1").def(1).range(1, 100), num("tx_w", "Power into the cable", "W").def(5).positive(100000)}};
}
void r_coax(Ctx& c) {
  std::string id;
  double f, l, swr, w;
  if (!c.choice("cable", &id) || !c.num("freq_mhz", &f) || !c.num("length_m", &l) || !c.num("swr", &swr) || !c.num("tx_w", &w)) return;
  const rf::CoaxType* ct = rf::find_coax(id);
  if (!ct) { c.fail("unknown cable"); return; }
  if (f > ct->max_mhz) { c.fail("Frequency above the range of this cable model (" + fmt(ct->max_mhz, 0) + " MHz)"); return; }
  double ml = rf::coax_loss_db(*ct, f, l);
  double tl = swr > 1 ? rf::coax_total_loss_with_swr_db(ml, swr) : ml;
  c.out("per_100m", "Matched loss per 100 m", rf::coax_loss_db_per_100ft(*ct, f) / 30.48 * 100, "dB", 2);
  c.out("per_100ft", "Matched loss per 100 ft", rf::coax_loss_db_per_100ft(*ct, f), "dB", 2);
  c.out("matched_loss", "Matched loss for this length", ml, "dB", 2);
  c.out("total_loss", "Total loss including SWR", tl, "dB", 2);
  c.out("power_out", "Power reaching the antenna", w * std::pow(10, -tl / 10), "W", 3);
  c.out("efficiency", "Feedline efficiency", 100 * std::pow(10, -tl / 10), "%", 1);
  c.out("z0", "Impedance", ct->z0_ohm, "ohm", 0);
  c.out("vf", "Velocity factor", ct->velocity_factor, "", 2);
  c.note(ct->notes);
  c.note("Typical manufacturer figures for new cable; old, kinked or water-logged coax is worse.");
}

// ---- repeater / relay site ----
ToolDef d_rep() {
  return {"relay_coverage", "Repeater / relay site planner", "rf",
          "Expected coverage radius of a relay from antenna heights, clutter and both directions of the link budget.",
          {freq(146.0, 1, 100000),
           num("relay_height_m", "Relay antenna height above ground", "m").def(15).positive(20000),
           num("user_height_m", "User antenna height", "m").def(1.5).positive(1000),
           num("clutter_m", "Height of trees/buildings around users", "m").def(0).range(0, 200),
           num("relay_power_dbm", "Relay TX power", "dBm").def(37).range(-30, 70),
           num("user_power_dbm", "User TX power", "dBm").def(37).range(-30, 70),
           num("relay_gain_dbi", "Relay antenna gain (incl. feedline loss)", "dBi").def(3).range(-30, 30),
           num("user_gain_dbi", "User antenna gain", "dBi").def(0).range(-30, 30),
           preset_field().def_s("nbfm_voice"),
           num("rx_sens_dbm", "Custom RX sensitivity", "dBm").def(-119).range(-160, 0).adv(),
           num("fade_margin_db", "Fade margin", "dB").def(10).range(0, 50), env_field()}};
}
void r_rep(Ctx& c) {
  double f, hr, hu, cl, pr, pu, gr, gu, sens, fm;
  std::string env, lab;
  if (!c.num("freq_mhz", &f) || !c.num("relay_height_m", &hr) || !c.num("user_height_m", &hu) || !c.num("clutter_m", &cl) ||
      !c.num("relay_power_dbm", &pr) || !c.num("user_power_dbm", &pu) || !c.num("relay_gain_dbi", &gr) ||
      !c.num("user_gain_dbi", &gu) || !sensitivity(c, &sens, &lab) || !c.num("fade_margin_db", &fm) || !c.choice("environment", &env))
    return;
  if (hr <= cl) { c.fail("Relay antenna must be above the surrounding clutter"); return; }
  double n = env_exponent(env);
  double fhz = f * 1e6;
  // Horizon from the clutter top: relay above clutter + user (if above clutter).
  double hor = (rf::radio_horizon_km(hr - cl) + rf::radio_horizon_km(std::max(hu - cl, 0.0))) * 1000;
  // Users inside clutter get roughly 6-10 dB clutter loss at VHF/UHF; apply 8 dB if below clutter top.
  double clutter_loss = hu < cl ? 8.0 : 0.0;
  double down = pr + gr + gu - sens - fm - clutter_loss;
  double up = pu + gu + gr - sens - fm - clutter_loss;
  double r_down = down > 0 ? rf::log_distance_range_m(down, fhz, n) : 0;
  double r_up = up > 0 ? rf::log_distance_range_m(up, fhz, n) : 0;
  double radius = std::min({hor, r_down, r_up});
  c.out_s("horizon", "Radio horizon (relay to users)", dist_str(hor));
  c.out_s("range_down", "Talk-out range (relay -> user)", dist_str(r_down));
  c.out_s("range_up", "Talk-in range (user -> relay)", dist_str(r_up));
  c.out_s("coverage", "Expected coverage radius", dist_str(radius));
  c.out("coverage_km", "Expected coverage radius", radius / 1000, "km", 2);
  c.out("area_km2", "Coverage area", rf::kPi * radius * radius / 1e6, "km2", 1);
  const char* lim = radius == hor ? "the radio horizon (raise the antenna)" :
                    (radius == r_up ? "the user's transmitter (talk-in)" : "the relay's transmitter (talk-out)");
  c.note(std::string("Coverage is limited by ") + lim + ".");
  if (clutter_loss > 0) c.note("Users below the clutter top: 8 dB clutter loss assumed.");
  c.note("Put relays on the highest accessible point (hilltop, tall building, mast); add a tower before adding power.");
}

// ---- HF guidance ----
ToolDef d_hf() {
  return {"hf_bands", "HF band / time-of-day guide (incl. NVIS)", "rf",
          "Which amateur HF bands are likely to work for a given distance, time of day, season and solar activity.",
          {num("distance_km", "Distance to the other station", "km").def(250).positive(20000),
           num("local_hour", "Local solar time (hour)", "h").def(12).range(0, 24).step(0.5),
           sel("season", "Season").opt("0", "Winter").opt("1", "Spring / autumn").opt("2", "Summer").def_s("1"),
           sel("solar", "Solar activity").opt("0", "Low (solar minimum)").opt("1", "Medium").opt("2", "High (solar maximum)").def_s("1")}};
}
void r_hf(Ctx& c) {
  double d, h;
  std::string s, sa;
  if (!c.num("distance_km", &d) || !c.num("local_hour", &h) || !c.choice("season", &s) || !c.choice("solar", &sa)) return;
  auto v = rf::hf_guidance(d, h, s[0] - '0', sa[0] - '0');
  std::string best;
  int top = -1;
  std::string t = "Band    Rating  Why\n";
  static const char* stars[] = {"-   ", "*   ", "**  ", "*** "};
  for (const auto& b : v) {
    t += pad_right(b.band, 8) + stars[b.score < 0 ? 0 : (b.score > 3 ? 3 : b.score)] + "    " + b.reason + "\n";
    if (b.score > top) { top = b.score; best = b.band; }
    else if (b.score == top) best += ", " + b.band;
  }
  c.r.text = t;
  c.out_s("best", "Best band(s)", best);
  c.out_s("mode", "Propagation mode", d <= 400 ? "NVIS (near-vertical incidence)" : (d <= 1500 ? "Single-hop skywave" : "Multi-hop skywave"));
  if (d <= 400) c.note("NVIS: use a low horizontal antenna (0.1-0.25 lambda high). Vertical antennas are poor for NVIS.");
  c.note("Heuristic guidance only. Listening to time stations (WWV 5/10/15 MHz) or beacons tells you what is actually open.");
}

// ---- LoRa airtime ----
ToolDef d_air() {
  return {"lora_airtime", "LoRa time on air / duty cycle", "rf",
          "Packet airtime for a LoRa setting and how many packets fit in a duty-cycle limit.",
          {num("sf", "Spreading factor", "").def(11).range(6, 12).whole(),
           sel("bw", "Bandwidth").opt("125000", "125 kHz").opt("250000", "250 kHz").opt("500000", "500 kHz").def_s("250000"),
           num("payload", "Payload", "bytes").def(50).range(1, 255).whole(),
           num("cr", "Coding rate 4/x", "").def(5).range(5, 8).whole(),
           num("preamble", "Preamble symbols", "").def(16).range(6, 65535).whole().adv(),
           num("duty_pct", "Duty-cycle limit", "%").def(10).positive(100)}};
}
void r_air(Ctx& c) {
  int sf, pl, cr, pre;
  double duty;
  std::string bw;
  if (!c.integer("sf", &sf) || !c.choice("bw", &bw) || !c.integer("payload", &pl) || !c.integer("cr", &cr) ||
      !c.integer("preamble", &pre) || !c.num("duty_pct", &duty))
    return;
  double b = std::atof(bw.c_str());
  bool ldro = std::pow(2.0, sf) / b * 1000.0 > 16.0;
  double t = rf::lora_time_on_air_ms(sf, b, pl, cr, pre, ldro);
  c.out("airtime_ms", "Time on air", t, "ms", 1);
  c.out("per_hour", "Max packets per hour at duty limit", std::floor(3600000.0 * duty / 100.0 / t), "", 0);
  c.out("bitrate", "Raw bit rate", sf * (4.0 / cr) * b / std::pow(2.0, sf), "bit/s", 0);
  c.out("sensitivity", "Approx. sensitivity", rf::lora_sensitivity_dbm(sf, b), "dBm", 1);
  if (ldro) c.note("Low data rate optimisation enabled (symbol time > 16 ms).");
}

const Entry kEntries[] = {{d_wl, r_wl},     {d_fspl, r_fspl}, {d_lb, r_lb},   {d_hor, r_hor},   {d_fr, r_fr},
                          {d_erp, r_erp},   {d_dbm, r_dbm},   {d_swr, r_swr}, {d_coax, r_coax}, {d_rep, r_rep},
                          {d_hf, r_hf},     {d_air, r_air}};

}  // namespace

const Entry* rf_tools(size_t* n) {
  *n = sizeof(kEntries) / sizeof(kEntries[0]);
  return kEntries;
}

}  // namespace detail
}  // namespace tools
}  // namespace ember
