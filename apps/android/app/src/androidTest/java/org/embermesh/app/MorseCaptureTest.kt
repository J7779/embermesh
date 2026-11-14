package org.embermesh.app

import android.Manifest
import android.util.Log
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.capture.CameraLumaSource
import org.embermesh.app.capture.MicPcmSource
import org.embermesh.app.capture.MorseFeeder
import org.embermesh.app.capture.Roi
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

    /** A generated blinking light (SOS SOS at 10 WPM, 30 fps, exposure noise) through the camera source's frame path. */
    @Test fun syntheticBlinkingLightDecodes() {
        val n = TestNode("light", null, emptyList())
        try {
            assumeTrue("core has no morse.rx yet", morseSupported(n))
            val s = session(n, "light")
            val feeder = MorseFeeder(n.runner, s)
            val src = CameraLumaSource(n.ctx, feeder, Roi.CENTRE, "back", null)
            val w = 160; val h = 120
            val frame = ByteBuffer.allocateDirect(w * h)
            val rnd = java.util.Random(1)
            val dotMs = MorseKeyer.dotMs(10).toDouble()
            val pattern = units("SOS SOS")
            var t = 0.0
            val frameMs = 1000.0 / 30
            var ts = 1_000_000_000L
            // 1 s dark lead-in, the message, 1 s dark tail.
            val edges = ArrayList<Pair<Double, Boolean>>()
            var acc = 1000.0
            for ((i, u) in pattern.withIndex()) { edges.add(acc to (i % 2 == 0)); acc += u * dotMs }
            val end = acc + 1500
            while (t < end) {
                val lit = edges.lastOrNull { it.first <= t }?.second ?: false
                val base = if (lit) 220 else 35
                frame.clear()
                for (i in 0 until w * h) frame.put((base + rnd.nextInt(21) - 10).coerceIn(0, 255).toByte())
                frame.flip()
                src.processFrame(frame, w, 1, w, h, ts + (rnd.nextInt(5) - 2) * 1_000_000L) // ±2 ms frame jitter
                t += frameMs; ts += (frameMs * 1e6).toLong()
            }
            feeder.flushLight()
            val st = n.rpc("morse.rx.status", JSONObject().put("session", s))
            Log.i("MorseCaptureTest", "light: frames=${src.frames} status=$st")
            assertTrue(st.toString(), st.getString("transcript").replace(" ", "").contains("SOSSOS"))
        } finally { n.close() }
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
        EmberRuntime.router.handle("host.capture.start", JSONObject().put("kind", "camera_luma").put("session", session))
        try {
            TestNode.waitUntil(8000, "60 camera frames") { cap.status().getJSONObject("camera_luma").getLong("frames") >= 60 }
            TestNode.waitUntil(4000, "exposure locked") { cap.status().getJSONObject("camera_luma").getBoolean("ae_locked") }
            val st = cap.status().getJSONObject("camera_luma")
            Log.i("MorseCaptureTest", "real camera: $st")
            assertTrue(st.toString(), st.getDouble("fps") >= 10)
            assertTrue(st.getDouble("level") in 0.0..1.0)
            assertTrue(st.getLong("samples") >= 60)
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
