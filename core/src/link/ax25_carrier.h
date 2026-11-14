// EmberMesh core / link — AX.25 carriage of mesh frames + APRS interop, shared by the kiss, audio
// (AFSK/HDLC modems) and rtl_tcp drivers.
//
// Mesh frames travel as AX.25 UI frames:
//   dst = EMBER (SSID 0)  src = operator callsign-SSID  path = optional digipeaters
//   control 0x03 (UI)  PID 0xF0  info = the mesh frame, unchanged
// Any other UI frame is offered to the APRS parser: messages (with ack handling), bulletins,
// positions and status reports become InteropText (network "aprs").
#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "base/json.h"
#include "ember/bytes.h"
#include "ember/link.h"
#include "framing/ax25.h"

namespace ember {
namespace link {

constexpr const char* kMeshDestCall = "EMBER";
constexpr const char* kAprsToCall = "APZEMB";  // APRS "experimental" tocall block

struct Ax25Config {
  ax25::Address mycall;              // validated operator callsign-SSID
  bool amateur = true;               // licence-free + N0CALL -> the carrier's pseudo station id
  std::vector<ax25::Address> path;   // digipeaters for mesh frames
  std::vector<ax25::Address> aprs_path;
  bool aprs = true;                  // decode APRS / allow send_text
  bool auto_ack = true;              // ack APRS messages addressed to mycall
  size_t max_info = 256;             // largest info field accepted on receive
  uint32_t retry_ms = 30000;         // first APRS message retry (doubles)
  uint8_t retries = 3;
};

// Parses the shared config keys (callsign, path, aprs_path) with useful error messages.
bool parse_callsign(const std::string& text, ax25::Address* out, std::string* error);
bool parse_path(const std::string& text, std::vector<ax25::Address>* out, std::string* error);

class Ax25Carrier {
 public:
  struct Output {
    std::vector<Bytes> frames;  // AX.25 frames (no FCS) to transmit, in order
    std::vector<std::pair<std::string, Json>> events;
  };
  enum class Rx : uint8_t { Mesh, Interop, Handled, Ignored, Bad };

  // Licence-free pseudo station id: when not amateur and mycall is the placeholder N0CALL, the
  // carrier sends as "QX" + 4 letters + "-SSID" instead, drawn once from `seed` (host entropy) by
  // the driver at creation, so two default-configured stations never share an address. It has no
  // digit, so it can never be a real ITU callsign; the Q prefix block is not issued for callsigns.
  void set_pseudo_seed(uint32_t seed);
  void configure(const Ax25Config& c);
  const Ax25Config& config() const { return cfg_; }

  // Wraps a mesh frame. Returns false if it cannot be encoded (invalid callsign).
  bool wrap(ByteView mesh_frame, Bytes* out);
  // Overhead added by wrap() (addresses + control + PID).
  size_t overhead() const { return 16 + 7 * cfg_.path.size(); }

  // Classifies a received AX.25 frame (without FCS). Mesh: *mesh holds the frame. Interop: *text.
  // May append frames (acks) and events to *out.
  Rx on_frame(ByteView frame, uint64_t now, Bytes* mesh, InteropText* text, Output* out);

  // Queues an APRS message (split into <= 67-char parts). Returns false if invalid/unsupported.
  bool send_text(const InteropText& msg, uint64_t now, Output* out);
  // Retries unacked messages; emits "aprs_ack" {to,id,acked:false} when giving up.
  void tick(uint64_t now, Output* out);

  size_t outstanding() const { return pending_.size(); }
  Json status() const;

 private:
  struct Pending {
    std::string to, id;
    Bytes frame;
    uint64_t next_ms;
    uint32_t interval_ms;
    uint8_t tries_left;
  };
  bool seen_recent(uint32_t h, uint64_t now);
  bool make_aprs(const std::string& info, Bytes* out);
  // Own-echo suppression keys: a received frame is our echo only if its source is mycall AND its
  // (dst, src, info) matches something we sent recently, so a second station using the same
  // callsign (or the same pseudo id) is still heard.
  static uint32_t frame_key(const ax25::Address& dst, const ax25::Address& src, ByteView info);
  void note_sent(uint32_t key);
  bool sent_recently(uint32_t key) const;
  void handle_ack(const std::string& from, const std::string& id, bool acked, Output* out);

  Ax25Config cfg_;
  std::deque<Pending> pending_;
  std::deque<std::pair<uint32_t, uint64_t>> recent_;  // dedup hashes of heard APRS packets
  std::deque<uint32_t> sent_;                          // keys of our recent transmissions
  ax25::Address pseudo_;
  uint32_t next_id_ = 0;
  uint64_t aprs_heard_ = 0, aprs_ignored_ = 0, aprs_acks_sent_ = 0, aprs_acked_ = 0, aprs_failed_ = 0;
  uint64_t own_echoes_ = 0, other_frames_ = 0;
};

}  // namespace link
}  // namespace ember
