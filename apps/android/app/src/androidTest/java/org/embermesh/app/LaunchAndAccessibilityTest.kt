package org.embermesh.app

import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Color
import android.os.Build
import android.os.Process
import android.os.SystemClock
import android.util.Log
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.host.HostError
import org.embermesh.app.ui.LaunchHandover
import org.embermesh.app.ui.MainActivity
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.CopyOnWriteArrayList

/** Finishes MainActivity instances started outside ActivityScenario (main thread). */
object MainActivityTracker {
    fun finishAll() {
        val reg = androidx.test.runner.lifecycle.ActivityLifecycleMonitorRegistry.getInstance()
        for (st in listOf(androidx.test.runner.lifecycle.Stage.RESUMED, androidx.test.runner.lifecycle.Stage.PAUSED, androidx.test.runner.lifecycle.Stage.STARTED))
            for (a in reg.getActivitiesInStage(st)) if (a is MainActivity) a.finish()
    }
}

/** RPC_API §8 on Android: launch hand-over, accessibility hints, haptics. */
@RunWith(AndroidJUnit4::class)
class LaunchAndAccessibilityTest {
    private val inst = InstrumentationRegistry.getInstrumentation()
    private val app get() = inst.targetContext

    private fun shell(cmd: String): String =
        inst.uiAutomation.executeShellCommand(cmd).let { fd -> android.os.ParcelFileDescriptor.AutoCloseInputStream(fd).use { it.readBytes().toString(Charsets.UTF_8) } }

    private fun near(c: Int, r: Int, g: Int, b: Int, tol: Int = 12) =
        kotlin.math.abs(Color.red(c) - r) <= tol && kotlin.math.abs(Color.green(c) - g) <= tol && kotlin.math.abs(Color.blue(c) - b) <= tol

    /**
     * Start-up seen through screenshots. Each screenshot is bracketed by the clock before and after the
     * capture, so its pixels are from somewhere inside [before, after]. What the app can guarantee, and
     * the observable signal each claim rests on:
     *   from the launch on                never a white frame
     *   once the open transition is over  (Activity.onEnterAnimationComplete, recorded by the app) the
     *                                     app's window covers the screen: brand background at the
     *                                     margins — no launcher / wallpaper / previous screen
     *   ... and while our overlay is the  (after the system splash was removed, before the overlay
     *       topmost content               was removed) the logo in the middle
     * Before the transition ends the system animates the starting window in over the previous screen,
     * which the app does not control. Our overlay's own first frame is also checked directly (logo
     * already at full size), since the windows above can be a few ms short or empty.
     */
    @Test fun launchHandsOverToTheWebViewWithoutAWhiteFrame() {
        // The notification-permission dialog would cover the screen with its scrim: grant it up front.
        if (Build.VERSION.SDK_INT >= 33) inst.uiAutomation.grantRuntimePermission(app.packageName, android.Manifest.permission.POST_NOTIFICATIONS)
        class Shot(val before: Long, val after: Long, val bmp: Bitmap)
        val shots = ArrayList<Shot>()
        // Start without waiting for the activity (ActivityScenario.launch would block past the hand-over).
        LaunchHandover.resetForTest()
        val launchedAt = SystemClock.uptimeMillis()
        // Like a tap on the launcher icon: from Android 13 other launches get an icon-less splash by default.
        val opts = if (Build.VERSION.SDK_INT >= 33) android.app.ActivityOptions.makeBasic()
            .setSplashScreenStyle(android.window.SplashScreen.SPLASH_SCREEN_STYLE_ICON).toBundle() else null
        app.startActivity(Intent(app, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK), opts)
        val scenario: AutoCloseable = AutoCloseable { inst.runOnMainSync { MainActivityTracker.finishAll() } }
        try {
            val end = SystemClock.uptimeMillis() + 4000
            while (SystemClock.uptimeMillis() < end) {
                val t0 = SystemClock.uptimeMillis()
                val bmp = inst.uiAutomation.takeScreenshot()
                val t1 = SystemClock.uptimeMillis()
                if (bmp != null) shots.add(Shot(t0, t1, bmp))
                if (LaunchHandover.last != null && shots.size > 6) break
            }
            TestNode.waitUntil(8000, "overlay removed") { LaunchHandover.last != null }
            // The transition end may come after the hand-over; it is recorded in the same timeline.
            TestNode.waitUntil(5000, "open transition finished") { LaunchHandover.last!!.has("enter_animation_complete_ms") }
            val tl = LaunchHandover.last!!
            Log.i("LaunchTest", "timeline $tl, ${shots.size} screenshots")
            assertEquals("web_frame", tl.getString("reason"))
            // Our overlay's first frame already had the logo at its final size.
            val expected = tl.getInt("expected_logo_px")
            assertTrue("overlay first frame: $tl", expected > 50 && tl.getInt("first_frame_logo_px") == expected)

            val start = if (Build.VERSION.SDK_INT >= 24) Process.getStartUptimeMillis() else 0L
            val removedAt = start + tl.getLong("overlay_removed_ms")
            // A window change reaches the screen with the next composition.
            val coveredFrom = start + tl.getLong("enter_animation_complete_ms") + COMPOSE_MS
            val splashGone = start + (if (tl.has("system_splash_removed_ms")) tl.getLong("system_splash_removed_ms") else tl.getLong("first_frame_ms")) + COMPOSE_MS
            val overlayTopFrom = maxOf(coveredFrom, splashGone)
            var transition = 0; var covered = 0; var logo = 0
            for (s in shots) {
                val bmp = s.bmp
                val w = bmp.width; val h = bmp.height
                if (s.before < launchedAt || s.after > removedAt) continue // before the launch, or possibly the page's
                val span = "[+${s.before - start}, +${s.after - start}] ms"
                val isCovered = s.before >= coveredFrom
                for (y in listOf(h / 4, h / 2, 3 * h / 4)) for (x in listOf(w / 20, w - w / 20)) {
                    val c = bmp.getPixel(x, y)
                    if (Color.red(c) > 235 && Color.green(c) > 235 && Color.blue(c) > 235) { saveFailure(bmp); throw AssertionError("white frame in $span") }
                    if (isCovered && !near(c, 0x17, 0x11, 0x0f)) { saveFailure(bmp); throw AssertionError("frame in $span (window covers the screen): background ${Integer.toHexString(c)}") }
                }
                if (!isCovered) { transition++; continue }
                covered++
                if (s.before < overlayTopFrom) continue
                logo++
                // Our overlay is on top: the logo is drawn in the middle (the ember is bright).
                var bright = 0
                for (dy in -h / 20..h / 20 step 4) for (dx in -w / 20..w / 20 step 4) {
                    val c = bmp.getPixel(w / 2 + dx, h / 2 + dy)
                    if (Color.red(c) > 200) bright++
                }
                if (bright == 0) { saveFailure(bmp); throw AssertionError("frame in $span (overlay on top) has no logo") }
            }
            Log.i("LaunchTest", "frames: $transition during the open transition, $covered with the window covering the screen ($logo with the overlay on top)")
            assertTrue("no screenshot taken during the launch", transition + covered > 0)
        } finally { scenario.close(); shots.forEach { it.bmp.recycle() } }
    }

    /**
     * RPC_API §8 / ui/README.md "Launch screen": every cold start plays the FULL start-up animation,
     * the same as the web, although the native core makes the app ready long before it ends. Frame 0
     * comes only after the native overlay is gone, and the whole minimum plays from there. Two launches:
     * the second (storage and preferences already written) runs with the system's "Remove animations"
     * on (all three animation scales 0; the host then reports reduce_motion, which the rest of the UI
     * follows) and is still full — owner decision: only the app's own settings change the launch
     * animation. (The page's own prefers-reduced-motion reading is logged; this WebView does not
     * derive it from the animation scales.)
     */
    @Test fun everyColdStartPlaysTheFullAnimationFromFrameZero() {
        if (Build.VERSION.SDK_INT >= 33) inst.uiAutomation.grantRuntimePermission(app.packageName, android.Manifest.permission.POST_NOTIFICATIONS)
        val scales = listOf("animator_duration_scale", "transition_animation_scale", "window_animation_scale")
        val old = scales.associateWith { shell("settings get global $it").trim() }
        try {
            for (launch in 1..2) {
                val osReduce = launch == 2
                for (k in scales) shell("settings put global $k ${if (osReduce) "0" else "1"}")
                assertTrue(EmberRuntime.start(app))
                TestNode.waitUntil(5000, "host reports reduce_motion=$osReduce") {
                    EmberRuntime.router.handle("host.info", JSONObject()).getJSONObject("accessibility").getBoolean("reduce_motion") == osReduce
                }
                LaunchHandover.resetForTest()
                app.startActivity(Intent(app, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK))
                TestNode.waitUntil(10000, "launch $launch: overlay removed") { LaunchHandover.last != null }
                assertEquals("web_frame", LaunchHandover.last!!.getString("reason"))
                var boot: JSONObject? = null
                TestNode.waitUntil(10000, "launch $launch: animation finished") {
                    inst.runOnMainSync {
                        val a = androidx.test.runner.lifecycle.ActivityLifecycleMonitorRegistry.getInstance()
                            .getActivitiesInStage(androidx.test.runner.lifecycle.Stage.RESUMED).filterIsInstance<MainActivity>().firstOrNull()
                        a?.webViewForTest()?.evaluateJavascript(
                            "(() => window.__emberBoot ? {variant: __emberBoot.variant, active: __emberBoot.active, reason: __emberBoot.reason, canvas: __emberBoot.canvas, osReduce: __emberBoot.osReduce, at: __emberBoot.at} : null)()",
                        ) { v -> try { boot = JSONObject(v) } catch (_: Exception) {} }
                    }
                    SystemClock.sleep(100)
                    boot?.optBoolean("active", true) == false
                }
                val b = boot!!
                val at = b.getJSONObject("at")
                Log.i("LaunchTest", "launch $launch start-up animation (system reduce motion ${if (osReduce) "ON" else "off"}): $b")
                assertEquals("launch $launch: full animation by default (system reduce motion ${if (osReduce) "on" else "off"})", "full", b.getString("variant"))
                assertEquals("the WebView has a 2D canvas", true, b.optBoolean("canvas"))
                assertEquals("launch $launch: handed over by itself", "ready", b.getString("reason"))
                val frame0 = at.getLong("frame0"); val removed = at.optLong("removed", -1); val out = at.getLong("out"); val ready = at.optLong("ready", Long.MAX_VALUE)
                // frame0 is a requestAnimationFrame timestamp (the frame's start), removed is performance.now()
                // when the page handled 'ember:launch-removed'. The page holds its still until that event,
                // so frame 0 is produced in the frame after it — whose start time may precede the event's
                // handling by up to one frame interval. Anything earlier would mean frames ran under the overlay.
                assertTrue("frame 0 not drawn behind the native overlay: $at", removed >= 0 && frame0 >= removed - FRAME_MS)
                assertTrue("frame 0 follows the overlay's removal at once: $at", frame0 - removed < 120)
                assertTrue("launch $launch: the whole 2.4 s animation played: $at", out - frame0 >= 2400 - 20)
                assertTrue("the app was ready before the animation ended: $at", ready < out)
                inst.runOnMainSync { MainActivityTracker.finishAll() }
                SystemClock.sleep(800)
            }
        } finally {
            for (k in scales) shell("settings put global $k ${old[k].let { if (it == null || it == "null" || it.isEmpty()) "1" else it }}")
            inst.runOnMainSync { MainActivityTracker.finishAll() }
        }
    }

    /** A failing frame is kept where it survives the test APK being uninstalled (additional test output). */
    private fun saveFailure(bmp: Bitmap) {
        val dirs = listOfNotNull(
            InstrumentationRegistry.getArguments().getString("additionalTestOutputDir")?.let { java.io.File(it) },
            app.getExternalFilesDir(null),
        )
        for (d in dirs) try { d.mkdirs(); java.io.File(d, "launch-frame.png").outputStream().use { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) } } catch (_: Exception) {}
    }

    private companion object {
        /** A window change shows up on screen with the next composition (one 60 Hz frame, plus slack). */
        const val COMPOSE_MS = 34L
        /** One display frame at 60 Hz (+1 ms rounding of the page's millisecond stamps). */
        const val FRAME_MS = 17L
    }

    @Test fun accessibilityHintsAndChangeEvents() {
        assertTrue(EmberRuntime.start(app))
        val info = EmberRuntime.router.handle("host.info", JSONObject())
        val a = info.getJSONObject("accessibility")
        for (k in listOf("reduce_motion", "font_scale", "screen_reader", "high_contrast", "bold_text")) assertTrue("missing $k", a.has(k))
        assertTrue(info.getJSONArray("features").toString().contains("haptic"))
        val events = CopyOnWriteArrayList<JSONObject>()
        val l: (String) -> Unit = { s -> if (s.contains("host.accessibility")) events.add(JSONObject(s).getJSONObject("data")) }
        EmberRuntime.eventListeners.add(l)
        val oldAnim = shell("settings get global animator_duration_scale").trim()
        val oldFont = shell("settings get system font_scale").trim()
        try {
            shell("settings put global animator_duration_scale 0")
            TestNode.waitUntil(5000, "reduce_motion event") { events.any { it.getBoolean("reduce_motion") } }
            shell("settings put system font_scale 1.3")
            TestNode.waitUntil(5000, "font_scale event") { events.any { it.getDouble("font_scale") == 1.3 } }
            assertEquals(1.3, EmberRuntime.router.handle("host.info", JSONObject()).getJSONObject("accessibility").getDouble("font_scale"), 0.001)
        } finally {
            shell("settings put global animator_duration_scale ${if (oldAnim == "null" || oldAnim.isEmpty()) "1" else oldAnim}")
            // Restore, and make sure it took: a settings write can race the configuration change the
            // previous one caused, so check and re-apply until the system and the app report it.
            val font = if (oldFont == "null" || oldFont.isEmpty()) "1.0" else oldFont
            val fontVal = font.toDouble()
            val end = System.currentTimeMillis() + 10_000
            while (System.currentTimeMillis() < end) {
                shell("settings put system font_scale $font")
                Thread.sleep(300)
                val now = shell("settings get system font_scale").trim().toDoubleOrNull()
                val app = try { EmberRuntime.router.handle("host.info", JSONObject()).getJSONObject("accessibility").getDouble("font_scale") } catch (_: Throwable) { -1.0 }
                if (now == fontVal && kotlin.math.abs(app - fontVal) < 0.001) break
            }
            EmberRuntime.eventListeners.remove(l)
        }
        Log.i("LaunchTest", "accessibility events: $events")
    }

    @Test fun hapticRouting() {
        assertTrue(EmberRuntime.start(app))
        try { EmberRuntime.router.handle("host.haptic", JSONObject().put("kind", "boom")); throw AssertionError() } catch (e: HostError) { assertEquals("bad_params", e.code) }
        val vib = (app.getSystemService(android.content.Context.VIBRATOR_SERVICE) as android.os.Vibrator).hasVibrator()
        if (!vib) {
            try { EmberRuntime.router.handle("host.haptic", JSONObject().put("kind", "tap")); throw AssertionError() } catch (e: HostError) { assertEquals("unsupported", e.code) }
            return
        }
        ActivityScenario.launch<MainActivity>(Intent(app, MainActivity::class.java)).use {
            TestNode.waitUntil(5000, "UI attached") { EmberRuntime.hostContext.ui != null }
            for (k in listOf("tap", "success", "warning", "error")) {
                val r = EmberRuntime.router.handle("host.haptic", JSONObject().put("kind", k))
                assertNotNull(r.opt("performed"))
                Log.i("LaunchTest", "haptic $k -> $r")
            }
        }
    }
}
