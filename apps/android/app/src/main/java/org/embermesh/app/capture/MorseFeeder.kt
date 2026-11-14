package org.embermesh.app.capture

import android.util.Base64
import org.embermesh.app.core.CoreApi
import org.embermesh.app.host.HostError
import org.embermesh.app.host.rpc
import org.json.JSONArray
import org.json.JSONObject

/** A rectangle in 0..1 fractions of the camera frame (RPC_API §6 `roi`). */
data class Roi(val x: Double, val y: Double, val w: Double, val h: Double) {
    companion object {
        /** Default: the centre fifth of the frame. */
        val CENTRE = Roi(0.4, 0.4, 0.2, 0.2)

        fun from(o: JSONObject?): Roi {
            if (o == null) return CENTRE
            val x = o.optDouble("x", 0.4).coerceIn(0.0, 1.0)
            val y = o.optDouble("y", 0.4).coerceIn(0.0, 1.0)
            val w = o.optDouble("w", 0.2).coerceIn(0.01, 1.0 - x)
            val h = o.optDouble("h", 0.2).coerceIn(0.01, 1.0 - y)
            return Roi(x, y, w, h)
        }
    }
}

/** Mean brightness 0..1 of a region of an 8-bit luma (Y) plane. Pure; samples a grid of ≤ 64×64 points. */
object LumaMeter {
    fun average(y: java.nio.ByteBuffer, rowStride: Int, pixelStride: Int, width: Int, height: Int, roi: Roi): Double {
        val x0 = (roi.x * width).toInt().coerceIn(0, width - 1)
        val y0 = (roi.y * height).toInt().coerceIn(0, height - 1)
        val x1 = ((roi.x + roi.w) * width).toInt().coerceIn(x0 + 1, width)
        val y1 = ((roi.y + roi.h) * height).toInt().coerceIn(y0 + 1, height)
        val sx = maxOf(1, (x1 - x0) / 64)
        val sy = maxOf(1, (y1 - y0) / 64)
        var sum = 0L
        var n = 0
        val lim = y.limit()
        var r = y0
        while (r < y1) {
            var c = x0
            val base = r * rowStride
            while (c < x1) {
                val i = base + c * pixelStride
                if (i < lim) { sum += y.get(i).toInt() and 0xff; n++ }
                c += sx
            }
            r += sy
        }
        return if (n == 0) 0.0 else sum.toDouble() / n / 255.0
    }
}

/**
 * Batches capture samples into `morse.rx.feed` calls (light: per-frame times + levels; audio:
 * 16-bit PCM at the session rate). Flushes every [flushMs] of samples. Thread-safe.
 */
class MorseFeeder(private val core: CoreApi, val session: Any, private val flushMs: Long = 100) {
    private val times = ArrayList<Double>()
    private val levels = ArrayList<Double>()
    private var pcm = ShortArray(0)
    private var pcmN = 0
    @Volatile var samples = 0L; private set
    @Volatile var feeds = 0L; private set
    @Volatile var lastError: String? = null; private set
    @Volatile var lastStatus: JSONObject? = null; private set

    @Synchronized
    fun light(tMs: Double, level: Double) {
        times.add(tMs); levels.add(level.coerceIn(0.0, 1.0)); samples++
        if (times.last() - times.first() >= flushMs) flushLight()
    }

    @Synchronized
    fun flushLight() {
        if (times.isEmpty()) return
        val p = JSONObject().put("session", session).put("times_ms", JSONArray(times.map { Math.round(it * 10) / 10.0 }))
            .put("levels", JSONArray(levels.map { Math.round(it * 10000) / 10000.0 }))
        times.clear(); levels.clear()
        send(p)
    }

    /** [rate] must equal the session's sample_rate. */
    @Synchronized
    fun audio(buf: ShortArray, n: Int, rate: Int) {
        if (pcm.size < rate) pcm = ShortArray(rate)
        var off = 0
        while (off < n) {
            val k = minOf(n - off, pcm.size - pcmN)
            System.arraycopy(buf, off, pcm, pcmN, k)
            pcmN += k; off += k; samples += k
            if (pcmN >= rate * flushMs / 1000 || pcmN == pcm.size) flushAudio()
        }
    }

    @Synchronized
    fun flushAudio() {
        if (pcmN == 0) return
        val b = ByteArray(pcmN * 2)
        for (i in 0 until pcmN) { val v = pcm[i].toInt(); b[2 * i] = v.toByte(); b[2 * i + 1] = (v shr 8).toByte() }
        pcmN = 0
        send(JSONObject().put("session", session).put("pcm_b64", Base64.encodeToString(b, Base64.NO_WRAP)))
    }

    private fun send(p: JSONObject) {
        try {
            lastStatus = core.rpc("morse.rx.feed", p)
            feeds++
            lastError = null
        } catch (e: HostError) {
            lastError = "${e.code}: ${e.message}"
        }
    }

    fun stats(): JSONObject = JSONObject().put("samples", samples).put("feeds", feeds)
        .put("error", lastError ?: JSONObject.NULL).apply { lastStatus?.let { put("status", it) } }
}
