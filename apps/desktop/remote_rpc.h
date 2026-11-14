// Blocking JSON-RPC client for a running emberd's /rpc WebSocket (used by `emberd --attach`).
// Runs its own Loop on a background thread; call() is thread-safe.
#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "host/json.h"
#include "host/loop.h"
#include "host/ws_client.h"

namespace emberd {

class RemoteRpc {
 public:
  RemoteRpc();
  ~RemoteRpc();
  // Connects and waits up to timeout_ms for the WebSocket to open.
  bool connect(const std::string& url, std::string* err, int timeout_ms = 5000);
  ember_host::Json call(const std::string& method, const ember_host::Json& params, int timeout_ms = 15000);
  bool connected() const { return open_.load(); }
  std::function<void(const std::string& event_json)> on_event;  // background thread
  std::function<void(const std::string& reason)> on_disconnect;  // background thread

 private:
  struct Pending {
    bool done = false;
    ember_host::Json reply;
  };
  ember_host::Loop loop_;
  std::unique_ptr<ember_host::WsClient> ws_;
  std::thread thread_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::map<uint64_t, std::shared_ptr<Pending>> pending_;
  uint64_t next_id_ = 1;
  std::atomic<bool> open_{false};
  bool connect_done_ = false;
  std::string connect_err_;
};

}  // namespace emberd
