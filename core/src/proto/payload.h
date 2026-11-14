// EmberMesh core — packet body (payload) encodings for each Kind. See docs/PROTOCOL.md §5.
// Every decode_* is strict: bounded lengths, valid UTF-8 for text fields, no trailing bytes.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "crypto/identity.h"
#include "ember/bytes.h"
#include "proto/packet.h"

namespace ember::proto {

constexpr size_t kMaxText = 2000;        // bytes of UTF-8 in a text message
constexpr size_t kMaxName = 32;          // announce name
constexpr size_t kMaxStatus = 64;
constexpr size_t kMaxShortField = 64;    // labels, titles, file names, ...
constexpr size_t kMaxNote = 500;
constexpr size_t kMaxBulletinText = 1500;
constexpr size_t kMaxNetPlanEntries = 16;
constexpr size_t kMaxAcksPerPacket = 16;

struct Position {
  int32_t lat_e7 = 0;  // degrees * 1e7
  int32_t lon_e7 = 0;
  bool has_alt = false;
  int16_t alt_m = 0;
  bool has_accuracy = false;
  uint16_t accuracy_m = 0;

  double lat() const { return lat_e7 / 1e7; }
  double lon() const { return lon_e7 / 1e7; }
  static Position from_degrees(double lat, double lon);
  bool valid() const { return lat_e7 >= -900000000 && lat_e7 <= 900000000 && lon_e7 >= -1800000000 && lon_e7 <= 1800000000; }
  bool operator==(const Position& o) const {
    return lat_e7 == o.lat_e7 && lon_e7 == o.lon_e7 && has_alt == o.has_alt && alt_m == o.alt_m &&
           has_accuracy == o.has_accuracy && accuracy_m == o.accuracy_m;
  }
};
void write_position(ByteWriter& w, const Position& p);
bool read_position(ByteReader& r, Position& p);

struct TextBody {
  std::string text;
  std::optional<MsgId> reply_to;
};
struct AckEntry {
  uint32_t pkt_id = 0;
  std::array<uint8_t, 8> secret{};
};
struct AckBody {  // destination = origin of the acknowledged packets
  std::vector<AckEntry> acks;
};
struct ReadBody {
  std::vector<uint32_t> pkt_ids;
};
struct AnnounceBody {
  PublicKeys keys;
  bool relay = true;
  std::string name;
  std::string callsign;
  std::string status;
  uint8_t caps = 0;  // feature bits, reserved
  uint8_t time_quality = 15;  // sender's clock quality when it signed this (0 GPS .. 8 system .. 15 none)
};
struct PositionBody {
  Position pos;
};
struct SosBody {
  bool active = true;  // false = the sender cancelled the alert
  uint16_t seq = 0;
  std::optional<Position> pos;
  std::string text;
};
struct SosAckBody {
  uint32_t sos_pkt_id = 0;  // the SOS packet being answered (0 = any)
  std::string text;
};
enum class CheckinStatus : uint8_t { Ok = 0, NeedHelp = 1, Injured = 2, Moving = 3, Sheltering = 4 };
const char* checkin_status_name(CheckinStatus s);
bool parse_checkin_status(std::string_view s, CheckinStatus& out);
struct CheckinBody {
  CheckinStatus status = CheckinStatus::Ok;
  std::optional<Position> pos;
  std::string note;
};
enum class BulletinCategory : uint8_t {
  Water = 0, Food = 1, Medical = 2, Shelter = 3, Hazard = 4, Trade = 5, Missing = 6, Transport = 7, Info = 8
};
const char* bulletin_category_name(BulletinCategory c);
bool parse_bulletin_category(std::string_view s, BulletinCategory& out);
struct BulletinBody {
  BulletinCategory category = BulletinCategory::Info;
  bool is_delete = false;
  uint32_t delete_pkt_id = 0;  // with is_delete: the sender's bulletin to withdraw
  std::optional<Position> pos;
  std::string title;
  std::string text;
};
struct FileBody {
  uint32_t file_id = 0;
  uint16_t index = 0;
  uint16_t count = 1;
  // index 0 only:
  uint32_t total_size = 0;
  std::string name;
  std::string mime;
  std::array<uint8_t, 16> hash{};  // BLAKE2b-128 of the whole file
  Bytes data;
};
constexpr size_t kMaxFileChunks = 1024;
struct NetPlanEntry {
  std::string label;
  uint64_t freq_hz = 0;
  std::string mode;
  std::string tone;
  std::string schedule;
  std::string notes;
};
struct NetPlanBody {
  std::vector<NetPlanEntry> entries;
};
struct PingBody {
  uint32_t nonce = 0;
};
struct PongBody {
  uint32_t nonce = 0;
  uint8_t ping_hops = 0;
};
struct WhoisBody {
  NodeId target{};
};
struct AppBody {
  uint16_t app_type = 0;
  Bytes data;
};

Bytes encode(const TextBody& b);
Bytes encode(const AckBody& b);
Bytes encode(const ReadBody& b);
Bytes encode(const AnnounceBody& b);
Bytes encode(const PositionBody& b);
Bytes encode(const SosBody& b);
Bytes encode(const SosAckBody& b);
Bytes encode(const CheckinBody& b);
Bytes encode(const BulletinBody& b);
Bytes encode(const FileBody& b);
Bytes encode(const NetPlanBody& b);
Bytes encode(const PingBody& b);
Bytes encode(const PongBody& b);
Bytes encode(const WhoisBody& b);
Bytes encode(const AppBody& b);

bool decode(ByteView in, TextBody& b);
bool decode(ByteView in, AckBody& b);
bool decode(ByteView in, ReadBody& b);
bool decode(ByteView in, AnnounceBody& b);
bool decode(ByteView in, PositionBody& b);
bool decode(ByteView in, SosBody& b);
bool decode(ByteView in, SosAckBody& b);
bool decode(ByteView in, CheckinBody& b);
bool decode(ByteView in, BulletinBody& b);
bool decode(ByteView in, FileBody& b);
bool decode(ByteView in, NetPlanBody& b);
bool decode(ByteView in, PingBody& b);
bool decode(ByteView in, PongBody& b);
bool decode(ByteView in, WhoisBody& b);
bool decode(ByteView in, AppBody& b);

}  // namespace ember::proto
