// EmberMesh core / link — LoRa helpers shared by the rnode and lora_at drivers: regional presets,
// nominal bit rate and Semtech time-on-air.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "link/link_util.h"

namespace ember {
namespace link {

struct LoraRegion {
  const char* id;
  const char* label;
  uint32_t freq_hz;         // default channel
  uint16_t duty_permille;   // legal duty-cycle limit on that channel (1000 = none)
  int8_t power_dbm;         // conservative default TX power
  uint32_t min_hz, max_hz;  // band edges a frequency override must stay within (0 = any)
};

// Regional presets ("custom" last: no band restriction, no duty limit).
const std::vector<LoraRegion>& lora_regions();
const LoraRegion& lora_region(const std::string& id);  // unknown -> first entry
std::vector<FieldOption> lora_region_options();

// The LoRa preset id that suits a node `region` setting (data-set code such as "US", "EU", "UK",
// "AU", "BR", "ZA" or an ISO 3166 country code, case-insensitive), or nullptr when we do not know
// (e.g. "OTHER"): callers then keep the driver default.
const char* lora_region_for_setting(const std::string& region);

// Nominal LoRa bit rate: SF * (BW / 2^SF) * 4 / cr_den, cr_den = 5..8 (coding rate 4/5..4/8).
uint32_t lora_bitrate(uint8_t sf, uint32_t bw_hz, uint8_t cr_den);

// Semtech SX127x/SX126x time on air (explicit header, CRC on), in ms rounded up. Low-data-rate
// optimisation is assumed on when the symbol time exceeds 16 ms (as firmwares do).
uint32_t lora_airtime_ms(size_t payload, uint8_t sf, uint32_t bw_hz, uint8_t cr_den, uint16_t preamble = 8);

}  // namespace link
}  // namespace ember
