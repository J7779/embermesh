// rtl_tcp protocol helpers (osmocom rtl-sdr's rtl_tcp server, default TCP 1234).
//
// Server -> client: 12-byte header "RTL0" + tuner type (BE32) + tuner gain count (BE32), then raw
// interleaved unsigned 8-bit I/Q samples forever.
// Client -> server: 5-byte commands: cmd (u8) + parameter (BE32).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ember_host {

enum RtlTcpCmd : uint8_t {
  kRtlSetFreq = 0x01,            // Hz
  kRtlSetSampleRate = 0x02,      // samples/s
  kRtlSetGainMode = 0x03,        // 0 = auto, 1 = manual
  kRtlSetGain = 0x04,            // tenths of dB (signed, sent as u32)
  kRtlSetFreqCorrection = 0x05,  // ppm (signed, sent as u32)
  kRtlSetIfGain = 0x06,          // (stage << 16) | (tenths dB & 0xffff)
  kRtlSetTestMode = 0x07,
  kRtlSetAgcMode = 0x08,         // 0/1
  kRtlSetDirectSampling = 0x09,  // 0 off, 1 I branch, 2 Q branch
  kRtlSetOffsetTuning = 0x0a,
  kRtlSetRtlXtal = 0x0b,
  kRtlSetTunerXtal = 0x0c,
  kRtlSetGainByIndex = 0x0d,
  kRtlSetBiasTee = 0x0e,         // 0/1
};

// 5 bytes: cmd + big-endian param.
std::string rtltcp_command(uint8_t cmd, uint32_t param);
inline std::string rtltcp_command_signed(uint8_t cmd, int32_t param) {
  return rtltcp_command(cmd, static_cast<uint32_t>(param));
}

struct RtlTcpHeader {
  uint32_t tuner = 0;
  uint32_t gain_count = 0;
};
constexpr size_t kRtlTcpHeaderLen = 12;
// Returns 1 = parsed, 0 = need more bytes (len < 12), -1 = bad magic.
int rtltcp_parse_header(const uint8_t* data, size_t len, RtlTcpHeader* out);
const char* rtltcp_tuner_name(uint32_t tuner);

// Valid RTL2832U sample rates: 225001..300000 and 900001..3200000.
bool rtltcp_valid_sample_rate(uint32_t rate);

}  // namespace ember_host
