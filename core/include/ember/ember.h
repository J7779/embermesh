/* EmberMesh core — stable C API.
 *
 * This is the ONLY surface platform code uses: Android (JNI), iOS (Swift/ObjC), WebAssembly (JS),
 * desktop (C++). It is deliberately tiny and callback-free so every binding is a few lines:
 *
 *   - Everything the app does is a JSON-RPC style call:  ember_call(node, "messages.send", "{...}")
 *     (method catalogue: docs/RPC_API.md).
 *   - Everything the core wants from the platform is drained from an outbox with ember_poll():
 *     app events (JSON) and bytes to write to a link's endpoint.
 *   - Hot paths that are not JSON: link bytes in, audio samples in/out.
 *
 * Driving the node:
 *     ember_tick(node, now_unix_ms);            // every ~20-50 ms while active, and after feeding input
 *     while (ember_poll(node, &out)) { ... }    // then drain the outbox
 *     ember_next_wakeup_ms(node)                // lets battery powered hosts sleep longer when idle
 *
 * Threading: all functions are thread-safe (one internal lock per node). No function calls back
 * into platform code. Strings are UTF-8, NUL terminated. Exception: the data returned by
 * ember_poll() is only valid until the next ember_poll() on the same node, so drain the outbox
 * from one thread at a time (or copy the data before another thread polls).
 */
#ifndef EMBER_EMBER_H
#define EMBER_EMBER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(EMBER_SHARED)
#  ifdef EMBER_BUILDING
#    define EMBER_API __declspec(dllexport)
#  else
#    define EMBER_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define EMBER_API __attribute__((visibility("default"), used))
#else
#  define EMBER_API
#endif

typedef struct ember_node ember_node;

/* Library version, e.g. "0.1.0". Static string. */
EMBER_API const char* ember_version(void);

/* Create a node.
 *   config_json: {"storage_dir":"/path" | "" (in-memory, nothing persisted),
 *                 "platform":"android"|"ios"|"web"|"desktop",
 *                 "app_version":"...", "device_name":"...", "now_ms": <unix ms>}
 *   entropy:     >= 32 bytes from the platform CSPRNG (SecRandomCopyBytes, SecureRandom,
 *                crypto.getRandomValues, /dev/urandom, BCryptGenRandom). Required.
 * Returns NULL only on allocation failure or missing entropy. A node whose storage is locked with a
 * passphrase starts in the locked state (see security.* RPC methods). */
EMBER_API ember_node* ember_create(const char* config_json, const uint8_t* entropy, size_t entropy_len);
EMBER_API void ember_destroy(ember_node* node);

/* Mix more platform entropy into the node's CSPRNG (optional; call any time). */
EMBER_API void ember_add_entropy(ember_node* node, const uint8_t* data, size_t len);

/* JSON-RPC. Always returns a malloc'd JSON envelope that the caller releases with ember_free():
 *     {"ok":true,"result":{...}}
 *     {"ok":false,"error":{"code":"bad_params","message":"..."}}
 * params_json may be NULL or "" (treated as {}). */
EMBER_API char* ember_call(ember_node* node, const char* method, const char* params_json);
EMBER_API void ember_free(void* ptr);

/* Advance timers. now_ms is wall-clock Unix time in milliseconds as best the host knows it
 * (the core tolerates jumps and wrong clocks; mesh time is tracked separately). */
EMBER_API void ember_tick(ember_node* node, uint64_t now_ms);
/* Milliseconds until the node next needs a tick if no input arrives (0 = now, capped at 60000). */
EMBER_API uint32_t ember_next_wakeup_ms(ember_node* node);

/* Bytes received from the endpoint underneath link `link_id` (ids come from links.add).
 * Stream endpoints: any chunking. Datagram endpoints: exactly one datagram per call. */
EMBER_API void ember_link_rx(ember_node* node, uint32_t link_id, const uint8_t* data, size_t len);

/* Audio for soft-modem links: mono signed 16-bit PCM at any sample rate >= 8000 Hz.
 * rx: samples captured from the radio / microphone.
 * tx: fills `out` with up to `max` samples to play; returns samples written, 0 when idle. */
EMBER_API void ember_audio_rx(ember_node* node, uint32_t link_id, const int16_t* samples, size_t count,
                              uint32_t sample_rate);
EMBER_API size_t ember_audio_tx(ember_node* node, uint32_t link_id, int16_t* out, size_t max,
                                uint32_t sample_rate);

/* Outbox. */
enum {
  EMBER_OUT_NONE = 0,
  EMBER_OUT_EVENT = 1,   /* data = UTF-8 JSON {"event":"message.new","data":{...}} (len excludes NUL; NUL terminated) */
  EMBER_OUT_LINK_TX = 2  /* data = bytes to write to the endpoint of link_id (one datagram per item for datagram endpoints) */
};
typedef struct ember_out {
  int32_t kind;
  uint32_t link_id;
  const uint8_t* data;
  size_t len;
} ember_out;

/* Pops the next outbox item into *out. Returns 1 if an item was returned, 0 if the outbox is empty.
 * out->data stays valid until the next ember_poll() or ember_destroy() on this node. Events and
 * link TX are bounded separately: if the host polls too slowly, old events are dropped first and
 * never displace frames waiting for a link (TX has its own bound). */
EMBER_API int ember_poll(ember_node* node, ember_out* out);

/* Load the offline guide library (a guides.pack built by tools/guidepack).
 * If copy == 0 the memory must stay valid and unchanged until ember_destroy() (zero-copy for
 * memory-mapped assets); otherwise the core keeps its own copy. Returns 0 on success, <0 on a
 * malformed pack. May be called again to replace the library. */
EMBER_API int ember_guides_load(ember_node* node, const uint8_t* pack, size_t len, int copy);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EMBER_EMBER_H */
