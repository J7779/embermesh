package org.embermesh.app.host

import org.embermesh.app.endpoint.loopback.LoopbackEndpoint
import org.json.JSONArray
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class LinkManagerTest {
    @get:Rule val tmp = TemporaryFolder()
    private lateinit var core: FakeCore
    private lateinit var ctx: HostContext
    private val events = ArrayList<Pair<String, JSONObject>>()
    private val perms = ArrayList<List<String>>()

    /** Records writes and PTT; lets the test drive state changes. */
    class Probe(config: JSONObject, sink: EndpointSink) : Endpoint(config, sink) {
        val written = ArrayList<ByteArray>()
        val ptt = ArrayList<Boolean>()
        val lines = ArrayList<String>()
        var closed = false
        var releaseDelay = 0L
        override fun pttReleaseDelayMs() = releaseDelay
        override fun keyLine(line: String, invert: Boolean, on: Boolean) { lines.add("$line:${on != invert}") }
        override fun open() { sink.onState(true) }
        override fun close() { closed = true }
        override fun write(data: ByteArray) { written.add(data) }
        override fun setPtt(on: Boolean) { ptt.add(on) }
    }

    val probes = ArrayList<Probe>()
    private val probeFactory = object : EndpointFactory {
        override val kind = "serial"
        override val title = "Probe"
        override val description = ""
        override val linkTypes = listOf("kiss", "stream")
        override fun configSchema(ctx: HostContext): JSONArray = Fields.of { number("baud", "Baud", default = 9600) }
        override fun permissions(ctx: HostContext, config: JSONObject) = listOf("perm.X")
        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink) = Probe(config, sink).also { probes.add(it) }
    }
    private val registry = EndpointRegistry(listOf(probeFactory, LoopbackEndpoint.Factory))

    private fun manager() = LinkManager(core, registry, ctx) { perms.add(it) }

    @Before fun setUp() {
        core = FakeCore()
        ctx = HostContext(null) { e, d -> synchronized(events) { events.add(e to d) } }
    }

    @After fun tearDown() = ctx.shutdown()

    private fun createParams(kind: String = "serial", type: String? = null) = JSONObject()
        .put("name", "Radio").put("endpoint", JSONObject().put("kind", kind).put("config", JSONObject()))
        .put("link", JSONObject().apply { if (type != null) put("type", type) }.put("config", JSONObject().put("x", 1)))

    @Test fun createAddsLinkAppliesDefaultsPersistsAndOpens() {
        val lm = manager()
        val r = lm.create(createParams())
        val id = r.getJSONObject("link").getInt("id")
        assertEquals("kiss", core.links[id]!!.getString("type")) // default link type
        assertEquals(9600, core.links[id]!!.getJSONObject("endpoint").getJSONObject("config").getInt("baud"))
        assertEquals(listOf(listOf("perm.X")), perms)
        assertTrue(lm.active(id)!!.connected)
        assertTrue(core.endpointStates.any { it.getInt("id") == id && it.getBoolean("connected") })
        assertTrue(events.any { it.first == "host.endpoint.state" && it.second.getInt("link_id") == id })
        assertFalse("hosts keep no saved-link list", core.settings.has("links_autostart"))
        assertFalse(core.calls.contains("settings.set"))
    }

    @Test fun txRoutesToEndpointAndRxToCore() {
        val lm = manager()
        val id = lm.create(createParams()).getJSONObject("link").getInt("id")
        lm.onLinkTx(id, byteArrayOf(1, 2, 3))
        lm.onLinkTx(999, byteArrayOf(9)) // unknown link: ignored
        assertArrayEquals(byteArrayOf(1, 2, 3), probes[0].written.single())
        lm.active(id)!!.onBytes(byteArrayOf(5, 6, 7, 8), 1, 2)
        assertEquals(id, core.rx.single().first)
        assertArrayEquals(byteArrayOf(6, 7), core.rx.single().second)
    }

    @Test fun loopbackEchoesIntoCore() {
        val lm = manager()
        val id = lm.create(createParams("loopback", "raw")).getJSONObject("link").getInt("id")
        lm.onLinkTx(id, byteArrayOf(42))
        val deadline = System.currentTimeMillis() + 2000
        while (core.rx.isEmpty() && System.currentTimeMillis() < deadline) Thread.sleep(10)
        assertArrayEquals(byteArrayOf(42), core.rx.single().second)
    }

    @Test fun pttDirectAndViaOtherLink() {
        val lm = manager()
        val serial = lm.create(createParams()).getJSONObject("link").getInt("id")
        lm.onPtt(serial, true)
        assertEquals(listOf(true), probes[0].ptt)
        // An audio-style link whose config names the serial link as its PTT line.
        val p = createParams()
        p.getJSONObject("endpoint").getJSONObject("config").put("ptt", "link:$serial").put("ptt_line", "dtr")
        val audio = lm.create(p).getJSONObject("link").getInt("id")
        lm.onPtt(audio, true)
        lm.onPtt(audio, false)
        assertEquals(listOf("dtr:true", "dtr:false"), probes[0].lines)
        assertTrue(probes[1].ptt.isEmpty())
        // Legacy numeric ptt_link_id still works; "vox" keys nothing on the other link.
        val legacy = createParams().also { it.getJSONObject("endpoint").getJSONObject("config").put("ptt_link_id", serial) }
        lm.onPtt(lm.create(legacy).getJSONObject("link").getInt("id"), true)
        assertEquals("rts:true", probes[0].lines.last())
        assertEquals(listOf(serial to "Radio"), lm.pttCandidates().take(1))
    }

    @Test fun pttReleaseIsDelayedByOutputLatency() {
        val lm = manager()
        val id = lm.create(createParams()).getJSONObject("link").getInt("id")
        probes[0].releaseDelay = 150
        lm.onPtt(id, true)
        lm.onPtt(id, false)
        assertEquals(listOf(true), probes[0].ptt) // still keyed
        Thread.sleep(400)
        assertEquals(listOf(true, false), probes[0].ptt)
        // A new key-up inside the hold window cancels the pending release.
        lm.onPtt(id, false); lm.onPtt(id, true)
        Thread.sleep(300)
        assertEquals(listOf(true, false, true), probes[0].ptt)
    }

    @Test fun removeClosesEndpointAndForgets() {
        val lm = manager()
        val id = lm.create(createParams()).getJSONObject("link").getInt("id")
        lm.remove(id)
        assertTrue(probes[0].closed)
        assertFalse(core.links.containsKey(id))
        assertEquals(null, lm.active(id))
        try { lm.remove(12345); fail() } catch (e: HostError) { assertEquals("not_found", e.code) }
    }

    @Test fun unknownKindAndCoreErrors() {
        val lm = manager()
        try { lm.create(createParams("nope")); fail() } catch (e: HostError) { assertEquals("unsupported", e.code) }
        try { lm.create(JSONObject()); fail() } catch (e: HostError) { assertEquals("bad_params", e.code) }
        core.supportsLinks = false
        try { lm.create(createParams()); fail() } catch (e: HostError) { assertEquals("not_implemented", e.code) }
        assertTrue(lm.activeIds().isEmpty())
    }

    @Test fun syncReopensEndpointsFromCoreLinks() {
        val id = manager().create(createParams()).getJSONObject("link").getInt("id")
        val adds = core.calls.count { it == "links.add" }
        probes.clear(); core.endpointStates.clear()
        // A new process: nothing local, the core lists the persisted link with its endpoint descriptor.
        val lm2 = manager()
        lm2.sync()
        assertEquals(adds, core.calls.count { it == "links.add" })
        assertTrue(lm2.active(id)!!.connected)
        assertEquals(9600, probes.single().config.getInt("baud"))
        assertTrue(core.endpointStates.any { it.getInt("id") == id && it.getBoolean("connected") })
        // node.ready again (e.g. after unlock): same endpoint stays open, its state is re-reported.
        core.endpointStates.clear()
        lm2.sync()
        assertEquals(1, probes.size)
        assertTrue(core.endpointStates.any { it.getInt("id") == id && it.getBoolean("connected") })
    }

    @Test fun syncWhileLockedDoesNothingAndClosesVanishedLinks() {
        val lm = manager()
        val id = lm.create(createParams()).getJSONObject("link").getInt("id")
        core.locked = true // a node that started locked lists no links
        val lm2 = manager()
        lm2.sync()
        assertTrue(lm2.activeIds().isEmpty())
        core.locked = false
        lm2.sync()
        assertTrue(lm2.active(id)!!.connected)
        core.links.clear() // e.g. security.wipe
        lm2.sync()
        assertEquals(null, lm2.active(id))
        assertTrue(probes.last().closed)
    }

    @Test fun syncSkipsForeignKindsAndNoAutostart() {
        core.links[7] = JSONObject().put("id", 7).put("type", "raw").put("config", JSONObject())
            .put("endpoint", JSONObject().put("kind", "multipeer").put("config", JSONObject()))
        core.links[8] = JSONObject().put("id", 8).put("type", "stream").put("config", JSONObject())
            .put("endpoint", JSONObject().put("kind", "serial").put("config", JSONObject()).put("autostart", false))
        core.links[9] = JSONObject().put("id", 9).put("type", "raw").put("config", JSONObject()) // no endpoint
        val lm = manager()
        lm.sync()
        assertTrue(lm.activeIds().isEmpty())
        assertTrue(core.endpointStates.isEmpty())
        // reconnect opens a link that was not autostarted
        lm.reconnect(8)
        assertTrue(lm.active(8)!!.connected)
        try { lm.reconnect(7); fail() } catch (e: HostError) { assertEquals("unsupported", e.code) }
    }

    @Test fun configureEndpointRecreatesWhenCoreCannotStoreIt() {
        val lm = manager()
        val id = lm.create(createParams()).getJSONObject("link").getInt("id")
        val r = lm.configure(JSONObject().put("id", id).put("config", JSONObject().put("y", 2))
            .put("endpoint", JSONObject().put("config", JSONObject().put("baud", 115200))))
        val nid = r.getJSONObject("link").getInt("id")
        assertEquals(id, r.getInt("replaced"))
        assertTrue(nid != id)
        assertFalse(core.links.containsKey(id))
        assertEquals(115200, core.links[nid]!!.getJSONObject("endpoint").getJSONObject("config").getInt("baud"))
        assertEquals(2, core.links[nid]!!.getJSONObject("config").getInt("y"))
        assertEquals("Radio", core.links[nid]!!.getString("name"))
        assertTrue(probes[0].closed)
        assertTrue(lm.active(nid)!!.connected)
        // Link-config-only changes stay on the same link.
        val r2 = lm.configure(JSONObject().put("id", nid).put("name", "Radio 2"))
        assertEquals(nid, r2.getJSONObject("link").getInt("id"))
        assertEquals("Radio 2", core.links[nid]!!.getString("name"))
    }

    @Test fun configureEndpointLiveWhenCoreStoresIt() {
        core.configureStoresEndpoint = true
        val lm = manager()
        val id = lm.create(createParams()).getJSONObject("link").getInt("id")
        val r = lm.configure(JSONObject().put("id", id).put("endpoint", JSONObject().put("config", JSONObject().put("baud", 57600))))
        assertEquals(id, r.getJSONObject("link").getInt("id"))
        assertEquals(57600, core.links[id]!!.getJSONObject("endpoint").getJSONObject("config").getInt("baud"))
        assertEquals(2, probes.size) // Probe cannot reconfigure live -> reopened under the same id
        assertTrue(probes[0].closed)
        assertTrue(lm.active(id)!!.connected)
    }

    @Test fun linkTypesComeFromCore() {
        val reg = EndpointRegistry(listOf(probeFactory, LoopbackEndpoint.Factory))
        assertEquals(listOf("kiss", "stream"), reg.linkTypesFor(probeFactory)) // before links.types: the factory's ranking
        reg.setCoreLinkTypes(JSONObject(core.call("links.types", "{}")).getJSONObject("result"))
        // serial: everything the core lists for serial that a byte stream can carry, factory ranking first.
        assertEquals(listOf("kiss", "stream", "meshtastic", "lora_at"), reg.linkTypesFor(probeFactory))
        assertEquals("kiss", reg.defaultLinkType(probeFactory))
        assertEquals(listOf("raw"), reg.linkTypesFor(LoopbackEndpoint.Factory))
        val streamOnlyWifi = object : EndpointFactory {
            override val kind = "wifi_direct"; override val title = ""; override val description = ""
            override val linkTypes = listOf("stream")
            override fun configSchema(ctx: HostContext) = JSONArray()
            override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink) = throw UnsupportedOperationException()
        }
        // The core lists wifi_direct only for raw datagrams; a byte-stream endpoint gets the stream types instead.
        assertFalse("raw" in reg.linkTypesFor(streamOnlyWifi))
        assertTrue("stream" in reg.linkTypesFor(streamOnlyWifi))
        val blePeer = object : EndpointFactory {
            override val kind = "ble_peer"; override val title = ""; override val description = ""
            override val linkTypes = listOf("stream")
            override fun configSchema(ctx: HostContext) = JSONArray()
            override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink) = throw UnsupportedOperationException()
        }
        assertEquals(listOf("stream"), reg.linkTypesFor(blePeer))
    }

    @Test fun pttTargetParsing() {
        assertEquals(PttTarget.Vox, PttTarget.parse(JSONObject()))
        assertEquals(PttTarget.Vox, PttTarget.parse(JSONObject().put("ptt", "vox").put("ptt_link_id", 3)))
        assertEquals(PttTarget.Link(4), PttTarget.parse(JSONObject().put("ptt", "link:4")))
        assertEquals(PttTarget.Link(3), PttTarget.parse(JSONObject().put("ptt_link_id", 3)))
        assertEquals(PttTarget.Usb("0403:6001:A1"), PttTarget.parse(JSONObject().put("ptt", "usb:0403:6001:A1")))
        assertEquals("rts", PttTarget.line(JSONObject().put("ptt_line", "bogus")))
    }

    @Test fun disconnectSchedulesReconnect() {
        val lm = manager()
        val id = lm.create(createParams()).getJSONObject("link").getInt("id")
        lm.active(id)!!.onState(false, "unplugged")
        assertFalse(lm.active(id)!!.connected)
        assertTrue(events.any { it.second.optString("error") == "unplugged" })
        lm.kick("serial") // reconnect immediately
        val deadline = System.currentTimeMillis() + 2000
        while (probes.size < 2 && System.currentTimeMillis() < deadline) Thread.sleep(10)
        assertEquals(2, probes.size)
        Thread.sleep(50)
        assertTrue(lm.active(id)!!.connected)
    }

    @Test fun backoff() {
        assertEquals(2000, LinkManager.backoffMs(0))
        assertEquals(4000, LinkManager.backoffMs(1))
        assertEquals(60000, LinkManager.backoffMs(10))
    }
}

class LinkDefTest {
    @Test fun fromCoreLinkAndEndpointDescriptor() {
        val l = JSONObject().put("id", 3).put("name", "n").put("type", "raw").put("config", JSONObject().put("preset", "udp_lan"))
            .put("endpoint", JSONObject().put("kind", "udp").put("config", JSONObject().put("port", 1)).put("autostart", false))
        val d = LinkDef.fromCoreLink(l)!!
        assertEquals(3, d.id); assertEquals("udp", d.endpointKind); assertEquals(1, d.endpointConfig.getInt("port"))
        assertFalse(d.autostart)
        assertFalse(d.endpointJson().getBoolean("autostart"))
        assertFalse(d.copy(autostart = true).endpointJson().has("autostart"))
        assertEquals(null, LinkDef.fromCoreLink(JSONObject().put("id", 1).put("endpoint", JSONObject())))
        assertEquals(null, LinkDef.fromCoreLink(JSONObject().put("id", 1)))
    }
}

class FieldsTest {
    @Test fun schemaAndDefaults() {
        val s = Fields.of {
            text("host", "Host", required = true)
            number("port", "Port", default = 8001, min = 1, max = 65535, unit = "")
            select("mode", "Mode", listOf("a" to "A", "b" to "B"))
            bool("x", "X", default = true, advanced = true)
        }
        assertEquals(4, s.length())
        assertEquals("number", s.getJSONObject(1).getString("type"))
        assertEquals(2, s.getJSONObject(2).getJSONArray("options").length())
        assertTrue(s.getJSONObject(0).getBoolean("required"))
        val c = Fields.applyDefaults(s, JSONObject().put("port", 9))
        assertEquals(9, c.getInt("port")); assertEquals("a", c.getString("mode")); assertTrue(c.getBoolean("x"))
        assertFalse(c.has("host"))
    }
}

class HostRouterLogicTest {
    @Test fun patternAndNames() {
        assertArrayEquals(longArrayOf(100, 0, 10000), HostRouter.pattern(JSONObject().put("pattern_ms", JSONArray(listOf(100, -5, 99999)))))
        try { HostRouter.pattern(JSONObject()); fail() } catch (e: HostError) { assertEquals("bad_params", e.code) }
        try { HostRouter.pattern(JSONObject().put("pattern_ms", JSONArray(List(100) { 10000 }))); fail() } catch (e: HostError) { assertEquals("too_large", e.code) }
        assertEquals("passwd", HostRouter.sanitizeFileName("../../etc/passwd"))
        assertEquals("a b.txt", HostRouter.sanitizeFileName("a<b>\u0001 b.txt".replace("<b>", "")))
        assertEquals("ember-file", HostRouter.sanitizeFileName("..."))
        assertEquals(5, HostRouter.requireId(JSONObject().put("id", 5)))
        try { HostRouter.requireId(JSONObject()); fail() } catch (e: HostError) {}
    }
}
