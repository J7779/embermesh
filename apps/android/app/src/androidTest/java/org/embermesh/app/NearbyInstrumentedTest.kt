package org.embermesh.app

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.connectivity.NearbyManager
import org.embermesh.app.connectivity.NearbyPlatform
import org.embermesh.app.connectivity.NearbyRecord
import org.embermesh.app.endpoint.net.NsdNearbyPlatform
import org.embermesh.app.host.HostContext
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.net.InetAddress
import java.net.Socket
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/**
 * Host side of RPC_API §11 on a real device: host.info, NsdManager advertising exactly the
 * CONNECTIVITY §3.2 record, two nearby managers finding each other through NSD and connecting.
 */
@RunWith(AndroidJUnit4::class)
class NearbyInstrumentedTest {
    private val app get() = InstrumentationRegistry.getInstrumentation().targetContext

    /** What the user grants when turning Nearby devices on (Android 17: local network). */
    private fun grantLocalNetwork() {
        for (p in org.embermesh.app.util.Perms.localNetwork())
            InstrumentationRegistry.getInstrumentation().uiAutomation.grantRuntimePermission(app.packageName, p)
    }

    @Test fun hostInfoReportsNearbyRelayAndTls() {
        assertTrue(EmberRuntime.start(app))
        val info = EmberRuntime.router.handle("host.info", JSONObject())
        val f = info.getJSONArray("features").let { a -> (0 until a.length()).map { a.getString(it) } }
        assertTrue(f.toString(), "nearby" in f && "relay" in f)
        assertTrue(info.getBoolean("websocket_tls"))
    }

    /** Before the core reports the switches on (or without connectivity.get at all) nothing runs. */
    @Test fun runtimeFollowsTheCoreSwitches() {
        assertTrue(EmberRuntime.start(app))
        val r = JSONObject(EmberRuntime.core.call("connectivity.get", "{}"))
        Thread.sleep(500)
        val nearby = EmberRuntime.nearby!!
        val relays = EmberRuntime.links.autoLinks("relay")
        if (!r.optBoolean("ok")) {
            assertFalse("no connectivity.get in this core: nothing may listen or advertise", nearby.running)
            assertTrue(relays.isEmpty())
        } else {
            val c = r.getJSONObject("result")
            if (!c.getJSONObject("internet").optBoolean("active")) assertTrue("no relay link before consent", relays.isEmpty())
            if (!c.getJSONObject("nearby").optBoolean("active")) assertFalse("nothing before setup is done and the node unlocked", nearby.running)
            if (nearby.running) assertEquals(c.getJSONObject("nearby").getString("token"), nearby.token)
        }
    }

    /** Permissions are asked when the page turns Nearby devices on — not for other settings. */
    @Test fun turningNearbyOnAsksForItsPermissions() {
        assertTrue(EmberRuntime.start(app))
        val asked = CopyOnWriteArrayList<List<String>>()
        val prev = EmberRuntime.hostContext.ui
        EmberRuntime.hostContext.ui = object : org.embermesh.app.host.UiHost {
            override fun requestPermissions(perms: Array<String>, cb: (Boolean) -> Unit) { asked.add(perms.toList()); cb(false) }
            override fun startForResult(intent: android.content.Intent, cb: (Int, android.content.Intent?) -> Unit) {}
            override fun setKeepScreenOn(on: Boolean) {}
        }
        try {
            val d = EmberRuntime.newDispatcher { }
            val missing = org.embermesh.app.util.Perms.missing(EmberRuntime.hostContext, EmberRuntime.nearbyPermissions())
            d.dispatch("connectivity.set", JSONObject().put("internet", false))
            d.dispatch("settings.get", JSONObject())
            assertTrue("nothing asked for other calls: $asked", asked.isEmpty())
            val r = JSONObject(d.dispatch("connectivity.set", JSONObject().put("nearby", true)))
            assertTrue("the setting goes through even when refused: $r", r.getBoolean("ok"))
            if (missing.isEmpty()) assertTrue(asked.isEmpty()) else assertEquals(listOf(missing), asked.toList())
        } finally { EmberRuntime.hostContext.ui = prev }
    }

    @Test fun nsdAdvertisesExactlyTheContractRecord() {
        grantLocalNetwork()
        val ctx = HostContext(app)
        try {
            val token = "%08x".format(java.security.SecureRandom().nextInt())
            val seen = CountDownLatch(1)
            var txt: Map<String, ByteArray?>? = null
            var addrs: List<InetAddress> = emptyList()
            var port = 0
            val ad = NsdNearbyPlatform(ctx).advertise(token, 47123)!!
            val br = NsdNearbyPlatform(ctx).browse(object : NearbyPlatform.Listener {
                override fun found(key: String, t: Map<String, ByteArray?>?, a: List<InetAddress>, p: Int) {
                    if (key.startsWith(NearbyRecord.instanceName(token))) { txt = t; addrs = a; port = p; seen.countDown() }
                }
                override fun lost(key: String) {}
            })!!
            try {
                assertTrue("own advertisement found by browsing", seen.await(30, TimeUnit.SECONDS))
                assertEquals(setOf("ember", "v", "link", "t"), txt!!.keys)
                assertEquals(token, NearbyRecord.peerToken(txt))
                assertEquals(47123, port)
                assertTrue(addrs.isNotEmpty())
            } finally { br.close(); ad.close() }
        } finally { ctx.shutdown() }
    }

    @Test fun twoNodesFindEachOtherOverNsdAndConnect() {
        grantLocalNetwork()
        val ctx = HostContext(app)
        class Got(val role: String, val key: String?, val s: Socket)
        val got = ConcurrentHashMap<String, CopyOnWriteArrayList<Got>>()
        fun node(token: String) = NearbyManager(NsdNearbyPlatform(ctx), { role, key, s, _ ->
            got.getOrPut(token) { CopyOnWriteArrayList() }.add(Got(role, key, s)); 1
        }, ctx.io, ctx.scheduler, preferredPort = 0)
        val lowTok = "0" + "%07x".format(java.security.SecureRandom().nextInt(0x0fffffff))
        val highTok = "f" + "%07x".format(java.security.SecureRandom().nextInt(0x0fffffff))
        val low = node(lowTok); val high = node(highTok)
        try {
            low.start(lowTok); high.start(highTok)
            val end = System.currentTimeMillis() + 40_000
            while (System.currentTimeMillis() < end && (got[lowTok]?.size ?: 0) < 1) Thread.sleep(100)
            val l = got[lowTok].orEmpty(); val h = got[highTok].orEmpty()
            assertTrue("low token dialled: $l", l.any { it.role == "dial" && it.key == highTok })
            Thread.sleep(1000)
            assertTrue("high token accepted", got[highTok].orEmpty().any { it.role == "accept" })
            assertFalse("high token did not dial while the link is up", got[highTok].orEmpty().any { it.role == "dial" })
            // bytes both ways
            val dial = l.first { it.role == "dial" }.s
            val acc = got[highTok]!!.first { it.role == "accept" }.s
            dial.getOutputStream().write("ping".toByteArray()); val b = ByteArray(4); acc.getInputStream().read(b)
            assertEquals("ping", String(b))
            acc.getOutputStream().write("pong".toByteArray()); dial.getInputStream().read(b)
            assertEquals("pong", String(b))
            assertTrue(h.isEmpty() || h.none { it.role == "dial" })
        } finally {
            low.stop(); high.stop()
            got.values.flatten().forEach { try { it.s.close() } catch (_: Throwable) {} }
            ctx.shutdown()
        }
    }
}
