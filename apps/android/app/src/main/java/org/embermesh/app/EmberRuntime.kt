package org.embermesh.app

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.net.wifi.WifiManager
import android.os.Build
import android.os.PowerManager
import android.provider.Settings
import android.util.Log
import org.embermesh.app.bridge.BridgeDispatcher
import org.embermesh.app.core.CoreApi
import org.embermesh.app.core.EmberNode
import org.embermesh.app.core.NodeRunner
import org.embermesh.app.core.OutboxSink
import org.embermesh.app.endpoint.Endpoints
import org.embermesh.app.host.AppControls
import org.embermesh.app.host.EndpointRegistry
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.embermesh.app.host.HostRouter
import org.embermesh.app.host.LinkManager
import org.embermesh.app.host.rpc
import org.embermesh.app.hub.PhoneHub
import org.embermesh.app.service.AppPrefs
import org.embermesh.app.service.Notifier
import org.embermesh.app.services.DeviceServices
import org.embermesh.app.services.DeviceServicesFactory
import org.embermesh.app.util.bool
import org.json.JSONObject
import java.io.File
import java.security.SecureRandom
import java.util.concurrent.CopyOnWriteArrayList

/**
 * Process-wide owner of the node, its thread, the host router and every live endpoint. Started by
 * MainActivity or MeshService (whichever comes first) and kept alive by the foreground service, so
 * links keep relaying with the UI closed and the screen off.
 */
@SuppressLint("StaticFieldLeak") // holds the Application context only
object EmberRuntime : OutboxSink, AppControls {
    private const val TAG = "EmberRuntime"

    @Volatile private var started = false
    lateinit var appContext: Context
        private set

    var node: EmberNode? = null
        private set
    var runner: NodeRunner? = null
        private set
    lateinit var hostContext: HostContext
        private set
    lateinit var registry: EndpointRegistry
        private set
    lateinit var links: LinkManager
        private set
    lateinit var router: HostRouter
        private set
    lateinit var services: DeviceServices
        private set
    lateinit var prefs: AppPrefs
        private set
    @Volatile private var phoneHub: PhoneHub? = null
    @Volatile var capture: org.embermesh.app.capture.CaptureManager? = null
        private set
    @Volatile var a11y: org.embermesh.app.host.AccessibilityHints? = null
        private set

    override fun accessibility(): JSONObject? = a11y?.snapshot()

    override fun haptic(kind: String): Boolean {
        val vib = if (Build.VERSION.SDK_INT >= 31) (appContext.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as android.os.VibratorManager).defaultVibrator
            else @Suppress("DEPRECATION") (appContext.getSystemService(Context.VIBRATOR_SERVICE) as android.os.Vibrator)
        if (!vib.hasVibrator()) throw HostError.unsupported("this device has no vibrator")
        return hostContext.ui?.haptic(kind) ?: false // only with the UI in front (touch feedback)
    }
    override val hub: PhoneHub? get() = phoneHub

    /** /hub.json extras: this node's name and id (guests show "Connected to <name>'s hub"). */
    internal fun hubProbe(): JSONObject {
        val info = try { core.rpc("node.info") } catch (_: Throwable) { JSONObject() }
        // Privacy: only a name the user chose is shown to guests; otherwise "EmberMesh <id prefix>".
        val name = info.optString("name", "").ifEmpty { "EmberMesh hub " + info.optString("node_id", "").take(4) }.trim()
        return JSONObject().put("name", name).put("node_id", info.optString("node_id", "")).put("platform", "android")
    }

    /** Receives every event as {"event":..,"data":..} JSON text (core and host events). */
    val eventListeners = CopyOnWriteArrayList<(String) -> Unit>()

    @Volatile var uiVisible = false
    @Volatile var guidesStatus = "none"
        private set
    @Volatile private var keepAwakeRequested = false
    private var cpuLock: PowerManager.WakeLock? = null
    private var wifiLock: WifiManager.WifiLock? = null
    private var multicastHint = false

    val core: CoreApi get() = runner ?: throw HostError(HostError.INTERNAL, "node not running")

    val isStarted: Boolean get() = started

    @Synchronized
    fun start(context: Context): Boolean {
        if (started) return true
        appContext = context.applicationContext
        prefs = AppPrefs(appContext)
        Notifier.createChannels(appContext)

        val storage = File(appContext.filesDir, "ember").apply { mkdirs() }
        val cfg = JSONObject()
            .put("storage_dir", storage.absolutePath)
            .put("platform", "android")
            .put("app_version", appVersion)
            .put("device_name", "Android") // privacy: no model or owner-set device name in the core
            .put("now_ms", System.currentTimeMillis())
        val entropy = ByteArray(64).also { SecureRandom().nextBytes(it) } // >= 32 bytes required by ember_create
        val n = try { EmberNode.create(cfg.toString(), entropy) } catch (t: Throwable) {
            Log.e(TAG, "native library failed to load", t)
            null
        }
        if (n == null) {
            Log.e(TAG, "ember_create failed")
            return false
        }
        node = n
        loadGuides(n)

        hostContext = HostContext(appContext) { event, data -> emitHostEvent(event, data) }
        val r = NodeRunner(n, this)
        runner = r
        phoneHub = try {
            PhoneHub(hostContext, appContext.assets, appContext.filesDir, appVersion, appContext.applicationInfo.sourceDir)
        } catch (t: Throwable) { Log.w(TAG, "hub unavailable", t); null }
        registry = EndpointRegistry(Endpoints.factories({ phoneHub }, ::hubProbe))
        links = LinkManager(r, registry, hostContext)
        hostContext.pttLinks = { links.pttCandidates() }
        // Links live in the core only (RPC_API "Link persistence"); drop the old Android-side copy.
        File(appContext.filesDir, "links.json").delete()
        services = DeviceServicesFactory.create(hostContext)
        capture = org.embermesh.app.capture.CaptureManager(hostContext, r) { org.embermesh.app.util.Perms.ensure(hostContext, it) }
        (services as? org.embermesh.app.services.AndroidDeviceServices)?.cameraInUseByApp = { capture?.cameraActive() == true }
        router = HostRouter(hostContext, r, registry, links, services, this, capture)
        r.start()
        started = true
        a11y = org.embermesh.app.host.AccessibilityHints(appContext) { s -> emitHostEvent("host.accessibility", s) }
            .also { h -> hostContext.main.post { try { h.start() } catch (t: Throwable) { Log.w(TAG, "a11y", t) } } }
        // Fresh OS entropy mixed into the core's CSPRNG every 15 min (on top of the 64 bytes at create).
        entropyTask = try {
            hostContext.scheduler.scheduleWithFixedDelay({ mixEntropy() }, 15, 15, java.util.concurrent.TimeUnit.MINUTES)
        } catch (_: Exception) { null }
        registerHotplug()
        // Endpoints are (re)opened from the core's links.list on node.ready (see onCoreEvent).
        Log.i(TAG, "node started (core ${EmberNode.version()}, guides=$guidesStatus)")
        return true
    }

    private var hotplug: android.content.BroadcastReceiver? = null

    /** USB attach → retry serial links now (detach is handled by each endpoint). */
    private fun registerHotplug() {
        val r = object : android.content.BroadcastReceiver() {
            override fun onReceive(c: Context, i: Intent) {
                if (i.action == android.hardware.usb.UsbManager.ACTION_USB_DEVICE_ATTACHED) links.kick("serial")
            }
        }
        hotplug = r
        try {
            org.embermesh.app.endpoint.serial.UsbSerialEndpoint.register(appContext, r,
                android.content.IntentFilter(android.hardware.usb.UsbManager.ACTION_USB_DEVICE_ATTACHED))
        } catch (t: Throwable) { Log.w(TAG, "hotplug receiver", t) }
    }

    /** Stops everything (Quit). The next start() creates a fresh node over the same storage. */
    @Synchronized
    fun stop() {
        if (!started) return
        started = false
        entropyTask?.cancel(false)
        a11y?.let { h -> try { hostContext.main.post { h.stop() } } catch (_: Throwable) {} }
        try { links.closeAll() } catch (t: Throwable) { Log.w(TAG, "closeAll", t) }
        try { phoneHub?.stop() } catch (_: Throwable) {}
        try { capture?.stopAll() } catch (_: Throwable) {}
        try { services.shutdown() } catch (_: Throwable) {}
        hotplug?.let { try { appContext.unregisterReceiver(it) } catch (_: Throwable) {} }
        hotplug = null
        runner?.stop()
        node?.close()
        runner = null
        node = null
        hostContext.shutdown()
        releaseLocks()
    }

    private fun loadGuides(n: EmberNode) {
        // A newer pack received/saved at runtime wins over the one bundled in the APK.
        val updated = File(appContext.filesDir, "guides.pack")
        if (updated.isFile && n.loadGuidesFile(updated.absolutePath) == 0) {
            guidesStatus = "file"
            return
        }
        val bundled = try { appContext.assets.list("")?.contains("guides.pack") == true } catch (_: Exception) { false }
        guidesStatus = if (!bundled) "none" else {
            val rc = n.loadGuidesAsset(appContext.assets, "guides.pack")
            if (rc == 0) "asset" else "error($rc)"
        }
    }

    /** Builds a dispatcher for one WebView. */
    fun newDispatcher(evalJs: (String) -> Unit): BridgeDispatcher =
        BridgeDispatcher(core, { m, p -> router.handle(m, p) }, hostContext.io, evalJs)

    // --- OutboxSink (node thread) ------------------------------------------------------------------

    override fun onCoreEvent(json: String) {
        for (l in eventListeners) try { l(json) } catch (t: Throwable) { Log.w(TAG, "event listener", t) }
        // Cheap pre-filter: only parse the events the host itself reacts to.
        if (json.startsWith("{\"event\":\"node.ready\"") || json.contains("\"event\":\"node.ready\"")) {
            onNodeReady()
            return
        }
        if (json.contains("\"link.ptt\"") || json.contains("\"message.new\"") || json.contains("\"sos.received\"")) {
            val o = try { JSONObject(json) } catch (_: Exception) { return }
            val data = o.optJSONObject("data") ?: JSONObject()
            when (o.optString("event")) {
                "link.ptt" -> links.onPtt(data.optInt("link_id"), data.bool("on", false))
                "message.new" -> data.optJSONObject("message")?.let { m ->
                    if (!m.bool("outgoing", false) && !uiVisible && prefs.notifyMessages) Notifier.message(appContext, m)
                }
                "sos.received" -> Notifier.sos(appContext, data)
            }
        }
    }

    override fun onLinkTx(linkId: Int, data: ByteArray) = links.onLinkTx(linkId, data)

    /**
     * The normative host startup step, re-run on EVERY node.ready (after create, unlock, duress
     * unlock and wipe): links.types → compatible link types per endpoint kind; links.list → reopen
     * each endpoint → links.endpoint_state. A node that starts locked lists no links; its node.ready
     * after the unlock brings them up.
     */
    private fun onNodeReady() {
        try {
            hostContext.io.execute {
                mixEntropy()
                try { registry.setCoreLinkTypes(core.rpc("links.types")) } catch (t: Throwable) { Log.w(TAG, "links.types: $t") }
                try { links.sync() } catch (t: Throwable) { Log.w(TAG, "link sync failed", t) }
                updatePowerLocks()
            }
        } catch (_: Exception) { /* shutting down */ }
    }

    @Volatile private var lastPermRetryMs = 0L
    private var entropyTask: java.util.concurrent.ScheduledFuture<*>? = null
    private val rng = SecureRandom()

    /** 32 bytes from the OS CSPRNG (SecureRandom) into ember_add_entropy. */
    fun mixEntropy() {
        try { node?.addEntropy(ByteArray(32).also { rng.nextBytes(it) }) } catch (t: Throwable) { Log.w(TAG, "entropy", t) }
    }

    /** Called when the UI is in front: prompt for permissions links are waiting for (at most every 10 min). */
    fun retryPermissionBlockedLinks() {
        val now = System.currentTimeMillis()
        if (!started || now - lastPermRetryMs < 600_000) return
        lastPermRetryMs = now
        try { hostContext.io.execute { try { links.retryPermissionBlocked() } catch (t: Throwable) { Log.w(TAG, "permission retry", t) } } } catch (_: Exception) {}
    }

    private fun emitHostEvent(event: String, data: JSONObject) {
        val json = BridgeDispatcher.eventJson(event, data)
        for (l in eventListeners) try { l(json) } catch (t: Throwable) { Log.w(TAG, "event listener", t) }
        if (event == "host.endpoint.state") updatePowerLocks()
    }

    // --- AppControls -------------------------------------------------------------------------------

    override val appVersion: String
        get() = try {
            @Suppress("DEPRECATION")
            appContext.packageManager.getPackageInfo(appContext.packageName, 0).versionName ?: "0"
        } catch (_: Exception) { "0" }

    override fun notifyUser(title: String, body: String) = Notifier.generic(appContext, title, body)

    override fun setKeepAwake(on: Boolean) {
        keepAwakeRequested = on
        hostContext.ui?.let { ui -> hostContext.main.post { ui.setKeepScreenOn(on) } }
        updatePowerLocks()
    }

    override fun androidSettings(): JSONObject = prefs.toJson().put("ignoring_battery_optimizations", ignoringBatteryOptimizations())
        .put("guides", guidesStatus)

    override fun setAndroidSettings(s: JSONObject): JSONObject {
        prefs.update(s)
        if (s.has("run_in_background")) {
            if (prefs.runInBackground) MeshServiceControl.start(appContext) else MeshServiceControl.stopKeepingNode(appContext)
        }
        updatePowerLocks()
        return androidSettings()
    }

    override fun openBatterySettings(): Boolean {
        if (Build.VERSION.SDK_INT < 23) return false
        val pkg = appContext.packageName
        val intents = listOf(
            Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS, Uri.parse("package:$pkg")),
            Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS),
            Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:$pkg")),
        )
        for (i in intents) {
            try {
                i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                appContext.startActivity(i)
                return true
            } catch (_: Exception) {}
        }
        return false
    }

    override fun onLocationFix(pos: JSONObject) {
        try { core.call("position.set", pos.toString()) } catch (_: Throwable) {}
        emitHostEvent("host.location", pos)
    }

    fun ignoringBatteryOptimizations(): Boolean {
        if (Build.VERSION.SDK_INT < 23) return true
        val pm = appContext.getSystemService(Context.POWER_SERVICE) as PowerManager
        return pm.isIgnoringBatteryOptimizations(appContext.packageName)
    }

    // --- power -------------------------------------------------------------------------------------

    /** Endpoint kinds whose input only arrives if the CPU is awake (no OS wake-up on data). */
    private val cpuKinds = setOf("serial", "audio", "udp", "tcp", "tcp_server", "websocket", "ws_server", "bt_spp", "wifi_direct", "loopback")
    private val wifiKinds = setOf("udp", "tcp", "tcp_server", "websocket", "ws_server", "wifi_direct")

    /**
     * Wake-lock strategy: a PARTIAL wake lock is held only while a link that needs the CPU polling
     * is up (or the user asked for it / host.keep_awake); BLE-only meshes rely on the Bluetooth
     * stack waking the CPU. A high-performance Wi-Fi lock keeps multicast flowing for LAN links.
     */
    @Synchronized
    fun updatePowerLocks() {
        if (!started) return
        val kinds = try { links.activeKinds() } catch (_: Throwable) { emptySet() }
        val needCpu = keepAwakeRequested || prefs.keepCpuAwake == "always" ||
            (prefs.keepCpuAwake == "auto" && kinds.any { it in cpuKinds })
        val needWifi = kinds.any { it in wifiKinds }
        runner?.maxIntervalMs = if ("audio" in kinds) 20 else 0
        try {
            if (needCpu && cpuLock?.isHeld != true) {
                val pm = appContext.getSystemService(Context.POWER_SERVICE) as PowerManager
                cpuLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "EmberMesh:mesh").apply { setReferenceCounted(false); acquire() }
            } else if (!needCpu && cpuLock?.isHeld == true) {
                cpuLock?.release()
            }
            if (needWifi && wifiLock?.isHeld != true) {
                val wm = appContext.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
                @Suppress("DEPRECATION")
                val mode = if (Build.VERSION.SDK_INT >= 29) WifiManager.WIFI_MODE_FULL_LOW_LATENCY else WifiManager.WIFI_MODE_FULL_HIGH_PERF
                wifiLock = wm.createWifiLock(mode, "EmberMesh:lan").apply { setReferenceCounted(false); acquire() }
            } else if (!needWifi && wifiLock?.isHeld == true) {
                wifiLock?.release()
            }
        } catch (t: Throwable) {
            Log.w(TAG, "power locks", t)
        }
        multicastHint = needWifi
    }

    private fun releaseLocks() {
        try { if (cpuLock?.isHeld == true) cpuLock?.release() } catch (_: Throwable) {}
        try { if (wifiLock?.isHeld == true) wifiLock?.release() } catch (_: Throwable) {}
    }

    /** Short status for the persistent notification. */
    fun summary(): String {
        if (!started) return "Stopped"
        val st = try { links.statusJson() } catch (_: Throwable) { null } ?: return "Running"
        var up = 0
        for (i in 0 until st.length()) if (st.optJSONObject(i)?.optBoolean("connected") == true) up++
        val peers = try {
            JSONObject(core.call("mesh.peers", "{}")).optJSONObject("result")?.optJSONArray("peers")?.length()
        } catch (_: Throwable) { null }
        val base = if (st.length() == 0) "No links" else "$up of ${st.length()} links up"
        return if (peers != null) "$base · $peers peers" else base
    }
}

/** Starting/stopping the foreground service from anywhere. */
object MeshServiceControl {
    fun start(context: Context) {
        val i = Intent(context, org.embermesh.app.service.MeshService::class.java)
        try {
            if (Build.VERSION.SDK_INT >= 26) context.startForegroundService(i) else context.startService(i)
        } catch (t: Throwable) {
            Log.w("EmberRuntime", "cannot start mesh service now: $t")
        }
    }

    /** Stops the service (the node keeps running while the UI is open). */
    fun stopKeepingNode(context: Context) {
        context.stopService(Intent(context, org.embermesh.app.service.MeshService::class.java))
    }

    /** Quit: stop the service and the node. */
    fun quit(context: Context) {
        stopKeepingNode(context)
        EmberRuntime.stop()
    }
}
