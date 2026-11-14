package org.embermesh.app.core

import android.content.res.AssetManager
import java.util.concurrent.locks.ReentrantReadWriteLock
import kotlin.concurrent.read
import kotlin.concurrent.write

/**
 * Owns one native ember_node. All methods are thread-safe: the native API has its own lock and the
 * read/write lock here guarantees [close] never races a call in flight.
 */
class EmberNode private constructor(private var handle: Long) {
    private val lock = ReentrantReadWriteLock()
    @Volatile var waker: (() -> Unit)? = null

    val isOpen: Boolean get() = lock.read { handle != 0L }

    fun call(method: String, paramsJson: String): String = lock.read {
        if (handle == 0L) NOT_RUNNING else EmberNative.call(handle, method, paramsJson)
    }

    fun tick(nowMs: Long) = lock.read { if (handle != 0L) EmberNative.tick(handle, nowMs) }

    fun nextWakeupMs(): Int = lock.read { if (handle != 0L) EmberNative.nextWakeupMs(handle) else 60_000 }

    fun addEntropy(data: ByteArray) = lock.read { if (handle != 0L) EmberNative.addEntropy(handle, data) }

    fun linkRx(linkId: Int, data: ByteArray, off: Int, len: Int) = lock.read {
        if (handle != 0L) EmberNative.linkRx(handle, linkId, data, off, len)
    }

    fun audioRx(linkId: Int, samples: ShortArray, count: Int, sampleRate: Int) = lock.read {
        if (handle != 0L) EmberNative.audioRx(handle, linkId, samples, count, sampleRate)
    }

    fun audioTx(linkId: Int, out: ShortArray, max: Int, sampleRate: Int): Int = lock.read {
        if (handle != 0L) EmberNative.audioTx(handle, linkId, out, max, sampleRate) else 0
    }

    /** Pops one outbox item, or null. Only the node thread should poll. */
    fun poll(): OutItem? = lock.read {
        if (handle == 0L) return null
        val raw = EmberNative.poll(handle) ?: return null
        OutItem.decode(raw)
    }

    fun loadGuidesAsset(assets: AssetManager, path: String): Int =
        lock.read { if (handle != 0L) EmberNative.guidesLoadAsset(handle, assets, path) else -1 }

    fun loadGuidesFile(path: String): Int = lock.read { if (handle != 0L) EmberNative.guidesLoadFile(handle, path) else -1 }

    fun loadGuides(pack: ByteArray): Int = lock.read { if (handle != 0L) EmberNative.guidesLoad(handle, pack) else -1 }

    fun close() = lock.write {
        if (handle != 0L) {
            EmberNative.destroy(handle)
            handle = 0L
        }
    }

    companion object {
        const val NOT_RUNNING = """{"ok":false,"error":{"code":"internal","message":"node not running"}}"""

        fun version(): String {
            EmberNative.load()
            return EmberNative.version()
        }

        /** Returns null if the core refused to create a node (allocation failure / bad entropy). */
        fun create(configJson: String, entropy: ByteArray): EmberNode? {
            EmberNative.load()
            val h = EmberNative.create(configJson, entropy)
            return if (h == 0L) null else EmberNode(h)
        }
    }
}

/** One decoded outbox item. */
class OutItem(val kind: Int, val linkId: Int, val data: ByteArray) {
    val text: String get() = String(data, Charsets.UTF_8)

    companion object {
        fun decode(raw: ByteArray): OutItem? {
            if (raw.size < 8) return null
            fun le(o: Int) = (raw[o].toInt() and 0xff) or ((raw[o + 1].toInt() and 0xff) shl 8) or
                ((raw[o + 2].toInt() and 0xff) shl 16) or ((raw[o + 3].toInt() and 0xff) shl 24)
            return OutItem(le(0), le(4), raw.copyOfRange(8, raw.size))
        }
    }
}
