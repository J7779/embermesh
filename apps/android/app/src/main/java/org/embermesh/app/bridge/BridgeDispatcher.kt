package org.embermesh.app.bridge

import org.embermesh.app.core.CoreApi
import org.embermesh.app.host.HostError
import org.embermesh.app.util.Json
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.Executor

/**
 * The native side of the WebView bridge (RPC_API §5), independent of Android so it is unit-tested:
 *   JS → native: postMessage(JSON.stringify({id, method, params}))
 *   native → JS: window.__emberBridge.onReply({id, ok, result?, error?})
 *                window.__emberBridge.onEvent({event, data})
 * `host.*` goes to [host], everything else to the core. Calls run on [executor] so slow methods
 * (QR scan, file pick, BLE scan) never block the WebView's binder thread.
 */
class BridgeDispatcher(
    private val core: CoreApi,
    private val host: (method: String, params: JSONObject) -> JSONObject,
    private val executor: Executor,
    /** Receives JavaScript to evaluate in the page (the caller hops to the UI thread). */
    private val evalJs: (String) -> Unit,
) {
    fun onMessage(text: String?) {
        if (text == null || text.length > MAX_MESSAGE) {
            evalJs(replyJs(null, error(HostError.TOO_LARGE, "message too large")))
            return
        }
        val req = try { JSONObject(text) } catch (e: Exception) {
            evalJs(replyJs(null, error(HostError.BAD_PARAMS, "request is not a JSON object")))
            return
        }
        val id = req.opt("id")
        val method = req.optString("method", "")
        if (method.isEmpty()) {
            evalJs(replyJs(id, error(HostError.BAD_PARAMS, "method required")))
            return
        }
        val params = req.optJSONObject("params") ?: JSONObject()
        try {
            executor.execute { evalJs(replyJs(id, dispatch(method, params))) }
        } catch (e: Exception) {
            evalJs(replyJs(id, error(HostError.BUSY, "bridge shutting down")))
        }
    }

    /** Returns the reply envelope text {"ok":...}. */
    fun dispatch(method: String, params: JSONObject): String {
        return if (method.startsWith("host.")) {
            try {
                JSONObject().put("ok", true).put("result", host(method, params)).toString()
            } catch (e: HostError) {
                error(e.code, e.message ?: e.code)
            } catch (t: Throwable) {
                error(HostError.INTERNAL, t.message ?: t.toString())
            }
        } else {
            val r = try { core.call(method, params.toString()) } catch (t: Throwable) { return error(HostError.INTERNAL, t.toString()) }
            if (r.startsWith("{")) r else error(HostError.INTERNAL, "malformed core reply")
        }
    }

    companion object {
        const val MAX_MESSAGE = 48 * 1024 * 1024

        fun error(code: String, message: String): String =
            JSONObject().put("ok", false).put("error", JSONObject().put("code", code).put("message", message)).toString()

        /** Splices the request id into a reply envelope without re-serialising (results can be large). */
        fun replyJs(id: Any?, envelope: String): String {
            val idJson = if (id == null || id == JSONObject.NULL) "null" else JSONArray().put(id).toString().let { it.substring(1, it.length - 1) }
            val body = envelope.trim()
            val rest = if (body.length > 2) body.substring(1) else "}"
            val sep = if (body.length > 2) "," else ""
            return "window.__emberBridge&&window.__emberBridge.onReply({\"id\":$idJson$sep${Json.jsSafe(rest)});"
        }

        /** [eventJson] is a full {"event":...,"data":...} object as produced by the core outbox. */
        fun eventJs(eventJson: String): String =
            "window.__emberBridge&&window.__emberBridge.onEvent(${Json.jsSafe(eventJson)});"

        fun eventJson(event: String, data: JSONObject): String = JSONObject().put("event", event).put("data", data).toString()
    }
}
