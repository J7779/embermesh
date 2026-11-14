// RFC 6455 WebSocket: frame encoder, incremental decoder (fragmentation, control frames, masking
// rules, size limits, UTF-8 and close-code validation) and opening-handshake helpers.
// No extensions (permessage-deflate is never negotiated), no subprotocols required.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "host/http.h"

namespace ember_host {

enum WsOpcode : uint8_t { kWsCont = 0x0, kWsText = 0x1, kWsBinary = 0x2, kWsClose = 0x8, kWsPing = 0x9, kWsPong = 0xA };

enum WsCloseCode : uint16_t {
  kWsCloseNormal = 1000,
  kWsCloseGoingAway = 1001,
  kWsCloseProtocol = 1002,
  kWsCloseUnsupported = 1003,
  kWsCloseNoStatus = 1005,  // never sent on the wire
  kWsCloseAbnormal = 1006,  // never sent on the wire
  kWsCloseBadData = 1007,
  kWsClosePolicy = 1008,
  kWsCloseTooBig = 1009,
  kWsCloseInternal = 1011,
};

// Builds one frame. `mask` (4 bytes) must be given for client->server frames, null for server->client.
std::string ws_encode_frame(uint8_t opcode, const void* data, size_t len, bool fin = true, const uint8_t* mask = nullptr);
std::string ws_close_payload(uint16_t code, std::string_view reason);
// Codes an endpoint may legitimately send/receive in a Close frame.
bool ws_close_code_valid(uint16_t code);

struct WsEvent {
  enum Kind { None, Message, Ping, Pong, Close, Error } kind = None;
  uint8_t opcode = 0;        // Message: kWsText or kWsBinary
  std::string payload;       // Message/Ping/Pong payload; Close reason
  uint16_t close_code = 0;   // Close: received code (kWsCloseNoStatus if none). Error: code to send.
  std::string error;         // Error: description
};

class WsDecoder {
 public:
  struct Options {
    bool expect_masked = true;   // server side: client frames MUST be masked
    size_t max_message = 1 << 20;
  };
  WsDecoder() = default;
  explicit WsDecoder(Options o) : opt_(o) {}

  void feed(const uint8_t* data, size_t len);
  // Pops the next complete event; WsEvent::None when more bytes are needed. After an Error event the
  // decoder is dead (always returns Error again); the caller sends Close(error code) and drops the
  // connection.
  WsEvent next();
  size_t buffered() const { return buf_.size() - off_; }

 private:
  WsEvent fail(uint16_t code, const char* msg);
  Options opt_;
  std::string buf_;
  size_t off_ = 0;
  bool in_message_ = false;
  uint8_t msg_opcode_ = 0;
  std::string msg_;
  bool dead_ = false;
  WsEvent dead_event_;
};

// ---- handshake -----------------------------------------------------------------------------------

extern const char* const kWsGuid;
std::string ws_accept_key(std::string_view client_key);
// Validates a server-side upgrade request. Returns 0 if OK, else the HTTP status to reply with
// (400, or 426 for an unsupported version; also *why).
int ws_validate_upgrade(const HttpRequest& req, std::string* why);
bool ws_is_upgrade_request(const HttpRequest& req);
std::string ws_server_handshake_response(const HttpRequest& req, const std::string& subprotocol = std::string());
// Client side.
std::string ws_make_client_key();  // base64 of 16 random bytes
std::string ws_client_handshake_request(const std::string& host_header, const std::string& path_and_query,
                                        const std::string& key, const std::string& origin = std::string(),
                                        const std::vector<std::string>& protocols = {}, bool user_agent = true);
// Parses the server's response head (bytes up to and including CRLFCRLF). Returns true if it is a
// valid 101 with matching Sec-WebSocket-Accept and no unrequested extensions.
bool ws_check_server_response(std::string_view head, const std::string& key, std::string* why,
                              const std::vector<std::string>& offered = {}, std::string* chosen = nullptr);

// Parses "ws://host[:port]/path?query" (and "wss://…" when this build has TLS, EMBER_HOST_TLS).
bool ws_parse_url(const std::string& url, std::string* host, uint16_t* port, std::string* path, std::string* err);
// Same, reporting whether the URL is wss:// (default port 443) regardless of TLS support.
bool ws_parse_url_tls(const std::string& url, std::string* host, uint16_t* port, std::string* path, bool* tls,
                      std::string* err);
bool ws_tls_available();

}  // namespace ember_host
