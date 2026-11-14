#include "rf/hardware.h"

#include <algorithm>

namespace ember {
namespace rf {

namespace {

bool has(const std::vector<std::string>& v, const std::string& x) { return std::find(v.begin(), v.end(), x) != v.end(); }

HardwareItem I(const char* name, const char* why, bool required, std::vector<std::string> alt = {}) {
  HardwareItem i;
  i.name = name;
  i.why = why;
  i.required = required;
  i.alternatives = std::move(alt);
  return i;
}

// Shared items and notes.
const char* kUsbSerial = "USB serial adapter with a CP210x, CH340/CH341, FTDI or PL2303 chip (or a CDC-ACM device)";
const char* kOtg = "USB-C (or micro-USB) OTG adapter";
const char* kWebSecure =
    "Microphone, camera, Web Serial and Web Bluetooth need a secure page: the single file "
    "(embermesh.html), https or localhost. A hub page at http://<ip>:8437/ is not secure.";
const char* kWebSerial = "Web Serial works only in Chrome / Edge on a computer (not on phones, Firefox or Safari).";
const char* kWebBle = "Web Bluetooth works in Chrome / Edge on a computer (Linux: behind a flag) and Chrome on Android.";
const char* kAndroidUsb = "Plug USB devices in with an OTG adapter; the app asks for USB permission.";
const char* kIosNoSerial = "iOS gives apps no USB serial and no Classic Bluetooth serial: use Bluetooth LE devices "
                           "or the audio path instead.";
const char* kDesktopSerial = "On Linux add your user to the dialout group (serial ports) and audio group (sound cards).";

HardwareItem radio_audio_cable() {
  return I("K1 two-pin radio audio cable (3.5 mm + 2.5 mm) to a 4-pole (TRRS) headset plug",
           "Carries the modem audio to the radio's microphone input and the radio's speaker audio back.", true,
           {"cable for your radio's own accessory jack", "USB sound-card radio interface (see the PTT interface method)"});
}
HardwareItem headset_adapter() {
  return I("USB-C (or Lightning) to 3.5 mm headset adapter with microphone support",
           "Phones without a headset jack need it to plug in the 4-pole radio cable.", false,
           {"phone or computer with a 3.5 mm headset jack", "TRRS splitter (separate mic and headphone plugs) on computers"});
}
HardwareItem attenuator() {
  return I("Audio attenuator / level-adjustable cable",
           "The radio microphone input is very sensitive; too much level distorts the transmitted signal.", false,
           {"turn the device volume down (start around 30 %)"});
}

std::vector<HardwareMethod> build() {
  std::vector<HardwareMethod> v;
  {
    HardwareMethod m;
    m.id = "radio_vox_cable";
    m.title = "Handheld radio by audio cable, keyed by its VOX";
    m.what_it_connects = "phone or computer ↔ handheld radio audio";
    m.link_type = "audio";
    m.preset = "handheld_vox";
    m.any_kinds = {"audio"};
    m.items = {I("FM handheld radio with VOX (e.g. the Baofeng UV-5R family)", "Transmits and receives the modem tones.", true,
                 {"any radio with VOX and an audio jack"}),
               radio_audio_cable(), headset_adapter(), attenuator()};
    m.note_android = "Choose the headset/USB audio device as input and output in the link settings.";
    m.note_ios = "Use a Lightning/USB-C headset adapter with microphone support.";
    m.note_web = kWebSecure;
    m.note_desktop = "A computer with a combined headset jack needs a TRRS cable; separate mic/line jacks need a splitter.";
    m.steps_summary = "Set VOX, squelch and power on the radio, plug the cable in, add an audio link with preset "
                      "handheld_vox, send a test message to a second station.";
    m.manual_chapter = "embermesh/handheld-audio-modem";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "radio_ptt_interface";
    m.title = "Radio with a USB sound-card + PTT interface";
    m.what_it_connects = "phone or computer ↔ radio audio + PTT line";
    m.link_type = "audio";
    m.preset = "handheld_ptt";
    m.any_kinds = {"audio"};
    m.all_kinds = {"serial"};
    m.items = {I("USB sound-card + serial-PTT radio interface (AIOC / Digirig class)",
                 "Gives a clean sound card and keys the transmitter by RTS/DTR, so no VOX is needed.", true,
                 {"separate USB sound card plus a USB serial adapter driving a PTT transistor"}),
               I("Interface lead for your radio's connector (e.g. K1 two-pin)", "Connects the interface to the radio.", true),
               I(kOtg, "Phones connect the interface's USB plug through it.", false, {"USB-C interface cable"}),
               I("FM radio", "The radio itself.", true)};
    m.note_android = kAndroidUsb;
    m.note_web = std::string(kWebSerial) + " " + kWebSecure;
    m.note_desktop = kDesktopSerial;
    m.note_ios = kIosNoSerial;
    m.steps_summary = "Connect the interface, pick its sound card and its serial port as PTT, add an audio link with "
                      "preset handheld_ptt.";
    m.manual_chapter = "embermesh/handheld-audio-modem";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "radio_acoustic";
    m.title = "Any radio with no cable (acoustic coupling)";
    m.what_it_connects = "device speaker/microphone ↔ radio microphone/speaker";
    m.link_type = "audio";
    m.preset = "acoustic";
    m.any_kinds = {"audio"};
    m.items = {I("Any voice radio (handheld, FRS/GMRS/PMR446, CB)", "You press its PTT when the app asks.", true),
               I("A quiet spot", "Room noise and echo slow the robust modem down.", false)};
    m.note_web = kWebSecure;
    m.steps_summary = "Add an audio link with preset acoustic, hold the device to the radio, press PTT when asked.";
    m.manual_chapter = "embermesh/handheld-audio-modem";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "radio_data_port";
    m.title = "Mobile / base radio with a data port or USB audio";
    m.what_it_connects = "computer or phone ↔ radio data port (audio + PTT)";
    m.link_type = "audio";
    m.preset = "sound_card";
    m.any_kinds = {"audio"};
    m.all_kinds = {"serial"};
    m.items = {I("Sound-card radio interface with a data-port lead (e.g. 6-pin mini-DIN)",
                 "Connects line-level audio and PTT to the data port.", true,
                 {"radio with a built-in USB sound card and CAT/CI-V serial port: just a USB cable"}),
               I(kUsbSerial, "Keys PTT (or the radio's own USB serial port does).", false),
               I(kOtg, "Phones connect USB devices through it.", false)};
    m.note_android = kAndroidUsb;
    m.note_web = std::string(kWebSerial) + " " + kWebSecure;
    m.note_desktop = kDesktopSerial;
    m.note_ios = kIosNoSerial;
    m.steps_summary = "Wire the data port, select the radio's sound card and PTT port, add an audio link with preset "
                      "sound_card.";
    m.manual_chapter = "embermesh/radio-gateway";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "virtual_audio";
    m.title = "Virtual audio pipe to other radio software";
    m.what_it_connects = "emberd ↔ another program's audio (pipes / files)";
    m.link_type = "audio";
    m.preset = "custom";
    m.any_kinds = {"audio_pipe"};
    m.items = {I("Nothing extra", "The audio_pipe endpoint uses named pipes or files of raw 16-bit PCM.", false)};
    m.note_desktop = "emberd only (endpoint kind audio_pipe).";
    m.steps_summary = "Start emberd with an audio_pipe endpoint and an audio link with your own settings.";
    m.manual_chapter = "embermesh/radio-gateway";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "bluetooth_tnc";
    m.title = "Bluetooth KISS TNC";
    m.what_it_connects = "phone or computer ↔ TNC ↔ radio";
    m.link_type = "kiss";
    m.any_kinds = {"ble", "bt_spp"};
    m.os_serial_platforms = {"desktop"};
    m.items = {I("Bluetooth KISS TNC (Bluetooth LE or Classic SPP)", "Does the packet modem in hardware.", true,
                 {"radio with a built-in Bluetooth KISS TNC"}),
               I("TNC-to-radio cable", "Connects the TNC to the radio's audio/PTT.", true),
               I("FM radio", "The radio itself.", true)};
    m.note_android = "Both Bluetooth LE and Classic (SPP) TNCs work.";
    m.note_ios = "Bluetooth LE TNCs only (Nordic UART or BLE KISS service); Classic SPP is not available to apps.";
    m.note_web = std::string(kWebBle) + " Bluetooth LE only. " + kWebSecure;
    m.note_desktop = "Pair the TNC in the operating system's Bluetooth settings; it then appears as a serial port: "
                     "add a kiss link on that serial endpoint.";
    m.steps_summary = "Pair/scan the TNC, add a kiss link with your callsign.";
    m.manual_chapter = "embermesh/kiss-tnc-packet";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "serial_tnc";
    m.title = "KISS TNC by USB serial or TCP (hardware TNC, Direwolf)";
    m.what_it_connects = "computer or phone ↔ TNC ↔ radio";
    m.link_type = "kiss";
    m.any_kinds = {"serial", "tcp"};
    m.items = {I("KISS TNC with USB or serial port", "Does the packet modem in hardware.", true,
                 {"Direwolf software TNC reached over TCP (no extra hardware beyond its sound card)"}),
               I(kUsbSerial, "Needed when the TNC has an RS-232 or TTL serial port.", false),
               I(kOtg, "Phones connect USB devices through it.", false),
               I("TNC-to-radio cable", "Connects the TNC to the radio.", true)};
    m.note_android = kAndroidUsb;
    m.note_ios = "USB serial is not available on iOS; Direwolf over TCP on another computer works.";
    m.note_web = std::string(kWebSerial) + " " + kWebSecure;
    m.note_desktop = kDesktopSerial;
    m.steps_summary = "Connect the TNC (or start Direwolf), add a kiss link over serial or TCP port 8001.";
    m.manual_chapter = "embermesh/kiss-tnc-packet";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "meshtastic";
    m.title = "Meshtastic LoRa board";
    m.what_it_connects = "phone or computer ↔ Meshtastic device";
    m.link_type = "meshtastic";
    m.any_kinds = {"serial", "ble", "tcp"};
    m.items = {I("LoRa board running stock Meshtastic firmware", "Sends Ember traffic as Meshtastic private-app packets.",
                 true),
               I("Antenna for the board's band", "Never transmit without an antenna.", true),
               I("USB cable", "Serial connection and power (not needed over Bluetooth or Wi-Fi).", false,
                 {"Bluetooth LE", "Wi-Fi (TCP port 4403)"}),
               I(kOtg, "Phones connect USB devices through it.", false)};
    m.note_android = kAndroidUsb;
    m.note_ios = "Bluetooth LE or Wi-Fi (TCP) only.";
    m.note_web = std::string(kWebSerial) + " " + kWebBle + " " + kWebSecure;
    m.note_desktop = std::string("USB serial or TCP. ") + kDesktopSerial;
    m.steps_summary = "Flash Meshtastic, set the region on the device, add a meshtastic link.";
    m.manual_chapter = "embermesh/lora";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "rnode";
    m.title = "RNode LoRa board";
    m.what_it_connects = "phone or computer ↔ RNode";
    m.link_type = "rnode";
    m.any_kinds = {"serial", "ble", "bt_spp", "tcp"};
    m.items = {I("LoRa board running RNode firmware", "A LoRa modem controlled by the app.", true),
               I("Antenna for the board's band", "Never transmit without an antenna.", true),
               I("USB cable", "Serial connection and power.", false, {"Bluetooth"}),
               I(kOtg, "Phones connect USB devices through it.", false)};
    m.note_android = kAndroidUsb;
    m.note_ios = "Bluetooth LE or TCP only.";
    m.note_web = std::string(kWebSerial) + " " + kWebBle + " " + kWebSecure;
    m.note_desktop = kDesktopSerial;
    m.steps_summary = "Flash RNode firmware, add an rnode link with your region's frequency.";
    m.manual_chapter = "embermesh/lora";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "lora_at";
    m.title = "AT-command LoRa module (REYAX RYLR class)";
    m.what_it_connects = "phone or computer ↔ LoRa module";
    m.link_type = "lora_at";
    m.any_kinds = {"serial", "ble", "bt_spp", "tcp"};
    m.items = {I("AT-command LoRa module (REYAX RYLR998/498/896/406 class)", "Sends and receives LoRa packets.", true),
               I("3.3 V USB serial adapter (CP210x, CH340/CH341, FTDI or PL2303)", "The module has a 3.3 V UART.", true,
                 {"Bluetooth LE / SPP serial bridge"}),
               I("Antenna for the module's band", "Never transmit without an antenna.", true),
               I(kOtg, "Phones connect USB devices through it.", false)};
    m.note_android = kAndroidUsb;
    m.note_ios = "Only through a Bluetooth LE serial bridge or TCP.";
    m.note_web = std::string(kWebSerial) + " " + kWebSecure;
    m.note_desktop = kDesktopSerial;
    m.steps_summary = "Wire the module to the adapter (3.3 V), add a lora_at link.";
    m.manual_chapter = "embermesh/lora";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "serial_radio_module";
    m.title = "Transparent serial radio module (Ebyte E22/E32, HC-12, SiK, XBee)";
    m.what_it_connects = "phone or computer ↔ serial radio module";
    m.link_type = "stream";
    m.preset = "ebyte";
    m.any_kinds = {"serial"};
    m.items = {I("Transparent serial radio module pair", "One per station; they act as a wireless serial cable.", true),
               I(kUsbSerial, "Connects the module's UART (check its logic voltage).", true),
               I(kOtg, "Phones connect USB devices through it.", false)};
    m.note_android = kAndroidUsb;
    m.note_web = std::string(kWebSerial) + " " + kWebSecure;
    m.note_desktop = kDesktopSerial;
    m.steps_summary = "Configure both modules alike, add a stream link with the matching preset (ebyte, hc12, sik, xbee).";
    m.manual_chapter = "embermesh/radio-gateway";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "sdr_dongle";
    m.title = "Receive-only SDR dongle (RTL-SDR class) via rtl_tcp";
    m.what_it_connects = "rtl_tcp server ↔ app (listen only)";
    m.link_type = "rtl_tcp";
    m.any_kinds = {"rtl_tcp", "tcp"};
    m.items = {I("RTL-SDR class USB dongle", "Receives the channel; it cannot transmit.", true),
               I("Antenna for the band", "Reception.", true),
               I("A device running an rtl_tcp server", "The app connects to rtl_tcp over the network.", true,
                 {"the same computer (127.0.0.1:1234)"})};
    m.note_android = "Connect to an rtl_tcp server over the network (tcp endpoint).";
    m.note_ios = "Connect to an rtl_tcp server over the network.";
    m.note_desktop = "Run rtl_tcp locally and connect to 127.0.0.1:1234.";
    m.steps_summary = "Start rtl_tcp, add an rtl_tcp link with the frequency.";
    m.manual_chapter = "embermesh/sdr-listening";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "phone_to_phone_direct";
    m.title = "Phone to phone, nothing else (Bluetooth LE / Wi-Fi Direct)";
    m.what_it_connects = "phone ↔ phone";
    m.link_type = "stream";
    m.preset = "ble";
    m.any_kinds = {"ble_peer", "wifi_direct"};
    m.items = {I("Nothing extra", "Both phones run the app; Bluetooth must be on.", false)};
    m.note_android = "Bluetooth LE peer service; Wi-Fi Direct between Android phones.";
    m.note_ios = "Bluetooth LE peer service (iPhone ↔ Android or iPhone).";
    m.steps_summary = "Turn on Bluetooth on both phones and add the phone-to-phone link.";
    m.manual_chapter = "embermesh/phone-to-phone";
    m.proven = "untested hardware";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "local_network";
    m.title = "Same Wi-Fi or Apple peer-to-peer, WebRTC pairing";
    m.what_it_connects = "devices on one network ↔ each other";
    m.link_type = "raw";
    m.any_kinds = {"udp", "multipeer", "webrtc"};
    m.items = {I("A shared Wi-Fi network or a phone hotspot", "Carries the multicast / peer traffic.", false,
                 {"Apple peer-to-peer (iPhones, no network needed)", "WebRTC pairing by QR on a hotspot"})};
    m.note_ios = "UDP multicast needs Apple's multicast entitlement; MultipeerConnectivity works without it.";
    m.note_web = "Browsers use WebRTC pairing (copy or scan the pairing blob), no UDP.";
    m.steps_summary = "Join the same Wi-Fi; the LAN link finds others automatically.";
    m.manual_chapter = "embermesh/phone-to-phone";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "cable_between_devices";
    m.title = "Serial cable between two devices";
    m.what_it_connects = "device ↔ device by cable";
    m.link_type = "stream";
    m.preset = "cable";
    m.any_kinds = {"serial"};
    m.items = {I(kUsbSerial, "One per device (USB devices are hosts, so two adapters are needed).", true),
               I("Null-modem cable (TX↔RX crossed, common ground)", "Connects the two adapters.", true),
               I(kOtg, "Phones connect USB devices through it.", false)};
    m.note_android = kAndroidUsb;
    m.note_web = std::string(kWebSerial) + " " + kWebSecure;
    m.note_desktop = kDesktopSerial;
    m.steps_summary = "Connect both adapters with the null-modem cable, add a stream link with preset cable.";
    m.manual_chapter = "embermesh/phone-to-phone";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "hub_hotspot";
    m.title = "Hub on a hotspot-capable phone or computer";
    m.what_it_connects = "hub ↔ every browser on its Wi-Fi";
    m.link_type = "raw";
    m.preset = "websocket";
    m.any_kinds = {"ws_server"};
    m.items = {I("Hotspot-capable phone or computer", "Creates the Wi-Fi that guests join and serves the web app.", true,
                 {"an existing Wi-Fi router everyone can join"}),
               I("Power bank or charger", "A hub runs for hours with Wi-Fi on.", false)};
    m.note_ios = "The hub runs only while the app is in the foreground; Personal Hotspot URL usually http://172.20.10.1:8437/.";
    m.note_android = "Hotspot addresses are listed first in the hub URLs.";
    m.note_desktop = "emberd serves the hub on port 8437; Raspberry Pi works as a hotspot hub.";
    m.steps_summary = "Turn on the hotspot, start the hub, share the URL or QR with guests.";
    m.manual_chapter = "embermesh/hub-setup";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "hub_guest";
    m.title = "Join a hub from any browser";
    m.what_it_connects = "phone or laptop ↔ hub";
    m.link_type = "raw";
    m.preset = "websocket";
    m.any_kinds = {"websocket"};
    m.items = {I("Nothing extra", "Join the hub's Wi-Fi and open its address.", false)};
    m.note_web = "The hub page is plain http, so radios, microphone and camera are unavailable there; use the single "
                 "file or an app for those.";
    m.steps_summary = "Join the Wi-Fi, open http://<hub-ip>:8437/.";
    m.manual_chapter = "embermesh/hub-setup";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "sneakernet";
    m.title = "Carry messages on a USB stick or card (sneakernet)";
    m.what_it_connects = "device ↔ storage ↔ device";
    m.link_type = "none";
    m.items = {I("USB stick or memory card", "Carries exported bundles between devices.", false,
                 {"file sharing (Bluetooth, AirDrop, Quick Share)", "QR codes for small bundles"}),
               I(kOtg, "Phones read USB sticks through it.", false)};
    m.note_ios = "Use the Files app for sticks on USB-C iPhones/iPads.";
    m.steps_summary = "Export a bundle, carry it, import it on the other device.";
    m.manual_chapter = "embermesh/sneakernet";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "qr_codes";
    m.title = "QR contact cards and invites";
    m.what_it_connects = "screen ↔ camera";
    m.link_type = "none";
    m.items = {I("Nothing extra", "Shown on one screen, scanned by the other device's camera.", false,
                 {"copy and paste the ember: link"})};
    m.note_web = std::string("QR scanning in the browser needs BarcodeDetector (Chrome on Android, macOS, ChromeOS). ") + kWebSecure;
    m.steps_summary = "Show your card, scan it on the other device.";
    m.manual_chapter = "embermesh/sneakernet";
    m.proven = "untested hardware";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "morse_light";
    m.title = "Morse by light (flashlight, lamp or screen)";
    m.what_it_connects = "light ↔ camera";
    m.link_type = "none";
    m.items = {I("Nothing extra", "The screen or torch flashes; another device's camera reads it.", false,
                 {"any flashlight or lamp keyed by hand"})};
    m.note_web = kWebSecure;
    m.steps_summary = "Point the camera at the light and start Morse receive.";
    m.manual_chapter = "embermesh/morse-reading";
    m.proven = "emulated";
    v.push_back(m);
  }
  {
    HardwareMethod m;
    m.id = "morse_audio";
    m.title = "Morse by sound (tone, radio speaker, whistle)";
    m.what_it_connects = "sound ↔ microphone";
    m.link_type = "none";
    m.items = {I("Nothing extra", "The device microphone hears the tone.", false, {"radio speaker audio by cable"})};
    m.note_web = kWebSecure;
    m.steps_summary = "Hold the device near the sound and start Morse receive.";
    m.manual_chapter = "embermesh/morse-reading";
    m.proven = "emulated";
    v.push_back(m);
  }
  return v;
}

}  // namespace

const std::vector<std::string>& hardware_platforms() {
  static const std::vector<std::string> p = {"android", "ios", "web", "desktop"};
  return p;
}

std::vector<std::string> platform_endpoint_kinds(const std::string& platform) {
  // Transcribed from the endpoint tables in apps/<platform>/README.md (kinds marked unavailable or
  // unsupported there are left out).
  if (platform == "android")
    return {"udp", "ble_peer", "audio", "serial", "ble", "bt_spp", "tcp", "tcp_server", "websocket", "wifi_direct",
            "loopback", "ws_server"};
  if (platform == "ios")
    return {"ble_peer", "multipeer", "audio", "tcp", "rtl_tcp", "tcp_server", "websocket", "udp", "ble", "loopback",
            "ws_server"};
  if (platform == "web") return {"websocket", "webrtc", "serial", "ble", "audio", "loopback"};
  if (platform == "desktop")
    return {"tcp", "tcp_server", "udp", "websocket", "ws_server", "serial", "audio", "audio_pipe", "rtl_tcp", "loopback"};
  return {};
}

std::vector<HardwareMethod> hardware_methods(const std::string& platform, const std::string& method) {
  std::vector<HardwareMethod> out;
  for (HardwareMethod& m : build()) {
    if (!method.empty() && m.id != method) continue;
    for (const std::string& p : hardware_platforms()) {
      std::vector<std::string> k = platform_endpoint_kinds(p);
      bool any = m.any_kinds.empty();
      for (const std::string& x : m.any_kinds) any = any || has(k, x);
      if (has(m.os_serial_platforms, p) && has(k, "serial")) any = true;
      bool all = true;
      for (const std::string& x : m.all_kinds) all = all && has(k, x);
      if (any && all) m.platforms.push_back(p);
    }
    // Notes stay for platforms where the method does not work: they say why (e.g. no USB serial on iOS).
    if (!platform.empty() && !has(m.platforms, platform)) continue;
    out.push_back(std::move(m));
  }
  return out;
}

}  // namespace rf
}  // namespace ember
