// Power tools (category "power"): battery runtime, solar sizing, wire voltage drop, Ohm's law,
// battery packs.
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "tools/tool_impl.h"

namespace ember {
namespace tools {
namespace detail {
namespace {

std::string hours_str(double h) {
  if (h < 48) return fmt(h, 1) + " h";
  return fmt(h / 24.0, 1) + " days (" + fmt(h, 0) + " h)";
}

// ---- battery runtime ----
ToolDef d_bat() {
  return {"battery_runtime", "Battery runtime", "power",
          "Runtime of a radio/node from battery capacity, receive/transmit currents and TX duty cycle.",
          {num("capacity_mah", "Battery capacity", "mAh").def(3000).positive(1e8),
           num("rx_ma", "Receive / idle current", "mA").def(15).range(0, 1e6),
           num("tx_ma", "Transmit current", "mA").def(120).range(0, 1e6),
           num("tx_duty_pct", "Time spent transmitting", "%").def(5).range(0, 100),
           num("extra_ma", "Other constant load (screen, GPS)", "mA").def(0).range(0, 1e6),
           num("usable_pct", "Usable capacity", "%").def(80).positive(100)
               .help("80 % for Li-ion/LiFePO4 (ageing, cold, cut-off); 50 % for lead-acid to avoid damage.")}};
}
void r_bat(Ctx& c) {
  double cap, rx, tx, duty, ex, use;
  if (!c.num("capacity_mah", &cap) || !c.num("rx_ma", &rx) || !c.num("tx_ma", &tx) || !c.num("tx_duty_pct", &duty) ||
      !c.num("extra_ma", &ex) || !c.num("usable_pct", &use))
    return;
  double avg = tx * duty / 100.0 + rx * (1 - duty / 100.0) + ex;
  if (avg <= 0) { c.fail("Average current is zero: enter receive or transmit current"); return; }
  double h = cap * use / 100.0 / avg;
  c.out("avg_ma", "Average current", avg, "mA", 2);
  c.out("hours", "Runtime", h, "h", 1);
  c.out("days", "Runtime", h / 24.0, "days", 2);
  c.out_s("runtime", "Runtime", hours_str(h));
  c.note("Cold (below 0 C) can halve the usable capacity of most batteries. Keep them inside your clothing.");
  if (duty > 20) c.note("High TX duty cycle: transmit current dominates. Lower power or fewer transmissions save the most.");
}

// ---- solar ----
ToolDef d_solar() {
  return {"solar_sizing", "Solar panel and battery sizing", "power",
          "Panel wattage and battery capacity for a daily energy need, sun hours and days of autonomy.",
          {num("daily_wh", "Daily energy use", "Wh").def(60).positive(1e7)
               .help("Sum of watts x hours per day. A LoRa node ~1-3 Wh; a 5 W handheld at 10 % TX ~10 Wh; a laptop 50-100 Wh."),
           num("sun_hours", "Peak sun hours per day", "h").def(3).positive(12)
               .help("Equivalent full-sun hours: winter at high latitude 1-2, temperate summer 4-6, desert 6-7."),
           num("system_eff_pct", "System efficiency", "%").def(75).positive(100).help("Panel angle, dirt, heat, controller and battery losses."),
           num("autonomy_days", "Days without sun to ride through", "days").def(2).range(0, 30),
           num("battery_v", "Battery voltage", "V").def(12).positive(1000),
           num("dod_pct", "Allowed depth of discharge", "%").def(80).positive(100).help("LiFePO4 80-90 %, lead-acid 50 %.")}};
}
void r_solar(Ctx& c) {
  double wh, sun, eff, days, v, dod;
  if (!c.num("daily_wh", &wh) || !c.num("sun_hours", &sun) || !c.num("system_eff_pct", &eff) ||
      !c.num("autonomy_days", &days) || !c.num("battery_v", &v) || !c.num("dod_pct", &dod))
    return;
  double panel = wh / (sun * eff / 100.0);
  double store_days = days < 1 ? 1 : days;
  double bat_wh = wh * store_days / (dod / 100.0);
  c.out("panel_w", "Minimum panel rating", panel, "W", 0);
  c.out("battery_wh", "Battery capacity", bat_wh, "Wh", 0);
  c.out("battery_ah", "Battery capacity", bat_wh / v, "Ah", 1);
  c.out("controller_a", "Charge controller current (panel W / V x 1.25)", panel / v * 1.25, "A", 1);
  c.note("Round the panel size up generously: clouds, short winter days and ageing all eat into the margin.");
  c.note("Tilt panels towards the equator at roughly your latitude; even partial shade on one cell can collapse output.");
}

// ---- wire gauge voltage drop ----
struct Gauge { const char* id; int awg; double ampacity; };  // awg: 0 → "1/0" is -0 … use n with 0=-0
// NEC Table 310.16 copper, 60 °C column (building wire); 14 AWG limited to 15 A by 240.4(D).
const Gauge kGauges[] = {{"4/0", -3, 195}, {"3/0", -2, 165}, {"2/0", -1, 145}, {"1/0", 0, 125}, {"1", 1, 110},
                         {"2", 2, 95},     {"3", 3, 85},     {"4", 4, 70},     {"6", 6, 55},    {"8", 8, 40},
                         {"10", 10, 30},   {"12", 12, 20},   {"14", 14, 15},   {"16", 16, 0},   {"18", 18, 0},
                         {"20", 20, 0},    {"22", 22, 0},    {"24", 24, 0},    {"26", 26, 0}};
double awg_diam_mm(int n) { return 0.127 * std::pow(92.0, (36.0 - n) / 39.0); }

ToolDef d_wire() {
  FB g = sel("awg", "Wire gauge (AWG)");
  for (const auto& x : kGauges) g.opt(x.id, std::string(x.id) + " AWG (" + fmt(std::pow(awg_diam_mm(x.awg), 2) * 3.14159265 / 4, 2) + " mm2)");
  g.def_s("14");
  return {"wire_drop", "Wire voltage drop / ampacity", "power",
          "Voltage drop and loss of a DC run (there and back), with ampacity and the smallest gauge for 3 % drop.",
          {g, num("current_a", "Current", "A").def(10).positive(10000), num("length_m", "One-way length", "m").def(5).positive(100000),
           num("voltage_v", "Supply voltage", "V").def(12).positive(100000),
           sel("material", "Conductor").opt("cu", "Copper").opt("al", "Aluminium").def_s("cu")}};
}
void r_wire(Ctx& c) {
  std::string id, mat;
  double i, l, v;
  if (!c.choice("awg", &id) || !c.num("current_a", &i) || !c.num("length_m", &l) || !c.num("voltage_v", &v) ||
      !c.choice("material", &mat))
    return;
  const Gauge* g = nullptr;
  for (const auto& x : kGauges) if (id == x.id) g = &x;
  if (!g) { c.fail("unknown gauge"); return; }
  double rho = mat == "al" ? 2.82e-8 : 1.724e-8;  // ohm·m at 20 °C
  auto res_per_m = [&](int awg) { double d = awg_diam_mm(awg) / 1000.0; return rho / (3.14159265358979 * d * d / 4); };
  double r = res_per_m(g->awg) * 2 * l;
  double drop = i * r;
  c.out("diameter", "Conductor diameter", awg_diam_mm(g->awg), "mm", 3);
  c.out("area", "Cross-section", std::pow(awg_diam_mm(g->awg), 2) * 3.14159265358979 / 4, "mm2", 3);
  c.out("resistance", "Loop resistance (both wires)", r, "ohm", 4);
  c.out("drop_v", "Voltage drop", drop, "V", 3);
  c.out("drop_pct", "Voltage drop", drop / v * 100, "%", 2);
  c.out("load_v", "Voltage at the load", v - drop, "V", 2);
  c.out("loss_w", "Power lost in the wire", i * i * r, "W", 2);
  if (mat == "cu" && g->ampacity > 0) {
    c.out("ampacity", "Ampacity (NEC 60 C copper)", g->ampacity, "A", 0);
    if (i > g->ampacity) c.note("Current exceeds the tabulated ampacity: the wire will overheat. Use a thicker gauge.");
  } else {
    c.note("No ampacity listed for this gauge/material here; keep currents modest and check the wire stays cool.");
  }
  const char* best = nullptr;
  for (int k = static_cast<int>(sizeof(kGauges) / sizeof(kGauges[0])) - 1; k >= 0; --k) {
    if (i * res_per_m(kGauges[k].awg) * 2 * l / v <= 0.03) { best = kGauges[k].id; break; }
  }
  c.out_s("min_awg_3pct", "Smallest gauge for <= 3 % drop", best ? std::string(best) + " AWG" : "thicker than 4/0: use a higher voltage");
  if (drop > v * 0.1) c.note("More than 10 % drop: radios may reset on transmit. Shorten the run or use thicker wire.");
  c.note("Resistance at 20 C; warm wire has higher resistance. Fuse every positive lead near the battery.");
}

// ---- Ohm's law ----
ToolDef d_ohm() {
  return {"ohms_law", "Ohm's law / power", "power", "Enter any two of voltage, current, resistance and power (others 0).",
          {num("v", "Voltage", "V").def(12).range(0, 1e7), num("i", "Current", "A").def(2).range(0, 1e7),
           num("r", "Resistance", "ohm").def(0).range(0, 1e12), num("p", "Power", "W").def(0).range(0, 1e9)}};
}
void r_ohm(Ctx& c) {
  double v, i, r, p;
  if (!c.num("v", &v) || !c.num("i", &i) || !c.num("r", &r) || !c.num("p", &p)) return;
  int known = (v > 0) + (i > 0) + (r > 0) + (p > 0);
  if (known != 2) { c.fail("Enter exactly two non-zero values (the others 0)"); return; }
  if (v > 0 && i > 0) { r = v / i; p = v * i; }
  else if (v > 0 && r > 0) { i = v / r; p = v * i; }
  else if (v > 0 && p > 0) { i = p / v; r = v / i; }
  else if (i > 0 && r > 0) { v = i * r; p = v * i; }
  else if (i > 0 && p > 0) { v = p / i; r = v / i; }
  else { i = std::sqrt(p / r); v = i * r; }
  c.out_s("v", "Voltage", fmt_g(v, 5), "V");
  c.out_s("i", "Current", fmt_g(i, 5), "A");
  c.out_s("r", "Resistance", fmt_g(r, 5), "ohm");
  c.out_s("p", "Power", fmt_g(p, 5), "W");
}

// ---- battery pack ----
ToolDef d_pack() {
  return {"battery_pack", "Series / parallel battery pack", "power",
          "Voltage, capacity and energy of a pack built from cells (e.g. salvaged 18650s).",
          {sel("chemistry", "Chemistry").opt("liion", "Li-ion (3.6-3.7 V)").opt("lifepo4", "LiFePO4 (3.2 V)")
               .opt("nimh", "NiMH (1.2 V)").opt("lead", "Lead-acid cell (2.0 V)").opt("custom", "Custom").def_s("liion"),
           num("cell_v", "Custom cell nominal voltage", "V").def(3.7).positive(100).adv(),
           num("cell_mah", "Cell capacity", "mAh").def(2500).positive(1e7),
           num("series", "Cells in series (S)", "").def(3).range(1, 500).whole(),
           num("parallel", "Cells in parallel (P)", "").def(2).range(1, 500).whole()}};
}
void r_pack(Ctx& c) {
  std::string ch;
  double cv, mah;
  int s, p;
  if (!c.choice("chemistry", &ch) || !c.num("cell_v", &cv) || !c.num("cell_mah", &mah) || !c.integer("series", &s) ||
      !c.integer("parallel", &p))
    return;
  double vmax = 0, vmin = 0;
  if (ch == "liion") { cv = 3.6; vmax = 4.2; vmin = 3.0; }
  else if (ch == "lifepo4") { cv = 3.2; vmax = 3.65; vmin = 2.5; }
  else if (ch == "nimh") { cv = 1.2; vmax = 1.45; vmin = 1.0; }
  else if (ch == "lead") { cv = 2.0; vmax = 2.4; vmin = 1.75; }
  double ah = mah * p / 1000.0;
  c.out_s("config", "Configuration", std::to_string(s) + "S" + std::to_string(p) + "P (" + std::to_string(s * p) + " cells)");
  c.out("nominal_v", "Nominal voltage", cv * s, "V", 2);
  if (vmax > 0) {
    c.out("full_v", "Fully charged", vmax * s, "V", 2);
    c.out("empty_v", "Empty (cut-off)", vmin * s, "V", 2);
  }
  c.out("capacity_ah", "Capacity", ah, "Ah", 2);
  c.out("energy_wh", "Energy", ah * cv * s, "Wh", 1);
  c.note("Match cells (same type, capacity and state of charge) before connecting them in parallel.");
  if (ch == "liion" || ch == "lifepo4")
    c.note("Lithium packs need a BMS (protection board) per series cell group: over-charge or over-discharge can cause fire.");
}

const Entry kEntries[] = {{d_bat, r_bat}, {d_solar, r_solar}, {d_wire, r_wire}, {d_ohm, r_ohm}, {d_pack, r_pack}};

}  // namespace

const Entry* power_tools(size_t* n) {
  *n = sizeof(kEntries) / sizeof(kEntries[0]);
  return kEntries;
}

}  // namespace detail
}  // namespace tools
}  // namespace ember
