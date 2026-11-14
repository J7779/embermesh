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
                ensurePermissions(listOf(Manifest.permission.CAMERA)) // asked when the user starts reading
                camera?.stop(); camera = null
                var src: CameraLumaSource? = null
                // §12: whole-frame grids for the core's tracker; a core without it refuses the first
                // feed and the source falls back to the section-6 centre region.
                val grids = GridFeeder(core, session, CameraLumaSource.sender, { t -> src?.onTarget(t) }, { src?.onFallback() })
                val s = CameraLumaSource(ctx, MorseFeeder(core, session), grids, p.str("camera", "back"), p.optJSONObject("rect")) { why ->
                    ctx.io.execute { stopCamera(why) }
                }
                src = s
                try { s.start() } catch (t: Throwable) { s.stop(); throw t }
                camera = s
                return s.startResult()
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
            if (p.has("rect")) c.updateRect(p.optJSONObject("rect"))
            if (p.has("zoom")) c.setZoom(p.optDouble("zoom", 1.0)) // RPC_API §12; roi is obsolete (whole frame)
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

    /**
     * The camera capture stopped by itself or must stop (app in the background, screen locked,
     * camera lost): release the camera now and tell the page (`host.capture.state`, RPC_API §12).
     */
    @Synchronized
    fun stopCamera(why: String) {
        val c = camera ?: return
        camera = null
        c.stop()
        ctx.emit("host.capture.state", JSONObject().put("kind", "camera_luma").put("running", false).put("error", why))
    }

    /** The running camera source (tests). */
    fun cameraSource(): CameraLumaSource? = camera

    /** This app holds a camera right now (the torch cannot be switched then). */
    fun cameraActive(): Boolean = camera != null

    @Synchronized
    fun stopAll() { camera?.stop(); camera = null; mic?.stop(); mic = null }

    /** host.capture.status (Android extension): counters for diagnostics and tests. */
    @Synchronized
    fun status(): JSONObject = JSONObject()
        .put("camera_luma", camera?.stats() ?: JSONObject.NULL)
        .put("mic_pcm", mic?.stats() ?: JSONObject.NULL)
}
