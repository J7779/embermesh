package org.embermesh.app.connectivity

import org.embermesh.app.core.CoreApi
import org.embermesh.app.host.HostError
import org.embermesh.app.host.rpc
import org.json.JSONObject
import java.util.concurrent.Executor

/**
 * Follows the core's connectivity state (RPC_API §11 revision 2): `connectivity.get` on node.ready
 * and the `connectivity.changed` event drive the nearby manager and the relay links. Nearby runs
 * only while `nearby.active`, relay links only while `internet.active` (both imply `ready`: setup
 * done and unlocked), and only while the app is in the foreground or its foreground service runs
 * (CONNECTIVITY §3.7, §4.9). When `ready` goes false everything stops and every automatic endpoint
 * is closed. Until the core has answered, nothing listens, advertises, browses or connects.
 */
class ConnectivityController(
    private val core: CoreApi,
    private val nearby: NearbyManager?,
    private val relays: RelayManager,
    /** Removes every open `auto:"nearby"` link (the switch went off). */
    private val closeNearbyLinks: () -> Unit,
    private val io: Executor,
    /** Extra nearby transports (the Bluetooth LE peer link): called with the token, or null to stop. */
    private val nearbyExtras: (String?) -> Unit = {},
    private val log: (String) -> Unit = {},
) {
    @Volatile var state: ConnectivityState = ConnectivityState.OFF; private set
    @Volatile private var known = false
    @Volatile var foreground = false; private set

    /** node.ready: re-read the switches (a locked node answers with an error → stay as we are). */
    fun refresh() = runIo {
        val c = try { core.rpc("connectivity.get") } catch (e: HostError) { log("connectivity.get: ${e.message}"); return@runIo }
        update(c)
    }

    /** `connectivity.changed` event data (a full Connectivity). */
    fun onChanged(data: JSONObject) = runIo { update(data) }

    /** Also re-applies when unchanged (back in front: a permission may have been granted meanwhile). */
    fun setForeground(on: Boolean) {
        if (foreground == on && !on) return
        foreground = on
        runIo { applyNow() }
    }

    /**
     * The default network changed or went away (CONNECTIVITY §3.6): re-advertise and re-browse on
     * the new network (the listener and the NSD registration are recreated).
     */
    fun networkChanged() = runIo {
        synchronized(this) {
            if (!known || !state.ready) return@runIo
            // A new token for the new network (RPC_API §11 connectivity.network_changed).
            val t = try { core.rpc("connectivity.network_changed").optString("token", "") } catch (e: HostError) { log("network_changed: ${e.message}"); "" }
            if (Regex("^[0-9a-f]{8}$").matches(t)) state = state.copy(token = t)
            nearby?.stop()
            applyNow()
        }
    }

    /** The node stopped (Quit): stop everything; the links go with the node. */
    @Synchronized
    fun shutdown() {
        known = false
        state = ConnectivityState.OFF
        nearby?.stop()
        nearbyExtras(null)
    }

    @Synchronized
    private fun update(c: JSONObject) {
        state = ConnectivityState.parse(c)
        known = true
        applyNow()
    }

    @Synchronized
    private fun applyNow() {
        if (!known) return
        val s = state
        val nearbyOn = s.nearbyActive && foreground && s.token != null
        if (nearby != null) {
            if (nearbyOn) nearby.start(s.token!!) else if (nearby.running) nearby.stop()
        }
        if (!s.nearbyActive) try { closeNearbyLinks() } catch (t: Throwable) { log("close nearby: $t") }
        try { nearbyExtras(if (nearbyOn) s.token else null) } catch (t: Throwable) { log("nearby extras: $t") }
        relays.apply(s, foreground)
    }

    private fun runIo(block: () -> Unit) {
        try { io.execute { try { block() } catch (t: Throwable) { log("connectivity: $t") } } } catch (_: Exception) {}
    }
}
