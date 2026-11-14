package org.embermesh.app.host

import android.content.Context
import android.content.Intent
import android.os.Handler
import android.os.Looper
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.ThreadFactory
import java.util.concurrent.atomic.AtomicInteger

/*
 * Endpoint contract (ARCHITECTURE §1): an endpoint only moves bytes or PCM between hardware and the
 * core. It never parses mesh/KISS/modem data. One Endpoint instance backs exactly one core link.
 */

/** What an endpoint feeds into / pulls out of the core for its link. Thread-safe; callable from any thread. */
interface EndpointSink {
    val linkId: Int

    /** Received bytes. Stream endpoints: any chunking. Datagram endpoints: exactly one datagram per call. */
    fun onBytes(data: ByteArray, off: Int = 0, len: Int = data.size)

    /** Captured mono 16-bit PCM at [sampleRate] (the rate the hardware actually delivers). */
    fun onAudio(samples: ShortArray, count: Int, sampleRate: Int)

    /** Fill [out] with up to [max] samples to play at [sampleRate]; returns count (0 = idle). */
    fun pullAudio(out: ShortArray, max: Int, sampleRate: Int): Int

    /** The endpoint opened (true) or closed / failed (false, with a human readable error). */
    fun onState(connected: Boolean, error: String? = null)

    /** Optional live status shown with the link (peer count, device name, ...). */
    fun onStatus(status: JSONObject) {}

    /** Diagnostic log line (goes to logcat with the link id). */
    fun log(msg: String) {}
}

/**
 * A live connection. Lifecycle: [open] (non-blocking: spawn your own threads, report with
 * sink.onState) → [write]* → [close]. [write] is called on the node thread and must not block:
 * queue and return. [close] must be idempotent and release all hardware.
 */
abstract class Endpoint(val config: JSONObject, val sink: EndpointSink) {
    abstract fun open()
    abstract fun close()
    abstract fun write(data: ByteArray)

    /** `link.ptt` event from the core (serial endpoints key RTS/DTR, audio endpoints their PTT target). */
    open fun setPtt(on: Boolean) {}

    /** Keys a control line of this endpoint for another link's PTT (USB serial: "rts" / "dtr"). */
    open fun keyLine(line: String, invert: Boolean, on: Boolean) {
        throw HostError.unsupported("this endpoint has no PTT control lines")
    }

    /** How long PTT must stay keyed after the core's `on:false` (output buffer latency), ms. */
    open fun pttReleaseDelayMs(): Long = 0

    /** Applies a new endpoint config without reopening; false = caller must reopen the endpoint. */
    open fun reconfigure(newConfig: JSONObject): Boolean = false

    /** Extra status for host.info / link status. */
    open fun status(): JSONObject = JSONObject()
}

enum class Transport { DATAGRAM, STREAM, AUDIO }

/** One endpoint kind (ARCHITECTURE §6 names), advertised in host.info. */
interface EndpointFactory {
    val kind: String
    val title: String
    val description: String
    val scannable: Boolean get() = false
    /**
     * Preferred link types, most typical first. Only a ranking/fallback: the compatible set comes from
     * the core's links.types `endpoint_kinds` (see [EndpointRegistry]).
     */
    val linkTypes: List<String>
    val defaultLinkType: String get() = linkTypes.first()

    /** What the pipe carries: datagrams (one frame per write), a byte stream, or PCM audio. */
    val transport: Transport get() = Transport.STREAM

    /** Field schema for the endpoint config form (RPC_API §1). */
    fun configSchema(ctx: HostContext): JSONArray

    /** null when usable on this device, else why not (no hardware, OS too old, ...). */
    fun unavailableReason(ctx: HostContext): String? = null

    /** Android runtime permissions that must be granted before [create]/[scan] (may depend on config). */
    fun permissions(ctx: HostContext, config: JSONObject): List<String> = emptyList()

    /** host.endpoints.scan: blocking discovery for up to [timeoutMs]; returns devices
     *  [{id, name, detail?, rssi?, config:{…}}]. Runs on an I/O thread. */
    fun scan(ctx: HostContext, timeoutMs: Long): JSONArray =
        throw HostError.unsupported("$kind does not support scanning")

    fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink): Endpoint
}

/** Activity-side services available while the UI is in the foreground (permission prompts, pickers). */
interface UiHost {
    /** Requests runtime permissions; cb(true) when all granted. Callback on the main thread. */
    fun requestPermissions(perms: Array<String>, cb: (Boolean) -> Unit)

    /** startActivityForResult with a callback on the main thread. */
    fun startForResult(intent: Intent, cb: (resultCode: Int, data: Intent?) -> Unit)

    /** Screen-on flag while true (host.keep_awake). */
    fun setKeepScreenOn(on: Boolean)

    /** The activity is visible (started); a screen flash is only possible then. */
    fun isInFront(): Boolean = true

    /** Screen flash session: save brightness and window flags, keep the screen on. Main thread. */
    fun screenFlashBegin() {}

    /** Full-screen white (on) or the page (off) at maximum brightness, for host.signal.flash. Main thread. */
    fun setScreenFlash(on: Boolean) {}

    /** End of a screen flash: light off, previous brightness and window flags restored exactly. Main thread. */
    fun screenFlashEnd() {}

    /**
     * Shows [view] (a native camera preview) over the page at [rectCss] {x,y,w,h} in CSS pixels of
     * the WebView, or removes it when [view] is null. Main thread.
     */
    fun setCaptureOverlay(view: android.view.View?, rectCss: org.json.JSONObject?) {}

    /** host.haptic on the visible view (respects the system touch-feedback setting); false if not performed. */
    fun haptic(kind: String): Boolean = false
}

/** Shared environment handed to endpoints and services. */
class HostContext(
    private val appContext: Context?,
    /** Emits a host event to the UI (and anything else listening), e.g. host.endpoint.state. */
    val emit: (event: String, data: JSONObject) -> Unit = { _, _ -> },
) {
    val app: Context get() = appContext ?: error("no Android context (unit test)")
    val hasContext: Boolean get() = appContext != null

    @Volatile var ui: UiHost? = null

    /** Open serial links usable as a PTT keyer (id, name), for the audio endpoint's PTT picker. */
    @Volatile var pttLinks: () -> List<Pair<Int, String>> = { emptyList() }

    val io: ExecutorService = Executors.newCachedThreadPool(named("ember-io"))
    val scheduler: ScheduledExecutorService = Executors.newScheduledThreadPool(1, named("ember-sched"))
    val main: Handler by lazy { Handler(Looper.getMainLooper()) }

    fun shutdown() {
        io.shutdownNow()
        scheduler.shutdownNow()
    }

    companion object {
        fun named(prefix: String): ThreadFactory {
            val n = AtomicInteger()
            return ThreadFactory { r -> Thread(r, "$prefix-${n.incrementAndGet()}").apply { isDaemon = true } }
        }
    }
}
