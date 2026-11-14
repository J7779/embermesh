package org.embermesh.app.services

import org.json.JSONObject

/**
 * Keys a light (torch or screen) through an on/off pattern with absolute deadlines (no drift: element
 * n starts at t0 + sum of the previous durations), sleeping until ~2 ms before each edge and then
 * spinning, on a thread the caller gives high priority. Each switch call is started early by half its
 * running-average duration (a torch switch is a binder call of several ms). Records the error of each
 * call's midpoint against the schedule ([Stats.lateMs], can be negative) and the call duration
 * ([Stats.callMs]) — the scheduling error the receiver sees, not counting the lamp's own rise time.
 */
class MorseKeyer(
    private val nanos: () -> Long = System::nanoTime,
    private val sleepMs: (Long) -> Unit = { Thread.sleep(it) },
    private val spin: () -> Unit = { Thread.yield() },
) {
    class Stats(val edges: Int, val lateMs: DoubleArray, val callMs: DoubleArray, val totalErrorMs: Double) {
        private fun pct(a: DoubleArray, p: Double) = if (a.isEmpty()) 0.0 else a.sorted()[((a.size - 1) * p).toInt()]
        fun json(): JSONObject = JSONObject().put("edges", edges)
            .put("late_mean_ms", r(lateMs.average().takeIf { !it.isNaN() } ?: 0.0))
            .put("abs_p95_ms", r(pct(lateMs.map { kotlin.math.abs(it) }.toDoubleArray(), 0.95)))
            .put("abs_max_ms", r(lateMs.maxOfOrNull { kotlin.math.abs(it) } ?: 0.0))
            .put("call_mean_ms", r(callMs.average().takeIf { !it.isNaN() } ?: 0.0)).put("call_max_ms", r(callMs.maxOrNull() ?: 0.0))
            .put("total_error_ms", r(totalErrorMs))
        private fun r(v: Double) = Math.round(v * 100) / 100.0
    }

    /** pattern_ms = [on, off, on, …]; [act] switches the light. Returns timing statistics. */
    fun run(pattern: LongArray, cancelled: () -> Boolean, act: (Boolean) -> Unit): Stats {
        val late = ArrayList<Double>(pattern.size + 1)
        val call = ArrayList<Double>(pattern.size + 1)
        // Warm-up: the first torch call is slow (camera service spin-up, ~100+ ms); measure a real call time.
        var w0 = nanos(); act(false); var callNs = (nanos() - w0).toDouble()
        w0 = nanos(); act(false); callNs = (nanos() - w0).toDouble()
        val t0 = nanos() + 20_000_000L // lead so the first edge is scheduled like the others
        var deadline = t0
        fun edge(on: Boolean) {
            // The lamp changes somewhere inside the switch call: start half a call early.
            val target = deadline - (callNs / 2).toLong()
            while (true) {
                val left = target - nanos()
                if (left <= 0) break
                if (left > 6_000_000L) sleepMs((left - 5_000_000L) / 1_000_000L) else spin()
            }
            val a = nanos()
            act(on)
            val b = nanos()
            late.add((a + (b - a) / 2 - deadline) / 1e6) // error of the call's midpoint vs the schedule
            call.add((b - a) / 1e6)
            callNs = callNs * 0.8 + (b - a) * 0.2
        }
        for ((i, ms) in pattern.withIndex()) {
            if (cancelled()) break
            edge(i % 2 == 0)
            deadline += ms.coerceAtLeast(0) * 1_000_000L
        }
        if (pattern.size % 2 == 1 || pattern.isEmpty()) edge(false) // end dark
        else act(false)
        val totalError = (nanos() - t0) / 1e6 - pattern.sum()
        return Stats(late.size, late.toDoubleArray(), call.toDoubleArray(), totalError)
    }

    companion object {
        /** Morse timing helper: dot length for [wpm] (PARIS: 1200 / wpm ms). */
        fun dotMs(wpm: Int) = 1200L / wpm.coerceIn(1, 60)
    }
}
