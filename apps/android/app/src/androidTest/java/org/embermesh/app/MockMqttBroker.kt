package org.embermesh.app

import org.embermesh.app.endpoint.net.WsCodec
import java.io.BufferedInputStream
import java.io.ByteArrayOutputStream
import java.io.OutputStream
import java.net.InetAddress
import java.net.ServerSocket
import java.net.Socket
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.atomic.AtomicInteger
import kotlin.concurrent.thread

/**
 * Test double of a public relay (CONNECTIVITY §4): MQTT 3.1.1, QoS 0, over WebSocket with the
 * `mqtt` subprotocol, on 127.0.0.1. Exact-match topics; it forwards PUBLISH to every subscriber.
 * Counts connections so tests can assert that nothing connected before consent.
 */
class MockMqttBroker : AutoCloseable {
    /** A LAN address of this device: the core allows plain ws:// relays only on LAN/link-local/.local. */
    private val addr: InetAddress = java.net.NetworkInterface.getNetworkInterfaces().toList()
        .filter { it.isUp && !it.isLoopback }.flatMap { it.inetAddresses.toList() }
        .first { it is java.net.Inet4Address && it.isSiteLocalAddress }
    private val server = ServerSocket(0, 16, addr)
    val port: Int get() = server.localPort
    val url: String get() = "ws://${addr.hostAddress}:$port/mqtt"
    val connections = AtomicInteger()
    val publishes = AtomicInteger()
    val offeredProtocols = CopyOnWriteArrayList<String>()
    private val clients = CopyOnWriteArrayList<Client>()
    @Volatile private var closed = false

    private inner class Client(val s: Socket) {
        val out: OutputStream = s.getOutputStream()
        val subs = java.util.concurrent.ConcurrentHashMap.newKeySet<String>()
        @Synchronized fun send(pkt: ByteArray) { out.write(WsCodec.encode(WsCodec.OP_BINARY, pkt, null)); out.flush() }
    }

    init {
        thread(isDaemon = true, name = "mock-mqtt") {
            while (!closed) {
                val s = try { server.accept() } catch (_: Throwable) { break }
                connections.incrementAndGet()
                thread(isDaemon = true) { serve(s) }
            }
        }
    }

    private fun serve(s: Socket) {
        val c = Client(s)
        try {
            val inp = BufferedInputStream(s.getInputStream())
            val head = StringBuilder()
            while (!head.endsWith("\r\n\r\n")) { val b = inp.read(); if (b < 0) return; head.append(b.toChar()) }
            val lines = head.lines()
            val key = lines.first { it.startsWith("Sec-WebSocket-Key:", true) }.substringAfter(':').trim()
            lines.firstOrNull { it.startsWith("Sec-WebSocket-Protocol:", true) }?.let { offeredProtocols.add(it.substringAfter(':').trim()) }
            c.out.write(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" +
                "Sec-WebSocket-Accept: ${WsCodec.acceptKey(key)}\r\nSec-WebSocket-Protocol: mqtt\r\n\r\n").toByteArray())
            c.out.flush()
            clients.add(c)
            val buf = ByteArrayOutputStream()
            while (!closed) {
                val f = WsCodec.readFrame(inp, 1 shl 20)
                when (f.opcode) {
                    WsCodec.OP_CLOSE -> return
                    WsCodec.OP_PING -> synchronized(c) { c.out.write(WsCodec.encode(WsCodec.OP_PONG, f.payload, null)) }
                    WsCodec.OP_BINARY, WsCodec.OP_CONT -> { buf.write(f.payload); drain(c, buf) }
                }
            }
        } catch (_: Throwable) {
        } finally {
            clients.remove(c)
            try { s.close() } catch (_: Throwable) {}
        }
    }

    /** Parses every complete MQTT packet in [buf] (a WebSocket message may hold several or part of one). */
    private fun drain(c: Client, buf: ByteArrayOutputStream) {
        var b = buf.toByteArray()
        while (true) {
            if (b.size < 2) break
            var len = 0; var mul = 1; var i = 1
            while (true) {
                if (i >= b.size) return keep(buf, b)
                val d = b[i].toInt() and 0xff
                len += (d and 0x7f) * mul; mul *= 128; i++
                if (d and 0x80 == 0) break
                if (i > 4) throw java.io.IOException("bad length")
            }
            if (b.size < i + len) return keep(buf, b)
            val pkt = b.copyOfRange(0, i + len)
            handle(c, pkt[0].toInt() and 0xff, pkt.copyOfRange(i, i + len), pkt)
            b = b.copyOfRange(i + len, b.size)
        }
        keep(buf, b)
    }

    private fun keep(buf: ByteArrayOutputStream, rest: ByteArray) { buf.reset(); buf.write(rest) }

    private fun str(p: ByteArray, at: Int): Pair<String, Int> {
        val n = ((p[at].toInt() and 0xff) shl 8) or (p[at + 1].toInt() and 0xff)
        return String(p, at + 2, n, Charsets.UTF_8) to at + 2 + n
    }

    private fun handle(c: Client, h: Int, body: ByteArray, raw: ByteArray) {
        when (h shr 4) {
            1 -> c.send(byteArrayOf(0x20, 2, 0, 0)) // CONNECT -> CONNACK accepted
            8 -> { // SUBSCRIBE
                var at = 2
                val granted = ByteArrayOutputStream()
                while (at < body.size) { val (t, nx) = str(body, at); c.subs.add(t); at = nx + 1; granted.write(0) }
                val g = granted.toByteArray()
                c.send(byteArrayOf(0x90.toByte(), (2 + g.size).toByte(), body[0], body[1]) + g)
            }
            10 -> { // UNSUBSCRIBE
                var at = 2
                while (at < body.size) { val (t, nx) = str(body, at); c.subs.remove(t); at = nx }
                c.send(byteArrayOf(0xB0.toByte(), 2, body[0], body[1]))
            }
            3 -> { // PUBLISH QoS 0
                publishes.incrementAndGet()
                val (topic, _) = str(body, 0)
                for (o in clients) if (topic in o.subs) try { o.send(raw) } catch (_: Throwable) {}
            }
            12 -> c.send(byteArrayOf(0xD0.toByte(), 0)) // PINGREQ -> PINGRESP
            14 -> throw java.io.IOException("disconnect")
        }
    }

    override fun close() {
        closed = true
        try { server.close() } catch (_: Throwable) {}
        for (c in clients) try { c.s.close() } catch (_: Throwable) {}
    }
}
