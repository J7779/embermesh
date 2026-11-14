package org.embermesh.app.endpoint.net

import org.embermesh.app.endpoint.QueuedWriter
import org.embermesh.app.endpoint.StreamReader
import org.embermesh.app.endpoint.ble.StreamArbiter
import org.embermesh.app.endpoint.writeFlush
import org.embermesh.app.host.Endpoint
import org.embermesh.app.host.EndpointFactory
import org.embermesh.app.host.EndpointSink
import org.embermesh.app.host.Fields
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.embermesh.app.util.bool
import org.embermesh.app.util.int
import org.embermesh.app.util.str
import org.json.JSONArray
import org.json.JSONObject
import java.io.BufferedInputStream
import java.io.InputStream
import java.io.OutputStream
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.Socket
import java.security.SecureRandom
import java.security.cert.X509Certificate
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit
import javax.net.ssl.HttpsURLConnection
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLSocket
import javax.net.ssl.SSLSocketFactory
import javax.net.ssl.TrustManager
import javax.net.ssl.X509TrustManager

/** TCP client: a plain byte stream (KISS-over-TCP to Direwolf, a desktop daemon, another phone's tcp_server). */
class TcpEndpoint(private val ctx: HostContext, config: JSONObject, sink: EndpointSink) : Endpoint(config, sink) {
    private val host = config.str("host").trim()
    private val port = config.int("port", TcpServerEndpoint.DEFAULT_PORT)
    private val timeoutMs = config.int("connect_timeout_ms", 8000).coerceIn(500, 60_000)
    @Volatile private var socket: Socket? = null
    private var writer: QueuedWriter? = null
    private var reader: StreamReader? = null
    @Volatile private var closed = false

    override fun open() {
        if (host.isEmpty()) throw HostError.badParams("host required")
        if (port !in 1..65535) throw HostError.badParams("bad port")
        ctx.io.execute {
            try {
                val s = Socket()
                s.tcpNoDelay = true
                s.keepAlive = true
                s.connect(InetSocketAddress(host, port), timeoutMs)
                if (closed) { s.close(); return@execute }
                socket = s
                val out = s.getOutputStream()
                writer = QueuedWriter("tcp", 512, { out.writeFlush(it) }) { t -> end(t) }
                reader = StreamReader("tcp", s.getInputStream(), { b, n -> sink.onBytes(b, 0, n) }, { t -> end(t) })
                sink.onState(true)
            } catch (t: Throwable) {
                end(t)
            }
        }
    }

    private fun end(t: Throwable?) {
        if (closed) return
        close()
        sink.onState(false, t?.message ?: "connection closed")
    }

    override fun write(data: ByteArray) { writer?.offer(data) }

    override fun close() {
        closed = true
        writer?.close()
        reader?.close()
        try { socket?.close() } catch (_: Exception) {}
    }

    override fun status(): JSONObject = JSONObject().put("remote", "$host:$port").put("connected", socket?.isConnected == true && !closed)

    object Factory : EndpointFactory {
        override val kind = "tcp"
        override fun permissions(ctx: HostContext, config: JSONObject) = org.embermesh.app.util.Perms.localNetwork()
        override val title = "TCP client"
        override val description = "Connect to a TCP server: Direwolf / KISS TNC (port 8001), the desktop daemon, or another phone's TCP server."
        override val scannable = true
        override val linkTypes = listOf("stream", "kiss", "meshtastic", "rnode", "lora_at", "rtl_tcp")
        override fun configSchema(ctx: HostContext): JSONArray = Fields.of {
            text("host", "Host", default = "", required = true, help = "IP address or host name")
            number("port", "Port", default = TcpServerEndpoint.DEFAULT_PORT, min = 1, max = 65535, required = true)
            number("connect_timeout_ms", "Connect timeout", default = 8000, min = 500, max = 60000, unit = "ms", advanced = true)
        }
        override fun scan(ctx: HostContext, timeoutMs: Long): JSONArray = Nsd.discover(ctx, timeoutMs) { h, p, name ->
            JSONObject().put("host", h).put("port", p).also { it.put("_name", name) }
        }
        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink) = TcpEndpoint(ctx, config, sink)
    }
}

/**
 * TCP server: other devices (phones on this hotspot, a laptop) connect here. TX goes to every client;
 * RX from several clients is serialised by a floor rule so their byte streams do not interleave.
 */
class TcpServerEndpoint(private val ctx: HostContext, config: JSONObject, sink: EndpointSink) : Endpoint(config, sink) {
    private val port = config.int("port", DEFAULT_PORT)
    private val maxClients = config.int("max_clients", 8).coerceIn(1, 64)
    private val advertise = config.bool("advertise", true)
    @Volatile private var closed = false
    private var server: ServerSocket? = null
    private val clients = CopyOnWriteArrayList<Client>()
    private var tick: ScheduledFuture<*>? = null
    private var nsd: Nsd.Registration? = null
    private val arbiter = StreamArbiter(idleMs = 60, capBytes = 256 * 1024, clock = System::currentTimeMillis) { b -> sink.onBytes(b) }

    private inner class Client(val socket: Socket) {
        val id = "${socket.inetAddress.hostAddress}:${socket.port}"
        val writer: QueuedWriter
        val reader: StreamReader
        init {
            val out = socket.getOutputStream()
            writer = QueuedWriter("tcps", 512, { out.writeFlush(it) }) { drop(this) }
            reader = StreamReader("tcps", socket.getInputStream(), { b, n -> arbiter.offer(id, b.copyOf(n), 4096) }, { drop(this) })
        }
        fun close() { writer.close(); reader.close(); try { socket.close() } catch (_: Exception) {} }
    }

    override fun open() {
        if (port !in 1..65535) throw HostError.badParams("bad port")
        ctx.io.execute {
            try {
                val ss = ServerSocket()
                ss.reuseAddress = true
                ss.bind(InetSocketAddress(port))
                server = ss
                tick = ctx.scheduler.scheduleWithFixedDelay({ arbiter.tick() }, 30, 30, TimeUnit.MILLISECONDS)
                // A manually created server: findable in "Scan" lists, but without ember=1/v=1 so
                // Nearby discovery never auto-dials it (CONNECTIVITY §3.2). Nothing identifying.
                if (advertise && ctx.hasContext) nsd = Nsd.register(ctx, "EmberMesh-" + Nsd.randomToken(), port, mapOf("link" to "stream"))
                sink.onState(true)
                publish()
                while (!closed) {
                    val s = ss.accept()
                    if (clients.size >= maxClients) { s.close(); continue }
                    s.tcpNoDelay = true
                    s.keepAlive = true
                    clients.add(Client(s))
                    publish()
                }
            } catch (t: Throwable) {
                if (!closed) { close(); sink.onState(false, t.message ?: t.toString()) }
            }
        }
    }

    private fun drop(c: Client) {
        if (clients.remove(c)) {
            c.close()
            arbiter.release(c.id)
            publish()
        }
    }

    private fun publish() = sink.onStatus(status())

    override fun write(data: ByteArray) { for (c in clients) c.writer.offer(data) }

    override fun close() {
        closed = true
        tick?.cancel(false)
        nsd?.unregister()
        try { server?.close() } catch (_: Exception) {}
        for (c in clients) c.close()
        clients.clear()
    }

    override fun status(): JSONObject = JSONObject().put("port", port).put("clients", JSONArray(clients.map { it.id }))

    companion object {
        const val DEFAULT_PORT = 47475
    }

    object Factory : EndpointFactory {
        override val kind = "tcp_server"
        override fun permissions(ctx: HostContext, config: JSONObject) = org.embermesh.app.util.Perms.localNetwork()
        override val title = "TCP server"
        override val description = "Let other devices on this Wi-Fi / hotspot connect to this phone over TCP (advertised as _embermesh._tcp)."
        override val linkTypes = listOf("stream", "kiss")
        override fun configSchema(ctx: HostContext): JSONArray = Fields.of {
            number("port", "Port", default = DEFAULT_PORT, min = 1024, max = 65535, required = true)
            number("max_clients", "Max clients", default = 8, min = 1, max = 64, advanced = true)
            bool("advertise", "Advertise on the network (DNS-SD)", default = true, advanced = true)
        }
        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink) = TcpServerEndpoint(ctx, config, sink)
    }
}

/** WebSocket client (hub guest mode: ws://<hub>:8437/ws, one mesh frame per binary message). */
class WebSocketEndpoint(private val ctx: HostContext, config: JSONObject, sink: EndpointSink) : Endpoint(config, sink) {
    private val url = config.str("url").trim()
    private val protocols: List<String> = protocolsOf(config)
    /**
     * A relay socket (RPC_API §11, CONNECTIVITY §4.11): `protocols` contains "mqtt". Then the TLS
     * certificate chain and host name are always validated (`insecure` is refused), `ws://` is
     * allowed only with `allow_plaintext` to a LAN / link-local / `.local` host, and messages are
     * capped at 8 KiB.
     */
    private val relaySocket = "mqtt" in protocols
    private val allowPlaintext = config.bool("allow_plaintext", false)
    private val insecure = config.bool("insecure", false) && !relaySocket
    private val maxMessage = if (relaySocket) RELAY_MAX_MESSAGE else MAX_MESSAGE
    private val pingS = config.int("ping_interval_s", 25).coerceIn(0, 600)
    @Volatile private var closed = false
    @Volatile private var socket: Socket? = null
    private var out: OutputStream? = null
    private var writer: QueuedWriter? = null
    private var pinger: ScheduledFuture<*>? = null
    private val rnd = SecureRandom()
    @Volatile private var lastRx = 0L

    override fun open() {
        val u = try { WsCodec.parseUrl(url) } catch (e: IllegalArgumentException) { throw HostError.badParams(e.message ?: "bad url") }
        if (relaySocket) {
            if (config.bool("insecure", false)) throw HostError.badParams("relay sockets always validate certificates (insecure refused)")
            if (!u.secure && !(allowPlaintext && org.embermesh.app.connectivity.LocalNets.isLanHost(u.host)))
                throw HostError.badParams("this relay needs wss:// (plain ws:// only with allow_plaintext and a LAN or .local host)")
        }
        ctx.io.execute {
            try {
                val s = connect(u)
                if (closed) { s.close(); return@execute }
                socket = s
                val input = BufferedInputStream(s.getInputStream())
                val o = s.getOutputStream()
                out = o
                handshake(u, input, o)
                s.soTimeout = 0
                writer = QueuedWriter("ws", 512, { sendFrame(WsCodec.OP_BINARY, it) }) { t -> end(t) }
                if (pingS > 0) pinger = ctx.scheduler.scheduleWithFixedDelay({ keepalive() }, pingS.toLong(), pingS.toLong(), TimeUnit.SECONDS)
                sink.onState(true)
                lastRx = System.currentTimeMillis()
                readLoop(input)
            } catch (t: Throwable) {
                end(t)
            }
        }
    }

    private fun connect(u: WsCodec.Url): Socket {
        val plain = Socket()
        plain.tcpNoDelay = true
        plain.keepAlive = true
        plain.connect(InetSocketAddress(u.host, u.port), 10_000)
        plain.soTimeout = 15_000
        if (!u.secure) return plain
        val factory: SSLSocketFactory = if (insecure) {
            val trustAll = arrayOf<TrustManager>(object : X509TrustManager {
                override fun checkClientTrusted(c: Array<out X509Certificate>?, a: String?) {}
                override fun checkServerTrusted(c: Array<out X509Certificate>?, a: String?) {}
                override fun getAcceptedIssuers(): Array<X509Certificate> = arrayOf()
            })
            SSLContext.getInstance("TLS").apply { init(null, trustAll, SecureRandom()) }.socketFactory
        } else SSLSocketFactory.getDefault() as SSLSocketFactory
        val ssl = factory.createSocket(plain, u.host, u.port, true) as SSLSocket
        ssl.startHandshake()
        if (!insecure && !HttpsURLConnection.getDefaultHostnameVerifier().verify(u.host, ssl.session)) {
            ssl.close()
            throw java.io.IOException("TLS certificate does not match ${u.host}")
        }
        return ssl
    }

    private fun handshake(u: WsCodec.Url, input: InputStream, o: OutputStream) {
        val key = Base64Std.encode(ByteArray(16).also { rnd.nextBytes(it) })
        val req = "GET ${u.path} HTTP/1.1\r\nHost: ${u.hostHeader}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" +
            "Sec-WebSocket-Key: $key\r\nSec-WebSocket-Version: 13\r\n" +
            (if (protocols.isNotEmpty()) "Sec-WebSocket-Protocol: ${protocols.joinToString(", ")}\r\n" else "") +
            "\r\n" // no User-Agent: nothing that names the app or device
        o.writeFlush(req.toByteArray(Charsets.US_ASCII))
        val head = readHttpHead(input)
        val lines = head.split("\r\n")
        val status = lines.firstOrNull() ?: ""
        if (!status.startsWith("HTTP/1.1 101") && !status.startsWith("HTTP/1.0 101")) throw java.io.IOException("upgrade refused: $status")
        val accept = lines.drop(1).firstOrNull { it.startsWith("sec-websocket-accept:", true) }?.substringAfter(':')?.trim()
        if (accept != WsCodec.acceptKey(key)) throw java.io.IOException("bad Sec-WebSocket-Accept")
        if (protocols.isNotEmpty()) {
            val chosen = lines.drop(1).firstOrNull { it.startsWith("sec-websocket-protocol:", true) }?.substringAfter(':')?.trim()
            if (chosen == null || chosen !in protocols) throw java.io.IOException("server did not accept subprotocol ${protocols.joinToString()}")
        }
    }

    private fun readHttpHead(input: InputStream): String {
        val sb = StringBuilder()
        while (sb.length < 16 * 1024) {
            val c = input.read()
            if (c < 0) throw java.io.EOFException("closed during handshake")
            sb.append(c.toChar())
            if (sb.endsWith("\r\n\r\n")) return sb.substring(0, sb.length - 4)
        }
        throw java.io.IOException("handshake response too large")
    }

    private fun readLoop(input: InputStream) {
        val asm = WsCodec.Assembler(maxMessage)
        while (!closed) {
            val f = WsCodec.readFrame(input, maxMessage)
            lastRx = System.currentTimeMillis()
            val (op, payload) = asm.push(f) ?: continue
            when (op) {
                WsCodec.OP_BINARY -> sink.onBytes(payload)
                WsCodec.OP_TEXT -> sink.log("ignoring text message (${payload.size} bytes)")
                WsCodec.OP_PING -> sendFrame(WsCodec.OP_PONG, payload)
                WsCodec.OP_PONG -> {}
                WsCodec.OP_CLOSE -> {
                    try { sendFrame(WsCodec.OP_CLOSE, WsCodec.closePayload(1000)) } catch (_: Exception) {}
                    throw java.io.IOException("server closed (${WsCodec.closeCode(payload)})")
                }
            }
        }
    }

    private fun keepalive() {
        if (closed) return
        if (System.currentTimeMillis() - lastRx > pingS * 3000L) { end(java.io.IOException("ping timeout")); return }
        try { sendFrame(WsCodec.OP_PING, ByteArray(0)) } catch (t: Throwable) { end(t) }
    }

    @Synchronized
    private fun sendFrame(op: Int, payload: ByteArray) {
        val o = out ?: return
        val mask = ByteArray(4).also { rnd.nextBytes(it) }
        o.writeFlush(WsCodec.encode(op, payload, mask))
    }

    private fun end(t: Throwable?) {
        if (closed) return
        close()
        sink.onState(false, t?.message ?: "connection closed")
    }

    override fun write(data: ByteArray) {
        if (data.size > maxMessage) { sink.log("dropping a ${data.size}-byte message (cap $maxMessage)"); return }
        writer?.offer(data)
    }

    override fun close() {
        if (!closed) {
            closed = true
            try { ctx.io.execute { try { sendFrame(WsCodec.OP_CLOSE, WsCodec.closePayload(1000)) } catch (_: Exception) {}; try { socket?.close() } catch (_: Exception) {} } }
            catch (_: Exception) { try { socket?.close() } catch (_: Exception) {} }
        }
        pinger?.cancel(false)
        writer?.close()
    }

    override fun status(): JSONObject = JSONObject().put("url", url)

    companion object {
        const val MAX_MESSAGE = 1024 * 1024
        const val RELAY_MAX_MESSAGE = 8 * 1024
        private val TOKEN = Regex("^[!#$%&'*+.^_`|~0-9A-Za-z-]{1,32}$")

        /** `protocols` (RFC 6455 subprotocol tokens, ≤ 8); anything else is a bad_params error. */
        fun protocolsOf(c: JSONObject): List<String> {
            val a = c.optJSONArray("protocols") ?: return emptyList()
            if (a.length() > 8) throw HostError.badParams("protocols: at most 8")
            return (0 until a.length()).map { i ->
                val p = a.optString(i, "")
                if (!TOKEN.matches(p)) throw HostError.badParams("protocols: invalid token")
                p
            }
        }
    }

    object Factory : EndpointFactory {
        override val kind = "websocket"
        override fun permissions(ctx: HostContext, config: JSONObject) = org.embermesh.app.util.Perms.localNetwork()
        override val title = "WebSocket (hub)"
        override val description = "Join a hub (a computer or phone serving EmberMesh at http://<ip>:8437/) over its /ws mesh socket."
        override val scannable = true
        override val linkTypes = listOf("raw")
        override val transport = org.embermesh.app.host.Transport.DATAGRAM
        override fun configSchema(ctx: HostContext): JSONArray = Fields.of {
            text("url", "Hub URL", default = "ws://192.168.43.1:8437/ws", required = true, help = "ws://<hub ip>:8437/ws")
            number("ping_interval_s", "Keepalive", default = 25, min = 0, max = 600, unit = "s", advanced = true)
            bool("insecure", "Accept any TLS certificate (wss)", default = false, advanced = true,
                help = "Never used for internet relays, which always validate certificates.")
        }
        override fun scan(ctx: HostContext, timeoutMs: Long): JSONArray = Nsd.discover(ctx, timeoutMs) { h, p, name ->
            JSONObject().put("url", "ws://${if (h.contains(':')) "[$h]" else h}:$p/ws").also { it.put("_name", name) }
        }
        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink) = WebSocketEndpoint(ctx, config, sink)
    }
}
