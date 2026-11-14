package org.embermesh.app.connectivity

import org.embermesh.app.endpoint.QueuedWriter
import org.embermesh.app.endpoint.StreamReader
import org.embermesh.app.endpoint.writeFlush
import org.embermesh.app.host.Endpoint
import org.embermesh.app.host.EndpointSink
import org.json.JSONObject
import java.net.Socket
import java.util.concurrent.atomic.AtomicBoolean

/**
 * The endpoint of one nearby link: a TCP socket that is already connected (accepted by the nearby
 * listener or dialled after discovery). It is never reopened: when it ends the link goes away and
 * discovery dials again. [onClosed] runs exactly once.
 */
class NearbySocketEndpoint(
    private val socket: Socket,
    config: JSONObject,
    sink: EndpointSink,
    private val onClosed: () -> Unit,
) : Endpoint(config, sink) {
    private var writer: QueuedWriter? = null
    private var reader: StreamReader? = null
    private val closed = AtomicBoolean(false)
    private val remote = "${socket.inetAddress?.hostAddress}:${socket.port}"

    override fun open() {
        val out = socket.getOutputStream()
        writer = QueuedWriter("nearby", 512, { out.writeFlush(it) }) { t -> end(t) }
        reader = StreamReader("nearby", socket.getInputStream(), { b, n -> sink.onBytes(b, 0, n) }, { t -> end(t) })
        sink.onState(true)
    }

    private fun end(t: Throwable?) {
        if (closed.get()) return
        sink.onState(false, t?.message ?: "connection closed")
        close()
    }

    override fun write(data: ByteArray) { writer?.offer(data) }

    override fun close() {
        if (!closed.compareAndSet(false, true)) return
        writer?.close()
        reader?.close()
        try { socket.close() } catch (_: Exception) {}
        try { onClosed() } catch (_: Throwable) {}
    }

    override fun status(): JSONObject = JSONObject().put("remote", remote).put("connected", !closed.get())
}
