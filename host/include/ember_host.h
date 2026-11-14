/* EmberMesh native host layer — C API.
 *
 * Embeds the hub (HTTP + /ws mesh bus + /rpc remote control on port 8437) and the socket / serial /
 * audio / SDR endpoints around an existing core node (ember.h). Used by emberd and intended for the
 * Android (JNI) and iOS shells, which build it with EMBER_HOST_AUDIO=OFF / EMBER_HOST_SERIAL=OFF.
 *
 * THREADING MODEL
 *   - All host work (sockets, endpoints, hub, ember_tick + ember_poll for the node) happens on ONE
 *     loop thread. Pick one of:
 *       (a) thread mode: ember_host_start() spawns the loop thread; ember_host_stop() joins it.
 *       (b) poll mode:   call ember_host_run(h, max_wait_ms) repeatedly from a thread you own
 *                        (it blocks up to max_wait_ms waiting for I/O; -1 = until something happens).
 *     Never mix the two for one host.
 *   - The host is the ONLY consumer of ember_poll() for its node, and exactly ONE thread polls: the
 *     loop thread (thread mode), or the thread calling ember_host_run() (poll mode); after
 *     ember_host_stop() / during ember_host_destroy() the final drain runs on the calling thread
 *     once the loop thread has been joined. ember_poll() data is only valid until the next poll on
 *     that node, so never call ember_poll() yourself on a node owned by a host (you would race the
 *     loop thread and corrupt each other's buffers); receive events through the event callback. Prefer ember_host_call() over ember_call()
 *     so the host notices outbox work immediately (if you do call ember_call() directly, follow it
 *     with ember_host_wakeup()).
 *   - ember_host_call() is thread-safe in thread mode (posted to the loop thread; blocks until the
 *     reply). In poll mode call it from the thread that runs ember_host_run(). It may be called from
 *     inside the event callback.
 *   - The event callback runs on the loop thread with {"event":…,"data":…} JSON (core events plus
 *     host.endpoint.state / host.hub.clients). The string is valid only during the callback. Keep
 *     the callback short; never block it on another thread that is waiting in ember_host_call().
 *   - The node must outlive the host: destroy the host first, then the node.
 *
 * Apps that drive the node themselves (they poll it) cannot use this API; they embed only the hub
 * with <ember_hub.h> (same HTTP server, /ws bus, fairness and limits; no node access).
 */
#ifndef EMBER_HOST_H
#define EMBER_HOST_H

#include <stddef.h>
#include <stdint.h>

#include "ember/ember.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ember_host_s ember_host_t;

typedef void (*ember_host_event_fn)(void* user, const char* event_json, size_t len);
/* level: 0 debug, 1 info, 2 warn, 3 error. Called from any thread. */
typedef void (*ember_host_log_fn)(void* user, int level, const char* line);

EMBER_API const char* ember_host_version(void);

/* Creates a host around `node` (not owned).
 *   config_json (all optional):
 *     {"data_dir":"/path",            host.file.* dirs ("" = none)
 *      "platform":"desktop|android|ios", "app_version":"…",
 *      "web_root":"/path/to/ui",       files served at /  (built-in fallback page if empty)
 *      "download_dirs":["/path",…],    files served at /download/
      "download_files":["/p/embermesh.html"],  single files also offered there
 *      "admin_token":"…",              required by /rpc ("" = /rpc refused unless local_no_token)
 *      "local_no_token":false,         loopback clients may use /rpc without the token
 *      "captive_portal":false,
 *      "restore_links":true,           on start and on every node.ready: links.list, then reopen
 *                                       the endpoint of every link whose endpoint.kind this host
 *                                       provides (the core persists links; the host keeps no list)
 *      "links":[{host.link.create params},…]   links for this run only (persist:false), re-created
 *                                       after unlock/wipe if missing (additive)
 *      "hub":{"autostart":false,"port":8437,"bind":"0.0.0.0","max_guests":200,
 *             "restore":true,        restore: reopen a persisted ws_server (hub) link at start
 *             "mqtt":false,          mqtt: also run the /mqtt relay broker (CONNECTIVITY §4.12)
 *             "mqtt_port":8438},     on its own listener (never the hub port)
 *      "nearby":false,                 mDNS Nearby devices (CONNECTIVITY §3), follows connectivity.nearby
 *      "nearby_service":"_embermesh._tcp"}  (additive)
 *    "data_dir" is only used for host.file.* now (saved/, inbox/); host_links.json is no longer used.
 * Returns NULL on bad JSON or NULL node. */
EMBER_API ember_host_t* ember_host_create(ember_node* node, const char* config_json);
EMBER_API void ember_host_destroy(ember_host_t* h); /* stops the thread, closes everything */

EMBER_API void ember_host_set_event_callback(ember_host_t* h, ember_host_event_fn fn, void* user);
EMBER_API void ember_host_set_log_callback(ember_host_log_fn fn, void* user); /* process-wide */
EMBER_API void ember_host_set_log_level(int level);

/* Thread mode. start() also restores saved links and autostarts the hub. Returns 0 on success. */
EMBER_API int ember_host_start(ember_host_t* h);
EMBER_API void ember_host_stop(ember_host_t* h);

/* Poll mode: the first call restores saved links / autostarts the hub. */
EMBER_API void ember_host_run(ember_host_t* h, int max_wait_ms);
/* Thread-safe: makes the loop iterate (e.g. after ember_call()/ember_link_rx() done by you). */
EMBER_API void ember_host_wakeup(ember_host_t* h);

/* JSON-RPC for host.* methods (RPC_API.md §3) AND core methods (forwarded to ember_call).
 * Returns a malloc'd envelope {"ok":true,"result":{…}} / {"ok":false,"error":{code,message}};
 * release with ember_host_free(). */
EMBER_API char* ember_host_call(ember_host_t* h, const char* method, const char* params_json);
EMBER_API void ember_host_free(void* p);

/* Convenience: current hub port (0 if not running). */
EMBER_API int ember_host_hub_port(ember_host_t* h);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EMBER_HOST_H */
