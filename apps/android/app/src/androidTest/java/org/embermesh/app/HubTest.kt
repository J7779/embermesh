package org.embermesh.app

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.endpoint.net.WebSocketEndpoint
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.net.HttpURLConnection
import java.net.URL

/**
 * The phone hub (host library Hub embedded via JNI) on the device: HTTP (web app, /hub.json,
 * /download/), and the /ws mesh bus carrying a real DM between a guest node and the phone's node.
 */
@RunWith(AndroidJUnit4::class)
class HubTest {
    private val app get() = InstrumentationRegistry.getInstrumentation().targetContext

    /** (status, first 64 KiB of the body); [lastLength] is the full body length (streamed, not kept). */
    private var lastLength = 0L
    private fun get(url: String): Pair<Int, ByteArray> {
        val c = URL(url).openConnection() as HttpURLConnection
        c.connectTimeout = 3000; c.readTimeout = 10000
        return try {
            val code = c.responseCode
            val head = java.io.ByteArrayOutputStream()
            var total = 0L
            ((if (code < 400) c.inputStream else c.errorStream) ?: java.io.ByteArrayInputStream(ByteArray(0))).use { inp ->
                val buf = ByteArray(64 * 1024)
                while (true) {
                    val n = inp.read(buf); if (n < 0) break
                    if (head.size() < 64 * 1024) head.write(buf, 0, minOf(n, 64 * 1024 - head.size()))
                    total += n
                }
            }
            lastLength = total
            code to head.toByteArray()
        } finally { c.disconnect() }
    }

    @Test fun hubServesWebAppAndCarriesMeshFrames() {
        // Android 17: serving the LAN needs ACCESS_LOCAL_NETWORK (the app prompts; the test grants it).
        if (android.os.Build.VERSION.SDK_INT >= 37) InstrumentationRegistry.getInstrumentation().uiAutomation
            .grantRuntimePermission(app.packageName, org.embermesh.app.util.Perms.LOCAL_NETWORK)
        assertTrue(EmberRuntime.start(app))
        val r = EmberRuntime.router
        val keep = InstrumentationRegistry.getArguments().getString("keep_hub") == "1"
        try {
            val st = r.handle("host.hub.start", JSONObject().put("port", 8437))
            assertTrue(st.toString(), st.getBoolean("running"))
            assertTrue(st.getJSONArray("urls").length() > 0)
            assertTrue(st.getJSONArray("urls").getString(0).matches(Regex("http://\\d+\\.\\d+\\.\\d+\\.\\d+:8437/")))
            // Idempotent; status agrees; the hub link is a normal core link of type raw.
            assertEquals(st.getInt("link_id"), r.handle("host.hub.start", JSONObject()).getInt("link_id"))
            val linkId = st.getInt("link_id")
            val coreLinks = JSONObject(EmberRuntime.core.call("links.list", "{}")).getJSONObject("result").getJSONArray("links")
            val hl = (0 until coreLinks.length()).map { coreLinks.getJSONObject(it) }.first { it.getInt("id") == linkId }
            assertEquals("raw", hl.getString("type"))
            assertEquals("ws_server", hl.getJSONObject("endpoint").getString("kind"))

            val (code, probe) = get("http://127.0.0.1:8437/hub.json")
            assertEquals(200, code)
            val pj = JSONObject(String(probe))
            assertTrue(pj.getBoolean("embermesh_hub"))
            assertEquals("/ws", pj.getString("ws"))
            val (ci, index) = get("http://127.0.0.1:8437/")
            assertEquals(200, ci)
            assertTrue(String(index).contains("<html", ignoreCase = true))
            val web = r.handle("host.hub.status", JSONObject()).getString("web_root")
            if (web == "web") {
                val (cw, wasm) = get("http://127.0.0.1:8437/wasm/ember.wasm")
                assertEquals(200, cw)
                assertEquals("WASM magic", 0x6d736100, (wasm[0].toInt() and 0xff) or ((wasm[1].toInt() and 0xff) shl 8) or ((wasm[2].toInt() and 0xff) shl 16) or ((wasm[3].toInt() and 0xff) shl 24))
                val (cg, _) = get("http://127.0.0.1:8437/data/guides.pack")
                assertEquals(200, cg)
                assertTrue(lastLength > 1_000_000)
            }
            val (cd, apk) = get("http://127.0.0.1:8437/download/EmberMesh.apk")
            assertEquals(200, cd)
            assertEquals('P'.code.toByte(), apk[0]); assertEquals('K'.code.toByte(), apk[1])

            // A guest node joins over ws://.../ws and exchanges a DM with the phone's own node.
            val guest = TestNode("guest", null, listOf(WebSocketEndpoint.Factory))
            try {
                TestNode.waitUntil(5000, "guest ready") { guest.readyCount >= 1 }
                guest.rpc("node.set_profile", JSONObject().put("name", "Guest"))
                val gl = guest.host("host.link.create", JSONObject().put("endpoint", JSONObject().put("kind", "websocket")
                    .put("config", JSONObject().put("url", "ws://127.0.0.1:8437/ws"))).put("link", JSONObject().put("type", "raw")
                    .put("config", JSONObject().put("preset", "websocket")))).getJSONObject("link").getInt("id")
                TestNode.waitUntil(5000, "guest connected") { guest.links.active(gl)?.connected == true }
                TestNode.waitUntil(5000, "hub sees a client") { r.handle("host.hub.status", JSONObject()).getInt("clients") >= 1 }
                val phoneCard = JSONObject(EmberRuntime.core.call("identity.card", "{}")).getJSONObject("result").getString("card")
                val phoneId = JSONObject(EmberRuntime.core.call("node.info", "{}")).getJSONObject("result").getString("node_id")
                guest.rpc("contacts.add", JSONObject().put("card", phoneCard))
                EmberRuntime.core.call("contacts.add", JSONObject().put("card", guest.rpc("identity.card").getString("card")).toString())
                val text = "via the phone hub ${System.nanoTime()}"
                val m = guest.rpc("messages.send", JSONObject().put("conv", "dm:$phoneId").put("text", text)).getJSONObject("message")
                TestNode.waitUntil(15_000, "phone node received the DM") {
                    val ms = JSONObject(EmberRuntime.core.call("messages.list", JSONObject().put("conv", "dm:${guest.nodeId}").toString()))
                        .getJSONObject("result").getJSONArray("messages")
                    (0 until ms.length()).any { ms.getJSONObject(it).getString("text") == text }
                }
                assertNotNull(guest.waitEvent(15_000) { it.optString("event") == "message.status" && it.getJSONObject("data").optString("id") == m.getString("id") &&
                    it.getJSONObject("data").optString("status") in setOf("delivered", "read") })
            } finally { guest.close() }
        } finally {
            if (!keep) {
                val s = r.handle("host.hub.stop", JSONObject())
                assertTrue(!s.getBoolean("running"))
                try { get("http://127.0.0.1:8437/hub.json"); throw AssertionError("hub still answering") } catch (_: java.io.IOException) {}
            }
        }
    }
}
