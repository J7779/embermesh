package org.embermesh.app.connectivity

import org.embermesh.app.endpoint.net.WebSocketEndpoint
import org.embermesh.app.endpoint.net.WsCodec
import org.embermesh.app.host.EndpointSink
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.json.JSONArray
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import java.net.InetAddress
import java.net.ServerSocket
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import kotlin.concurrent.thread

/** The websocket endpoint as relay links use it: subprotocol negotiation, TLS required unless allowed. */
class RelayWebSocketTest {
    private val ctx = HostContext(null)
    @After fun tearDown() = ctx.shutdown()

    class Sink : EndpointSink {
        override val linkId = 1
        val up = CountDownLatch(1)
        val down = CountDownLatch(1)
        @Volatile var error: String? = null
        override fun onBytes(data: ByteArray, off: Int, len: Int) {}
        override fun onAudio(samples: ShortArray, count: Int, sampleRate: Int) {}
        override fun pullAudio(out: ShortArray, max: Int, sampleRate: Int) = 0
        override fun onState(connected: Boolean, error: String?) { if (connected) up.countDown() else { this.error = error; down.countDown() } }
    }

    /** A one-shot server: records the request head, answers 101 with [protocol] (or none). */
    private fun server(protocol: String?): Pair<ServerSocket, () -> String> {
        val ss = ServerSocket(0, 1, InetAddress.getLoopbackAddress())
        var head = ""
        val done = CountDownLatch(1)
        thread(isDaemon = true) {
            try {
                val s = ss.accept()
                val inp = s.getInputStream()
                val sb = StringBuilder()
                while (!sb.endsWith("\r\n\r\n")) { val c = inp.read(); if (c < 0) break; sb.append(c.toChar()) }
                head = sb.toString()
                done.countDown()
                val key = head.lines().first { it.startsWith("Sec-WebSocket-Key:", true) }.substringAfter(':').trim()
                val resp = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${WsCodec.acceptKey(key)}\r\n" +
                    (if (protocol != null) "Sec-WebSocket-Protocol: $protocol\r\n" else "") + "\r\n"
                s.getOutputStream().write(resp.toByteArray())
                Thread.sleep(2000)
                s.close()
            } catch (_: Throwable) { done.countDown() }
        }
        return ss to { done.await(5, TimeUnit.SECONDS); head }
    }

    private fun relayConfig(url: String) = JSONObject().put("url", url).put("protocols", JSONArray().put("mqtt")).put("ping_interval_s", 0)

    private fun refused(cfg: JSONObject, why: String) {
        try { WebSocketEndpoint(ctx, cfg, Sink()).open(); fail(why) } catch (e: HostError) { assertEquals(why, HostError.BAD_PARAMS, e.code) }
    }

    @Test fun relaySocketsRefusePlaintextAndInsecure() {
        refused(relayConfig("ws://192.168.1.5:1/mqtt"), "ws:// without allow_plaintext")
        refused(relayConfig("ws://broker.example.com/mqtt").put("allow_plaintext", true), "ws:// to an internet host")
        refused(relayConfig("ws://127.0.0.1:1/mqtt").put("allow_plaintext", true), "ws:// to loopback")
        refused(relayConfig("wss://broker.example.com/mqtt").put("insecure", true), "insecure on a relay socket")
    }

    @Test fun sendsTheSubprotocolNoUserAgentAndRequiresItBack() {
        val (ss, head) = server("chat")
        val sink = Sink()
        val cfg = JSONObject().put("url", "ws://127.0.0.1:${ss.localPort}/x").put("protocols", JSONArray().put("chat")).put("ping_interval_s", 0)
        val ep = WebSocketEndpoint(ctx, cfg, sink)
        ep.open()
        assertTrue(head().contains("Sec-WebSocket-Protocol: chat\r\n"))
        assertFalse("no User-Agent", head().contains("User-Agent", ignoreCase = true))
        assertFalse("nothing naming the app", head().contains("EmberMesh", ignoreCase = true))
        assertTrue("connected", sink.up.await(5, TimeUnit.SECONDS))
        ep.close(); ss.close()
    }

    @Test fun serverThatIgnoresTheSubprotocolIsRejected() {
        val (ss, _) = server(null)
        val sink = Sink()
        WebSocketEndpoint(ctx, JSONObject().put("url", "ws://127.0.0.1:${ss.localPort}/x").put("protocols", JSONArray().put("chat")).put("ping_interval_s", 0), sink).open()
        assertTrue(sink.down.await(5, TimeUnit.SECONDS))
        assertEquals(1L, sink.up.count)
        assertTrue(sink.error!!, sink.error!!.contains("subprotocol"))
        ss.close()
    }
}
