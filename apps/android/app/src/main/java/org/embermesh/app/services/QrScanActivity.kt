package org.embermesh.app.services

import android.app.Activity
import android.content.Intent
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import com.google.zxing.BarcodeFormat
import com.google.zxing.BinaryBitmap
import com.google.zxing.DecodeHintType
import com.google.zxing.LuminanceSource
import com.google.zxing.NotFoundException
import com.google.zxing.PlanarYUVLuminanceSource
import com.google.zxing.common.HybridBinarizer
import com.google.zxing.qrcode.QRCodeReader
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Camera QR scanner for host.qr.scan: android.hardware.Camera preview (works on every API level we
 * support) decoded with the vendored ZXing core. Returns EXTRA_TEXT on success.
 */
@Suppress("DEPRECATION")
class QrScanActivity : Activity(), SurfaceHolder.Callback, android.hardware.Camera.PreviewCallback {
    private var camera: android.hardware.Camera? = null
    private lateinit var surface: SurfaceView
    private val decoder = Executors.newSingleThreadExecutor()
    private val busy = AtomicBoolean(false)
    private val done = AtomicBoolean(false)
    private var torchOn = false
    private var previewW = 0
    private var previewH = 0
    private val reader = QRCodeReader()
    private val hints = mapOf(DecodeHintType.TRY_HARDER to true, DecodeHintType.POSSIBLE_FORMATS to listOf(BarcodeFormat.QR_CODE))

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val root = FrameLayout(this).apply { setBackgroundColor(Color.BLACK) }
        surface = SurfaceView(this)
        root.addView(surface, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        root.addView(TextView(this).apply {
            text = "Point the camera at a QR code"
            setTextColor(Color.WHITE); textSize = 18f; gravity = Gravity.CENTER
            setPadding(24, 96, 24, 24)
        }, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP))
        val bar = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER; setPadding(24, 24, 24, 72) }
        bar.addView(button("Torch") { toggleTorch() })
        bar.addView(button("Cancel") { setResult(RESULT_CANCELED); finish() })
        root.addView(bar, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM))
        setContentView(root)
        surface.holder.addCallback(this)
    }

    private fun button(label: String, onClick: () -> Unit) = TextView(this).apply {
        text = label; setTextColor(Color.WHITE); textSize = 18f
        setPadding(48, 24, 48, 24)
        setBackgroundColor(Color.argb(0x80, 0x30, 0x30, 0x30))
        layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { setMargins(24, 0, 24, 0) }
        setOnClickListener { onClick() }
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        try {
            val id = (0 until android.hardware.Camera.getNumberOfCameras()).firstOrNull {
                val info = android.hardware.Camera.CameraInfo(); android.hardware.Camera.getCameraInfo(it, info)
                info.facing == android.hardware.Camera.CameraInfo.CAMERA_FACING_BACK
            } ?: 0
            val cam = android.hardware.Camera.open(id)
            camera = cam
            val p = cam.parameters
            val size = p.supportedPreviewSizes.minByOrNull { kotlin.math.abs(it.width * it.height - 1280 * 720) }
            if (size != null) p.setPreviewSize(size.width, size.height)
            when {
                p.supportedFocusModes.contains(android.hardware.Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE) ->
                    p.focusMode = android.hardware.Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE
                p.supportedFocusModes.contains(android.hardware.Camera.Parameters.FOCUS_MODE_AUTO) ->
                    p.focusMode = android.hardware.Camera.Parameters.FOCUS_MODE_AUTO
            }
            cam.parameters = p
            previewW = cam.parameters.previewSize.width
            previewH = cam.parameters.previewSize.height
            cam.setDisplayOrientation(displayOrientation(id))
            cam.setPreviewDisplay(holder)
            val buf = ByteArray(previewW * previewH * 3 / 2)
            cam.addCallbackBuffer(buf)
            cam.addCallbackBuffer(buf.copyOf())
            cam.setPreviewCallbackWithBuffer(this)
            cam.startPreview()
        } catch (e: Exception) {
            setResult(RESULT_CANCELED, Intent().putExtra(EXTRA_ERROR, "camera unavailable: ${e.message}"))
            finish()
        }
    }

    private fun displayOrientation(id: Int): Int {
        val info = android.hardware.Camera.CameraInfo()
        android.hardware.Camera.getCameraInfo(id, info)
        val deg = when (windowManager.defaultDisplay.rotation) { Surface.ROTATION_90 -> 90; Surface.ROTATION_180 -> 180; Surface.ROTATION_270 -> 270; else -> 0 }
        return (info.orientation - deg + 360) % 360
    }

    override fun onPreviewFrame(data: ByteArray, cam: android.hardware.Camera) {
        if (done.get() || !busy.compareAndSet(false, true)) { cam.addCallbackBuffer(data); return }
        decoder.execute {
            val text = decode(data, previewW, previewH)
            busy.set(false)
            if (text != null && done.compareAndSet(false, true)) {
                runOnUiThread { setResult(RESULT_OK, Intent().putExtra(EXTRA_TEXT, text)); finish() }
            } else {
                try { cam.addCallbackBuffer(data) } catch (_: Exception) {}
            }
        }
    }

    private fun decode(data: ByteArray, w: Int, h: Int): String? {
        if (w <= 0 || h <= 0) return null
        val side = (minOf(w, h) * 0.8).toInt()
        val src: LuminanceSource = try { PlanarYUVLuminanceSource(data, w, h, (w - side) / 2, (h - side) / 2, side, side, false) } catch (_: Exception) { return null }
        for (s in listOf(src, src.invert())) {
            try { return reader.decode(BinaryBitmap(HybridBinarizer(s)), hints).text } catch (_: NotFoundException) {
            } catch (_: Exception) {
            } finally { reader.reset() }
        }
        return null
    }

    private fun toggleTorch() {
        val cam = camera ?: return
        try {
            val p = cam.parameters
            if (p.supportedFlashModes?.contains(android.hardware.Camera.Parameters.FLASH_MODE_TORCH) != true) return
            torchOn = !torchOn
            p.flashMode = if (torchOn) android.hardware.Camera.Parameters.FLASH_MODE_TORCH else android.hardware.Camera.Parameters.FLASH_MODE_OFF
            cam.parameters = p
        } catch (_: Exception) {}
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {}
    override fun surfaceDestroyed(holder: SurfaceHolder) = releaseCamera()

    private fun releaseCamera() {
        camera?.let { c -> try { c.setPreviewCallbackWithBuffer(null); c.stopPreview(); c.release() } catch (_: Exception) {} }
        camera = null
    }

    override fun onPause() {
        super.onPause()
        if (isFinishing) releaseCamera()
    }

    override fun onDestroy() {
        releaseCamera()
        decoder.shutdownNow()
        super.onDestroy()
    }

    companion object {
        const val EXTRA_TEXT = "text"
        const val EXTRA_ERROR = "error"
    }
}
