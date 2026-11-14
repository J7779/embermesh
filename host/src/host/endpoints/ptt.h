// Transmitter keying helpers shared by endpoints (audio PTT via a serial control line or rigctld).
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "host/json.h"
#include "host/loop.h"

namespace ember_host {

class PttControl {
 public:
  virtual ~PttControl() = default;
  virtual void set(bool on) = 0;  // loop thread
  virtual bool ok() const = 0;
  virtual std::string describe() const = 0;
};

#ifdef EMBER_HOST_HAVE_SERIAL
// Opens `port` only for its modem-control lines and keys `line` ("rts" | "dtr" | "both").
// Implemented in endpoints/serial_port.cpp. Returns nullptr and sets *err on failure.
std::unique_ptr<PttControl> make_serial_line_ptt(const std::string& port, const std::string& line, bool invert,
                                                 std::string* err);
#endif

#ifdef EMBER_HOST_HAVE_AUDIO
// CM108/CM119-style HID GPIO keying (AIOC, many USB sound-card radio interfaces). device: Linux
// "/dev/hidrawN" or "auto"; macOS "VID:PID" or "auto". Implemented in endpoints/audio_ptt_cm108.cpp.
std::unique_ptr<PttControl> make_cm108_ptt(const std::string& device, int gpio, bool invert, std::string* err);
std::vector<std::string> list_cm108_devices();
#endif

#ifdef EMBER_HOST_HAVE_SERIAL
// host.endpoints.scan for serial ports (endpoints/serial.cpp); entries carry ptt_hint /
// audio_ptt_config for known radio interfaces (AIOC, Digirig, USB-serial adapters).
Json scan_serial(const Json& params);
#endif

// Keys via a Hamlib rigctld TCP server ("T 1" / "T 0"). Implemented in endpoints/rigctl.cpp.
std::unique_ptr<PttControl> make_rigctld_ptt(Loop& loop, const std::string& host, uint16_t port);

// Builds a PttControl from an endpoint config:
//   ptt: "none"|"vox"|"serial_rts"|"serial_dtr"|"rigctld", ptt_port, ptt_invert, rig_host, rig_port.
// Returns nullptr for none/vox. Implemented in endpoints/rigctl.cpp.
std::unique_ptr<PttControl> make_ptt_from_config(Loop& loop, const Json& cfg, std::string* err);

}  // namespace ember_host
