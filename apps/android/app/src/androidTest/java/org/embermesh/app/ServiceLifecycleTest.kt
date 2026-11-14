package org.embermesh.app

import android.content.Intent
import android.os.Build
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.service.MeshService
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ServiceLifecycleTest {
    private fun waitUntil(ms: Long = 8000, f: () -> Boolean): Boolean {
        val end = System.currentTimeMillis() + ms
        while (System.currentTimeMillis() < end) { if (f()) return true; Thread.sleep(100) }
        return f()
    }

    @Test fun foregroundServiceStartsNodeAndQuitStopsIt() {
        val ctx = InstrumentationRegistry.getInstrumentation().targetContext
        MeshServiceControl.start(ctx)
        assertTrue("service running", waitUntil { MeshService.running })
        assertTrue("node started", EmberRuntime.isStarted)
        assertTrue(EmberRuntime.summary().isNotEmpty())
        // Quit action from the notification.
        val quit = Intent(ctx, MeshService::class.java).setAction(MeshService.ACTION_QUIT)
        if (Build.VERSION.SDK_INT >= 26) ctx.startForegroundService(quit) else ctx.startService(quit)
        assertTrue("service stopped", waitUntil { !MeshService.running })
        assertFalse("node stopped", EmberRuntime.isStarted)
        // And it comes back.
        MeshServiceControl.start(ctx)
        assertTrue(waitUntil { MeshService.running && EmberRuntime.isStarted })
        MeshServiceControl.quit(ctx)
        assertTrue(waitUntil { !MeshService.running })
    }
}
