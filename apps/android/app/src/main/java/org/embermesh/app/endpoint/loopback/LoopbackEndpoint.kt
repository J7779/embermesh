package org.embermesh.app.endpoint.loopback

import org.embermesh.app.host.Endpoint
import org.embermesh.app.host.EndpointFactory
import org.embermesh.app.host.EndpointSink
import org.embermesh.app.host.Fields
import org.embermesh.app.host.HostContext
import org.embermesh.app.util.int
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.TimeUnit

/** Echoes every written frame back into the link after an optional delay (testing / demos). */
class LoopbackEndpoint(private val ctx: HostContext, config: JSONObject, sink: EndpointSink) : Endpoint(config, sink) {
    @Volatile private var open = false
    private val delayMs = config.int("delay_ms", 0).toLong().coerceIn(0, 10_000)

    override fun open() {
        open = true
        sink.onState(true)
    }

    override fun close() {
        open = false
    }

    override fun write(data: ByteArray) {
        if (!open) return
        val copy = data.copyOf()
        if (delayMs == 0L) ctx.io.execute { if (open) sink.onBytes(copy) }
        else ctx.scheduler.schedule({ if (open) sink.onBytes(copy) }, delayMs, TimeUnit.MILLISECONDS)
    }

    object Factory : EndpointFactory {
        override val kind = "loopback"
        override val title = "Loopback (test)"
        override val description = "Echoes transmitted frames back to this node. For testing."
        override val linkTypes = listOf("raw")
        override val transport = org.embermesh.app.host.Transport.DATAGRAM
        override fun configSchema(ctx: HostContext): JSONArray = Fields.of {
            number("delay_ms", "Echo delay", default = 0, min = 0, max = 10000, unit = "ms")
        }
        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink) = LoopbackEndpoint(ctx, config, sink)
    }
}
