package org.embermesh.app.hub

import android.content.res.AssetManager
import android.util.Log
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.net.Inet4Address
import java.net.NetworkInterface

/**
 * The phone as a hub (ARCHITECTURE §5.2): serves the bundled web app (the WASM build, so guests run
 * their own node) and /hub.json on port 8437, and carries mesh frames on /ws. Reachable from phones
 * and laptops on the same Wi-Fi or on this phone's hotspot. The HTTP/WebSocket server is the host
 * library's Hub (C++), embedded through JNI; this class owns it and its web root on disk.
 *
 * Exactly one [Sink] (the `ws_server` endpoint of the hub link) receives guest frames.
 */
class PhoneHub(private val ctx: HostContext, private val assets: AssetManager?, private val filesDir: File, private val version: String,
               private val apkPath: String?) {
    interface Sink {
        fun onGuestFrame(frame: ByteArray)
        fun onClients(clients: Int)
    }

    @Volatile private var handle = 0L
    @Volatile private var sink: Sink? = null
    @Volatile var port = 0
        private set
    @Volatile var webRootKind = "none"
        private set
    @Volatile var clients = 0
        private set

    // Callbacks run on the hub's own loop thread. ember_hub.h forbids calling start/stop/status from
    // there (it would wait for itself), so everything beyond the frame hand-off is moved off it.
    private val cb = object : HubNative.Callback {
        override fun onFrame(frame: ByteArray) { sink?.onGuestFrame(frame) }
        override fun onClients(clients: Int) {
            this@PhoneHub.clients = clients
            try {
                ctx.io.execute {
                    sink?.onClients(clients)
                    ctx.emit("host.hub.clients", JSONObject().put("clients", clients))
                }
            } catch (_: Exception) {}
        }
    }

    val running: Boolean get() = handle != 0L && port != 0

    @Synchronized
    fun start(port: Int, sink: Sink, probe: JSONObject) {
        if (!HubNative.available) throw HostError.unsupported("this build has no hub (built with -PemberHub=false)")
        if (handle == 0L) handle = HubNative.create(cb)
        if (handle == 0L) throw HostError(HostError.INTERNAL, "hub could not be created")
        this.sink = sink
        val root = prepareWebRoot()
        HubNative.setProbe(handle, probe.toString())
        val opts = JSONObject().put("port", port).put("bind", "0.0.0.0").put("web_root", root?.absolutePath ?: "")
            .put("download_files", JSONArray(downloads().toList())).put("max_guests", 64).put("version", version)
            .put("name", probe.optString("name")).put("node_id", probe.optString("node_id"))
        val err = HubNative.start(handle, opts.toString())
        if (err != null) {
            this.port = 0
            throw HostError(HostError.IO_ERROR, if (err.contains("in use", true)) "port $port is already in use" else err)
        }
        this.port = port
        Log.i(TAG, "hub listening on $port (web root: $webRootKind) ${urls()}")
    }

    @Synchronized
    fun stop() {
        sink = null
        if (handle != 0L) {
            HubNative.stop(handle)
            HubNative.destroy(handle)
        }
        handle = 0L
        port = 0
        clients = 0
    }

    fun broadcast(frame: ByteArray) {
        val h = handle
        if (h != 0L) HubNative.broadcast(h, frame)
    }

    fun setProbe(probe: JSONObject) { val h = handle; if (h != 0L) HubNative.setProbe(h, probe.toString()) }

    /** host.hub.status shape: {running, urls, clients, port, web_root}. */
    fun status(): JSONObject {
        val h = handle
        val native = if (h != 0L) try { JSONObject(HubNative.status(h)) } catch (_: Exception) { JSONObject() } else JSONObject()
        val run = h != 0L && native.optBoolean("running", false)
        return JSONObject().put("running", run).put("urls", if (run) JSONArray(urls()) else JSONArray())
            .put("clients", if (run) native.optInt("clients", clients) else 0)
            .put("port", if (run) port else 0).put("web_root", webRootKind)
            .apply { native.optJSONObject("stats")?.let { put("stats", it) } }
    }

    /** http://<ipv4>:<port>/ for every usable interface; the hotspot address (often 192.168.43.1 / .49.1) first. */
    fun urls(): List<String> {
        if (port == 0) return emptyList()
        val out = ArrayList<Pair<Int, String>>()
        try {
            for (ni in NetworkInterface.getNetworkInterfaces()?.toList() ?: emptyList()) {
                if (!ni.isUp || ni.isLoopback) continue
                for (a in ni.inetAddresses.toList()) {
                    if (a !is Inet4Address || a.isLoopbackAddress) continue
                    val ip = a.hostAddress ?: continue
                    val name = ni.name ?: ""
                    val rank = when {
                        name.startsWith("ap") || name.startsWith("swlan") || name.startsWith("softap") -> 0 // hotspot
                        name.startsWith("wlan") || name.startsWith("eth") -> 1
                        name.startsWith("rndis") || name.startsWith("usb") || name.startsWith("bt-pan") -> 2   // tethering
                        name.startsWith("rmnet") || name.startsWith("ccmni") || name.startsWith("v4-") -> 9  // mobile data
                        else -> 5
                    }
                    if (ip.startsWith("169.254.")) continue
                    out.add(rank to "http://$ip:$port/")
                }
            }
        } catch (_: Exception) {}
        return out.sortedBy { it.first }.map { it.second }.distinct().ifEmpty { listOf("http://127.0.0.1:$port/") }
    }

    /** The APK is offered at /download/ so a guest phone can install the app from the hub. */
    private fun downloads(): Array<String> {
        val apk = apkPath?.let { File(it) }?.takeIf { it.isFile } ?: return emptyArray()
        val dir = File(filesDir, "hub-download").apply { mkdirs() }
        val link = File(dir, "EmberMesh.apk")
        try {
            if (link.exists() || isSymlink(link)) link.delete()
            android.system.Os.symlink(apk.absolutePath, link.absolutePath)
        } catch (_: Throwable) { return emptyArray() }
        return arrayOf(link.absolutePath)
    }

    private fun isSymlink(f: File) = try { f.canonicalPath != f.absolutePath } catch (_: Exception) { false }

    /**
     * Copies the bundled web build (assets/web = dist/web: WASM core + UI) to disk once per app
     * version; its guide library is the APK's own guides.pack. Falls back to assets/ui (no WASM:
     * guests see the UI but cannot run a node) and finally to the hub's built-in page.
     */
    private fun prepareWebRoot(): File? {
        val am = assets ?: return null
        val src = when {
            hasAsset(am, "web", "wasm") -> "web"
            hasAsset(am, "ui", "index.html") -> "ui"
            else -> { webRootKind = "builtin"; return null }
        }
        val root = File(filesDir, "hub-web")
        val stamp = File(root, ".stamp")
        val want = "$version/$src/${apkPath?.let { File(it).lastModified() } ?: 0}"
        if (!(stamp.isFile && stamp.readText() == want)) {
            root.deleteRecursively()
            // assets/web holds only what differs from assets/ui (deduplicated at build time): overlay it.
            if (src == "web" && hasAsset(am, "ui", "index.html")) copyAssetDir(am, "ui", root)
            copyAssetDir(am, src, root)
            if (src == "web") {
                // The web build looks for data/guides.pack; serve the same pack the native core maps.
                if (hasAsset(am, "", "guides.pack")) {
                    File(root, "data").mkdirs()
                    am.open("guides.pack").use { i -> File(root, "data/guides.pack").outputStream().use { i.copyTo(it, 256 * 1024) } }
                }
            }
            stamp.writeText(want)
        }
        webRootKind = if (src == "web") "web" else "ui-only"
        return root
    }

    private fun hasAsset(am: AssetManager, dir: String, name: String) = try { am.list(dir)?.contains(name) == true } catch (_: Exception) { false }

    private fun copyAssetDir(am: AssetManager, path: String, dest: File) {
        val kids = am.list(path) ?: emptyArray()
        if (kids.isEmpty()) {
            dest.parentFile?.mkdirs()
            am.open(path).use { i -> dest.outputStream().use { i.copyTo(it, 64 * 1024) } }
            return
        }
        dest.mkdirs()
        for (k in kids) copyAssetDir(am, "$path/$k", File(dest, k))
    }

    companion object {
        const val TAG = "EmberHub"
        const val DEFAULT_PORT = 8437
    }
}
