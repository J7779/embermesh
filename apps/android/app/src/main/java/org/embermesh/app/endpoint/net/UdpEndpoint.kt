package org.embermesh.app.endpoint.net

import android.content.Context
import android.net.wifi.WifiManager
import org.embermesh.app.endpoint.QueuedWriter
import org.embermesh.app.host.Endpoint
import org.embermesh.app.host.EndpointFactory
import org.embermesh.app.host.EndpointSink
import org.embermesh.app.host.Fields
import org.embermesh.app.host.HostContext
import org.embermesh.app.util.int
import org.embermesh.app.util.str
import org.json.JSONArray
import org.json.JSONObject
import java.net.DatagramPacket
import java.net.Inet4Address
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.MulticastSocket
import java.net.NetworkInterface
import java.net.SocketTimeoutException
import java.util.Collections
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit

/**
 * LAN / hotspot mesh over IPv4 multicast (ARCHITECTURE §6: 239.83.77.69:47474, one mesh frame per
 * datagram). Joins the group on every up IPv4 interface — including the hotspot interface when this
 * phone *is* the hotspot (ap0 / swlan0 / wlan1), and USB/Bluetooth tethering — and sends each frame
 * on each of them, plus optional unicast peers for networks that filter multicast.
 */
class UdpEndpoint(private val ctx: HostContext, config: JSONObject, sink: EndpointSink) : Endpoint(config, sink) {
    private val group: InetAddress = InetAddress.getByName(config.str("group", DEFAULT_GROUP))
    private val port = config.int("port", DEFAULT_PORT).coerceIn(1, 65535)
    private val ttl = config.int("ttl", 1).coerceIn(1, 32)
    private val ifaceChoice = config.str("interface", "auto")
    private val maxDatagram = config.int("max_datagram", 1472).coerceIn(64, 65507)
    private val unicast: List<InetSocketAddress> = parsePeers(config.str("unicast_peers"), port).mapNotNull {
        try { InetSocketAddress(InetAddress.getByName(it.first), it.second) } catch (_: Exception) { null }
    }

    @Volatile private var running = false
    private var socket: MulticastSocket? = null
    private var lock: WifiManager.MulticastLock? = null
    private val joined: MutableSet<NetworkInterface> = Collections.synchronizedSet(LinkedHashSet())
    @Volatile private var localAddrs: Set<InetAddress> = emptySet()
    private var rescan: ScheduledFuture<*>? = null
    private var writer: QueuedWriter? = null
    private var rxThread: Thread? = null
    @Volatile private var rxCount = 0L
    @Volatile private var txCount = 0L

    override fun open() {
        running = true
        ctx.io.execute {
            try {
                if (ctx.hasContext) {
                    val wm = ctx.app.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
                    lock = wm.createMulticastLock("EmberMesh:udp").apply { setReferenceCounted(false); acquire() }
                }
                val s = MulticastSocket(null)
                s.reuseAddress = true
                s.bind(InetSocketAddress(port))
                s.timeToLive = ttl
                @Suppress("DEPRECATION")
                s.loopbackMode = true // true = disable loopback of our own datagrams
                s.soTimeout = 1000
                s.receiveBufferSize = 256 * 1024
                socket = s
                rejoin()
                writer = QueuedWriter("udp", 256, ::send) { t -> fail(t) }
                rxThread = Thread({ rxLoop(s) }, "ember-udp-rx").apply { isDaemon = true; start() }
                rescan = ctx.scheduler.scheduleWithFixedDelay({ try { rejoin() } catch (_: Throwable) {} }, 10, 10, TimeUnit.SECONDS)
                sink.onState(true)
            } catch (t: Throwable) {
                fail(t)
            }
        }
    }

    private fun fail(t: Throwable) {
        if (!running) return
        close()
        sink.onState(false, t.message ?: t.toString())
    }

    /** (Re)joins the group on every eligible interface; leaves vanished ones. */
    @Synchronized
    private fun rejoin() {
        val s = socket ?: return
        val now = eligibleInterfaces(ifaceChoice)
        val addrs = HashSet<InetAddress>()
        for (nif in now) for (a in Collections.list(nif.inetAddresses)) addrs.add(a)
        localAddrs = addrs
        val gone = joined.filter { j -> now.none { it.name == j.name } }
        for (g in gone) {
            try { s.leaveGroup(InetSocketAddress(group, port), g) } catch (_: Exception) {}
            joined.remove(g)
        }
        for (nif in now) {
            if (joined.any { it.name == nif.name }) continue
            try {
                s.joinGroup(InetSocketAddress(group, port), nif)
                joined.add(nif)
            } catch (e: Exception) {
                sink.log("join on ${nif.name} failed: $e")
            }
        }
        sink.onStatus(status())
    }

    private fun rxLoop(s: MulticastSocket) {
        val buf = ByteArray(65535)
        val pkt = DatagramPacket(buf, buf.size)
        while (running) {
            try {
                pkt.setData(buf, 0, buf.size)
                s.receive(pkt)
                if (pkt.port == port && pkt.address in localAddrs) continue // our own datagram via another interface
                if (pkt.length <= 0 || pkt.length > maxDatagram) continue
                rxCount++
                sink.onBytes(buf, 0, pkt.length)
            } catch (_: SocketTimeoutException) {
            } catch (t: Throwable) {
                if (running) { fail(t) }
                return
            }
        }
    }

    private fun send(data: ByteArray) {
        val s = socket ?: return
        if (data.size > maxDatagram) { sink.log("dropping ${data.size} byte datagram > max ${maxDatagram}"); return }
        val pkt = DatagramPacket(data, data.size, group, port)
        var sent = false
        synchronized(this) {
            for (nif in joined.toList()) {
                try {
                    s.networkInterface = nif
                    s.send(pkt)
                    sent = true
                } catch (e: Exception) {
                    sink.log("send on ${nif.name} failed: $e")
                }
            }
        }
        for (peer in unicast) {
            try { s.send(DatagramPacket(data, data.size, peer)); sent = true } catch (_: Exception) {}
        }
        if (sent) txCount++
    }

    override fun write(data: ByteArray) { writer?.offer(data) }

    override fun close() {
        running = false
        rescan?.cancel(false)
        writer?.close()
        try { socket?.close() } catch (_: Exception) {}
        socket = null
        try { if (lock?.isHeld == true) lock?.release() } catch (_: Exception) {}
        joined.clear()
    }

    override fun status(): JSONObject = JSONObject()
        .put("group", group.hostAddress).put("port", port)
        .put("interfaces", JSONArray(joined.map { it.name }))
        .put("unicast_peers", unicast.size)
        .put("rx_datagrams", rxCount).put("tx_datagrams", txCount)

    companion object {
        const val DEFAULT_GROUP = "239.83.77.69"
        const val DEFAULT_PORT = 47474

        /** "host:port, host" → pairs (default port when omitted). Pure, unit-tested. */
        fun parsePeers(text: String, defaultPort: Int): List<Pair<String, Int>> =
            text.split(',', ';', ' ', '\n').map { it.trim() }.filter { it.isNotEmpty() }.mapNotNull { p ->
                if (p.startsWith("[")) return@mapNotNull null // IPv6 not supported on this endpoint
                val i = p.lastIndexOf(':')
                if (i < 0) p to defaultPort
                else {
                    val port = p.substring(i + 1).toIntOrNull() ?: return@mapNotNull null
                    if (port !in 1..65535 || i == 0) null else p.substring(0, i) to port
                }
            }

        /** Interfaces that may carry the LAN mesh: up, not loopback, with an IPv4 address. */
        fun eligibleInterfaces(choice: String): List<NetworkInterface> {
            val all = try { Collections.list(NetworkInterface.getNetworkInterfaces()) } catch (_: Exception) { emptyList() }
            return all.filter { nif ->
                try {
                    nif.isUp && !nif.isLoopback && Collections.list(nif.inetAddresses).any { it is Inet4Address } &&
                        !nif.name.startsWith("rmnet") && !nif.name.startsWith("ccmni") && !nif.name.startsWith("dummy") &&
                        (choice == "auto" || choice.isEmpty() || nif.name == choice) &&
                        (nif.supportsMulticast() || isWifiLike(nif.name))
                } catch (_: Exception) { false }
            }
        }

        fun isWifiLike(name: String) = name.startsWith("wlan") || name.startsWith("ap") || name.startsWith("swlan") ||
            name.startsWith("rndis") || name.startsWith("usb") || name.startsWith("eth") || name.startsWith("bt-pan") || name.startsWith("p2p")
    }

    object Factory : EndpointFactory {
        override val kind = "udp"
        override fun permissions(ctx: HostContext, config: JSONObject) = org.embermesh.app.util.Perms.localNetwork()
        override val title = "Wi-Fi LAN / hotspot (UDP)"
        override val description = "Every EmberMesh device on the same Wi-Fi, hotspot or tethered network, via UDP multicast. Works when this phone is the hotspot; no internet needed."
        override val linkTypes = listOf("raw")
        override val transport = org.embermesh.app.host.Transport.DATAGRAM
        override fun configSchema(ctx: HostContext): JSONArray {
            val ifaces = listOf("auto" to "All networks (automatic)") +
                eligibleInterfaces("auto").map { it.name to it.name }
            return Fields.of {
                select("interface", "Network", ifaces, default = "auto", help = "Usually automatic: joins Wi-Fi, hotspot and tethering interfaces.")
                text("group", "Multicast group", default = DEFAULT_GROUP, advanced = true)
                number("port", "Port", default = DEFAULT_PORT, min = 1, max = 65535, advanced = true)
                number("ttl", "Multicast TTL", default = 1, min = 1, max = 32, advanced = true, help = "1 = this network only")
                text("unicast_peers", "Extra peers", default = "", advanced = true,
                    help = "Comma-separated host[:port] list for networks that block multicast.")
                number("max_datagram", "Max datagram", default = 1472, min = 64, max = 65507, unit = "bytes", advanced = true)
            }
        }
        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink) = UdpEndpoint(ctx, config, sink)
    }
}
