#include "link/lora_common.h"

#include <cmath>

namespace ember {
namespace link {

const std::vector<LoraRegion>& lora_regions() {
  static const std::vector<LoraRegion> r = {
      // EU 863-870: the 869.40-869.65 MHz sub-band allows 10 % duty cycle and 500 mW ERP.
      {"eu868", "Europe 868 MHz (869.525 MHz, 10 % duty cycle)", 869525000, 100, 14, 863000000, 870000000},
      // EU 868.0-868.6 MHz sub-band: 1 % duty cycle, 25 mW ERP.
      {"eu868_1pct", "Europe 868.1 MHz (1 % duty cycle)", 868100000, 10, 14, 863000000, 870000000},
      // EU 433.05-434.79 MHz SRD band: 10 % duty cycle, 10 mW ERP.
      {"eu433", "Europe 433 MHz (433.775 MHz, 10 % duty cycle)", 433775000, 100, 10, 433050000, 434790000},
      {"us915", "Americas 915 MHz (914.875 MHz)", 914875000, 1000, 17, 902000000, 928000000},
      {"au915", "Australia / NZ 915 MHz (917 MHz)", 917000000, 1000, 17, 915000000, 928000000},
      {"in865", "India 865 MHz (866 MHz)", 866000000, 1000, 14, 865000000, 867000000},
      // AS923-1 channel, inside Japan's 920.6-928 MHz and Korea's 920.9-923.3 MHz bands. Japan and
      // Korea require listen-before-talk (the firmware's job) and low power: conservative 13 dBm, 10 %.
      {"as923", "Asia 923 MHz (923.2 MHz, JP / KR / SG / TH ...)", 923200000, 100, 13, 920000000, 925000000},
      {"custom", "Other / custom frequency", 869525000, 1000, 10, 0, 0},
  };
  return r;
}

const LoraRegion& lora_region(const std::string& id) {
  for (const auto& r : lora_regions())
    if (id == r.id) return r;
  return lora_regions()[0];
}

const char* lora_region_for_setting(const std::string& region_in) {
  std::string r;
  for (char c : region_in) r.push_back((c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c);
  auto any = [&](std::initializer_list<const char*> l) {
    for (const char* x : l)
      if (r == x) return true;
    return false;
  };
  if (r.empty()) return nullptr;
  // Europe (CEPT 863-870 MHz SRD rules), UK, and South Africa (EU868 plan).
  if (any({"EU", "UK", "GB", "ZA", "AT", "BE", "BG", "HR", "CY", "CZ", "DK", "EE", "FI", "FR", "DE", "GR", "HU",
           "IE", "IT", "LV", "LT", "LU", "MT", "NL", "PL", "PT", "RO", "SK", "SI", "ES", "SE", "NO", "IS", "LI",
           "CH", "R1"}))
    return "eu868";
  // North and Central America, Caribbean: 902-928 MHz.
  if (any({"US", "CA", "MX", "PR", "VI", "GU", "GT", "HN", "SV", "NI", "CR", "PA", "CU", "DO", "HT", "JM", "TT",
           "BS", "BB", "BZ", "R2"}))
    return "us915";
  // Australia, New Zealand, and South America: 917 MHz lies inside both 915-928 MHz (AU/NZ,
  // Brazil's upper ISM segment, AU915-plan countries) and 902-928 MHz.
  if (any({"AU", "NZ", "BR", "AR", "CL", "CO", "PE", "EC", "BO", "PY", "UY", "VE", "GY", "SR"})) return "au915";
  if (r == "IN") return "in865";
  if (any({"JP", "KR", "AS", "SG", "TH", "MY", "VN", "PH", "HK", "TW", "ID", "BN", "KH", "LA", "MM"})) return "as923";
  return nullptr;
}

std::vector<FieldOption> lora_region_options() {
  std::vector<FieldOption> o;
  for (const auto& r : lora_regions()) o.push_back({r.id, r.label});
  return o;
}

uint32_t lora_bitrate(uint8_t sf, uint32_t bw_hz, uint8_t cr_den) {
  if (sf < 5 || sf > 12 || bw_hz == 0 || cr_den < 5 || cr_den > 8) return 1;
  double r = sf * (static_cast<double>(bw_hz) / static_cast<double>(1u << sf)) * 4.0 / cr_den;
  return r < 1 ? 1 : static_cast<uint32_t>(r);
}

uint32_t lora_airtime_ms(size_t payload, uint8_t sf, uint32_t bw_hz, uint8_t cr_den, uint16_t preamble) {
  if (sf < 5 || sf > 12 || bw_hz == 0 || cr_den < 5 || cr_den > 8) return 0;
  double tsym = static_cast<double>(1u << sf) / bw_hz * 1000.0;  // ms
  int de = tsym > 16.0 ? 1 : 0;
  double num = 8.0 * static_cast<double>(payload) - 4.0 * sf + 28 + 16;  // CRC on, explicit header
  double den = 4.0 * (sf - 2 * de);
  double extra = std::ceil(num / den) * cr_den;
  if (extra < 0) extra = 0;
  double symbols = 8 + extra;
  double t = (preamble + 4.25) * tsym + symbols * tsym;
  return static_cast<uint32_t>(std::ceil(t));
}

}  // namespace link
}  // namespace ember
