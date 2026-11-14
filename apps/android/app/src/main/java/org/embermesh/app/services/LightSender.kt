package org.embermesh.app.services

import org.embermesh.app.host.HostError
import org.json.JSONObject
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/** A torch the host can switch without the camera permission or a camera session. */
interface TorchLight {
    /** The device has a usable torch (back camera with a flash unit). */
    val present: Boolean
    /** False while the camera is in use (by this app or another one) and the torch cannot be switched. */
    fun available(): Boolean
    /** Switches the torch; throws [TorchBusyException] when the camera has become unavailable. */
    fun set(on: Boolean)
}

class TorchBusyException(message: String) : Exception(message)

/** The full-screen flash fallback (white window, maximum brightness), restored exactly by [end]. */
interface ScreenLight {
    /** The app's window is in front (a screen flash is only possible then). */
    fun available(): Boolean
    /** Saves brightness/flags and keeps the screen on. */
    fun begin()
    fun set(on: Boolean)
    /** Light off, previous brightness and window flags restored. */
    fun end()
}

/**
 * Morse sending by light (RPC_API §10): validates the pattern, picks the light for the `mode`, runs
 * the [MorseKeyer] on a dedicated high-priority thread and blocks the caller until the pattern has
 * finished or was stopped. One run at a time: a new flash stops the running one first. The light
 * always goes off — on the pattern's end, [stop], an error, or the keyer thread being interrupted.
 * Nothing about the pattern is logged or kept.
 */
class LightSender(
    private val torch: TorchLight?,
    private val screen: ScreenLight,
    private val keyer: () -> MorseKeyer = { MorseKeyer(nanos = { android.os.SystemClock.elapsedRealtimeNanos() }) },
    private val threadSetup: () -> Unit = {
        try { android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_DISPLAY) } catch (_: Throwable) {}
    },
) {
    private class Run {
        @Volatile var stopped = false
        @Volatile var thread: Thread? = null
        val done = CountDownLatch(1)
    }

    private val lock = Any()
    @Volatile private var current: Run? = null
    @Volatile var lastTiming: JSONObject? = null
        private set

    /** "yes" when a torch is present (Android can always tell without a permission). */
    fun torchInfo(): String = if (torch?.present == true) "yes" else "no"

    /**
     * The light [mode] resolves to now: ("torch" | "screen", fallback?) — fallback says why `auto`
     * uses the screen although a torch was wanted ("no_torch" | "busy"). Throws when neither is possible.
     */
    fun choose(mode: String): Pair<String, String?> = when (mode) {
        "torch" -> when {
            torch == null || !torch.present -> throw HostError.unsupported("this device has no flashlight")
            !torch.available() -> throw HostError(HostError.BUSY, "the flashlight is in use by the camera (this or another app)")
            else -> "torch" to null
        }
        "screen" -> if (screen.available()) "screen" to null else throw HostError.unsupported("a screen flash needs the app in the foreground")
        "auto" -> when {
            torch != null && torch.present && torch.available() -> "torch" to null
            screen.available() -> "screen" to (if (torch != null && torch.present) "busy" else "no_torch")
            torch != null && torch.present -> throw HostError(HostError.BUSY, "the flashlight is in use by the camera and the app is not in front")
            else -> throw HostError.unsupported("no flashlight, and the app is not in front for a screen flash")
        }
        else -> throw HostError.badParams("mode must be auto, torch or screen")
    }

    /** host.signal.flash: returns {used, stopped, timing} when the pattern has finished or was stopped. */
    fun flash(pattern: LongArray, mode: String): JSONObject {
        validate(pattern)
        val run = Run()
        // Atomically become the current run; the previous one (if any) is stopped and awaited, so its
        // light is off before this one starts.
        val previous = synchronized(lock) { val p = current; current = run; p }
        previous?.let { it.stopped = true; it.thread?.interrupt(); it.done.await(3, TimeUnit.SECONDS) }
        val (used, fallback) = try { choose(mode) } catch (e: Throwable) { synchronized(lock) { if (current === run) current = null }; throw e }
        if (run.stopped) { synchronized(lock) { if (current === run) current = null }; return JSONObject().put("used", used).put("stopped", true).apply { if (fallback != null) put("fallback", fallback) } }
        var stats: MorseKeyer.Stats? = null
        var error: Throwable? = null
        val t = Thread({
            threadSetup()
            try {
                if (used == "screen") screen.begin()
                val act: (Boolean) -> Unit = if (used == "torch") { on -> torch!!.set(on) } else { on -> screen.set(on) }
                stats = keyer().run(pattern, { run.stopped }, act)
            } catch (e: Throwable) {
                error = e
            } finally {
                // Belt and braces: whatever happened, the light is off and the screen restored.
                if (used == "torch") try { torch!!.set(false) } catch (_: Throwable) {}
                else try { screen.end() } catch (_: Throwable) {}
                synchronized(lock) { if (current === run) current = null }
                run.done.countDown()
            }
        }, "ember-light")
        t.isDaemon = true
        t.priority = Thread.MAX_PRIORITY
        run.thread = t
        t.start()
        run.done.await()
        error?.let { e ->
            if (e is TorchBusyException) throw HostError(HostError.BUSY, "the flashlight became unavailable (camera in use)")
            if (e is HostError) throw e
            throw HostError(HostError.INTERNAL, "flash failed")
        }
        val st = stats!!
        val timing = st.json().put("light", used)
        lastTiming = timing
        return JSONObject().put("used", used).put("stopped", st.stopped || run.stopped).put("timing", timing)
            .apply { if (fallback != null) put("fallback", fallback) }
    }

    /** host.signal.stop, a newer flash, backgrounding: ends a running flash at once (light off). */
    fun stop() {
        val r = synchronized(lock) { current } ?: return
        r.stopped = true
        r.thread?.interrupt()
        r.done.await(3, TimeUnit.SECONDS)
    }

    val running: Boolean get() = current != null

    companion object {
        const val MAX_ENTRIES = 4096
        const val MIN_MS = 10L
        const val MAX_MS = 60_000L
        const val MAX_TOTAL_MS = 600_000L

        /** RPC_API §10 limits (also for host.signal.tone): bad_params otherwise. */
        fun validate(pattern: LongArray) {
            if (pattern.isEmpty()) throw HostError.badParams("pattern_ms must not be empty")
            if (pattern.size > MAX_ENTRIES) throw HostError.badParams("pattern_ms: at most $MAX_ENTRIES entries")
            var total = 0L
            for (v in pattern) {
                if (v < MIN_MS || v > MAX_MS) throw HostError.badParams("pattern_ms: each entry must be $MIN_MS..$MAX_MS ms")
                total += v
            }
            if (total > MAX_TOTAL_MS) throw HostError.badParams("pattern_ms: total at most 10 minutes")
        }
    }
}
