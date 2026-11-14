package org.embermesh.app.host

import android.content.ComponentCallbacks
import android.content.Context
import android.content.res.Configuration
import android.database.ContentObserver
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.view.accessibility.AccessibilityManager
import org.json.JSONObject

/**
 * Platform accessibility state for the shared UI (RPC_API §8): `host.info.accessibility` and the
 * `host.accessibility` event whenever it changes.
 *   reduce_motion  "Remove animations" (animator duration scale 0)
 *   font_scale     Configuration.fontScale (the web view's own text zoom is pinned to 100 %, so the UI
 *                  applies this once, multiplied with its --text-scale: no double scaling)
 *   screen_reader  a touch-exploring accessibility service (TalkBack) is on
 *   high_contrast  "High contrast text" (secure setting, where readable)
 *   bold_text      "Bold text" (Configuration.fontWeightAdjustment > 0, Android 12+)
 */
class AccessibilityHints(private val app: Context, private val emit: (JSONObject) -> Unit) {
    private val main = Handler(Looper.getMainLooper())
    @Volatile private var last: String? = null
    private val am = app.getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager

    fun snapshot(config: Configuration = app.resources.configuration): JSONObject {
        val anim = try { Settings.Global.getFloat(app.contentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, 1f) } catch (_: Exception) { 1f }
        val hc = try { Settings.Secure.getInt(app.contentResolver, "high_text_contrast_enabled", 0) == 1 } catch (_: Exception) { false }
        val bold = Build.VERSION.SDK_INT >= 31 && config.fontWeightAdjustment != Configuration.FONT_WEIGHT_ADJUSTMENT_UNDEFINED && config.fontWeightAdjustment > 0
        return JSONObject()
            .put("reduce_motion", anim == 0f)
            .put("font_scale", Math.round(config.fontScale * 100) / 100.0)
            .put("screen_reader", am.isEnabled && am.isTouchExplorationEnabled)
            .put("high_contrast", hc)
            .put("bold_text", bold)
    }

    /** Re-reads the state and emits host.accessibility if anything changed. */
    fun check(config: Configuration = app.resources.configuration) {
        val s = snapshot(config)
        val t = s.toString()
        if (t != last) {
            val first = last == null
            last = t
            if (!first) emit(s)
        }
    }

    private val observer = object : ContentObserver(main) { override fun onChange(selfChange: Boolean) = check() }
    private val touchListener = AccessibilityManager.TouchExplorationStateChangeListener { check() }
    private val stateListener = AccessibilityManager.AccessibilityStateChangeListener { check() }
    private val callbacks = object : ComponentCallbacks {
        override fun onConfigurationChanged(newConfig: Configuration) = check(newConfig)
        @Deprecated("ComponentCallbacks") override fun onLowMemory() {}
    }

    fun start() {
        check()
        try { app.contentResolver.registerContentObserver(Settings.Global.getUriFor(Settings.Global.ANIMATOR_DURATION_SCALE), false, observer) } catch (_: Exception) {}
        try { app.contentResolver.registerContentObserver(Settings.Secure.getUriFor("high_text_contrast_enabled"), false, observer) } catch (_: Exception) {}
        am.addTouchExplorationStateChangeListener(touchListener)
        am.addAccessibilityStateChangeListener(stateListener)
        app.registerComponentCallbacks(callbacks)
    }

    fun stop() {
        try { app.contentResolver.unregisterContentObserver(observer) } catch (_: Exception) {}
        am.removeTouchExplorationStateChangeListener(touchListener)
        am.removeAccessibilityStateChangeListener(stateListener)
        try { app.unregisterComponentCallbacks(callbacks) } catch (_: Exception) {}
    }
}
