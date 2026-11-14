package org.embermesh.app.capture

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.ImageFormat
import android.graphics.SurfaceTexture
import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CaptureRequest
import android.media.ImageReader
import android.os.Handler
import android.os.HandlerThread
import android.os.Process
import android.util.Range
import android.util.Size
import android.view.Surface
import android.view.TextureView
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.json.JSONObject
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/**
 * `capture_camera_luma`: Camera2 → brightness of a region of each frame → morse.rx.feed.
 * The live preview is shown natively over the WebView at the CSS-pixel `rect` the UI passes (so the
 * operator can aim at the lamp); frames come from a small YUV ImageReader whose Y plane is averaged
 * over `roi`. Auto exposure runs for [settleMs], then exposure and white balance are locked so a
 * blinking light does not pump the exposure. Torch is forced off. Frame times are the sensor
 * timestamps (monotonic, ns → ms), passed through as `times_ms`.
 */
class CameraLumaSource(
    private val ctx: HostContext,
    private val feeder: MorseFeeder,
    @Volatile var roi: Roi,
    private val facing: String,
    @Volatile var rect: JSONObject?,
    private val settleMs: Long = 1500,
) {
    private val thread = HandlerThread("ember-camera", Process.THREAD_PRIORITY_DISPLAY).apply { start() }
    private val handler = Handler(thread.looper)
    private var device: CameraDevice? = null
    private var session: CameraCaptureSession? = null
    private var reader: ImageReader? = null
    private var preview: TextureView? = null
    private var previewSurface: Surface? = null
    @Volatile private var closed = false
    @Volatile var frames = 0L; private set
    @Volatile var aeLocked = false; private set
    @Volatile var lastLevel = 0.0; private set
    @Volatile var size: Size? = null; private set
    @Volatile var error: String? = null; private set
    private var firstTs = 0L
    private var lastTs = 0L

    /** Synthetic / test entry: one frame's Y plane. Same path the ImageReader uses. */
    fun processFrame(y: java.nio.ByteBuffer, rowStride: Int, pixelStride: Int, w: Int, h: Int, timestampNs: Long) {
        val level = LumaMeter.average(y, rowStride, pixelStride, w, h, roi)
        lastLevel = level
        frames++
        if (firstTs == 0L) firstTs = timestampNs
        lastTs = timestampNs
        feeder.light(timestampNs / 1_000_000.0, level)
    }

    val fps: Double get() = if (frames > 1 && lastTs > firstTs) (frames - 1) * 1e9 / (lastTs - firstTs) else 0.0

    @SuppressLint("MissingPermission")
    fun start() {
        val cm = ctx.app.getSystemService(Context.CAMERA_SERVICE) as CameraManager
        val want = if (facing == "front") CameraCharacteristics.LENS_FACING_FRONT else CameraCharacteristics.LENS_FACING_BACK
        val ids = cm.cameraIdList
        if (ids.isEmpty()) throw HostError.unsupported("no camera")
        val id = ids.firstOrNull { cm.getCameraCharacteristics(it).get(CameraCharacteristics.LENS_FACING) == want } ?: ids.first()
        val ch = cm.getCameraCharacteristics(id)
        val map = ch.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)
        val sizes = map?.getOutputSizes(ImageFormat.YUV_420_888)?.toList() ?: emptyList()
        val sz = sizes.filter { it.width <= 640 && it.height <= 480 }.maxByOrNull { it.width * it.height } ?: sizes.minByOrNull { it.width * it.height }
            ?: throw HostError(HostError.IO_ERROR, "camera has no YUV output")
        size = sz
        // Highest fixed frame rate available (30 typical): Morse timing resolution is the frame period.
        val fpsRange = ch.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES)
            ?.sortedWith(compareByDescending<Range<Int>> { it.lower }.thenByDescending { it.upper })?.firstOrNull()

        val r = ImageReader.newInstance(sz.width, sz.height, ImageFormat.YUV_420_888, 4)
        reader = r
        r.setOnImageAvailableListener({ rd ->
            val img = try { rd.acquireNextImage() } catch (_: Exception) { null } ?: return@setOnImageAvailableListener
            try {
                val p = img.planes[0]
                processFrame(p.buffer, p.rowStride, p.pixelStride, img.width, img.height, img.timestamp)
            } catch (t: Throwable) { error = t.toString() } finally { img.close() }
        }, handler)

        showPreview(sz)

        val opened = CountDownLatch(1)
        cm.openCamera(id, object : CameraDevice.StateCallback() {
            override fun onOpened(d: CameraDevice) { device = d; opened.countDown(); if (closed) d.close() else createSession(d, fpsRange) }
            override fun onDisconnected(d: CameraDevice) { error = "camera disconnected"; d.close(); opened.countDown() }
            override fun onError(d: CameraDevice, e: Int) { error = "camera error $e (in use by another app?)"; d.close(); opened.countDown() }
        }, handler)
        if (!opened.await(5, TimeUnit.SECONDS)) throw HostError(HostError.IO_ERROR, "camera did not open")
        error?.let { throw HostError(HostError.BUSY, it) }
    }

    private fun request(d: CameraDevice, fpsRange: Range<Int>?, lock: Boolean): CaptureRequest {
        val b = d.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW)
        reader?.surface?.let { b.addTarget(it) }
        previewSurface?.let { b.addTarget(it) }
        b.set(CaptureRequest.CONTROL_MODE, CaptureRequest.CONTROL_MODE_AUTO)
        b.set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_ON) // never auto-flash
        b.set(CaptureRequest.FLASH_MODE, CaptureRequest.FLASH_MODE_OFF)
        if (fpsRange != null) b.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, fpsRange)
        b.set(CaptureRequest.CONTROL_AE_LOCK, lock)
        b.set(CaptureRequest.CONTROL_AWB_LOCK, lock)
        return b.build()
    }

    @Suppress("DEPRECATION")
    private fun createSession(d: CameraDevice, fpsRange: Range<Int>?) {
        val targets = listOfNotNull(reader?.surface, previewSurface)
        try {
            d.createCaptureSession(targets, object : CameraCaptureSession.StateCallback() {
                override fun onConfigured(s: CameraCaptureSession) {
                    if (closed) { s.close(); return }
                    session = s
                    try { s.setRepeatingRequest(request(d, fpsRange, false), null, handler) } catch (t: Throwable) { error = t.toString(); return }
                    // Let auto exposure settle on the scene, then lock it.
                    handler.postDelayed({
                        if (!closed) try { s.setRepeatingRequest(request(d, fpsRange, true), null, handler); aeLocked = true } catch (t: Throwable) { error = t.toString() }
                    }, settleMs)
                }
                override fun onConfigureFailed(s: CameraCaptureSession) { error = "camera session failed" }
            }, handler)
        } catch (t: Throwable) { error = t.toString() }
    }

    /** Native preview at the CSS-pixel rect (only while the UI is in front). */
    private fun showPreview(sz: Size) {
        val ui = ctx.ui ?: return
        val rc = rect ?: return
        val latch = CountDownLatch(1)
        ctx.main.post {
            val tv = TextureView(ctx.app)
            tv.surfaceTextureListener = object : TextureView.SurfaceTextureListener {
                override fun onSurfaceTextureAvailable(st: SurfaceTexture, w: Int, h: Int) {
                    st.setDefaultBufferSize(sz.width, sz.height)
                    previewSurface = Surface(st)
                    latch.countDown()
                }
                override fun onSurfaceTextureSizeChanged(st: SurfaceTexture, w: Int, h: Int) {}
                override fun onSurfaceTextureDestroyed(st: SurfaceTexture) = true
                override fun onSurfaceTextureUpdated(st: SurfaceTexture) {}
            }
            preview = tv
            ui.setCaptureOverlay(tv, rc)
        }
        latch.await(2, TimeUnit.SECONDS) // no preview if the view never appears; capture still works
    }

    fun updateRect(r: JSONObject?) {
        rect = r
        val tv = preview ?: return
        ctx.main.post { ctx.ui?.setCaptureOverlay(if (r == null) null else tv, r) }
    }

    fun stop() {
        closed = true
        try { session?.close() } catch (_: Throwable) {}
        try { device?.close() } catch (_: Throwable) {}
        try { reader?.close() } catch (_: Throwable) {}
        session = null; device = null; reader = null
        if (preview != null) ctx.main.post { ctx.ui?.setCaptureOverlay(null, null) }
        try { previewSurface?.release() } catch (_: Throwable) {}
        preview = null; previewSurface = null
        thread.quitSafely()
        feeder.flushLight()
    }

    fun stats(): JSONObject = feeder.stats().put("frames", frames).put("fps", Math.round(fps * 10) / 10.0)
        .put("ae_locked", aeLocked).put("level", Math.round(lastLevel * 1000) / 1000.0)
        .put("size", size?.let { "${it.width}x${it.height}" } ?: JSONObject.NULL).put("camera_error", error ?: JSONObject.NULL)
}
