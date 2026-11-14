package org.embermesh.app.endpoint

import android.util.Log
import java.io.InputStream
import java.io.OutputStream
import java.util.concurrent.LinkedBlockingDeque
import java.util.concurrent.TimeUnit

/**
 * Bounded write queue drained by its own thread, so Endpoint.write() (called on the node thread)
 * never blocks. When full, the oldest item is dropped (the mesh retries; fresh data matters more).
 */
class QueuedWriter(
    private val name: String,
    private val capacity: Int = 256,
    private val writeFn: (ByteArray) -> Unit,
    private val onError: (Throwable) -> Unit,
) {
    private val q = LinkedBlockingDeque<ByteArray>()
    @Volatile private var running = true
    @Volatile var dropped = 0L
        private set
    private val thread = Thread({ loop() }, "ember-w-$name").apply { isDaemon = true; start() }

    fun offer(data: ByteArray) {
        if (!running) return
        while (q.size >= capacity) { q.pollFirst(); dropped++ }
        q.offerLast(data)
    }

    val depth: Int get() = q.size

    private fun loop() {
        while (running) {
            val item = try { q.pollFirst(500, TimeUnit.MILLISECONDS) } catch (_: InterruptedException) { break } ?: continue
            try {
                writeFn(item)
            } catch (t: Throwable) {
                if (running) {
                    running = false
                    onError(t)
                }
                break
            }
        }
    }

    fun close() {
        running = false
        q.clear()
        thread.interrupt()
    }
}

/** Reads an InputStream on a dedicated thread until EOF/error. */
class StreamReader(
    name: String,
    private val input: InputStream,
    private val onData: (ByteArray, Int) -> Unit,
    private val onEnd: (Throwable?) -> Unit,
    bufSize: Int = 4096,
) {
    @Volatile private var running = true
    private val buf = ByteArray(bufSize)
    private val thread = Thread({
        var err: Throwable? = null
        try {
            while (running) {
                val n = input.read(buf)
                if (n < 0) break
                if (n > 0) onData(buf, n)
            }
        } catch (t: Throwable) { err = t }
        if (running) { running = false; onEnd(err) }
    }, "ember-r-$name").apply { isDaemon = true; start() }

    fun close() { running = false; thread.interrupt() }
}

fun OutputStream.writeFlush(b: ByteArray) { write(b); flush() }

internal fun logw(tag: String, msg: String) { try { Log.w(tag, msg) } catch (_: Throwable) {} }
