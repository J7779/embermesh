package org.embermesh.app

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.core.EmberNative
import org.embermesh.app.core.EmberNode
import org.embermesh.app.core.NodeRunner
import org.embermesh.app.core.OutboxSink
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import java.security.SecureRandom
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/** JNI binding against the real native library (whatever core is linked: stub or full node). */
@RunWith(AndroidJUnit4::class)
class NativeCoreTest {
    private fun entropy() = ByteArray(32).also { SecureRandom().nextBytes(it) }
    private fun cfg(dir: String = "") = JSONObject().put("storage_dir", dir).put("platform", "android")
        .put("app_version", "test").put("device_name", "emulator").put("now_ms", System.currentTimeMillis()).toString()

    @Test fun versionAndCreateRejectsShortEntropy() {
        assertTrue(EmberNode.version().matches(Regex("\\d+\\.\\d+\\.\\d+.*")))
        assertNull(EmberNode.create(cfg(), ByteArray(8)))
    }

    @Test fun nodeInfoTickPollAndErrors() {
        val n = EmberNode.create(cfg(), entropy())!!
        try {
            val info = JSONObject(n.call("node.info", "{}"))
            assertTrue(info.toString(), info.getBoolean("ok"))
            assertEquals(16, info.getJSONObject("result").getString("node_id").length)
            val bad = JSONObject(n.call("no.such.method", "{}"))
            assertTrue(!bad.getBoolean("ok"))
            assertTrue(bad.getJSONObject("error").getString("code") in setOf("not_implemented", "not_found", "bad_params", "unsupported"))
            // Unicode (incl. a supplementary character) must survive the JNI string conversion.
            val r = JSONObject(n.call("node.info", "{\"x\":\"héllo 🔥\"}"))
            assertTrue(r.has("ok"))
            n.tick(System.currentTimeMillis())
            val w = n.nextWakeupMs()
            assertTrue("wakeup $w", w in 0..60_000)
            var events = 0
            var ready = false
            while (true) {
                val o = n.poll() ?: break
                if (o.kind == EmberNative.OUT_EVENT) { events++; if (JSONObject(o.text).getString("event") == "node.ready") ready = true }
            }
            assertTrue("events=$events", ready)
            // Hot paths with an unknown link id / odd sizes must be harmless.
            n.linkRx(12345, byteArrayOf(1, 2, 3), 0, 3)
            n.linkRx(12345, byteArrayOf(1, 2, 3), 2, 5) // out of bounds: ignored natively
            n.linkRx(12345, ByteArray(70_000), 0, 70_000)
            n.audioRx(12345, ShortArray(480), 480, 48_000)
            assertTrue(n.audioTx(12345, ShortArray(480), 480, 48_000) in 0..480)
            n.addEntropy(entropy())
            assertTrue(n.loadGuides(byteArrayOf(0, 1, 2, 3)) <= 0)
        } finally {
            n.close()
        }
        assertTrue(JSONObject(n.call("node.info", "{}")).getJSONObject("error").getString("code") == "internal")
        assertNull(n.poll())
    }

    @Test fun persistentStorageDir() {
        val dir = File(InstrumentationRegistry.getInstrumentation().targetContext.cacheDir, "ember-test-store").apply { deleteRecursively(); mkdirs() }
        val a = EmberNode.create(cfg(dir.absolutePath), entropy())!!
        val id = JSONObject(a.call("node.info", "{}")).getJSONObject("result").getString("node_id")
        a.close()
        val b = EmberNode.create(cfg(dir.absolutePath), entropy())!!
        val id2 = JSONObject(b.call("node.info", "{}")).getJSONObject("result").getString("node_id")
        b.close()
        assertEquals("identity must persist in storage_dir", id, id2)
    }

    @Test fun runnerThreadDeliversEventsAndRpcWakes() {
        val n = EmberNode.create(cfg(), entropy())!!
        val events = CopyOnWriteArrayList<String>()
        val latch = CountDownLatch(1)
        val r = NodeRunner(n, object : OutboxSink {
            override fun onCoreEvent(json: String) { events.add(json); latch.countDown() }
            override fun onLinkTx(linkId: Int, data: ByteArray) {}
        })
        r.start()
        try {
            assertTrue(latch.await(5, TimeUnit.SECONDS))
            assertTrue(events.any { it.contains("node.ready") })
            val t0 = r.ticks
            assertNotNull(JSONObject(r.call("node.info", "{}")))
            Thread.sleep(200)
            assertTrue("rpc should wake the loop", r.ticks > t0)
        } finally {
            r.stop()
            n.close()
        }
    }
}
