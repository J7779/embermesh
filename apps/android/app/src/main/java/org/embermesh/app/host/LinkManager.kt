package org.embermesh.app.host

import android.util.Log
import org.embermesh.app.core.CoreApi
import org.embermesh.app.util.Json
import org.embermesh.app.util.bool
import org.embermesh.app.util.int
import org.embermesh.app.util.str
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit

/** Calls the core and unwraps the RPC envelope; throws HostError on {"ok":false}. */
fun CoreApi.rpc(method: String, params: JSONObject = JSONObject()): JSONObject {
    val text = call(method, params.toString())
    val env = try { JSONObject(text) } catch (e: Exception) { throw HostError(HostError.INTERNAL, "bad core reply to $method") }
    if (env.optBoolean("ok", false)) return env.optJSONObject("result") ?: JSONObject()
    val err = env.optJSONObject("error") ?: JSONObject()
    throw HostError(err.str("code", HostError.INTERNAL), err.str("message", "$method failed"))
}

/**
 * One core link with an endpoint this host opens. Built from the core's `Link` object: the core is
 * the single source of truth (RPC_API "Link persistence"); nothing is saved on the Android side.
 * `endpoint` is the opaque descriptor stored with links.add: {kind, config, autostart?}.
 */
data class LinkDef(
    val id: Int,
    val name: String,
    val endpointKind: String,
    val endpointConfig: JSONObject,
    val linkType: String,
    val linkConfig: JSONObject,
    val autostart: Boolean,
) {
    /** The opaque endpoint object stored in the core with links.add. */
    fun endpointJson(): JSONObject = JSONObject().put("kind", endpointKind).put("config", endpointConfig).also {
        if (!autostart) it.put("autostart", false)
    }

    companion object {
        /** From a core `Link` (links.list / links.add result); null when it has no endpoint descriptor. */
        fun fromCoreLink(l: JSONObject): LinkDef? {
            val ep = l.optJSONObject("endpoint") ?: return null
            val kind = ep.str("kind")
            if (kind.isEmpty()) return null
            return LinkDef(
                id = l.int("id", 0), name = l.str("name"), endpointKind = kind,
                endpointConfig = ep.optJSONObject("config") ?: JSONObject(),
                linkType = l.str("type"), linkConfig = l.optJSONObject("config") ?: JSONObject(),
                autostart = ep.bool("autostart", true),
            )
        }
    }
}

/**
 * Owns every live endpoint and its core link: host.link.create/remove/reconnect/configure, outbox
 * LINK_TX routing, PTT and exponential-backoff reconnects. On every `node.ready` [sync] re-reads
 * links.list and (re)opens the endpoints — the normative startup step.
 */
class LinkManager(
    private val core: CoreApi,
    private val registry: EndpointRegistry,
    private val ctx: HostContext,
    /** Hook to request permissions (blocking). Default: Perms.ensure; unit tests override. */
    private val ensurePermissions: (List<String>) -> Unit = { org.embermesh.app.util.Perms.ensure(ctx, it) },
) {
    inner class Active(@Volatile var def: LinkDef) : EndpointSink {
        @Volatile var endpoint: Endpoint? = null
        @Volatile var connected = false
        @Volatile var error: String? = null
        @Volatile var closedByUser = false
        @Volatile var attempts = 0
        @Volatile var retry: ScheduledFuture<*>? = null
        @Volatile var pttRelease: ScheduledFuture<*>? = null
        @Volatile var lastStatus: JSONObject = JSONObject()
        override val linkId: Int get() = def.id

        override fun onBytes(data: ByteArray, off: Int, len: Int) {
            if (closedByUser || len <= 0) return
            rxBytes += len
            core.linkRx(def.id, data, off, len)
        }

        override fun onAudio(samples: ShortArray, count: Int, sampleRate: Int) {
            if (!closedByUser && count > 0) core.audioRx(def.id, samples, count, sampleRate)
        }

        override fun pullAudio(out: ShortArray, max: Int, sampleRate: Int): Int =
            if (closedByUser) 0 else core.audioTx(def.id, out, max, sampleRate)

        override fun onState(connected: Boolean, error: String?) = stateChanged(this, connected, error)

        override fun onStatus(status: JSONObject) { lastStatus = status }

        override fun log(msg: String) { logd("link ${def.id} (${def.endpointKind}): $msg") }

        @Volatile var rxBytes = 0L
        @Volatile var txBytes = 0L
    }

    private val links = ConcurrentHashMap<Int, Active>()
    private val syncLock = Any()

    fun active(id: Int): Active? = links[id]
    fun activeIds(): Set<Int> = links.keys.toSet()
    fun activeKinds(): Set<String> = links.values.filter { !it.closedByUser }.map { it.def.endpointKind }.toSet()
    fun activeLinks(): List<Active> = links.values.sortedBy { it.def.id }

    /** host.link.create */
    fun create(params: JSONObject): JSONObject {
        val ep = params.optJSONObject("endpoint") ?: throw HostError.badParams("endpoint required")
        val kind = ep.str("kind")
        val factory = registry.get(kind) ?: throw HostError.unsupported("unknown endpoint kind '$kind'")
        factory.unavailableReason(ctx)?.let { throw HostError.unsupported(it) }
        val epConfig = Fields.applyDefaults(factory.configSchema(ctx), ep.optJSONObject("config") ?: JSONObject())
        val link = params.optJSONObject("link") ?: JSONObject()
        val type = link.str("type").ifEmpty { registry.defaultLinkType(factory) }
        val linkConfig = link.optJSONObject("config") ?: JSONObject()
        val name = params.str("name").ifEmpty { factory.title }
        val autostart = params.bool("autostart", true)
        val persist = params.bool("persist", true)

        ensurePermissions(factory.permissions(ctx, epConfig))

        val draft = LinkDef(0, name, kind, epConfig, type, linkConfig, autostart)
        val added = core.rpc("links.add", JSONObject()
            .put("type", type).put("name", name).put("config", linkConfig)
            .put("endpoint", draft.endpointJson()).apply { if (!persist) put("persist", false) })
        val coreLink = added.optJSONObject("link") ?: throw HostError(HostError.INTERNAL, "links.add returned no link")
        val id = coreLink.int("id", 0)
        if (id <= 0) throw HostError(HostError.INTERNAL, "links.add returned no id")
        start(draft.copy(id = id))
        return JSONObject().put("link", coreLink(id) ?: coreLink)
    }

    /** host.link.remove */
    fun remove(id: Int) {
        val a = links.remove(id)
        if (a != null) stopEndpoint(a, report = false)
        try { core.rpc("links.remove", JSONObject().put("id", id)) } catch (e: HostError) {
            if (a == null || e.code != HostError.NOT_FOUND) throw e
        }
    }

    /** host.link.reconnect */
    fun reconnect(id: Int): JSONObject {
        val a = links[id]
        val def = a?.def ?: coreLink(id)?.let(LinkDef::fromCoreLink) ?: throw HostError(HostError.NOT_FOUND, "no link $id")
        if (registry.get(def.endpointKind) == null) throw HostError.unsupported("endpoint kind '${def.endpointKind}' is not available on this device")
        if (a != null) {
            links.remove(id)
            stopEndpoint(a, report = false)
        }
        ensurePermissions(registry.get(def.endpointKind)?.permissions(ctx, def.endpointConfig) ?: emptyList())
        start(def)
        return JSONObject().put("link", coreLink(id) ?: JSONObject().put("id", id))
    }

    /**
     * host.link.configure {id, name?, config?, endpoint?:{config}} (Android extension): link config and
     * name go to links.configure (live in the core). A changed endpoint config is applied live when the
     * core stores the new descriptor and the endpoint can [Endpoint.reconfigure]; otherwise the endpoint
     * is reopened, or — when the core cannot update the stored descriptor — the link is recreated
     * (links.add + links.remove; result `replaced:<old id>`).
     */
    fun configure(params: JSONObject): JSONObject {
        val id = params.int("id", 0).also { if (it <= 0) throw HostError.badParams("id required") }
        val cur = links[id]?.def ?: coreLink(id)?.let(LinkDef::fromCoreLink) ?: throw HostError(HostError.NOT_FOUND, "no link $id")
        val factory = registry.get(cur.endpointKind) ?: throw HostError.unsupported("unknown endpoint kind '${cur.endpointKind}'")
        val epIn = params.optJSONObject("endpoint")?.optJSONObject("config")
        val newEp = if (epIn == null) cur.endpointConfig else JSONObject(cur.endpointConfig.toString()).also { m ->
            epIn.keys().forEach { k -> m.put(k, epIn.get(k)) }
        }
        val autostart = params.optJSONObject("endpoint")?.let { if (it.has("autostart")) it.bool("autostart", true) else null } ?: cur.autostart
        val epChanged = newEp.toString() != cur.endpointConfig.toString() || autostart != cur.autostart
        val cp = JSONObject().put("id", id)
        params.optJSONObject("config")?.let { cp.put("config", it) }
        if (params.str("name").isNotBlank()) cp.put("name", params.str("name"))
        val next = cur.copy(endpointConfig = newEp, autostart = autostart)
        if (epChanged) {
            ensurePermissions(factory.permissions(ctx, newEp))
            cp.put("endpoint", next.endpointJson())
        }
        val res = core.rpc("links.configure", cp).optJSONObject("link") ?: JSONObject()
        val stored = res.optJSONObject("endpoint")?.optJSONObject("config")
        if (!epChanged) {
            links[id]?.let { a -> a.def = a.def.copy(name = res.str("name", a.def.name), linkConfig = res.optJSONObject("config") ?: a.def.linkConfig) }
            return JSONObject().put("link", res)
        }
        if (stored != null && stored.toString() == newEp.toString()) {
            // The core kept the new descriptor: same link id, update the endpoint in place if possible.
            val a = links[id]
            if (a != null && autostart) {
                a.def = next.copy(name = res.str("name", next.name))
                val ep = a.endpoint
                if (ep == null || !ep.reconfigure(newEp)) reopen(a)
            } else if (a != null) {
                links.remove(id); stopEndpoint(a, report = true)
            } else if (autostart) start(next)
            return JSONObject().put("link", coreLink(id) ?: res)
        }
        // Recreate under a new id with the old link's type, name and (just updated) config.
        val name = res.str("name", cur.name)
        val linkCfg = res.optJSONObject("config") ?: cur.linkConfig
        val added = core.rpc("links.add", JSONObject().put("type", cur.linkType).put("name", name).put("config", linkCfg)
            .put("endpoint", next.endpointJson()))
        val nid = added.optJSONObject("link")?.int("id", 0) ?: 0
        if (nid <= 0) throw HostError(HostError.INTERNAL, "links.add returned no id")
        remove(id)
        if (autostart) start(next.copy(id = nid, name = name, linkConfig = linkCfg))
        return JSONObject().put("link", coreLink(nid) ?: added.optJSONObject("link")).put("replaced", id)
    }

    /**
     * The normative startup step, run on every `node.ready` (start, unlock, duress unlock, wipe):
     * links.list → (re)open each endpoint we can provide → links.endpoint_state. Endpoints whose link
     * no longer exists in the core are closed. A locked node lists no links; nothing happens then.
     */
    fun sync() = synchronized(syncLock) {
        val list: JSONArray = try { core.rpc("links.list").optJSONArray("links") ?: JSONArray() } catch (e: HostError) {
            logw("links.list failed: ${e.code} ${e.message}")
            return
        }
        val seen = HashSet<Int>()
        for (i in 0 until list.length()) {
            val l = list.optJSONObject(i) ?: continue
            val def = LinkDef.fromCoreLink(l) ?: continue
            if (def.id <= 0) continue
            seen.add(def.id)
            val existing = links[def.id]
            if (existing != null && !existing.closedByUser && existing.def.endpointKind == def.endpointKind &&
                existing.def.endpointConfig.toString() == def.endpointConfig.toString()) {
                // Same endpoint already open: the (re-created) core link starts down, so report again.
                existing.def = def
                reportState(def.id, existing.connected, existing.error)
                continue
            }
            if (existing != null) { links.remove(def.id); stopEndpoint(existing, report = false) }
            if (!def.autostart) continue
            val f = registry.get(def.endpointKind)
            if (f == null) continue // not ours (e.g. a link for another platform's endpoint): leave it alone
            val reason = try { f.unavailableReason(ctx) } catch (t: Throwable) { t.message ?: "unavailable" }
            val missing = if (ctx.hasContext) org.embermesh.app.util.Perms.missing(ctx, f.permissions(ctx, def.endpointConfig)) else emptyList()
            if (reason != null || missing.isNotEmpty()) {
                reportState(def.id, false, reason ?: "permission required: ${missing.joinToString()}")
                continue
            }
            start(def)
        }
        for ((id, a) in links) if (id !in seen) { links.remove(id); stopEndpoint(a, report = false) }
    }

    /** Outbox LINK_TX: bytes the core wants written to [linkId]'s endpoint. Node thread. */
    fun onLinkTx(linkId: Int, data: ByteArray) {
        val a = links[linkId] ?: return
        val ep = a.endpoint ?: return
        if (!a.connected) return
        a.txBytes += data.size
        try { ep.write(data) } catch (t: Throwable) { logw("write on link $linkId failed: $t") }
    }

    /**
     * Core event link.ptt. An audio link keys its radio through the PTT target its endpoint config
     * names (`ptt`: "vox", "link:<id>" = an open serial link's RTS/DTR, "usb:<device>" = a USB serial
     * adapter used only for PTT; legacy `ptt_link_id`). Release (`on:false`, sent when the last sample
     * was handed out) is delayed by the endpoint's output latency so the tail is not cut off.
     */
    fun onPtt(linkId: Int, on: Boolean) {
        val a = links[linkId] ?: return
        val src = a.endpoint ?: return
        a.pttRelease?.cancel(false)
        a.pttRelease = null
        val key = { v: Boolean -> keyPtt(a, src, v) }
        if (on) { key(true); return }
        val delay = try { src.pttReleaseDelayMs() } catch (_: Throwable) { 0L }
        if (delay <= 0) { key(false); return }
        a.pttRelease = try {
            ctx.scheduler.schedule({ if (a.pttRelease != null) { a.pttRelease = null; key(false) } }, delay, TimeUnit.MILLISECONDS)
        } catch (_: Exception) { key(false); null }
    }

    private fun keyPtt(a: Active, src: Endpoint, on: Boolean) {
        val target = PttTarget.parse(a.def.endpointConfig)
        try {
            when (target) {
                is PttTarget.Link -> {
                    val via = links[target.id]?.takeIf { target.id != a.def.id }?.endpoint
                    if (via == null) { a.log("PTT target link ${target.id} is not open"); return }
                    via.keyLine(PttTarget.line(a.def.endpointConfig), PttTarget.invert(a.def.endpointConfig), on)
                }
                else -> src.setPtt(on) // VOX (no-op), a dedicated USB PTT adapter, or the endpoint itself
            }
        } catch (t: Throwable) { logw("ptt on link ${a.def.id} failed: $t") }
    }

    /**
     * The UI came to the foreground: links that could not open at startup because a runtime
     * permission was missing (e.g. Android 17's local-network permission after an upgrade) are
     * retried now, which prompts for the permission.
     */
    fun retryPermissionBlocked() {
        val list = try { core.rpc("links.list").optJSONArray("links") ?: JSONArray() } catch (_: HostError) { return }
        for (i in 0 until list.length()) {
            val l = list.optJSONObject(i) ?: continue
            val def = LinkDef.fromCoreLink(l) ?: continue
            if (!def.autostart || links[def.id]?.connected == true) continue
            val f = registry.get(def.endpointKind) ?: continue
            val perms = f.permissions(ctx, def.endpointConfig)
            if (perms.isEmpty() || !ctx.hasContext || org.embermesh.app.util.Perms.missing(ctx, perms).isEmpty()) continue
            try { reconnect(def.id) } catch (e: HostError) { reportState(def.id, false, e.message) }
        }
    }

    /** Open serial links usable as a PTT keyer (for the audio endpoint's picker). */
    fun pttCandidates(): List<Pair<Int, String>> = links.values.filter { it.def.endpointKind == "serial" && !it.closedByUser }
        .sortedBy { it.def.id }.map { it.def.id to it.def.name }

    /** Hot-plug / adapter-on: retry every disconnected link of [kind] now instead of waiting for backoff. */
    fun kick(kind: String) {
        for (a in links.values) {
            if (a.def.endpointKind != kind || a.connected || a.closedByUser) continue
            a.retry?.cancel(false)
            a.attempts = 0
            ctx.io.execute { if (links[a.def.id] === a && !a.connected && !a.closedByUser) reopen(a) }
        }
    }

    fun closeAll() {
        for (a in links.values) stopEndpoint(a, report = false)
        links.clear()
    }

    fun statusJson(): JSONArray = Json.arr(links.values.sortedBy { it.def.id }.map {
        JSONObject().put("id", it.def.id).put("kind", it.def.endpointKind).put("connected", it.connected)
            .put("error", it.error ?: JSONObject.NULL).put("rx_bytes", it.rxBytes).put("tx_bytes", it.txBytes)
            .put("status", it.lastStatus)
    })

    // ---------------------------------------------------------------------------------------------

    private fun start(def: LinkDef) {
        val a = Active(def)
        links.put(def.id, a)?.let { old -> stopEndpoint(old, report = false) }
        open(a)
    }

    private fun reopen(a: Active) {
        val old = a.endpoint
        a.endpoint = null
        try { old?.close() } catch (_: Throwable) {}
        open(a)
    }

    private fun open(a: Active) {
        if (a.closedByUser) return
        val factory = registry.get(a.def.endpointKind)
        if (factory == null) {
            reportState(a.def.id, false, "unknown endpoint kind ${a.def.endpointKind}")
            return
        }
        try {
            val ep = factory.create(ctx, a.def.endpointConfig, a)
            a.endpoint = ep
            ep.open()
        } catch (e: HostError) {
            stateChanged(a, false, e.message)
        } catch (t: Throwable) {
            stateChanged(a, false, t.message ?: t.toString())
        }
    }

    private fun stopEndpoint(a: Active, report: Boolean) {
        a.closedByUser = true
        a.retry?.cancel(false)
        a.pttRelease?.cancel(false)
        val ep = a.endpoint
        a.endpoint = null
        if (ep != null) try { ep.close() } catch (t: Throwable) { logw("close failed: $t") }
        if (a.connected || report) {
            a.connected = false
            reportState(a.def.id, false, null)
        }
    }

    private fun stateChanged(a: Active, connected: Boolean, error: String?) {
        if (links[a.def.id] !== a || a.closedByUser) return
        val changed = a.connected != connected || a.error != error
        a.connected = connected
        a.error = error
        if (connected) a.attempts = 0
        if (changed) reportState(a.def.id, connected, error)
        if (!connected) scheduleReconnect(a)
    }

    private fun scheduleReconnect(a: Active) {
        if (a.closedByUser) return
        a.retry?.cancel(false)
        val delay = backoffMs(a.attempts++)
        a.retry = try {
            ctx.scheduler.schedule({
                if (links[a.def.id] === a && !a.connected && !a.closedByUser) reopen(a)
            }, delay, TimeUnit.MILLISECONDS)
        } catch (e: Exception) { null }
    }

    private fun reportState(id: Int, connected: Boolean, error: String?) {
        val p = JSONObject().put("id", id).put("connected", connected)
        if (error != null) p.put("error", error.take(500))
        try { core.rpc("links.endpoint_state", p) } catch (_: HostError) {}
        val ev = JSONObject().put("link_id", id).put("connected", connected)
        if (error != null) ev.put("error", error)
        ctx.emit("host.endpoint.state", ev)
    }

    private fun coreLink(id: Int): JSONObject? = try {
        val arr = core.rpc("links.list").optJSONArray("links") ?: JSONArray()
        (0 until arr.length()).mapNotNull { arr.optJSONObject(it) }.firstOrNull { it.int("id", 0) == id }
    } catch (e: HostError) { null }

    companion object {
        private const val TAG = "EmberLinks"

        /** 2 s, 4 s, 8 s ... capped at 60 s. */
        fun backoffMs(attempt: Int): Long = (2_000L shl attempt.coerceIn(0, 5)).coerceAtMost(60_000L)

        private fun logd(m: String) { try { Log.d(TAG, m) } catch (_: Throwable) {} }
        private fun logw(m: String) { try { Log.w(TAG, m) } catch (_: Throwable) {} }
    }
}

/** Where an audio link's PTT goes (audio endpoint config `ptt`, legacy `ptt_link_id`). */
sealed class PttTarget {
    object Vox : PttTarget()
    data class Link(val id: Int) : PttTarget()
    data class Usb(val device: String) : PttTarget()

    companion object {
        fun parse(c: JSONObject): PttTarget {
            val s = c.str("ptt").trim()
            when {
                s.startsWith("link:") -> s.removePrefix("link:").toIntOrNull()?.takeIf { it > 0 }?.let { return Link(it) }
                s.startsWith("usb:") -> s.removePrefix("usb:").takeIf { it.isNotEmpty() }?.let { return Usb(it) }
                s.isNotEmpty() -> return Vox
            }
            val legacy = c.int("ptt_link_id", 0)
            return if (legacy > 0) Link(legacy) else Vox
        }

        fun line(c: JSONObject): String = c.str("ptt_line", "rts").let { if (it == "dtr") "dtr" else "rts" }
        fun invert(c: JSONObject): Boolean = c.bool("ptt_invert", false)
    }
}
