package org.embermesh.app

import android.Manifest
import android.content.Intent
import android.content.pm.ActivityInfo
import android.os.SystemClock
import android.util.Log
import android.view.MotionEvent
import android.view.View
import androidx.lifecycle.Lifecycle
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.capture.PreviewOverlay
import org.embermesh.app.ui.MainActivity
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/**
 * RPC_API §12 "Native preview" on the device: the camera preview sits exactly inside the page's
 * rectangle (CSS px of the web view's viewport → screen), follows `host.capture.update`, stays right
 * after a rotation, is clipped to the web view, lets taps through, draws the marker ring per target
 * state, and is gone at once on stop and when the app goes to the background.
 */
@RunWith(AndroidJUnit4::class)
class CapturePreviewTest {
    private val inst = InstrumentationRegistry.getInstrumentation()
    private val app get() = inst.targetContext

    /** Runs on the main thread without waiting for the looper to idle (the preview redraws constantly). */
    private fun <T> onMain(a: MainActivity, f: () -> T): T {
        var out: T? = null; var err: Throwable? = null
        val l = CountDownLatch(1)
        a.runOnUiThread { try { out = f() } catch (t: Throwable) { err = t } finally { l.countDown() } }
        assertTrue(l.await(10, TimeUnit.SECONDS))
        err?.let { throw it }
        @Suppress("UNCHECKED_CAST") return out as T
    }

    private fun rect(x: Int, y: Int, w: Int, h: Int) = JSONObject().put("x", x).put("y", y).put("w", w).put("h", h)

    /** Expected vs actual on-screen bounds of the preview for [r]; asserts within 2 px. */
    private fun checkBounds(a: MainActivity, ov: PreviewOverlay, r: JSONObject, what: String) {
        TestNode.waitUntil(5000, "$what laid out") { onMain(a) { ov.isLaidOut && ov.width > 1 && !ov.isLayoutRequested } }
        Thread.sleep(150)
        val (exp, act) = onMain(a) {
            val wv = a.webViewForTest()!!
            val wl = IntArray(2); wv.getLocationOnScreen(wl)
            @Suppress("DEPRECATION") val k = wv.scale
            val e = intArrayOf(wl[0] + Math.round(r.getDouble("x") * k).toInt(), wl[1] + Math.round(r.getDouble("y") * k).toInt(),
                Math.round(r.getDouble("w") * k).toInt(), Math.round(r.getDouble("h") * k).toInt())
            val ol = IntArray(2); ov.getLocationOnScreen(ol)
            Log.i("CapturePreviewTest", "$what: correction of the activity's margins = (${ov.translationX}, ${ov.translationY}) px")
            e to intArrayOf(ol[0], ol[1], ov.width, ov.height)
        }
        Log.i("CapturePreviewTest", "$what: expected ${exp.toList()} actual ${act.toList()}")
        for (i in 0..3) assertEquals("$what [x,y,w,h][$i] expected ${exp.toList()} got ${act.toList()}", exp[i].toDouble(), act[i].toDouble(), 2.0)
    }

    @Test fun previewSitsInTheRectFollowsUpdatesRotationRingAndStops() {
        inst.uiAutomation.grantRuntimePermission(app.packageName, Manifest.permission.CAMERA)
        assertTrue(EmberRuntime.start(app))
        val events = CopyOnWriteArrayList<String>()
        val listener: (String) -> Unit = { if (it.contains("host.capture.state")) events.add(it) }
        EmberRuntime.eventListeners.add(listener)
        val scenario = ActivityScenario.launch<MainActivity>(Intent(app, MainActivity::class.java))
        var a: MainActivity? = null
        scenario.onActivity { a = it }
        val act = a!!
        try {
            TestNode.waitUntil(10_000, "activity is the UI host") { EmberRuntime.hostContext.ui === act && act.webViewForTest()?.isLaidOut == true }
            // A session the core does not know: its feeds are refused, so only this test drives the ring.
            val session = "preview-test-no-such-session"
            val r1 = rect(16, 120, 320, 240)
            val res = EmberRuntime.router.handle("host.capture.start", JSONObject().put("kind", "camera_luma").put("session", session).put("rect", r1))
            assertTrue(res.has("zoom_steps"))
            val src = EmberRuntime.capture!!.cameraSource()!!
            TestNode.waitUntil(5000, "preview shown") { src.previewView?.parent != null }
            val ov = src.previewView!!
            checkBounds(act, ov, r1, "rect 1")

            val r2 = rect(40, 60, 200, 300)
            EmberRuntime.router.handle("host.capture.update", JSONObject().put("kind", "camera_luma").put("rect", r2))
            checkBounds(act, ov, r2, "rect 2")

            // Taps go to the page, never to the preview.
            val down = MotionEvent.obtain(SystemClock.uptimeMillis(), SystemClock.uptimeMillis(), MotionEvent.ACTION_DOWN, 10f, 10f, 0)
            assertFalse(onMain(act) { ov.dispatchTouchEvent(down) }); down.recycle()

            // The ring: locked → at x,y with max(size × width, 28 dp); lost → dimmed; searching → none.
            src.onTarget(JSONObject().put("state", "locked").put("x", 0.25).put("y", 0.5).put("size", 0.1).put("confidence", 0.9))
            Thread.sleep(400)
            onMain(act) {
                val ring = ov.ringView
                assertEquals(View.VISIBLE, ring.visibility)
                assertEquals(0.25 * ov.width, ring.centreX.toDouble(), 1.5)
                assertEquals(0.5 * ov.height, ring.centreY.toDouble(), 1.5)
                assertEquals(maxOf((0.1 * ov.width).toInt(), (28 * act.resources.displayMetrics.density).toInt()), ring.diameterPx)
                assertEquals(ring.centreX - ring.diameterPx / 2f, ring.translationX, 1.5f)
                assertEquals(1f, ring.alpha, 0.01f)
            }
            src.onTarget(JSONObject().put("state", "lost").put("x", 0.6).put("y", 0.4).put("size", 0.01).put("confidence", 0.2))
            Thread.sleep(400)
            onMain(act) {
                assertEquals(View.VISIBLE, ov.ringView.visibility); assertEquals(0.4f, ov.ringView.alpha, 0.05f)
                assertEquals(0.6 * ov.width, ov.ringView.centreX.toDouble(), 1.5)
                assertEquals((28 * act.resources.displayMetrics.density).toInt(), ov.ringView.diameterPx)
            }
            src.onTarget(JSONObject().put("state", "searching").put("confidence", 0).put("candidates", 0))
            Thread.sleep(200)
            assertEquals(View.GONE, onMain(act) { ov.ringView.visibility })

            // Clipped to the web view: a rect starting above it never draws over what is above.
            val r3 = rect(10, -40, 200, 150)
            EmberRuntime.router.handle("host.capture.update", JSONObject().put("kind", "camera_luma").put("rect", r3))
            checkBounds(act, ov, r3, "rect 3 (partly above the page)")
            val clip = onMain(act) { ov.clipBounds }
            assertNotNull("clipped at the web view's top", clip)
            @Suppress("DEPRECATION") val k = onMain(act) { act.webViewForTest()!!.scale }
            assertEquals(40 * k, clip!!.top.toFloat(), 2f)

            // Rotation: landscape, the page sends a new rect, the preview follows and turns.
            onMain(act) { act.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE }
            TestNode.waitUntil(8000, "landscape") { onMain(act) { val wv = act.webViewForTest()!!; wv.width > wv.height && ov.displayDegrees % 180 == 90 } }
            Thread.sleep(500)
            val r4 = rect(100, 40, 360, 200)
            EmberRuntime.router.handle("host.capture.update", JSONObject().put("kind", "camera_luma").put("rect", r4))
            checkBounds(act, ov, r4, "rect 4 (landscape)")
            assertEquals("the grid follows the display rotation", ov.displayDegrees, src.displayDegrees)
            onMain(act) { act.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED }

            // Stop: preview gone at once.
            EmberRuntime.router.handle("host.capture.stop", JSONObject().put("kind", "camera_luma"))
            TestNode.waitUntil(1000, "preview removed on stop") { onMain(act) { ov.parent == null } }

            // Background: the camera is released and the page told.
            EmberRuntime.router.handle("host.capture.start", JSONObject().put("kind", "camera_luma").put("session", session).put("rect", r1))
            assertTrue(EmberRuntime.capture!!.cameraActive())
            scenario.moveToState(Lifecycle.State.CREATED)
            TestNode.waitUntil(3000, "camera released in the background") { !EmberRuntime.capture!!.cameraActive() }
            TestNode.waitUntil(2000, "host.capture.state running:false") { events.any { it.contains("\"running\":false") && it.contains("camera_luma") } }
            assertNull(EmberRuntime.capture!!.cameraSource())
        } finally {
            EmberRuntime.eventListeners.remove(listener)
            try { onMain(act) { act.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED } } catch (_: Throwable) {}
            try { EmberRuntime.router.handle("host.capture.stop", JSONObject().put("kind", "camera_luma")) } catch (_: Throwable) {}
            scenario.close()
        }
    }
}
