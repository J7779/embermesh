// ember_host.h implementation.
#include "ember_host.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <new>

#include "host/host.h"
#include "host/util.h"

using namespace ember_host;

struct ember_host_s {
  std::unique_ptr<Host> host;
  std::mutex cb_mu;
  ember_host_event_fn cb = nullptr;
  void* cb_user = nullptr;
  bool poll_started = false;
};

static char* dup_str(const std::string& s) {
  char* p = static_cast<char*>(std::malloc(s.size() + 1));
  if (!p) return nullptr;
  std::memcpy(p, s.c_str(), s.size() + 1);
  return p;
}

extern "C" {

const char* ember_host_version(void) { return EMBER_HOST_VERSION; }

ember_host_t* ember_host_create(ember_node* node, const char* config_json) {
  if (!node) return nullptr;
  Json cfg = Json::object();
  if (config_json && *config_json) {
    if (!Json::parse(config_json, &cfg) || !cfg.is_object()) return nullptr;
  }
  ember_host_t* h = new (std::nothrow) ember_host_s();
  if (!h) return nullptr;
  h->host = std::make_unique<Host>(node, HostConfig::from_json(cfg));
  h->host->add_event_listener([h](const std::string& ev) {
    ember_host_event_fn fn;
    void* u;
    {
      std::lock_guard<std::mutex> lk(h->cb_mu);
      fn = h->cb;
      u = h->cb_user;
    }
    if (fn) fn(u, ev.c_str(), ev.size());
  });
  return h;
}

void ember_host_destroy(ember_host_t* h) {
  if (!h) return;
  h->host.reset();
  delete h;
}

void ember_host_set_event_callback(ember_host_t* h, ember_host_event_fn fn, void* user) {
  if (!h) return;
  std::lock_guard<std::mutex> lk(h->cb_mu);
  h->cb = fn;
  h->cb_user = user;
}

void ember_host_set_log_callback(ember_host_log_fn fn, void* user) {
  if (!fn) {
    set_log_sink(nullptr);
    return;
  }
  set_log_sink([fn, user](LogLevel l, const char* line) { fn(user, int(l), line); });
}

void ember_host_set_log_level(int level) {
  if (level < 0) level = 0;
  if (level > 4) level = 4;
  set_log_level(LogLevel(level));
}

int ember_host_start(ember_host_t* h) {
  if (!h) return -1;
  h->host->start_thread();
  h->host->start();
  return 0;
}

void ember_host_stop(ember_host_t* h) {
  if (!h) return;
  h->host->stop_thread();
}

void ember_host_run(ember_host_t* h, int max_wait_ms) {
  if (!h) return;
  if (!h->poll_started) {
    h->poll_started = true;
    h->host->loop().bind_thread();
    h->host->start();
  }
  h->host->run_once(max_wait_ms);
}

void ember_host_wakeup(ember_host_t* h) {
  if (!h) return;
  h->host->wakeup();
}

char* ember_host_call(ember_host_t* h, const char* method, const char* params_json) {
  if (!h || !method) return dup_str(R"({"ok":false,"error":{"code":"bad_params","message":"null host or method"}})");
  return dup_str(h->host->call_json(method, params_json ? params_json : "", CallOrigin::Local));
}

void ember_host_free(void* p) { std::free(p); }

int ember_host_hub_port(ember_host_t* h) {
  if (!h) return 0;
  Json r = h->host->call("host.hub.status", Json::object());
  return int(r.get("result").get("port").as_int(0));
}

}  // extern "C"
