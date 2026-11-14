// Survival tools (category "survival"): water disinfection, fallout decay, wind chill, heat index,
// unit conversion, ration planning.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "tools/tool_impl.h"

namespace ember {
namespace tools {
namespace detail {
namespace {

constexpr double kLitresPerGal = 3.785411784;

// ---- water disinfection ----
ToolDef d_water() {
  return {"water_disinfection", "Water disinfection dosing", "survival",
          "Household bleach or iodine dose for a volume of water, or boiling time (EPA / CDC figures).",
          {sel("method", "Method").opt("bleach", "Unscented household bleach").opt("iodine", "2 % tincture of iodine")
               .opt("boil", "Boiling").def_s("bleach"),
           num("volume_l", "Water volume", "L").def(4).positive(100000),
           num("strength_pct", "Bleach strength (sodium hypochlorite)", "%").def(6).range(1, 15)
               .help("Read the label: US bleach is usually 6 % or 8.25 %."),
           flag("cloudy", "Water is cloudy, coloured or very cold", false),
           num("altitude_m", "Altitude (for boiling)", "m").def(0).range(-500, 9000)}};
}
void r_water(Ctx& c) {
  std::string m;
  double vol, s, alt;
  bool cloudy;
  if (!c.choice("method", &m) || !c.num("volume_l", &vol) || !c.num("strength_pct", &s) || !c.boolean("cloudy", &cloudy) ||
      !c.num("altitude_m", &alt))
    return;
  if (m == "boil") {
    int minutes = alt > 2000 ? 3 : 1;
    c.out("boil_min", "Rolling boil for", minutes, "min", 0);
    c.note("CDC: bring water to a rolling boil for 1 minute; above 6,500 ft (2,000 m) boil for 3 minutes. Let it cool naturally.");
    c.note("Let cloudy water settle and filter it through a clean cloth first.");
    return;
  }
  double gal = vol / kLitresPerGal;
  double mult = cloudy ? 2.0 : 1.0;
  if (m == "bleach") {
    // EPA: 8 drops of 6 % (or 6 drops of 8.25 %) bleach per gallon of clear water; double if cloudy.
    double drops = gal * 8.0 * (6.0 / s) * mult;
    double ml = drops * 0.05;
    c.out("drops", "Bleach", std::ceil(drops), "drops", 0);
    c.out("ml", "Bleach (about)", ml, "mL", 2);
    c.out("tsp", "Bleach (about)", ml / 4.93, "tsp", 2);
    c.out("wait_min", "Wait before drinking", 30, "min", 0);
    c.note("Source: US EPA, Emergency Disinfection of Drinking Water: 8 drops of 6 % (6 drops of 8.25 %) unscented bleach per gallon; double for cloudy, coloured or very cold water.");
    c.note("Stir and wait 30 minutes. The water should smell slightly of chlorine; if not, repeat the dose and wait another 15 minutes.");
    c.note("Use only plain unscented bleach (no 'splash-less', colour-safe or added cleaners). Bleach loses strength with age: old bottles need more.");
    c.note("1 drop is taken as about 0.05 mL; use a clean eyedropper.");
  } else {
    double quarts = vol / 0.946352946;
    double drops = quarts * 5.0 * mult;
    c.out("drops", "2 % iodine tincture", std::ceil(drops), "drops", 0);
    c.out("wait_min", "Wait before drinking", 30, "min", 0);
    c.note("Source: CDC / older EPA emergency guidance: 5 drops of 2 % tincture of iodine per quart or litre of clear water, 10 drops if cloudy or cold; wait at least 30 minutes.");
    c.note("Not for pregnant women, people with thyroid problems or iodine allergy, or for continuous use over weeks.");
  }
  c.note("Chemical disinfection does not reliably kill Cryptosporidium; boiling does. Neither removes chemicals or fuel.");
}

// ---- fallout decay ----
ToolDef d_fallout() {
  return {"fallout_decay", "Fallout decay (7-10 rule)", "survival",
          "Dose-rate decay after a nuclear detonation: rate R(t) = R1 t^-1.2. Every 7x time, rate drops 10x.",
          {num("rate", "Measured dose rate", "per hour").def(100).positive(1e9).help("Any unit (R/h, mSv/h, mGy/h); results use the same unit."),
           num("t_meas_h", "Hours after detonation when measured", "h").def(1).range(0.5, 1e5),
           num("t_future_h", "Hours after detonation to predict", "h").def(49).range(0.5, 1e6),
           num("threshold", "Safe-ish rate to wait for", "per hour").def(1).positive(1e9)}};
}
void r_fallout(Ctx& c) {
  double r, t0, t1, thr;
  if (!c.num("rate", &r) || !c.num("t_meas_h", &t0) || !c.num("t_future_h", &t1) || !c.num("threshold", &thr)) return;
  double r1 = r * std::pow(t0, 1.2);  // rate at H+1
  double rt = r1 * std::pow(t1, -1.2);
  double t_thr = std::pow(r1 / thr, 1.0 / 1.2);
  double ta = std::min(t0, t1), tb = std::max(t0, t1);
  double dose = 5.0 * r1 * (std::pow(ta, -0.2) - std::pow(tb, -0.2));
  c.out("rate_h1", "Rate at H+1", r1, "per hour", 3);
  c.out("rate_future", "Rate at the predicted time", rt, "per hour", 4);
  c.out("time_to_threshold_h", "Time after detonation until below threshold", t_thr, "h", 1);
  c.out("time_to_threshold_d", "Time after detonation until below threshold", t_thr / 24.0, "days", 1);
  c.out("dose_unsheltered", "Unsheltered dose between the two times", dose, "", 2);
  c.note("7-10 rule: at 7 h the rate is ~1/10 of the 1 h rate; at 49 h ~1/100; at 2 weeks ~1/1000.");
  c.note("Shelter reduces dose: a basement or the centre of a large building can cut exposure 10-100x. Stay in for at least 24 h unless told otherwise.");
  c.note("Approximation (Way-Wigner t^-1.2); real fallout varies. Use a real meter when you have one.");
}

// ---- wind chill ----
ToolDef d_wc() {
  return {"wind_chill", "Wind chill", "survival", "NWS / Environment Canada 2001 wind-chill index and frostbite risk.",
          {sel("units", "Units").opt("metric", "C and km/h").opt("imperial", "F and mph").def_s("metric"),
           num("temp", "Air temperature", "C/F").def(-10).range(-100, 60), num("wind", "Wind speed", "km/h or mph").def(30).range(0, 400)}};
}
void r_wc(Ctx& c) {
  std::string u;
  double t, w;
  if (!c.choice("units", &u) || !c.num("temp", &t) || !c.num("wind", &w)) return;
  double tf = u == "metric" ? t * 9 / 5 + 32 : t;
  double mph = u == "metric" ? w / 1.609344 : w;
  if (tf > 50) { c.fail("Wind chill is defined only for temperatures at or below 10 C / 50 F"); return; }
  double wc = mph < 3 ? tf : 35.74 + 0.6215 * tf - 35.75 * std::pow(mph, 0.16) + 0.4275 * tf * std::pow(mph, 0.16);
  double wcc = (wc - 32) * 5 / 9;
  c.out("wind_chill_c", "Wind chill", wcc, "C", 1);
  c.out("wind_chill_f", "Wind chill", wc, "F", 1);
  const char* risk = wcc > -10 ? "Low" : (wcc > -28 ? "Moderate: cover exposed skin" :
                     (wcc > -40 ? "High: frostbite in 10-30 minutes" : (wcc > -48 ? "Very high: frostbite in 5-10 minutes" :
                     (wcc > -55 ? "Severe: frostbite in 2-5 minutes" : "Extreme: frostbite in under 2 minutes"))));
  c.out_s("risk", "Frostbite risk", risk);
  if (mph < 3) c.note("Below 3 mph (5 km/h) wind chill equals the air temperature.");
  c.note("Source: NWS/MSC wind chill formula (2001) and Environment Canada frostbite guidance.");
}

// ---- heat index ----
ToolDef d_hi() {
  return {"heat_index", "Heat index", "survival", "NWS heat index (apparent temperature) from temperature and humidity.",
          {sel("units", "Units").opt("metric", "C").opt("imperial", "F").def_s("metric"),
           num("temp", "Air temperature", "C/F").def(32).range(-60, 160), num("rh", "Relative humidity", "%").def(60).range(0, 100)}};
}
void r_hi(Ctx& c) {
  std::string u;
  double t, rh;
  if (!c.choice("units", &u) || !c.num("temp", &t) || !c.num("rh", &rh)) return;
  if (u == "metric" && t > 70) { c.fail("Air temperature: must be at most 70 C"); return; }
  double T = u == "metric" ? t * 9 / 5 + 32 : t;
  double hi = 0.5 * (T + 61.0 + (T - 68.0) * 1.2 + rh * 0.094);
  if ((hi + T) / 2 >= 80) {
    hi = -42.379 + 2.04901523 * T + 10.14333127 * rh - 0.22475541 * T * rh - 0.00683783 * T * T - 0.05481717 * rh * rh +
         0.00122874 * T * T * rh + 0.00085282 * T * rh * rh - 0.00000199 * T * T * rh * rh;
    if (rh < 13 && T > 80 && T < 112) hi -= ((13 - rh) / 4) * std::sqrt((17 - std::fabs(T - 95)) / 17);
    else if (rh > 85 && T > 80 && T < 87) hi += ((rh - 85) / 10) * ((87 - T) / 5);
  }
  c.out("heat_index_c", "Heat index", (hi - 32) * 5 / 9, "C", 1);
  c.out("heat_index_f", "Heat index", hi, "F", 1);
  const char* cat = hi < 80 ? "No heat warning" : (hi < 90 ? "Caution: fatigue possible" :
                    (hi < 103 ? "Extreme caution: heat cramps / exhaustion possible" :
                    (hi < 125 ? "Danger: heat exhaustion likely, heat stroke possible" : "Extreme danger: heat stroke likely")));
  c.out_s("category", "Category", cat);
  c.note("Source: NWS Rothfusz regression with NWS adjustments. Full sun can add up to ~8 C (15 F).");
  c.note("Rest in shade during the hottest hours, drink regularly, and watch for confusion (heat stroke = emergency).");
}

// ---- unit conversion ----
struct Unit { const char* id; const char* label; int cat; double factor; double offset; };  // value_si = v*factor + offset
const Unit kUnits[] = {
    {"m", "metre", 0, 1, 0}, {"km", "kilometre", 0, 1000, 0}, {"cm", "centimetre", 0, 0.01, 0}, {"mm", "millimetre", 0, 0.001, 0},
    {"in", "inch", 0, 0.0254, 0}, {"ft", "foot", 0, 0.3048, 0}, {"yd", "yard", 0, 0.9144, 0}, {"mi", "mile", 0, 1609.344, 0},
    {"nmi", "nautical mile", 0, 1852, 0},
    {"kg", "kilogram", 1, 1, 0}, {"g", "gram", 1, 0.001, 0}, {"lb", "pound", 1, 0.45359237, 0}, {"oz", "ounce", 1, 0.028349523125, 0},
    {"l", "litre", 2, 1, 0}, {"ml", "millilitre", 2, 0.001, 0}, {"gal", "US gallon", 2, 3.785411784, 0},
    {"qt", "US quart", 2, 0.946352946, 0}, {"cup", "US cup", 2, 0.2365882365, 0}, {"floz", "US fluid ounce", 2, 0.0295735295625, 0},
    {"tbsp", "US tablespoon", 2, 0.01478676478125, 0}, {"tsp", "US teaspoon", 2, 0.00492892159375, 0},
    {"impgal", "Imperial gallon", 2, 4.54609, 0},
    {"c", "degree Celsius", 3, 1, 273.15}, {"f", "degree Fahrenheit", 3, 5.0 / 9.0, 273.15 - 32 * 5.0 / 9.0}, {"k", "kelvin", 3, 1, 0},
    {"mps", "metre/second", 4, 1, 0}, {"kmh", "km/h", 4, 1 / 3.6, 0}, {"mph", "mile/hour", 4, 0.44704, 0}, {"kn", "knot", 4, 1852.0 / 3600, 0},
    {"kpa", "kilopascal", 5, 1000, 0}, {"hpa", "hectopascal (mbar)", 5, 100, 0}, {"bar", "bar", 5, 100000, 0},
    {"psi", "psi", 5, 6894.757293168, 0}, {"inhg", "inch of mercury", 5, 3386.389, 0}, {"mmhg", "mm of mercury", 5, 133.322387415, 0},
    {"kj", "kilojoule", 6, 1000, 0}, {"kcal", "kilocalorie (food Calorie)", 6, 4184, 0}, {"wh", "watt-hour", 6, 3600, 0},
    {"kwh", "kilowatt-hour", 6, 3.6e6, 0}, {"btu", "BTU", 6, 1055.05585262, 0},
    {"ha", "hectare", 7, 10000, 0}, {"acre", "acre", 7, 4046.8564224, 0}, {"m2", "square metre", 7, 1, 0}, {"km2", "square km", 7, 1e6, 0},
    {"ft2", "square foot", 7, 0.09290304, 0}, {"mi2", "square mile", 7, 2589988.110336, 0},
};
ToolDef d_conv() {
  FB from = sel("from", "From"), to = sel("to", "To");
  for (const auto& u : kUnits) { from.opt(u.id, u.label); to.opt(u.id, u.label); }
  from.def_s("gal");
  to.def_s("l");
  return {"unit_convert", "Unit converter", "survival",
          "Length, mass, volume, temperature, speed, pressure, energy and area conversions.",
          {num("value", "Value", "").def(1).range(-1e12, 1e12), from, to}};
}
void r_conv(Ctx& c) {
  double v;
  std::string f, t;
  if (!c.num("value", &v) || !c.choice("from", &f) || !c.choice("to", &t)) return;
  const Unit *a = nullptr, *b = nullptr;
  for (const auto& u : kUnits) { if (f == u.id) a = &u; if (t == u.id) b = &u; }
  if (!a || !b) { c.fail("unknown unit"); return; }
  if (a->cat != b->cat) { c.fail(std::string("Cannot convert ") + a->label + " to " + b->label); return; }
  double si = v * a->factor + a->offset;
  if (a->cat == 3 && si < 0) { c.fail("Temperature below absolute zero"); return; }
  double out = (si - b->offset) / b->factor;
  c.out_s("result", fmt_g(v, 8) + " " + a->label, fmt_g(out, 8), b->label);
}

// ---- rations ----
ToolDef d_rations() {
  return {"rations", "Food and water ration planner", "survival",
          "Total food and water needs for a group, or how long current stocks last.",
          {num("people", "People", "").def(4).range(1, 100000).whole(), num("days", "Days to plan for", "days").def(14).range(1, 3650),
           num("kcal_pp", "Food per person per day", "kcal").def(2000).range(500, 6000)
               .help("~2000 kcal adult at rest; 3000-4000 for hard physical work or cold."),
           num("water_pp", "Water per person per day", "L").def(3.8).range(0.5, 20)
               .help("FEMA/Ready.gov: at least 1 gallon (3.8 L) per person per day for drinking and sanitation; more in heat."),
           num("food_stock_kcal", "Food in stock (0 = unknown)", "kcal").def(0).range(0, 1e10),
           num("water_stock_l", "Water in stock (0 = unknown)", "L").def(0).range(0, 1e9)}};
}
void r_rations(Ctx& c) {
  int n;
  double days, kcal, water, fs, ws;
  if (!c.integer("people", &n) || !c.num("days", &days) || !c.num("kcal_pp", &kcal) || !c.num("water_pp", &water) ||
      !c.num("food_stock_kcal", &fs) || !c.num("water_stock_l", &ws))
    return;
  c.out("food_total_kcal", "Food needed", n * days * kcal, "kcal", 0);
  c.out("rice_kg", "...as dry rice/beans (~3600 kcal/kg)", n * days * kcal / 3600.0, "kg", 1);
  c.out("water_total_l", "Water needed", n * days * water, "L", 1);
  c.out("water_total_gal", "Water needed", n * days * water / kLitresPerGal, "US gal", 0);
  if (fs > 0) {
    c.out("food_days", "Food stock lasts", fs / (n * kcal), "days", 1);
    c.out("food_ration_kcal", "Daily ration per person to last the plan", fs / (n * days), "kcal", 0);
  }
  if (ws > 0) {
    c.out("water_days", "Water stock lasts", ws / (n * water), "days", 1);
    c.out("water_ration_l", "Daily water per person to last the plan", ws / (n * days), "L", 2);
  }
  c.note("Do not ration drinking water below need: drink what you need today and find more tomorrow. Reduce exertion and stay cool instead.");
  c.note("Children, nursing mothers, the sick and hot climates need more water.");
}

const Entry kEntries[] = {{d_water, r_water}, {d_fallout, r_fallout}, {d_wc, r_wc},
                          {d_hi, r_hi},       {d_conv, r_conv},       {d_rations, r_rations}};

}  // namespace

const Entry* survival_tools(size_t* n) {
  *n = sizeof(kEntries) / sizeof(kEntries[0]);
  return kEntries;
}

}  // namespace detail
}  // namespace tools
}  // namespace ember
