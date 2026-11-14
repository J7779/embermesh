package org.embermesh.app

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.endpoint.loopback.LoopbackEndpoint
import org.embermesh.app.host.Endpoint
import org.embermesh.app.host.EndpointFactory
import org.embermesh.app.host.EndpointSink
import org.embermesh.app.host.Fields
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.embermesh.app.host.Transport
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import java.util.concurrent.Executors

/** A virtual datagram cable between two TestNodes (endpoint kind `loopback`, core link type raw). */
class Wire {
    private val sinks = arrayOfNulls<EndpointSink>(2)
    private val exec = Executors.newSingleThreadExecutor()
    @Volatile var frames = 0
    fun factory(side: Int): EndpointFactory = object : EndpointFactory {
        override val kind = "loopback"
        override val title = "Wire $side"
        override val description = "test cable"
        override val linkTypes = listOf("raw")
        override val transport = Transport.DATAGRAM
        override fun configSchema(ctx: HostContext): JSONArray = Fields.of { }
        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink): Endpoint = object : Endpoint(config, sink) {
            override fun open() { sinks[side] = sink; sink.onState(true) }
            override fun close() { if (sinks[side] === sink) sinks[side] = null }
            override fun write(data: ByteArray) {
                val c = data.copyOf()
                exec.execute { sinks[1 - side]?.let { frames++; it.onBytes(c) } }
            }
        }
    }
}

/** The Android host stack against the REAL core (node + C API) — no stub. */
@RunWith(AndroidJUnit4::class)
class RealCoreTest {
    private val app get() = InstrumentationRegistry.getInstrumentation().targetContext

    private fun loopCreate() = JSONObject().put("name", "loop").put("endpoint", JSONObject().put("kind", "loopback").put("config", JSONObject()))
        .put("link", JSONObject().put("type", "raw").put("config", JSONObject().put("preset", "loopback")))

    @Test fun freshIdentityProfileAndLockUnlockWithLinkRestore() {
        val dir = TestNode.freshDir("rc-lock")
        var n = TestNode("a", dir, listOf(LoopbackEndpoint.Factory))
        try {
            TestNode.waitUntil(5000, "first node.ready") { n.readyCount >= 1 }
            val info = n.rpc("node.info")
            assertEquals("first run == empty name", "", info.getString("name"))
            assertEquals(16, info.getString("node_id").length)
            val id = info.getString("node_id")
            val p = n.rpc("node.set_profile", JSONObject().put("name", "Ann Droid").put("callsign", "N0CALL").put("status", "ok"))
            assertEquals("Ann Droid", p.getString("name"))
            assertEquals("N0CALL", p.getString("callsign"))
            // A link created through the host router: persisted by the core only.
            val lid = n.host("host.link.create", loopCreate()).getJSONObject("link").getInt("id")
            TestNode.waitUntil(3000, "loopback up") { n.links.active(lid)?.connected == true }
            assertFalse(File(app.filesDir, "links.json").exists())
            // Passphrase + runtime lock.
            n.rpc("security.set_passphrase", JSONObject().put("new", "correct horse"))
            n.rpc("security.lock")
            assertTrue(n.rpc("security.status").getBoolean("locked"))
            assertEquals("locked", n.call("messages.list", JSONObject().put("conv", "ch:public")).getJSONObject("error").getString("code"))
            n.close()

            // "Restart": a node on the encrypted store starts locked, with no links and no identity.
            n = TestNode("a2", dir, listOf(LoopbackEndpoint.Factory))
            TestNode.waitUntil(5000, "node.ready (locked)") { n.readyCount >= 1 }
            val li = n.rpc("node.info")
            assertTrue(li.getBoolean("locked"))
            assertEquals("", li.getString("node_id"))
            assertEquals(0, n.rpc("links.list").getJSONArray("links").length())
            assertTrue(n.links.activeIds().isEmpty())
            assertEquals("denied", n.call("security.unlock", JSONObject().put("passphrase", "wrong")).getJSONObject("error").getString("code"))
            assertTrue(n.rpc("security.unlock", JSONObject().put("passphrase", "correct horse")).getBoolean("unlocked"))
            // node.ready after unlock -> links.list -> endpoint reopened -> links.endpoint_state.
            TestNode.waitUntil(5000, "node.ready after unlock + link restore") { n.readyCount >= 2 && n.links.active(lid)?.connected == true }
            TestNode.waitUntil(3000, "core link up") {
                val l = n.rpc("links.list").getJSONArray("links").getJSONObject(0)
                l.getInt("id") == lid && l.getString("state") == "up"
            }
            val ui = n.rpc("node.info")
            assertEquals(id, ui.getString("node_id"))
            assertEquals("Ann Droid", ui.getString("name"))
            // Remove: gone from the core too.
            n.host("host.link.remove", JSONObject().put("id", lid))
            assertEquals(0, n.rpc("links.list").getJSONArray("links").length())
        } finally {
            n.close()
        }
    }

    /**
     * Persistence across a real process restart. Run as two separate `am instrument` invocations with
     * -e persist_phase write / -e persist_phase check (scripts/android-e2e.sh); without the argument
     * both phases run here in one process (node destroyed and re-created).
     */
    @Test fun identityPersistsAcrossProcessRestart() {
        val phase = InstrumentationRegistry.getArguments().getString("persist_phase") ?: "both"
        val dir = File(app.filesDir, "persist-test")
        val marker = File(app.filesDir, "persist-test.json")
        if (phase == "write" || phase == "both") {
            dir.deleteRecursively(); dir.mkdirs()
            val n = TestNode("w", dir, listOf(LoopbackEndpoint.Factory))
            try {
                TestNode.waitUntil(5000, "ready") { n.readyCount >= 1 }
                n.rpc("node.set_profile", JSONObject().put("name", "Persisted Pat"))
                val lid = n.host("host.link.create", loopCreate()).getJSONObject("link").getInt("id")
                n.rpc("messages.send", JSONObject().put("conv", "ch:public").put("text", "survives restart"))
                marker.writeText(JSONObject().put("node_id", n.nodeId).put("link", lid).put("pid", android.os.Process.myPid()).toString())
            } finally { n.close() }
        }
        if (phase == "check" || phase == "both") {
            val m = JSONObject(marker.readText())
            if (phase == "check") assertTrue("must run in a new process", m.getInt("pid") != android.os.Process.myPid())
            val n = TestNode("c", dir, listOf(LoopbackEndpoint.Factory))
            try {
                TestNode.waitUntil(5000, "link reopened from the core's list") { n.links.active(m.getInt("link"))?.connected == true }
                val info = n.rpc("node.info")
                assertEquals(m.getString("node_id"), info.getString("node_id"))
                assertEquals("Persisted Pat", info.getString("name"))
                val msgs = n.rpc("messages.list", JSONObject().put("conv", "ch:public")).getJSONArray("messages")
                assertTrue((0 until msgs.length()).any { msgs.getJSONObject(it).getString("text") == "survives restart" })
            } finally { n.close() }
        }
    }

    /** Two real nodes in one process, wired by a virtual cable through LinkManager/HostRouter: a DM end to end. */
    @Test fun twoNodesExchangeDmWithDeliveryReceipt() {
        val wire = Wire()
        val a = TestNode("a", null, listOf(wire.factory(0)))
        val b = TestNode("b", null, listOf(wire.factory(1)))
        try {
            TestNode.waitUntil(5000, "ready") { a.readyCount >= 1 && b.readyCount >= 1 }
            a.rpc("node.set_profile", JSONObject().put("name", "Alice"))
            b.rpc("node.set_profile", JSONObject().put("name", "Bob"))
            val la = a.host("host.link.create", loopCreate()).getJSONObject("link").getInt("id")
            val lb = b.host("host.link.create", loopCreate()).getJSONObject("link").getInt("id")
            assertTrue(la > 0 && lb > 0)
            a.rpc("contacts.add", JSONObject().put("card", b.rpc("identity.card").getString("card")))
            b.rpc("contacts.add", JSONObject().put("card", a.rpc("identity.card").getString("card")))
            val text = "hello Bob 🔥 ${System.nanoTime()}"
            val msg = a.rpc("messages.send", JSONObject().put("conv", "dm:${b.nodeId}").put("text", text)).getJSONObject("message")
            assertTrue(msg.getBoolean("encrypted"))
            val got = b.waitEvent(15_000) { it.optString("event") == "message.new" && it.getJSONObject("data").getJSONObject("message").optString("text") == text }
            assertNotNull("Bob received the DM", got)
            assertEquals("dm:${a.nodeId}", got!!.getJSONObject("data").getJSONObject("message").getString("conv"))
            assertEquals("Alice", got.getJSONObject("data").getJSONObject("message").getString("from_name"))
            val delivered = a.waitEvent(15_000) { it.optString("event") == "message.status" && it.getJSONObject("data").optString("id") == msg.getString("id") &&
                it.getJSONObject("data").optString("status") in setOf("delivered", "read") }
            assertNotNull("Alice saw the delivery receipt", delivered)
            assertTrue(wire.frames > 0)
            // Public channel, the other direction.
            b.rpc("messages.send", JSONObject().put("conv", "ch:public").put("text", "public from Bob"))
            assertNotNull(a.waitEvent(15_000) { it.optString("event") == "message.new" && it.getJSONObject("data").getJSONObject("message").optString("text") == "public from Bob" })
            // The router reports the core's link and endpoint state.
            val st = a.host("host.link.status").getJSONArray("links").getJSONObject(0)
            assertTrue(st.getBoolean("connected"))
            assertTrue(st.getLong("tx_bytes") > 0 && st.getLong("rx_bytes") > 0)
        } finally {
            a.close(); b.close()
        }
    }

    /** host.info against the real core: link types per endpoint kind come from links.types. */
    @Test fun hostInfoLinkTypesFromCore() {
        assertTrue(EmberRuntime.start(app))
        val info = EmberRuntime.router.handle("host.info", JSONObject())
        val kinds = info.getJSONArray("endpoint_kinds")
        val byKind = (0 until kinds.length()).associate { kinds.getJSONObject(it).getString("kind") to kinds.getJSONObject(it) }
        fun types(k: String) = byKind.getValue(k).getJSONArray("link_types").let { a -> (0 until a.length()).map { a.getString(it) } }
        assertTrue(types("tcp").containsAll(listOf("stream", "kiss", "meshtastic", "rnode", "lora_at", "rtl_tcp")))
        assertTrue(types("serial").containsAll(listOf("kiss", "stream", "meshtastic", "rnode", "lora_at")))
        assertTrue(types("ble").containsAll(listOf("kiss", "stream", "rnode", "meshtastic")))
        assertEquals(listOf("stream"), types("ble_peer"))
        assertEquals(listOf("raw"), types("udp"))
        assertEquals(listOf("raw"), types("websocket"))
        assertEquals(listOf("audio"), types("audio"))
        assertFalse("raw" in types("bt_spp"))
        assertEquals(listOf("raw"), types("ws_server"))
        // Every advertised type is a real core type.
        val core = EmberRuntime.core.call("links.types", "{}").let { JSONObject(it).getJSONObject("result").getJSONArray("types") }
        val names = (0 until core.length()).map { core.getJSONObject(it).getString("type") }.toSet()
        for ((k, v) in byKind) for (t in types(k)) assertTrue("$k -> $t", t in names)
        // Platform extras and the hub.
        val actions = info.getJSONArray("actions")
        val ids = (0 until actions.length()).map { actions.getJSONObject(it).getString("id") }
        assertTrue(ids.containsAll(listOf("share_app", "start_on_boot", "battery_optimization")))
        for (i in 0 until actions.length()) {
            val a = actions.getJSONObject(i)
            assertTrue(a.getString("method").startsWith("host."))
            assertTrue(a.getString("title").isNotEmpty() && a.getString("description").isNotEmpty())
        }
        val features = info.getJSONArray("features").toString()
        assertTrue(features, features.contains("\"hub\"") && features.contains("share_app"))
    }

    @Test fun guidesMemoryMappedFromApk() {
        assertTrue(EmberRuntime.start(app))
        assumeTrue("APK built without guides.pack", EmberRuntime.guidesStatus != "none")
        assertEquals("asset", EmberRuntime.guidesStatus)
        val t0 = System.nanoTime()
        val c = JSONObject(EmberRuntime.core.call("guides.categories", "{}")).getJSONObject("result")
        val t1 = System.nanoTime()
        assertEquals(20, c.getJSONArray("categories").length())
        assertTrue(c.getInt("total") > 800)
        val r = JSONObject(EmberRuntime.core.call("guides.search", JSONObject().put("query", "water purification").toString()))
            .getJSONObject("result").getJSONArray("results")
        val t2 = System.nanoTime()
        assertTrue(r.length() > 0)
        android.util.Log.i("RealCoreTest", "guides: total=${c.getInt("total")} categories=${(t1 - t0) / 1e6} ms search=${(t2 - t1) / 1e6} ms")
        val g = JSONObject(EmberRuntime.core.call("guides.get", JSONObject().put("id", r.getJSONObject(0).getString("id")).toString())).getJSONObject("result")
        assertTrue(g.getString("markdown").length > 100)
    }

    @Test fun lockedErrorsAndUnknownMethods() {
        val n = TestNode("e", null, emptyList())
        try {
            assertEquals("not_implemented", n.call("no.such").getJSONObject("error").getString("code"))
            assertEquals("unsupported", n.call("host.info").getJSONObject("error").getString("code")) // host.* reaching the core
            try { n.host("host.link.create", JSONObject().put("endpoint", JSONObject().put("kind", "nope"))); throw AssertionError() } catch (e: HostError) { assertEquals("unsupported", e.code) }
        } finally { n.close() }
    }
}
