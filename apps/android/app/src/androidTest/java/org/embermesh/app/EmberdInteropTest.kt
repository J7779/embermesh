package org.embermesh.app

import android.util.Base64
import android.util.Log
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.json.JSONObject
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Cross-platform over real sockets: the app's own runtime (EmberRuntime, the real router) against the
 * desktop daemon emberd running on the development machine (10.0.2.2 from the emulator).
 * Driven by apps/android/scripts/e2e_emberd.py, which starts emberd, passes its endpoints and card,
 * injects emberd's messages and checks what arrived there. Skipped without the arguments:
 *   emberd_url   endpoint for the app to open, e.g. ws://10.0.2.2:18437/ws or tcp://10.0.2.2:18440
 *   emberd_card  base64 of emberd's identity.card
 *   nonce        unique text tag of this run
 *   expect_public optional: exact text that must show up in the app's own Public channel within 20 s
 *                (sent by another node on the mesh); reported as an instrumentation status line
 *                "interop expect-public: ok (from <node_id>, signed_ok <bool>)" or "... timeout"
 */
@RunWith(AndroidJUnit4::class)
class EmberdInteropTest {
    private val args = InstrumentationRegistry.getArguments()
    private val app get() = InstrumentationRegistry.getInstrumentation().targetContext

    private fun core(method: String, p: JSONObject = JSONObject()): JSONObject {
        val r = JSONObject(EmberRuntime.core.call(method, p.toString()))
        if (!r.getBoolean("ok")) throw AssertionError("$method: $r")
        return r.getJSONObject("result")
    }

    private fun texts(conv: String): List<JSONObject> {
        val a = core("messages.list", JSONObject().put("conv", conv).put("limit", 200)).getJSONArray("messages")
        return (0 until a.length()).map { a.getJSONObject(it) }
    }

    @Test fun chatWithEmberd() {
        val url: String = args.getString("emberd_url") ?: run { assumeTrue("run through scripts/e2e_emberd.py", false); return }
        val nonce = args.getString("nonce") ?: "n"
        val card = String(Base64.decode(args.getString("emberd_card"), Base64.DEFAULT))
        val emberdId = Regex("id=([0-9a-f]{16})").find(card)!!.groupValues[1]
        // Android 17: LAN sockets need ACCESS_LOCAL_NETWORK (the app prompts; the test grants it).
        if (android.os.Build.VERSION.SDK_INT >= 37) InstrumentationRegistry.getInstrumentation().uiAutomation
            .grantRuntimePermission(app.packageName, org.embermesh.app.util.Perms.LOCAL_NETWORK)
        assertTrue(EmberRuntime.start(app))
        val r = EmberRuntime.router
        if (core("node.info").getString("name").isEmpty()) core("node.set_profile", JSONObject().put("name", "Android E2E"))
        val ep = when {
            url.startsWith("ws") -> JSONObject().put("kind", "websocket").put("config", JSONObject().put("url", url))
            url.startsWith("tcp://") -> url.removePrefix("tcp://").split(":").let {
                JSONObject().put("kind", "tcp").put("config", JSONObject().put("host", it[0]).put("port", it[1].toInt()))
            }
            url.startsWith("udp") -> JSONObject().put("kind", "udp").put("config", JSONObject())
            else -> throw AssertionError("bad url $url")
        }
        val type = if (url.startsWith("tcp://")) "stream" else "raw"
        val preset = when { url.startsWith("ws") -> "websocket"; url.startsWith("tcp") -> "tcp"; else -> "udp_lan" }
        val link = r.handle("host.link.create", JSONObject().put("name", "emberd $url").put("endpoint", ep)
            .put("link", JSONObject().put("type", type).put("config", JSONObject().put("preset", preset)))).getJSONObject("link").getInt("id")
        try {
            try { TestNode.waitUntil(10_000, "connected to emberd") { EmberRuntime.links.active(link)?.connected == true } }
            catch (e: AssertionError) { throw AssertionError("${e.message}: ${EmberRuntime.links.statusJson()} ${EmberRuntime.links.active(link)?.error}") }
            core("contacts.add", JSONObject().put("card", card))
            // 1. Public channel, app -> emberd (the script checks emberd's ch:public).
            core("messages.send", JSONObject().put("conv", "ch:public").put("text", "android->emberd public $nonce"))
            // 2. Public channel, emberd -> app (the script sends it repeatedly until we stop).
            TestNode.waitUntil(60_000, "emberd's public message") { texts("ch:public").any { it.getString("text") == "emberd->android public $nonce" } }
            // 3. DM app -> emberd with delivery receipt.
            val m = core("messages.send", JSONObject().put("conv", "dm:$emberdId").put("text", "android->emberd dm $nonce")).getJSONObject("message")
            TestNode.waitUntil(60_000, "delivery receipt from emberd") {
                texts("dm:$emberdId").any { it.getString("id") == m.getString("id") && it.getString("status") in setOf("delivered", "read") }
            }
            // 4. DM emberd -> app.
            TestNode.waitUntil(60_000, "emberd's DM") { texts("dm:$emberdId").any { it.getString("text") == "emberd->android dm $nonce" && !it.getBoolean("outgoing") } }
            // 5. Optional: a Public message from another node on the mesh (e.g. a browser guest of the hub).
            args.getString("expect_public")?.takeIf { it.isNotEmpty() }?.let { want ->
                val got = { texts("ch:public").firstOrNull { it.getString("text") == want && !it.getBoolean("outgoing") } }
                val arrived = runCatching { TestNode.waitUntil(20_000, "expected Public text") { got() != null } }.isSuccess
                val m = got()
                val line = if (arrived && m != null) "interop expect-public: ok (from ${m.optString("from")}, signed_ok ${m.opt("signed_ok")})"
                           else "interop expect-public: timeout"
                Log.i("EmberdInterop", line)
                InstrumentationRegistry.getInstrumentation().sendStatus(99, android.os.Bundle().apply { putString("stream", line + "\n") })
                assertTrue("expected Public text '$want' in the app's Public channel", arrived && m != null)
            }
            val peers = core("mesh.peers").getJSONArray("peers")
            Log.i("EmberdInterop", "OK over $url: peers=$peers dm=${texts("dm:$emberdId").map { it.getString("text") + "/" + it.getString("status") }}")
        } finally {
            r.handle("host.link.remove", JSONObject().put("id", link))
        }
    }
}
