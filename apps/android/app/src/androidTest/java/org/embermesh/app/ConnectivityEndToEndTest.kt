package org.embermesh.app

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.connectivity.NearbyPlatform
import org.embermesh.app.endpoint.net.NsdNearbyPlatform
import org.embermesh.app.endpoint.net.WebSocketEndpoint
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/**
 * RPC_API §11 end to end on the device: two REAL core nodes, each with the production Android host
 * wiring (ConnectivityWiring: nearby manager on NsdManager, relay manager, LinkManager), find each
 * other on the local network without any link being configured and exchange messages both ways;
 * then, with Nearby off, reach each other only through an internet relay (a local MQTT broker)
 * once — and only once — the user consented.
 */
@RunWith(AndroidJUnit4::class)
class ConnectivityEndToEndTest {
    private val app get() = InstrumentationRegistry.getInstrumentation().targetContext

    private fun grantLocalNetwork() {
        for (p in org.embermesh.app.util.Perms.localNetwork())
            InstrumentationRegistry.getInstrumentation().uiAutomation.grantRuntimePermission(app.packageName, p)
    }

    private fun node(label: String, platform: NearbyPlatform) =
        TestNode(label, TestNode.freshDir("conn-$label"), listOf(WebSocketEndpoint.Factory), platform)

    private fun coreLinks(n: TestNode): List<JSONObject> = n.rpc("links.list").getJSONArray("links").let { a -> (0 until a.length()).map { a.getJSONObject(it) } }
    private fun texts(n: TestNode, conv: String): List<JSONObject> =
        n.rpc("messages.list", JSONObject().put("conv", conv).put("limit", 100)).getJSONArray("messages").let { a -> (0 until a.length()).map { a.getJSONObject(it) } }

    private fun befriend(a: TestNode, b: TestNode) {
        a.rpc("contacts.add", JSONObject().put("card", b.rpc("identity.card").getString("card")))
        b.rpc("contacts.add", JSONObject().put("card", a.rpc("identity.card").getString("card")))
    }

    private fun dmBothWays(a: TestNode, b: TestNode, tag: String, timeoutMs: Long) {
        val m = a.rpc("messages.send", JSONObject().put("conv", "dm:${b.nodeId}").put("text", "a->b $tag")).getJSONObject("message")
        TestNode.waitUntil(timeoutMs, "B receives A's DM over $tag") { texts(b, "dm:${a.nodeId}").any { it.getString("text") == "a->b $tag" } }
        TestNode.waitUntil(timeoutMs, "delivery receipt over $tag") {
            texts(a, "dm:${b.nodeId}").any { it.getString("id") == m.getString("id") && it.getString("status") in setOf("delivered", "read") }
        }
        b.rpc("messages.send", JSONObject().put("conv", "dm:${a.nodeId}").put("text", "b->a $tag"))
        TestNode.waitUntil(timeoutMs, "A receives B's DM over $tag") { texts(a, "dm:${b.nodeId}").any { it.getString("text") == "b->a $tag" } }
    }

    @Test fun nearbyDevicesFindEachOtherAndChatWithoutConfiguration() {
        grantLocalNetwork()
        val a = node("A", NsdNearbyPlatform(org.embermesh.app.host.HostContext(app)))
        val b = node("B", NsdNearbyPlatform(org.embermesh.app.host.HostContext(app)))
        try {
            TestNode.waitUntil(10_000, "both nodes ready") { a.readyCount > 0 && b.readyCount > 0 }
            Thread.sleep(1500)
            assertFalse("nothing advertised before first-run setup is applied", a.connectivity!!.nearby.running || b.connectivity!!.nearby.running)
            assertFalse(a.rpc("connectivity.get").getBoolean("ready"))
            // "Start using EmberMesh" (the UI's first-run step) — Nearby stays at its default (on).
            for (n in listOf(a, b)) n.rpc("connectivity.set", JSONObject().put("setup_done", true))
            val ca = a.rpc("connectivity.get"); val cb = b.rpc("connectivity.get")
            assertTrue("Nearby devices is on by default", ca.getJSONObject("nearby").getBoolean("active"))
            TestNode.waitUntil(10_000, "advertising") { a.connectivity!!.nearby.running && b.connectivity!!.nearby.running }
            assertEquals(ca.getJSONObject("nearby").getString("token"), a.connectivity!!.nearby.token)
            assertEquals(cb.getJSONObject("nearby").getString("token"), b.connectivity!!.nearby.token)
            // No link was configured: discovery → dial → links.add {auto:"nearby"} → beacons.
            val toB = { coreLinks(a).filter { it.optString("auto") == "nearby" && it.optString("neighbour") == b.nodeId && it.optString("state") == "up" } }
            val toA = { coreLinks(b).filter { it.optString("auto") == "nearby" && it.optString("neighbour") == a.nodeId && it.optString("state") == "up" } }
            try { TestNode.waitUntil(45_000, "an automatic nearby link A<->B with the neighbour known") { toB().isNotEmpty() && toA().isNotEmpty() } }
            catch (e: AssertionError) { throw AssertionError("${e.message}; A=${a.nodeId} links ${coreLinks(a)}; B=${b.nodeId} links ${coreLinks(b)}; peers A ${a.connectivity!!.nearby.knownPeers()} B ${b.connectivity!!.nearby.knownPeers()}") }
            for (l in toB() + toA()) {
                assertTrue("never persisted: $l", l.optBoolean("persist", false).not())
                assertEquals("nearby", l.getJSONObject("config").optString("preset"))
                assertTrue("auto_key format: $l", Regex("^([0-9a-f]{8})?@[0-9a-f.:]+$").matches(l.optString("auto_key")))
            }
            // Public channel both ways, then DMs with a delivery receipt.
            a.rpc("messages.send", JSONObject().put("conv", "ch:public").put("text", "nearby hello from A"))
            b.rpc("messages.send", JSONObject().put("conv", "ch:public").put("text", "nearby hello from B"))
            TestNode.waitUntil(30_000, "Public A->B") { texts(b, "ch:public").any { it.getString("text") == "nearby hello from A" } }
            TestNode.waitUntil(30_000, "Public B->A") { texts(a, "ch:public").any { it.getString("text") == "nearby hello from B" } }
            befriend(a, b)
            dmBothWays(a, b, "nearby", 30_000)
            // Duplicates: after settling, one nearby link per pair on each side (the core drops the other).
            TestNode.waitUntil(30_000, "one nearby link per pair") { toB().size == 1 && toA().size == 1 }
            assertEquals("A's link role", if (a.connectivity!!.nearby.token!! < b.connectivity!!.nearby.token!!) "dial" else toB()[0].optString("auto_role"), toB()[0].optString("auto_role"))
            // Switch off: the host stops advertising and closes the nearby links.
            a.rpc("connectivity.set", JSONObject().put("nearby", false))
            TestNode.waitUntil(10_000, "A's nearby links closed") { coreLinks(a).none { it.optString("auto") == "nearby" } && !a.connectivity!!.nearby.running }
        } finally { a.close(); b.close() }
    }

    @Test fun internetRelayOnlyAfterConsentThenMessagesBothWays() {
        MockMqttBroker().use { broker ->
            val none = object : NearbyPlatform {
                override fun advertise(token: String, port: Int): AutoCloseable? = null
                override fun browse(listener: NearbyPlatform.Listener): AutoCloseable? = null
            }
            val a = node("RA", none)
            val b = node("RB", none)
            try {
                TestNode.waitUntil(10_000, "both nodes ready") { a.readyCount > 0 && b.readyCount > 0 }
                val relays = JSONArray().put(JSONObject().put("url", broker.url).put("enabled", true).put("allow_plaintext", true))
                for (n in listOf(a, b)) n.rpc("connectivity.set", JSONObject().put("setup_done", true).put("nearby", false).put("relays", relays))
                befriend(a, b)
                Thread.sleep(3_000)
                assertEquals("nothing may connect to a relay before consent", 0, broker.connections.get())
                assertTrue(coreLinks(a).none { it.optString("type") == "relay" })
                // Consent (the "Reach contacts over the internet" switch).
                for (n in listOf(a, b)) n.rpc("connectivity.set", JSONObject().put("internet", true))
                val relayUp = { n: TestNode -> coreLinks(n).any { it.optString("auto") == "relay" && it.optString("type") == "relay" && it.optString("state") == "up" } }
                TestNode.waitUntil(30_000, "relay links up on both nodes") { relayUp(a) && relayUp(b) }
                assertTrue(broker.offeredProtocols.toString(), broker.offeredProtocols.isNotEmpty() && broker.offeredProtocols.all { it == "mqtt" })
                assertTrue(coreLinks(a).none { it.optString("auto") == "nearby" })
                dmBothWays(a, b, "relay", 60_000)
                assertTrue("traffic went through the broker", broker.publishes.get() > 0)
                // Consent withdrawn: the relay links go away (core link.removed / host removal).
                a.rpc("connectivity.set", JSONObject().put("internet", false))
                TestNode.waitUntil(15_000, "A's relay link removed") { coreLinks(a).none { it.optString("type") == "relay" } && a.links.autoLinks("relay").isEmpty() }
            } finally { a.close(); b.close() }
        }
    }
}
