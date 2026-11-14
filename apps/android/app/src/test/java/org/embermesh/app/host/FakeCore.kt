package org.embermesh.app.host

import org.embermesh.app.core.CoreApi
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.CopyOnWriteArrayList

/** In-memory stand-in for the core's links.* / settings.* RPC (enough for host logic tests). */
class FakeCore(var supportsLinks: Boolean = true) : CoreApi {
    val links = LinkedHashMap<Int, JSONObject>()
    var nextId = 1
    val calls = CopyOnWriteArrayList<String>()
    val rx = CopyOnWriteArrayList<Pair<Int, ByteArray>>()
    val endpointStates = CopyOnWriteArrayList<JSONObject>()
    var settings = JSONObject()
    /** When true, links.configure stores a new `endpoint` descriptor (a core that supports it). */
    var configureStoresEndpoint = false
    var locked = false

    private fun ok(r: JSONObject) = JSONObject().put("ok", true).put("result", r).toString()
    private fun err(code: String) = JSONObject().put("ok", false).put("error", JSONObject().put("code", code).put("message", code)).toString()

    @Synchronized
    override fun call(method: String, paramsJson: String): String {
        calls.add(method)
        val p = JSONObject(paramsJson)
        if (!supportsLinks && method.startsWith("links.")) return err("not_implemented")
        return when (method) {
            "links.add" -> {
                val id = nextId++
                val l = JSONObject().put("id", id).put("type", p.getString("type")).put("name", p.optString("name"))
                    .put("config", p.optJSONObject("config") ?: JSONObject()).put("endpoint", p.optJSONObject("endpoint")).put("state", "down")
                links[id] = l
                ok(JSONObject().put("link", l))
            }
            "links.list" -> ok(JSONObject().put("links", if (locked) JSONArray() else JSONArray(links.values.toList())))
            "links.types" -> ok(JSONObject().put("types", JSONArray()
                .put(JSONObject().put("type", "raw").put("endpoint_kinds", JSONArray(listOf("udp", "websocket", "ws_server", "loopback", "wifi_direct"))))
                .put(JSONObject().put("type", "stream").put("endpoint_kinds", JSONArray(listOf("serial", "tcp", "ble", "ble_peer"))))
                .put(JSONObject().put("type", "kiss").put("endpoint_kinds", JSONArray(listOf("serial", "tcp", "ble"))))
                .put(JSONObject().put("type", "audio").put("endpoint_kinds", JSONArray(listOf("audio"))))
                .put(JSONObject().put("type", "meshtastic").put("endpoint_kinds", JSONArray(listOf("serial", "tcp", "ble"))))
                .put(JSONObject().put("type", "lora_at").put("endpoint_kinds", JSONArray(listOf("serial", "ble", "tcp"))))))
            "links.configure" -> {
                val l = links[p.optInt("id")] ?: return err("not_found")
                p.optJSONObject("config")?.let { c -> val m = l.getJSONObject("config"); c.keys().forEach { m.put(it, c.get(it)) } }
                if (p.optString("name").isNotEmpty()) l.put("name", p.getString("name"))
                if (configureStoresEndpoint && p.has("endpoint")) l.put("endpoint", p.getJSONObject("endpoint"))
                ok(JSONObject().put("link", JSONObject(l.toString())))
            }
            "links.remove" -> if (links.remove(p.optInt("id")) != null) ok(JSONObject()) else err("not_found")
            "links.endpoint_state" -> { endpointStates.add(p); ok(JSONObject()) }
            "settings.get" -> ok(JSONObject().put("settings", settings))
            "settings.set" -> { val s = p.getJSONObject("settings"); s.keys().forEach { settings.put(it, s.get(it)) }; ok(JSONObject().put("settings", settings)) }
            "node.info" -> ok(JSONObject().put("node_id", "0123456789abcdef"))
            else -> err("not_implemented")
        }
    }

    override fun linkRx(linkId: Int, data: ByteArray, off: Int, len: Int) { rx.add(linkId to data.copyOfRange(off, off + len)) }
    override fun audioRx(linkId: Int, samples: ShortArray, count: Int, sampleRate: Int) {}
    override fun audioTx(linkId: Int, out: ShortArray, max: Int, sampleRate: Int) = 0
    override fun wake() {}
}
