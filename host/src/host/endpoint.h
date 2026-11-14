// Endpoint contract for the host layer.
//
// An Endpoint is a byte (or PCM audio) pipe to some hardware / network resource underneath ONE core
// link. It contains no protocol logic: bytes written by the core (EMBER_OUT_LINK_TX) go out
// unchanged, bytes received go to ember_link_rx unchanged. Framing, KISS, modems, Meshtastic... are
// all core link drivers.
//
// Threading: every method is called on the loop thread. Endpoints report back through Sink, also on
// the loop thread — except the audio hooks, which are called from the audio device thread (they map
// to the thread-safe ember_audio_rx / ember_audio_tx).
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "host/json.h"
#include "host/loop.h"

namespace ember_host {

class Endpoint;

class EndpointSink {
 public:
  virtual ~EndpointSink() = default;
  // Bytes (stream chunk or exactly one datagram) from the endpoint. Loop thread.
  virtual void ep_rx(Endpoint& ep, const uint8_t* data, size_t len) = 0;
  // Endpoint opened (connected=true) or closed/failed. Loop thread.
  virtual void ep_state(Endpoint& ep, bool connected, const std::string& error) = 0;
  // Audio device thread. Mono s16 PCM.
  virtual void ep_audio_rx(Endpoint& ep, const int16_t* samples, size_t count, uint32_t rate) {}
  virtual size_t ep_audio_tx(Endpoint& ep, int16_t* out, size_t max, uint32_t rate) { return 0; }
  // Something in the endpoint's status() changed (client count, tuner info...). Loop thread.
  virtual void ep_status_changed(Endpoint& ep) {}
};

class Endpoint {
 public:
  Endpoint(Loop& loop, EndpointSink& sink, const Json& config) : loop_(loop), sink_(sink), config_(config) {}
  virtual ~Endpoint() = default;
  Endpoint(const Endpoint&) = delete;
  Endpoint& operator=(const Endpoint&) = delete;

  virtual const char* kind() const = 0;
  // Datagram endpoints deliver/accept exactly one mesh frame per call.
  virtual bool datagram() const { return false; }
  // Starts opening (may complete asynchronously). Must eventually report via set_state(). Must not
  // call the sink synchronously from inside open() (defer with loop().timer(0, ...)).
  virtual void open() = 0;
  // Closes everything. Must not call the sink.
  virtual void close() = 0;
  // Bytes from the core for this endpoint. Drop silently while closed.
  virtual void write(const uint8_t* data, size_t len) = 0;
  // Transmitter keying requested by the core (event link.ptt). Default: nothing to key.
  virtual void set_ptt(bool on) {}
  // Driver-independent status for links.list/host.link.* responses (merged into the link's endpoint).
  virtual Json status() const { return Json::object(); }
  // Should the host re-open this endpoint after it closes with an error? (TCP/WS client/serial: yes.)
  virtual bool auto_reconnect() const { return true; }
  // Relay sockets (RPC_API §11): reconnect backoff 2 s ... 5 min with +-20 % jitter, reset only
  // after 2 min connected. Others: 1 s doubling to 30 s.
  virtual bool relay_backoff() const { return false; }
  // Audio endpoints are fed from a device thread; the node runner ticks faster while one is open.
  virtual bool is_audio() const { return false; }

  uint32_t link_id = 0;
  bool connected() const { return connected_; }
  const std::string& last_error() const { return error_; }
  const Json& config() const { return config_; }
  Loop& loop() { return loop_; }

 protected:
  void set_state(bool connected, const std::string& error = std::string()) {
    connected_ = connected;
    error_ = error;
    sink_.ep_state(*this, connected, error);
  }
  void deliver(const uint8_t* data, size_t len) { sink_.ep_rx(*this, data, len); }
  EndpointSink& sink() { return sink_; }

  Loop& loop_;
  EndpointSink& sink_;
  Json config_;
  bool connected_ = false;
  std::string error_;
};

// ---- endpoint kind catalogue ---------------------------------------------------------------------

struct EndpointKind {
  std::string kind;                     // "tcp", "serial", ...
  std::string title;
  std::string description;
  bool available = true;
  std::string unavailable_reason;
  bool scannable = false;
  Json config = Json::array();          // [Field] (RPC_API.md §1)
  std::vector<std::string> link_types;  // compatible core link types (best guess; refined from links.types)
  std::string default_link_type;
  // Creates an endpoint; returns nullptr and sets *error on bad config.
  std::function<std::unique_ptr<Endpoint>(Loop&, EndpointSink&, const Json& config, std::string* error)> create;
  // Optional discovery: returns {"devices":[{id,name,detail?,config:{...}}]}. Loop thread; may block
  // briefly (bounded by timeout_ms).
  std::function<Json(const Json& params)> scan;
};

class EndpointRegistry {
 public:
  void add(EndpointKind k) { kinds_.push_back(std::move(k)); }
  const EndpointKind* find(const std::string& kind) const {
    for (const auto& k : kinds_)
      if (k.kind == kind) return &k;
    return nullptr;
  }
  const std::vector<EndpointKind>& kinds() const { return kinds_; }
  std::vector<EndpointKind>& kinds() { return kinds_; }

 private:
  std::vector<EndpointKind> kinds_;
};

// Each endpoint source file provides one registration function; registry.cpp calls them explicitly
// (static-library-safe: no static initialisers).
void register_net_endpoints(EndpointRegistry& r);     // tcp, tcp_server, udp, websocket, loopback
void register_serial_endpoints(EndpointRegistry& r);  // serial          (EMBER_HOST_HAVE_SERIAL)
void register_audio_endpoints(EndpointRegistry& r);   // audio           (EMBER_HOST_HAVE_AUDIO)
void register_audio_pipe_endpoints(EndpointRegistry& r);  // audio_pipe (EMBER_HOST_HAVE_AUDIO, POSIX)
void register_sdr_endpoints(EndpointRegistry& r);     // rtl_tcp
// ws_server is registered by the Host itself (it is backed by the hub).
EndpointRegistry make_default_registry();

// ---- Field schema builder (RPC_API.md §1) --------------------------------------------------------

struct Field {
  Json j;
  Field(const char* key, const char* label, const char* type) {
    j = Json::object();
    j.set("key", key);
    j.set("label", label);
    j.set("type", type);
  }
  Field& def(Json v) { j.set("default", std::move(v)); return *this; }
  Field& help(const char* h) { j.set("help", h); return *this; }
  Field& unit(const char* u) { j.set("unit", u); return *this; }
  Field& min(double v) { j.set("min", v); return *this; }
  Field& max(double v) { j.set("max", v); return *this; }
  Field& step(double v) { j.set("step", v); return *this; }
  Field& required(bool r = true) { j.set("required", r); return *this; }
  Field& advanced(bool a = true) { j.set("advanced", a); return *this; }
  Field& option(Json value, const char* label) {
    Json o = Json::object();
    o.set("value", std::move(value));
    o.set("label", label);
    if (!j.get("options").is_array()) j.set("options", Json::array());
    // set() returned reference is to the stored value; fetch again and push.
    Json opts = j.get("options");
    opts.push(std::move(o));
    j.set("options", std::move(opts));
    return *this;
  }
  operator Json() const { return j; }
};

// Tolerant config readers: forms may send numbers as strings and bools as "true"/"1".
std::string cfg_str(const Json& cfg, const char* key, const std::string& def = std::string());
int64_t cfg_int(const Json& cfg, const char* key, int64_t def);
double cfg_num(const Json& cfg, const char* key, double def);
bool cfg_bool(const Json& cfg, const char* key, bool def);
// Merge `cfg` over the schema defaults of `fields` (so missing keys get documented defaults).
Json cfg_with_defaults(const Json& fields, const Json& cfg);

}  // namespace ember_host
