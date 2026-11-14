package org.embermesh.app.services

import org.embermesh.app.host.HostError
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import java.util.concurrent.CopyOnWriteArrayList

class LightSenderTest {
    /** A torch that records every switch, with an optional busy state and a failure after N switches. */
    class FakeTorch(override val present: Boolean = true, var busy: Boolean = false, var failAfter: Int = -1, val onSet: () -> Unit = {}) : TorchLight {
        val calls = CopyOnWriteArrayList<Boolean>()
        @Volatile var lit = false
        override fun available() = present && !busy
        override fun set(on: Boolean) {
            onSet()
            if (failAfter >= 0 && on && calls.count { it } >= failAfter) throw TorchBusyException("camera in use")
            calls.add(on); lit = on
        }
    }

    class FakeScreen(var front: Boolean = true) : ScreenLight {
        val events = CopyOnWriteArrayList<String>()
        @Volatile var lit = false
        override fun available() = front
        override fun begin() { events.add("begin") }
        override fun set(on: Boolean) { lit = on; events.add(if (on) "on" else "off") }
        override fun end() { lit = false; events.add("end") }
    }

    /** Fake monotonic clock: sleeps overshoot by [overshootNs], spins step 0.1 ms. */
    class FakeClock(private val overshootNs: Long = 0) {
        @Volatile var now = 1_000_000_000L
        fun keyer() = MorseKeyer({ now }, { ms -> now += ms * 1_000_000L + overshootNs }, { now += 100_000L })
    }

    private fun sender(t: TorchLight?, s: ScreenLight, clock: FakeClock = FakeClock()) = LightSender(t, s, { clock.keyer() }, {})

    @Test fun noAccumulatedDriftWithLateSleeps() {
        val clock = FakeClock(overshootNs = 3_000_000L) // every sleep 3 ms late
        val torch = FakeTorch(onSet = { clock.now += 2_000_000L }) // each switch takes 2 ms
        val p = LongArray(101) { 60 }
        val r = sender(torch, FakeScreen(), clock).flash(p, "torch")
        assertEquals("torch", r.getString("used"))
        assertFalse(r.getBoolean("stopped"))
        assertFalse(r.has("fallback"))
        val t = r.getJSONObject("timing")
        assertTrue(t.has("max_late_ms") && t.has("mean_late_ms") && t.has("edges"))
        // Edges are absolute deadlines: lateness does not add up over 100 edges.
        assertTrue(t.toString(), kotlin.math.abs(t.getDouble("total_error_ms")) < 5.0)
        assertTrue(t.toString(), t.getDouble("max_late_ms") < 2.0)
        assertFalse("torch ends off", torch.lit)
        assertEquals(false, torch.calls.last())
    }

    @Test fun stopMidPatternLeavesTheTorchOff() {
        val torch = FakeTorch()
        val ls = LightSender(torch, FakeScreen(), { MorseKeyer() }, {})
        var result: org.json.JSONObject? = null
        val th = Thread { result = ls.flash(LongArray(40) { 50 }, "torch") }
        th.start()
        val t0 = System.currentTimeMillis()
        while (torch.calls.count { it } < 3 && System.currentTimeMillis() - t0 < 5000) Thread.sleep(5)
        ls.stop()
        th.join(5000)
        assertTrue(result.toString(), result!!.getBoolean("stopped"))
        assertFalse(torch.lit)
        assertTrue("stopped well before the 2 s pattern end", System.currentTimeMillis() - t0 < 1500)
        assertFalse(ls.running)
    }

    @Test fun aNewFlashStopsTheRunningOne() {
        val torch = FakeTorch()
        val ls = LightSender(torch, FakeScreen(), { MorseKeyer() }, {})
        var first: org.json.JSONObject? = null
        val th = Thread { first = ls.flash(LongArray(40) { 50 }, "torch") }
        th.start()
        while (torch.calls.count { it } < 2) Thread.sleep(5)
        val second = ls.flash(longArrayOf(30, 30, 30), "torch")
        th.join(5000)
        assertTrue(first!!.getBoolean("stopped"))
        assertFalse(second.getBoolean("stopped"))
        assertFalse(torch.lit)
    }

    @Test fun limits() {
        fun bad(p: LongArray) = try { LightSender.validate(p); fail("accepted ${p.size}") } catch (e: HostError) { assertEquals("bad_params", e.code) }
        bad(LongArray(0))
        bad(LongArray(4097) { 20 })
        bad(longArrayOf(100, 9))
        bad(longArrayOf(60_001))
        bad(LongArray(11) { 60_000 }) // 11 minutes
        LightSender.validate(LongArray(4096) { 10 })
        LightSender.validate(LongArray(10) { 60_000 })
    }

    @Test fun modeSelection() {
        val screen = FakeScreen()
        assertEquals("torch" to null, sender(FakeTorch(), screen).choose("auto"))
        assertEquals("screen" to "no_torch", sender(FakeTorch(present = false), screen).choose("auto"))
        assertEquals("screen" to "no_torch", sender(null, screen).choose("auto"))
        assertEquals("screen" to "busy", sender(FakeTorch(busy = true), screen).choose("auto"))
        assertEquals("screen" to null, sender(FakeTorch(), screen).choose("screen"))
        fun err(code: String, f: () -> Unit) = try { f(); fail() } catch (e: HostError) { assertEquals(code, e.code) }
        err("unsupported") { sender(FakeTorch(present = false), screen).choose("torch") }
        err("unsupported") { sender(FakeTorch(), FakeScreen(front = false)).choose("screen") }
        err("unsupported") { sender(null, FakeScreen(front = false)).choose("auto") }
        err("bad_params") { sender(FakeTorch(), screen).choose("laser") }
        assertEquals("yes", sender(FakeTorch(), screen).torchInfo())
        assertEquals("no", sender(FakeTorch(present = false), screen).torchInfo())
        assertEquals("no", sender(null, screen).torchInfo())
    }

    @Test fun cameraBusyFallsBackToTheScreenInAutoAndFailsInTorchMode() {
        val torch = FakeTorch(busy = true)
        val screen = FakeScreen()
        val r = sender(torch, screen).flash(longArrayOf(40, 40, 40), "auto")
        assertEquals("screen", r.getString("used"))
        assertEquals("busy", r.getString("fallback"))
        assertTrue("the busy torch is never touched", torch.calls.isEmpty())
        assertEquals(listOf("begin"), screen.events.take(1))
        assertEquals("end", screen.events.last())
        assertFalse(screen.lit)
        try { sender(torch, screen).flash(longArrayOf(40), "torch"); fail() } catch (e: HostError) { assertEquals("busy", e.code) }
        try { sender(torch, FakeScreen(front = false)).flash(longArrayOf(40), "auto"); fail() } catch (e: HostError) { assertEquals("busy", e.code) }
    }

    @Test fun torchLostMidPatternIsAnErrorWithTheLightOff() {
        val torch = FakeTorch(failAfter = 2)
        try { sender(torch, FakeScreen()).flash(longArrayOf(40, 40, 40, 40, 40, 40, 40), "torch"); fail() } catch (e: HostError) { assertEquals("busy", e.code) }
        assertFalse(torch.lit)
        assertEquals(false, torch.calls.last())
    }

    @Test fun screenIsRestoredAfterStop() {
        val screen = FakeScreen()
        val ls = LightSender(null, screen, { MorseKeyer() }, {})
        val th = Thread { ls.flash(LongArray(40) { 50 }, "screen") }
        th.start()
        while (!screen.events.contains("on")) Thread.sleep(5)
        ls.stop()
        th.join(5000)
        assertEquals("end", screen.events.last())
        assertFalse(screen.lit)
    }

    @Test fun keyerStopsAtOnceWhenInterrupted() {
        val calls = CopyOnWriteArrayList<Boolean>()
        var stats: MorseKeyer.Stats? = null
        val th = Thread { stats = MorseKeyer().run(longArrayOf(5000, 100), { false }) { calls.add(it) } }
        th.start()
        while (!calls.contains(true)) Thread.sleep(5)
        th.interrupt()
        th.join(1000)
        assertTrue(stats!!.stopped)
        assertEquals(false, calls.last())
    }
}
