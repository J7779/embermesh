package org.embermesh.app.connectivity

import org.embermesh.app.endpoint.net.WebSocketEndpoint
import org.embermesh.app.host.HostError
import org.embermesh.app.host.LinkManager
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import java.net.InetAddress

/** CONNECTIVITY §3.2/§3.3/§3.5/§4.9/§4.11 host rules, pure. */
class ConnectivityLogicTest {
    private fun txt(vararg kv: Pair<String, String?>): Map<String, ByteArray?> = kv.associate { (k, v) -> k to v?.toByteArray() }
    private val good = arrayOf("ember" to "1", "v" to "1", "link" to "stream", "t" to "0a1b2c3d")

    @Test fun serviceRecordIsExactlyTheContract() {
        assertEquals("EmberMesh-0a1b2c3d", NearbyRecord.instanceName("0a1b2c3d"))
        assertEquals(linkedMapOf("ember" to "1", "v" to "1", "link" to "stream", "t" to "0a1b2c3d"), NearbyRecord.txt("0a1b2c3d"))
        assertEquals("_embermesh._tcp.", NearbyRecord.SERVICE_TYPE)
    }

    @Test fun peerTokenOnlyFromAValidTxt() {
        assertEquals("0a1b2c3d", NearbyRecord.peerToken(txt(*good)))
        assertNull(NearbyRecord.peerToken(null))
        assertNull(NearbyRecord.peerToken(emptyMap()))
        assertNull("old Android TXT", NearbyRecord.peerToken(txt("proto" to "tcp")))
        assertNull("old iOS TXT (no t)", NearbyRecord.peerToken(txt("ember" to "1", "v" to "1", "link" to "stream")))
        assertNull("ember missing", NearbyRecord.peerToken(txt("v" to "1", "link" to "stream", "t" to "0a1b2c3d")))
        assertNull("v=2", NearbyRecord.peerToken(txt("ember" to "1", "v" to "2", "link" to "stream", "t" to "0a1b2c3d")))
        assertNull("link", NearbyRecord.peerToken(txt("ember" to "1", "v" to "1", "link" to "kiss", "t" to "0a1b2c3d")))
        for (bad in listOf("0A1B2C3D", "0a1b2c3", "0a1b2c3d0", "0a1b2c3g", " 0a1b2c3", "", "0a1b2c3d\u0000"))
            assertNull("token '$bad'", NearbyRecord.peerToken(txt("ember" to "1", "v" to "1", "link" to "stream", "t" to bad)))
        assertNull("null value", NearbyRecord.peerToken(txt("ember" to "1", "v" to "1", "link" to "stream", "t" to null)))
        val huge = HashMap<String, ByteArray?>(txt(*good)); huge["t"] = ByteArray(100_000) { '0'.code.toByte() }
        assertNull("oversized value", NearbyRecord.peerToken(huge))
        val many = HashMap<String, ByteArray?>(txt(*good)); repeat(20) { many["x$it"] = "y".toByteArray() }
        assertNull("too many entries", NearbyRecord.peerToken(many))
        val extra = HashMap<String, ByteArray?>(txt(*good)); extra["name"] = "someone".toByteArray()
        assertEquals("unknown keys are ignored, not trusted", "0a1b2c3d", NearbyRecord.peerToken(extra))
        val binary = HashMap<String, ByteArray?>(txt(*good)); binary["t"] = byteArrayOf(0xff.toByte(), 0xfe.toByte(), 0, 1, 2, 3, 4, 5)
        assertNull("binary garbage", NearbyRecord.peerToken(binary))
    }

    @Test fun lowerTokenDialsHigherWaitsEightSeconds() {
        assertEquals(0L, NearbyRecord.dialDelayMs("00000001", "ffffffff"))
        assertEquals(8_000L, NearbyRecord.dialDelayMs("ffffffff", "00000001"))
        assertEquals("bytewise on the hex strings", 0L, NearbyRecord.dialDelayMs("0a000000", "a0000000"))
    }

    @Test fun addressOrderIpv6LinkLocalThenIpv4() {
        val v4 = InetAddress.getByName("192.168.1.20")
        val ll = InetAddress.getByName("fe80::1%1")
        val g6 = InetAddress.getByName("2001:db8::1")
        assertEquals(listOf(ll, v4, g6), NearbyRecord.order(listOf(g6, v4, ll, v4)))
    }

    @Test fun onLinkPolicyOnlyOwnSubnetsAndLinkLocal() {
        val wlan4 = InetAddress.getByName("192.168.1.20")
        val ll6 = InetAddress.getByName("fe80::1%1")
        val ula = InetAddress.getByName("fd12:3456:789a:1::20")
        val nets = LocalNets(listOf(wlan4 to 24, ll6 to 64, ula to 64))
        assertTrue(nets.onLink(InetAddress.getByName("192.168.1.77")))
        assertFalse("other private subnet", nets.onLink(InetAddress.getByName("192.168.2.7")))
        assertFalse("10/8 not ours", nets.onLink(InetAddress.getByName("10.0.0.5")))
        assertFalse("public", nets.onLink(InetAddress.getByName("8.8.8.8")))
        assertFalse("loopback", nets.onLink(InetAddress.getByName("127.0.0.1")))
        assertTrue("IPv6 link-local", nets.onLink(InetAddress.getByName("fe80::abcd%1")))
        assertTrue("ULA in our prefix", nets.onLink(InetAddress.getByName("fd12:3456:789a:1::99")))
        assertFalse("ULA elsewhere", nets.onLink(InetAddress.getByName("fd12:3456:789a:2::99")))
        assertFalse("global IPv6", nets.onLink(InetAddress.getByName("2001:db8::1")))
        assertFalse("169.254 outside the interface prefix", nets.onLink(InetAddress.getByName("169.254.3.3")))
        assertTrue(LocalNets(listOf(InetAddress.getByName("169.254.3.1") to 16)).onLink(InetAddress.getByName("169.254.9.9")))
        assertFalse("no interface, nothing", LocalNets(emptyList()).onLink(InetAddress.getByName("192.168.1.77")))
        assertEquals(listOf("192.168.1.20", "fe80:0:0:0:0:0:0:1%1"), nets.listenAddresses().map { it.hostAddress })
    }

    @Test fun sourceKeysAreIpv4OrIpv6Slash64() {
        assertEquals("192.168.1.5", LocalNets.sourceKey(InetAddress.getByName("192.168.1.5")))
        assertEquals(LocalNets.sourceKey(InetAddress.getByName("2001:db8:1:2::1")), LocalNets.sourceKey(InetAddress.getByName("2001:db8:1:2:ffff::9")))
        assertFalse(LocalNets.sourceKey(InetAddress.getByName("2001:db8:1:2::1")) == LocalNets.sourceKey(InetAddress.getByName("2001:db8:1:3::1")))
        assertEquals("zone ignored", LocalNets.sourceKey(InetAddress.getByName("fe80::1%1")), LocalNets.sourceKey(InetAddress.getByName("fe80::1%2")))
    }

    @Test fun limitsCapLinksPerIpAndAttempts() {
        var t = 1_000_000L
        val l = NearbyLimits { t }
        assertTrue(l.acceptInbound("10.0.0.2")); assertTrue(l.acceptInbound("10.0.0.2"))
        assertFalse("third connection from one address", l.acceptInbound("10.0.0.2"))
        l.linkClosed("10.0.0.2")
        assertTrue(l.acceptInbound("10.0.0.2"))
        assertFalse("4 attempts per minute from one address", l.acceptInbound("10.0.0.2"))
        t += 60_000
        l.linkClosed("10.0.0.2")
        assertTrue("after a minute", l.acceptInbound("10.0.0.2"))

        val m = NearbyLimits { t }
        for (i in 0 until NearbyLimits.MAX_LINKS) assertTrue(m.acceptInbound("10.0.1.$i"))
        assertFalse("socket ceiling", m.acceptInbound("10.0.1.99"))
        assertFalse("no dial beyond it either", m.startDial("0000aaaa", "10.0.2.1"))
        m.linkClosed("10.0.1.0")
        assertTrue(m.acceptInbound("10.0.1.98"))
    }

    @Test fun limitsOneDialPerTokenAndFourDialsPerMinute() {
        var t = 5_000_000L
        val l = NearbyLimits { t }
        assertTrue(l.startDial("0000000a", "10.0.0.1"))
        assertFalse("one dial per token", l.startDial("0000000a", "10.0.0.1"))
        l.dialDone("0000000a", null, false)
        assertTrue(l.startDial("0000000a", "10.0.0.1")); l.dialDone("0000000a", null, false)
        assertTrue(l.startDial("0000000b", "10.0.0.2")); l.dialDone("0000000b", null, false)
        assertTrue(l.startDial("0000000c", "10.0.0.3")); l.dialDone("0000000c", null, false)
        assertFalse("5th dial within a minute", l.startDial("0000000d", "10.0.0.4"))
        t += 60_001
        assertTrue(l.startDial("0000000d", "10.0.0.4"))
        l.dialDone("0000000d", "10.0.0.4", true)
        assertEquals(1, l.linkCount())
    }

    @Test fun relayBackoffTwoSecondsToFiveMinutesWithJitter() {
        assertEquals(2_000L, LinkManager.relayBackoffMs(0, 0, 0, 1.0))
        assertEquals(4_000L, LinkManager.relayBackoffMs(1, 0, 0, 1.0))
        assertEquals(300_000L, LinkManager.relayBackoffMs(20, 0, 0, 1.0))
        assertEquals(1_600L, LinkManager.relayBackoffMs(0, 0, 0, 0.5)) // jitter clamped to ±20 %
        assertEquals(360_000L, LinkManager.relayBackoffMs(30, 0, 0, 1.2))
        assertEquals("reset after 2 min connected", 2_000L, LinkManager.relayBackoffMs(7, 1_000, 121_000, 1.0))
    }

    @Test fun relayUrlsMustBeWssUnlessPlaintextAllowed() {
        assertTrue(ConnectivityState.relayUrlOk("wss://broker.emqx.io:8084/mqtt", false))
        assertTrue(ConnectivityState.relayUrlOk("wss://[2001:db8::1]:8084/mqtt", false))
        assertFalse(ConnectivityState.relayUrlOk("ws://192.168.1.5:8437/mqtt", false))
        assertTrue(ConnectivityState.relayUrlOk("ws://192.168.1.5:8437/mqtt", true))
        assertTrue(ConnectivityState.relayUrlOk("ws://hub.local:8437/mqtt", true))
        assertTrue(ConnectivityState.relayUrlOk("ws://[fe80::1]:8437/mqtt", true))
        assertFalse("plaintext only to a LAN host", ConnectivityState.relayUrlOk("ws://broker.example.com/mqtt", true))
        assertFalse(ConnectivityState.relayUrlOk("ws://8.8.8.8/mqtt", true))
        assertFalse(ConnectivityState.relayUrlOk("ws://127.0.0.1/mqtt", true))
        for (bad in listOf("https://x/mqtt", "wss://", "wss://a b/", "wss://x:99999/", "wss://x/\u0000", "wss://user@x/", "wss://x/a?b", "javascript:alert(1)"))
            assertFalse(bad, ConnectivityState.relayUrlOk(bad, true))
    }

    @Test fun connectivityParseIsHostileTolerant() {
        assertEquals(ConnectivityState.OFF, ConnectivityState.parse(null))
        assertEquals(ConnectivityState.OFF, ConnectivityState.parse(JSONObject()))
        val badToken = JSONObject().put("ready", true).put("nearby", JSONObject().put("enabled", true).put("active", true).put("token", "XYZ"))
        assertFalse("no valid token, no nearby", ConnectivityState.parse(badToken).nearbyActive)
        val relays = JSONArray()
            .put(JSONObject().put("url", "wss://a.example/mqtt").put("enabled", true).put("username", "u"))
            .put(JSONObject().put("url", "wss://b.example/mqtt").put("enabled", false))
            .put(JSONObject().put("url", "ws://c.example/mqtt").put("enabled", true))
            .put(JSONObject().put("url", "ws://192.168.1.5:8437/mqtt").put("enabled", true).put("allow_plaintext", true))
            .put("garbage")
        fun conn(ready: Boolean, nearbyActive: Boolean, internetActive: Boolean) = JSONObject().put("ready", ready)
            .put("nearby", JSONObject().put("enabled", true).put("active", nearbyActive).put("token", "0a1b2c3d"))
            .put("internet", JSONObject().put("enabled", true).put("active", internetActive).put("relays", relays))
        val on = ConnectivityState.parse(conn(true, true, true))
        assertTrue(on.ready); assertTrue(on.nearbyActive); assertEquals("0a1b2c3d", on.token); assertTrue(on.internetActive)
        assertEquals(listOf("wss://a.example/mqtt", "ws://192.168.1.5:8437/mqtt"), on.relays.map { it.url })
        val notReady = ConnectivityState.parse(conn(false, true, true))
        assertFalse("nothing before ready (setup done + unlocked), whatever else says", notReady.nearbyActive || notReady.internetActive)
        assertTrue(notReady.relays.isEmpty())
        val enabledButInactive = ConnectivityState.parse(conn(true, false, false))
        assertFalse(enabledButInactive.nearbyActive); assertTrue(enabledButInactive.nearbyEnabled)
        assertTrue("no relays without internet.active", enabledButInactive.relays.isEmpty())
    }

    @Test fun websocketProtocolsAreValidatedTokens() {
        assertEquals(listOf("mqtt"), WebSocketEndpoint.protocolsOf(JSONObject().put("protocols", JSONArray().put("mqtt"))))
        assertEquals(emptyList<String>(), WebSocketEndpoint.protocolsOf(JSONObject()))
        for (bad in listOf("mq tt", "mqtt\r\nX-Evil: 1", "", "a,b", "x".repeat(33))) try {
            WebSocketEndpoint.protocolsOf(JSONObject().put("protocols", JSONArray().put(bad)))
            fail("accepted '$bad'")
        } catch (e: HostError) { assertEquals(HostError.BAD_PARAMS, e.code) }
    }

    @Test fun relayLinkParamsFollowTheContract() {
        val p = RelayManager.params(ConnectivityState.RelaySpec("wss://broker.emqx.io:8084/mqtt", false))
        val ep = p.getJSONObject("endpoint")
        assertEquals("websocket", ep.getString("kind"))
        val c = ep.getJSONObject("config")
        assertEquals("wss://broker.emqx.io:8084/mqtt", c.getString("url"))
        assertEquals("mqtt", c.getJSONArray("protocols").getString(0))
        assertFalse(c.has("allow_plaintext")); assertFalse(c.has("insecure"))
        assertEquals("relay", p.getJSONObject("link").getString("type"))
        assertEquals("only the url: credentials are attached by the core", setOf("url"), p.getJSONObject("link").getJSONObject("config").keys().asSequence().toSet())
        assertFalse(p.getBoolean("persist"))
        assertEquals("relay", p.getString("auto")); assertEquals("wss://broker.emqx.io:8084/mqtt", p.getString("auto_key"))
        assertEquals("Internet relay (broker.emqx.io)", p.getString("name"))
        val plain = RelayManager.params(ConnectivityState.RelaySpec("ws://192.168.1.5:8437/mqtt", true))
        assertTrue(plain.getJSONObject("endpoint").getJSONObject("config").getBoolean("allow_plaintext"))
    }
}
