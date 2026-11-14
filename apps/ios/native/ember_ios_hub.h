/* EmberMesh iOS — tiny C wrapper around the host library's hub (host/src/host/hub.h).
 *
 * The iOS shell keeps driving the core node itself (NodeRunner: ember_tick / ember_poll on a
 * serial queue), so it cannot hand the node to ember_host_create(), which wants to be the only
 * consumer of ember_poll(). Instead this wrapper embeds just the host library's Hub — the same
 * HTTP server, /hub.json probe, static web root, /download/ and /ws mesh bus that emberd uses — on
 * its own loop thread, and exposes it as the `ws_server` endpoint:
 *
 *   core EMBER_OUT_LINK_TX for the hub link -> ember_ios_hub_broadcast()  -> every /ws guest
 *   a guest's binary /ws message           -> frame callback              -> ember_link_rx()
 *
 * Threading: create/start/stop/destroy/status from any one thread at a time (the Swift side uses
 * its endpoint queue); broadcast is thread-safe. Callbacks run on the hub's loop thread and must
 * not block on a thread that is itself waiting for start/stop/status.
 */
#ifndef EMBER_IOS_HUB_H
#define EMBER_IOS_HUB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ember_ios_hub ember_ios_hub;

typedef void (*ember_ios_hub_frame_fn)(void* user, const uint8_t* data, size_t len);
typedef void (*ember_ios_hub_clients_fn)(void* user, size_t guests);

ember_ios_hub* ember_ios_hub_create(ember_ios_hub_frame_fn on_frame, ember_ios_hub_clients_fn on_clients, void* user);
void ember_ios_hub_destroy(ember_ios_hub* h); /* stops first */

/* options_json: {"port":8437,"bind":"0.0.0.0","web_root":"/path","download_dirs":[…],
 *                "download_files":[…],"max_guests":200,"version":"…","name":"…","node_id":"…"}
 * Returns 0 on success; otherwise writes a message to err (NUL terminated, truncated to err_len). */
int ember_ios_hub_start(ember_ios_hub* h, const char* options_json, char* err, size_t err_len);
void ember_ios_hub_stop(ember_ios_hub* h);

/* Thread-safe; frames are dropped while the hub is not running. */
void ember_ios_hub_broadcast(ember_ios_hub* h, const uint8_t* data, size_t len);

/* Thread-safe: name/node_id shown in /hub.json (JSON object text, e.g. {"name":"Ann","node_id":"…"}). */
void ember_ios_hub_set_probe(ember_ios_hub* h, const char* probe_json);

/* malloc'd JSON {"running":bool,"port":n,"urls":[…],"clients":n,"stats":{…}}; free with ember_ios_hub_free. */
char* ember_ios_hub_status(ember_ios_hub* h);
void ember_ios_hub_free(void* p);

#ifdef __cplusplus
}
#endif

#endif
