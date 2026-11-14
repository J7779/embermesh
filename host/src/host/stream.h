// Buffered non-blocking TCP stream on the Loop (client connect or adopted accepted socket).
//
// Safety: callbacks may destroy the TcpStream; all internal code re-checks a liveness token after
// invoking a callback. on_close fires at most once and never as a result of the owner calling close().
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "host/loop.h"
#include "host/net.h"

namespace ember_host {

class TcpStream {
 public:
  explicit TcpStream(Loop& loop);
  ~TcpStream();
  TcpStream(const TcpStream&) = delete;
  TcpStream& operator=(const TcpStream&) = delete;

  // Takes ownership of a connected, non-blocking socket.
  void adopt(sock_t s, const SockAddr& peer);
  // Asynchronous connect. Hostnames are resolved on a worker thread (DNS may hang with no network).
  // on_connect("") on success, on_connect(error) on failure (the stream is then closed).
  void connect(const std::string& host, uint16_t port, uint32_t timeout_ms = 10000);

  void write(const void* data, size_t len);
  void write(const std::string& s) { write(s.data(), s.size()); }
  void close();                // immediate; no callbacks
  void close_after_flush();    // stop reading, flush output, then close (fires on_close("closed"))
  void set_read_paused(bool paused);
  bool read_paused() const { return paused_; }

  bool is_open() const { return sock_ != kInvalidSock; }
  bool connected() const { return connected_; }
  size_t pending() const { return out_.size() - out_off_; }
  const SockAddr& peer() const { return peer_; }
  uint64_t last_activity_ms() const { return last_activity_; }
  uint64_t bytes_in() const { return bytes_in_; }
  uint64_t bytes_out() const { return bytes_out_; }

  std::function<void(const std::string& err)> on_connect;
  std::function<void(const uint8_t* data, size_t len)> on_data;
  std::function<void()> on_drain;                       // output buffer fully flushed
  std::function<void(const std::string& reason)> on_close;  // peer closed / error

 private:
  struct DnsReq;
  void start_connect(const SockAddr& a);
  void finish_connect(const std::string& err);
  void on_io(short re);
  void update_events();
  void flush();
  void fail(const std::string& reason);

  Loop& loop_;
  sock_t sock_ = kInvalidSock;
  uint64_t watch_ = 0;
  uint64_t connect_timer_ = 0;
  bool connecting_ = false;
  bool connected_ = false;
  bool paused_ = false;
  bool closing_ = false;
  std::string out_;
  size_t out_off_ = 0;
  SockAddr peer_;
  uint64_t last_activity_ = 0;
  uint64_t bytes_in_ = 0, bytes_out_ = 0;
  uint32_t timeout_ms_ = 10000;
  std::shared_ptr<bool> alive_;
  std::shared_ptr<DnsReq> dns_;
};

}  // namespace ember_host
