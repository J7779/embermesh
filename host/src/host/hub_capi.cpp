// ember_hub.h implementation: the Hub on its own loop thread, without a node.
#include "ember_hub.h"

#include <cstdlib>
#include <cstring>
#include <future>
#include <mutex>
#include <new>
#include <thread>

#include "host/hub.h"
#include "host/json.h"
#include "host/loop.h"
#include "host/util.h"

using namespace ember_host;

struct ember_hub : public HubDelegate {
  ember_hub_frame_fn on_frame = nullptr;
  ember_hub_clients_fn on_clients = nullptr;
  void* user = nullptr;
  Loop loop;
  Hub hub;
  std::thread th;
  std::mutex probe_mu;
  Json probe = Json::object();

  ember_hub() : hub(loop, *this) {}

  // Runs fn on the loop thread and waits (the loop thread is always running while the object lives).
  template <class F>
  auto sync(F fn) -> decltype(fn()) {
    using R = decltype(fn());
    std::promise<R> p;
    auto f = p.get_future();
    loop.post([&] { p.set_value(fn()); });
    return f.get();
  }

  // HubDelegate (loop thread)
  void hub_bus_rx(const uint8_t* frame, size_t len) override {
    if (on_frame) on_frame(user, frame, len);
  }
  Json hub_rpc(const std::string&, const Json&) override {
    Json e = Json::object();
    e.set("ok", false);
    Json err = Json::object();
    err.set("code", "unsupported");
    err.set("message", "remote control is not available on this hub");
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
};

static char* dup_cstr(const std::string& s) {
  char* p = static_cast<char*>(std::malloc(s.size() + 1));
  if (p) std::memcpy(p, s.c_str(), s.size() + 1);
  return p;
}

static void put_err(char* err, size_t err_len, const std::string& msg) {
  if (!err || err_len == 0) return;
  size_t n = msg.size() < err_len - 1 ? msg.size() : err_len - 1;
  std::memcpy(err, msg.data(), n);
  err[n] = 0;
}

extern "C" {

ember_hub* ember_hub_create(ember_hub_frame_fn on_frame, ember_hub_clients_fn on_clients, void* user) {
  ember_hub* h = new (std::nothrow) ember_hub();
  if (!h) return nullptr;
  h->on_frame = on_frame;
  h->on_clients = on_clients;
  h->user = user;
  h->loop.unbind_thread();  // every call from now on is posted to the loop thread
  h->th = std::thread([h] { h->loop.run(); });
  return h;
}

void ember_hub_destroy(ember_hub* h) {
  if (!h) return;
  h->sync([h] {
    h->hub.stop();
    return 0;
  });
  h->loop.stop();
  h->th.join();
  h->loop.bind_thread();
  delete h;
}

int ember_hub_start(ember_hub* h, const char* options_json, char* err, size_t err_len) {
  if (!h) {
    put_err(err, err_len, "null hub");
    return -1;
  }
  Json o = Json::object();
  if (options_json && *options_json) {
    std::string perr;
    if (!Json::parse(options_json, &o, &perr) || !o.is_object()) {
      put_err(err, err_len, "options are not a JSON object: " + perr);
      return -1;
    }
  }
  HubOptions opt;
  int64_t port = o.get("port").as_int(8437);
  if (port < 0 || port > 65535) {
    put_err(err, err_len, "port must be 0..65535");
    return -1;
  }
  opt.port = uint16_t(port);
  opt.bind = o.get("bind").as_string("0.0.0.0");
  opt.web_root = o.get("web_root").as_string();
  for (const auto& d : o.get("download_dirs").elements())
    if (d.is_string()) opt.download_dirs.push_back(d.as_string());
  for (const auto& d : o.get("download_files").elements())
    if (d.is_string()) opt.download_files.push_back(d.as_string());
  int64_t mg = o.get("max_guests").as_int(int64_t(opt.max_guests));
  if (mg > 0) opt.max_guests = size_t(mg);
  opt.captive_portal = o.get("captive_portal").as_bool(false);
  opt.version = o.get("version").as_string(EMBER_HOST_VERSION);
  {
    std::lock_guard<std::mutex> lk(h->probe_mu);
    if (o.get("name").is_string()) h->probe.set("name", o.get("name"));
    if (o.get("node_id").is_string()) h->probe.set("node_id", o.get("node_id"));
  }
  std::string e = h->sync([h, &opt] {
    std::string why;
    h->hub.stop();
    if (!h->hub.start(opt, &why) && why.empty()) why = "hub failed to start";
    return why;
  });
  if (!e.empty()) {
    put_err(err, err_len, e);
    return -1;
  }
  return 0;
}

void ember_hub_stop(ember_hub* h) {
  if (!h) return;
  h->sync([h] {
    h->hub.stop();
    return 0;
  });
}

void ember_hub_broadcast(ember_hub* h, const uint8_t* data, size_t len) {
  if (!h || !data || len == 0) return;
  std::string f(reinterpret_cast<const char*>(data), len);
  h->loop.post([h, f] { h->hub.broadcast_frame(reinterpret_cast<const uint8_t*>(f.data()), f.size()); });
}

void ember_hub_set_probe(ember_hub* h, const char* probe_json) {
  if (!h || !probe_json) return;
  Json j = Json::parse_or_null(probe_json);
  if (!j.is_object()) return;
  std::lock_guard<std::mutex> lk(h->probe_mu);
  for (const auto& kv : j.items()) h->probe.set(kv.first, kv.second);
}

char* ember_hub_status(ember_hub* h) {
  if (!h) return dup_cstr("{\"running\":false}");
  std::string s = h->sync([h] {
    Json j = Json::object();
    j.set("running", h->hub.running());
    j.set("port", h->hub.port());
    Json urls = Json::array();
    for (auto& u : h->hub.urls()) urls.push(u);
    j.set("urls", urls);
    j.set("clients", double(h->hub.guest_count()));
    j.set("stats", h->hub.stats());
    return j.dump();
  });
  return dup_cstr(s);
}

void ember_hub_free(void* p) { std::free(p); }

}  // extern "C"
