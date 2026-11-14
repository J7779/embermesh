package org.embermesh.app.bridge

import org.embermesh.app.host.FakeCore
import org.embermesh.app.host.HostError
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.concurrent.Executor

class BridgeDispatcherTest {
    private val js = ArrayList<String>()
    private val direct = Executor { it.run() }
    private val hostCalls = ArrayList<String>()
    private val d = BridgeDispatcher(FakeCore(), { m, p ->
        hostCalls.add(m)
        when (m) {
            "host.info" -> JSONObject().put("platform", "android").put("echo", p.optString("x"))
            "host.fail" -> throw HostError("denied", "nope")
            else -> throw RuntimeException("boom")
        }
    }, direct) { js.add(it) }

    /** Extracts the object passed to onReply(...) */
    private fun reply(s: String): JSONObject {
        assertTrue(s, s.startsWith("window.__emberBridge&&window.__emberBridge.onReply("))
        return JSONObject(s.substringAfter("onReply(").removeSuffix(");"))
    }

    @Test fun coreCallIsForwardedWithIdSpliced() {
        d.onMessage("""{"id":7,"method":"node.info","params":{}}""")
        val r = reply(js.single())
        assertEquals(7, r.getInt("id")); assertTrue(r.getBoolean("ok"))
        assertEquals("0123456789abcdef", r.getJSONObject("result").getString("node_id"))
        assertTrue(hostCalls.isEmpty())
    }

    @Test fun hostCallRoutedToHost() {
        d.onMessage("""{"id":"a1","method":"host.info","params":{"x":"y"}}""")
        val r = reply(js.single())
        assertEquals("a1", r.getString("id")); assertEquals("y", r.getJSONObject("result").getString("echo"))
        assertEquals(listOf("host.info"), hostCalls)
    }

    @Test fun errorsBecomeEnvelopes() {
        d.onMessage("""{"id":1,"method":"host.fail"}""")
        d.onMessage("""{"id":2,"method":"host.other"}""")
        d.onMessage("""{"id":3,"method":"messages.send","params":{}}""")
        val codes = js.map { reply(it).getJSONObject("error").getString("code") }
        assertEquals(listOf("denied", "internal", "not_implemented"), codes)
        js.forEach { assertFalse(reply(it).getBoolean("ok")) }
    }

    @Test fun malformedRequests() {
        d.onMessage("not json")
        d.onMessage("""{"id":4}""")
        d.onMessage(null)
        val r = js.map { reply(it) }
        assertEquals("bad_params", r[0].getJSONObject("error").getString("code"))
        assertTrue(r[0].isNull("id"))
        assertEquals(4, r[1].getInt("id"))
        assertEquals("too_large", r[2].getJSONObject("error").getString("code"))
    }

    @Test fun eventJsEscapesLineSeparators() {
        val ev = BridgeDispatcher.eventJson("message.new", JSONObject().put("text", "a b"))
        val s = BridgeDispatcher.eventJs(ev)
        assertFalse(s.contains(' '))
        val obj = JSONObject(s.substringAfter("onEvent(").removeSuffix(");"))
        assertEquals("a b", obj.getJSONObject("data").getString("text"))
        assertEquals("message.new", obj.getString("event"))
    }

    @Test fun replyJsEdgeCases() {
        assertEquals(JSONObject("{\"id\":null}").toString(), JSONObject(BridgeDispatcher.replyJs(null, "{}").substringAfter("onReply(").removeSuffix(");")).toString())
        val r = JSONObject(BridgeDispatcher.replyJs("q\"x", """{"ok":true,"result":{}}""").substringAfter("onReply(").removeSuffix(");"))
        assertEquals("q\"x", r.getString("id"))
    }
}
