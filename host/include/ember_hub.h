/* EmberMesh host library — hub-only C API (for apps that drive the core node themselves).
 *
 * ember_host.h embeds the whole host around a node and must be the ONLY consumer of ember_poll().
 * Phone shells that already run their own node loop (ember_tick / ember_poll on their own queue)
 * use this API instead: it runs just the hub — HTTP server, static web root, /hub.json probe,
 * /download/ and the /ws mesh bus, with the same per-guest fair queuing, rate limits and slow-guest
 * eviction as emberd — on its own loop thread, and never touches the node. The app wires it to a
 * core link it created itself:
 *
 *   links.add {"type":"raw","config":{"preset":"websocket"},
 *              "endpoint":{"kind":"ws_server","config":{"port":8437}}}          -> link id L
 *   links.endpoint_state {id:L, connected:true} once ember_hub_start() returned 0
 *   EMBER_OUT_LINK_TX for link L         -> ember_hub_broadcast()  (to every /ws guest)
 *   on_frame(data,len) from a guest      -> ember_link_rx(node, L, data, len)
 *
 * /rpc (remote control) is not offered by this API (it answers 401). Use ember_host.h for that.
 *
 * Threading: create/start/stop/status/destroy may be called from any thread, one at a time.
 * broadcast and set_probe are thread-safe. Callbacks run on the hub's loop thread: keep them short,
 * do not call start/stop/status/destroy from inside them, and do not block them on a thread that is
 * waiting in start/stop/status. ember_link_rx() is thread-safe, so on_frame may call it directly.
 *
 * Built in every configuration of the host library (EMBER_HOST_AUDIO/SERIAL OFF included).
 */
#ifndef EMBER_HUB_H
#define EMBER_HUB_H

#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#define EMBER_HUB_API __attribute__((visibility("default")))
#else
#define EMBER_HUB_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ember_hub ember_hub;

/* One mesh frame from a guest (already size- and rate-limited). Valid only during the call. */
typedef void (*ember_hub_frame_fn)(void* user, const uint8_t* data, size_t len);
/* Number of guests on /ws changed. */
typedef void (*ember_hub_clients_fn)(void* user, size_t guests);

/* Starts the loop thread. Returns NULL on allocation failure. Callbacks may be NULL. */
EMBER_HUB_API ember_hub* ember_hub_create(ember_hub_frame_fn on_frame, ember_hub_clients_fn on_clients, void* user);
/* Stops the hub, joins the loop thread, frees everything. NULL is ignored. */
EMBER_HUB_API void ember_hub_destroy(ember_hub* h);

/* options_json (all optional):
 *   {"port":8437, "bind":"0.0.0.0", "web_root":"/path/to/dist/web", "download_dirs":["…"],
 *    "download_files":["…/embermesh.html"], "max_guests":200, "captive_portal":false,
 *    "version":"0.1.0", "name":"Ann", "node_id":"…"}          (name/node_id: shown in /hub.json)
 * port 0 = any free port (see ember_hub_status). Restarting with new options is allowed.
 * Returns 0 on success; otherwise -1 and a NUL-terminated message in err (if err_len > 0). */
EMBER_HUB_API int ember_hub_start(ember_hub* h, const char* options_json, char* err, size_t err_len);
EMBER_HUB_API void ember_hub_stop(ember_hub* h);

/* Thread-safe. Sends one frame to every guest; dropped while the hub is not running. */
EMBER_HUB_API void ember_hub_broadcast(ember_hub* h, const uint8_t* data, size_t len);

/* Thread-safe. JSON object merged into /hub.json, e.g. {"name":"Ann","node_id":"0123…"}. */
EMBER_HUB_API void ember_hub_set_probe(ember_hub* h, const char* probe_json);

/* malloc'd JSON {"running":bool,"port":n,"urls":[…],"clients":n,"stats":{…}}; release with
 * ember_hub_free(). */
EMBER_HUB_API char* ember_hub_status(ember_hub* h);
EMBER_HUB_API void ember_hub_free(void* p);

#ifdef __cplusplus
}
#endif

#endif /* EMBER_HUB_H */
