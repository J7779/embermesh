#include "rf/radios.h"

#include <cctype>

namespace ember {
namespace rf {

namespace {

// Shared wording for cables (no pinouts are given anywhere: buy or check a documented cable).
const char* kK1Cable =
    "Kenwood-style K1 two-pin accessory jack (a 3.5 mm and a 2.5 mm plug side by side). Easiest: a "
    "ready-made 'APRS-K1' / 'K1 audio' cable to your phone or computer headset jack, or a USB interface "
    "made for K1 radios (AIOC, Digirig with a K1 cable) that gives a sound card plus a PTT line. Keep "
    "the device's output level low (start around 30 %) - the radio microphone input is far more "
    "sensitive than a line input; an attenuator or the interface's level control avoids distortion. "
    "Ground-loop hum or RF feedback: use the interface's isolation, ferrites, and lower transmit power.";

const char* kDataPortCable =
    "Use the radio's data / accessory port (on many mobiles a 6-pin mini-DIN 'DATA' jack carrying "
    "1200/9600 packet audio, PTT and ground) with a sound-card interface (Digirig, SignaLink-style "
    "interfaces, or a cable for your interface). Take the pin assignment from the radio's manual. Use a "
    "fixed receive level (data port audio is not affected by the volume knob on most radios).";

const char* kAcousticNotes =
    "No cable: hold the phone or computer speaker close to the radio's microphone and its microphone "
    "close to the radio's speaker, in a quiet place. Press and hold the radio's PTT "
    "while the app shows TX ON AIR. Turn the radio volume to a moderate level; too loud distorts.";

RadioSetting S(const char* label, const char* value, const char* how) { return {label, value, how}; }

std::vector<RadioSetting> generic_handheld_settings(bool vox) {
  std::vector<RadioSetting> v = {
      S("Squelch", "Low but closed (radio silent with no signal)",
        "Squelch menu; too high hides weak packets, fully open (0) lets noise keep the channel busy."),
      S("Transmit power", "Low to start",
        "Power menu or key; raise only if the other station cannot hear you."),
      S("Bandwidth", "Same as the other stations (wide/narrow)", "Wide/narrow menu."),
      S("Dual watch", "Off", "Dual watch / dual standby menu - it mutes audio while it scans."),
      S("Battery save", "Off", "Battery saver menu - it clips the first part of received packets."),
      S("Roger beep / end tones", "Off", "Roger or end-of-transmission tone menu."),
      S("Keypad beep and voice prompts", "Off", "Beep and voice menus - they mix into the data audio."),
      S("Transmit time-out", "60 s or longer", "TOT menu - a long message burst must not be cut off."),
      S("CTCSS / DCS tones", "Off for simplex data", "Tone squelch menus (unless your group agreed on one)."),
  };
  if (vox)
    v.push_back(S("VOX", "On, low sensitivity number first",
                  "VOX menu. Raise the level if the radio keys up by itself, lower it if packets start late."));
  return v;
}

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::vector<RadioProfile> build() {
  std::vector<RadioProfile> r;

  // ---------------------------------------------------------------- Baofeng UV-5R family (verified)
  {
    RadioProfile p;
    p.id = "baofeng_uv5r";
    p.maker = "Baofeng";
    p.model = "UV-5R / UV-5RA / UV-5RE / BF-F8HP";
    p.kind = "handheld";
    p.bands = {"2m", "70cm"};
    p.connector = "K1 2-pin (3.5 mm + 2.5 mm)";
    p.vox = true;
    p.power_w = {1, 4};
    p.verified = true;
    p.settings = {
        S("Squelch", "1-3 (closed with no signal)", "MENU 0 SQL"),
        S("Transmit power", "LOW to start (BF-F8HP: LOW/MID)", "MENU 2 TXP"),
        S("Battery save", "OFF", "MENU 3 SAVE"),
        S("VOX", "1-3 to start (OFF disables)", "MENU 4 VOX - raise if it keys by itself, lower if packets start late"),
        S("Bandwidth", "WIDE (or NARR if everyone else uses narrow)", "MENU 5 WN"),
        S("Dual watch", "OFF", "MENU 7 TDR"),
        S("Keypad beep", "OFF", "MENU 8 BEEP"),
        S("Transmit time-out", "60 s or more", "MENU 9 TOT"),
        S("Tone squelch / tones", "OFF for simplex data", "MENU 10 R-DCS, 11 R-CTCS, 12 T-DCS, 13 T-CTCS"),
        S("Voice prompts", "OFF", "MENU 14 VOICE"),
        S("PTT ID", "OFF", "MENU 19 PTT-ID"),
        S("Roger beep", "OFF", "MENU 39 ROGER"),
    };
    p.cable_notes = kK1Cable;
    p.notes =
        "Menu numbers from the Baofeng UV-5R series menu definitions (miklor.com 'UV5R Menu Definitions', "
        "derived from the Baofeng manual). Numbering can differ on other firmware versions and clones - "
        "check the name shown on the display. The BF-F8HP adds a MID power level. With VOX, keep the "
        "phone/computer volume constant and do not use the radio for voice while VOX is on.";
    p.preset = "handheld_vox";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"handheld_vox","modem":"afsk1200"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "baofeng_uv82";
    p.maker = "Baofeng";
    p.model = "UV-82 / UV-82HP / GT-3 family";
    p.kind = "handheld";
    p.bands = {"2m", "70cm"};
    p.connector = "K1 2-pin (3.5 mm + 2.5 mm)";
    p.vox = true;
    p.settings = generic_handheld_settings(true);
    p.cable_notes = kK1Cable;
    p.notes = "Same settings as the UV-5R in spirit, but the menu numbers differ by model: use the menu names. "
              "The UV-82 dual PTT key selects which display line transmits.";
    p.preset = "handheld_vox";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"handheld_vox"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "baofeng_bf888s";
    p.maker = "Baofeng";
    p.model = "BF-888S";
    p.kind = "handheld";
    p.bands = {"70cm"};
    p.connector = "K1 2-pin (3.5 mm + 2.5 mm)";
    p.vox = true;
    p.settings = {
        S("Channels", "Program the data channel with a PC", "No display or keypad: use the programming software "
                                                            "(e.g. CHIRP) for frequency, power, squelch and VOX."),
        S("VOX", "On", "Programming software setting."),
        S("Squelch", "Low but closed", "Programming software setting."),
        S("Battery save and voice prompts", "Off", "Programming software settings."),
    };
    p.cable_notes = kK1Cable;
    p.notes = "UHF-only channel radio without VFO; all settings are made with programming software.";
    p.preset = "handheld_vox";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"handheld_vox"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "baofeng_uv9r";
    p.maker = "Baofeng";
    p.model = "UV-9R family (waterproof)";
    p.kind = "handheld";
    p.bands = {"2m", "70cm"};
    p.connector = "Model-specific sealed accessory connector on many versions (not always K1)";
    p.vox = true;
    p.settings = generic_handheld_settings(true);
    p.cable_notes = "Waterproof models often use their own accessory plug: buy the audio cable or adapter sold "
                    "for your exact model, or use acoustic coupling.";
    p.notes = "Menu numbers not verified for this family; use the menu names.";
    p.preset = "handheld_vox";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"handheld_vox"})";
    r.push_back(p);
  }
  // ---------------------------------------------------------------- other handhelds
  {
    RadioProfile p;
    p.id = "quansheng_uvk5";
    p.maker = "Quansheng";
    p.model = "UV-K5 / UV-K6 / UV-5R Plus";
    p.kind = "handheld";
    p.bands = {"2m", "70cm"};
    p.connector = "K1 2-pin (3.5 mm + 2.5 mm)";
    p.vox = true;
    p.settings = generic_handheld_settings(true);
    p.cable_notes = kK1Cable;
    p.notes = "Menu numbering differs between factory firmware versions and the popular custom firmwares, so "
              "use the menu names (squelch, VOX, battery save, dual watch, roger, TX time-out).";
    p.preset = "handheld_vox";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"handheld_vox"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "generic_k1";
    p.maker = "Generic";
    p.model = "K1 two-pin handheld (Retevis, TYT, Wouxun, Radtel, ...)";
    p.kind = "handheld";
    p.bands = {"2m", "70cm"};
    p.connector = "K1 2-pin (3.5 mm + 2.5 mm)";
    p.vox = true;
    p.settings = generic_handheld_settings(true);
    p.cable_notes = kK1Cable;
    p.notes = "Most low-cost handhelds use the Kenwood-style K1 jack. If yours has no VOX, use a USB interface "
              "with a PTT line (preset handheld_ptt) or acoustic coupling.";
    p.preset = "handheld_vox";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"handheld_vox"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "yaesu_ft60r";
    p.maker = "Yaesu";
    p.model = "FT-60R";
    p.kind = "handheld";
    p.bands = {"2m", "70cm"};
    p.connector = "Yaesu accessory jack (use a cable made for the FT-60)";
    p.vox = true;
    p.settings = generic_handheld_settings(true);
    p.cable_notes = "Not a K1 jack: use an audio/APRS cable or interface lead sold for the FT-60, or acoustic "
                    "coupling. Keep the device output level low.";
    p.notes = "Menu numbers not verified; see the FT-60R operating manual for the VOX and squelch menus.";
    p.preset = "handheld_vox";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"handheld_vox"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "yaesu_ft65r_ft4x";
    p.maker = "Yaesu";
    p.model = "FT-65R / FT-4XR / FT-4XE";
    p.kind = "handheld";
    p.bands = {"2m", "70cm"};
    p.connector = "Yaesu accessory jack (use a cable made for your model)";
    p.vox = true;
    p.settings = generic_handheld_settings(true);
    p.cable_notes = "Use the audio cable or interface lead sold for your model, or acoustic coupling.";
    p.notes = "VOX operation is described in the Yaesu FT-65R advance manual and the FT-4XR/XE operating "
              "manual; the exact menu steps were not verified here.";
    p.preset = "handheld_vox";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"handheld_vox"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "kenwood_thk20";
    p.maker = "Kenwood";
    p.model = "TH-K20";
    p.kind = "handheld";
    p.bands = {"2m"};
    p.connector = "Kenwood 2-pin (3.5 mm + 2.5 mm)";
    p.settings = generic_handheld_settings(false);
    p.cable_notes = kK1Cable;
    p.notes = "Simple 2 m handheld. VOX details not verified here; a USB interface with a PTT line "
              "(preset handheld_ptt) works regardless.";
    p.preset = "handheld_ptt";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"handheld_ptt"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "kenwood_thd72";
    p.maker = "Kenwood";
    p.model = "TH-D72";
    p.kind = "handheld";
    p.bands = {"2m", "70cm"};
    p.connector = "USB (KISS TNC) and Kenwood 2-pin audio";
    p.builtin_tnc = true;
    p.settings = {S("TNC", "KISS mode", "Switch the built-in TNC to KISS mode (see the manual), connect by USB."),
                  S("Data band", "The band/frequency your group uses", "Select the TNC data band.")};
    p.cable_notes = "USB cable to a computer or an OTG-capable phone; the radio appears as a serial port.";
    p.notes = "Built-in 1200/9600 TNC: use the kiss link instead of audio.";
    p.link_type = "kiss";
    p.link_config_json = R"({"bitrate_bps":1200})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "kenwood_thd74_thd75";
    p.maker = "Kenwood";
    p.model = "TH-D74 / TH-D75";
    p.kind = "handheld";
    p.bands = {"2m", "70cm"};
    p.connector = "USB and Bluetooth (KISS TNC)";
    p.builtin_tnc = true;
    p.bluetooth = true;
    p.settings = {S("TNC", "KISS mode", "Switch the built-in TNC to KISS mode (see the manual)."),
                  S("Bluetooth", "On and paired", "Pair with the phone/computer; the TNC is a Bluetooth serial port.")};
    p.cable_notes = "No cable needed with Bluetooth (bt_spp endpoint); USB also works.";
    p.notes = "Built-in KISS TNC: use the kiss link over Bluetooth serial or USB.";
    p.link_type = "kiss";
    p.link_config_json = R"({"bitrate_bps":1200})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "icom_id52";
    p.maker = "Icom";
    p.model = "ID-52";
    p.kind = "handheld";
    p.bands = {"2m", "70cm"};
    p.connector = "Icom accessory jack, USB, Bluetooth";
    p.bluetooth = true;
    p.settings = generic_handheld_settings(false);
    p.cable_notes = "Use an audio interface lead made for this model, or acoustic coupling.";
    p.notes = "Analog FM data through an audio interface; data-port and Bluetooth data details not verified here.";
    p.preset = "handheld_ptt";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"handheld_ptt"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "icom_ict70";
    p.maker = "Icom";
    p.model = "IC-T70";
    p.kind = "handheld";
    p.bands = {"2m", "70cm"};
    p.connector = "Icom accessory jack";
    p.settings = generic_handheld_settings(false);
    p.cable_notes = "Use an audio interface lead made for this model, or acoustic coupling.";
    p.notes = "Settings not verified here.";
    p.preset = "handheld_ptt";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"handheld_ptt"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "anytone_878uv";
    p.maker = "AnyTone";
    p.model = "AT-D878UV (Plus)";
    p.kind = "handheld";
    p.bands = {"2m", "70cm"};
    p.connector = "K1 2-pin (3.5 mm + 2.5 mm)";
    p.bluetooth = true;
    p.vox = true;
    p.settings = generic_handheld_settings(true);
    p.cable_notes = std::string(kK1Cable) + " Bluetooth on the Plus models is for audio headsets, not a data port.";
    p.notes = "Use an analog channel for data. Menu paths not verified here.";
    p.preset = "handheld_vox";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"handheld_vox"})";
    r.push_back(p);
  }
  // ---------------------------------------------------------------- mobiles / bases
  {
    RadioProfile p;
    p.id = "generic_data_port";
    p.maker = "Generic";
    p.model = "Mobile / base radio with a 6-pin mini-DIN data port";
    p.kind = "mobile";
    p.bands = {"2m", "70cm"};
    p.connector = "6-pin mini-DIN DATA jack";
    p.data_port = true;
    p.settings = {S("Data speed", "1200 bps (packet port setting)", "Data/packet menu of the radio."),
                  S("Squelch", "Low but closed", "Squelch control."),
                  S("Transmit power", "Low to start", "Power key or menu.")};
    p.cable_notes = kDataPortCable;
    p.notes = "Generic entry for packet-ready mobiles.";
    p.preset = "sound_card";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"sound_card"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "yaesu_ft817_family";
    p.maker = "Yaesu";
    p.model = "FT-817 / FT-818 / FT-857 / FT-897";
    p.kind = "mobile";
    p.bands = {"HF", "6m", "2m", "70cm"};
    p.connector = "6-pin mini-DIN DATA jack";
    p.data_port = true;
    p.settings = {S("Mode", "FM for 1200 packet; USB/PKT for the HF/robust modes", "Mode key / PKT mode menu."),
                  S("Data level", "Set so the transmitted audio does not distort", "Data level menu (see manual).")};
    p.cable_notes = kDataPortCable;
    p.notes = "Menu numbers not verified here.";
    p.preset = "sound_card";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"sound_card"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "yaesu_ft991a";
    p.maker = "Yaesu";
    p.model = "FT-991A";
    p.kind = "base";
    p.bands = {"HF", "6m", "2m", "70cm"};
    p.connector = "USB (built-in sound card + CAT) and rear DATA jack";
    p.data_port = true;
    p.settings = {S("Data input", "USB (rear)", "Data mode input selection menu (see manual)."),
                  S("Mode", "DATA-FM or DATA-USB", "Mode key.")};
    p.cable_notes = "A single USB cable gives a sound card and a serial port (CAT for PTT).";
    p.notes = "Menu numbers not verified here.";
    p.preset = "sound_card";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"sound_card"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "icom_usb_audio";
    p.maker = "Icom";
    p.model = "IC-705 / IC-7100 / IC-7300";
    p.kind = "base";
    p.bands = {"HF", "6m"};
    p.connector = "USB (built-in sound card + CI-V)";
    p.data_port = true;
    p.bluetooth = false;
    p.settings = {S("Modulation input in data mode", "USB", "Connectors menu (see manual)."),
                  S("Mode", "Data mode of the band (e.g. USB-D / FM-D)", "Mode key.")};
    p.cable_notes = "A single USB cable gives a sound card and a serial port (CI-V) for PTT. The IC-7100 and "
                    "IC-705 also cover 2 m / 70 cm.";
    p.notes = "Menu paths not verified here.";
    p.preset = "sound_card";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"sound_card"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "kenwood_tmd710";
    p.maker = "Kenwood";
    p.model = "TM-D710";
    p.kind = "mobile";
    p.bands = {"2m", "70cm"};
    p.connector = "Serial (built-in TNC) and data jack";
    p.builtin_tnc = true;
    p.data_port = true;
    p.settings = {S("TNC", "KISS mode", "Switch the built-in TNC to KISS mode (see the manual).")};
    p.cable_notes = "Serial cable from the control head / radio to a USB-serial adapter.";
    p.notes = "Built-in TNC: use the kiss link.";
    p.link_type = "kiss";
    p.link_config_json = R"({"bitrate_bps":1200})";
    r.push_back(p);
  }
  // ---------------------------------------------------------------- licence-free / CB
  {
    RadioProfile p;
    p.id = "generic_frs_pmr";
    p.maker = "Generic";
    p.model = "FRS / GMRS / PMR446 blister-pack radio";
    p.kind = "handheld";
    p.bands = {"FRS/GMRS", "PMR446"};
    p.connector = "None (acoustic coupling only)";
    p.settings = {S("Privacy codes", "Off (or the same on every radio)", "Code / CTCSS menu."),
                  S("Roger beep and key tones", "Off", "Tone menus."),
                  S("VOX", "Off", "Use the PTT button by hand.")};
    p.cable_notes = kAcousticNotes;
    p.notes = "Check your country's rules: many licence-free services restrict data transmissions. Expect slow "
              "but reliable operation with the robust modem.";
    p.preset = "acoustic";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"acoustic","licence":"licence_free"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "generic_cb";
    p.maker = "Generic";
    p.model = "CB radio";
    p.kind = "mobile";
    p.bands = {"CB"};
    p.connector = "Microphone connector (model-specific)";
    p.settings = {S("Squelch", "Just closed", "Squelch knob."),
                  S("Roger beep / echo", "Off", "Front panel or menu.")};
    p.cable_notes = std::string(kAcousticNotes) + " A microphone-connector cable is model-specific.";
    p.notes = "Check your country's rules for data on CB. AM/FM CB channels are noisy: use the robust modem.";
    p.preset = "acoustic";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"acoustic","licence":"licence_free"})";
    r.push_back(p);
  }
  // ---------------------------------------------------------------- generic by connection method
  {
    RadioProfile p;
    p.id = "generic_vox_cable";
    p.maker = "Generic";
    p.model = "Any handheld with VOX + audio cable";
    p.kind = "handheld";
    p.connector = "Audio cable to the radio's mic/speaker jack";
    p.vox = true;
    p.settings = generic_handheld_settings(true);
    p.cable_notes = kK1Cable;
    p.notes = "The radio keys itself from the app's audio. Lead-in tone and txdelay are long so VOX has "
              "time to open.";
    p.preset = "handheld_vox";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"handheld_vox"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "generic_usb_interface";
    p.maker = "Generic";
    p.model = "Any radio + USB interface with PTT (AIOC, Digirig, ...)";
    p.kind = "handheld";
    p.connector = "USB sound card + serial RTS/DTR or CM108 GPIO PTT";
    p.settings = generic_handheld_settings(false);
    p.cable_notes = "The interface provides the sound card and keys PTT; the host keys PTT on the app's 'ptt' "
                    "events. Set its output level low and VOX off on the radio.";
    p.notes = "Fastest and most reliable cabled setup.";
    p.preset = "handheld_ptt";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"handheld_ptt"})";
    r.push_back(p);
  }
  {
    RadioProfile p;
    p.id = "generic_acoustic";
    p.maker = "Generic";
    p.model = "Any radio, no cable (acoustic coupling)";
    p.kind = "handheld";
    p.connector = "None";
    p.settings = {S("Squelch", "Low but closed", "Squelch menu."),
                  S("Volume", "Moderate", "Too loud distorts in the device microphone."),
                  S("Key and roger tones", "Off", "Tone menus.")};
    p.cable_notes = kAcousticNotes;
    p.notes = "Works with any radio, slowly. Uses the robust error-corrected modem.";
    p.preset = "acoustic";
    p.link_type = "audio";
    p.link_config_json = R"({"preset":"acoustic"})";
    r.push_back(p);
  }
  return r;
}

}  // namespace

std::vector<RadioProfile> radio_profiles(const std::string& query) {
  std::vector<RadioProfile> all = build();
  if (query.empty()) return all;
  std::string q = lower(query);
  std::vector<RadioProfile> out;
  for (const RadioProfile& p : all) {
    std::string hay = lower(p.id + " " + p.maker + " " + p.model + " " + p.kind + " " + p.connector);
    if (hay.find(q) != std::string::npos) out.push_back(p);
  }
  return out;
}

}  // namespace rf
}  // namespace ember
