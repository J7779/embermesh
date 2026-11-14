// The native host: node runner + endpoint manager + hub + host.* router.
//
// THREADING MODEL
//   * Everything (sockets, endpoints, hub, ember_tick/ember_poll) runs on ONE loop thread.
//   * Either start_thread() spawns that thread, or the embedder drives it by calling run_once()
//     repeatedly from a thread of its choice ("poll mode") — never both.
//   * call() is thread-safe when the loop thread is running: from another thread it is posted to
//     the loop and waits for the reply; from the loop thread (e.g. inside an event callback) it runs
//     inline. In poll mode call() must be made from the thread that calls run_once().
//   * Event listeners are invoked on the loop thread with the raw event JSON; they may call call().
//   * The host is the ONLY consumer of ember_poll() for its node. The embedder must not poll.
//   * Audio endpoints call ember_audio_rx/ember_audio_tx (thread-safe per ember.h) from the audio
//     device thread; the node runner ticks at least every 20 ms while an audio endpoint is open.
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ember/ember.h"
#include "host/capture.h"
#include "host/endpoint.h"
#include "host/hub.h"
#include "host/json.h"
#include "host/loop.h"
#include "host/nearby.h"

namespace ember_host {

// The subset of ember.h the host uses, as a table so tests can substitute a fake core.
struct CoreApi {
  char* (*call)(ember_node*, const char*, const char*) = nullptr;
  void (*free_)(void*) = nullptr;
  void (*tick)(ember_node*, uint64_t) = nullptr;
  uint32_t (*next_wakeup_ms)(ember_node*) = nullptr;
  void (*link_rx)(ember_node*, uint32_t, const uint8_t*, size_t) = nullptr;
  void (*audio_rx)(ember_node*, uint32_t, const int16_t*, size_t, uint32_t) = nullptr;
  size_t (*audio_tx)(ember_node*, uint32_t, int16_t*, size_t, uint32_t) = nullptr;
  int (*poll)(ember_node*, ember_out*) = nullptr;
  static CoreApi real();
};

struct HostConfig {
  std::string data_dir;                     // "" = no host.file.* support
  std::string platform = "desktop";
  std::string app_version = EMBER_HOST_VERSION;
  // Link persistence is owned by the core (RPC_API.md "Link persistence"): on start() and on every
  // node.ready the host calls links.list and reopens the endpoint of every link whose
  // endpoint.kind it can provide. `restore_links=false` skips that (links stay down).
  bool restore_links = true;
  // Links the embedder wants for this run only (host.link.create params, e.g. emberd --link):
  // created with persist:false and re-created on every node.ready if missing (after unlock / wipe).
  // A missing "host_key" is derived from the spec, so the same spec is never added twice.
  Json startup_links = Json::array();
  // hub
  bool hub_autostart = false;               // start the hub at start()
  bool hub_restore = true;                  // reopen a persisted ws_server (hub) link at start
  // Nearby devices (CONNECTIVITY §3): mDNS + TCP auto links, following connectivity.nearby.
  // Off by default for embedders that do their own discovery (phones); emberd turns it on.
  bool nearby = false;
  std::string nearby_service = "_embermesh._tcp";  // tests use a private service type
  std::string hub_bind = "0.0.0.0";
  uint16_t hub_port = 8437;
  std::string web_root;
  std::vector<std::string> download_dirs;
  std::vector<std::string> download_files;  // e.g. dist/web/embermesh.html
  std::string admin_token;
  bool local_no_token = false;
  bool captive_portal = false;
  size_t hub_max_guests = 200;
  bool hub_mqtt = false;                    // /mqtt relay broker (CONNECTIVITY §4.12), its own port
  uint16_t hub_mqtt_port = 8438;
  uint32_t max_tick_interval_ms = 1000;     // upper bound between ticks even if the core says idle

  // Reads the JSON form used by the C API (keys == field names, plus "hub":{port,bind,...}).
  static HostConfig from_json(const Json& j);
};

enum class CallOrigin { Local, Remote };

// Relay reconnect backoff (RPC_API §11): returns the next delay (base 2 s ... 5 min, +-20 % from
// `rnd`, 0..65535) and doubles *backoff_ms. Exposed for tests.
uint32_t relay_backoff_step(uint32_t* backoff_ms, uint16_t rnd);

class Host : public EndpointSink, public HubDelegate, public NearbyHostApi {
 public:
  Host(ember_node* node, HostConfig cfg, CoreApi core = CoreApi::real(),
       EndpointRegistry registry = make_default_registry());
  ~Host() override;
  Host(const Host&) = delete;
  Host& operator=(const Host&) = delete;

  // Brings up persisted links / the hub (if configured). Runs on the loop thread (posted if needed).
  void start();
  // Closes every endpoint and the hub. Idempotent; safe from any thread.
  void shutdown();

  void start_thread();
  void stop_thread();  // shutdown() + join
  bool thread_running() const { return thread_.joinable(); }
  void run_once(int64_t max_wait_ms);
  // Thread-safe: tick + drain the node outbox on the next loop iteration.
  void wakeup() {
    loop_.post([this] {
      need_drain_ = true;
      next_tick_due_ = 0;
    });
  }

  // JSON-RPC entry point for host.* and core methods. Returns the envelope.
  Json call(const std::string& method, const Json& params, CallOrigin origin = CallOrigin::Local);
  std::string call_json(const std::string& method, const std::string& params_json, CallOrigin origin = CallOrigin::Local);

  // Event listeners (raw {"event":…,"data":…} JSON), loop thread.
  int add_event_listener(std::function<void(const std::string&)> fn);
  void remove_event_listener(int id);

  Loop& loop() { return loop_; }
  Hub& hub() { return hub_; }
  const HostConfig& config() const { return cfg_; }
  const EndpointRegistry& registry() const { return registry_; }
  uint32_t hub_link_id() const { return hub_link_; }

  // EndpointSink
  void ep_rx(Endpoint& ep, const uint8_t* data, size_t len) override;
  void ep_state(Endpoint& ep, bool connected, const std::string& error) override;
  void ep_audio_rx(Endpoint& ep, const int16_t* s, size_t n, uint32_t rate) override;
  size_t ep_audio_tx(Endpoint& ep, int16_t* out, size_t max, uint32_t rate) override;
  void ep_status_changed(Endpoint& ep) override;

  // HubDelegate
  void hub_bus_rx(const uint8_t* frame, size_t len) override;
  Json hub_rpc(const std::string& method, const Json& params) override;
  void hub_clients_changed(size_t guests) override;
  Json hub_probe_info() override;

  // NearbyHostApi
  uint32_t nearby_install(std::unique_ptr<Endpoint> ep, const Json& add_params) override;
  void nearby_remove(uint32_t id) override;
  std::string nearby_network_changed() override;

 private:
  struct LinkRec {
    uint32_t id = 0;
    std::string host_key;
    std::string name;
    Json endpoint;  // {kind, config}
    Json link;      // {type, config}
    bool autostart = true;
    bool persist = true;
    std::unique_ptr<Endpoint> ep;
    uint64_t retry_timer = 0;
    uint32_t backoff_ms = 1000;
    uint64_t up_since_ms = 0;  // relay backoff: when the current connection came up
    bool connected = false;
    std::string error;
    uint64_t opened_ms = 0;
    bool auto_nearby = false;  // created by Nearby: removed when its connection ends
    bool auto_relay = false;   // an internet relay socket (RPC_API §11), keyed by its URL
    std::string auto_key;
  };

  Json call_on_loop(const std::string& method, const Json& params, CallOrigin origin);
  Json core_call(const std::string& method, const Json& params);
  Json route_host(const std::string& method, const Json& params, CallOrigin origin);
  int64_t prepare();
  void drain();
  void handle_event(const char* json, size_t len);
  void emit(const std::string& event, const Json& data);

  // host.* methods
  Json m_info(CallOrigin origin);
  Json m_scan(const Json& p);
  Json m_link_create(const Json& p);
  Json m_link_remove(const Json& p);
  Json m_link_reconnect(const Json& p);
  Json m_hub_start(const Json& p);
  Json m_hub_stop();
  Json m_hub_status();
  Json m_file_save(const Json& p);
  Json m_file_pick(const Json& p);
  Json m_notify(const Json& p);
  Json m_capture_start(const Json& p);
  Json m_capture_stop(const Json& p);
  void capture_flush();

  void open_endpoint(LinkRec& r);
  void schedule_reconnect(uint32_t id);
  void destroy_endpoint_later(std::unique_ptr<Endpoint> ep);
  void close_link_rec(uint32_t id);  // closes the endpoint, forgets the record (core untouched)
  void count_audio_endpoints();
  Json link_json(uint32_t id);  // core Link object (links.list) merged with endpoint status
  // Normative startup step, also run on every node.ready: links.list -> reopen endpoints.
  void sync_links();
  void ensure_startup_links();
  // Opens the endpoint for an existing core link (from links.list). Returns false if this host
  // cannot provide its endpoint kind.
  bool open_core_link(const Json& link);
  LinkRec& install_link(uint32_t id, const std::string& name, const Json& endpoint, const Json& lspec,
                        const std::string& host_key, bool persist, std::unique_ptr<Endpoint> ep);
  bool hub_server_start(uint16_t port, const std::string& bind, std::string* err);  // HTTP only
  bool hub_start_internal(uint16_t port, const std::string& bind, std::string* err);  // + core link
  void ensure_hub_link();
  void adopt_hub_link(uint32_t id);
  Json core_link_types();
  // Compatibility of a core link type with an endpoint kind, from the core's links.types (falls
  // back to the endpoint kind's own list for kinds the core does not know, e.g. audio_pipe).
  bool link_type_ok(const std::string& kind, const std::string& type, std::string* err);
  std::vector<std::string> compatible_link_types(const EndpointKind* k, const std::string& kind);

  ember_node* node_;
  HostConfig cfg_;
  CoreApi core_;
  EndpointRegistry registry_;
  Loop loop_;
  Hub hub_;
  std::map<uint32_t, LinkRec> links_;
  uint32_t hub_link_ = 0;
  bool hub_wanted_ = false;
  // Native Morse capture (host.capture.*, kind mic_pcm): samples from the capture thread are
  // batched (~30 ms) and fed to the core with morse.rx.feed on the loop thread.
  struct CaptureBuf {
    std::mutex mu;
    std::vector<int16_t> pcm;  // bounded (1 s)
    std::string error;
    uint64_t dropped = 0;
  };
  struct Capture {
    std::string session;
    std::string source;  // device name or file path
    uint32_t rate = 8000;
    std::shared_ptr<CaptureBuf> buf;
    std::unique_ptr<PcmSource> src;
    uint64_t timer = 0;
    uint64_t fed_samples = 0;
  };
  std::unique_ptr<Capture> capture_;
  void refresh_nearby(const Json* connectivity = nullptr);
  // RPC_API §11: one relay link per enabled relay while `internet.active` (TLS builds only).
  void refresh_relays(const Json* connectivity = nullptr);
  void relay_install(const std::string& url, bool allow_plaintext);
  void relay_remove(uint32_t id);
  bool relays_refreshing_ = false;
  std::unique_ptr<Nearby> nearby_;
  bool syncing_ = false;
  bool locked_warned_ = false;
  std::vector<std::unique_ptr<Endpoint>> dying_;
  uint64_t dying_timer_ = 0;
  std::map<int, std::function<void(const std::string&)>> listeners_;
  int next_listener_ = 1;
  uint64_t next_tick_due_ = 0;
  bool need_drain_ = false;
  bool draining_ = false;
  bool started_ = false;
  bool shut_down_ = false;
  size_t audio_endpoints_ = 0;
  Json link_types_cache_;
  std::thread thread_;
  std::mutex call_mu_;
  bool accepting_ = true;   // guarded by call_mu_
  Json probe_cache_;
  uint64_t probe_cache_ms_ = 0;
  std::shared_ptr<bool> alive_;
};

// Error envelope helpers.
Json rpc_ok(Json result);
Json rpc_error(const std::string& code, const std::string& message);

}  // namespace ember_host
