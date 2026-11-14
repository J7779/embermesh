package org.embermesh.app.endpoint.wifidirect

import android.Manifest
import android.annotation.SuppressLint
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.net.wifi.p2p.WifiP2pConfig
import android.net.wifi.p2p.WifiP2pInfo
import android.net.wifi.p2p.WifiP2pManager
import android.os.Build
import android.os.Looper
import org.embermesh.app.endpoint.QueuedWriter
import org.embermesh.app.endpoint.StreamReader
import org.embermesh.app.endpoint.ble.StreamArbiter
import org.embermesh.app.endpoint.serial.UsbSerialEndpoint
import org.embermesh.app.endpoint.writeFlush
import org.embermesh.app.host.Endpoint
import org.embermesh.app.host.EndpointFactory
import org.embermesh.app.host.EndpointSink
import org.embermesh.app.host.Fields
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.embermesh.app.util.str
import org.json.JSONArray
import org.json.JSONObject
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.Socket
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.CountDownLatch
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit

/**
 * Wi-Fi Direct (P2P) without an access point. With a peer address we connect to it; without one we
 * create a group and wait. The group owner listens on TCP [PORT]; clients connect to it; the link is
 * then a byte stream (several clients are serialised like the TCP server).
 * Status: functional but only lightly tested (emulators have no Wi-Fi Direct).
 */
@SuppressLint("MissingPermission")
class WifiDirectEndpoint(private val ctx: HostContext, config: JSONObject, sink: EndpointSink) : Endpoint(config, sink) {
    private val peer = config.str("peer").trim()
    private val mgr = ctx.app.getSystemService(Context.WIFI_P2P_SERVICE) as WifiP2pManager
    private var channel: WifiP2pManager.Channel? = null
    @Volatile private var closed = false
    private var receiver: BroadcastReceiver? = null
    private var server: ServerSocket? = null
    private val conns = CopyOnWriteArrayList<Conn>()
    private val arbiter = StreamArbiter(60, 256 * 1024, System::currentTimeMillis) { b -> sink.onBytes(b) }
    private var tick: ScheduledFuture<*>? = null
    @Volatile private var started = false

    private inner class Conn(val s: Socket) {
        val id = "${s.inetAddress.hostAddress}:${s.port}"
        val w = QueuedWriter("p2p", 256, { s.getOutputStream().writeFlush(it) }) { drop(this) }
        val r = StreamReader("p2p", s.getInputStream(), { b, n -> arbiter.offer(id, b.copyOf(n), 4096) }, { drop(this) })
        fun close() { w.close(); r.close(); try { s.close() } catch (_: Exception) {} }
    }

    override fun open() {
        val ch = mgr.initialize(ctx.app, Looper.getMainLooper(), null) ?: throw HostError.unsupported("Wi-Fi Direct unavailable")
        channel = ch
        tick = ctx.scheduler.scheduleWithFixedDelay({ arbiter.tick() }, 30, 30, TimeUnit.MILLISECONDS)
        val r = object : BroadcastReceiver() {
            override fun onReceive(c: Context, i: Intent) {
                if (i.action == WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION) mgr.requestConnectionInfo(ch) { info -> onInfo(info) }
            }
        }
        receiver = r
        UsbSerialEndpoint.register(ctx.app, r, IntentFilter(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION))
        val listener = object : WifiP2pManager.ActionListener {
            override fun onSuccess() { sink.log("p2p request accepted") }
            override fun onFailure(reason: Int) { fail("Wi-Fi Direct request failed (${reasonText(reason)})") }
        }
        if (peer.isNotEmpty()) mgr.connect(ch, WifiP2pConfig().apply { deviceAddress = peer }, listener)
        else mgr.createGroup(ch, listener)
    }

    private fun onInfo(info: WifiP2pInfo?) {
        if (closed || info == null || !info.groupFormed) return
        if (started) return
        started = true
        ctx.io.execute {
            try {
                if (info.isGroupOwner) {
                    val ss = ServerSocket()
                    ss.reuseAddress = true
                    ss.bind(InetSocketAddress(PORT))
                    server = ss
                    sink.onState(true)
                    while (!closed) {
                        val s = ss.accept()
                        s.tcpNoDelay = true
                        conns.add(Conn(s))
                        sink.onStatus(status())
                    }
                } else {
                    val s = Socket()
                    var ok = false
                    for (i in 0 until 10) { // the owner may not be listening yet
                        try { s.connect(InetSocketAddress(info.groupOwnerAddress, PORT), 3000); ok = true; break } catch (_: Exception) { Thread.sleep(1000) }
                        if (closed) return@execute
                    }
                    if (!ok) throw java.io.IOException("cannot reach the group owner")
                    s.tcpNoDelay = true
                    conns.add(Conn(s))
                    sink.onState(true)
                    sink.onStatus(status())
                }
            } catch (t: Throwable) { fail(t.message ?: t.toString()) }
        }
    }

    private fun drop(c: Conn) {
        if (conns.remove(c)) { c.close(); arbiter.release(c.id); sink.onStatus(status()) }
        if (conns.isEmpty() && server == null) fail("peer disconnected")
    }

    private fun fail(msg: String) {
        if (closed) return
        close()
        sink.onState(false, msg)
    }

    override fun write(data: ByteArray) { for (c in conns) c.w.offer(data) }

    override fun close() {
        closed = true
        tick?.cancel(false)
        receiver?.let { try { ctx.app.unregisterReceiver(it) } catch (_: Exception) {} }
        try { server?.close() } catch (_: Exception) {}
        for (c in conns) c.close()
        conns.clear()
        channel?.let { ch -> try { mgr.removeGroup(ch, null) } catch (_: Exception) {} }
    }

    override fun status(): JSONObject = JSONObject().put("peers", JSONArray(conns.map { it.id })).put("owner", server != null)

    companion object {
        const val PORT = 47476
        fun reasonText(r: Int) = when (r) { WifiP2pManager.P2P_UNSUPPORTED -> "unsupported"; WifiP2pManager.BUSY -> "busy"; else -> "error $r" }
        fun perms(): List<String> = if (Build.VERSION.SDK_INT >= 33) listOf(Manifest.permission.NEARBY_WIFI_DEVICES) else listOf(Manifest.permission.ACCESS_FINE_LOCATION)
    }

    @SuppressLint("MissingPermission")
    object Factory : EndpointFactory {
        override val kind = "wifi_direct"
        override val title = "Wi-Fi Direct"
        override val description = "Phone-to-phone Wi-Fi without a router or hotspot. One phone creates a group, the other connects to it."
        override val scannable = true
        override val linkTypes = listOf("stream")

        override fun unavailableReason(ctx: HostContext): String? =
            if (!ctx.app.packageManager.hasSystemFeature(PackageManager.FEATURE_WIFI_DIRECT)) "this device has no Wi-Fi Direct" else null

        override fun permissions(ctx: HostContext, config: JSONObject) = perms() + org.embermesh.app.util.Perms.localNetwork()

        override fun configSchema(ctx: HostContext): JSONArray = Fields.of {
            text("peer", "Peer device address", default = "", help = "Leave empty to create a group and wait; or pick a device from Scan.")
        }

        override fun scan(ctx: HostContext, timeoutMs: Long): JSONArray {
            val mgr = ctx.app.getSystemService(Context.WIFI_P2P_SERVICE) as WifiP2pManager
            val ch = mgr.initialize(ctx.app, Looper.getMainLooper(), null) ?: throw HostError.unsupported("Wi-Fi Direct unavailable")
            mgr.discoverPeers(ch, null)
            try { Thread.sleep(timeoutMs.coerceIn(2000, 15_000)) } catch (_: InterruptedException) {}
            val latch = CountDownLatch(1)
            val out = JSONArray()
            mgr.requestPeers(ch) { list ->
                for (d in list?.deviceList ?: emptyList()) {
                    out.put(JSONObject().put("id", d.deviceAddress).put("name", d.deviceName ?: d.deviceAddress)
                        .put("detail", d.deviceAddress).put("config", JSONObject().put("peer", d.deviceAddress)))
                }
                latch.countDown()
            }
            latch.await(3, TimeUnit.SECONDS)
            try { mgr.stopPeerDiscovery(ch, null) } catch (_: Exception) {}
            return out
        }

        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink) = WifiDirectEndpoint(ctx, config, sink)
    }
}
