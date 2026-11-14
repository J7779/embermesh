package org.embermesh.app.capture

import org.embermesh.app.core.CoreApi
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.nio.ByteBuffer
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.Executor

/** RPC_API §12 grids: whole-frame max pooling, orientation, batching, fallback, camera choice. */
class LumaGridTest {
    private fun plane(w: Int, h: Int, stride: Int, fill: Int = 20) = ByteBuffer.allocateDirect(stride * h).also { b ->
        for (i in 0 until stride * h) b.put(i, fill.toByte())
    }

    @Test fun gridShapeIs64OnTheLongSide() {
        assertEquals(64 to 36, LumaGrid(1920, 1080).let { it.gw to it.gh })
        assertEquals(64 to 48, LumaGrid(640, 480).let { it.gw to it.gh })
        assertEquals(36 to 64, LumaGrid(1080, 1920).let { it.gw to it.gh })
        assertTrue(LumaGrid(4000, 3000).let { it.gw * it.gh } <= 4096)
        assertEquals(8 to 6, LumaGrid(8, 6).let { it.gw to it.gh }) // never more cells than pixels
    }

    @Test fun aSinglePixelDecidesExactlyOneCellOfTheMaxGrid() {
        val w = 1920; val h = 1080; val stride = 2048 // padded rows, as cameras deliver them
        val g = LumaGrid(w, h)
        for ((px, py) in listOf(0 to 0, 1919 to 1079, 977 to 533, 30 to 29, 29 to 30)) {
            val y = plane(w, h, stride)
            y.put(py * stride + px, 255.toByte())
            // garbage in the row padding must not be read
            for (r in 0 until h) for (c in w until stride) y.put(r * stride + c, 250.toByte())
            g.reduce(y, stride, 1)
            val hot = g.max.indices.filter { g.max[it] == 255 }
            assertEquals("pixel ($px,$py)", 1, hot.size)
            val cx = px * g.gw / w; val cy = py * g.gh / h
            assertEquals(cy * g.gw + cx, hot.single())
            assertTrue("padding ignored", g.max.all { it == 255 || it == 20 })
        }
    }

    @Test fun pixelStrideIsRespected() {
        val w = 100; val h = 50; val ps = 2
        val g = LumaGrid(w, h)
        val y = plane(w * ps, h, w * ps)
        for (i in 0 until w * ps * h) if (i % 2 == 1) y.put(i, 255.toByte()) // interleaved chroma-like bytes
        y.put(10 * w * ps + 42 * ps, 200.toByte())
        g.reduce(y, w * ps, ps)
        assertEquals(1, g.max.count { it == 200 })
        assertEquals(0, g.max.count { it == 255 })
    }

    @Test fun meanIsTheCellAverage() {
        val w = 64; val h = 36
        val g = LumaGrid(w, h)
        val y = plane(w, h, w, fill = 0)
        y.put(0, 255.toByte()) // cell (0,0) is exactly one pixel
        g.reduce(y, w, 1)
        val mx = ByteArray(64 * 36); val mn = ByteArray(64 * 36)
        g.orient(0, false, mx, mn)
        assertEquals(255, mx[0].toInt() and 0xff); assertEquals(255, mn[0].toInt() and 0xff)
        val g2 = LumaGrid(640, 360)
        val y2 = plane(640, 360, 640, fill = 0)
        for (r in 0 until 10) for (c in 0 until 5) y2.put(r * 640 + c, 200.toByte()) // half of cell (0,0)
        g2.reduce(y2, 640, 1)
        g2.orient(0, false, mx, mn)
        assertEquals(200, mx[0].toInt() and 0xff); assertEquals(100, mn[0].toInt() and 0xff)
    }

    @Test fun orientationMatchesWhatTheUserSees() {
        // A light at the sensor's top-left. Back camera, sensor 90°, phone upright: the sensor's top-left
        // appears at the top-right of the screen.
        val w = 640; val h = 480
        val g = LumaGrid(w, h)
        val y = plane(w, h, w)
        y.put(0, 255.toByte())
        g.reduce(y, w, 1)
        fun hot(rot: Int, mirror: Boolean): Pair<Int, Int> {
            val mx = ByteArray(g.gw * g.gh); val mn = ByteArray(g.gw * g.gh)
            g.orient(rot, mirror, mx, mn)
            val i = mx.indexOfFirst { (it.toInt() and 0xff) == 255 }
            val sw = g.shownW(rot)
            return (i % sw) to (i / sw)
        }
        assertEquals(0 to 0, hot(0, false))
        assertEquals(47 to 0, hot(90, false))   // 48×64 grid shown, top-right
        assertEquals(63 to 47, hot(180, false))
        assertEquals(0 to 63, hot(270, false))
        assertEquals(63 to 0, hot(0, true))     // mirrored (front camera)
        assertEquals(48, g.shownW(90)); assertEquals(64, g.shownH(90))
        assertEquals(90, LumaGrid.rotation(90, 0, false))
        assertEquals(0, LumaGrid.rotation(90, 90, false))
        assertEquals(0, LumaGrid.rotation(270, 90, true)); assertEquals(270, LumaGrid.rotation(270, 0, true))
    }

    class Recorder(var reject: String? = null) : CoreApi {
        val feeds = CopyOnWriteArrayList<JSONObject>()
        var target: JSONObject? = JSONObject().put("state", "searching").put("confidence", 0)
        override fun call(method: String, paramsJson: String): String {
            if (method != "morse.rx.feed") return """{"ok":false,"error":{"code":"not_implemented","message":"x"}}"""
            feeds.add(JSONObject(paramsJson))
            reject?.let { return """{"ok":false,"error":{"code":"$it","message":"levels required"}}""" }
            return JSONObject().put("ok", true).put("result", JSONObject().put("transcript", "").put("target", target)).toString()
        }
        override fun linkRx(linkId: Int, data: ByteArray, off: Int, len: Int) {}
        override fun audioRx(linkId: Int, samples: ShortArray, count: Int, sampleRate: Int) {}
        override fun audioTx(linkId: Int, out: ShortArray, max: Int, sampleRate: Int) = 0
        override fun wake() {}
    }

    private val now = Executor { it.run() }

    @Test fun batchesOfAtMost8FramesOr100msInOrder() {
        val core = Recorder()
        val targets = ArrayList<JSONObject?>()
        val f = GridFeeder(core, 7, now, { targets.add(it) })
        val mx = ByteArray(4); val mn = ByteArray(4)
        var t = 1000.0
        repeat(40) { f.frame(t, 2, 2, mx, mn); t += 1000.0 / 60 }   // 60 fps: 8-frame batches
        repeat(10) { f.frame(t, 2, 2, mx, mn); t += 40.0 }           // 25 fps: the 100 ms rule
        f.flush()
        val all = core.feeds.flatMap { p -> (0 until p.getJSONArray("frames").length()).map { p.getJSONArray("frames").getJSONObject(it) } }
        assertEquals(50, all.size)
        val ts = all.map { it.getDouble("t_ms") }
        assertEquals("never reordered", ts.sorted(), ts)
        for (p in core.feeds) {
            val fr = p.getJSONArray("frames")
            assertTrue(fr.length() in 1..8)
            assertTrue("span ≤ 100 ms", fr.getJSONObject(fr.length() - 1).getDouble("t_ms") - fr.getJSONObject(0).getDouble("t_ms") <= 100.0)
            assertEquals(7, p.getInt("session"))
            assertFalse(p.has("levels"))
        }
        val one = all.first()
        assertEquals(2, one.getInt("w")); assertEquals(2, one.getInt("h"))
        assertEquals("AAAAAA==", one.getString("max_b64")); assertTrue(one.has("mean_b64"))
        assertEquals(core.feeds.size, targets.size)
    }

    @Test fun framesAreDroppedNotQueuedWhileTheCoreIsBusy() {
        val core = Recorder()
        val held = ArrayList<Runnable>()
        val f = GridFeeder(core, 1, Executor { held.add(it) }) // the first send never finishes
        val mx = ByteArray(1)
        var t = 0.0
        repeat(100) { f.frame(t, 1, 1, mx, null); t += 16.7 }
        assertEquals("one call in flight at a time", 1, held.size)
        assertEquals(100L, f.frames + f.dropped)
        assertTrue("dropped, not queued: ${f.dropped}", f.dropped >= 80 && f.frames <= 16)
        held.removeAt(0).run() // core answers: what was collected meanwhile goes out at once
        assertEquals(1, held.size)
    }

    @Test fun anOldCoreMakesTheSourceFallBackOnce() {
        val core = Recorder(reject = "bad_params")
        var fallbacks = 0
        val f = GridFeeder(core, 1, now, {}, { fallbacks++ })
        val mx = ByteArray(1)
        repeat(30) { f.frame(it * 33.0, 1, 1, mx, null) }
        assertTrue(f.fellBack); assertEquals(1, fallbacks); assertEquals(1, core.feeds.size)
        val g2 = GridFeeder(Recorder(reject = "unsupported"), 1, now, {}, { fallbacks++ })
        repeat(9) { g2.frame(it * 33.0, 1, 1, mx, null) }
        assertTrue(g2.fellBack); assertEquals(2, fallbacks)
        // a later error after frames were accepted is not a fallback
        val c3 = Recorder(); val g3 = GridFeeder(c3, 1, now, {}, { fallbacks++ })
        repeat(9) { g3.frame(it * 33.0, 1, 1, mx, null) }
        c3.reject = "bad_params"
        repeat(9) { g3.frame(400 + it * 33.0, 1, 1, mx, null) }
        assertFalse(g3.fellBack); assertEquals(2, fallbacks)
    }

    @Test fun cameraChoicePrefers60fpsAtHdThenTheRectAspect() {
        val sizes = listOf(4000 to 3000, 1920 to 1080, 1440 to 1080, 1280 to 720, 640 to 480)
        val fast = { w: Int, h: Int -> if (w * h <= 1280 * 720) 16_666_666L else 33_333_333L }
        val ranges = listOf(15 to 30, 30 to 30, 60 to 60)
        assertEquals(CameraChoice.Pick(1280, 720, 60 to 60), CameraChoice.pick(sizes, fast, ranges, null))
        val slow = { _: Int, _: Int -> 33_333_333L }
        assertEquals(CameraChoice.Pick(1920, 1080, 30 to 30), CameraChoice.pick(sizes, slow, ranges, 16.0 / 9))
        assertEquals(CameraChoice.Pick(1440, 1080, 30 to 30), CameraChoice.pick(sizes, slow, ranges, 4.0 / 3))
        assertEquals("no 60 range", 30 to 30, CameraChoice.pick(sizes, fast, listOf(15 to 30, 30 to 30), null).fps)
        assertTrue("capped", CameraChoice.pick(listOf(4000 to 3000, 640 to 480), slow, ranges, null).w == 640)
    }

    @Test fun previewPlacementGeometry() {
        val r = JSONObject().put("x", 20).put("y", 100).put("w", 300).put("h", 200)
        assertEquals(listOf(0 + 55, 120 + 275, 825, 550), CapturePlacement.expected(r, 2.75f, 0, 120).toList())
        assertNull("inside the web view", CapturePlacement.clip(55, 395, 825, 550, 0, 120, 1080, 2000))
        assertEquals("clipped at the web view's top", listOf(0, 20, 825, 550), CapturePlacement.clip(55, 100, 825, 550, 0, 120, 1080, 2000)!!.toList())
        assertEquals(listOf(0, 0, 0, 0), CapturePlacement.clip(0, 0, 10, 10, 100, 100, 10, 10)!!.toList())
    }
}
