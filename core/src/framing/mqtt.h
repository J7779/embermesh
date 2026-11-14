// EmberMesh framing — MQTT 3.1.1 (OASIS standard) packet codec, the subset the internet relay uses
// (docs/CONNECTIVITY.md §4): CONNECT/CONNACK, PUBLISH (QoS 0; QoS 1/2 are parsed so a broker's
// packets can be recognised, never sent), SUBSCRIBE/SUBACK, UNSUBSCRIBE/UNSUBACK, PINGREQ/PINGRESP,
// DISCONNECT. Used by the `relay` link driver (client) and by test / hub brokers (server side).
//
// The decoder is a byte-stream parser (an MQTT packet may span WebSocket messages and one message
// may hold several packets). Every input is hostile: the remaining length is limited to 4 bytes and
// to `max_packet`, strings must be valid UTF-8 without U+0000, reserved flag bits must be exactly as
// the standard requires, and any violation puts the decoder in a sticky error state (the
// connection must then be closed). Nothing throws; allocations are bounded by max_packet.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ember/bytes.h"

namespace ember::mqtt {

enum class Type : uint8_t {
  Connect = 1, Connack = 2, Publish = 3, Puback = 4, Pubrec = 5, Pubrel = 6, Pubcomp = 7,
  Subscribe = 8, Suback = 9, Unsubscribe = 10, Unsuback = 11, Pingreq = 12, Pingresp = 13, Disconnect = 14,
};

constexpr size_t kMaxRemainingLength = 268435455;  // 4-byte varint limit of the standard
constexpr size_t kMaxTopic = 256;                  // ours are 32 bytes; anything longer is refused
constexpr size_t kMaxFilters = 64;                 // per SUBSCRIBE / UNSUBSCRIBE packet

struct Packet {
  Type type = Type::Pingreq;
  uint8_t flags = 0;          // low nibble of the fixed header
  uint16_t packet_id = 0;     // SUBSCRIBE/SUBACK/UNSUBSCRIBE/UNSUBACK, PUBLISH with QoS > 0, PUBACK…
  std::string topic;          // PUBLISH
  Bytes payload;              // PUBLISH
  uint8_t qos = 0;            // PUBLISH
  bool retain = false, dup = false;
  // CONNECT
  std::string client_id, username, password;
  bool clean_session = true, has_username = false, has_password = false;
  uint16_t keepalive = 0;
  // CONNACK
  bool session_present = false;
  uint8_t return_code = 0;
  // SUBSCRIBE (filter, requested QoS) / UNSUBSCRIBE (filter, 0) / SUBACK return codes
  std::vector<std::pair<std::string, uint8_t>> filters;
  std::vector<uint8_t> codes;
};

// --- encoders (return an empty buffer for invalid arguments) -------------------------------------
Bytes connect(const std::string& client_id, uint16_t keepalive_s, bool clean_session = true,
              const std::string* username = nullptr, const std::string* password = nullptr);
Bytes connack(bool session_present, uint8_t return_code);
Bytes publish(const std::string& topic, ByteView payload);  // QoS 0, no retain
Bytes subscribe(uint16_t packet_id, const std::vector<std::string>& filters);  // QoS 0 each
Bytes suback(uint16_t packet_id, const std::vector<uint8_t>& codes);
Bytes unsubscribe(uint16_t packet_id, const std::vector<std::string>& filters);
Bytes unsuback(uint16_t packet_id);
Bytes pingreq();
Bytes pingresp();
Bytes disconnect();

// --- decoder -------------------------------------------------------------------------------------
class Decoder {
 public:
  explicit Decoder(size_t max_packet = 8192) : max_(max_packet) {}
  // Appends bytes. Returns false once the stream is malformed (sticky).
  bool feed(ByteView in);
  // Pops the next complete packet. False when none is complete (or after an error).
  bool next(Packet* out);
  bool failed() const { return failed_; }
  const char* error() const { return error_; }
  void reset();
  size_t buffered() const { return buf_.size() - head_; }

 private:
  bool fail(const char* why);
  Bytes buf_;
  size_t head_ = 0;
  size_t max_;
  bool failed_ = false;
  const char* error_ = "";
};

// Parses one packet body (fixed header already split off). Exposed for tests.
bool parse(uint8_t header, ByteView body, Packet* out, const char** error);

// True for topic names we publish to / filters we accept on the server side: 1..kMaxTopic bytes of
// valid UTF-8, no U+0000; `allow_wildcards` permits '+' and '#' in their legal positions.
bool valid_topic(const std::string& t, bool allow_wildcards);

}  // namespace ember::mqtt
