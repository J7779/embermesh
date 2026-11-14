package org.embermesh.app

import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.core.EmberNode
import org.embermesh.app.core.NodeRunner
import org.embermesh.app.core.OutboxSink
import org.embermesh.app.host.AppControls
import org.embermesh.app.host.EndpointFactory
import org.embermesh.app.host.EndpointRegistry
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostRouter
import org.embermesh.app.host.LinkManager
import org.embermesh.app.host.rpc
import org.embermesh.app.services.NoDeviceServices
import org.json.JSONObject
import java.io.File
import java.security.SecureRandom
import java.util.concurrent.CopyOnWriteArrayList

/**
 * A complete Android host stack around one REAL core node, as EmberRuntime builds it (node thread,
 * LinkManager, HostRouter, node.ready → links sync, link.ptt → PTT), but with its own storage and
 * endpoint factories so several nodes can live in one test process.
 */
class TestNode(
    val label: String, val dir: File?, factories: List<EndpointFactory>,
    /** With a platform: Nearby devices + internet relay exactly as EmberRuntime wires them (RPC_API §11). */
    nearbyPlatform: org.embermesh.app.connectivity.NearbyPlatform? = null,
) : OutboxSink {
    val app = InstrumentationRegistry.getInstrumentation().targetContext
    val events = CopyOnWriteArrayList<JSONObject>()
    val ctx = HostContext(app) { e, d -> events.add(JSONObject().put("event", e).put("data", d)) }
    val node: EmberNode = EmberNode.create(cfg(dir), ByteArray(32).also { SecureRandom().nextBytes(it) })!!
    val runner = NodeRunner(node, this)
    val registry = EndpointRegistry(factories)
    val links = LinkManager(runner, registry, ctx) { /* permissions pre-granted in tests */ }
    val router = HostRouter(ctx, runner, registry, links, NoDeviceServices(), object : AppControls {
        override val appVersion = "test"
        override fun notifyUser(title: String, body: String) {}
        override fun setKeepAwake(on: Boolean) {}
        override fun androidSettings() = JSONObject()
        override fun setAndroidSettings(s: JSONObject) = JSONObject()
        override fun openBatterySettings() = false
        override fun onLocationFix(pos: JSONObject) {}
    })
    @Volatile var readyCount = 0
    val connectivity: org.embermesh.app.connectivity.ConnectivityWiring.Parts? =
        nearbyPlatform?.let { org.embermesh.app.connectivity.ConnectivityWiring.create(ctx, runner, links, it) { m -> android.util.Log.i("TestNode", "$label: $m") } }

    init {
        ctx.pttLinks = { links.pttCandidates() }
        connectivity?.controller?.setForeground(true)
        runner.start()
    }

    override fun onCoreEvent(json: String) {
        val o = JSONObject(json)
        events.add(o)
        val data = o.optJSONObject("data") ?: JSONObject()
        when (o.optString("event")) {
            "node.ready" -> ctx.io.execute {
                try { registry.setCoreLinkTypes(runner.rpc("links.types")) } catch (_: Throwable) {}
                links.sync()
                connectivity?.controller?.refresh()
                readyCount++
            }
            "link.ptt" -> links.onPtt(data.optInt("link_id"), data.optBoolean("on"))
            "connectivity.changed", "link.removed" -> org.embermesh.app.connectivity.ConnectivityWiring.onCoreEvent(o.optString("event"), data, connectivity?.controller, links)
        }
    }

    override fun onLinkTx(linkId: Int, data: ByteArray) = links.onLinkTx(linkId, data)

    fun rpc(method: String, params: JSONObject = JSONObject()): JSONObject = runner.rpc(method, params)
    fun call(method: String, params: JSONObject = JSONObject()): JSONObject = JSONObject(runner.call(method, params.toString()))
    fun host(method: String, params: JSONObject = JSONObject()): JSONObject = router.handle(method, params)
    val nodeId: String get() = rpc("node.info").getString("node_id")

    fun waitEvent(timeoutMs: Long, pred: (JSONObject) -> Boolean): JSONObject? {
        val end = System.currentTimeMillis() + timeoutMs
        while (System.currentTimeMillis() < end) {
            events.firstOrNull(pred)?.let { return it }
            Thread.sleep(20)
        }
        return null
    }

    fun close() {
        connectivity?.controller?.shutdown()
        links.closeAll()
        runner.stop()
        node.close()
        ctx.shutdown()
    }

    companion object {
        fun cfg(dir: File?) = JSONObject().put("storage_dir", dir?.absolutePath ?: "").put("platform", "android")
            .put("app_version", "test").put("device_name", "emulator").put("now_ms", System.currentTimeMillis()).toString()

        fun freshDir(name: String): File =
            File(InstrumentationRegistry.getInstrumentation().targetContext.cacheDir, name).apply { deleteRecursively(); mkdirs() }

        fun waitUntil(timeoutMs: Long, what: String = "condition", pred: () -> Boolean) {
            val end = System.currentTimeMillis() + timeoutMs
            while (System.currentTimeMillis() < end) {
                if (try { pred() } catch (_: Throwable) { false }) return
                Thread.sleep(25)
            }
            throw AssertionError("timed out after $timeoutMs ms waiting for $what")
        }
    }
}
