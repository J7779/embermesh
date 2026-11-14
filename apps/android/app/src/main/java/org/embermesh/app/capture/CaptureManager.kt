package org.embermesh.app.capture

import android.Manifest
import android.content.pm.PackageManager
import org.embermesh.app.core.CoreApi
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.embermesh.app.util.int
import org.embermesh.app.util.str
import org.json.JSONObject

/**
 * Native Morse capture sources (RPC_API §6): host.capture.start/update/stop/status for
 * `camera_luma` and `mic_pcm`. One source of each kind at a time; each feeds `morse.rx.feed` itself.
 */
class CaptureManager(private val ctx: HostContext, private val core: CoreApi, private val ensurePermissions: (List<String>) -> Unit) {
    private var camera: CameraLumaSource? = null
    private var mic: MicPcmSource? = null

    fun features(): List<String> {
        if (!ctx.hasContext) return emptyList()
        val pm = ctx.app.packageManager
        val f = ArrayList<String>()
        if (pm.hasSystemFeature(PackageManager.FEATURE_CAMERA_ANY)) f.add("capture_camera_luma")
        if (pm.hasSystemFeature(PackageManager.FEATURE_MICROPHONE)) f.add("capture_mic_pcm")
        return f
    }

    @Synchronized
    fun start(p: JSONObject): JSONObject {
        val session = p.opt("session") ?: throw HostError.badParams("session required")
        when (val kind = p.str("kind")) {
            "camera_luma" -> {
                ensurePermissions(listOf(Manifest.permission.CAMERA))
                camera?.stop(); camera = null
                val src = CameraLumaSource(ctx, MorseFeeder(core, session), Roi.from(p.optJSONObject("roi")), p.str("camera", "back"), p.optJSONObject("rect"))
                try { src.start() } catch (t: Throwable) { src.stop(); throw t }
                camera = src
            }
            "mic_pcm" -> {
                ensurePermissions(listOf(Manifest.permission.RECORD_AUDIO))
                mic?.stop(); mic = null
                val src = MicPcmSource(MorseFeeder(core, session), p.int("sample_rate", 8000).coerceIn(8000, 48000))
                src.start()
                mic = src
            }
            else -> throw HostError.badParams("kind must be camera_luma or mic_pcm (got '$kind')")
        }
        return JSONObject()
    }

    @Synchronized
    fun update(p: JSONObject): JSONObject {
        if (p.str("kind") == "camera_luma") {
            val c = camera ?: throw HostError(HostError.NOT_FOUND, "camera capture not running")
            if (p.has("roi")) c.roi = Roi.from(p.optJSONObject("roi"))
            if (p.has("rect")) c.updateRect(p.optJSONObject("rect"))
        }
        return JSONObject()
    }

    @Synchronized
    fun stop(p: JSONObject): JSONObject {
        when (p.str("kind")) {
            "camera_luma" -> { camera?.stop(); camera = null }
            "mic_pcm" -> { mic?.stop(); mic = null }
            "" -> stopAll()
        }
        return JSONObject()
    }

    @Synchronized
    fun stopAll() { camera?.stop(); camera = null; mic?.stop(); mic = null }

    /** host.capture.status (Android extension): counters for diagnostics and tests. */
    @Synchronized
    fun status(): JSONObject = JSONObject()
        .put("camera_luma", camera?.stats() ?: JSONObject.NULL)
        .put("mic_pcm", mic?.stats() ?: JSONObject.NULL)
}
