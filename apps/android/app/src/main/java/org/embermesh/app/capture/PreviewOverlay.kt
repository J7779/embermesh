package org.embermesh.app.capture

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.SurfaceTexture
import android.view.MotionEvent
import android.view.Surface
import android.view.TextureView
import android.view.View
import android.view.ViewGroup
import android.webkit.WebView
import android.widget.FrameLayout
import org.json.JSONObject

/**
 * The native camera preview of `capture_camera_luma` (RPC_API §12 "Native preview"): the whole
 * camera frame drawn exactly into the page's rectangle, clipped to the web view, upright for the
 * current display rotation, and on top of it the marker ring at the tracker's `target` — the only
 * thing the host draws. Not interactive: every touch falls through to the web view, which turns a
 * tap in the rectangle into `morse.rx.target`.
 */
@SuppressLint("ViewConstructor")
class PreviewOverlay(
    context: Context,
    private val bufW: Int,
    private val bufH: Int,
    /** Display rotation in degrees whenever the view is laid out (the grid follows it). */
    private val onRotation: (Int) -> Unit,
) : FrameLayout(context) {
    var onSurface: ((SurfaceTexture) -> Unit)? = null
    val texture = TextureView(context)
    val ringView = RingView(context)

    /** The page's rectangle (CSS px of the web view's visible viewport), set by the capture source. */
    @Volatile var rectCss: JSONObject? = null
        set(v) { field = v; post { fixPlacement(); requestLayout() } }

    init {
        isClickable = false; isFocusable = false
        clipChildren = true
        importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
        texture.isClickable = false
        texture.surfaceTextureListener = object : TextureView.SurfaceTextureListener {
            override fun onSurfaceTextureAvailable(st: SurfaceTexture, w: Int, h: Int) { onSurface?.invoke(st); onSurface = null; applyTransform() }
            override fun onSurfaceTextureSizeChanged(st: SurfaceTexture, w: Int, h: Int) = applyTransform()
            override fun onSurfaceTextureDestroyed(st: SurfaceTexture) = true
            override fun onSurfaceTextureUpdated(st: SurfaceTexture) {}
        }
        addView(texture, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
        addView(ringView, LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT))
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent?) = false
    override fun onInterceptTouchEvent(ev: MotionEvent?) = false
    override fun dispatchTouchEvent(ev: MotionEvent?) = false

    override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {
        super.onLayout(changed, l, t, r, b)
        fixPlacement()
        if (changed) { applyTransform(); ringView.relayout(width, height) }
    }

    /**
     * Puts the preview exactly at the page's rectangle — the web view's top-left + rect × the page's
     * CSS scale — whatever margins the activity gave this view. (The activity's margin math counts the
     * root's system-bar inset padding twice: on its own the preview sat one status-bar height too
     * low and could cover the page's bottom bar.) Clipped to the web view's box, so it never covers a
     * system bar or anything outside the page.
     */
    fun fixPlacement() {
        val p = parent as? ViewGroup ?: return
        val web = (0 until p.childCount).map { p.getChildAt(it) }.firstOrNull { it is WebView } as? WebView ?: return
        val rc = rectCss ?: return
        @Suppress("DEPRECATION")
        val scale = web.scale.takeIf { it > 0f } ?: resources.displayMetrics.density
        val e = CapturePlacement.expected(rc, scale, web.left, web.top)
        translationX = (e[0] - left).toFloat(); translationY = (e[1] - top).toFloat()
        clipBounds = CapturePlacement.clip(e[0], e[1], width, height, web.left, web.top, web.width, web.height)
            ?.let { android.graphics.Rect(it[0], it[1], it[2], it[3]) }
    }

    /** Display rotation in degrees (0, 90, 180, 270). */
    val displayDegrees: Int get() = (display?.rotation ?: Surface.ROTATION_0) * 90

    /**
     * The camera's SurfaceTexture is already upright for the device's natural orientation; with the
     * display turned, rotate the content back and stretch it so the WHOLE frame fills the view
     * (the frame's x,y 0..1 then equals the view's x,y 0..1 — what the ring and the page's taps use).
     */
    fun applyTransform() {
        val w = width.toFloat(); val h = height.toFloat()
        if (w <= 0 || h <= 0) return
        val deg = displayDegrees
        onRotation(deg)
        texture.setTransform(transformFor(deg, w, h))
    }

    /** Draws/moves/hides the ring from a §12 target (main thread). */
    fun ring(target: JSONObject?) = ringView.update(target, width, height)

    companion object {
        fun transformFor(displayDeg: Int, w: Float, h: Float): Matrix {
            val m = Matrix()
            val cx = w / 2; val cy = h / 2
            when (displayDeg) {
                90, 270 -> {
                    m.postRotate(if (displayDeg == 90) -90f else 90f, cx, cy)
                    m.postScale(w / h, h / w, cx, cy)
                }
                180 -> m.postRotate(180f, cx, cy)
            }
            return m
        }

        const val ACCENT = 0xFFFF8A3D.toInt()
        const val MIN_RING_DP = 28f
    }
}

/** The thin accent ring on the tracked light: locked → full, lost → dimmed, searching → hidden. */
class RingView(context: Context) : View(context) {
    private val dp = resources.displayMetrics.density
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeWidth = 2.5f * dp; color = PreviewOverlay.ACCENT }
    private val shadow = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeWidth = 4.5f * dp; color = Color.argb(90, 0, 0, 0) }
    @Volatile var state = "searching"; private set
    var diameterPx = 0; private set
    /** Centre in the overlay's pixels (where the ring is going, not the animated position). */
    var centreX = 0f; private set
    var centreY = 0f; private set
    private var last: JSONObject? = null

    init { isClickable = false; isFocusable = false; visibility = GONE }

    override fun onDraw(c: Canvas) {
        val r = width / 2f - shadow.strokeWidth / 2
        c.drawCircle(width / 2f, height / 2f, r, shadow)
        c.drawCircle(width / 2f, height / 2f, r, paint)
    }

    fun relayout(w: Int, h: Int) { last?.let { update(it, w, h, animate = false) } }

    fun update(t: JSONObject?, w: Int, h: Int, animate: Boolean = true) {
        last = t
        state = t?.optString("state", "searching") ?: "searching"
        val x = t?.optDouble("x", Double.NaN) ?: Double.NaN
        val y = t?.optDouble("y", Double.NaN) ?: Double.NaN
        if (state == "searching" || x.isNaN() || y.isNaN() || w <= 0 || h <= 0) { animate().cancel(); visibility = GONE; return }
        val d = maxOf((t!!.optDouble("size", 0.0) * w).toInt(), (PreviewOverlay.MIN_RING_DP * dp).toInt())
        if (d != diameterPx) { diameterPx = d; layoutParams = layoutParams.apply { width = d; height = d } }
        centreX = (x.coerceIn(0.0, 1.0) * w).toFloat(); centreY = (y.coerceIn(0.0, 1.0) * h).toFloat()
        val tx = centreX - d / 2f; val ty = centreY - d / 2f
        val wasHidden = visibility != VISIBLE
        visibility = VISIBLE
        val a = if (state == "locked") 1f else 0.4f
        if (!animate || wasHidden) { animate().cancel(); translationX = tx; translationY = ty; alpha = a }
        else animate().translationX(tx).translationY(ty).alpha(a).setDuration(120).start()
    }
}

/** Pure geometry of the preview placement (unit-tested). */
object CapturePlacement {
    /**
     * Clip rect in the overlay's own coordinates so it never draws outside the web view's box
     * ([wl],[wt],[ww],[wh], parent coordinates); null when the overlay lies inside it.
     */
    fun clip(left: Int, top: Int, w: Int, h: Int, wl: Int, wt: Int, ww: Int, wh: Int): IntArray? {
        val l = maxOf(left, wl); val t = maxOf(top, wt); val r = minOf(left + w, wl + ww); val b = minOf(top + h, wt + wh)
        if (l == left && t == top && r == left + w && b == top + h) return null
        if (r <= l || b <= t) return intArrayOf(0, 0, 0, 0)
        return intArrayOf(l - left, t - top, r - left, b - top)
    }

    /**
     * Where the preview must be (device px, the activity root's coordinates): the page's CSS rect ×
     * [scale] from the web view's top-left ([webLeft], [webTop]). Returns x, y, w, h.
     */
    fun expected(rectCss: JSONObject, scale: Float, webLeft: Int, webTop: Int): IntArray = intArrayOf(
        webLeft + Math.round(rectCss.optDouble("x", 0.0) * scale).toInt(),
        webTop + Math.round(rectCss.optDouble("y", 0.0) * scale).toInt(),
        Math.round(rectCss.optDouble("w", 0.0) * scale).toInt().coerceAtLeast(1),
        Math.round(rectCss.optDouble("h", 0.0) * scale).toInt().coerceAtLeast(1),
    )
}
