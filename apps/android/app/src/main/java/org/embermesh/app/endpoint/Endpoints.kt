package org.embermesh.app.endpoint

import org.embermesh.app.endpoint.loopback.LoopbackEndpoint
import org.embermesh.app.host.Endpoint
import org.embermesh.app.host.EndpointFactory
import org.embermesh.app.host.EndpointSink
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.json.JSONArray
import org.json.JSONObject

/** Every endpoint kind of the Android app, in host.info order. */
object Endpoints {
    /** [hub] / [hubProbe]: the phone hub behind the `ws_server` kind (null = not offered, e.g. unit tests). */
    fun factories(hub: (() -> org.embermesh.app.hub.PhoneHub?)? = null, hubProbe: () -> JSONObject = { JSONObject() }): List<EndpointFactory> = listOfNotNull(
        org.embermesh.app.endpoint.ble.BlePeerEndpoint.Factory,                              // ENDPOINT:ble_peer
        org.embermesh.app.endpoint.net.UdpEndpoint.Factory,                                  // ENDPOINT:udp
        org.embermesh.app.endpoint.audio.AudioEndpoint.Factory,                              // ENDPOINT:audio
        org.embermesh.app.endpoint.serial.UsbSerialEndpoint.Factory,                         // ENDPOINT:serial
        org.embermesh.app.endpoint.ble.BleUartEndpoint.Factory,                              // ENDPOINT:ble
        org.embermesh.app.endpoint.bt.SppEndpoint.Factory,                                   // ENDPOINT:bt_spp
        org.embermesh.app.endpoint.net.TcpEndpoint.Factory,                                  // ENDPOINT:tcp
        org.embermesh.app.endpoint.net.TcpServerEndpoint.Factory,                            // ENDPOINT:tcp_server
        org.embermesh.app.endpoint.net.WebSocketEndpoint.Factory,                            // ENDPOINT:websocket
        org.embermesh.app.endpoint.wifidirect.WifiDirectEndpoint.Factory,                    // ENDPOINT:wifi_direct
        hub?.let { org.embermesh.app.hub.HubEndpoint.Factory(it, hubProbe) },                // ENDPOINT:ws_server
        LoopbackEndpoint.Factory,
    )

    /** Placeholder for a kind this build does not implement: listed in host.info as unavailable. */
    class Unavailable(override val kind: String, override val title: String, override val linkTypes: List<String>,
                      private val reason: String = "not implemented in this build") : EndpointFactory {
        override val description = title
        override fun configSchema(ctx: HostContext) = JSONArray()
        override fun unavailableReason(ctx: HostContext) = reason
        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink): Endpoint = throw HostError.unsupported(reason)
    }
}
