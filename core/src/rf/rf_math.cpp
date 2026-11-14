#include "rf/rf_math.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ember {
namespace rf {

double wavelength_m(double freq_hz) { return kC / freq_hz; }
double dbm_to_w(double dbm) { return std::pow(10.0, (dbm - 30.0) / 10.0); }
double w_to_dbm(double w) { return 10.0 * std::log10(w) + 30.0; }
double dbm_to_vrms(double dbm, double z) { return std::sqrt(dbm_to_w(dbm) * z); }
double vrms_to_dbm(double v, double z) { return w_to_dbm(v * v / z); }
double dbm_to_uv(double dbm, double z) { return dbm_to_vrms(dbm, z) * 1e6; }

double fspl_db(double dist_m, double freq_hz) {
  return 20.0 * std::log10(4.0 * kPi * dist_m * freq_hz / kC);
}
double fspl_distance_m(double loss_db, double freq_hz) {
  return std::pow(10.0, loss_db / 20.0) * kC / (4.0 * kPi * freq_hz);
}

// ---- SWR ------------------------------------------------------------------------------------
static SwrInfo from_gamma(double g) {
  SwrInfo s;
  if (g < 0) g = 0;
  if (g > 0.999999) g = 0.999999;
  s.gamma = g;
  s.swr = (1.0 + g) / (1.0 - g);
  s.return_loss_db = g > 1e-6 ? -20.0 * std::log10(g) : 120.0;
  s.mismatch_loss_db = -10.0 * std::log10(1.0 - g * g);
  s.reflected_pct = g * g * 100.0;
  return s;
}
SwrInfo swr_from_gamma(double g) { return from_gamma(g); }
SwrInfo swr_from_swr(double swr) { return from_gamma((swr - 1.0) / (swr + 1.0)); }
SwrInfo swr_from_return_loss(double rl) { return from_gamma(std::pow(10.0, -rl / 20.0)); }
SwrInfo swr_from_powers(double fwd, double ref) { return from_gamma(std::sqrt(ref / fwd)); }
SwrInfo swr_from_impedance(double r, double x, double z0) {
  // Γ = (Z - Z0) / (Z + Z0) for complex Z = r + jx and real Z0.
  double nr = r - z0, dr = r + z0;
  double num = std::sqrt(nr * nr + x * x), den = std::sqrt(dr * dr + x * x);
  return from_gamma(num / den);
}

// ---- geometry ----------------------------------------------------------------------------------
double radio_horizon_km(double h_m, double k) {
  // d = sqrt(2·k·R·h); with R in km and h in m → sqrt(2·k·R·h/1000)
  return std::sqrt(2.0 * k * kEarthRadiusKm * h_m / 1000.0);
}
double los_distance_km(double h1, double h2, double k) { return radio_horizon_km(h1, k) + radio_horizon_km(h2, k); }
double fresnel_radius_m(double d1, double d2, double f, int n) {
  double lam = wavelength_m(f);
  return std::sqrt(n * lam * d1 * d2 / (d1 + d2));
}
double earth_bulge_m(double d1, double d2, double k) {
  return d1 * d2 / (2.0 * k * kEarthRadiusKm * 1000.0);
}

// ---- coax ------------------------------------------------------------------------------------
// Attenuation coefficients are two-point fits of typical published manufacturer figures for matched
// loss in dB/100 ft (k1·√f + k2·f, f in MHz). Expect ±15 % versus a particular brand; aged or wet
// cable is worse.
static const CoaxType kCoax[] = {
    {"rg174", "RG-174", 50, 0.66, 0.86286, 0.002714, 3000, "Thin (2.8 mm). Fine for short jumpers only."},
    {"rg58", "RG-58", 50, 0.66, 0.42412, 0.006588, 3000, "Common 5 mm coax. Acceptable on HF, lossy at UHF."},
    {"rg8x", "RG-8X (mini-8)", 50, 0.78, 0.34368, 0.002632, 3000, "6 mm foam. Velocity factor varies 0.78-0.82 by brand."},
    {"rg213", "RG-213", 50, 0.66, 0.20475, 0.001525, 3000, "10 mm solid PE. Robust, good on HF/VHF."},
    {"rg6", "RG-6 (75 ohm TV)", 75, 0.85, 0.19743, 0.000257, 3000,
     "75 ohm satellite/TV cable: low loss and easy to scavenge; 50 ohm radios see ~1.5:1 SWR from the mismatch alone."},
    {"lmr195", "LMR-195", 50, 0.80, 0.35253, 0.000549, 6000, "RG-58 size, lower loss."},
    {"lmr240", "LMR-240", 50, 0.84, 0.24208, 0.000330, 6000, "RG-8X size, lower loss."},
    {"lmr400", "LMR-400", 50, 0.85, 0.12229, 0.000260, 6000, "10 mm low-loss foam; the usual choice for VHF/UHF/LoRa runs."},
    {"hardline_half", "1/2 in hardline (LDF4-50A class)", 50, 0.88, 0.06289, 0.000311, 6000,
     "Corrugated 1/2 in heliax; very low loss, needs proper connectors."},
    {"ladder450", "450 ohm window/ladder line", 450, 0.91, 0.02312, 0.000688, 300,
     "Balanced line, dry. Wet or iced line can be several times lossier; keep away from metal."},
};

const CoaxType* coax_types(size_t* n) {
  *n = sizeof(kCoax) / sizeof(kCoax[0]);
  return kCoax;
}
const CoaxType* find_coax(const std::string& id) {
  for (const auto& c : kCoax)
    if (id == c.id) return &c;
  return nullptr;
}
double coax_loss_db_per_100ft(const CoaxType& c, double f) { return c.k1 * std::sqrt(f) + c.k2 * f; }
double coax_loss_db(const CoaxType& c, double f, double len_m) {
  return coax_loss_db_per_100ft(c, f) * (len_m / (100.0 * kMetersPerFoot));
}
double coax_total_loss_with_swr_db(double ml, double swr) {
  double a = std::pow(10.0, ml / 10.0);
  double rho = (swr - 1.0) / (swr + 1.0);
  double r2 = rho * rho;
  return 10.0 * std::log10((a * a - r2) / (a * (1.0 - r2)));
}

// ---- sensitivity --------------------------------------------------------------------------------
double lora_snr_limit_db(int sf) {
  // Semtech SX1276/77/78/79 datasheet demodulator SNR limits.
  switch (sf) {
    case 6: return -5.0;
    case 7: return -7.5;
    case 8: return -10.0;
    case 9: return -12.5;
    case 10: return -15.0;
    case 11: return -17.5;
    case 12: return -20.0;
    default: return std::nan("");
  }
}
double lora_sensitivity_dbm(int sf, double bw_hz, double nf_db) {
  return -174.0 + 10.0 * std::log10(bw_hz) + nf_db + lora_snr_limit_db(sf);
}
double lora_time_on_air_ms(int sf, double bw_hz, int pl, int cr_den, int preamble, bool ldro) {
  double tsym = std::pow(2.0, sf) / bw_hz * 1000.0;
  int cr = cr_den - 4;  // 4/5 → 1 … 4/8 → 4
  int de = ldro ? 1 : 0;
  double num = 8.0 * pl - 4.0 * sf + 28.0 + 16.0 /*CRC*/ - 0.0 /*explicit header*/;
  double den = 4.0 * (sf - 2 * de);
  double n = std::ceil(num / den) * (cr + 4);
  if (n < 0) n = 0;
  double payload_sym = 8.0 + n;
  return (preamble + 4.25) * tsym + payload_sym * tsym;
}

std::vector<SensitivityPreset> sensitivity_presets() {
  std::vector<SensitivityPreset> v;
  const double bws[] = {125000.0, 250000.0, 500000.0};
  for (double bw : bws) {
    for (int sf = 7; sf <= 12; ++sf) {
      char id[32], label[64];
      std::snprintf(id, sizeof id, "lora_sf%d_%d", sf, static_cast<int>(bw / 1000));
      std::snprintf(label, sizeof label, "LoRa SF%d / %d kHz", sf, static_cast<int>(bw / 1000));
      double s = lora_sensitivity_dbm(sf, bw);
      v.push_back({id, label, std::round(s * 10) / 10,
                   "Semtech formula -174+10log10(BW)+NF(6 dB)+SNR limit; datasheets quote within ~1-2 dB."});
    }
  }
  v.push_back({"nbfm_voice", "NBFM voice (12 dB SINAD, typical handheld)", -119.0,
               "0.25 uV into 50 ohm; good handhelds quote 0.16-0.25 uV. Readable voice needs a few dB more."});
  v.push_back({"afsk1200", "1200 bd AFSK packet over FM", -110.0,
               "Approximate: 1200 bd packet needs a fairly full-quieting FM signal (~0.7 uV)."});
  v.push_back({"hf300", "300 bd HF FSK/packet (SSB receiver)", -118.0,
               "Approximate. On HF the limit is usually atmospheric/man-made noise, not receiver sensitivity."});
  v.push_back({"robust_slow", "Slow robust mode (~50-100 bd FSK + FEC, SSB receiver)", -126.0,
               "Estimate: -174 + 10log10(100 bd) + NF 8 dB + 20 dB Eb/N0. Noise-limited on HF."});
  return v;
}
bool find_sensitivity_preset(const std::string& id, SensitivityPreset* out) {
  for (const auto& p : sensitivity_presets()) {
    if (p.id == id) { *out = p; return true; }
  }
  return false;
}

// ---- link budget ------------------------------------------------------------------------------
static constexpr double kRefDist = 100.0;  // m: breakpoint of the log-distance model

double log_distance_loss_db(double d, double f, double n) {
  if (d <= kRefDist || n <= 2.0) return fspl_db(d, f);
  return fspl_db(kRefDist, f) + 10.0 * n * std::log10(d / kRefDist);
}
double log_distance_range_m(double loss, double f, double n) {
  double l0 = fspl_db(kRefDist, f);
  if (loss <= l0 || n <= 2.0) return fspl_distance_m(loss, f);
  return kRefDist * std::pow(10.0, (loss - l0) / (10.0 * n));
}

LinkBudgetOut link_budget(const LinkBudgetIn& in) {
  LinkBudgetOut o{};
  o.eirp_dbm = in.tx_power_dbm - in.tx_feed_loss_db + in.tx_gain_dbi;
  double budget = o.eirp_dbm + in.rx_gain_dbi - in.rx_feed_loss_db - in.rx_sensitivity_dbm;
  o.max_path_loss_db = budget - in.fade_margin_db;
  o.max_range_m = o.max_path_loss_db > 0 ? log_distance_range_m(o.max_path_loss_db, in.freq_hz, in.path_exponent) : 0;
  if (in.distance_m > 0) {
    o.path_loss_db = log_distance_loss_db(in.distance_m, in.freq_hz, in.path_exponent);
    o.rx_level_dbm = o.eirp_dbm - o.path_loss_db + in.rx_gain_dbi - in.rx_feed_loss_db;
    o.margin_db = o.rx_level_dbm - in.rx_sensitivity_dbm;
  }
  o.effective_range_m = o.max_range_m;
  if (in.tx_height_m > 0 && in.rx_height_m > 0) {
    o.horizon_m = los_distance_km(in.tx_height_m, in.rx_height_m) * 1000.0;
    o.effective_range_m = std::min(o.max_range_m, o.horizon_m);
  }
  return o;
}

// ---- HF guidance ------------------------------------------------------------------------------
std::vector<HfBandAdvice> hf_guidance(double dist, double hour, int season, int solar) {
  struct B { const char* name; double lo, hi; };
  static const B bands[] = {{"160 m", 1.8, 2.0},     {"80 m", 3.5, 4.0},     {"60 m", 5.3, 5.41},
                            {"40 m", 7.0, 7.3},      {"30 m", 10.1, 10.15},  {"20 m", 14.0, 14.35},
                            {"17 m", 18.068, 18.168}, {"15 m", 21.0, 21.45}, {"12 m", 24.89, 24.99},
                            {"10 m", 28.0, 29.7}};
  bool day = hour >= 8.0 && hour < 17.0;
  bool grey = (hour >= 5.0 && hour < 8.0) || (hour >= 17.0 && hour < 20.0);
  bool night = !day && !grey;
  int range = dist <= 400 ? 0 : (dist <= 1500 ? 1 : 2);  // NVIS / regional / DX
  std::vector<HfBandAdvice> out;
  for (const auto& b : bands) {
    int s = 0;
    std::string why;
    double f = b.lo;
    if (range == 0) {
      // NVIS: the frequency must stay below the critical frequency foF2 (roughly 5-10 MHz by day,
      // 2-5 MHz at night, higher with more solar activity and in summer days).
      double fo = day ? 7.0 : (grey ? 4.5 : 3.0);
      fo += 1.2 * solar;
      if (season == 2 && day) fo += 0.5;
      if (season == 0 && night) fo -= 0.5;
      if (f < 2.5) {
        s = night ? 2 : (grey ? 1 : 0);
        why = night ? "NVIS works at night; high noise and big antennas needed" : "absorbed by the D layer in daylight";
      } else if (f <= fo * 0.9) {
        s = 3;
        why = "below the critical frequency: reliable NVIS skywave";
        if (day && f < 5.0) {
          s = 2;
          why = "below the critical frequency, but weakened by midday D-layer absorption";
        }
      } else if (f <= fo * 1.1) {
        s = 2;
        why = "near the critical frequency: NVIS may be patchy";
      } else {
        s = 0;
        why = "above the critical frequency: signals pass through (skip zone)";
      }
    } else if (range == 1) {
      if (day) {
        if (f >= 5 && f < 11) { s = 3; why = "best daytime regional band"; }
        else if (f >= 3 && f < 5) { s = 2; why = "works by day over shorter regional paths"; }
        else if (f >= 14 && f < 15) { s = solar >= 1 ? 2 : 1; why = "may skip over closer stations"; }
        else { s = f < 3 ? 0 : (solar == 2 && f < 22 ? 1 : 0); why = f < 3 ? "absorbed in daylight" : "usually skips over regional distances"; }
      } else {
        if (f >= 3 && f < 5) { s = 3; why = "best night-time regional band"; }
        else if (f < 3) { s = 2; why = "good at night, high noise"; }
        else if (f >= 5 && f < 8) { s = 2; why = "useful at night, may go long"; }
        else { s = (grey && f < 15) ? 1 : 0; why = "usually closed or skips at night"; }
      }
    } else {
      if (day) {
        if (f >= 14 && f < 15) { s = 3; why = "workhorse daytime long-distance band"; }
        else if (f >= 18 && f < 22) { s = solar >= 1 ? 3 : 1; why = "good when solar activity is moderate or high"; }
        else if (f >= 24) { s = solar == 2 ? 2 : (solar == 1 ? 1 : 0); why = "open mainly near solar maximum"; }
        else if (f >= 10 && f < 11) { s = 2; why = "reliable medium-long distance"; }
        else if (f >= 7 && f < 8) { s = 1; why = "long distance mostly after dark"; }
        else { s = 0; why = "absorbed by day over long paths"; }
      } else {
        if (f >= 7 && f < 11) { s = 3; why = "best night-time long-distance band"; }
        else if (f >= 3 && f < 5) { s = 2; why = "long distance at night, especially in winter"; }
        else if (f >= 14 && f < 15) { s = (season == 2 || solar == 2 || grey) ? 2 : 1; why = "may stay open after dark in summer / high activity"; }
        else if (f < 3) { s = season == 0 ? 1 : 0; why = "winter nights only"; }
        else { s = (grey && solar >= 1) ? 1 : 0; why = "usually closed at night"; }
      }
    }
    out.push_back({b.name, b.lo, b.hi, s, why});
  }
  return out;
}

}  // namespace rf
}  // namespace ember
