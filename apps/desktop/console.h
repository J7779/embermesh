// Line-based terminal chat client for headless boxes. Works purely through RPC calls (RPC_API.md),
// so it runs unchanged against the in-process node (emberd --console) or a running daemon over the
// /rpc WebSocket (emberd --attach).
#pragma once

#include <atomic>
#include <functional>
#include <iosfwd>
#include <mutex>
#include <string>
#include <vector>

#include "host/json.h"

namespace emberd {

class Console {
 public:
  using CallFn = std::function<ember_host::Json(const std::string& method, const ember_host::Json& params)>;

  Console(CallFn call, std::ostream& out);

  // Thread-safe: prints an incoming event ({"event":…,"data":…}) if it is interesting.
  void on_event(const std::string& event_json);
  // Executes one command line. Returns false when the user asked to quit.
  bool execute(const std::string& line);
  // Interactive loop until EOF / quit / *stop becomes true. Returns 0.
  int run(std::istream& in, const std::atomic<bool>* stop_flag = nullptr);

  void set_show_all_events(bool on) { all_events_ = on; }

 private:
  ember_host::Json call(const std::string& method, const ember_host::Json& params);
  bool ok(const ember_host::Json& env);  // prints the error if not ok
  void print(const std::string& s);
  std::string resolve_conv(const std::string& arg);
  std::string public_channel_conv();
  std::string name_of(const std::string& node_id);
  void cmd_help();
  void cmd_info();
  void cmd_peers();
  void cmd_contacts();
  void cmd_channels();
  void cmd_convs();
  void cmd_read(const std::string& conv, int n);
  void cmd_send(const std::string& conv, const std::string& text);
  void cmd_sos(const std::string& args);
  void cmd_checkin(const std::string& args);
  void cmd_links();
  void cmd_link(const std::string& args);
  void cmd_hub(const std::string& args);
  void cmd_call(const std::string& args);
  void cmd_morse(const std::string& args);

  CallFn call_;
  std::ostream& out_;
  std::mutex out_mu_;
  std::vector<std::string> last_convs_;
  bool all_events_ = false;
  std::mutex morse_mu_;
  std::string morse_session_;  // native capture session started by `morse listen`
  std::string morse_line_;     // decoded text not printed yet (printed per word / line)
  int morse_wpm_ = 0;
  void morse_flush_locked(bool force);
};

std::string format_time(double ts_ms);

}  // namespace emberd
