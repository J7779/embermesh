package org.embermesh.app.capture

import org.embermesh.app.core.CoreApi
import org.embermesh.app.endpoint.net.Base64Std
import org.embermesh.app.host.HostError
import org.embermesh.app.host.rpc
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.Executor
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Sends §12 brightness grids to `morse.rx.feed {session, frames:[…]}`: batches of at most
 * [maxFrames] frames or [maxSpanMs] ms, in order, on [sender] (never the camera thread). While a
 * call is in flight new frames are collected up to one batch; beyond that they are dropped, not
 * queued (timestamps tell the core). A core without the tracker answers the first feed with
 * `bad_params` / `unsupported`: then [onFallback] runs once and the source feeds levels instead.
 * The `target` of every answer goes to [onTarget].
 */
class GridFeeder(
    private val core: CoreApi,
    val session: Any,
    private val sender: Executor,
    private val onTarget: (JSONObject?) -> Unit = {},
    private val onFallback: () -> Unit = {},
    private val maxFrames: Int = 8,
    private val maxSpanMs: Double = 100.0,
) {
    private val batch = ArrayList<JSONObject>(maxFrames)
    private val busy = AtomicBoolean(false)
    private var firstMs = 0.0
    @Volatile private var accepted = false
    @Volatile var fellBack = false; private set
    @Volatile var frames = 0L; private set
    @Volatile var dropped = 0L; private set
    @Volatile var feeds = 0L; private set
    @Volatile var lastError: String? = null; private set
    @Volatile var lastStatus: JSONObject? = null; private set

    /** One oriented grid (bytes copied into base64 now, so the caller may reuse its arrays). */
    @Synchronized
    fun frame(tMs: Double, w: Int, h: Int, max: ByteArray, mean: ByteArray?) {
        if (fellBack) return
        // A batch spans at most maxSpanMs: send it before adding a frame that would stretch it.
        if (batch.isNotEmpty() && tMs - firstMs > maxSpanMs && !busy.get()) flushLocked()
        if (batch.size >= maxFrames || (batch.isNotEmpty() && tMs - firstMs > maxSpanMs)) { dropped++; return } // core busy: drop, never queue
        val f = JSONObject().put("t_ms", Math.round(tMs * 10) / 10.0).put("w", w).put("h", h).put("max_b64", Base64Std.encode(max))
        if (mean != null) f.put("mean_b64", Base64Std.encode(mean))
        if (batch.isEmpty()) firstMs = tMs
        batch.add(f); frames++
        if (batch.size >= maxFrames && !busy.get()) flushLocked()
    }

    @Synchronized
    fun flush() { if (!busy.get()) flushLocked() }

    private fun flushLocked() {
        if (batch.isEmpty() || !busy.compareAndSet(false, true)) return
        val p = JSONObject().put("session", session).put("frames", JSONArray(batch.toList()))
        batch.clear()
        try { sender.execute { send(p) } } catch (_: Exception) { busy.set(false) }
    }

    private fun send(p: JSONObject) {
        try {
            val st = core.rpc("morse.rx.feed", p)
            accepted = true
            feeds++
            lastStatus = st
            lastError = null
            onTarget(st.optJSONObject("target"))
        } catch (e: HostError) {
            lastError = "${e.code}: ${e.message}"
            if (!accepted && (e.code == HostError.BAD_PARAMS || e.code == HostError.UNSUPPORTED || e.code == "not_implemented")) {
                fellBack = true
                synchronized(this) { batch.clear() }
                onFallback()
            }
        } finally {
            busy.set(false)
        }
        // Frames collected meanwhile go out now (in order).
        synchronized(this) { if (batch.isNotEmpty()) flushLocked() }
    }

    fun stats(): JSONObject = JSONObject().put("grid_frames", frames).put("grid_dropped", dropped).put("feeds", feeds)
        .put("fallback_levels", fellBack).put("error", lastError ?: JSONObject.NULL).apply { lastStatus?.let { put("status", it) } }

    companion object {
        /** RPC_API §12: a core with the tracker lists "morse_frames" in node.info.features. */
        fun coreHasFrames(core: CoreApi): Boolean = try {
            val f = core.rpc("node.info").optJSONArray("features")
            f != null && (0 until f.length()).any { f.optString(it) == "morse_frames" }
        } catch (_: HostError) { false }
    }
}
