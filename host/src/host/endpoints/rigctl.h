// Hamlib rigctld (NET rigctl, default TCP 4532) client helper: set frequency / mode / PTT on
// CAT-capable radios. Text protocol, one command per line; set commands answer "RPRT <n>"
// (0 = OK, negative = Hamlib error), get commands answer the value line(s).
#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "host/loop.h"
#include "host/stream.h"

namespace ember_host {

// ---- pure encoders / parsers (unit tested) ------------------------------------------------------
std::string rigctl_cmd_set_freq(uint64_t hz);                            // "F 145500000\n"
std::string rigctl_cmd_set_mode(const std::string& mode, int passband);  // "M FM 0\n" (0 = rig default)
std::string rigctl_cmd_set_ptt(bool on);                                 // "T 1\n" / "T 0\n"
std::string rigctl_cmd_get_freq();                                       // "f\n"
// Parses "RPRT <n>" (surrounding whitespace / CR tolerated). Returns false if not an RPRT line.
bool rigctl_parse_rprt(std::string_view line, int* code);

// Result codes passed to RigctlClient callbacks besides Hamlib's own RPRT codes.
enum : int {
  kRigctlTimeout = -1000,     // no answer within 3 s
  kRigctlConnectFailed = -1001,
  kRigctlDisconnected = -1002,
};

// Non-blocking client with a small FIFO command queue. Connects on demand, reconnects on the next
// command after a failure. Loop thread only.
class RigctlClient {
 public:
  // rprt: Hamlib RPRT code (0 = OK), or kRigctl* for transport failures. reply: value line for get
  // commands ("145500000"), "" for set commands.
  using Done = std::function<void(int rprt, const std::string& reply)>;

  RigctlClient(Loop& loop, std::string host, uint16_t port);
  ~RigctlClient();
  RigctlClient(const RigctlClient&) = delete;
  RigctlClient& operator=(const RigctlClient&) = delete;

  // `cmd` is one encoded line (from the encoders above). Queue is capped at 32; overflow drops the
  // oldest queued (not in-flight) command with kRigctlDisconnected.
  void send(std::string cmd, Done done = {});
  void set_freq(uint64_t hz, Done d = {}) { send(rigctl_cmd_set_freq(hz), std::move(d)); }
  void set_mode(const std::string& mode, int passband = 0, Done d = {}) { send(rigctl_cmd_set_mode(mode, passband), std::move(d)); }
  void set_ptt(bool on, Done d = {}) { send(rigctl_cmd_set_ptt(on), std::move(d)); }

  bool connected() const { return stream_ && stream_->connected(); }
  const std::string& host() const { return host_; }
  uint16_t port() const { return port_; }
  int last_rprt() const { return last_rprt_; }
  size_t queued() const { return queue_.size(); }

 private:
  struct Cmd {
    std::string line;
    Done done;
  };
  void pump();
  void on_line(const std::string& line);
  void complete(int code, const std::string& reply);
  void fail_all(int code);

  Loop& loop_;
  std::string host_;
  uint16_t port_;
  std::unique_ptr<TcpStream> stream_;
  bool connecting_ = false;
  bool in_flight_ = false;
  uint64_t timer_ = 0;
  std::deque<Cmd> queue_;
  std::string rx_;
  int last_rprt_ = 0;
};

}  // namespace ember_host
