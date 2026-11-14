package org.embermesh.app

import android.content.Intent
import android.os.Build
import android.util.Log
import android.view.WindowManager
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.host.HostError
import org.embermesh.app.services.AndroidDeviceServices
import org.embermesh.app.ui.MainActivity
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith

/** RPC_API §10 Morse sending by light, through the real router on the device. */
@RunWith(AndroidJUnit4::class)
class LightSendingTest {
    private val inst = InstrumentationRegistry.getInstrumentation()
    private val app get() = inst.targetContext
    private val router get() = EmberRuntime.router
    private val services get() = EmberRuntime.services as AndroidDeviceServices

    private fun flash(pattern: List<Long>, mode: String? = null): JSONObject =
        router.handle("host.signal.flash", JSONObject().put("pattern_ms", JSONArray(pattern)).apply { if (mode != null) put("mode", mode) })

    /** Runs a shell command to completion (closing its pipe early can cancel it). */
    private fun shell(cmd: String): String =
        android.os.ParcelFileDescriptor.AutoCloseInputStream(inst.uiAutomation.executeShellCommand(cmd)).use { it.readBytes().toString(Charsets.UTF_8) }

    private fun torchPresent() = router.handle("host.info", JSONObject()).getJSONObject("signal").getString("torch") == "yes"

    /** The emulator's only flash unit is on its front camera: let the torch tests use it. */
    private fun <T> withAnyTorch(f: () -> T): T {
        assertTrue(EmberRuntime.start(app))
        services.useAnyFacingTorchForTest(true)
        try { return f() } finally { services.useAnyFacingTorchForTest(false) }
    }

    /** Runs a flash on another thread (it blocks until finished/stopped). */
    private fun flashAsync(pattern: List<Long>, mode: String): () -> JSONObject? {
        var r: JSONObject? = null
        var err: Throwable? = null
        val t = Thread { try { r = flash(pattern, mode) } catch (e: Throwable) { err = e } }.apply { start() }
        return { t.join(10_000); err?.let { throw it }; r }
    }

    private fun windowState(s: ActivityScenario<MainActivity>): String {
        var out = ""
        s.onActivity { a -> out = "brightness=${a.window.attributes.screenBrightness} keepOn=${(a.window.attributes.flags and WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON) != 0} fg=${a.window.decorView.findViewById<android.view.View>(android.R.id.content).let { (it as? android.view.ViewGroup)?.getChildAt(0)?.foreground }}" }
        return out
    }

    @Test fun infoAndLimits() {
        assertTrue(EmberRuntime.start(app))
        val info = router.handle("host.info", JSONObject())
        val sig = info.getJSONObject("signal")
        assertTrue(sig.getString("torch") in setOf("yes", "no"))
        assertTrue(sig.getBoolean("screen"))
        val features = info.getJSONArray("features").toString()
        assertTrue(features.contains("\"flash\""))
        assertEquals(sig.getString("torch") == "yes", features.contains("\"torch\""))
        for (bad in listOf(listOf(5L), listOf(60_001L), List(4097) { 10L }, List(11) { 60_000L })) {
            try { flash(bad, "auto"); throw AssertionError("accepted") } catch (e: HostError) { assertEquals("bad_params", e.code) }
        }
        try { flash(listOf(100L), "laser"); throw AssertionError() } catch (e: HostError) { assertEquals("bad_params", e.code) }
        // No app in front: a screen flash is impossible.
        try { flash(listOf(100L), "screen"); throw AssertionError() } catch (e: HostError) { assertEquals("unsupported", e.code) }
        router.handle("host.signal.stop", JSONObject()) // no-op when idle
    }

    @Test fun noBackTorchMeansScreenInAuto() {
        assertTrue(EmberRuntime.start(app))
        if (torchPresent()) return // a device with a back torch: covered by the torch tests
        try { flash(listOf(60L), "torch"); throw AssertionError() } catch (e: HostError) { assertEquals("unsupported", e.code) }
        if (Build.VERSION.SDK_INT >= 33) inst.uiAutomation.grantRuntimePermission(app.packageName, android.Manifest.permission.POST_NOTIFICATIONS)
        ActivityScenario.launch<MainActivity>(Intent(app, MainActivity::class.java)).use {
            TestNode.waitUntil(5000, "UI in front") { EmberRuntime.hostContext.ui?.isInFront() == true }
            val r = flash(listOf(60L, 60L, 60L), "auto")
            assertEquals("screen", r.getString("used"))
            assertEquals("no_torch", r.getString("fallback"))
            assertTrue(r.getJSONObject("timing").has("max_late_ms"))
        }
        assertFalse(router.handle("host.info", JSONObject()).getJSONArray("features").toString().contains("\"torch\""))
    }

    @Test fun torchModeAndStopLeaveTheTorchOff() = withAnyTorch {
        assertTrue(EmberRuntime.start(app))
        assumeTrue("this device has no torch", torchPresent())
        val r = flash(listOf(60L, 60L, 180L, 60L, 60L), "torch")
        assertEquals("torch", r.getString("used"))
        assertFalse(r.getBoolean("stopped"))
        Log.i("LightTest", "torch timing ${r.getJSONObject("timing")}")
        TestNode.waitUntil(2000, "torch off") { !services.torchLitForTest() }
        // auto picks the torch
        assertEquals("torch", flash(listOf(30L), null).getString("used"))
        // stop mid-pattern: resolves with stopped:true, torch off
        TestNode.waitUntil(2000, "torch reported off") { !services.torchLitForTest() }
        val pending = flashAsync(List(60) { 100L }, "torch")
        TestNode.waitUntil(3000, "torch on") { services.torchLitForTest() }
        val t0 = System.currentTimeMillis()
        router.handle("host.signal.stop", JSONObject())
        val res = pending()!!
        Log.i("LightTest", "after stop (${System.currentTimeMillis() - t0} ms): $res")
        assertTrue(res.toString(), res.getBoolean("stopped"))
        assertTrue("stop is immediate", System.currentTimeMillis() - t0 < 1000)
        TestNode.waitUntil(2000, "torch off after stop") { !services.torchLitForTest() }
        // a second flash stops the first
        TestNode.waitUntil(2000, "torch reported off") { !services.torchLitForTest() }
        val first = flashAsync(List(60) { 100L }, "torch")
        TestNode.waitUntil(3000, "torch on") { services.torchLitForTest() }
        assertFalse(flash(listOf(40L, 40L, 40L), "torch").getBoolean("stopped"))
        assertTrue(first()!!.getBoolean("stopped"))
        TestNode.waitUntil(2000, "torch off") { !services.torchLitForTest() }
    }

    @Test fun screenModeRestoresTheWindowAfterTheEndStopAndBackgrounding() {
        if (Build.VERSION.SDK_INT >= 33) inst.uiAutomation.grantRuntimePermission(app.packageName, android.Manifest.permission.POST_NOTIFICATIONS)
        ActivityScenario.launch<MainActivity>(Intent(app, MainActivity::class.java)).use { s ->
            TestNode.waitUntil(5000, "UI in front") { EmberRuntime.hostContext.ui?.isInFront() == true }
            val before = windowState(s)
            val r = flash(listOf(60L, 60L, 60L), "screen")
            assertEquals("screen", r.getString("used"))
            assertEquals(before, windowState(s))
            // stop
            val pending = flashAsync(List(60) { 100L }, "screen")
            Thread.sleep(400)
            router.handle("host.signal.stop", JSONObject())
            assertTrue(pending()!!.getBoolean("stopped"))
            assertEquals(before, windowState(s))
            // a tap anywhere stops a screen flash (the white light hides the page's Stop button)
            val tapped = flashAsync(List(60) { 100L }, "screen")
            Thread.sleep(400)
            shell("input tap 540 1200")
            assertTrue(tapped()!!.getBoolean("stopped"))
            TestNode.waitUntil(3000, "window restored after tap") { windowState(s) == before }
            // backgrounding (home): the flash ends and the window is restored
            val bg = flashAsync(List(60) { 100L }, "screen")
            Thread.sleep(400)
            shell("input keyevent 3")
            assertTrue(bg()!!.getBoolean("stopped"))
            TestNode.waitUntil(3000, "window restored") { windowState(s) == before }
            Log.i("LightTest", "window state restored: $before")
        }
    }

    @Test fun cameraBusyFallsBackToTheScreen() = withAnyTorch {
        inst.uiAutomation.grantRuntimePermission(app.packageName, android.Manifest.permission.CAMERA)
        if (Build.VERSION.SDK_INT >= 33) inst.uiAutomation.grantRuntimePermission(app.packageName, android.Manifest.permission.POST_NOTIFICATIONS)
        assertTrue(EmberRuntime.start(app))
        assumeTrue("this device has no torch", torchPresent())
        ActivityScenario.launch<MainActivity>(Intent(app, MainActivity::class.java)).use {
            TestNode.waitUntil(5000, "UI in front") { EmberRuntime.hostContext.ui?.isInFront() == true }
            // This app's Morse camera reader holds the back camera: the torch is not available.
            router.handle("host.capture.start", JSONObject().put("kind", "camera_luma").put("session", 1).put("camera", "front"))
            try {
                TestNode.waitUntil(5000, "camera frames") { EmberRuntime.capture!!.status().optJSONObject("camera_luma")?.optLong("frames", 0) ?: 0 > 5 }
                val fb = flash(listOf(60L, 60L, 60L), "auto")
                assertEquals("screen", fb.getString("used"))
                assertEquals("busy", fb.getString("fallback"))
                try { flash(listOf(60L), "torch"); throw AssertionError("torch while the camera is open") } catch (e: HostError) { assertEquals("busy", e.code) }
            } finally {
                router.handle("host.capture.stop", JSONObject().put("kind", "camera_luma"))
            }
            TestNode.waitUntil(5000, "torch available again") { EmberRuntime.router.handle("host.signal.flash", JSONObject().put("pattern_ms", JSONArray(listOf(20L))).put("mode", "auto")).getString("used") == "torch" }
        }
    }
}
