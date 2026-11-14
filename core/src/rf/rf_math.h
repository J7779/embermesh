// EmberMesh core — RF / link calculators (pure math, no allocation-heavy state, no I/O).
//
// Units are in the function names (_hz, _mhz, _m, _km, _db, _dbm, _w). Every function is total:
// invalid input yields NaN only through the *_checked wrappers in tools/, which validate first;
// callers in the core should validate ranges themselves (see tools/tool.h Args).
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace ember {
namespace rf {

constexpr double kC = 299792458.0;           // speed of light, m/s
constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthRadiusKm = 6371.0088;  // IUGG mean radius
constexpr double kMetersPerFoot = 0.3048;

// ---- basic conversions ----------------------------------------------------------------------
double wavelength_m(double freq_hz);                   // free-space λ
double dbm_to_w(double dbm);
double w_to_dbm(double w);
double dbm_to_vrms(double dbm, double z_ohm = 50.0);   // RMS volts across z
double vrms_to_dbm(double v, double z_ohm = 50.0);
double dbm_to_uv(double dbm, double z_ohm = 50.0);     // µV RMS (receiver sensitivity style)

// Free-space path loss in dB (Friis, isotropic antennas): 20log10(4πd/λ).
double fspl_db(double dist_m, double freq_hz);
// Distance at which FSPL equals loss_db.
double fspl_distance_m(double loss_db, double freq_hz);

// ---- SWR / reflection ------------------------------------------------------------------------
struct SwrInfo {
  double swr;               // ≥ 1
  double gamma;             // |Γ| 0..1
  double return_loss_db;    // ≥ 0 (inf for perfect match → reported as large finite 99.9)
  double mismatch_loss_db;  // ≥ 0
  double reflected_pct;     // % of forward power reflected
};
SwrInfo swr_from_swr(double swr);                      // swr ≥ 1
SwrInfo swr_from_return_loss(double rl_db);            // rl > 0
SwrInfo swr_from_gamma(double gamma);                  // 0 ≤ Γ < 1
SwrInfo swr_from_powers(double fwd_w, double ref_w);   // 0 ≤ ref < fwd
SwrInfo swr_from_impedance(double r_load, double x_load, double z0);  // complex load on real Z0

// ---- propagation geometry ----------------------------------------------------------------------
// Radio horizon from one antenna height using effective earth radius factor k (4/3 standard).
double radio_horizon_km(double height_m, double k = 4.0 / 3.0);
// Max line-of-sight distance between two antennas (sum of horizons).
double los_distance_km(double h1_m, double h2_m, double k = 4.0 / 3.0);
// Radius of the n-th Fresnel zone at a point d1 / d2 from each end (metres).
double fresnel_radius_m(double d1_m, double d2_m, double freq_hz, int n = 1);
// Earth bulge above the chord at a point d1 / d2 from each end (metres), effective radius k·R.
double earth_bulge_m(double d1_m, double d2_m, double k = 4.0 / 3.0);

// ---- coax ------------------------------------------------------------------------------------
struct CoaxType {
  const char* id;
  const char* name;
  double z0_ohm;
  double velocity_factor;
  double k1;                 // attenuation model: dB/100 ft = k1·sqrt(f_MHz) + k2·f_MHz (matched)
  double k2;
  double max_mhz;            // sensible upper limit of the model
  const char* notes;
};
const CoaxType* coax_types(size_t* count);
const CoaxType* find_coax(const std::string& id);
double coax_loss_db_per_100ft(const CoaxType& c, double freq_mhz);
double coax_loss_db(const CoaxType& c, double freq_mhz, double length_m);
// Extra loss caused by SWR at the load on a lossy line (ARRL formula). matched_loss_db ≥ 0, swr ≥ 1.
double coax_total_loss_with_swr_db(double matched_loss_db, double swr_at_load);

// ---- receiver sensitivity presets ---------------------------------------------------------------
// Semtech LoRa sensitivity: -174 + 10log10(BW) + NF + SNR_limit(SF). NF defaults to 6 dB (SX127x).
double lora_snr_limit_db(int sf);                       // SF6..SF12, NaN otherwise
double lora_sensitivity_dbm(int sf, double bw_hz, double nf_db = 6.0);
// LoRa time on air in milliseconds (Semtech AN1200.13), explicit header, CRC on.
double lora_time_on_air_ms(int sf, double bw_hz, int payload_bytes, int coding_rate_denominator = 5,
                           int preamble_symbols = 8, bool low_data_rate_optimize = false);

struct SensitivityPreset {
  std::string id;
  std::string label;
  double sensitivity_dbm;
  std::string notes;
};
// Built-in presets: lora_sf7_125 … lora_sf12_500, nbfm_voice, afsk1200, hf300, robust_slow.
std::vector<SensitivityPreset> sensitivity_presets();
bool find_sensitivity_preset(const std::string& id, SensitivityPreset* out);

// ---- link budget ------------------------------------------------------------------------------
struct LinkBudgetIn {
  double freq_hz;
  double tx_power_dbm;
  double tx_gain_dbi;
  double tx_feed_loss_db;
  double rx_gain_dbi;
  double rx_feed_loss_db;
  double rx_sensitivity_dbm;
  double fade_margin_db;      // margin the user wants to keep in reserve
  double distance_m;          // distance to evaluate (0 → skip)
  double path_exponent;       // 2 = free space; >2 = log-distance model beyond 100 m
  double tx_height_m;         // 0 → no horizon cap
  double rx_height_m;
};
struct LinkBudgetOut {
  double eirp_dbm;
  double max_path_loss_db;    // allowed path loss leaving fade_margin_db spare
  double path_loss_db;        // at distance_m (if given)
  double rx_level_dbm;        // at distance_m
  double margin_db;           // rx_level - sensitivity at distance_m
  double max_range_m;         // from path-loss model at fade margin
  double horizon_m;           // 0 if heights not given
  double effective_range_m;   // min(max_range, horizon) when heights given
};
LinkBudgetOut link_budget(const LinkBudgetIn& in);
// Path loss of the log-distance model used above: FSPL up to 100 m, then 10·n·log10(d/100 m).
double log_distance_loss_db(double dist_m, double freq_hz, double n);
double log_distance_range_m(double loss_db, double freq_hz, double n);

// ---- HF guidance ------------------------------------------------------------------------------
struct HfBandAdvice {
  std::string band;     // "40 m"
  double low_mhz;
  double high_mhz;
  int score;            // 0 (useless) .. 3 (best choice)
  std::string reason;
};
// Heuristic guidance for amateur HF bands given path distance, local solar time and season.
// season: 0 = winter, 1 = equinox, 2 = summer. solar_activity: 0 low, 1 medium, 2 high.
std::vector<HfBandAdvice> hf_guidance(double distance_km, double local_hour, int season, int solar_activity);

}  // namespace rf
}  // namespace ember
