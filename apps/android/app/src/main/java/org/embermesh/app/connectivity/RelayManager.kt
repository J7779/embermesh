package org.embermesh.app.connectivity

import org.json.JSONArray
import org.json.JSONObject

/** What the relay manager needs from the link manager (a fake in JVM tests). */
interface RelayLinks {
    /** auto_key → link id of every open `auto:"relay"` link. */
    fun current(): Map<String, Int>
    /** host.link.create with these params; throws HostError when the core refuses. */
    fun create(params: JSONObject)
    fun remove(id: Int)
}

/**
 * Internet relay links (CONNECTIVITY §4, RPC_API §11 "Host side"): one `relay` link over a
 * `websocket` endpoint per enabled relay while `connectivity.internet` is on, none otherwise.
 * Nothing connects before consent: [apply] only creates links for a state whose internet switch is
 * on, and the core refuses relay links without consent anyway. Reconnect/backoff is LinkManager's.
 */
class RelayManager(
    private val links: RelayLinks,
    private val now: () -> Long = System::currentTimeMillis,
    private val log: (String) -> Unit = {},
) {
    private val failedAt = HashMap<String, Long>()

    @Synchronized
    fun apply(state: ConnectivityState, active: Boolean) {
        val want = if (active && state.internetActive) state.relays.associateBy { key(it.url) } else emptyMap()
        val have = try { links.current() } catch (t: Throwable) { log("relay list: $t"); return }
        for ((k, id) in have) if (k !in want) try { links.remove(id) } catch (t: Throwable) { log("relay remove: $t") }
        val t = now()
        for ((k, spec) in want) {
            if (k in have) continue
            val f = failedAt[k]
            if (f != null && t - f < RETRY_AFTER_FAILURE_MS) continue
            try {
                links.create(params(spec))
                failedAt.remove(k)
            } catch (e: Throwable) {
                failedAt[k] = t
                log("relay ${spec.url}: ${e.message}")
            }
        }
        failedAt.keys.retainAll(want.keys)
    }

    companion object {
        const val RETRY_AFTER_FAILURE_MS = 30_000L

        fun key(url: String) = url.take(64)

        /**
         * host.link.create params for one relay (RPC_API §11). `protocols:["mqtt"]` makes it a relay
         * socket in the endpoint (validated TLS, no `insecure`). Credentials are the core's business.
         */
        fun params(r: ConnectivityState.RelaySpec): JSONObject {
            val ep = JSONObject().put("url", r.url).put("protocols", JSONArray().put("mqtt"))
                .put("ping_interval_s", 0) // the MQTT keepalive is the only idle traffic (CONNECTIVITY §4.9)
            if (r.allowPlaintext) ep.put("allow_plaintext", true)
            val lc = JSONObject().put("url", r.url)
            val host = Regex("^wss?://([^/:]+|\\[[^]]+])").find(r.url)?.groupValues?.get(1) ?: "relay"
            return JSONObject()
                .put("name", "Internet relay ($host)")
                .put("endpoint", JSONObject().put("kind", "websocket").put("config", ep))
                .put("link", JSONObject().put("type", "relay").put("config", lc))
                .put("persist", false).put("auto", "relay").put("auto_key", key(r.url))
        }
    }
}
