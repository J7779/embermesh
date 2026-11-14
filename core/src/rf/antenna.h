// EmberMesh core — antenna designers. Each function returns cut dimensions (metres), extra values,
// an ASCII build diagram and practical notes. Inputs are assumed validated by the caller (tools/
// does schema validation); every function still re-checks its own physical limits and sets
// Design::error instead of returning nonsense.
#pragma once

#include <string>
#include <vector>

namespace ember {
namespace rf {

struct Dim {
  std::string key;
  std::string label;
  double m;  // length in metres
};
struct Value {
  std::string key;
  std::string label;
  double v;
  std::string unit;
  int decimals;
};
struct Design {
  std::string error;
  std::vector<Dim> dims;
  std::vector<Value> values;
  std::string diagram;
  std::vector<std::string> notes;
  bool ok() const { return error.empty(); }
};

// end_factor: shortening factor applied to the free-space half wave (0.95 ≈ the 468/f ft rule).
Design dipole(double f_mhz, double end_factor);
Design inverted_v(double f_mhz, double end_factor, double apex_height_m, double included_angle_deg);
Design ground_plane(double f_mhz, double end_factor, double radial_droop_deg, int radials);
Design j_pole(double f_mhz, double end_factor);
Design slim_jim(double f_mhz, double end_factor);
Design five_eighths_vertical(double f_mhz, double end_factor);
// elements: 3, 5 or 6 (NBS TN-688 / Viezbicke designs, d/λ = 0.0085). element_diam_mm for warnings.
Design yagi(double f_mhz, int elements, double element_diam_mm);
// WB2HOL 3-element tape-measure Yagi (published for 146.565 MHz), scaled to f.
Design tape_yagi(double f_mhz);
Design moxon(double f_mhz, double wire_diam_mm);
// shape: 0 = quad (square), 1 = delta (equilateral). match_vf: velocity factor of the 75 Ω match.
Design full_wave_loop(double f_mhz, int shape, double match_vf);
Design end_fed_half_wave(double f_mhz, double end_factor);
// Random-wire lengths that avoid half-wave multiples (high impedance) on the selected HF bands.
// band_mask bit i = band i of hf_band_list(). Lengths in metres.
Design random_wire(double min_m, double max_m, unsigned band_mask, double wire_vf);
Design nvis_dipole(double f_mhz, double end_factor);
// sections: number of λ/2 coax sections (2..16)
Design coaxial_collinear(double f_mhz, int sections, double coax_vf);
// Air-wound coax choke: inductance (Wheeler) and choking reactance at f.
Design coax_choke(double f_mhz, double form_diam_mm, double coax_od_mm, int turns);
// kind: 0 = λ/4 sleeve (bazooka) 1:1 balun, 1 = λ/2 coax 4:1 balun
Design coax_balun(double f_mhz, int kind, double coax_vf, double sleeve_vf);
Design cantenna(double f_mhz, double can_diam_mm);
Design biquad(double f_mhz);
Design lora_quarter_wave(double f_mhz, double end_factor);

struct HfBand {
  const char* name;
  double lo_mhz;
  double hi_mhz;
};
const HfBand* hf_band_list(size_t* count);  // 160 m … 10 m

}  // namespace rf
}  // namespace ember
