// EmberMesh core — radio profile database for the "Connect a radio" wizard (RPC `rf.radios`,
// docs/RPC_API.md §6). Static, offline, deliberately small.
//
// Accuracy rule: a menu number or menu name appears in `settings[].how` only for entries whose
// `verified` flag is true, i.e. the settings were checked against the maker's manual (the source is
// named in `notes`). Unverified entries give generic wording ("the VOX menu") without numbers.
// No pinouts or frequencies are given beyond the widely documented connector type.
#pragma once

#include <string>
#include <vector>

namespace ember {
namespace rf {

struct RadioSetting {
  std::string label;  // what to set ("VOX")
  std::string value;  // to what ("On, level 1-3")
  std::string how;    // menu path / number (verified entries) or generic instructions
};

struct RadioProfile {
  std::string id;      // stable, e.g. "baofeng_uv5r"
  std::string maker;
  std::string model;   // may list family members ("UV-5R / UV-5RA / UV-5RE")
  std::string kind;    // "handheld" | "mobile" | "base"
  std::vector<std::string> bands;  // "2m", "70cm", "HF", "6m", "PMR446", "FRS/GMRS", "CB", ...
  std::string connector;
  bool vox = false;          // has a usable VOX for data keying
  bool data_port = false;    // dedicated data / accessory audio port
  bool builtin_tnc = false;  // KISS TNC inside the radio
  bool bluetooth = false;
  std::vector<double> power_w;  // selectable power levels (nominal, from the maker); empty = varies
  bool verified = false;
  std::vector<RadioSetting> settings;
  std::string cable_notes;
  std::string notes;
  // recommended: {preset, link:{type, config}} — config as a JSON object text.
  std::string preset;     // audio-link preset id, or "" when the link is not "audio"
  std::string link_type;  // "audio" | "kiss" | ...
  std::string link_config_json;
};

// All profiles; `query` (case-insensitive substring of id, maker, model, kind or connector; "" =
// all) filters them.
std::vector<RadioProfile> radio_profiles(const std::string& query = "");

}  // namespace rf
}  // namespace ember
