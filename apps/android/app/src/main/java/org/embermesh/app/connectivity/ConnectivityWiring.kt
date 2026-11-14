package org.embermesh.app.connectivity

import org.embermesh.app.core.CoreApi
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.embermesh.app.host.LinkManager
import org.json.JSONObject

/** Builds the nearby manager + relay manager + controller around one node's LinkManager. */
object ConnectivityWiring {
    class Parts(val controller: ConnectivityController, val nearby: NearbyManager)

    fun create(
        ctx: HostContext,
        core: CoreApi,
        links: LinkManager,
        platform: NearbyPlatform,
        nearbyExtras: (String?) -> Unit = {},
        log: (String) -> Unit = {},
    ): Parts {
        val nm = NearbyManager(
            platform = platform,
            attach = { role, peerToken, socket, onClosed ->
                val addr = NearbyManager.addressText(socket.inetAddress)
                val ep = JSONObject().put("host", addr).put("port", socket.port)
                // RPC_API §11: auto_key "<token>@<address>" when dialing, "@<address>" when accepting;
                // preset "nearby": the core sends and checks the 8-byte hello.
                val key = if (role == "dial") "$peerToken@$addr" else "@$addr"
                links.createAuto("Nearby device", "tcp", ep, "stream", JSONObject().put("preset", "nearby"),
                    "nearby", role, key) { sink -> NearbySocketEndpoint(socket, ep, sink, onClosed) }
            },
            io = ctx.io, sched = ctx.scheduler, log = log,
        )
        val relayLinks = object : RelayLinks {
            override fun current(): Map<String, Int> = links.autoLinks("relay").mapNotNull { a -> a.def.autoKey?.let { it to a.def.id } }.toMap()
            override fun create(params: JSONObject) { links.create(params) }
            override fun remove(id: Int) = links.remove(id)
        }
        val ctl = ConnectivityController(
            core = core, nearby = nm,
            relays = RelayManager(relayLinks, log = log),
            closeNearbyLinks = {
                for (a in links.autoLinks("nearby")) try { links.remove(a.def.id) } catch (_: HostError) {}
            },
            io = ctx.io, nearbyExtras = nearbyExtras, log = log,
        )
        return Parts(ctl, nm)
    }

    /** Core events the host acts on (RPC_API §11). Returns true when handled. */
    fun onCoreEvent(event: String, data: JSONObject, ctl: ConnectivityController?, links: LinkManager): Boolean = when (event) {
        "connectivity.changed" -> { ctl?.onChanged(data); true }
        "link.removed" -> { links.onRemoved(data.optInt("id")); true }
        else -> false
    }
}
