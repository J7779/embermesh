package org.embermesh.app.services

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.math.abs

class ServiceLogicTest {
    @Test fun acceptParsing() {
        assertEquals(listOf("application/json", "image/*"), ServiceLogic.acceptToMimes(".json, image/*"))
        assertEquals(emptyList<String>(), ServiceLogic.acceptToMimes(null))
        assertEquals(emptyList<String>(), ServiceLogic.acceptToMimes("*/*,.txt"))
        assertEquals(listOf("application/octet-stream"), ServiceLogic.acceptToMimes(".weird"))
    }

    @Test fun positionJson() {
        val p = ServiceLogic.position(51.5, -0.12, 30.0, 4.5f, 1000)
        assertEquals(51.5, p.getDouble("lat"), 0.0); assertEquals(4.5, p.getDouble("accuracy"), 0.0); assertEquals(1000, p.getLong("ts"))
        val q = ServiceLogic.position(0.0, 0.0, null, null, 1)
        assertFalse(q.has("alt")); assertFalse(q.has("accuracy"))
        try { ServiceLogic.position(91.0, 0.0, null, null, 1); throw AssertionError() } catch (e: IllegalArgumentException) {}
    }

    @Test fun sineHasRampsAndNoClicks() {
        val rate = 8000; val total = 800; val ramp = 40
        val buf = ShortArray(total)
        ServiceLogic.sineChunk(buf, total, 0, total, rate, 700, ramp)
        assertEquals(0, buf[0].toInt())
        assertTrue(abs(buf[total - 1].toInt()) < 200)
        assertTrue(buf.maxOrNull()!! > 15000)
        for (i in 1 until total) assertTrue(abs(buf[i] - buf[i - 1]) < 12000)
        // Chunked generation equals one-shot generation.
        val a = ShortArray(300); val b = ShortArray(300)
        ServiceLogic.sineChunk(a, 300, 0, total, rate, 700, ramp)
        ServiceLogic.sineChunk(b, 150, 0, total, rate, 700, ramp)
        val c = ShortArray(150); ServiceLogic.sineChunk(c, 150, 150, total, rate, 700, ramp)
        for (i in 0 until 150) { assertEquals(a[i], b[i]); assertEquals(a[150 + i], c[i]) }
    }

    @Test fun names() {
        assertEquals("x.json", ServiceLogic.cleanName("/sdcard/x.json", "f"))
        assertEquals("f", ServiceLogic.cleanName(null, "f"))
        assertEquals(1800, ServiceLogic.totalMs(longArrayOf(600, 600, 600)))
    }
}
