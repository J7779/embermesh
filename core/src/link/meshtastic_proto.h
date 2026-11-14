// EmberMesh core / link — minimal hand-written protobuf codec for the Meshtastic client API
// (no protobuf library). Field numbers follow github.com/meshtastic/protobufs (meshtastic/mesh.proto,
// config.proto, portnums.proto), checked in Nov 2026:
//
//   ToRadio:    packet=1 (MeshPacket), want_config_id=3 (uint32), disconnect=4, heartbeat=7 (Heartbeat{nonce=1})
//   FromRadio:  id=1, packet=2, my_info=3, node_info=4, config=5, log_record=6, config_complete_id=7,
//               rebooted=8, moduleConfig=9, channel=10, queueStatus=11, metadata=13
//   MeshPacket: from=1 fixed32, to=2 fixed32, channel=3, decoded=4 (Data), encrypted=5 bytes, id=6 fixed32,
//               rx_time=7 fixed32, rx_snr=8 float, hop_limit=9, want_ack=10, priority=11, rx_rssi=12 int32,
//               via_mqtt=14, hop_start=15
//   Data:       portnum=1, payload=2, want_response=3, dest=4, source=5, request_id=6, reply_id=7, emoji=8
//   MyNodeInfo: my_node_num=1        NodeInfo: num=1, user=2, snr=4 float, last_heard=5 fixed32
//   User:       id=1, long_name=2, short_name=3, hw_model=5, is_licensed=6
//   Config:     lora=6 (LoRaConfig: use_preset=1, modem_preset=2, bandwidth=3, spread_factor=4,
//               coding_rate=5, region=7, hop_limit=8, tx_enabled=9, tx_power=10 int32)
//   QueueStatus: res=1 int32, free=2, maxlen=3, mesh_packet_id=4     DeviceMetadata: firmware_version=1
//   PortNum: TEXT_MESSAGE_APP=1, PRIVATE_APP=256.  Constants.DATA_PAYLOAD_LEN = 233.
//
// Decoders are strict about wire types of known fields, skip unknown fields, reject groups,
// truncation and overlong varints, and cap every string they keep.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "ember/bytes.h"

namespace ember {
namespace link {
namespace mt {

constexpr uint32_t kPortText = 1;
constexpr uint32_t kPortPrivate = 256;
constexpr size_t kDataPayloadLen = 233;
constexpr size_t kMaxToFromRadio = 512;
constexpr uint32_t kBroadcast = 0xFFFFFFFFu;
constexpr uint8_t kStart1 = 0x94;
constexpr uint8_t kStart2 = 0xC3;

struct Data {
  uint32_t portnum = 0;
  Bytes payload;
  bool want_response = false;
};

struct MeshPacket {
  uint32_t from = 0, to = 0, channel = 0, id = 0, hop_limit = 0, hop_start = 0, priority = 0;
  bool has_decoded = false;
  Data decoded;
  bool encrypted = false;  // payload was still encrypted (not for us / unknown channel)
  bool has_snr = false;
  float rx_snr = 0;
  int32_t rx_rssi = 0;
  bool want_ack = false;
  bool via_mqtt = false;
};

struct LoraConfig {
  bool use_preset = false;
  uint32_t modem_preset = 0, bandwidth = 0, spread_factor = 0, coding_rate = 0, region = 0, hop_limit = 0;
  bool tx_enabled = false;
  int32_t tx_power = 0;
};

struct NodeInfo {
  uint32_t num = 0;
  std::string user_id, long_name, short_name;
  uint32_t hw_model = 0;
  bool is_licensed = false;
  bool has_snr = false;
  float snr = 0;
};

struct QueueStatus {
  int32_t res = 0;
  uint32_t free = 0, maxlen = 0, mesh_packet_id = 0;
};

enum class FromKind : uint8_t {
  None, Packet, MyInfo, NodeInfo, Config, LogRecord, ConfigComplete, Rebooted, ModuleConfig, Channel,
  QueueStatus, Metadata, Other
};

struct FromRadio {
  uint32_t id = 0;
  FromKind kind = FromKind::None;
  MeshPacket packet;
  uint32_t my_node_num = 0;
  NodeInfo node;
  bool has_lora = false;
  LoraConfig lora;
  uint32_t config_complete_id = 0;
  QueueStatus queue;
  std::string firmware_version;
};

enum class ToKind : uint8_t { None, Packet, WantConfig, Disconnect, Heartbeat, Other };

struct ToRadio {
  ToKind kind = ToKind::None;
  MeshPacket packet;
  uint32_t want_config_id = 0;
};

// --- low level -----------------------------------------------------------------------------------

class PbReader {
 public:
  explicit PbReader(ByteView v) : v_(v) {}
  bool ok() const { return ok_; }
  bool at_end() const { return pos_ >= v_.size(); }
  // Next tag; false at end or on error (check ok()).
  bool next(uint32_t* field, uint8_t* wire);
  uint64_t varint();
  uint32_t fixed32();
  uint64_t fixed64();
  ByteView bytes();
  bool skip(uint8_t wire);
  void fail() { ok_ = false; }

 private:
  ByteView v_;
  size_t pos_ = 0;
  bool ok_ = true;
};

void pb_varint(Bytes& out, uint64_t v);
void pb_tag(Bytes& out, uint32_t field, uint8_t wire);
void pb_uint(Bytes& out, uint32_t field, uint64_t v);   // omitted when 0 (proto3 default)
void pb_int32(Bytes& out, uint32_t field, int32_t v);   // negative -> 10-byte varint
void pb_bool(Bytes& out, uint32_t field, bool v);
void pb_fixed32(Bytes& out, uint32_t field, uint32_t v);  // omitted when 0
void pb_float(Bytes& out, uint32_t field, float v);
void pb_bytes(Bytes& out, uint32_t field, ByteView v);   // always written (also when empty)

// --- messages ------------------------------------------------------------------------------------

Bytes encode_mesh_packet(const MeshPacket& p);
bool decode_mesh_packet(ByteView in, MeshPacket* out);

Bytes encode_to_radio(const ToRadio& t);
bool decode_to_radio(ByteView in, ToRadio* out);

bool decode_from_radio(ByteView in, FromRadio* out);
// Encoder for the variants the driver understands (used by tests / emulators).
Bytes encode_from_radio(const FromRadio& f);

// Serial/TCP framing: 0x94 0xC3 len_hi len_lo payload. Returns empty if payload > 512.
Bytes frame(ByteView payload);

// Streaming deframer: resynchronises on garbage / debug-log text, rejects length > 512.
class Deframer {
 public:
  // Calls on_packet(ByteView) for every complete frame; on_text(char) for bytes outside frames.
  template <class P, class T>
  void feed(ByteView in, P&& on_packet, T&& on_text) {
    for (uint8_t b : in) {
      switch (st_) {
        case 0:
          if (b == kStart1) st_ = 1;
          else on_text(static_cast<char>(b));
          break;
        case 1:
          if (b == kStart2) st_ = 2;
          else if (b == kStart1) st_ = 1;
          else { st_ = 0; ++bad_; on_text(static_cast<char>(b)); }
          break;
        case 2: len_ = static_cast<size_t>(b) << 8; st_ = 3; break;
        case 3:
          len_ |= b;
          if (len_ > kMaxToFromRadio) { ++bad_; st_ = 0; break; }
          buf_.clear();
          if (len_ == 0) { st_ = 0; on_packet(ByteView(buf_)); break; }
          st_ = 4;
          break;
        default:
          buf_.push_back(b);
          if (buf_.size() == len_) { st_ = 0; on_packet(ByteView(buf_)); }
          break;
      }
    }
  }
  void reset() { st_ = 0; buf_.clear(); }
  uint64_t bad() const { return bad_; }

 private:
  int st_ = 0;
  size_t len_ = 0;
  Bytes buf_;
  uint64_t bad_ = 0;
};

}  // namespace mt
}  // namespace link
}  // namespace ember
