package org.embermesh.app.connectivity

import org.json.JSONObject
import java.net.Inet4Address
import java.net.Inet6Address
import java.net.InetAddress

/** The parts of the core's `Connectivity` (RPC_API §11, revision 2) the host acts on. Pure. */
data class ConnectivityState(
    /** setup done AND unlocked: nothing may start before this (CONNECTIVITY §3.8). */
    val ready: Boolean,
    /** `nearby.active` (= enabled && ready) with a valid token. */
    val nearbyActive: Boolean,
    val nearbyEnabled: Boolean,
    val token: String?,
    /** `internet.active` (= enabled && ready, i.e. consent given and unlocked). */
    val internetActive: Boolean,
    val relays: List<RelaySpec>,
) {
    data class RelaySpec(val url: String, val allowPlaintext: Boolean)

    companion object {
        private val TOKEN = Regex("^[0-9a-f]{8}$")
        val OFF = ConnectivityState(false, false, false, null, false, emptyList())

        /** Hostile-input tolerant: anything malformed or missing means "off" for that part. */
        fun parse(c: JSONObject?): ConnectivityState {
            if (c == null) return OFF
            val ready = c.optBoolean("ready", false)
            val n = c.optJSONObject("nearby")
            val token = n?.optString("token", "")?.takeIf { TOKEN.matches(it) }
            val nearbyActive = ready && n?.optBoolean("active", false) == true && token != null
            val i = c.optJSONObject("internet")
            val internet = ready && i?.optBoolean("active", false) == true
            val relays = ArrayList<RelaySpec>()
            val arr = i?.optJSONArray("relays")
            if (internet && arr != null) for (k in 0 until minOf(arr.length(), 8)) {
                val r = arr.optJSONObject(k) ?: continue
                if (!r.optBoolean("enabled", false)) continue
                val url = r.optString("url", "").trim()
                val plain = r.optBoolean("allow_plaintext", false)
                if (!relayUrlOk(url, plain)) continue
                relays.add(RelaySpec(url, plain))
            }
            return ConnectivityState(ready, nearbyActive, n?.optBoolean("enabled", false) == true, token, internet, relays)
        }

        /**
         * wss://host[:port][/path]; ws:// only with allow_plaintext AND a LAN, link-local or .local
         * host (RPC_API §11, CONNECTIVITY §4.11). No user info, query or fragment.
         */
        fun relayUrlOk(url: String, allowPlaintext: Boolean): Boolean {
            if (url.length > 256 || url.any { it <= ' ' || it.code > 126 }) return false
            val m = Regex("^(wss?)://([A-Za-z0-9.-]+|\\[[0-9A-Fa-f:.]+])(:([0-9]{1,5}))?(/[^?#]*)?$").find(url) ?: return false
            val port = m.groupValues[4]
            if (port.isNotEmpty() && port.toInt() !in 1..65535) return false
            if (m.groupValues[1] == "ws") return allowPlaintext && LocalNets.isLanHost(m.groupValues[2].removePrefix("[").removeSuffix("]"))
            return true
        }
    }
}

/**
 * Which addresses nearby may use (CONNECTIVITY §3.4): the local-network interfaces' own subnets
 * and link-local, nothing else. [ifaces] = (address, prefix length) of the local-network
 * interfaces (Wi-Fi, Ethernet, hotspot; never cellular or VPN). Pure.
 */
class LocalNets(private val ifaces: List<Pair<InetAddress, Int>>) {
    /** May we dial / accept this peer address? */
    fun onLink(a: InetAddress): Boolean = when {
        a.isLoopbackAddress || a.isAnyLocalAddress || a.isMulticastAddress -> false
        a is Inet6Address && a.isLinkLocalAddress -> ifaces.any { it.first is Inet6Address }
        a is Inet4Address && a.isLinkLocalAddress -> inSubnet(a)
        a is Inet4Address && a.isSiteLocalAddress -> inSubnet(a)
        a is Inet6Address && isUla(a) -> inSubnet(a)
        else -> false
    }

    private fun inSubnet(a: InetAddress) = ifaces.any { (ia, len) -> ia.javaClass == a.javaClass && samePrefix(ia, a, len) }

    /** Addresses the listener binds to: the interfaces' private IPv4 and IPv6 link-local addresses. */
    fun listenAddresses(): List<InetAddress> = ifaces.map { it.first }.filter {
        (it is Inet4Address && (it.isSiteLocalAddress || it.isLinkLocalAddress)) || (it is Inet6Address && it.isLinkLocalAddress)
    }.distinctBy { it.hostAddress }

    companion object {
        fun isUla(a: Inet6Address) = (a.address[0].toInt() and 0xfe) == 0xfc

        fun samePrefix(x: InetAddress, y: InetAddress, len: Int): Boolean {
            val a = x.address; val b = y.address
            if (a.size != b.size || len < 0 || len > a.size * 8) return false
            var bits = len; var i = 0
            while (bits >= 8) { if (a[i] != b[i]) return false; i++; bits -= 8 }
            if (bits == 0) return true
            val mask = (0xff shl (8 - bits)) and 0xff
            return (a[i].toInt() and mask) == (b[i].toInt() and mask)
        }

        /** Rate/connection key of a source: the IPv4 address, or the IPv6 /64 (CONNECTIVITY §3.4). */
        fun sourceKey(a: InetAddress?): String = when (a) {
            null -> ""
            is Inet6Address -> a.address.copyOfRange(0, 8).joinToString("") { "%02x".format(it) } + "::/64"
            else -> a.hostAddress ?: ""
        }

        /** A literal LAN / link-local address or a `.local` name (no DNS lookups). */
        fun isLanHost(host: String): Boolean {
            val h = host.lowercase()
            if (h.endsWith(".local") && h.length > 6) return true
            val literal = Regex("^[0-9.]+$").matches(h) || (h.contains(':') && Regex("^[0-9a-f:.]+$").matches(h))
            if (!literal) return false
            val a = try { InetAddress.getByName(h) } catch (_: Exception) { return false }
            return a.isSiteLocalAddress || a.isLinkLocalAddress || (a is Inet6Address && isUla(a))
        }

        /** The device's local-network interfaces (Wi-Fi, Ethernet, hotspot): never cellular, VPN or loopback. */
        fun fromSystem(): LocalNets {
            val out = ArrayList<Pair<InetAddress, Int>>()
            try {
                for (ni in java.net.NetworkInterface.getNetworkInterfaces()?.toList().orEmpty()) {
                    if (!ni.isUp || ni.isLoopback || ni.isPointToPoint || ni.isVirtual) continue
                    if (CELLULAR.containsMatchIn(ni.name)) continue
                    for (ia in ni.interfaceAddresses) ia.address?.let { out.add(it to ia.networkPrefixLength.toInt()) }
                }
            } catch (_: Exception) {}
            return LocalNets(out)
        }

        private val CELLULAR = Regex("^(rmnet|ccmni|pdp|ppp|tun|dummy|ims|clat|v4-|ipsec|seth|ccinet|wwan|usb)")
    }
}

/** DNS-SD record of a nearby EmberMesh device (CONNECTIVITY §3.2). Pure. */
object NearbyRecord {
    const val SERVICE_TYPE = "_embermesh._tcp."
    private val TOKEN = Regex("^[0-9a-f]{8}$")

    fun instanceName(token: String) = "EmberMesh-$token"

    /** TXT exactly ember=1 v=1 link=stream t=<token>. */
    fun txt(token: String): Map<String, String> = linkedMapOf("ember" to "1", "v" to "1", "link" to "stream", "t" to token)

    /**
     * The peer token from a received TXT record, or null to ignore the entry. Browsers take the token
     * only from `t` (never the instance name) and require ember=1, v=1, link=stream. TXT data is
     * hostile: values are bytes of any length; anything unexpected is rejected.
     */
    fun peerToken(attrs: Map<String, ByteArray?>?): String? {
        if (attrs == null || attrs.size > 16) return null
        fun v(k: String): String? {
            val b = attrs[k] ?: return null
            if (b.size > 16) return null
            return String(b, Charsets.US_ASCII)
        }
        if (v("ember") != "1" || v("v") != "1" || v("link") != "stream") return null
        val t = v("t") ?: return null
        return if (TOKEN.matches(t)) t else null
    }

    /** CONNECTIVITY §3.3: the lower token dials at once; the other side waits 8 s, then dials too. */
    fun dialDelayMs(own: String, peer: String): Long = if (own < peer) 0L else 8_000L

    /** Addresses to try in order: IPv6 link-local first, then IPv4, then the rest (250 ms apart). */
    fun order(addrs: List<InetAddress>): List<InetAddress> =
        addrs.distinctBy { it.hostAddress }.sortedBy {
            when {
                it is Inet6Address && it.isLinkLocalAddress -> 0
                it is Inet4Address -> 1
                else -> 2
            }
        }
}

/**
 * Host-side limits for nearby connections (CONNECTIVITY §3.4): at most 2 connections per source
 * (IPv4 address or IPv6 /64, see [LocalNets.sourceKey]), at most 4 new connections per source per
 * minute, at most 4 new dials per minute, one dial per peer token at a time, and a socket ceiling
 * ([MAX_LINKS]; the core itself keeps 16 nearby links and evicts unproven ones). Keys are source
 * keys. Pure (inject the clock); thread-safe.
 */
class NearbyLimits(private val now: () -> Long = System::currentTimeMillis) {
    private val perIp = HashMap<String, Int>()
    private val attempts = HashMap<String, ArrayDeque<Long>>()
    private val dials = ArrayDeque<Long>()
    private val dialing = HashSet<String>()
    private var links = 0

    @Synchronized fun linkCount() = links

    /** An inbound connection from [ip] arrived: may it become a link? Counts it if so. */
    @Synchronized
    fun acceptInbound(ip: String): Boolean {
        if (!attempt(ip)) return false
        if (links >= MAX_LINKS || (perIp[ip] ?: 0) >= MAX_PER_IP) return false
        perIp[ip] = (perIp[ip] ?: 0) + 1; links++
        return true
    }

    /** May we dial the peer [token] at [ip] now? Reserves the dial if so (call [dialDone] after). */
    @Synchronized
    fun startDial(token: String, ip: String): Boolean {
        val t = now()
        while (dials.isNotEmpty() && t - dials.first() >= 60_000L) dials.removeFirst()
        if (token in dialing || dials.size >= MAX_DIALS_PER_MIN || links >= MAX_LINKS || (perIp[ip] ?: 0) >= MAX_PER_IP) return false
        dialing.add(token); dials.addLast(t)
        return true
    }

    /** The dial ended: connected (counts the link to [ip]) or not. */
    @Synchronized
    fun dialDone(token: String, ip: String?, connected: Boolean) {
        dialing.remove(token)
        if (connected && ip != null) { perIp[ip] = (perIp[ip] ?: 0) + 1; links++ }
    }

    /** A nearby link to/from [ip] ended. */
    @Synchronized
    fun linkClosed(ip: String) {
        val n = (perIp[ip] ?: 0) - 1
        if (n <= 0) perIp.remove(ip) else perIp[ip] = n
        if (links > 0) links--
    }

    @Synchronized fun isDialing(token: String) = token in dialing

    @Synchronized fun reset() { perIp.clear(); attempts.clear(); dials.clear(); dialing.clear(); links = 0 }

    private fun attempt(ip: String): Boolean {
        val t = now()
        val q = attempts.getOrPut(ip) { ArrayDeque() }
        while (q.isNotEmpty() && t - q.first() >= 60_000L) q.removeFirst()
        if (q.size >= MAX_ATTEMPTS_PER_IP_PER_MIN) return false
        q.addLast(t)
        if (attempts.size > 1024) attempts.keys.firstOrNull()?.let { attempts.remove(it) }
        return true
    }

    companion object {
        const val MAX_LINKS = 32
        const val MAX_PER_IP = 2
        const val MAX_ATTEMPTS_PER_IP_PER_MIN = 4
        const val MAX_DIALS_PER_MIN = 4
    }
}

/** The policy as the nearby manager uses it. */
fun LocalNets.asScope(): NearbyScope = object : NearbyScope {
    override fun listenAddresses() = this@asScope.listenAddresses()
    override fun onLink(a: InetAddress) = this@asScope.onLink(a)
}
