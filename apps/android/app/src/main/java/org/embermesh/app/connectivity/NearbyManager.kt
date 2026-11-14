package org.embermesh.app.connectivity

import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.Socket
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.CountDownLatch
import java.util.concurrent.ExecutorService
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference

/** DNS-SD advertise/browse (NsdManager on Android; a fake in JVM tests). */
interface NearbyPlatform {
    /** Advertises instance EmberMesh-<token> with the TXT of [NearbyRecord.txt] on [port]. */
    fun advertise(token: String, port: Int): AutoCloseable?

    /** Browses `_embermesh._tcp` and resolves what it finds; results go to [listener] (any thread). */
    fun browse(listener: Listener): AutoCloseable?

    interface Listener {
        /** A resolved service [key] (instance name) with its raw TXT attributes, addresses and port. */
        fun found(key: String, txt: Map<String, ByteArray?>?, addrs: List<InetAddress>, port: Int)
        fun lost(key: String)
    }
}

/** Turns one connected socket into a core link (LinkManager.createAuto). Throws when the core refuses. */
fun interface NearbyAttach {
    /**
     * [peerToken] is the dialled peer's token (null when accepting). [onClosed] must be called
     * exactly once when the link's endpoint closes. Returns the link id.
     */
    fun attach(role: String, peerToken: String?, socket: Socket, onClosed: () -> Unit): Int
}

/** Which addresses nearby may listen on, dial and accept from ([LocalNets] on a device). */
interface NearbyScope {
    fun listenAddresses(): List<InetAddress>
    fun onLink(a: InetAddress): Boolean
}

/**
 * Nearby devices on the local network (CONNECTIVITY §3, RPC_API §11 "Host side"): listens on TCP
 * (47475 if free, else any port), advertises `_embermesh._tcp` with the current token, browses,
 * dials per the tie-break rule (§3.3) with Happy Eyeballs over the resolved addresses, and turns
 * every connection into an `auto:"nearby"` core link. Enforces the host limits of §3.5. Nothing
 * happens before [start]: no listener, no advertisement, no browse.
 */
class NearbyManager(
    private val platform: NearbyPlatform,
    private val attach: NearbyAttach,
    private val io: ExecutorService,
    private val sched: ScheduledExecutorService,
    private val now: () -> Long = System::currentTimeMillis,
    private val connect: (InetSocketAddress, Int) -> Socket = { a, t -> Socket().also { s -> s.tcpNoDelay = true; s.keepAlive = true; s.connect(a, t) } },
    private val preferredPort: Int = 47475,
    /** Re-read at every start (network change): the local-network interfaces and their subnets. */
    private val scope: () -> NearbyScope = { LocalNets.fromSystem().asScope() },
    private val log: (String) -> Unit = {},
) {
    private class Peer(val key: String, val token: String, val addrs: List<InetAddress>, val port: Int, val seenAt: Long) {
        @Volatile var lastDialAt = 0L
    }
    private class Conn(val role: String, val token: String?, val ip: String) { @Volatile var linkId = 0 }

    val limits = NearbyLimits(now)
    private val peers = ConcurrentHashMap<String, Peer>()
    private val conns = ConcurrentHashMap.newKeySet<Conn>()
    private val lock = Any()
    @Volatile private var generation = 0
    @Volatile var token: String? = null; private set
    @Volatile private var servers: List<ServerSocket> = emptyList()
    @Volatile private var currentScope: NearbyScope? = null
    private var advert: AutoCloseable? = null
    private var browser: AutoCloseable? = null
    private var ticker: ScheduledFuture<*>? = null

    val running: Boolean get() = token != null
    val listenPort: Int get() = servers.firstOrNull()?.localPort ?: 0
    fun linkCount() = conns.size
    fun knownPeers(): Set<String> = peers.values.map { it.token }.toSet()

    /** Starts (or restarts with a new [token]): listener, advertisement, browse, the dial ticker. */
    fun start(token: String) = synchronized(lock) {
        require(Regex("^[0-9a-f]{8}$").matches(token)) { "bad token" }
        if (this.token == token && servers.isNotEmpty()) {
            // Already listening: only retry what failed (NSD refused before the permission was granted).
            if (advert == null) advert = try { platform.advertise(token, listenPort) } catch (t: Throwable) { log("advertise: $t"); null }
            if (browser == null) browser = try { platform.browse(listener(generation)) } catch (t: Throwable) { log("browse: $t"); null }
            return
        }
        stopLocked()
        val gen = ++generation
        val sc = scope()
        val ss = listen(sc.listenAddresses())
        if (ss.isEmpty()) { log("nearby: no local-network interface"); return }
        servers = ss
        currentScope = sc
        this.token = token
        for (x in ss) io.execute { try { acceptLoop(x, gen) } catch (t: Throwable) { log("nearby accept: $t") } }
        advert = try { platform.advertise(token, ss.first().localPort) } catch (t: Throwable) { log("advertise: $t"); null }
        browser = try { platform.browse(listener(gen)) } catch (t: Throwable) { log("browse: $t"); null }
        ticker = try { sched.scheduleWithFixedDelay({ tick() }, TICK_MS, TICK_MS, TimeUnit.MILLISECONDS) } catch (_: Exception) { null }
    }

    /** Stops listening, advertising and browsing. Existing links stay until the caller removes them. */
    fun stop() = synchronized(lock) { stopLocked() }

    private fun stopLocked() {
        generation++
        token = null
        ticker?.cancel(false); ticker = null
        try { advert?.close() } catch (_: Throwable) {}
        try { browser?.close() } catch (_: Throwable) {}
        advert = null; browser = null
        for (x in servers) try { x.close() } catch (_: Throwable) {}
        servers = emptyList()
        currentScope = null
        peers.clear()
    }

    /**
     * One listener per local-network address (never the wildcard address, so cellular and VPN
     * interfaces are not reachable), all on the same port: 47475 if free, else whatever the first
     * address got.
     */
    private fun listen(addrs: List<InetAddress>): List<ServerSocket> {
        val out = ArrayList<ServerSocket>()
        var port = 0
        for (a in addrs) {
            val tries = if (out.isEmpty()) listOf(preferredPort, 0) else listOf(port)
            for (p in tries) {
                try {
                    val ss = ServerSocket()
                    ss.reuseAddress = true
                    ss.bind(InetSocketAddress(a, p), 16)
                    out.add(ss); port = ss.localPort
                    break
                } catch (t: Throwable) { log("listen on ${a.hostAddress}:$p: $t") }
            }
        }
        return out
    }

    private fun acceptLoop(ss: ServerSocket, gen: Int) {
        while (generation == gen && !ss.isClosed) {
            val s = try { ss.accept() } catch (_: Throwable) { break }
            val remote = s.inetAddress
            val ip = ipKey(remote)
            val sc = currentScope
            if (generation != gen || remote == null || sc == null || !sc.onLink(remote) || !limits.acceptInbound(ip)) { closeQuietly(s); continue }
            try { s.tcpNoDelay = true; s.keepAlive = true } catch (_: Throwable) {}
            val c = Conn("accept", null, ip)
            if (!attachConn(c, s)) limits.linkClosed(ip)
        }
    }

    /** True when the link exists (its close later releases the limits); false: socket closed here. */
    private fun attachConn(c: Conn, s: Socket): Boolean {
        val released = AtomicBoolean(false)
        conns.add(c)
        return try {
            c.linkId = attach.attach(c.role, c.token, s) {
                if (released.compareAndSet(false, true)) { conns.remove(c); limits.linkClosed(c.ip); onLinkGone(c) }
            }
            true
        } catch (t: Throwable) {
            log("nearby ${c.role} refused: ${t.message}")
            released.set(true)
            conns.remove(c)
            closeQuietly(s)
            false
        }
    }

    private fun onLinkGone(c: Conn) {
        // Let discovery dial again soon (the ticker re-evaluates every peer).
        val t = c.token ?: return
        peers.values.firstOrNull { it.token == t }?.lastDialAt = 0L
    }

    private fun listener(gen: Int) = object : NearbyPlatform.Listener {
        override fun found(key: String, txt: Map<String, ByteArray?>?, addrs: List<InetAddress>, port: Int) {
            if (generation != gen) return
            val t = NearbyRecord.peerToken(txt) ?: return
            if (t == token || port !in 1..65535 || addrs.isEmpty() || key.length > 128) return
            if (peers.size >= MAX_PEERS && !peers.containsKey(key)) return
            val old = peers[key]
            val sc = currentScope ?: return
            // Only on-link / private addresses of our own subnets, at most 4 per instance (§3.4).
            val usable = NearbyRecord.order(addrs.filter { sc.onLink(it) }).take(4)
            if (usable.isEmpty()) return
            val p = Peer(key, t, usable, port, if (old?.token == t) old.seenAt else now())
            p.lastDialAt = old?.lastDialAt ?: 0L
            peers[key] = p
            evaluate(p)
        }
        override fun lost(key: String) { if (generation == gen) peers.remove(key) }
    }

    /** Re-evaluates every known peer (dial rule, one-sided-discovery fallback, redial after a drop). */
    fun tick() { for (p in peers.values) evaluate(p) }

    private fun evaluate(p: Peer) {
        val own = token ?: return
        if (p.token == own) return
        if (hasLinkTo(p)) return
        val t = now()
        if (t - p.seenAt < NearbyRecord.dialDelayMs(own, p.token)) {
            val wait = NearbyRecord.dialDelayMs(own, p.token) - (t - p.seenAt)
            try { sched.schedule({ evaluate(p) }, wait + 50, TimeUnit.MILLISECONDS) } catch (_: Exception) {}
            return
        }
        if (p.lastDialAt != 0L && t - p.lastDialAt < REDIAL_MS) return
        val firstIp = ipKey(p.addrs.first())
        if (!limits.startDial(p.token, firstIp)) return
        p.lastDialAt = t
        val gen = generation
        try {
            io.execute {
                try { dial(p, gen) } catch (t: Throwable) { log("nearby dial: $t"); limits.dialDone(p.token, null, false) }
            }
        } catch (_: Exception) { limits.dialDone(p.token, null, false) }
    }

    private fun hasLinkTo(p: Peer): Boolean {
        val ips = p.addrs.map { ipKey(it) }.toSet()
        return conns.any { (it.role == "dial" && it.token == p.token) || (it.role == "accept" && it.ip in ips) }
    }

    private fun dial(p: Peer, gen: Int) {
        val s = happyEyeballs(p.addrs, p.port)
        if (s == null || generation != gen || hasLinkTo(p)) {
            s?.let { closeQuietly(it) }
            limits.dialDone(p.token, null, false)
            return
        }
        val ip = ipKey(s.inetAddress)
        limits.dialDone(p.token, ip, true)
        if (!attachConn(Conn("dial", p.token, ip), s)) limits.linkClosed(ip)
    }

    /** RFC 8305-style: one attempt per address, [STAGGER_MS] apart, first to connect wins. */
    fun happyEyeballs(addrs: List<InetAddress>, port: Int): Socket? {
        if (addrs.isEmpty()) return null
        val winner = AtomicReference<Socket?>(null)
        val abandoned = AtomicBoolean(false)
        val left = AtomicInteger(addrs.size)
        val done = CountDownLatch(1)
        val pending = ArrayList<ScheduledFuture<*>>()
        for ((i, a) in addrs.withIndex()) {
            val task = Runnable {
                if (winner.get() != null || abandoned.get()) { if (left.decrementAndGet() == 0) done.countDown(); return@Runnable }
                try {
                    val s = connect(InetSocketAddress(a, port), CONNECT_TIMEOUT_MS)
                    if (abandoned.get() || !winner.compareAndSet(null, s)) closeQuietly(s) else done.countDown()
                } catch (_: Throwable) {
                } finally { if (left.decrementAndGet() == 0) done.countDown() }
            }
            try {
                pending.add(sched.schedule({ try { io.execute(task) } catch (_: Exception) { if (left.decrementAndGet() == 0) done.countDown() } },
                    i * STAGGER_MS, TimeUnit.MILLISECONDS))
            } catch (_: Exception) { if (left.decrementAndGet() == 0) done.countDown() }
        }
        try { done.await(CONNECT_TIMEOUT_MS + addrs.size * STAGGER_MS + 500, TimeUnit.MILLISECONDS) }
        catch (_: InterruptedException) { Thread.currentThread().interrupt() } // shutting down
        abandoned.set(true)
        for (f in pending) f.cancel(false)
        if (Thread.currentThread().isInterrupted) { winner.getAndSet(null)?.let { closeQuietly(it) }; return null }
        return winner.get()
    }

    private fun closeQuietly(s: Socket) { try { s.close() } catch (_: Throwable) {} }

    companion object {
        /** Limits key of an address: the IPv4 address or the IPv6 /64 (zone ignored). */
        fun ipKey(a: InetAddress?): String = LocalNets.sourceKey(a)

        /** The address as written in an `auto_key` ("<token>@<address>"): no zone id. */
        fun addressText(a: InetAddress?): String = a?.hostAddress?.substringBefore('%') ?: ""

        const val STAGGER_MS = 250L
        const val CONNECT_TIMEOUT_MS = 5_000
        const val TICK_MS = 5_000L
        const val REDIAL_MS = 15_000L
        const val MAX_PEERS = 64
    }
}
