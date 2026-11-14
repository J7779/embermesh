package org.embermesh.app.connectivity

import org.embermesh.app.host.FakeCore
import org.embermesh.app.host.HostError
import org.json.JSONArray
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.Socket
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.Executor
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

/**
 * Nearby manager, relay manager and controller against fakes (no NsdManager, no core): real
 * loopback sockets, a fake DNS-SD platform, a controllable clock.
 */
class NearbyManagerTest {
    private val io = Executors.newCachedThreadPool()
    private val sched = Executors.newScheduledThreadPool(2)
    private val sync = Executor { it.run() }
    private val lo: InetAddress = InetAddress.getLoopbackAddress()
    /** Loopback stands in for the local network (LocalNets itself is tested in ConnectivityLogicTest). */
    private val loScope = { object : NearbyScope {
        override fun listenAddresses() = listOf(lo)
        override fun onLink(a: InetAddress) = a.isLoopbackAddress
    } }

    @After fun tearDown() { io.shutdownNow(); sched.shutdownNow() }

    class FakePlatform : NearbyPlatform {
        val adverts = CopyOnWriteArrayList<Pair<String, Int>>()
        var advertClosed = 0
        var browses = 0
        var browseClosed = 0
        var listener: NearbyPlatform.Listener? = null
        override fun advertise(token: String, port: Int): AutoCloseable { adverts.add(token to port); return AutoCloseable { advertClosed++ } }
        override fun browse(listener: NearbyPlatform.Listener): AutoCloseable { browses++; this.listener = listener; return AutoCloseable { browseClosed++ } }
    }

    class Attached(val role: String, val key: String?, val socket: Socket, val onClosed: () -> Unit)

    inner class Node(token: String, var refuse: Boolean = false, clock: () -> Long = System::currentTimeMillis) {
        val platform = FakePlatform()
        val attached = CopyOnWriteArrayList<Attached>()
        val dials = AtomicInteger()
        val ids = AtomicInteger(100)
        val mgr = NearbyManager(platform, { role, key, s, onClosed ->
            if (refuse) throw HostError(HostError.BUSY, "busy")
            attached.add(Attached(role, key, s, onClosed)); ids.incrementAndGet()
        }, io, sched, clock, connect = { a, t -> dials.incrementAndGet(); Socket().apply { connect(a, t) } }, preferredPort = 0, scope = loScope)
        init { mgr.start(token) }
        fun record(): Map<String, ByteArray?> = NearbyRecord.txt(mgr.token!!).mapValues { it.value.toByteArray() }
    }

    private fun waitFor(ms: Long = 5_000, what: String, cond: () -> Boolean) {
        val end = System.currentTimeMillis() + ms
        while (System.currentTimeMillis() < end) { if (cond()) return; Thread.sleep(20) }
        throw AssertionError("timed out: $what")
    }

    @Test fun advertisesTheTokenAndBrowsesOnStartOnly() {
        val n = Node("0a1b2c3d")
        assertEquals(listOf("0a1b2c3d" to n.mgr.listenPort), n.platform.adverts.toList())
        assertTrue(n.mgr.listenPort > 0)
        assertEquals(1, n.platform.browses)
        n.mgr.stop()
        assertEquals(1, n.platform.advertClosed); assertEquals(1, n.platform.browseClosed)
        assertFalse(n.mgr.running)
    }

    @Test fun nsdRefusedAtFirstIsRetriedWithoutANewListener() {
        var allowed = false
        val adverts = ArrayList<String>()
        var browses = 0
        val p = object : NearbyPlatform {
            override fun advertise(token: String, port: Int): AutoCloseable? = if (allowed) { adverts.add(token); AutoCloseable {} } else null
            override fun browse(listener: NearbyPlatform.Listener): AutoCloseable? = if (allowed) { browses++; AutoCloseable {} } else null
        }
        val m = NearbyManager(p, { _, _, _, _ -> 1 }, io, sched, preferredPort = 0, scope = loScope)
        m.start("0a1b2c3d")
        val port = m.listenPort
        assertTrue(adverts.isEmpty())
        allowed = true // the user granted the permission
        m.start("0a1b2c3d")
        assertEquals(port, m.listenPort)
        assertEquals(listOf("0a1b2c3d"), adverts); assertEquals(1, browses)
        m.start("0a1b2c3d")
        assertEquals("no duplicate registration", 1, adverts.size)
        m.stop()
    }

    @Test fun lowerTokenDialsHigherAcceptsOnlyOneLink() {
        val a = Node("00000001")
        val b = Node("ffffffff")
        // Both discover each other.
        a.platform.listener!!.found("EmberMesh-ffffffff", b.record(), listOf(lo), b.mgr.listenPort)
        b.platform.listener!!.found("EmberMesh-00000001", a.record(), listOf(lo), a.mgr.listenPort)
        waitFor(what = "dial + accept") { a.attached.size == 1 && b.attached.size == 1 }
        assertEquals("dial", a.attached[0].role); assertEquals("ffffffff", a.attached[0].key)
        assertEquals("accept", b.attached[0].role); assertNull(b.attached[0].key)
        b.mgr.tick(); a.mgr.tick()
        Thread.sleep(300)
        assertEquals("the higher token never dials while the peer's connection is up", 0, b.dials.get())
        assertEquals(1, a.dials.get())
        // Bytes flow both ways over the sockets handed to the links.
        a.attached[0].socket.getOutputStream().write(byteArrayOf(1, 2, 3))
        val buf = ByteArray(3); b.attached[0].socket.getInputStream().read(buf)
        assertEquals(listOf<Byte>(1, 2, 3), buf.toList())
        a.mgr.stop(); b.mgr.stop()
    }

    @Test fun oneSidedDiscoveryHigherTokenDialsAfterEightSeconds() {
        var t = 1_000_000L
        val a = Node("00000001")
        val b = Node("ffffffff", clock = { t })
        b.platform.listener!!.found("EmberMesh-00000001", a.record(), listOf(lo), a.mgr.listenPort)
        b.mgr.tick()
        Thread.sleep(200)
        assertEquals("waits for the lower token to dial", 0, b.dials.get())
        t += 7_900; b.mgr.tick(); Thread.sleep(100)
        assertEquals(0, b.dials.get())
        t += 200; b.mgr.tick()
        waitFor(what = "fallback dial") { b.attached.size == 1 && a.attached.size == 1 }
        assertEquals("dial", b.attached[0].role); assertEquals("00000001", b.attached[0].key)
        a.mgr.stop(); b.mgr.stop()
    }

    @Test fun hostileOrOwnRecordsAreNeverDialled() {
        val a = Node("00000001")
        val b = Node("ffffffff")
        val l = a.platform.listener!!
        l.found("EmberMesh-ffffffff", mapOf("proto" to "tcp".toByteArray()), listOf(lo), b.mgr.listenPort)
        l.found("x", mapOf("ember" to "1".toByteArray(), "v" to "1".toByteArray(), "link" to "stream".toByteArray(), "t" to "FFFFFFFF".toByteArray()), listOf(lo), b.mgr.listenPort)
        l.found("EmberMesh-ffffffff", b.record(), listOf(lo), 0)
        l.found("EmberMesh-ffffffff", b.record(), emptyList(), b.mgr.listenPort)
        l.found("self", a.record(), listOf(lo), a.mgr.listenPort)
        l.found("y".repeat(500), b.record(), listOf(lo), b.mgr.listenPort)
        a.mgr.tick(); Thread.sleep(300)
        assertEquals(0, a.dials.get())
        assertTrue(a.attached.isEmpty())
        a.mgr.stop(); b.mgr.stop()
    }

    @Test fun refusedConnectionIsClosedAndLimitsReleased() {
        val a = Node("00000001")
        val b = Node("ffffffff", refuse = true)
        a.platform.listener!!.found("EmberMesh-ffffffff", b.record(), listOf(lo), b.mgr.listenPort)
        waitFor(what = "dialled") { a.attached.size == 1 }
        // b refused (core busy): its side closes, so a's socket sees EOF.
        assertEquals(-1, a.attached[0].socket.getInputStream().read())
        waitFor(what = "b released its limits") { b.mgr.limits.linkCount() == 0 }
        a.attached[0].onClosed()
        assertEquals(0, a.mgr.limits.linkCount())
        a.mgr.stop(); b.mgr.stop()
    }

    @Test fun inboundLimitPerAddress() {
        val b = Node("ffffffff")
        val socks = (0 until 3).map { Socket().apply { connect(InetSocketAddress(lo, b.mgr.listenPort), 2000) } }
        waitFor(what = "two accepted") { b.attached.size == 2 }
        Thread.sleep(200)
        assertEquals("at most 2 connections per remote address", 2, b.attached.size)
        assertEquals(-1, socks[2].getInputStream().read())
        socks.forEach { it.close() }
        b.mgr.stop()
    }

    @Test fun happyEyeballsFirstAddressThatConnectsWins() {
        val b = Node("ffffffff")
        val a = Node("00000001")
        // An unroutable-ish first address (TEST-NET-1) then loopback 250 ms later.
        val s = a.mgr.happyEyeballs(listOf(InetAddress.getByName("192.0.2.1"), lo), b.mgr.listenPort)
        assertNotNull(s); assertEquals(lo, s!!.inetAddress)
        s.close(); a.mgr.stop(); b.mgr.stop()
    }

    // --- controller: consent gating -------------------------------------------------------------

    private fun connectivity(nearby: Boolean, internet: Boolean, token: String = "0a1b2c3d", ready: Boolean = true) = JSONObject()
        .put("ready", ready).put("setup_done", ready)
        .put("nearby", JSONObject().put("enabled", nearby).put("active", nearby && ready).put("token", token).put("links", 0))
        .put("internet", JSONObject().put("enabled", internet).put("active", internet && ready).put("notice", "…")
            .put("relays", JSONArray().put(JSONObject().put("url", "wss://broker.example:8084/mqtt").put("enabled", true))))

    class FakeRelayLinks : RelayLinks {
        val links = LinkedHashMap<String, Int>()
        val created = CopyOnWriteArrayList<JSONObject>()
        val removed = CopyOnWriteArrayList<Int>()
        var fail: HostError? = null
        override fun current() = LinkedHashMap(links)
        override fun create(params: JSONObject) { fail?.let { throw it }; created.add(params); links[params.getString("auto_key")] = 100 + created.size }
        override fun remove(id: Int) { removed.add(id); links.values.remove(id) }
    }

    private inner class Harness(val core: FakeCore = FakeCore()) {
        val platform = FakePlatform()
        val sockets = AtomicInteger()
        val nearby = NearbyManager(platform, { _, _, _, _ -> 1 }, io, sched, preferredPort = 0, scope = loScope)
        val relays = FakeRelayLinks()
        var closedNearby = 0
        val extras = CopyOnWriteArrayList<String?>()
        val ctl = ConnectivityController(core, nearby, RelayManager(relays), { closedNearby++ }, sync, { extras.add(it) })
    }

    @Test fun nothingBeforeConnectivityGetOrWithoutCore() {
        val h = Harness()
        h.ctl.setForeground(true)
        h.ctl.refresh() // core without connectivity.get (not_implemented)
        assertEquals(0, h.platform.adverts.size); assertEquals(0, h.platform.browses)
        assertEquals(0, h.nearby.listenPort); assertTrue(h.relays.created.isEmpty())
    }

    @Test fun noAdvertBrowseOrSocketWhileSwitchesOff() {
        val h = Harness()
        h.core.connectivity = connectivity(nearby = false, internet = false)
        h.ctl.setForeground(true)
        h.ctl.refresh()
        assertEquals(0, h.platform.adverts.size); assertEquals(0, h.platform.browses)
        assertEquals("no listening socket", 0, h.nearby.listenPort)
        assertTrue("no relay before consent", h.relays.created.isEmpty())
        assertEquals(listOf<String?>(null), h.extras.toList())
    }

    @Test fun nothingBeforeReadyAndEverythingStopsWhenReadyGoesFalse() {
        val h = Harness()
        h.ctl.setForeground(true)
        h.ctl.onChanged(connectivity(nearby = true, internet = true, ready = false)) // setup not done / locked
        assertEquals(0, h.platform.adverts.size); assertEquals(0, h.platform.browses)
        assertEquals(0, h.nearby.listenPort); assertTrue(h.relays.created.isEmpty())
        h.ctl.onChanged(connectivity(nearby = true, internet = true))
        assertTrue(h.nearby.running); assertEquals(1, h.relays.created.size)
        h.ctl.onChanged(connectivity(nearby = true, internet = true, ready = false)) // security.lock
        assertFalse(h.nearby.running); assertEquals(1, h.platform.advertClosed); assertEquals(1, h.platform.browseClosed)
        assertEquals(1, h.relays.removed.size); assertTrue(h.closedNearby > 0)
    }

    @Test fun networkChangeAsksTheCoreForANewTokenAndReadvertises() {
        val core = object : org.embermesh.app.core.CoreApi by FakeCore() {
            override fun call(method: String, paramsJson: String) =
                if (method == "connectivity.network_changed") JSONObject().put("ok", true).put("result", JSONObject().put("token", "beefcafe")).toString()
                else JSONObject().put("ok", false).put("error", JSONObject().put("code", "not_implemented")).toString()
        }
        val platform = FakePlatform()
        val nearby = NearbyManager(platform, { _, _, _, _ -> 1 }, io, sched, preferredPort = 0, scope = loScope)
        val ctl = ConnectivityController(core, nearby, RelayManager(FakeRelayLinks()), {}, sync)
        ctl.setForeground(true)
        ctl.onChanged(connectivity(nearby = true, internet = false))
        assertEquals("0a1b2c3d", platform.adverts.last().first)
        ctl.networkChanged()
        assertEquals("beefcafe", platform.adverts.last().first); assertEquals("beefcafe", nearby.token)
        nearby.stop()
    }

    @Test fun onlyInTheForeground() {
        val h = Harness()
        h.core.connectivity = connectivity(nearby = true, internet = true)
        h.ctl.refresh()
        assertEquals(0, h.platform.adverts.size); assertTrue(h.relays.created.isEmpty())
        h.ctl.setForeground(true)
        assertEquals(listOf("0a1b2c3d" to h.nearby.listenPort), h.platform.adverts.toList())
        assertEquals(1, h.relays.created.size)
        h.ctl.setForeground(false)
        assertFalse(h.nearby.running); assertEquals(1, h.platform.advertClosed)
        assertEquals(1, h.relays.removed.size)
    }

    @Test fun followsConnectivityChanged() {
        val h = Harness()
        h.ctl.setForeground(true)
        h.ctl.onChanged(connectivity(nearby = true, internet = false))
        assertTrue(h.nearby.running); assertTrue(h.relays.created.isEmpty())
        assertEquals("0a1b2c3d", h.extras.last())
        // consent given → relay link created, with the contract's params
        h.ctl.onChanged(connectivity(nearby = true, internet = true))
        assertEquals(1, h.relays.created.size)
        assertEquals("relay", h.relays.created[0].getString("auto"))
        // repeated event → no second link
        h.ctl.onChanged(connectivity(nearby = true, internet = true))
        assertEquals(1, h.relays.created.size)
        // new token (node restarted) → re-advertised with it
        h.ctl.onChanged(connectivity(nearby = true, internet = true, token = "deadbeef"))
        assertEquals("deadbeef", h.platform.adverts.last().first)
        // consent withdrawn, nearby off → relay removed, nearby stopped and its links closed
        h.ctl.onChanged(connectivity(nearby = false, internet = false))
        assertEquals(1, h.relays.removed.size)
        assertFalse(h.nearby.running); assertTrue(h.closedNearby > 0)
        assertNull(h.extras.last())
    }

    @Test fun relayFailureIsRetriedAfterACooldownOnly() {
        var t = 0L
        val links = FakeRelayLinks().apply { fail = HostError(HostError.DENIED, "denied") }
        val rm = RelayManager(links, { t })
        val s = ConnectivityState.parse(connectivity(nearby = false, internet = true))
        rm.apply(s, true); rm.apply(s, true)
        assertTrue(links.created.isEmpty())
        links.fail = null
        rm.apply(s, true)
        assertTrue("cooldown", links.created.isEmpty())
        t += RelayManager.RETRY_AFTER_FAILURE_MS
        rm.apply(s, true)
        assertEquals(1, links.created.size)
    }

    @Test fun waitIsBounded() {
        // Sanity: the manager's executors are not blocked after the tests above.
        assertTrue(sched.schedule<Int>({ 1 }, 1, TimeUnit.MILLISECONDS).get(1, TimeUnit.SECONDS) == 1)
    }
}
