package org.embermesh.app.core

import android.util.Log

/** Receives everything the core drains from its outbox. Called on the node thread; must not block. */
interface OutboxSink {
    fun onCoreEvent(json: String)
    fun onLinkTx(linkId: Int, data: ByteArray)
}

/**
 * Dedicated node thread: ticks the core and drains its outbox. The sleep between ticks follows
 * ember_next_wakeup_ms (capped at 60 s) and collapses to [activeIntervalMs] for a few seconds after
 * any input, RPC or outbox traffic so the mesh is responsive while busy and cheap while idle.
 */
class NodeRunner(
    private val node: EmberNode,
    private val sink: OutboxSink,
    private val clock: () -> Long = System::currentTimeMillis,
) : CoreApi {
    @Volatile private var running = false
    private var thread: Thread? = null
    private val signal = Object()
    private var pendingWake = false
    @Volatile private var lastActivity = 0L

    /** Upper bound on the sleep while recently active (ms). */
    @Volatile var activeIntervalMs = 25L
    /** How long after activity the loop stays in the fast cadence (ms). */
    @Volatile var activeWindowMs = 3_000L
    /** Optional extra cap, e.g. 20 ms while an audio modem link is open. 0 = none. */
    @Volatile var maxIntervalMs = 0L

    @Volatile var ticks = 0L
        private set
    @Volatile private var pokes = 0L
    private var zeroStreak = 0
    private fun pendingWakeSnapshot() = synchronized(signal) { pendingWake }
    @Volatile private var wakes = 0L
    private var framesOut = 0L
    private var statWakes = 0L
    private var statFrames = 0L
    private var statT0 = 0L
    private var statTicks = 0L
    private var statPokes = 0L

    fun start() {
        if (running) return
        running = true
        thread = Thread({ loop() }, "ember-node").apply {
            priority = Thread.NORM_PRIORITY + 1
            isDaemon = true
            start()
        }
    }

    fun stop() {
        running = false
        wake()
        thread?.let { if (it !== Thread.currentThread()) it.join(2_000) }
        thread = null
    }

    /** Link I/O: tick now and stay in the fast cadence for [activeWindowMs]. */
    override fun wake() {
        wakes++
        lastActivity = clock()
        poke()
    }

    /** Tick + drain once soon, without entering the fast cadence (RPC calls from the UI). */
    fun poke() {
        pokes++
        synchronized(signal) {
            pendingWake = true
            signal.notifyAll()
        }
    }

    override fun call(method: String, paramsJson: String): String {
        val r = node.call(method, paramsJson)
        poke() // the call may have queued events / frames; a UI poll must not keep the CPU busy
        return r
    }

    override fun linkRx(linkId: Int, data: ByteArray, off: Int, len: Int) {
        node.linkRx(linkId, data, off, len)
        wake()
    }

    override fun audioRx(linkId: Int, samples: ShortArray, count: Int, sampleRate: Int) {
        node.audioRx(linkId, samples, count, sampleRate)
        wake()
    }

    override fun audioTx(linkId: Int, out: ShortArray, max: Int, sampleRate: Int): Int =
        node.audioTx(linkId, out, max, sampleRate)

    private fun loop() {
        while (running) {
            try {
                val now = clock()
                node.tick(now)
                ticks++
                var frames = 0
                while (running) {
                    val item = node.poll() ?: break
                    when (item.kind) {
                        EmberNative.OUT_EVENT -> sink.onCoreEvent(item.text)
                        EmberNative.OUT_LINK_TX -> { frames++; framesOut++; sink.onLinkTx(item.linkId, item.data) }
                    }
                }
                // Only received traffic (wake() from link rx) opens the fast cadence; for transmit pacing the
                // core itself asks for 20 ms ticks while a link has queued frames (ember_next_wakeup_ms).
                var next = node.nextWakeupMs().toLong()
                // Guard: the core sometimes keeps answering 0 ("tick now") although ticking changes
                // nothing (seen for ~40 s after start with a hub link). Never spin on that: after a few
                // empty zero-wait ticks fall back to the 20 ms tick cadence ember.h asks for.
                if (next <= 0 && frames == 0 && !pendingWakeSnapshot()) { if (++zeroStreak > 3) next = ZERO_FLOOR_MS } else zeroStreak = 0
                val wait = computeWait(next, clock() - lastActivity)
                val now2 = clock()
                if (now2 - statT0 >= 10_000) {
                    val tps = (ticks - statTicks) * 1000 / maxOf(1, now2 - statT0)
                    val msg = "${ticks - statTicks} ticks, ${pokes - statPokes} pokes (${wakes - statWakes} link rx), ${framesOut - statFrames} frames out in ${now2 - statT0} ms; next_wakeup=$next"
                    if (tps > 20) Log.w(TAG, "busy: $msg") else Log.d(TAG, msg)
                    statT0 = now2; statTicks = ticks; statPokes = pokes; statWakes = wakes; statFrames = framesOut
                }
                synchronized(signal) {
                    if (!pendingWake && running && wait > 0) signal.wait(wait)
                    pendingWake = false
                }
            } catch (ie: InterruptedException) {
                break
            } catch (t: Throwable) {
                Log.e(TAG, "node loop error", t)
                try { Thread.sleep(200) } catch (_: InterruptedException) { break }
            }
        }
    }

    internal fun computeWait(nextWakeupMs: Long, sinceActivityMs: Long): Long =
        computeWait(nextWakeupMs, sinceActivityMs, activeIntervalMs, activeWindowMs, maxIntervalMs)

    companion object {
        private const val TAG = "EmberNode"
        const val ZERO_FLOOR_MS = 20L

        /** Pure scheduling rule (unit-tested). */
        fun computeWait(nextWakeupMs: Long, sinceActivityMs: Long, activeInterval: Long, activeWindow: Long, maxInterval: Long): Long {
            var w = nextWakeupMs.coerceIn(0, 60_000)
            if (sinceActivityMs in 0 until activeWindow) w = minOf(w, activeInterval)
            if (maxInterval > 0) w = minOf(w, maxInterval)
            return w.coerceAtLeast(1) // never spin: 0 ("tick now") becomes 1 ms
        }
    }
}
