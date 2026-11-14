package org.embermesh.app

import android.Manifest
import android.os.Build
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.endpoint.audio.AudioEndpoint
import org.embermesh.app.endpoint.audio.PcmIn
import org.embermesh.app.endpoint.audio.PcmOut
import org.embermesh.app.host.Endpoint
import org.embermesh.app.host.EndpointFactory
import org.embermesh.app.host.EndpointSink
import org.embermesh.app.host.Fields
import org.embermesh.app.host.HostContext
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.CopyOnWriteArrayList
import kotlin.math.sin

/**
 * A virtual audio cable running in real time: what one AudioEndpoint's TX loop writes (PcmOut) is
 * what the other's RX loop reads (PcmIn), with a 120 ms jitter buffer and low-level noise (no
 * digital silence). Both ends pace themselves at [rate] like AudioRecord/AudioTrack do.
 */
class VirtualCable(val rate: Int, private val latencyMs: Long = 60, private val gain: Float = 0.7f,
                   /** VOX model (attack ms, hang ms): audio only passes after the radio has keyed. */
                   private val vox: Pair<Int, Int>? = null) {
    private var voxOpen = false
    private var voxEnv = 0.0
    private var voxAbove = 0
    private var voxBelow = 0
    @Volatile var voxClippedSamples = 0L
    @Volatile var voxKeyUps = 0
    private val q = java.util.ArrayDeque<Short>()
    private var primed = false
    private val rnd = java.util.Random(7)
    @Volatile var written = 0L

    val out = object : PcmOut {
        override val sampleRate = rate
        override val latencyMs = this@VirtualCable.latencyMs
        private var t0 = 0L
        private var sent = 0L
        override fun play() { t0 = System.nanoTime(); sent = 0 }
        override fun pause() {}
        override fun write(buf: ShortArray, n: Int): Int {
            synchronized(q) {
                for (i in 0 until n) {
                    var v = (buf[i] * gain).toInt()
                    if (vox != null) {
                        voxEnv = maxOf(kotlin.math.abs(v).toDouble(), voxEnv * 0.9995) // peak envelope (~40 ms decay)
                        val loud = voxEnv > 600
                        if (!voxOpen) {
                            voxAbove = if (loud) voxAbove + 1 else 0
                            if (voxAbove >= rate * vox.first / 1000) { voxOpen = true; voxKeyUps++; voxBelow = 0 }
                            if (loud) voxClippedSamples++
                            v = 0 // the radio is not transmitting yet: this audio is lost
                        } else {
                            voxBelow = if (loud) 0 else voxBelow + 1
                            if (voxBelow >= rate * vox.second / 1000) { voxOpen = false; voxAbove = 0 }
                        }
                    }
                    q.add(v.toShort())
                }
            }
            written += n
            sent += n
            val due = t0 + sent * 1_000_000_000L / rate - latencyMs * 1_000_000L
            val wait = (due - System.nanoTime()) / 1_000_000L
            if (wait > 0) Thread.sleep(wait)
            return n
        }
        override fun close() {}
    }

    val inp = object : PcmIn {
        override val sampleRate = rate
        private val t0 = System.nanoTime()
        private var got = 0L
        override fun read(buf: ShortArray, n: Int): Int {
            got += n
            val wait = (t0 + got * 1_000_000_000L / rate - System.nanoTime()) / 1_000_000L
            if (wait > 0) Thread.sleep(wait)
            synchronized(q) {
                if (!primed && q.size >= rate * 120 / 1000) primed = true
                for (i in 0 until n) {
                    val s = if (primed && q.isNotEmpty()) q.poll()!!.toInt() else 0
                    if (q.isEmpty()) primed = false
                    buf[i] = (s + rnd.nextGaussian() * 30).toInt().coerceIn(-32768, 32767).toShort()
                }
            }
            return n
        }
        override fun close() {}
    }
}

@RunWith(AndroidJUnit4::class)
class AudioModemTest {
    private val app get() = InstrumentationRegistry.getInstrumentation().targetContext

    /** AudioEndpoint's factory (real schema/permissions) whose endpoints use a virtual cable instead of AudioRecord/AudioTrack. */
    class CableAudio(private val io: () -> Pair<PcmIn, PcmOut>) : EndpointFactory by AudioEndpoint.Factory {
        val endpoints = CopyOnWriteArrayList<AudioEndpoint>()
        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink): Endpoint =
            AudioEndpoint(ctx, config, sink, io).also { endpoints.add(it) }
    }

    /** A serial-kind endpoint that records RTS/DTR keying (stands in for a USB PTT cable). */
    class PttProbe : EndpointFactory {
        val keys = CopyOnWriteArrayList<Pair<Long, String>>()
        override val kind = "serial"; override val title = "PTT probe"; override val description = ""
        override val linkTypes = listOf("stream")
        override fun configSchema(ctx: HostContext): JSONArray = Fields.of { }
        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink): Endpoint = object : Endpoint(config, sink) {
            override fun open() = sink.onState(true)
            override fun close() {}
            override fun write(data: ByteArray) {}
            override fun keyLine(line: String, invert: Boolean, on: Boolean) { keys.add(System.currentTimeMillis() to "$line:$on") }
        }
    }

    @Test fun afskFrameCrossesTheAudioEndpointLoops() {
        val rate = 48_000
        val ab = VirtualCable(rate)
        val ba = VirtualCable(rate)
        val audioA = CableAudio { ba.inp to ab.out }
        val audioB = CableAudio { ab.inp to ba.out }
        val ptt = PttProbe()
        val a = TestNode("A", null, listOf(audioA, ptt))
        val b = TestNode("B", null, listOf(audioB))
        try {
            TestNode.waitUntil(5000, "ready") { a.readyCount >= 1 && b.readyCount >= 1 }
            a.rpc("node.set_profile", JSONObject().put("name", "Radio A"))
            b.rpc("node.set_profile", JSONObject().put("name", "Radio B"))
            val pttLink = a.host("host.link.create", JSONObject().put("name", "PTT cable").put("endpoint", JSONObject().put("kind", "serial").put("config", JSONObject()))
                .put("link", JSONObject().put("type", "stream"))).getJSONObject("link").getInt("id")
            // The PTT picker offers the open serial connection without the user knowing its id.
            val schema = a.host("host.info").getJSONArray("endpoint_kinds").let { k -> (0 until k.length()).map { k.getJSONObject(it) }.first { it.getString("kind") == "audio" } }.getJSONArray("config")
            val pttField = (0 until schema.length()).map { schema.getJSONObject(it) }.first { it.getString("key") == "ptt" }
            val opts = pttField.getJSONArray("options").let { o -> (0 until o.length()).map { o.getJSONObject(it).getString("value") } }
            assertTrue(opts.toString(), opts.contains("vox") && opts.contains("link:$pttLink"))
            // licence_free: no station id (and no callsign) needed for these virtual radios.
            val audioCfg = JSONObject().put("modem", "afsk1200").put("txdelay_ms", 300).put("licence", "licence_free")
            val la = a.host("host.link.create", JSONObject().put("name", "VHF A").put("endpoint", JSONObject().put("kind", "audio")
                .put("config", JSONObject().put("ptt", "link:$pttLink").put("ptt_line", "rts"))).put("link", JSONObject().put("type", "audio").put("config", audioCfg)))
                .getJSONObject("link").getInt("id")
            val lb = b.host("host.link.create", JSONObject().put("name", "VHF B").put("endpoint", JSONObject().put("kind", "audio")
                .put("config", JSONObject().put("ptt", "vox"))).put("link", JSONObject().put("type", "audio").put("config", audioCfg)))
                .getJSONObject("link").getInt("id")
            TestNode.waitUntil(5000, "audio links up") { a.links.active(la)?.connected == true && b.links.active(lb)?.connected == true }

            // Half duplex: a modem ignores RX while it transmits, and both nodes announce themselves at
            // start. Send on a quiet channel and repeat (as an operator would) if a collision ate it.
            val base = "73 from radio A over AFSK ${System.currentTimeMillis() % 100000}"
            var got: JSONObject? = null
            var text = base
            for (attempt in 1..6) {
                TestNode.waitUntil(30_000, "quiet channel") {
                    val busy = a.links.active(la)?.lastStatus?.optBoolean("transmitting") == true || b.links.active(lb)?.lastStatus?.optBoolean("transmitting") == true
                    val st = listOf(a, b).map { n -> n.rpc("links.list").getJSONArray("links").let { l -> (0 until l.length()).map { l.getJSONObject(it) }.filter { it.getString("type") == "audio" } } }.flatten()
                    !busy && st.all { it.optJSONObject("status")?.optBoolean("tx_active") != true && it.optInt("queue", 0) == 0 }
                }
                Thread.sleep(1500L + java.util.Random().nextInt(2500))
                text = if (attempt == 1) base else "$base #$attempt"
                a.rpc("messages.send", JSONObject().put("conv", "ch:public").put("text", text))
                val t = text
                got = b.waitEvent(25_000) { it.optString("event") == "message.new" && it.getJSONObject("data").getJSONObject("message").optString("text") == t }
                if (got != null) break
                android.util.Log.w("AudioModemTest", "attempt $attempt lost (collision?)")
            }
            assertNotNull("B decoded the AFSK frame (A wrote ${ab.written} samples); B link: " +
                b.rpc("links.list").getJSONArray("links").getJSONObject(0).optJSONObject("status"), got)
            assertEquals(lb, got!!.getJSONObject("data").getJSONObject("message").optInt("via_link", lb))
            // PTT: keyed through the serial link's RTS, released only after the output latency.
            assertTrue(a.events.any { it.optString("event") == "link.ptt" && it.getJSONObject("data").getBoolean("on") })
            TestNode.waitUntil(5000, "PTT released") { ptt.keys.any { it.second == "rts:false" } }
            val offEvent = a.events.last { it.optString("event") == "link.ptt" && !it.getJSONObject("data").getBoolean("on") }
            assertNotNull(offEvent)
            assertTrue(ptt.keys.first().second == "rts:true")
            val st = b.rpc("links.list").getJSONArray("links").getJSONObject(0)
            val ms = st.getJSONObject("status").optJSONObject("modem_status")
            android.util.Log.i("AudioModemTest", "decoded '$text' (attempt ${if (text == base) 1 else text.substringAfterLast('#')}); A out=${ab.written} samples; B modem_status=$ms; ptt=${ptt.keys}")
            assertTrue(audioA.endpoints.single().samplesOut > rate) // > 1 s of modem audio went through A's TX loop
            assertTrue(audioB.endpoints.single().samplesIn > rate)
            // Live reconfigure of the audio endpoint (PTT -> VOX, TX level) keeps the link id.
            val r = a.host("host.link.configure", JSONObject().put("id", la).put("endpoint", JSONObject().put("config", JSONObject().put("ptt", "vox").put("tx_gain", 80))))
            val nid = r.getJSONObject("link").getInt("id")
            assertTrue(a.links.active(nid)!!.def.endpointConfig.getString("ptt") == "vox")
        } finally {
            a.close(); b.close()
        }
    }

    /** The real AudioRecord/AudioTrack path on this device/emulator: opens, captures, plays modem audio. */
    @Test fun hardwareAudioPathOpensCapturesAndPlays() {
        assumeTrue(Build.VERSION.SDK_INT >= 28)
        InstrumentationRegistry.getInstrumentation().uiAutomation.grantRuntimePermission(app.packageName, Manifest.permission.RECORD_AUDIO)
        val n = TestNode("hw", null, listOf(AudioEndpoint.Factory))
        try {
            TestNode.waitUntil(5000, "ready") { n.readyCount >= 1 }
            n.rpc("node.set_profile", JSONObject().put("name", "HW"))
            val id = n.host("host.link.create", JSONObject().put("endpoint", JSONObject().put("kind", "audio").put("config", JSONObject()))
                .put("link", JSONObject().put("type", "audio").put("config", JSONObject().put("csma", false).put("licence", "licence_free")))).getJSONObject("link").getInt("id")
            TestNode.waitUntil(5000, "audio endpoint open") { n.links.active(id)?.connected == true }
            n.rpc("messages.send", JSONObject().put("conv", "ch:public").put("text", "tone test"))
            assertNotNull("modem keyed up", n.waitEvent(10_000) { it.optString("event") == "link.ptt" && it.getJSONObject("data").getBoolean("on") })
            assertNotNull("modem finished", n.waitEvent(15_000) { it.optString("event") == "link.ptt" && !it.getJSONObject("data").getBoolean("on") })
            val st = n.links.active(id)!!.endpoint!!.status()
            android.util.Log.i("AudioModemTest", "hardware audio status: $st")
            assertTrue(st.getInt("sample_rate") >= 8000)
            assertTrue(st.getLong("output_latency_ms") > 0)
            val ep = n.links.active(id)!!.endpoint as AudioEndpoint
            assertTrue("AudioRecord delivered samples", ep.samplesIn > 0)
            assertTrue("AudioTrack played modem audio", ep.samplesOut > 8000)
        } finally { n.close() }
    }

    @Suppress("unused")
    private fun tone(rate: Int, hz: Double, n: Int) = ShortArray(n) { (sin(2 * Math.PI * hz * it / rate) * 8000).toInt().toShort() }

    /**
     * Handheld on VOX: the transmitter only keys after 250 ms of audio and drops 400 ms after it stops,
     * so the start of every transmission is clipped. The core's `handheld_vox` preset must still get
     * the frame through (longer lead-in / VOX tone). Skipped until the core has the preset.
     */
    @Test fun handheldVoxPresetSurvivesVoxClipping() {
        val rate = 48_000
        val ab = VirtualCable(rate, vox = 250 to 400)
        val ba = VirtualCable(rate, vox = 250 to 400)
        val audioA = CableAudio { ba.inp to ab.out }
        val audioB = CableAudio { ab.inp to ba.out }
        val a = TestNode("A", null, listOf(audioA))
        val b = TestNode("B", null, listOf(audioB))
        try {
            TestNode.waitUntil(5000, "ready") { a.readyCount >= 1 && b.readyCount >= 1 }
            val types = a.rpc("links.types").getJSONArray("types")
            val audio = (0 until types.length()).map { types.getJSONObject(it) }.first { it.getString("type") == "audio" }
            val preset = audio.getJSONArray("config").let { c -> (0 until c.length()).map { c.getJSONObject(it) }.firstOrNull { it.getString("key") == "preset" } }
            val opts = preset?.optJSONArray("options")?.let { o -> (0 until o.length()).map { o.getJSONObject(it).getString("value") } } ?: emptyList()
            org.junit.Assume.assumeTrue("core audio link has no handheld_vox preset yet", "handheld_vox" in opts)
            a.rpc("node.set_profile", JSONObject().put("name", "VOX A"))
            b.rpc("node.set_profile", JSONObject().put("name", "VOX B"))
            val cfg = JSONObject().put("preset", "handheld_vox").put("licence", "licence_free")
            val la = a.host("host.link.create", JSONObject().put("endpoint", JSONObject().put("kind", "audio").put("config", JSONObject().put("ptt", "vox")))
                .put("link", JSONObject().put("type", "audio").put("config", cfg))).getJSONObject("link").getInt("id")
            val lb = b.host("host.link.create", JSONObject().put("endpoint", JSONObject().put("kind", "audio").put("config", JSONObject().put("ptt", "vox")))
                .put("link", JSONObject().put("type", "audio").put("config", cfg))).getJSONObject("link").getInt("id")
            TestNode.waitUntil(5000, "audio links up") { a.links.active(la)?.connected == true && b.links.active(lb)?.connected == true }
            var got: JSONObject? = null
            for (attempt in 1..6) {
                TestNode.waitUntil(40_000, "quiet channel") {
                    listOf(a, b).all { n -> n.rpc("links.list").getJSONArray("links").getJSONObject(0).optJSONObject("status")?.optBoolean("tx_active") != true }
                }
                Thread.sleep(1500L + java.util.Random().nextInt(2500))
                val text = "vox test $attempt ${System.currentTimeMillis() % 1000}"
                a.rpc("messages.send", JSONObject().put("conv", "ch:public").put("text", text))
                got = b.waitEvent(30_000) { it.optString("event") == "message.new" && it.getJSONObject("data").getJSONObject("message").optString("text") == text }
                if (got != null) break
            }
            android.util.Log.i("AudioModemTest", "VOX: key-ups=${ab.voxKeyUps}, clipped ${ab.voxClippedSamples * 1000 / rate} ms of A's audio; decoded=${got != null}")
            assertTrue("VOX clipped the start of A's transmissions", ab.voxClippedSamples > 0)
            assertNotNull("B decoded through the VOX model", got)
        } finally { a.close(); b.close() }
    }
}
