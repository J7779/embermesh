// Sound-card endpoint (miniaudio) for soft-modem links: radio audio in/out + PTT.
#pragma once

#include <string>
#include <vector>

namespace ember_host {

struct AudioDeviceInfo {
  std::string name;
  bool capture = false;  // true = input (microphone / radio RX audio), false = output
  bool is_default = false;
};

// Lists capture + playback devices of the default miniaudio backend. Thread-safe. Empty (and
// *error set) when no audio backend is usable.
std::vector<AudioDeviceInfo> enumerate_audio_devices(std::string* error = nullptr);

}  // namespace ember_host
