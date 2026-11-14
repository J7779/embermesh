#include "remote_rpc.h"

#include <chrono>

#include "host/host.h"

using ember_host::Json;

namespace emberd {

RemoteRpc::RemoteRpc() {
  ws_ = std::make_unique<ember_host::WsClient>(loop_);
  ws_->on_open = [this] {
    std::lock_guard<std::mutex> lk(mu_);
    open_ = true;
    connect_done_ = true;
    cv_.notify_all();
  };
  ws_->on_message = [this](uint8_t op, std::string& data) {
    if (op != ember_host::kWsText) return;
    Json m = Json::parse_or_null(data);
    if (m.has("event")) {
      if (on_event) on_event(data);
      return;
    }
    uint64_t id = uint64_t(m.get("id").as_int(0));
    std::lock_guard<std::mutex> lk(mu_);
    auto it = pending_.find(id);
    if (it == pending_.end()) return;
    Json env = Json::object();
    env.set("ok", m.get("ok").as_bool(false));
    if (m.get("ok").as_bool(false))
      env.set("result", m.get("result"));
    else
      env.set("error", m.get("error"));
    it->second->reply = env;
    it->second->done = true;
    cv_.notify_all();
  };
  ws_->on_close = [this](uint16_t code, const std::string& reason) {
    bool was_open = open_.exchange(false);
    {
      std::lock_guard<std::mutex> lk(mu_);
      if (!connect_done_) {
        connect_done_ = true;
        connect_err_ = reason.empty() ? "connection failed" : reason;
      }
      for (auto& kv : pending_) {
        kv.second->reply = ember_host::rpc_error("io_error", "disconnected: " + reason);
        kv.second->done = true;
      }
      cv_.notify_all();
    }
    if (was_open && on_disconnect) on_disconnect(reason);
  };
  thread_ = std::thread([this] { loop_.run(); });
}

RemoteRpc::~RemoteRpc() {
  loop_.post([this] { ws_->abort(); });
  loop_.stop();
  if (thread_.joinable()) thread_.join();
  loop_.bind_thread();
  ws_.reset();
}

bool RemoteRpc::connect(const std::string& url, std::string* err, int timeout_ms) {
  {
    std::lock_guard<std::mutex> lk(mu_);
    connect_done_ = false;
    connect_err_.clear();
  }
  std::string perr;
  bool started = false;
  {
    std::mutex m;
    std::condition_variable c;
    bool done = false;
    loop_.post([&] {
      started = ws_->connect(url, &perr);
      std::lock_guard<std::mutex> lk(m);
      done = true;
      c.notify_all();
    });
    std::unique_lock<std::mutex> lk(m);
    c.wait(lk, [&] { return done; });
  }
  if (!started) {
    if (err) *err = perr;
    return false;
  }
  std::unique_lock<std::mutex> lk(mu_);
  if (!cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms), [this] { return connect_done_; })) {
    if (err) *err = "timeout connecting to " + url;
    return false;
  }
  if (!open_) {
    if (err) *err = connect_err_;
    return false;
  }
  return true;
}

Json RemoteRpc::call(const std::string& method, const Json& params, int timeout_ms) {
  if (!open_) return ember_host::rpc_error("io_error", "not connected");
  auto p = std::make_shared<Pending>();
  uint64_t id;
  {
    std::lock_guard<std::mutex> lk(mu_);
    id = next_id_++;
    pending_[id] = p;
  }
  Json req = Json::object();
  req.set("id", double(id));
  req.set("method", method);
  req.set("params", params.is_object() ? params : Json::object());
  std::string text = req.dump();
  loop_.post([this, text] { ws_->send_text(text); });
  std::unique_lock<std::mutex> lk(mu_);
  bool ok = cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms), [&] { return p->done; });
  pending_.erase(id);
  if (!ok) return ember_host::rpc_error("busy", "timeout waiting for " + method);
  return p->reply;
}

}  // namespace emberd
