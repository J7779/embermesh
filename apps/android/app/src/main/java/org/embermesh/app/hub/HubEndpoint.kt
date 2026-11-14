package org.embermesh.app.hub

import org.embermesh.app.host.Endpoint
import org.embermesh.app.host.EndpointFactory
import org.embermesh.app.host.EndpointSink
import org.embermesh.app.host.Fields
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.embermesh.app.host.Transport
import org.embermesh.app.util.int
import org.json.JSONArray
import org.json.JSONObject

/**
 * `ws_server`: the hub's /ws mesh bus as one datagram endpoint under a `raw` link (every guest shares
 * it: frames from the core go to all guests, a guest's frame goes to the core and the mesh engine
 * relays it to the others and to every other link).
 */
class HubEndpoint(private val ctx: HostContext, private val hub: PhoneHub, config: JSONObject, sink: EndpointSink,
                  private val probe: () -> JSONObject) : Endpoint(config, sink), PhoneHub.Sink {
    private val port = config.int("port", PhoneHub.DEFAULT_PORT).coerceIn(1, 65535)
    @Volatile private var open = false

    override fun open() {
        open = true
        ctx.io.execute {
            try {
                hub.start(port, this, probe())
                if (open) { sink.onState(true); sink.onStatus(hub.status()) } else hub.stop()
            } catch (e: HostError) {
                sink.onState(false, e.message)
            } catch (t: Throwable) {
                sink.onState(false, t.message ?: t.toString())
            }
        }
    }

    override fun close() {
        if (!open) return
        open = false
        hub.stop()
    }

    override fun write(data: ByteArray) { if (open) hub.broadcast(data) }

    override fun onGuestFrame(frame: ByteArray) { if (open) sink.onBytes(frame) }

    override fun onClients(clients: Int) { sink.onStatus(hub.status()) }

    override fun status(): JSONObject = hub.status()

    class Factory(private val hub: () -> PhoneHub?, private val probe: () -> JSONObject) : EndpointFactory {
        override val kind = "ws_server"
        override val title = "Hub (serve the app on Wi-Fi)"
        override val description = "Turns this phone into a hub: phones and laptops on the same Wi-Fi or on this phone's hotspot open http://<phone ip>:8437/ in a browser and are on the mesh."
        override val linkTypes = listOf("raw")
        override val transport = Transport.DATAGRAM
        override fun permissions(ctx: HostContext, config: JSONObject) = org.embermesh.app.util.Perms.localNetwork()
        override fun unavailableReason(ctx: HostContext): String? =
            if (!HubNative.available) "this build has no hub" else if (hub() == null) "hub not initialised" else null
        override fun configSchema(ctx: HostContext): JSONArray = Fields.of {
            number("port", "Port", default = PhoneHub.DEFAULT_PORT, min = 1024, max = 65535,
                help = "Guests open http://<this phone's address>:<port>/ (8437 is what every EmberMesh hub uses).")
        }
        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink): Endpoint =
            HubEndpoint(ctx, hub() ?: throw HostError.unsupported("hub not initialised"), config, sink, probe)
    }
}
