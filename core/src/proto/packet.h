// EmberMesh core — wire format: frames, packets, fragments and control messages.
// Normative description: docs/PROTOCOL.md. Every parser here is bounds-checked and rejects
// non-canonical input (reserved bits, bad lengths, trailing garbage where the format is closed).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "crypto/identity.h"
#include "ember/bytes.h"

namespace ember::proto {

constexpr uint8_t kVersion = 2;  // frame b0 bits 7-6; v2: hop limit + AEAD salt in the packet header
// Mesh timestamps on the wire are seconds since 2020-01-01T00:00:00Z.
constexpr uint64_t kEpochUnixS = 1577836800ull;
constexpr size_t kMaxPacket = 4096;     // whole packet, bytes
constexpr size_t kMaxUnit = kMaxPacket + 1;  // fragmentable unit = inner type byte + body
constexpr size_t kMaxFragments = 128;
constexpr uint8_t kMaxTtl = 15;
constexpr size_t kMaxCallsign = 16;
constexpr size_t kMinMtu = 32;  // links with a smaller MTU cannot carry the protocol

inline uint32_t unix_ms_to_wire(uint64_t unix_ms) {
  uint64_t s = unix_ms / 1000;
  if (s <= kEpochUnixS) return 0;
  s -= kEpochUnixS;
  return s > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(s);
}
inline uint64_t wire_to_unix_ms(uint32_t w) { return (kEpochUnixS + w) * 1000ull; }

// --- frames ---------------------------------------------------------------------------------------

enum class FrameType : uint8_t { Packet = 0, Fragment = 1, Control = 2 };

struct FrameHeader {
  FrameType type = FrameType::Packet;
  std::string station;  // operator callsign ("" = absent)
  uint32_t hop_src = 0;  // first 4 bytes of the transmitting node's id (big-endian)
};

inline uint32_t short_id(const NodeId& id) {
  return uint32_t(id[0]) << 24 | uint32_t(id[1]) << 16 | uint32_t(id[2]) << 8 | id[3];
}

size_t frame_overhead(size_t station_len);
void write_frame_header(ByteWriter& w, const FrameHeader& h);
// Parses the header; `body` receives the rest. Rejects bad version / reserved bits / callsigns.
bool parse_frame(ByteView frame, FrameHeader& h, ByteView& body);
bool valid_callsign(std::string_view cs);

// Fragment body: u16 frag_id | u8 index | u8 count | [index 0: u16 total_len] | data
struct Fragment {
  uint16_t frag_id = 0;
  uint8_t index = 0;
  uint8_t count = 0;
  uint16_t total_len = 0;  // only meaningful for index 0
  ByteView data;
};
constexpr size_t kFragHeader0 = 6;  // header bytes of fragment 0
constexpr size_t kFragHeaderN = 4;  // header bytes of the others
bool parse_fragment(ByteView body, Fragment& out);
void write_fragment(ByteWriter& w, uint16_t frag_id, uint8_t index, uint8_t count, uint16_t total_len,
                    ByteView data);

// Splits `unit` (inner type byte + body) into fragment frame bodies whose frames fit `mtu`.
// Returns an empty vector if the unit cannot be fragmented (too big / MTU too small).
std::vector<Bytes> fragment_unit(ByteView unit, size_t mtu, const FrameHeader& h, uint16_t frag_id);

// --- packets --------------------------------------------------------------------------------------

enum class Sec : uint8_t { None = 0, Signed = 1, PairMac = 2, PairAead = 3, ChanAead = 4 };
enum class DestType : uint8_t { Broadcast = 0, Node = 1, Channel = 2 };
enum class Prio : uint8_t { Low = 0, Normal = 1, High = 2, Emergency = 3 };

enum class Kind : uint8_t {
  Text = 0x01,
  Ack = 0x02,
  Read = 0x03,
  Announce = 0x04,
  Position = 0x05,
  Sos = 0x06,
  SosAck = 0x07,
  Checkin = 0x08,
  Bulletin = 0x09,
  File = 0x0A,
  NetPlan = 0x0B,
  Ping = 0x0C,
  Pong = 0x0D,
  Whois = 0x0E,
  App = 0x0F,
};
bool known_kind(uint8_t k);
const char* kind_name(Kind k);
size_t tag_size(Sec s);
// Security modes that may be transmitted on links with encryption_allowed == false.
inline bool legal_cleartext(Sec s) { return s == Sec::None || s == Sec::Signed || s == Sec::PairMac; }

using AckCommit = std::array<uint8_t, 8>;

struct Header {
  Sec sec = Sec::Signed;
  DestType dst_type = DestType::Broadcast;
  Prio prio = Prio::Normal;
  bool want_ack = false;  // only with dst_type == Node; adds ack_commit
  Kind kind = Kind::Text;
  uint8_t ttl = 7;   // remaining hops after this transmission may still be relayed (0..15)
  uint8_t hops = 0;  // transmissions so far (saturates at 15)
  // Originator's ttl, authenticated (v2). Receivers require ttl <= hop_limit and never raise a
  // copy's reach beyond ttl + hops == hop_limit. write_header() writes max(hop_limit, ttl).
  uint8_t hop_limit = 7;
  uint32_t pkt_id = 0;
  NodeId origin{};
  NodeId dst_node{};
  uint32_t channel = 0;
  uint32_t created = 0;        // wire seconds (mesh time)
  uint16_t lifetime_min = 60;  // >= 1
  AckCommit ack_commit{};
  // AEAD modes only (v2): 8-byte nonce salt, authenticated; see secure.h.
  std::array<uint8_t, 8> salt{};

  bool has_ack_commit() const { return want_ack && dst_type == DestType::Node; }
  bool has_salt() const { return sec == Sec::PairAead || sec == Sec::ChanAead; }
  uint64_t expires_wire() const { return uint64_t(created) + uint64_t(lifetime_min) * 60; }
};
constexpr size_t kHopByteOffset = 2;
constexpr size_t kLimitByteOffset = 3;

size_t header_size(const Header& h);
void write_header(ByteWriter& w, const Header& h);

// A parsed packet. Views point into `raw`, which must outlive it.
struct PacketView {
  Header h;
  ByteView raw;
  ByteView auth_header;  // raw[0 .. header_len) — note: the hop byte must be zeroed for auth
  ByteView body;         // cleartext body, or ciphertext for *Aead modes
  ByteView tag;          // signature / mac (empty for Sec::None)
};
bool parse_packet(ByteView raw, PacketView& out);

// Copy of the header bytes with the mutable hop byte zeroed: the associated data for every
// authentication mode.
Bytes auth_header_bytes(const PacketView& p);
// 64-bit wire identity: BLAKE2b-64 of the packet with the hop byte zeroed. Used for duplicate
// suppression and store reconciliation (a modified copy is a different packet).
uint64_t wire_id(ByteView raw);
inline uint32_t sync_id(uint64_t wire) { return static_cast<uint32_t>(wire >> 32); }
void set_hop_byte(Bytes& raw, uint8_t ttl, uint8_t hops);

// End-to-end message identity: origin + pkt_id (24 hex chars).
struct MsgId {
  NodeId origin{};
  uint32_t pkt_id = 0;
  std::string to_hex() const;
  static bool parse(std::string_view hex, MsgId& out);
  bool operator==(const MsgId& o) const { return origin == o.origin && pkt_id == o.pkt_id; }
  bool operator!=(const MsgId& o) const { return !(*this == o); }
  bool operator<(const MsgId& o) const { return origin != o.origin ? origin < o.origin : pkt_id < o.pkt_id; }
};
struct MsgIdHash {
  size_t operator()(const MsgId& m) const {
    return NodeIdHash()(m.origin) ^ (static_cast<size_t>(m.pkt_id) * 0x9E3779B1u);
  }
};

// --- control messages (hop-local, never relayed) -----------------------------------------------

enum class Ctrl : uint8_t { Beacon = 1, Sync = 2, FragNack = 3 };

struct Beacon {
  NodeId node{};
  bool relay = true;
  bool has_time = false;
  uint64_t time_ms = 0;      // sender's mesh time (Unix ms)
  uint8_t time_quality = 15;  // 0 best (GPS/manual) .. 15 unknown
  uint32_t store_count = 0;  // packets offered for reconciliation on this link
  uint32_t store_sum = 0;    // sum of their sync ids (mod 2^32)
};
Bytes encode_beacon(const Beacon& b);  // control body including the Ctrl byte
bool decode_beacon(ByteView body_after_type, Beacon& out);

struct SyncDigest {  // per 4-bit child of a prefix
  uint16_t count = 0;
  uint32_t sum = 0;
  bool operator==(const SyncDigest& o) const { return count == o.count && sum == o.sum; }
  bool operator!=(const SyncDigest& o) const { return !(*this == o); }
};
struct SyncItem {
  enum Type : uint8_t { Dig = 1, Ids = 2, Want = 3 } type = Dig;
  uint8_t bits = 0;     // prefix length in bits (multiple of 4, 0..28)
  uint32_t prefix = 0;  // prefix value, left-aligned (low bits zero)
  std::array<SyncDigest, 16> children{};  // Dig
  std::vector<uint32_t> ids;               // Ids / Want (<= 255)
};
struct SyncMsg {
  uint32_t target = 0;  // hop short id of the neighbour this is addressed to
  std::vector<SyncItem> items;
};
constexpr size_t kMaxSyncItems = 64;
Bytes encode_sync(const SyncMsg& m);
bool decode_sync(ByteView body_after_type, SyncMsg& out);
size_t sync_item_size(const SyncItem& it);

struct FragNack {
  uint32_t target = 0;  // hop short id of the fragment sender
  uint16_t frag_id = 0;
  uint8_t count = 0;
  std::vector<uint8_t> missing;  // indexes
};
Bytes encode_frag_nack(const FragNack& n);
bool decode_frag_nack(ByteView body_after_type, FragNack& out);

}  // namespace ember::proto
