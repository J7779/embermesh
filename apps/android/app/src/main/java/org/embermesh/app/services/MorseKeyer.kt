package org.embermesh.app.services

import org.json.JSONObject

/**
 * Keys a light (torch or screen) through an on/off pattern (RPC_API §10). Edge n is due at
 * t0 + the sum of the previous durations on a monotonic clock (`SystemClock.elapsedRealtimeNanos`
 * in the app): no accumulated drift, a late edge shortens the next interval. The thread sleeps until
 * ~5 ms before each edge, then yields until it is due; each switch call is started early by half its
 * running-average duration (a torch switch is a binder call of several ms).
 *
 * Stopping ([cancelled] true, or the thread interrupted) ends the run at once with the light off.
 * Statistics: the error of each switch call's midpoint against its deadline ([Stats.lateMs], may be
 * negative) and the call durations — the scheduling error a receiver sees, not counting the lamp's
 * own rise time.
 */
class MorseKeyer(
    private val nanos: () -> Long = System::nanoTime,
    private val sleepMs: (Long) -> Unit = { Thread.sleep(it) },
    private val spin: () -> Unit = { Thread.yield() },
) {
    class Stats(val edges: Int, val lateMs: DoubleArray, val callMs: DoubleArray, val totalErrorMs: Double, val stopped: Boolean) {
        private fun pct(a: DoubleArray, p: Double) = if (a.isEmpty()) 0.0 else a.sorted()[((a.size - 1) * p).toInt()]
        private fun r(v: Double) = Math.round(v * 100) / 100.0
        /** Worst lateness of an edge (ms; 0 when every edge was on time or early). */
        val worstLateMs: Double get() = (lateMs.maxOrNull() ?: 0.0).coerceAtLeast(0.0)
        val meanLateMs: Double get() = if (lateMs.isEmpty()) 0.0 else lateMs.average()

        /** RPC_API §10 `timing`: max_late_ms (the UI warns on it), mean_late_ms, edges; plus Android extras. */
        fun json(): JSONObject = JSONObject().put("max_late_ms", r(worstLateMs)).put("mean_late_ms", r(meanLateMs)).put("edges", edges)
            .put("abs_p95_ms", r(pct(lateMs.map { kotlin.math.abs(it) }.toDoubleArray(), 0.95)))
            .put("abs_max_ms", r(lateMs.maxOfOrNull { kotlin.math.abs(it) } ?: 0.0))
            .put("call_mean_ms", r(if (callMs.isEmpty()) 0.0 else callMs.average())).put("call_max_ms", r(callMs.maxOrNull() ?: 0.0))
            .put("total_error_ms", r(totalErrorMs))
    }

    private class Stopped : RuntimeException()

    /** pattern_ms = [on, off, on, …]; [act] switches the light. Returns timing statistics. */
    fun run(pattern: LongArray, cancelled: () -> Boolean, act: (Boolean) -> Unit): Stats {
        val late = ArrayList<Double>(pattern.size + 1)
        val call = ArrayList<Double>(pattern.size + 1)
        var callNs: Double
        var stopped = false
        var lightOn = false
        var t0 = 0L
        try {
            // Warm-up: the first torch call can be slow; measure a real call time with the light off.
            var w0 = nanos(); act(false); callNs = (nanos() - w0).toDouble()
            w0 = nanos(); act(false); callNs = (nanos() - w0).toDouble()
            t0 = nanos() + 20_000_000L // lead so the first edge is scheduled like the others
            var deadline = t0
            fun waitUntil(target: Long) {
                while (true) {
                    if (cancelled()) throw Stopped()
                    val left = target - nanos()
                    if (left <= 0) break
                    if (left > 6_000_000L) {
                        try { sleepMs((left - 5_000_000L) / 1_000_000L) } catch (_: InterruptedException) { throw Stopped() }
                    } else spin()
                }
            }
            fun edge(on: Boolean) {
                waitUntil(deadline - (callNs / 2).toLong())
                val a = nanos()
                act(on)
                lightOn = on
                val b = nanos()
                late.add((a + (b - a) / 2 - deadline) / 1e6)
                call.add((b - a) / 1e6)
                callNs = callNs * 0.8 + (b - a) * 0.2
            }
            for ((i, ms) in pattern.withIndex()) {
                edge(i % 2 == 0)
                deadline += ms.coerceAtLeast(0) * 1_000_000L
            }
            // The pattern is over at its last deadline: switch off then (a pattern ending on "on"), or
            // wait out the final dark element, so the caller resolves when the whole pattern has passed.
            if (lightOn) edge(false) else waitUntil(deadline)
        } catch (_: Stopped) {
            stopped = true
        } finally {
            // The light always goes off (also on stop and on an error from act()).
            if (lightOn || stopped) try { act(false) } catch (_: Throwable) {}
        }
        val totalError = if (stopped || t0 == 0L) 0.0 else (nanos() - t0) / 1e6 - pattern.sum()
        return Stats(late.size, late.toDoubleArray(), call.toDoubleArray(), totalError, stopped)
    }

    companion object {
        /** Morse timing helper: dot length for [wpm] (PARIS: 1200 / wpm ms). */
        fun dotMs(wpm: Int) = 1200L / wpm.coerceIn(1, 60)
    }
}
