// Shared helpers for host tests: loop driving, temp dirs, and a fake core behind CoreApi.
#pragma once

#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "host/host.h"
#include "host/loop.h"
#include "host/util.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace test {

using ember_host::Json;

inline bool run_until(ember_host::Loop& loop, const std::function<bool()>& pred, int timeout_ms = 3000) {
  uint64_t end = ember_host::steady_ms() + uint64_t(timeout_ms);
  loop.bind_thread();
  while (!pred()) {
    if (ember_host::steady_ms() > end) return false;
    loop.run_once(10);
  }
  return true;
}

inline void run_for(ember_host::Loop& loop, int ms) {
  uint64_t end = ember_host::steady_ms() + uint64_t(ms);
  loop.bind_thread();
  while (ember_host::steady_ms() < end) loop.run_once(5);
}

inline void rmtree(const std::string& p) {
  if (ember_host::is_dir(p)) {
    for (const auto& n : ember_host::list_dir(p)) rmtree(ember_host::path_join(p, n));
#ifdef _WIN32
    _rmdir(p.c_str());
#else
    ::rmdir(p.c_str());
#endif
  } else {
    ember_host::remove_file(p);
  }
}

struct TempDir {
  std::string path;
  TempDir() {
    const char* t = std::getenv("TMPDIR");
#ifdef _WIN32
    if (!t || !*t) t = std::getenv("TEMP");
    if (!t || !*t) t = std::getenv("TMP");
#endif
    std::string base = t && *t ? t : "/tmp";
    path = ember_host::path_join(base, "ember_host_test_" + ember_host::random_hex(6));
    ember_host::make_dirs(path);
    path = ember_host::real_path(path);
  }
  ~TempDir() { rmtree(path); }
  std::string file(const std::string& rel, const std::string& content) const {
    std::string p = ember_host::path_join(path, rel);
    size_t s = p.find_last_of('/');
    ember_host::make_dirs(p.substr(0, s));
    ember_host::write_file_atomic(p, content);
    return p;
  }
};

// ---- fake core -----------------------------------------------------------------------------------
// Behaves like a tiny mesh engine: links.add/list/remove, and on link_rx it re-transmits the frame on
// every link ("flood") — or only back to the same link in echo mode.
struct FakeCore {
  struct Out {
    int kind;
    uint32_t link;
    std::string data;
  };
  struct L {
    uint32_t id;
    Json spec;
    bool connected = false;
  };
  std::deque<Out> outbox;
  Out current;
  std::map<uint32_t, L> links;
  uint32_t next_id = 1;
  enum Mode { Flood, Echo, Sink } mode = Flood;
  std::vector<std::pair<uint32_t, std::string>> rx;
  std::vector<std::string> calls;
  Json settings = Json::object();
  int ticks = 0;
  bool persist_links = true;  // links.list keeps links across "restarts" (same FakeCore object)

  static FakeCore* self(ember_node* n) { return reinterpret_cast<FakeCore*>(n); }
  ember_node* node() { return reinterpret_cast<ember_node*>(this); }

  static char* dup(const Json& j) {
    std::string s = j.dump();
    char* p = static_cast<char*>(std::malloc(s.size() + 1));
    std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
  }
  Json link_obj(const L& l) const {
    Json o = Json::object();
    o.set("id", l.id);
    o.set("type", l.spec.get("type"));
    o.set("name", l.spec.get("name"));
    o.set("state", l.connected ? "up" : "down");
    o.set("endpoint", l.spec.get("endpoint"));
    o.set("config", l.spec.get("config"));
    o.set("persist", l.spec.get("persist").as_bool(true));
    return o;
  }
  // A node restart: non-persistent links vanish, persisted ones come back down.
  void restart() {
    for (auto it = links.begin(); it != links.end();) {
      if (!it->second.spec.get("persist").as_bool(true))
        it = links.erase(it);
      else {
        it->second.connected = false;
        ++it;
      }
    }
  }
  size_t count_calls(const std::string& m) const {
    size_t n = 0;
    for (auto& c : calls) n += c == m;
    return n;
  }
  Json handle(const std::string& m, const Json& p) {
    calls.push_back(m);
    if (m == "node.info") {
      Json r = Json::object();
      r.set("node_id", "00112233aabbccdd");
      r.set("name", "fake");
      r.set("version", "test");
      return ember_host::rpc_ok(r);
    }
    if (m == "links.add") {
      L l;
      l.id = next_id++;
      l.spec = p;
      links[l.id] = l;
      Json r = Json::object();
      r.set("link", link_obj(l));
      return ember_host::rpc_ok(r);
    }
    if (m == "links.list") {
      Json arr = Json::array();
      for (auto& kv : links) arr.push(link_obj(kv.second));
      Json r = Json::object();
      r.set("links", arr);
      return ember_host::rpc_ok(r);
    }
    if (m == "links.remove") {
      uint32_t id = uint32_t(p.get("id").as_int());
      if (!links.erase(id)) return ember_host::rpc_error("not_found", "no link");
      return ember_host::rpc_ok(Json::object());
    }
    if (m == "links.endpoint_state") {
      uint32_t id = uint32_t(p.get("id").as_int());
      if (links.count(id)) links[id].connected = p.get("connected").as_bool();
      return ember_host::rpc_ok(Json::object());
    }
    if (m == "links.types") {
      return ember_host::rpc_ok(Json::parse_or_null(
          R"({"types":[{"type":"raw","endpoint_kinds":["udp","websocket","ws_server","loopback"]},{"type":"kiss","endpoint_kinds":["tcp","serial"]}]})"));
    }
    if (m == "settings.set") {
      for (auto& kv : p.get("settings").items()) settings.set(kv.first, kv.second);
      Json r = Json::object();
      r.set("settings", settings);
      return ember_host::rpc_ok(r);
    }
    if (m == "settings.get") {
      Json r = Json::object();
      r.set("settings", settings);
      return ember_host::rpc_ok(r);
    }
    if (m == "test.emit") {
      outbox.push_back({EMBER_OUT_EVENT, 0, p.get("json").as_string()});
      return ember_host::rpc_ok(Json::object());
    }
    return ember_host::rpc_error("not_implemented", m);
  }

  static ember_host::CoreApi api() {
    ember_host::CoreApi a;
    a.call = [](ember_node* n, const char* m, const char* pj) -> char* {
      return dup(self(n)->handle(m, Json::parse_or_null(pj ? pj : "{}")));
    };
    a.free_ = [](void* p) { std::free(p); };
    a.tick = [](ember_node* n, uint64_t) { self(n)->ticks++; };
    a.next_wakeup_ms = [](ember_node*) -> uint32_t { return 50; };
    a.link_rx = [](ember_node* n, uint32_t id, const uint8_t* d, size_t len) {
      FakeCore* c = self(n);
      std::string s(reinterpret_cast<const char*>(d), len);
      c->rx.emplace_back(id, s);
      if (c->mode == Echo) {
        c->outbox.push_back({EMBER_OUT_LINK_TX, id, s});
      } else if (c->mode == Flood) {
        for (auto& kv : c->links) c->outbox.push_back({EMBER_OUT_LINK_TX, kv.first, s});
      }
    };
    a.audio_rx = [](ember_node*, uint32_t, const int16_t*, size_t, uint32_t) {};
    a.audio_tx = [](ember_node*, uint32_t, int16_t*, size_t, uint32_t) -> size_t { return 0; };
    a.poll = [](ember_node* n, ember_out* out) -> int {
      FakeCore* c = self(n);
      if (c->outbox.empty()) return 0;
      c->current = c->outbox.front();
      c->outbox.pop_front();
      out->kind = c->current.kind;
      out->link_id = c->current.link;
      out->data = reinterpret_cast<const uint8_t*>(c->current.data.data());
      out->len = c->current.data.size();
      return 1;
    };
    return a;
  }
};

}  // namespace test
