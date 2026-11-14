package org.embermesh.app.services

import org.json.JSONObject
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

/** Pure helpers behind the device services (unit-tested). */
object ServiceLogic {
    /** HTML-style accept string (".json,application/json,image/png") → MIME types for EXTRA_MIME_TYPES. */
    fun acceptToMimes(accept: String?): List<String> {
        if (accept.isNullOrBlank()) return emptyList()
        val out = LinkedHashSet<String>()
        for (raw in accept.split(',')) {
            val a = raw.trim().lowercase()
            if (a.isEmpty()) continue
            if (a.contains('/')) { out.add(a); continue }
            val ext = a.removePrefix(".")
            out.add(EXT_MIME[ext] ?: "application/octet-stream")
        }
        if (out.contains("*/*")) return emptyList()
        return out.toList()
    }

    private val EXT_MIME = mapOf(
        "json" to "application/json", "txt" to "text/plain", "md" to "text/markdown", "csv" to "text/csv",
        "png" to "image/png", "jpg" to "image/jpeg", "jpeg" to "image/jpeg", "gif" to "image/gif", "webp" to "image/webp",
        "pdf" to "application/pdf", "zip" to "application/zip", "apk" to "application/vnd.android.package-archive",
        "html" to "text/html", "wav" to "audio/wav", "mp3" to "audio/mpeg", "pack" to "application/octet-stream",
        "ember" to "application/octet-stream", "bin" to "application/octet-stream",
    )

    /** Position object per RPC_API (WGS-84 degrees, metres, ms). */
    fun position(lat: Double, lon: Double, alt: Double?, accuracy: Float?, ts: Long): JSONObject {
        require(lat in -90.0..90.0 && lon in -180.0..180.0) { "bad coordinates" }
        val o = JSONObject().put("lat", lat).put("lon", lon).put("ts", ts)
        if (alt != null && !alt.isNaN()) o.put("alt", alt)
        if (accuracy != null && !accuracy.isNaN() && accuracy > 0) o.put("accuracy", accuracy.toDouble())
        return o
    }

    /**
     * Writes [count] samples of a sine tone starting at sample [start] of a burst of [total] samples,
     * with raised-cosine ramps of [ramp] samples at both ends (no clicks).
     */
    fun sineChunk(out: ShortArray, count: Int, start: Int, total: Int, rate: Int, freq: Int, ramp: Int, amplitude: Double = 0.6) {
        val w = 2 * PI * freq / rate
        for (i in 0 until count) {
            val n = start + i
            var env = 1.0
            if (ramp > 0) {
                if (n < ramp) env = 0.5 - 0.5 * cos(PI * n / ramp)
                else if (n >= total - ramp) env = 0.5 - 0.5 * cos(PI * (total - n).coerceAtLeast(0) / ramp)
            }
            out[i] = (sin(w * n) * env * amplitude * 32767).toInt().coerceIn(-32768, 32767).toShort()
        }
    }

    fun totalMs(pattern: LongArray): Long = pattern.sum()

    /** Sanitised display name for a picked / saved file. */
    fun cleanName(name: String?, fallback: String): String {
        val base = (name ?: "").substringAfterLast('/').substringAfterLast('\\')
        val c = base.filter { it >= ' ' && it !in "<>:\"|?*" }.trim().trimStart('.')
        return c.take(120).ifEmpty { fallback }
    }
}
