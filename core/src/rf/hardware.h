// EmberMesh core — "What you need": hardware for each way of connecting (RPC `rf.hardware`,
// docs/RPC_API.md §6). Static, offline data.
//
// Platform support is derived, not hand-written: each method names the endpoint kinds it can use
// (`any_kinds`, plus `all_kinds` that must all be present, e.g. audio + serial for a PTT line), and
// `platforms` is every platform whose endpoint table (platform_endpoint_kinds(), transcribed from
// apps/{android,ios,web,desktop}/README.md) provides them. Only generic part classes and the chip
// families the platform drivers support are named; no product claims. `proven` is "emulated" when
// the software path has been exercised against emulators/simulators, else "untested hardware".
#pragma once

#include <string>
#include <vector>

namespace ember {
namespace rf {

struct HardwareItem {
  std::string name;  // generic part class
  std::string why;
  bool required = true;
  std::vector<std::string> alternatives;
};

struct HardwareMethod {
  std::string id;
  std::string title;
  std::string what_it_connects;
  std::string link_type;  // core link type, or "none" (no link: sneakernet, QR, Morse)
  std::string preset;     // driver preset when relevant, else ""
  std::vector<std::string> any_kinds;  // endpoint kinds, at least one needed ("" list = no endpoint)
  std::vector<std::string> all_kinds;  // endpoint kinds that must all be available as well
  // Platforms whose operating system presents the device as a serial port once paired (Bluetooth
  // TNC on a desktop): there the "serial" endpoint kind counts as well.
  std::vector<std::string> os_serial_platforms;
  std::vector<std::string> platforms;  // derived (see above)
  std::string note_android, note_ios, note_web, note_desktop;  // platform_notes; "" = none
  std::vector<HardwareItem> items;
  std::string steps_summary;
  std::string manual_chapter;  // guide id
  std::string proven;          // "emulated" | "untested hardware"
};

// The four app platforms, in display order.
const std::vector<std::string>& hardware_platforms();
// Endpoint kinds a platform provides (empty for an unknown platform).
std::vector<std::string> platform_endpoint_kinds(const std::string& platform);

// All methods, optionally filtered: platform ("" = any; otherwise methods available there) and
// method id ("" = all).
std::vector<HardwareMethod> hardware_methods(const std::string& platform = "", const std::string& method = "");

}  // namespace rf
}  // namespace ember
