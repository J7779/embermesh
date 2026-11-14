package org.embermesh.app.endpoint.ble

import java.util.ArrayDeque
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit

/**
 * Serialises GATT operations on one connection (Android allows a single outstanding operation).
 * Every method must be called on [exec]'s single thread; the matching GATT callback calls
 * [complete]. An operation that never completes is abandoned after its timeout.
 */
class GattOpQueue(private val exec: ScheduledExecutorService) {
    enum class Start { STARTED, BUSY, FAILED }

    private class Op(val name: String, val timeoutMs: Long, val start: () -> Start) {
        var busyRetries = 0
    }

    private val q = ArrayDeque<Op>()
    private var current: Op? = null
    private var timeout: ScheduledFuture<*>? = null
    private var retryPending = false

    val size: Int get() = q.size + if (current != null) 1 else 0
    val currentName: String? get() = current?.name

    fun enqueue(name: String, timeoutMs: Long, start: () -> Start) {
        q.addLast(Op(name, timeoutMs, start))
        pump()
    }

    /** The in-flight operation finished (success or failure). */
    fun complete() {
        timeout?.cancel(false)
        timeout = null
        current = null
        pump()
    }

    fun clear() {
        timeout?.cancel(false)
        timeout = null
        current = null
        q.clear()
    }

    private fun pump() {
        while (current == null && !retryPending && q.isNotEmpty()) {
            val op = q.first()
            val r = try { op.start() } catch (t: Throwable) { Start.FAILED }
            when (r) {
                Start.STARTED -> {
                    q.removeFirst()
                    current = op
                    timeout = try {
                        exec.schedule({ if (current === op) complete() }, op.timeoutMs, TimeUnit.MILLISECONDS)
                    } catch (_: Exception) { null }
                }
                Start.BUSY -> {
                    if (++op.busyRetries > 100) { q.removeFirst(); continue }
                    retryPending = true
                    try {
                        exec.schedule({ retryPending = false; pump() }, 5, TimeUnit.MILLISECONDS)
                    } catch (_: Exception) { retryPending = false; return }
                    return
                }
                Start.FAILED -> q.removeFirst()
            }
        }
    }
}
