package org.embermesh.app

import android.Manifest
import android.util.Log
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.capture.CameraLumaSource
import org.embermesh.app.capture.MicPcmSource
import org.embermesh.app.capture.MorseFeeder
import org.embermesh.app.capture.GridFeeder
import org.embermesh.app.services.MorseKeyer
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.nio.ByteBuffer
import kotlin.math.sin

/** Morse receive (RPC_API §6) through the native capture sources, with synthetic and real input. */
@RunWith(AndroidJUnit4::class)
class MorseCaptureTest {
    private val inst = InstrumentationRegistry.getInstrumentation()
    private val app get() = inst.targetContext

    companion object {
        private val CODE = mapOf('S' to "...", 'O' to "---", 'C' to "-.-.", 'Q' to "--.-", 'P' to ".--.", 'A' to ".-", 'R' to ".-.", 'I' to "..", 'E' to ".")

        /** [on, off, on, …] in dot units for [text] (letters separated by 3, words by 7). */
        fun units(text: String): List<Int> {
            val out = ArrayList<Int>()
            for ((wi, word) in text.split(' ').withIndex()) {
                if (wi > 0) out[out.size - 1] = 7
                for ((li, ch) in word.withIndex()) {
                    if (li > 0) out[out.size - 1] = 3
                    for (sym in CODE.getValue(ch)) { out.add(if (sym == '.') 1 else 3); out.add(1) }
                }
            }
            out[out.size - 1] = 7
            return out
        }

        fun morseSupported(n: TestNode): Boolean =
            n.call("morse.rx.start", JSONObject().put("source", "light")).optBoolean("ok", false)
    }

    private fun session(n: TestNode, src: String, extra: JSONObject = JSONObject()): Any =
        n.rpc("morse.rx.start", extra.put("source", src)).get("session")

    private fun edgesFor(text: String, wpm: Int, leadMs: Double): Pair<List<Pair<Double, Boolean>>, Double> {
        val dotMs = MorseKeyer.dotMs(wpm).toDouble()
        val edges = ArrayList<Pair<Double, Boolean>>()
        var acc = leadMs
        for ((i, u) in units(text).withIndex()) { edges.add(acc to (i % 2 == 0)); acc += u * dotMs }
        return edges to acc
    }

    /**
     * §12: a far-away flashlight — a 2×2-pixel light blinking "SOS SOS" (10 WPM) somewhere in a dark,
     * noisy 1280×720 frame at 30 fps, plus a steady lamp elsewhere — through the camera source's frame
     * path (whole-frame max grids) into the core's tracker, which must find it by itself and decode it.
     */
    @Test fun tinyBlinkingLightIsFoundAndDecoded() {
        val n = TestNode("tiny", null, emptyList())
        try {
            assumeTrue("core has no morse.rx yet", morseSupported(n))
            val s = session(n, "light")
            val grids = GridFeeder(n.runner, s, java.util.concurrent.Executor { it.run() })
            val src = CameraLumaSource(n.ctx, MorseFeeder(n.runner, s), grids, "back", null)
            val w = 1280; val h = 720
            val frame = ByteBuffer.allocateDirect(w * h)
            val rnd = java.util.Random(3)
            val (edges, endMs) = edgesFor("SOS SOS", 10, 1500.0)
            val lx = (0.73 * w).toInt(); val ly = (0.31 * h).toInt()
            val frameMs = 1000.0 / 30
            var t = 0.0; var ts = 5_000_000_000L
            val noise = ByteArray(w * h) { (18 + rnd.nextInt(9)).toByte() }
            while (t < endMs + 2000) {
                val lit = edges.lastOrNull { it.first <= t }?.second ?: false
                frame.clear(); frame.put(noise, 0, noise.size)
                for (dy in 0..1) for (dx in 0..1) frame.put((ly + dy) * w + lx + dx, (if (lit) 250 else 22).toByte())
                frame.put((0.2 * h).toInt() * w + (0.2 * w).toInt(), 240.toByte()) // a steady lamp
                frame.position(0)
                src.processFrame(frame, w, 1, w, h, ts + (rnd.nextInt(5) - 2) * 1_000_000L)
                t += frameMs; ts += (frameMs * 1e6).toLong()
            }
            grids.flush()
            assumeTrue("core without the §12 tracker (fell back to levels)", !grids.fellBack)
            val st = n.rpc("morse.rx.status", JSONObject().put("session", s))
            Log.i("MorseCaptureTest", "tiny light: frames=${src.frames} feeds=${grids.feeds} status=$st stats=${src.stats()}")
            assertTrue(st.toString(), st.getString("transcript").replace(" ", "").contains("SOSSOS"))
            val tg = st.optJSONObject("target")
            if (tg != null && tg.has("x")) {
                assertEquals("tracked where the light is: $tg", 0.73, tg.getDouble("x"), 0.05)
                assertEquals(0.31, tg.getDouble("y"), 0.05)
            }
        } finally { n.close() }
    }

    /** Section-6 fallback (a core without frames): the source feeds centre-region levels. */
    @Test fun levelsFallbackStillDecodes() {
        val n = TestNode("light", null, emptyList())
        try {
            assumeTrue("core has no morse.rx yet", morseSupported(n))
            val s = session(n, "light")
            val feeder = MorseFeeder(n.runner, s)
            val src = CameraLumaSource(n.ctx, feeder, null, "back", null)
            val w = 160; val h = 120
            val frame = ByteBuffer.allocateDirect(w * h)
            val rnd = java.util.Random(1)
            val (edges, endMs) = edgesFor("SOS SOS", 10, 1000.0)
            var t = 0.0
            val frameMs = 1000.0 / 30
            var ts = 1_000_000_000L
            while (t < endMs + 1500) {
                val lit = edges.lastOrNull { it.first <= t }?.second ?: false
                val base = if (lit) 220 else 35
                frame.clear()
                for (i in 0 until w * h) frame.put((base + rnd.nextInt(21) - 10).coerceIn(0, 255).toByte())
                frame.flip()
                src.processFrame(frame, w, 1, w, h, ts + (rnd.nextInt(5) - 2) * 1_000_000L)
                t += frameMs; ts += (frameMs * 1e6).toLong()
            }
            feeder.flushLight()
            val st = n.rpc("morse.rx.status", JSONObject().put("session", s))
            assertTrue(st.toString(), st.getString("transcript").replace(" ", "").contains("SOSSOS"))
        } finally { n.close() }
    }

    /**
     * The camera source's frame path on the device: a single bright pixel lands in exactly one cell
     * of the max grid (as the user sees it, sensor 0°), timestamps are the sensor's, batches ≤ 8 / 100 ms.
     * Also measures the per-frame cost at 1920×1080 with padded rows.
     */
    @Test fun gridPipelineSinglePixelTimestampsBatchingAndCost() {
        val fed = java.util.concurrent.CopyOnWriteArrayList<JSONObject>()
        val core = object : org.embermesh.app.core.CoreApi {
            override fun call(method: String, paramsJson: String): String { fed.add(JSONObject(paramsJson)); return """{"ok":true,"result":{"transcript":""}}""" }
            override fun linkRx(linkId: Int, data: ByteArray, off: Int, len: Int) {}
            override fun audioRx(linkId: Int, samples: ShortArray, count: Int, sampleRate: Int) {}
            override fun audioTx(linkId: Int, out: ShortArray, max: Int, sampleRate: Int) = 0
            override fun wake() {}
        }
        val ctx = org.embermesh.app.host.HostContext(app)
        val grids = GridFeeder(core, 1, java.util.concurrent.Executor { it.run() })
        val src = CameraLumaSource(ctx, MorseFeeder(core, 1), grids, "back", null)
        try {
            val w = 1920; val h = 1080; val stride = 1984
            val frame = ByteBuffer.allocateDirect(stride * h)
            val bg = ByteArray(stride * h) { 30 }
            val base = 7_000_000_000L
            for (k in 0 until 150) {
                frame.clear(); frame.put(bg); frame.position(0)
                if (k == 3) frame.put(777 * stride + 1311, 255.toByte())
                src.processFrame(frame, stride, 1, w, h, base + k * 33_333_333L)
            }
            grids.flush()
            val frames = fed.flatMap { p -> p.getJSONArray("frames").let { a -> (0 until a.length()).map { a.getJSONObject(it) } } }
            assertEquals(150, frames.size)
            for ((k, f) in frames.withIndex()) assertEquals(Math.round((base + k * 33_333_333L) / 1e5) / 10.0, f.getDouble("t_ms"), 0.05)
            for (p in fed) { val a = p.getJSONArray("frames"); assertTrue(a.length() in 1..8)
                assertTrue(a.getJSONObject(a.length() - 1).getDouble("t_ms") - a.getJSONObject(0).getDouble("t_ms") <= 100.0) }
            val f3 = frames[3]
            assertEquals(64, f3.getInt("w")); assertEquals(36, f3.getInt("h"))
            val mx = android.util.Base64.decode(f3.getString("max_b64"), 0)
            val hot = mx.indices.filter { (mx[it].toInt() and 0xff) == 255 }
            assertEquals(listOf((777 * 36 / 1080) * 64 + 1311 * 64 / 1920), hot)
            assertTrue(android.util.Base64.decode(frames[4].getString("max_b64"), 0).all { it.toInt() == 30 })
            val st = src.stats()
            Log.i("MorseCaptureTest", "grid cost 1920x1080 (stride $stride): mean ${st.getDouble("reduce_ms_mean")} ms, max ${st.getDouble("reduce_ms_max")} ms per frame")
            InstrumentationRegistry.getInstrumentation().sendStatus(98, android.os.Bundle().apply {
                putString("stream", "grid cost 1920x1080: mean ${st.getDouble("reduce_ms_mean")} ms, max ${st.getDouble("reduce_ms_max")} ms per frame\n") })
            assertTrue("per-frame reduction too slow for 30 fps: $st", st.getDouble("reduce_ms_mean") < 33.0)
        } finally { src.stop(); ctx.shutdown() }
    }

    /** A keyed 700 Hz tone at 48 kHz ("CQ CQ" at 15 WPM) through the mic source's PCM path (resampled to 8 kHz). */
    @Test fun syntheticToneDecodes() {
        val n = TestNode("tone", null, emptyList())
        try {
            assumeTrue("core has no morse.rx yet", morseSupported(n))
            val s = session(n, "audio", JSONObject().put("sample_rate", 8000))
            val feeder = MorseFeeder(n.runner, s)
            val src = MicPcmSource(feeder, 8000)
            val rate = 48000
            val dot = rate * MorseKeyer.dotMs(15).toInt() / 1000
            val rnd = java.util.Random(2)
            val all = ArrayList<Short>()
            fun span(samples: Int, on: Boolean) {
                for (i in 0 until samples) {
                    val v = (if (on) sin(2 * Math.PI * 700 * all.size / rate) * 9000 else 0.0) + rnd.nextGaussian() * 300
                    all.add(v.toInt().coerceIn(-32768, 32767).toShort())
                }
            }
            span(rate / 2, false)
            for ((i, u) in units("CQ CQ").withIndex()) span(u * dot, i % 2 == 0)
            span(rate, false)
            val buf = all.toShortArray()
            var off = 0
            while (off < buf.size) { val k = minOf(960, buf.size - off); src.processPcm(buf.copyOfRange(off, off + k), k, rate); off += k }
            feeder.flushAudio()
            val st = n.rpc("morse.rx.status", JSONObject().put("session", s))
            Log.i("MorseCaptureTest", "audio: status=$st")
            assertTrue(st.toString(), st.getString("transcript").replace(" ", "").contains("CQCQ"))
        } finally { n.close() }
    }

    /** The real Camera2 path on this device/emulator: frames arrive, exposure locks, luma levels flow to morse.rx.feed. */
    @Test fun realCameraDeliversLumaFrames() {
        inst.uiAutomation.grantRuntimePermission(app.packageName, Manifest.permission.CAMERA)
        assertTrue(EmberRuntime.start(app))
        val cap = EmberRuntime.capture!!
        val session = JSONObject(EmberRuntime.core.call("morse.rx.start", JSONObject().put("source", "light").toString()))
            .optJSONObject("result")?.opt("session") ?: 1
        assertTrue(EmberRuntime.router.handle("host.info", JSONObject()).getJSONArray("features").toString().contains("capture_camera_luma"))
        val started = EmberRuntime.router.handle("host.capture.start", JSONObject().put("kind", "camera_luma").put("session", session))
        try {
            val steps = started.getJSONArray("zoom_steps")
            assertEquals(1, steps.getInt(0))
            TestNode.waitUntil(8000, "60 camera frames") { cap.status().getJSONObject("camera_luma").getLong("frames") >= 60 }
            TestNode.waitUntil(4000, "white balance locked") { cap.status().getJSONObject("camera_luma").getBoolean("awb_locked") }
            if (steps.length() > 1) EmberRuntime.router.handle("host.capture.update", JSONObject().put("kind", "camera_luma").put("zoom", steps.get(1)))
            Thread.sleep(500)
            val st = cap.status().getJSONObject("camera_luma")
            Log.i("MorseCaptureTest", "real camera: zoom_steps=$steps $st")
            assertTrue(st.toString(), st.getDouble("fps") >= 10)
            assertTrue("grids (§12) or levels (old core) reach the core: $st", st.getLong("grid_frames") >= 50 || st.getLong("samples") >= 50)
            assertTrue("64 cells on the long side: $st", "64" in st.getString("grid").split("x"))
        } finally {
            EmberRuntime.router.handle("host.capture.stop", JSONObject().put("kind", "camera_luma"))
        }
    }

    /** The real AudioRecord path for Morse audio. */
    @Test fun realMicDeliversPcm() {
        inst.uiAutomation.grantRuntimePermission(app.packageName, Manifest.permission.RECORD_AUDIO)
        assertTrue(EmberRuntime.start(app))
        val cap = EmberRuntime.capture!!
        EmberRuntime.router.handle("host.capture.start", JSONObject().put("kind", "mic_pcm").put("session", 1).put("sample_rate", 8000))
        try {
            TestNode.waitUntil(5000, "1 s of microphone PCM") { cap.status().getJSONObject("mic_pcm").getLong("samples") >= 8000 }
            Log.i("MorseCaptureTest", "real mic: ${cap.status()}")
        } finally {
            EmberRuntime.router.handle("host.capture.stop", JSONObject().put("kind", "mic_pcm"))
        }
    }

    /** Morse mic capture while an audio-modem link holds the microphone (two AudioRecords in one app). */
    @Test fun micCaptureCoexistsWithAudioLink() {
        inst.uiAutomation.grantRuntimePermission(app.packageName, Manifest.permission.RECORD_AUDIO)
        val n = TestNode("coexist", null, listOf(org.embermesh.app.endpoint.audio.AudioEndpoint.Factory))
        val cap = org.embermesh.app.capture.CaptureManager(n.ctx, n.runner) { }
        try {
            TestNode.waitUntil(5000, "ready") { n.readyCount >= 1 }
            val id = n.host("host.link.create", JSONObject().put("endpoint", JSONObject().put("kind", "audio").put("config", JSONObject()))
                .put("link", JSONObject().put("type", "audio"))).getJSONObject("link").getInt("id")
            TestNode.waitUntil(5000, "audio link open") { n.links.active(id)?.connected == true }
            val ep = n.links.active(id)!!.endpoint as org.embermesh.app.endpoint.audio.AudioEndpoint
            val before = ep.samplesIn
            cap.start(JSONObject().put("kind", "mic_pcm").put("session", 1))
            TestNode.waitUntil(5000, "Morse capture gets PCM") { cap.status().getJSONObject("mic_pcm").getLong("samples") >= 8000 }
            Thread.sleep(500)
            Log.i("MorseCaptureTest", "coexist: modem link +${ep.samplesIn - before} samples, morse ${cap.status().getJSONObject("mic_pcm").getLong("samples")}")
            assertTrue("modem link kept receiving audio", ep.samplesIn - before > 8000)
        } finally { cap.stopAll(); n.close() }
    }

    /** Torch / screen keying accuracy at 5, 12 and 20 WPM ("PARIS PARIS"). */
    @Test fun flashKeyingTiming() {
        assertTrue(EmberRuntime.start(app))
        // The emulator's only flash unit is on its front camera.
        val svc = EmberRuntime.services as org.embermesh.app.services.AndroidDeviceServices
        svc.useAnyFacingTorchForTest(true)
        try { flashKeyingTimingRun() } finally { svc.useAnyFacingTorchForTest(false) }
    }

    private fun flashKeyingTimingRun() {
        val results = JSONObject()
        for (wpm in listOf(5, 12, 20)) {
            val dot = MorseKeyer.dotMs(wpm)
            val pattern = units("PARIS PARIS").map { it * dot }.dropLast(1)
            val p = JSONObject().put("pattern_ms", org.json.JSONArray(pattern)).put("mode", "torch")
            // host.signal.flash resolves when the pattern has finished, with its timing.
            val r = EmberRuntime.router.handle("host.signal.flash", p)
            assertEquals("torch", r.getString("used"))
            val st = r.getJSONObject("timing")
            results.put("$wpm", st)
            // A 20 WPM dot is 60 ms: switching must start within a few ms of schedule, without drift.
            assertTrue("$wpm WPM: $st", st.getDouble("abs_p95_ms") < 5.0)
            assertTrue("$wpm WPM: $st", kotlin.math.abs(st.getDouble("total_error_ms")) < 30.0)
            Thread.sleep(300)
        }
        Log.i("MorseCaptureTest", "flash timing: $results")
    }

    @Test fun keyerHasNoDrift() {
        // Pure check with a fake clock: deadlines are absolute.
        var now = 0L
        val k = MorseKeyer({ now }, { ms -> now += ms * 1_000_000L + 1_500_000L }, { now += 100_000L }) // sleeps overshoot 1.5 ms; spin steps 0.1 ms
        val st = k.run(LongArray(20) { 60 }, { false }) { now += 200_000L } // each switch takes 0.2 ms
        assertEquals(20, st.edges) // even pattern: ends dark with the last element
        assertTrue(st.json().toString(), st.lateMs.maxOf { kotlin.math.abs(it) } < 2.0)
    }
}
