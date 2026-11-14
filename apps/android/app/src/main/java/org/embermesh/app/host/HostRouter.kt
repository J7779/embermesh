package org.embermesh.app.host

import android.util.Base64
import org.embermesh.app.core.CoreApi
import org.embermesh.app.services.DeviceServices
import org.embermesh.app.util.bool
import org.embermesh.app.util.int
import org.embermesh.app.util.long
import org.embermesh.app.util.str
import org.embermesh.app.util.strOrNull
import org.json.JSONArray
import org.json.JSONObject

/** App-level controls the router needs from the runtime (implemented by EmberRuntime). */
interface AppControls {
    val appVersion: String
    fun notifyUser(title: String, body: String)
    fun setKeepAwake(on: Boolean)
    /** Android-only settings: {start_on_boot, keep_cpu_awake, notify_messages, ignoring_battery_optimizations, ...}. */
    fun androidSettings(): JSONObject
    fun setAndroidSettings(s: JSONObject): JSONObject
    /** Opens the system battery-optimisation screen for this app; false if not possible. */
    fun openBatterySettings(): Boolean
    /** Pushes a fix to the core (position.set) and emits host.location. */
    fun onLocationFix(pos: JSONObject)
    /** The phone hub (null when this build/device has none). */
    val hub: org.embermesh.app.hub.PhoneHub? get() = null
    /** host.haptic; throws unsupported when the device has no vibrator. */
    fun haptic(kind: String): Boolean = throw HostError.unsupported("no haptics")
    /** Platform accessibility state for host.info.accessibility (RPC_API §8). */
    fun accessibility(): JSONObject? = null
}

/**
 * The Android host router: answers every `host.*` method of RPC_API §3 (plus a few documented
 * `host.android.*` / `host.app.*` extensions). Blocking; called on an I/O thread.
 */
class HostRouter(
    private val ctx: HostContext,
    private val core: CoreApi,
    private val registry: EndpointRegistry,
    private val links: LinkManager,
    private val services: DeviceServices,
    private val app: AppControls,
    /** Native Morse capture sources (RPC_API §6); null = not offered. */
    private val capture: org.embermesh.app.capture.CaptureManager? = null,
) {
    fun features(): List<String> {
        val f = LinkedHashSet<String>()
        f.addAll(try { services.features() } catch (t: Throwable) { emptyList() })
        f.add("notify")
        f.add("keep_awake")
        f.add("background")       // the mesh keeps running in a foreground service
        f.add("android_settings") // host.android.settings.*
        f.add("share_app")        // host.app.share
        f.add("haptic")           // host.haptic (unsupported on devices without a vibrator)
        f.add("link_configure")   // host.link.configure (endpoint settings editable after creation)
        capture?.let { f.addAll(it.features()) } // capture_camera_luma / capture_mic_pcm
        if (app.hub != null && org.embermesh.app.hub.HubNative.available) f.add("hub")
        return f.toList()
    }

    fun handle(method: String, params: JSONObject): JSONObject = when (method) {
        "host.info" -> info()
        "host.endpoints.scan" -> scan(params)
        "host.link.create" -> links.create(params)
        "host.link.remove" -> { links.remove(requireId(params)); JSONObject() }
        "host.link.reconnect" -> links.reconnect(requireId(params))
        "host.link.status" -> JSONObject().put("links", links.statusJson())
        "host.link.configure" -> links.configure(params)

        "host.location.get" -> {
            val pos = services.locationGet(params.long("timeout_ms", 30_000).coerceIn(1_000, 120_000))
            app.onLocationFix(pos)
            pos
        }
        "host.location.watch" -> {
            services.locationWatch(params.bool("enabled", true)) { app.onLocationFix(it) }
            JSONObject()
        }
        "host.qr.scan" -> JSONObject().put("text", services.qrScan())
        "host.file.save" -> {
            val name = params.str("name").ifEmpty { throw HostError.badParams("name required") }
            val data = decodeB64(params.str("data_b64"))
            services.fileSave(sanitizeFileName(name), params.str("mime", "application/octet-stream"), data, params.str("mode", "save"))
            JSONObject()
        }
        "host.file.pick" -> {
            val f = services.filePick(params.strOrNull("accept"))
            JSONObject().put("name", f.name).put("mime", f.mime).put("data_b64", Base64.encodeToString(f.data, Base64.NO_WRAP))
        }
        "host.signal.flash" -> {
            val mode = params.str("mode", "auto")
            if (mode !in setOf("auto", "torch", "screen")) throw HostError.badParams("mode must be auto, torch or screen")
            services.flash(pattern(params), mode)
        }
        "host.signal.tone" -> { services.tone(pattern(params), params.int("freq_hz", 700).coerceIn(100, 4000)); JSONObject() }
        "host.signal.stop" -> { services.stopSignals(); JSONObject() }
        "host.notify" -> {
            app.notifyUser(params.str("title", "EmberMesh"), params.str("body"))
            JSONObject()
        }
        "host.capture.start" -> (capture ?: throw HostError.unsupported("no native capture")).start(params)
        "host.capture.update" -> (capture ?: throw HostError.unsupported("no native capture")).update(params)
        "host.capture.stop" -> (capture ?: throw HostError.unsupported("no native capture")).stop(params)
        "host.capture.status" -> (capture ?: throw HostError.unsupported("no native capture")).status()
        "host.signal.stats" -> services.signalStats()
        "host.haptic" -> {
            val kind = params.str("kind", "tap")
            if (kind !in setOf("tap", "success", "warning", "error")) throw HostError.badParams("kind must be tap, success, warning or error")
            JSONObject().put("performed", app.haptic(kind))
        }
        "host.keep_awake" -> { app.setKeepAwake(params.bool("enabled", true)); JSONObject() }

        // Android extensions (not in RPC_API §3; advertised via features).
        "host.app.share" -> { services.shareApp(params.str("mode", "share")); JSONObject() }
        "host.android.settings.get" -> JSONObject().put("settings", app.androidSettings())
        "host.android.settings.set" -> JSONObject().put("settings", app.setAndroidSettings(params.optJSONObject("settings") ?: params))
        "host.android.battery_optimization" -> JSONObject().put("opened", app.openBatterySettings())

        "host.hub.start" -> hubStart(params)
        "host.hub.stop" -> hubStop()
        "host.hub.status" -> hubStatus()
        "host.webrtc.create_offer", "host.webrtc.accept_offer", "host.webrtc.accept_answer" ->
            throw HostError.unsupported("WebRTC pairing is not available in the Android app")
        else -> throw HostError.unsupported("unknown host method $method")
    }

    // --- hub (the hub is a `ws_server` endpoint under a `raw` link, persisted by the core like any link) ---

    private fun hubLinks() = links.activeLinks().filter { it.def.endpointKind == "ws_server" }

    private fun hubStatus(): JSONObject {
        val hub = app.hub ?: throw HostError.unsupported("hub mode is not available in this build")
        val st = hub.status()
        hubLinks().firstOrNull()?.let { a ->
            st.put("link_id", a.def.id)
            if (!a.connected && a.error != null) st.put("error", a.error)
        }
        return st
    }

    private fun hubStart(p: JSONObject): JSONObject {
        if (app.hub == null || !org.embermesh.app.hub.HubNative.available) throw HostError.unsupported("hub mode is not available in this build")
        val port = p.int("port", org.embermesh.app.hub.PhoneHub.DEFAULT_PORT).also { if (it !in 1..65535) throw HostError.badParams("port must be 1..65535") }
        val existing = hubLinks()
        val same = existing.firstOrNull { it.def.endpointConfig.int("port", org.embermesh.app.hub.PhoneHub.DEFAULT_PORT) == port }
        existing.filter { it !== same }.forEach { links.remove(it.def.id) }
        val id = if (same != null) {
            if (!same.connected) links.reconnect(same.def.id)
            same.def.id
        } else {
            links.create(JSONObject().put("name", "Hub").put("endpoint", JSONObject().put("kind", "ws_server").put("config", JSONObject().put("port", port)))
                .put("link", JSONObject().put("type", "raw").put("config", JSONObject().put("preset", "websocket")))).getJSONObject("link").getInt("id")
        }
        val deadline = System.currentTimeMillis() + 5_000
        while (System.currentTimeMillis() < deadline) {
            val a = links.active(id) ?: break
            if (a.connected) return hubStatus()
            if (a.error != null) {
                val err = a.error
                links.remove(id) // do not keep retrying a hub that cannot listen
                throw HostError(HostError.IO_ERROR, err ?: "hub failed to start")
            }
            Thread.sleep(25)
        }
        return hubStatus()
    }

    private fun hubStop(): JSONObject {
        for (a in hubLinks()) links.remove(a.def.id)
        app.hub?.stop()
        return app.hub?.status() ?: JSONObject().put("running", false).put("urls", JSONArray()).put("clients", 0)
    }

    /**
     * Platform extras the shared UI renders generically (RPC_API §3 `host.info.actions`): each is a
     * button that calls `method` with `params`.
     */
    private fun actions(): JSONArray {
        val s = try { app.androidSettings() } catch (_: Throwable) { JSONObject() }
        val boot = s.optBoolean("start_on_boot", false)
        val exempt = s.optBoolean("ignoring_battery_optimizations", false)
        val a = JSONArray()
        fun act(id: String, title: String, desc: String, method: String, params: JSONObject? = null) =
            a.put(JSONObject().put("id", id).put("title", title).put("description", desc).put("method", method).apply { if (params != null) put("params", params) })
        act("share_app", "Share this app", "Send the EmberMesh app (APK) to another phone by Bluetooth, Quick Share or a file manager. No internet needed.",
            "host.app.share", JSONObject().put("mode", "share"))
        act("save_apk", "Save the app as a file", "Save the APK to a USB stick or SD card so it can be installed on other phones.",
            "host.app.share", JSONObject().put("mode", "save"))
        act("start_on_boot", if (boot) "Don't start when the phone starts" else "Start when the phone starts",
            "Currently ${if (boot) "on" else "off"}: the mesh ${if (boot) "starts" else "does not start"} relaying by itself after a reboot.",
            "host.android.settings.set", JSONObject().put("settings", JSONObject().put("start_on_boot", !boot)))
        act("battery_optimization", if (exempt) "Battery optimisation: exempt" else "Keep running with the screen off",
            if (exempt) "EmberMesh is exempt from battery optimisation, so it keeps relaying with the screen off. Opens the system setting."
            else "Android may pause the mesh when the screen is off. Opens the system dialog to exempt EmberMesh from battery optimisation.",
            "host.android.battery_optimization")
        return a
    }

    private fun info(): JSONObject {
        if (!registry.hasCoreTypes) try { registry.setCoreLinkTypes(core.rpc("links.types")) } catch (_: HostError) {}
        return infoJson()
    }

    private fun infoJson(): JSONObject = JSONObject()
        .put("platform", "android")
        .put("app_version", app.appVersion)
        .put("backend", "native")
        .put("endpoint_kinds", registry.describe(ctx))
        .put("features", JSONArray(features()))
        .put("actions", actions())
        .put("signal", try { services.signalInfo() } catch (_: Throwable) { JSONObject().put("torch", "no").put("screen", true) })
        .apply { app.accessibility()?.let { put("accessibility", it) } }
        .put("os_version", android.os.Build.VERSION.RELEASE ?: "")
        .put("sdk_int", android.os.Build.VERSION.SDK_INT)
        .put("device", ((android.os.Build.MANUFACTURER ?: "") + " " + (android.os.Build.MODEL ?: "")).trim())

    private fun scan(params: JSONObject): JSONObject {
        val kind = params.str("kind")
        val f = registry.get(kind) ?: throw HostError.unsupported("unknown endpoint kind '$kind'")
        f.unavailableReason(ctx)?.let { throw HostError.unsupported(it) }
        if (!f.scannable) throw HostError.unsupported("$kind is not scannable")
        org.embermesh.app.util.Perms.ensure(ctx, f.permissions(ctx, JSONObject().put("_scan", true)))
        val timeout = params.long("timeout_ms", 5_000).coerceIn(500, 60_000)
        return JSONObject().put("devices", f.scan(ctx, timeout))
    }

    companion object {
        fun requireId(p: JSONObject): Int = p.int("id", 0).also { if (it <= 0) throw HostError.badParams("id required") }

        /** pattern_ms with the RPC_API §10 limits (≤ 4,096 entries, each 10–60,000 ms, ≤ 10 min): bad_params otherwise. */
        fun pattern(p: JSONObject): LongArray {
            val a = p.optJSONArray("pattern_ms") ?: throw HostError.badParams("pattern_ms required")
            if (a.length() > org.embermesh.app.services.LightSender.MAX_ENTRIES) throw HostError.badParams("pattern_ms: at most ${org.embermesh.app.services.LightSender.MAX_ENTRIES} entries")
            val out = LongArray(a.length()) { i ->
                val v = a.opt(i)
                if (v !is Number || v.toDouble() != Math.floor(v.toDouble())) throw HostError.badParams("pattern_ms: whole milliseconds")
                v.toLong()
            }
            org.embermesh.app.services.LightSender.validate(out)
            return out
        }

        fun decodeB64(s: String): ByteArray {
            if (s.length > 64 * 1024 * 1024) throw HostError(HostError.TOO_LARGE, "file too large")
            return try { Base64.decode(s, Base64.DEFAULT) } catch (e: IllegalArgumentException) { throw HostError.badParams("data_b64 is not base64") }
        }

        /** Strips path separators and control characters from a suggested file name. */
        fun sanitizeFileName(name: String): String {
            val base = name.substringAfterLast('/').substringAfterLast('\\')
            val clean = base.filter { it >= ' ' && it !in "<>:\"|?*" }.trim().trimStart('.')
            return clean.take(120).ifEmpty { "ember-file" }
        }
    }
}
