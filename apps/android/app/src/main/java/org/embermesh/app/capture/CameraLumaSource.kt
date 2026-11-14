package org.embermesh.app.capture

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.ImageFormat
import android.graphics.Rect
import android.graphics.SurfaceTexture
import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CaptureRequest
import android.media.ImageReader
import android.os.Build
import android.os.Handler
import android.os.HandlerThread
import android.os.Process
import android.util.Range
import android.util.Size
import android.view.Surface
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

/**
 * `capture_camera_luma` (RPC_API §6, §12): Camera2 → a whole-frame max/mean brightness grid of
 * every frame (from the full-resolution Y plane, see [LumaGrid]) → `morse.rx.feed {frames}`.
 * The core's tracker finds the blinking light; nothing here looks for it. A core without the
 * tracker refuses the first frames feed and the source falls back to the section-6 centre-region
 * levels for the session.
 *
 * Camera settings (§12): rear camera with the widest lens, 60 fps when a ≥ 1280×720 YUV size runs at
 * 60 (else 30), continuous-video autofocus (or fixed at infinity), exposure biased down 1 EV and
 * locked while the tracker reports `locked`, white balance locked after [settleMs], torch off, no
 * scene modes (HDR/night), no video stabilisation. Zoom steps 1/2/5 where the camera has them.
 * Frame times are sensor timestamps (monotonic, ns → ms). Frames never leave this class except as
 * grids; nothing is stored or logged.
 */
class CameraLumaSource(
    private val ctx: HostContext,
    private val levels: MorseFeeder,
    private val grids: GridFeeder?,
    private val facing: String,
    @Volatile var rect: JSONObject?,
    private val settleMs: Long = 1500,
    /** The capture stopped by itself (camera error, disconnect). */
    private val onFailure: (String) -> Unit = {},
) {
    private val thread = HandlerThread("ember-camera", Process.THREAD_PRIORITY_DISPLAY).apply { start() }
    private val handler = Handler(thread.looper)
    private var device: CameraDevice? = null
    private var session: CameraCaptureSession? = null
    private var reader: ImageReader? = null
    private var overlay: PreviewOverlay? = null
    private var previewSurface: Surface? = null
    private var chars: CameraCharacteristics? = null
    private var fpsRange: Range<Int>? = null
    @Volatile private var closed = false
    @Volatile var frames = 0L; private set
    @Volatile var aeLocked = false; private set
    @Volatile var awbLocked = false; private set
    @Volatile var lastLevel = 0.0; private set
    @Volatile var size: Size? = null; private set
    @Volatile var error: String? = null; private set
    @Volatile var zoom = 1.0; private set
    @Volatile var zoomSteps: List<Double> = listOf(1.0); private set
    @Volatile var targetState = "searching"; private set
    /** Sensor orientation and lens facing (for the grid's rotation/mirroring). */
    @Volatile private var sensorOrientation = 0
    @Volatile private var front = facing == "front"
    /** The display rotation in degrees (0/90/180/270), refreshed from the preview's display. */
    @Volatile var displayDegrees = 0
    private var firstTs = 0L
    private var lastTs = 0L
    private var grid: LumaGrid? = null
    private var maxOut = ByteArray(0)
    private var meanOut = ByteArray(0)
    @Volatile private var reduceNsTotal = 0L
    @Volatile private var reduceNsMax = 0L

    /** Synthetic / test entry: one frame's Y plane. The ImageReader path calls exactly this. */
    fun processFrame(y: java.nio.ByteBuffer, rowStride: Int, pixelStride: Int, w: Int, h: Int, timestampNs: Long) {
        val t0 = System.nanoTime()
        val g = grid?.takeIf { it.width == w && it.height == h } ?: LumaGrid(w, h).also {
            grid = it; maxOut = ByteArray(it.gw * it.gh); meanOut = ByteArray(it.gw * it.gh)
        }
        g.reduce(y, rowStride, pixelStride)
        val rot = LumaGrid.rotation(sensorOrientation, displayDegrees, front)
        val tMs = timestampNs / 1_000_000.0
        frames++
        if (firstTs == 0L) firstTs = timestampNs
        lastTs = timestampNs
        if (grids != null && !grids.fellBack) {
            g.orient(rot, front, maxOut, meanOut)
            grids.frame(tMs, g.shownW(rot), g.shownH(rot), maxOut, meanOut)
        } else {
            val level = g.centreLevel()
            lastLevel = level
            levels.light(tMs, level)
        }
        val dt = System.nanoTime() - t0
        reduceNsTotal += dt
        if (dt > reduceNsMax) reduceNsMax = dt
    }

    val fps: Double get() = if (frames > 1 && lastTs > firstTs) (frames - 1) * 1e9 / (lastTs - firstTs) else 0.0

    /** The tracker's target (from each feed answer): AE locks while it is `locked`; the ring follows it. */
    fun onTarget(t: JSONObject?) {
        val st = t?.optString("state", "searching") ?: "searching"
        overlay?.let { o -> ctx.main.post { o.ring(t) } }
        if (st == targetState) return
        targetState = st
        handler.post { applyRequest() }
    }

    /** The section-6 fallback took over (a core without the tracker): lock AE as before. */
    fun onFallback() { handler.post { applyRequest() } }

    /** host.capture.start result: zoom steps (RPC_API §12) and the frame size the grid covers. */
    fun startResult(): JSONObject = JSONObject()
        .put("zoom_steps", JSONArray(zoomSteps.map { if (it == Math.floor(it)) it.toInt() else it }))
        .apply { size?.let { s -> put("frame", JSONObject().put("w", s.width).put("h", s.height)) } }

    @SuppressLint("MissingPermission")
    fun start() {
        val cm = ctx.app.getSystemService(Context.CAMERA_SERVICE) as CameraManager
        val ids = cm.cameraIdList
        if (ids.isEmpty()) throw HostError.unsupported("no camera")
        val id = pickCamera(cm, ids)
        val ch = cm.getCameraCharacteristics(id)
        chars = ch
        sensorOrientation = ch.get(CameraCharacteristics.SENSOR_ORIENTATION) ?: 0
        front = ch.get(CameraCharacteristics.LENS_FACING) == CameraCharacteristics.LENS_FACING_FRONT
        displayDegrees = currentDisplayDegrees()
        val map = ch.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP) ?: throw HostError(HostError.IO_ERROR, "camera has no outputs")
        val ranges = ch.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES)?.toList() ?: emptyList()
        val yuv = map.getOutputSizes(ImageFormat.YUV_420_888)?.toList() ?: emptyList()
        if (yuv.isEmpty()) throw HostError(HostError.IO_ERROR, "camera has no YUV output")
        val pick = CameraChoice.pick(yuv.map { it.width to it.height }, { w, h -> map.getOutputMinFrameDuration(ImageFormat.YUV_420_888, Size(w, h)) },
            ranges.map { it.lower to it.upper }, rectAspect())
        val sz = Size(pick.w, pick.h)
        size = sz
        fpsRange = pick.fps?.let { Range(it.first, it.second) }
        zoomSteps = zoomStepsOf(ch)

        val r = ImageReader.newInstance(sz.width, sz.height, ImageFormat.YUV_420_888, 3)
        reader = r
        r.setOnImageAvailableListener({ rd ->
            // acquireLatestImage drops frames we could not keep up with instead of queueing them.
            val img = try { rd.acquireLatestImage() } catch (_: Exception) { null } ?: return@setOnImageAvailableListener
            try {
                val p = img.planes[0]
                processFrame(p.buffer, p.rowStride, p.pixelStride, img.width, img.height, img.timestamp)
            } catch (t: Throwable) { error = t.toString() } finally { img.close() }
        }, handler)

        val prevSize = map.getOutputSizes(SurfaceTexture::class.java)?.filter { it.width * sz.height == it.height * sz.width && it.width <= 1920 }
            ?.maxByOrNull { it.width } ?: sz
        showPreview(prevSize)

        val opened = CountDownLatch(1)
        cm.openCamera(id, object : CameraDevice.StateCallback() {
            override fun onOpened(d: CameraDevice) { device = d; opened.countDown(); if (closed) d.close() else createSession(d) }
            override fun onDisconnected(d: CameraDevice) { fail("camera disconnected"); d.close(); opened.countDown() }
            override fun onError(d: CameraDevice, e: Int) { fail("camera error $e (in use by another app?)"); d.close(); opened.countDown() }
        }, handler)
        if (!opened.await(5, TimeUnit.SECONDS)) throw HostError(HostError.IO_ERROR, "camera did not open")
        error?.let { throw HostError(HostError.BUSY, it) }
    }

    private fun fail(msg: String) {
        val first = error == null
        error = msg
        if (first && !closed) onFailure(msg)
    }

    /** Rear (or front) camera with the widest lens (smallest focal length). */
    private fun pickCamera(cm: CameraManager, ids: Array<String>): String {
        val want = if (facing == "front") CameraCharacteristics.LENS_FACING_FRONT else CameraCharacteristics.LENS_FACING_BACK
        val same = ids.filter { cm.getCameraCharacteristics(it).get(CameraCharacteristics.LENS_FACING) == want }
        if (same.isEmpty()) return ids.first()
        return same.minByOrNull { cm.getCameraCharacteristics(it).get(CameraCharacteristics.LENS_INFO_AVAILABLE_FOCAL_LENGTHS)?.minOrNull() ?: Float.MAX_VALUE }!!
    }

    private fun zoomStepsOf(ch: CameraCharacteristics): List<Double> {
        val maxZoom = if (Build.VERSION.SDK_INT >= 30) ch.get(CameraCharacteristics.CONTROL_ZOOM_RATIO_RANGE)?.upper?.toDouble()
            ?: ch.get(CameraCharacteristics.SCALER_AVAILABLE_MAX_DIGITAL_ZOOM)?.toDouble()
            else ch.get(CameraCharacteristics.SCALER_AVAILABLE_MAX_DIGITAL_ZOOM)?.toDouble()
        return listOf(1.0, 2.0, 5.0).filter { it <= (maxZoom ?: 1.0) + 1e-6 }
    }

    /** host.capture.update {zoom}: one of [zoomSteps]. */
    fun setZoom(z: Double) {
        if (zoomSteps.none { Math.abs(it - z) < 1e-6 }) throw HostError.badParams("zoom must be one of $zoomSteps")
        zoom = z
        handler.post { applyRequest() }
    }

    private fun rectAspect(): Double? {
        val r = rect ?: return null
        val w = r.optDouble("w", 0.0); val h = r.optDouble("h", 0.0)
        if (w <= 0 || h <= 0) return null
        // The aspect the sensor frame must have to fill the rect unstretched.
        val rot = LumaGrid.rotation(sensorOrientation, displayDegrees, front)
        return if (rot == 90 || rot == 270) h / w else w / h
    }

    private fun request(d: CameraDevice): CaptureRequest {
        val ch = chars
        val b = d.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW)
        reader?.surface?.let { b.addTarget(it) }
        previewSurface?.let { b.addTarget(it) }
        b.set(CaptureRequest.CONTROL_MODE, CaptureRequest.CONTROL_MODE_AUTO)
        b.set(CaptureRequest.CONTROL_SCENE_MODE, CaptureRequest.CONTROL_SCENE_MODE_DISABLED) // no HDR / night
        b.set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_ON) // never auto-flash
        b.set(CaptureRequest.FLASH_MODE, CaptureRequest.FLASH_MODE_OFF)
        b.set(CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE, CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE_OFF)
        fpsRange?.let { b.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, it) }
        val afModes = ch?.get(CameraCharacteristics.CONTROL_AF_AVAILABLE_MODES)?.toList() ?: emptyList()
        if (CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO in afModes) b.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO)
        else if ((ch?.get(CameraCharacteristics.LENS_INFO_MINIMUM_FOCUS_DISTANCE) ?: 0f) > 0f) {
            b.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_OFF)
            b.set(CaptureRequest.LENS_FOCUS_DISTANCE, 0f) // infinity
        }
        // Exposure biased down about 1 EV: a bright point must not bloom.
        val step = ch?.get(CameraCharacteristics.CONTROL_AE_COMPENSATION_STEP)?.toDouble() ?: 0.0
        val evRange = ch?.get(CameraCharacteristics.CONTROL_AE_COMPENSATION_RANGE)
        if (step > 0 && evRange != null) b.set(CaptureRequest.CONTROL_AE_EXPOSURE_COMPENSATION, Math.round(-1.0 / step).toInt().coerceIn(evRange.lower, evRange.upper))
        // AE locked while the tracker reads a light (a core without the tracker: after settling).
        val lockAe = if (grids != null && !grids.fellBack) targetState == "locked" else awbLocked
        b.set(CaptureRequest.CONTROL_AE_LOCK, lockAe)
        b.set(CaptureRequest.CONTROL_AWB_LOCK, awbLocked)
        aeLocked = lockAe
        if (zoom > 1.0 && ch != null) {
            if (Build.VERSION.SDK_INT >= 30 && ch.get(CameraCharacteristics.CONTROL_ZOOM_RATIO_RANGE) != null) b.set(CaptureRequest.CONTROL_ZOOM_RATIO, zoom.toFloat())
            else ch.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE)?.let { a ->
                val w = (a.width() / zoom).toInt(); val h = (a.height() / zoom).toInt()
                b.set(CaptureRequest.SCALER_CROP_REGION, Rect((a.width() - w) / 2, (a.height() - h) / 2, (a.width() + w) / 2, (a.height() + h) / 2))
            }
        }
        return b.build()
    }

    private fun applyRequest() {
        val d = device ?: return
        val s = session ?: return
        if (closed) return
        try { s.setRepeatingRequest(request(d), null, handler) } catch (t: Throwable) { error = t.toString() }
    }

    @Suppress("DEPRECATION")
    private fun createSession(d: CameraDevice) {
        val targets = listOfNotNull(reader?.surface, previewSurface)
        try {
            d.createCaptureSession(targets, object : CameraCaptureSession.StateCallback() {
                override fun onConfigured(s: CameraCaptureSession) {
                    if (closed) { s.close(); return }
                    session = s
                    applyRequest()
                    // White balance settles on the scene, then stays locked (a coloured flash must not shift it).
                    handler.postDelayed({ if (!closed) { awbLocked = true; applyRequest() } }, settleMs)
                }
                override fun onConfigureFailed(s: CameraCaptureSession) { fail("camera session failed") }
            }, handler)
        } catch (t: Throwable) { fail(t.toString()) }
    }

    private fun currentDisplayDegrees(): Int = try {
        @Suppress("DEPRECATION")
        val rot = (ctx.app.getSystemService(Context.WINDOW_SERVICE) as android.view.WindowManager).defaultDisplay.rotation
        rot * 90
    } catch (_: Throwable) { 0 }

    /** Native preview inside the CSS-pixel rect (only while the UI is in front). */
    private fun showPreview(sz: Size) {
        val ui = ctx.ui ?: return
        val rc = rect ?: return
        val latch = CountDownLatch(1)
        ctx.main.post {
            val o = PreviewOverlay(ctx.app, sz.width, sz.height) { deg -> displayDegrees = deg }
            o.rectCss = rc
            o.onSurface = { st -> st.setDefaultBufferSize(sz.width, sz.height); previewSurface = Surface(st); latch.countDown() }
            overlay = o
            ui.setCaptureOverlay(o, rc)
        }
        latch.await(2, TimeUnit.SECONDS) // no preview if the view never appears; capture still works
    }

    fun updateRect(r: JSONObject?) {
        rect = r
        val o = overlay ?: return
        ctx.main.post { if (r != null) o.rectCss = r; ctx.ui?.setCaptureOverlay(if (r == null) null else o, r) }
    }

    /** The overlay view (tests). */
    val previewView: PreviewOverlay? get() = overlay

    fun stop() {
        if (closed) return
        closed = true
        grids?.flush()
        val done = CountDownLatch(1)
        handler.post {
            try { session?.close() } catch (_: Throwable) {}
            try { device?.close() } catch (_: Throwable) {}
            try { reader?.close() } catch (_: Throwable) {}
            session = null; device = null; reader = null
            done.countDown()
        }
        done.await(2, TimeUnit.SECONDS)
        if (overlay != null) ctx.main.post { ctx.ui?.setCaptureOverlay(null, null) }
        try { previewSurface?.release() } catch (_: Throwable) {}
        overlay = null; previewSurface = null
        thread.quitSafely()
        levels.flushLight()
    }

    fun stats(): JSONObject {
        val js = levels.stats()
        grids?.stats()?.let { g -> g.keys().forEach { k -> js.put(k, g.get(k)) } }
        return js.put("frames", frames).put("fps", Math.round(fps * 10) / 10.0)
            .put("ae_locked", aeLocked).put("awb_locked", awbLocked).put("level", Math.round(lastLevel * 1000) / 1000.0)
            .put("size", size?.let { "${it.width}x${it.height}" } ?: JSONObject.NULL).put("camera_error", error ?: JSONObject.NULL)
            .put("fps_range", fpsRange?.toString() ?: JSONObject.NULL).put("zoom", zoom).put("target_state", targetState)
            .put("grid", grid?.let { "${it.gw}x${it.gh}" } ?: JSONObject.NULL)
            .put("reduce_ms_mean", if (frames > 0) Math.round(reduceNsTotal.toDouble() / frames / 1e4) / 100.0 else 0.0)
            .put("reduce_ms_max", Math.round(reduceNsMax / 1e4) / 100.0)
    }

    companion object {
        /** One sender thread for all grid feeds (never the camera thread). */
        val sender: java.util.concurrent.ExecutorService = Executors.newSingleThreadExecutor(HostContext.named("ember-grid"))
    }
}

/** Camera size / frame-rate choice (§12), pure. */
object CameraChoice {
    data class Pick(val w: Int, val h: Int, val fps: Pair<Int, Int>?)

    /** Pixel cap: every pixel is read on the CPU each frame (cost measured, see the README). */
    const val MAX_PIXELS = 1920 * 1080

    /**
     * 60 fps when a 60 fps range exists and a ≥ 1280×720 size runs at 60, else 30 (as on iOS).
     * Size: the largest ≤ [MAX_PIXELS] that runs at that rate, preferring the aspect [aspect]
     * (sensor-oriented width/height of the page's rectangle) within 2 %.
     */
    fun pick(sizes: List<Pair<Int, Int>>, minFrameNs: (Int, Int) -> Long, ranges: List<Pair<Int, Int>>, aspect: Double?): Pick {
        fun runsAt(s: Pair<Int, Int>, fps: Int) = minFrameNs(s.first, s.second).let { it <= 0 || it <= 1_000_000_000L / fps + 200_000 }
        val capped = sizes.filter { it.first * it.second <= MAX_PIXELS }.ifEmpty { listOf(sizes.minByOrNull { it.first * it.second }!!) }
        val r60 = ranges.filter { it.second >= 60 && it.first >= 30 }.maxByOrNull { it.first }
        val big60 = capped.filter { it.first * it.second >= 1280 * 720 && runsAt(it, 60) }
        val use60 = r60 != null && big60.isNotEmpty()
        val range = if (use60) r60 else (ranges.firstOrNull { it.first == 30 && it.second == 30 }
            ?: ranges.filter { it.second >= 30 }.minByOrNull { Math.abs(it.second - 30) * 10 + (30 - it.first).coerceAtLeast(0) }
            ?: ranges.maxByOrNull { it.second })
        val ok = if (use60) big60 else capped.filter { runsAt(it, 30) }.ifEmpty { capped }
        val pool = if (aspect != null) ok.filter { Math.abs(it.first.toDouble() / it.second / aspect - 1) < 0.02 }.ifEmpty { ok } else ok
        val best = pool.maxByOrNull { it.first.toLong() * it.second }!!
        return Pick(best.first, best.second, range)
    }
}
