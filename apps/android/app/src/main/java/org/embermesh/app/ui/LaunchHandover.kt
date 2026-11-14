package org.embermesh.app.ui

import android.app.Activity
import android.os.Build
import android.os.Handler
import android.os.Process
import android.os.SystemClock
import android.util.Log
import android.view.Gravity
import android.view.View
import android.webkit.WebView
import android.widget.FrameLayout
import android.widget.ImageView
import org.embermesh.app.R
import org.json.JSONObject

/**
 * Launch screen → web boot animation without a flash:
 *  1. The window background (pre-12) or the system splash (12+) shows the logo on #17110f.
 *  2. At the first app frame the system splash is removed WITHOUT a fade; in the same frame this
 *     overlay shows the identical logo, at the size and place the web boot animation's first frame
 *     uses (30 % of the shorter side of the web view, centred in it).
 *  3. When the WebView has committed its first visible frame (onPageCommitVisible + a visual-state
 *     callback, i.e. the page's static logo is on screen) the overlay is removed in one frame, so the
 *     animation appears to start from the still image. A 6 s timeout removes it in any case.
 * After step 3 nothing native (views, timers, animators) remains.
 */
class LaunchHandover(private val activity: Activity, private val root: FrameLayout, private val main: Handler, private val bg: Int,
                     /** Called right after the overlay is gone (the page may start animating at once). */
                     private val onRemoved: () -> Unit = {}) {
    private var overlay: FrameLayout? = null
    private val timeout = Runnable { remove("timeout") }
    private val processStart = if (Build.VERSION.SDK_INT >= 24) Process.getStartUptimeMillis() else SystemClock.uptimeMillis()
    private val timeline = JSONObject()

    private fun mark(what: String) { timeline.put(what, SystemClock.uptimeMillis() - processStart) }

    fun install() {
        mark("activity_created_ms")
        val logo = ImageView(activity).apply {
            setImageResource(R.drawable.ic_logo)
            scaleType = ImageView.ScaleType.FIT_CENTER
        }
        // The logo size is set while measuring, so the very first frame already has it right.
        val box = object : FrameLayout(activity) {
            override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
                val side = (minOf(MeasureSpec.getSize(widthMeasureSpec), MeasureSpec.getSize(heightMeasureSpec)) * LOGO_FRACTION).toInt().coerceAtLeast(1)
                val lp = logo.layoutParams as LayoutParams
                lp.width = side; lp.height = side
                super.onMeasure(widthMeasureSpec, heightMeasureSpec)
            }
        }.apply {
            setBackgroundColor(bg)
            importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
            isClickable = false
        }
        box.addView(logo, FrameLayout.LayoutParams(1, 1, Gravity.CENTER))
        root.addView(box, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT))
        overlay = box
        if (Build.VERSION.SDK_INT >= 31) {
            activity.splashScreen.setOnExitAnimationListener { v ->
                mark("system_splash_removed_ms")
                v.remove() // no fade: the overlay below already shows the same image
            }
        }
        box.viewTreeObserver.addOnDrawListener(object : android.view.ViewTreeObserver.OnDrawListener {
            override fun onDraw() {
                if (!timeline.has("first_frame_ms")) {
                    mark("first_frame_ms")
                    // Evidence that the overlay's very first frame already has the logo at full size.
                    timeline.put("first_frame_logo_px", logo.width)
                    timeline.put("expected_logo_px", (minOf(box.width, box.height) * LOGO_FRACTION).toInt())
                }
                main.post { try { box.viewTreeObserver.removeOnDrawListener(this) } catch (_: Exception) {} }
            }
        })
        main.postDelayed(timeout, TIMEOUT_MS)
    }

    /** The page committed its first visible frame. */
    fun webCommitted(wv: WebView) {
        if (overlay == null || timeline.has("web_commit_ms")) return
        mark("web_commit_ms")
        if (Build.VERSION.SDK_INT >= 23) wv.postVisualStateCallback(1L, object : WebView.VisualStateCallback() {
            override fun onComplete(requestId: Long) { main.post { remove("web_frame") } }
        }) else main.post { remove("web_frame") }
    }

    fun remove(why: String) {
        val o = overlay ?: return
        overlay = null
        main.removeCallbacks(timeout)
        root.removeView(o)
        mark("overlay_removed_ms")
        timeline.put("reason", why)
        last = timeline
        onRemoved()
        Log.i("EmberLaunch", "launch timeline (ms since process start): $timeline")
    }

    /**
     * The window's open transition has finished (Activity.onEnterAnimationComplete): from now on the
     * app's window covers the screen, nothing of the previous screen shows through.
     */
    fun enterAnimationComplete() {
        if (!timeline.has("enter_animation_complete_ms")) mark("enter_animation_complete_ms")
    }

    val active: Boolean get() = overlay != null

    companion object {
        /** Logo size relative to the shorter side of the web view (launch-screen contract, ui/README.md). */
        const val LOGO_FRACTION = 0.30f
        const val TIMEOUT_MS = 6_000L
        @Volatile var last: JSONObject? = null
            private set

        /** Tests: forget the previous launch's timeline. */
        fun resetForTest() { last = null }
    }
}
