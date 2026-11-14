#include <cmath>
#include <set>

#include "doctest.h"
#include "rf/antenna.h"
#include "rf/bands.h"
#include "rf/rf_math.h"

using namespace ember::rf;

namespace {
const BandService* find_svc(const std::vector<BandService>& v, const std::string& id) {
  for (const auto& s : v)
    if (s.id == id) return &s;
  return nullptr;
}
double dim(const Design& d, const std::string& key) {
  for (const auto& x : d.dims)
    if (x.key == key) return x.m;
  return std::nan("");
}
}  // namespace

TEST_SUITE("rf") {

TEST_CASE("wavelength and power conversions") {
  CHECK(wavelength_m(146e6) == doctest::Approx(2.05337).epsilon(1e-5));
  CHECK(wavelength_m(299792458.0) == doctest::Approx(1.0));
  CHECK(dbm_to_w(30) == doctest::Approx(1.0));
  CHECK(dbm_to_w(37) == doctest::Approx(5.0119).epsilon(1e-4));
  CHECK(w_to_dbm(0.001) == doctest::Approx(0.0));
  CHECK(dbm_to_vrms(0) == doctest::Approx(0.22361).epsilon(1e-4));
  CHECK(dbm_to_uv(-119) == doctest::Approx(0.2509).epsilon(1e-3));
  CHECK(vrms_to_dbm(dbm_to_vrms(-73.5)) == doctest::Approx(-73.5));
}

TEST_CASE("free-space path loss reference values") {
  // FSPL(dB) = 20log10(d_km) + 20log10(f_MHz) + 32.45
  CHECK(fspl_db(1000, 433e6) == doctest::Approx(85.18).epsilon(1e-3));
  CHECK(fspl_db(10000, 2437e6) == doctest::Approx(120.18).epsilon(1e-3));
  CHECK(fspl_distance_m(fspl_db(12345, 915e6), 915e6) == doctest::Approx(12345));
}

TEST_CASE("SWR conversions") {
  SwrInfo s = swr_from_swr(1.5);
  CHECK(s.gamma == doctest::Approx(0.2));
  CHECK(s.return_loss_db == doctest::Approx(13.979).epsilon(1e-4));
  CHECK(s.mismatch_loss_db == doctest::Approx(0.1773).epsilon(1e-3));
  CHECK(s.reflected_pct == doctest::Approx(4.0));
  CHECK(swr_from_return_loss(13.979).swr == doctest::Approx(1.5).epsilon(1e-4));
  CHECK(swr_from_powers(100, 4).swr == doctest::Approx(1.5));
  CHECK(swr_from_impedance(75, 0, 50).swr == doctest::Approx(1.5));
  CHECK(swr_from_impedance(50, 0, 50).swr == doctest::Approx(1.0));
  CHECK(swr_from_impedance(25, 0, 50).swr == doctest::Approx(2.0));
  CHECK(std::isfinite(swr_from_swr(1.0).return_loss_db));
  CHECK(std::isfinite(swr_from_gamma(1.0).swr));  // clamped, no inf
}

TEST_CASE("horizon, Fresnel zone and earth bulge") {
  CHECK(radio_horizon_km(10) == doctest::Approx(13.03).epsilon(2e-3));  // 4.12·sqrt(h)
  CHECK(radio_horizon_km(0) == doctest::Approx(0.0));
  CHECK(los_distance_km(10, 1.5) == doctest::Approx(13.03 + 5.05).epsilon(3e-3));
  CHECK(fresnel_radius_m(5000, 5000, 915e6) == doctest::Approx(28.62).epsilon(1e-3));
  CHECK(earth_bulge_m(5000, 5000) == doctest::Approx(1.4715).epsilon(1e-3));
}

TEST_CASE("coax loss model") {
  CHECK(coax_loss_db_per_100ft(*find_coax("lmr400"), 900) == doctest::Approx(3.9).epsilon(0.02));
  CHECK(coax_loss_db_per_100ft(*find_coax("rg58"), 100) == doctest::Approx(4.9).epsilon(0.01));
  CHECK(coax_loss_db(*find_coax("rg213"), 146, 30.48) == doctest::Approx(coax_loss_db_per_100ft(*find_coax("rg213"), 146)));
  CHECK(find_coax("nope") == nullptr);
  CHECK(coax_total_loss_with_swr_db(1.0, 1.0) == doctest::Approx(1.0));
  CHECK(coax_total_loss_with_swr_db(1.0, 3.0) == doctest::Approx(1.504).epsilon(1e-3));
  size_t n;
  const CoaxType* t = coax_types(&n);
  CHECK(n >= 10);
  for (size_t i = 0; i < n; ++i) {
    CHECK(t[i].velocity_factor > 0.5);
    CHECK(t[i].velocity_factor < 1.0);
    // Loss rises monotonically with frequency.
    CHECK(coax_loss_db_per_100ft(t[i], 10) < coax_loss_db_per_100ft(t[i], 100));
  }
}

TEST_CASE("LoRa sensitivity and time on air") {
  CHECK(lora_sensitivity_dbm(7, 125000) == doctest::Approx(-124.5).epsilon(1e-3));
  CHECK(lora_sensitivity_dbm(12, 125000) == doctest::Approx(-137.0).epsilon(1e-3));
  CHECK(std::isnan(lora_snr_limit_db(13)));
  CHECK(lora_time_on_air_ms(12, 125000, 10, 5, 8, true) == doctest::Approx(991.232).epsilon(1e-5));
  CHECK(lora_time_on_air_ms(7, 125000, 10, 5, 8, false) == doctest::Approx(41.216).epsilon(1e-5));
  auto p = sensitivity_presets();
  CHECK(p.size() == 18 + 4);
  SensitivityPreset sp;
  CHECK(find_sensitivity_preset("lora_sf12_125", &sp));
  CHECK(sp.sensitivity_dbm == doctest::Approx(-137.0));
  CHECK(find_sensitivity_preset("nbfm_voice", &sp));
  CHECK_FALSE(find_sensitivity_preset("bogus", &sp));
}

TEST_CASE("link budget is consistent with FSPL") {
  LinkBudgetIn in{};
  in.freq_hz = 433e6;
  in.tx_power_dbm = 20;
  in.rx_sensitivity_dbm = -120;
  in.path_exponent = 2;
  in.distance_m = 1000;
  LinkBudgetOut o = link_budget(in);
  CHECK(o.eirp_dbm == doctest::Approx(20));
  CHECK(o.path_loss_db == doctest::Approx(fspl_db(1000, 433e6)));
  CHECK(o.margin_db == doctest::Approx(140 - fspl_db(1000, 433e6)));
  CHECK(fspl_db(o.max_range_m, 433e6) == doctest::Approx(140).epsilon(1e-6));
  // Heights cap the range at the radio horizon.
  in.tx_height_m = 10;
  in.rx_height_m = 2;
  o = link_budget(in);
  CHECK(o.effective_range_m == doctest::Approx(los_distance_km(10, 2) * 1000));
  // Log-distance model: steeper exponent → shorter range; continuous at 100 m.
  CHECK(log_distance_range_m(130, 915e6, 3.0) < log_distance_range_m(130, 915e6, 2.0));
  CHECK(log_distance_loss_db(100, 915e6, 3.5) == doctest::Approx(fspl_db(100, 915e6)));
  CHECK(log_distance_range_m(log_distance_loss_db(5000, 915e6, 3), 915e6, 3) == doctest::Approx(5000));
}

TEST_CASE("HF guidance picks sensible bands") {
  auto best = [](const std::vector<HfBandAdvice>& v) {
    std::set<std::string> s;
    int top = 0;
    for (const auto& b : v) top = b.score > top ? b.score : top;
    for (const auto& b : v)
      if (b.score == top) s.insert(b.band);
    return s;
  };
  auto day_nvis = best(hf_guidance(200, 12, 1, 1));
  CHECK((day_nvis.count("40 m") || day_nvis.count("60 m")));
  auto night_nvis = best(hf_guidance(200, 1, 1, 1));
  CHECK(night_nvis.count("80 m"));
  CHECK_FALSE(night_nvis.count("20 m"));
  auto day_dx = best(hf_guidance(6000, 13, 1, 1));
  CHECK(day_dx.count("20 m"));
  CHECK(hf_guidance(6000, 13, 1, 1).size() == 10);
}

TEST_CASE("antenna: dipole, ground plane, J-pole at 146 MHz") {
  Design d = dipole(146, 0.95);
  REQUIRE(d.ok());
  CHECK(dim(d, "total") == doctest::Approx(0.9753).epsilon(1e-3));  // 468/f ft = 0.977 m
  CHECK(dim(d, "leg") == doctest::Approx(dim(d, "total") / 2));
  CHECK_FALSE(d.diagram.empty());
  CHECK_FALSE(d.notes.empty());
  Design gp = ground_plane(146, 0.95, 45, 4);
  CHECK(dim(gp, "radiator") == doctest::Approx(0.4877).epsilon(1e-3));
  CHECK(dim(gp, "radial_drop") == doctest::Approx(dim(gp, "radial") * std::sqrt(0.5)));
  Design j = j_pole(146, 0.95);
  CHECK(dim(j, "long") == doctest::Approx(3 * dim(j, "stub")));
  CHECK(dim(j, "long") == doctest::Approx(1.463).epsilon(2e-3));  // common 2 m copper J ~57.5-58.5 in
}

TEST_CASE("antenna: inverted V geometry and errors") {
  Design d = inverted_v(7.1, 0.95, 10, 120);
  REQUIRE(d.ok());
  double leg = dim(d, "leg");
  CHECK(dim(d, "end_height") == doctest::Approx(10 - leg * 0.5));
  CHECK(dim(d, "span") == doctest::Approx(2 * leg * std::sin(60 * kPi / 180)));
  CHECK_FALSE(inverted_v(3.6, 0.95, 5, 90).ok());  // 80 m legs reach the ground
}

TEST_CASE("antenna: Yagi NBS table and Moxon regression") {
  Design y = yagi(146, 3, 17.5);
  REQUIRE(y.ok());
  double lam = wavelength_m(146e6);
  CHECK(dim(y, "reflector") == doctest::Approx(0.482 * lam));
  CHECK(dim(y, "d1") == doctest::Approx(0.442 * lam));
  CHECK(dim(y, "boom") == doctest::Approx(0.4 * lam));
  CHECK(yagi(146, 5, 10).ok());
  CHECK(dim(yagi(432, 6, 5), "d4") == doctest::Approx(0.428 * wavelength_m(432e6)));
  CHECK_FALSE(yagi(146, 4, 10).ok());
  CHECK_FALSE(yagi(146, 7, 10).ok());
  // Moxon at d/λ = 0.001 → A 0.3609 λ, B 0.0501 λ, C 0.0145 λ, D 0.0688 λ (Cebik regression).
  Design m = moxon(146, lam * 1000 * 0.001);
  REQUIRE(m.ok());
  CHECK(dim(m, "A") / lam == doctest::Approx(0.36093).epsilon(1e-3));
  CHECK(dim(m, "B") / lam == doctest::Approx(0.05014).epsilon(1e-3));
  CHECK(dim(m, "C") / lam == doctest::Approx(0.01451).epsilon(2e-3));
  CHECK(dim(m, "D") / lam == doctest::Approx(0.06879).epsilon(1e-3));
  CHECK(dim(m, "E") == doctest::Approx(dim(m, "B") + dim(m, "C") + dim(m, "D")));
  CHECK_FALSE(moxon(146, 100).ok());  // 100 mm wire at 2 m: d/λ out of range
}

TEST_CASE("antenna: tape-measure Yagi matches WB2HOL at its design frequency") {
  Design t = tape_yagi(146.565);
  CHECK(dim(t, "reflector") == doctest::Approx(41.375 * 0.0254));
  CHECK(dim(t, "director") == doctest::Approx(35.125 * 0.0254));
  CHECK(dim(tape_yagi(435), "reflector") < 0.4);
}

TEST_CASE("antenna: loops, EFHW, collinear, baluns") {
  Design q = full_wave_loop(14.2, 0, 0.66);
  CHECK(dim(q, "perimeter") == doctest::Approx(1005.0 / 14.2 * 0.3048));
  CHECK(dim(q, "side") == doctest::Approx(dim(q, "perimeter") / 4));
  CHECK(dim(full_wave_loop(14.2, 1, 0.66), "side") == doctest::Approx(dim(q, "perimeter") / 3));
  CHECK(dim(end_fed_half_wave(7.1, 0.95), "wire") == doctest::Approx(20.06).epsilon(2e-3));
  Design c = coaxial_collinear(868, 4, 0.66);
  CHECK(dim(c, "section") == doctest::Approx(wavelength_m(868e6) / 2 * 0.66));
  Design b = coax_balun(146, 1, 0.66, 0.95);
  CHECK(dim(b, "loop") == doctest::Approx(wavelength_m(146e6) / 2 * 0.66));
  CHECK(dim(coax_balun(146, 0, 0.66, 0.95), "sleeve") == doctest::Approx(wavelength_m(146e6) / 4 * 0.95));
}

TEST_CASE("antenna: coax choke inductance (Wheeler)") {
  // 8 turns, 4 in mean diameter (r = 2 in), 2 in long: L = 4·64 / (18 + 20) = 6.737 µH
  Design d = coax_choke(14.2, 101.6 - 6.35, 6.35, 8);
  bool found = false;
  for (const auto& v : d.values)
    if (v.key == "inductance") { found = true; CHECK(v.v == doctest::Approx(6.737).epsilon(1e-3)); }
  CHECK(found);
}

TEST_CASE("antenna: cantenna waveguide maths") {
  Design d = cantenna(2437, 83);
  REQUIRE(d.ok());
  CHECK(dim(d, "probe_pos") * 1000 == doctest::Approx(62.1).epsilon(5e-3));
  CHECK(dim(d, "probe_len") * 1000 == doctest::Approx(30.75).epsilon(2e-3));
  CHECK_FALSE(cantenna(2437, 60).ok());   // below TE11 cutoff
  CHECK_FALSE(cantenna(2437, 120).ok());  // TM01 propagates
  CHECK(dim(biquad(2437), "side") * 1000 == doctest::Approx(30.75).epsilon(2e-3));
}

TEST_CASE("antenna: random wire avoids half-wave multiples") {
  size_t nb;
  const HfBand* bands = hf_band_list(&nb);
  CHECK(nb == 10);
  unsigned mask = (1u << 1) | (1u << 3) | (1u << 5);  // 80, 40, 20 m
  Design d = random_wire(9, 40, mask, 0.95);
  REQUIRE(d.ok());
  REQUIRE_FALSE(d.dims.empty());
  for (const auto& x : d.dims) {
    CHECK(x.m >= 9);
    CHECK(x.m <= 40);
    for (int b : {1, 3, 5}) {
      for (int n = 1; n < 20; ++n) {
        double lo = n * kC / (bands[b].hi_mhz * 1e6) / 2 * 0.95, hi = n * kC / (bands[b].lo_mhz * 1e6) / 2 * 0.95;
        CHECK_FALSE((x.m > lo && x.m < hi));
      }
    }
  }
  CHECK_FALSE(random_wire(10, 5, mask, 0.95).ok());
  CHECK_FALSE(random_wire(5, 10, 0, 0.95).ok());
}

TEST_CASE("band plans: channel frequencies") {
  auto all = band_services();
  std::set<std::string> ids;
  for (const auto& s : all) {
    CHECK(ids.insert(s.id).second);
    CHECK_FALSE(s.name.empty());
    CHECK_FALSE(s.license.empty());
    CHECK_FALSE(s.channels.empty());
    for (const auto& c : s.channels) {
      CHECK(c.freq_hz > 0);
      CHECK_FALSE(c.name.empty());
    }
  }
  const BandService* frs = find_svc(all, "frs");
  REQUIRE(frs);
  REQUIRE(frs->channels.size() == 22);
  CHECK(frs->channels[0].freq_hz == doctest::Approx(462.5625e6));
  CHECK(frs->channels[7].freq_hz == doctest::Approx(467.5625e6));
  CHECK(frs->channels[14].freq_hz == doctest::Approx(462.5500e6));
  CHECK(frs->channels[21].freq_hz == doctest::Approx(462.7250e6));
  const BandService* cb = find_svc(all, "cb_us");
  REQUIRE(cb->channels.size() == 40);
  CHECK(cb->channels[8].freq_hz == doctest::Approx(27.065e6));   // ch 9
  CHECK(cb->channels[18].freq_hz == doctest::Approx(27.185e6));  // ch 19
  CHECK(cb->channels[22].freq_hz == doctest::Approx(27.255e6));  // ch 23 (out of order)
  CHECK(find_svc(all, "pmr446")->channels[15].freq_hz == doctest::Approx(446.19375e6));
  CHECK(find_svc(all, "lpd433")->channels.back().freq_hz == doctest::Approx(434.775e6));
  const BandService* uhf = find_svc(all, "uhf_cb_au");
  REQUIRE(uhf->channels.size() == 80);
  CHECK(uhf->channels[39].freq_hz == doctest::Approx(477.400e6));
  CHECK(uhf->channels[79].freq_hz == doctest::Approx(477.4125e6));
  CHECK(uhf->channels[34].freq_hz - uhf->channels[4].freq_hz == doctest::Approx(750e3));
  CHECK(find_svc(all, "murs")->channels.size() == 5);
  CHECK(find_svc(all, "noaa_wx")->channels.size() == 7);
  CHECK(find_svc(all, "ism_eu868")->channels[0].freq_hz == doctest::Approx(869.525e6));
  CHECK(find_svc(all, "marine_vhf")->channels[0].freq_hz == doctest::Approx(156.8e6));
  CHECK(find_svc(all, "cb_uk")->channels.back().freq_hz == doctest::Approx(27.99125e6));
}

TEST_CASE("band plans: filters") {
  auto us = band_services("us");
  CHECK(find_svc(us, "frs"));
  CHECK(find_svc(us, "ham_2m_r2"));
  CHECK(find_svc(us, "marine_vhf"));  // INTL always included
  CHECK_FALSE(find_svc(us, "pmr446"));
  CHECK_FALSE(find_svc(us, "ham_2m_r1"));
  auto eu = band_services("EU");
  CHECK(find_svc(eu, "pmr446"));
  CHECK(find_svc(eu, "ham_2m_r1"));
  CHECK_FALSE(find_svc(eu, "frs"));
  auto one = band_services("", "gmrs");
  REQUIRE(one.size() == 1);
  CHECK(one[0].id == "gmrs");
  CHECK(band_services("US", "pmr446").empty());
  CHECK(band_services("", "nope").empty());
  CHECK(band_services("ZZ").size() < band_services().size());
  CHECK_FALSE(band_regions().empty());
  bool emergency_note = false;
  for (const auto& n : band_general_notes()) emergency_note |= n.find("life is in immediate danger") != std::string::npos;
  CHECK(emergency_note);
}

TEST_CASE("band plans: every region code returns worldwide plus regional services") {
  auto has_regional = [](const std::vector<BandService>& v) {
    for (const auto& s : v)
      if (s.region != "INTL") return true;
    return false;
  };
  std::vector<std::string> codes = band_regions();
  for (const char* c : {"OTHER", "other", "BR", "ZA", "DE", "fr", "GB", "SG", "MX", "AR", "KE", "CN", "XX", "Q1", "ZZZZZZZZ"})
    codes.push_back(c);
  for (const auto& c : codes) {
    CAPTURE(c);
    auto v = band_services(c);
    CHECK(find_svc(v, "marine_vhf"));       // worldwide
    CHECK(find_svc(v, "aviation_emergency"));
    if (c != "INTL") CHECK(has_regional(v));
    CHECK_FALSE(band_region_tags(c).empty());
  }
  CHECK(band_region_tags("").empty());
  auto br = band_services("BR");
  CHECK(find_svc(br, "ism_br915"));
  CHECK(find_svc(br, "ham_2m_br"));
  CHECK(find_svc(br, "ham_2m_r2"));          // IARU Region 2
  CHECK_FALSE(find_svc(br, "frs"));
  CHECK_FALSE(find_svc(br, "ism_us915"));
  CHECK(find_svc(br, "ham_2m_br")->channels[0].freq_hz == doctest::Approx(145.570e6));
  auto za = band_services("za");
  CHECK(find_svc(za, "ism_za868"));
  CHECK(find_svc(za, "ham_2m_r1"));
  CHECK_FALSE(find_svc(za, "pmr446"));       // CEPT rules are not assumed for ZA
  CHECK_FALSE(find_svc(za, "ham_2m_r2"));
  auto de = band_services("DE");             // ISO code -> CEPT data set
  CHECK(find_svc(de, "pmr446"));
  CHECK(find_svc(de, "ism_eu868"));
  CHECK(find_svc(de, "ham_2m_r1"));
  CHECK(find_svc(band_services("GB"), "cb_uk"));
  CHECK(find_svc(band_services("SG"), "ism_as923"));
  CHECK(find_svc(band_services("NZ"), "ism_au915"));
  CHECK(find_svc(band_services("MX"), "ham_2m_r2"));
  auto other = band_services("OTHER");       // unknown: all IARU regions' band plans
  CHECK(find_svc(other, "ham_2m_r1"));
  CHECK(find_svc(other, "ham_2m_r2"));
  CHECK_FALSE(find_svc(other, "frs"));
  CHECK_FALSE(find_svc(other, "pmr446"));
  auto us = band_services("US");
  CHECK_FALSE(find_svc(us, "ism_br915"));
  CHECK_FALSE(find_svc(us, "ham_2m_br"));
  CHECK(band_region_tags("de") == std::vector<std::string>{"DE", "EU", "R1", "INTL"});
}

}  // TEST_SUITE
