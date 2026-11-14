// RFC 6455 WebSocket client (ws:// only) on the Loop. Used for hub-to-hub bridging (`websocket`
// endpoint), the terminal client's --attach mode, and the integration tests.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "host/loop.h"
#include "host/stream.h"
#include "host/ws.h"

namespace ember_host {

class WsClient {
 public:
  struct Options {
    size_t max_message = 1 << 20;
    uint64_t ping_interval_ms = 30000;
    uint64_t timeout_ms = 90000;  // no input for this long -> drop
    uint32_t connect_timeout_ms = 10000;
    std::string origin;
  };
  explicit WsClient(Loop& loop) : WsClient(loop, Options()) {}
  WsClient(Loop& loop, Options opt);
  ~WsClient();
  WsClient(const WsClient&) = delete;
  WsClient& operator=(const WsClient&) = delete;

  // Starts connecting. Exactly one of on_open / on_close(1006, error) follows.
  bool connect(const std::string& url, std::string* err = nullptr);
  bool send_text(std::string_view t);
  bool send_binary(const void* data, size_t len);
  void close(uint16_t code = kWsCloseNormal, std::string_view reason = {});
  void abort();  // drop the TCP connection now, no callbacks
  bool is_open() const { return state_ == Open; }
  size_t pending() const { return stream_.pending(); }

  std::function<void()> on_open;
  std::function<void(uint8_t opcode, std::string& data)> on_message;
  // Fires once per connect() attempt that does not end in abort(): code 1006 if no Close was received.
  std::function<void(uint16_t code, const std::string& reason)> on_close;

 private:
  enum State { Idle, Connecting, Handshake, Open, Closing };
  void on_connected(const std::string& err);
  void on_bytes(const uint8_t* d, size_t n);
  void finish(uint16_t code, const std::string& reason);
  void schedule_tick();
  void tick();
  bool send_frame(uint8_t op, const void* data, size_t len);

  Loop& loop_;
  Options opt_;
  TcpStream stream_;
  State state_ = Idle;
  std::string key_, host_header_, path_;
  std::string head_buf_;
  WsDecoder dec_;
  uint64_t last_input_ = 0, last_ping_ = 0, close_deadline_ = 0;
  uint64_t tick_timer_ = 0;
  uint16_t peer_close_code_ = kWsCloseAbnormal;
  std::string peer_close_reason_;
  bool close_sent_ = false;
  std::shared_ptr<bool> alive_;
};

}  // namespace ember_host
