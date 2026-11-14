package org.embermesh.app.host

import org.embermesh.app.util.str
import org.json.JSONArray
import org.json.JSONObject

/**
 * All endpoint kinds this build knows, in host.info order. The link types each kind can carry come
 * from the core's links.types (`endpoint_kinds` of every type), set with [setCoreLinkTypes]; a
 * factory's own `linkTypes` only rank them and choose the default.
 */
class EndpointRegistry(factories: List<EndpointFactory>) {
    private val byKind = LinkedHashMap<String, EndpointFactory>().apply { factories.forEach { put(it.kind, it) } }

    /** link type -> endpoint kinds, in links.types order (null until the core answered). */
    @Volatile private var coreTypes: LinkedHashMap<String, List<String>>? = null

    fun get(kind: String): EndpointFactory? = byKind[kind]
    fun all(): Collection<EndpointFactory> = byKind.values
    val hasCoreTypes: Boolean get() = coreTypes != null

    /** Feeds the result of links.types ({types:[{type, endpoint_kinds}]}). */
    fun setCoreLinkTypes(result: JSONObject) {
        val arr = result.optJSONArray("types") ?: return
        val m = LinkedHashMap<String, List<String>>()
        for (i in 0 until arr.length()) {
            val t = arr.optJSONObject(i) ?: continue
            val type = t.str("type")
            if (type.isEmpty()) continue
            val k = t.optJSONArray("endpoint_kinds") ?: JSONArray()
            m[type] = (0 until k.length()).map { k.optString(it) }
        }
        if (m.isNotEmpty()) coreTypes = m
    }

    /** Compatible link types for [f], best first. */
    fun linkTypesFor(f: EndpointFactory): List<String> {
        val ct = coreTypes ?: return f.linkTypes
        fun carries(type: String) = when (type) {
            "raw" -> f.transport == Transport.DATAGRAM
            "audio" -> f.transport == Transport.AUDIO
            else -> f.transport == Transport.STREAM // stream / kiss / meshtastic / rnode / lora_at / rtl_tcp: byte streams
        }
        var types = ct.filter { (_, kinds) -> f.kind in kinds }.keys.filter(::carries)
        if (types.isEmpty()) {
            // The core lists this kind only under types its transport cannot carry (e.g. a byte-stream
            // wifi_direct listed for raw datagrams): use what the core offers for the same transport.
            val proxy = when (f.transport) { Transport.DATAGRAM -> "udp"; Transport.STREAM -> "tcp"; Transport.AUDIO -> "audio" }
            types = ct.filter { (_, kinds) -> proxy in kinds }.keys.filter(::carries).filter { it != "rtl_tcp" }
        }
        val rank = f.linkTypes
        return types.sortedBy { t -> rank.indexOf(t).let { if (it < 0) Int.MAX_VALUE else it } }
    }

    fun defaultLinkType(f: EndpointFactory): String {
        val l = linkTypesFor(f)
        return if (f.defaultLinkType in l) f.defaultLinkType else l.firstOrNull() ?: f.defaultLinkType
    }

    /** EndpointKind objects for host.info (RPC_API §3). */
    fun describe(ctx: HostContext): JSONArray {
        val out = JSONArray()
        for (f in byKind.values) {
            val reason = try { f.unavailableReason(ctx) } catch (t: Throwable) { t.message ?: "unavailable" }
            val o = JSONObject()
                .put("kind", f.kind).put("title", f.title).put("description", f.description)
                .put("available", reason == null)
                .put("scannable", f.scannable)
                .put("config", try { f.configSchema(ctx) } catch (t: Throwable) { JSONArray() })
                .put("link_types", JSONArray(linkTypesFor(f)))
                .put("default_link_type", defaultLinkType(f))
            if (reason != null) o.put("unavailable_reason", reason)
            out.put(o)
        }
        return out
    }
}
