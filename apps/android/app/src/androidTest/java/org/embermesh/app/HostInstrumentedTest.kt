package org.embermesh.app

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.endpoint.net.TcpEndpoint
import org.embermesh.app.endpoint.net.TcpServerEndpoint
import org.embermesh.app.host.EndpointSink
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.ByteArrayOutputStream
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
class HostInstrumentedTest {
    private val app get() = InstrumentationRegistry.getInstrumentation().targetContext

    @Test fun hostInfoAdvertisesEndpointKinds() {
        assertTrue(EmberRuntime.start(app))
        val info = EmberRuntime.router.handle("host.info", JSONObject())
        assertEquals("android", info.getString("platform"))
        assertEquals("native", info.getString("backend"))
        val kinds = info.getJSONArray("endpoint_kinds")
        val names = (0 until kinds.length()).map { kinds.getJSONObject(it).getString("kind") }
        for (k in listOf("ble_peer", "udp", "audio", "serial", "ble", "bt_spp", "tcp", "tcp_server", "websocket", "wifi_direct", "ws_server", "loopback")) {
            assertTrue("missing $k", k in names)
        }
        for (i in 0 until kinds.length()) {
            val k = kinds.getJSONObject(i)
            assertTrue(k.has("available")); assertTrue(k.has("config")); assertTrue(k.getJSONArray("link_types").length() > 0)
            if (!k.getBoolean("available")) assertTrue(k.getString("unavailable_reason").isNotEmpty())
        }
        assertTrue(info.getJSONArray("features").toString().contains("notify"))
        try { EmberRuntime.router.handle("host.nope", JSONObject()); throw AssertionError() } catch (e: HostError) { assertEquals("unsupported", e.code) }
    }

    @Test fun linkCreateWithLoopbackGoesThroughCore() {
        assertTrue(EmberRuntime.start(app))
        val p = JSONObject().put("name", "loop").put("endpoint", JSONObject().put("kind", "loopback").put("config", JSONObject()))
            .put("link", JSONObject().put("type", "raw").put("config", JSONObject()))
        try {
            val r = EmberRuntime.router.handle("host.link.create", p)
            val id = r.getJSONObject("link").getInt("id")
            assertTrue(EmberRuntime.links.active(id)!!.connected)
            EmberRuntime.links.onLinkTx(id, byteArrayOf(1, 2, 3)) // echoed into ember_link_rx
            Thread.sleep(100)
            assertTrue(EmberRuntime.links.active(id)!!.rxBytes >= 3)
            EmberRuntime.router.handle("host.link.remove", JSONObject().put("id", id))
        } finally {}
    }

    /** Real sockets on the device: tcp_server <-> tcp client byte pipes in both directions. */
    @Test fun tcpServerAndClientPipes() {
        val ctx = HostContext(app)
        class Sink(val name: String) : EndpointSink {
            override val linkId = 1
            val up = CountDownLatch(1)
            val buf = ByteArrayOutputStream()
            override fun onBytes(data: ByteArray, off: Int, len: Int) { synchronized(buf) { buf.write(data, off, len) } }
            override fun onAudio(samples: ShortArray, count: Int, sampleRate: Int) {}
            override fun pullAudio(out: ShortArray, max: Int, sampleRate: Int) = 0
            override fun onState(connected: Boolean, error: String?) { if (connected) up.countDown() }
            fun text() = synchronized(buf) { String(buf.toByteArray()) }
        }
        val ss = Sink("server"); val cs = Sink("client")
        val server = TcpServerEndpoint(ctx, JSONObject().put("port", 47999).put("advertise", false), ss)
        server.open()
        assertTrue(ss.up.await(5, TimeUnit.SECONDS))
        val client = TcpEndpoint(ctx, JSONObject().put("host", "127.0.0.1").put("port", 47999), cs)
        client.open()
        assertTrue(cs.up.await(5, TimeUnit.SECONDS))
        try {
            Thread.sleep(300) // let the server accept
            client.write("hello-server".toByteArray())
            server.write("hello-client".toByteArray())
            val deadline = System.currentTimeMillis() + 5000
            while ((ss.text() != "hello-server" || cs.text() != "hello-client") && System.currentTimeMillis() < deadline) Thread.sleep(20)
            assertEquals("hello-server", ss.text())
            assertEquals("hello-client", cs.text())
        } finally {
            client.close(); server.close(); ctx.shutdown()
        }
    }
}
