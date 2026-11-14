// See ember_ios_hub.h. Embeds ember_host::Hub (host/ library) on a private loop thread.
#include "ember_ios_hub.h"

#include <cstdlib>
#include <cstring>
#include <future>
#include <mutex>
#include <string>
#include <thread>

#include "host/hub.h"
#include "host/json.h"
#include "host/loop.h"

using ember_host::Hub;
using ember_host::HubDelegate;
using ember_host::HubOptions;
using ember_host::Json;
using ember_host::Loop;

struct ember_ios_hub : public HubDelegate {
  ember_ios_hub_frame_fn on_frame = nullptr;
  ember_ios_hub_clients_fn on_clients = nullptr;
  void* user = nullptr;
  Loop loop;
  Hub hub{loop, *this};
  std::thread thread;
  std::mutex probe_mu;
  Json probe = Json::object();

  void hub_bus_rx(const uint8_t* frame, size_t len) override {
    if (on_frame && len) on_frame(user, frame, len);
  }
  Json hub_rpc(const std::string& method, const Json& params) override {
    // Remote control of the phone's node from a browser is not offered (no admin token is ever set,
    // so the hub refuses /rpc before reaching this); answer defensively anyway.
    Json e = Json::object();
    e.set("ok", false);
    Json err = Json::object();
    err.set("code", "denied");
    err.set("message", "remote control is not available on the iOS hub");
    e.set("error", err);
    return e;
  }
  void hub_clients_changed(size_t guests) override {
    if (on_clients) on_clients(user, guests);
  }
  Json hub_probe_info() override {
    std::lock_guard<std::mutex> lk(probe_mu);
    return probe;
  }

  bool running_thread() const { return thread.joinable(); }

  // Runs fn on the loop thread and waits for it.
  template <class F>
  auto on_loop(F fn) -> decltype(fn()) {
    using R = decltype(fn());
    std::promise<R> p;
    auto f = p.get_future();
    loop.post([&] { p.set_value(fn()); });
    return f.get();
  }

  void ensure_thread() {
    if (thread.joinable()) return;
    loop.reset_stop();
    thread = std::thread([this] { loop.run(); });
  }

  void stop_thread() {
    if (!thread.joinable()) return;
    on_loop([this] {
      hub.stop();
      return 0;
    });
    loop.stop();
    thread.join();
    thread = std::thread();
  }
};

static char* dup_string(const std::string& s) {
  char* p = static_cast<char*>(std::malloc(s.size() + 1));
  if (!p) return nullptr;
  std::memcpy(p, s.data(), s.size());
  p[s.size()] = 0;
  return p;
}

extern "C" {

ember_ios_hub* ember_ios_hub_create(ember_ios_hub_frame_fn on_frame, ember_ios_hub_clients_fn on_clients, void* user) {
  auto* h = new ember_ios_hub();
  h->on_frame = on_frame;
  h->on_clients = on_clients;
  h->user = user;
  return h;
}

void ember_ios_hub_destroy(ember_ios_hub* h) {
  if (!h) return;
  h->stop_thread();
  delete h;
}

int ember_ios_hub_start(ember_ios_hub* h, const char* options_json, char* err, size_t err_len) {
  if (!h) return -1;
  Json j = Json::parse_or_null(options_json ? options_json : "{}");
  HubOptions o;
  o.port = uint16_t(j.get("port").as_int(8437));
  o.bind = j.get("bind").as_string(o.bind);
  o.web_root = j.get("web_root").as_string("");
  for (const auto& d : j.get("download_dirs").elements())
    if (d.is_string()) o.download_dirs.push_back(d.as_string());
  for (const auto& d : j.get("download_files").elements())
    if (d.is_string()) o.download_files.push_back(d.as_string());
  o.max_guests = size_t(j.get("max_guests").as_int(200));
  o.version = j.get("version").as_string("");
  {
    std::lock_guard<std::mutex> lk(h->probe_mu);
    Json p = Json::object();
    if (j.get("name").is_string()) p.set("name", j.get("name"));
    if (j.get("node_id").is_string()) p.set("node_id", j.get("node_id"));
    p.set("platform", "ios");
    h->probe = p;
  }
  h->ensure_thread();
  std::string e;
  bool ok = h->on_loop([&] {
    if (h->hub.running()) h->hub.stop();
    return h->hub.start(o, &e);
  });
  if (!ok) {
    if (err && err_len) {
      std::snprintf(err, err_len, "%s", e.empty() ? "hub failed to start" : e.c_str());
    }
    h->stop_thread();
    return -1;
  }
  return 0;
}

void ember_ios_hub_stop(ember_ios_hub* h) {
  if (h) h->stop_thread();
}

void ember_ios_hub_broadcast(ember_ios_hub* h, const uint8_t* data, size_t len) {
  if (!h || !data || !len) return;
  std::string frame(reinterpret_cast<const char*>(data), len);
  h->loop.post([h, frame] {
    if (h->hub.running()) h->hub.broadcast_frame(reinterpret_cast<const uint8_t*>(frame.data()), frame.size());
  });
}

void ember_ios_hub_set_probe(ember_ios_hub* h, const char* probe_json) {
  if (!h) return;
  Json p = Json::parse_or_null(probe_json ? probe_json : "{}");
  if (!p.is_object()) return;
  p.set("platform", "ios");
  std::lock_guard<std::mutex> lk(h->probe_mu);
  h->probe = p;
}

char* ember_ios_hub_status(ember_ios_hub* h) {
  Json s = Json::object();
  if (h && h->running_thread()) {
    s = h->on_loop([h] {
      Json r = Json::object();
      r.set("running", h->hub.running());
      r.set("port", int(h->hub.port()));
      Json urls = Json::array();
      for (const auto& u : h->hub.urls()) urls.push(u);
      r.set("urls", urls);
      r.set("clients", double(h->hub.guest_count()));
      r.set("stats", h->hub.stats());
      return r;
    });
  } else {
    s.set("running", false);
    s.set("port", 0);
    s.set("urls", Json::array());
    s.set("clients", 0);
  }
  return dup_string(s.dump());
}

void ember_ios_hub_free(void* p) { std::free(p); }

}  // extern "C"
