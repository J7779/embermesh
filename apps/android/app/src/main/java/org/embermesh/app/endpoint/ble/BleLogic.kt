package org.embermesh.app.endpoint.ble

import java.util.ArrayDeque
import java.util.Locale
import java.util.UUID

/*
 * Pure (Android-free) BLE logic, unit-tested on the JVM: UUIDs, chunking, stream arbitration,
 * connection tie-breaking and the scan-start budget.
 */

object BleUuids {
    /** Ember phone<->phone byte-stream service (ARCHITECTURE §6). */
    val EMBER_SERVICE: UUID = UUID.fromString("8e3b0001-5d1f-4c6a-9b7e-0d2a6f1c4e10")
    val EMBER_RX: UUID = UUID.fromString("8e3b0002-5d1f-4c6a-9b7e-0d2a6f1c4e10")
    val EMBER_TX: UUID = UUID.fromString("8e3b0003-5d1f-4c6a-9b7e-0d2a6f1c4e10")

    /** Nordic UART service (BLE TNCs, serial bridges). RX = we write, TX = they notify. */
    val NUS_SERVICE: UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    val NUS_RX: UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
    val NUS_TX: UUID = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")

    /** Client Characteristic Configuration Descriptor. */
    val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    /** Company id 0xFFFF ("reserved for testing") carries our 4-byte instance token in adverts. */
    const val TOKEN_COMPANY_ID = 0xFFFF

    private val HEX = Regex("^[0-9a-fA-F]+$")

    /**
     * Parses a full 128-bit UUID or a 16/32-bit short form ("180F", "0x2a37", "6e400001") expanded
     * on the Bluetooth base UUID. Returns null for anything else.
     */
    fun parse(text: String?): UUID? {
        var t = text?.trim() ?: return null
        if (t.isEmpty()) return null
        if (t.startsWith("0x") || t.startsWith("0X")) t = t.substring(2)
        if (t.length == 36) return try { UUID.fromString(t) } catch (_: IllegalArgumentException) { null }
        if ((t.length == 4 || t.length == 8) && HEX.matches(t)) {
            return UUID.fromString(t.padStart(8, '0').lowercase(Locale.ROOT) + "-0000-1000-8000-00805f9b34fb")
        }
        return null
    }

    fun parseOr(text: String?, def: UUID): UUID = parse(text) ?: def
}

object BleChunker {
    /** Largest ATT attribute value is 512 bytes; usable payload of an MTU is MTU-3. */
    fun payloadFor(mtu: Int): Int = (mtu - 3).coerceIn(20, 512)

    /** Splits one LINK_TX item into chunks of at most [payload] bytes (empty input -> no chunks). */
    fun split(data: ByteArray, payload: Int): List<ByteArray> {
        require(payload >= 1) { "payload must be >= 1" }
        if (data.isEmpty()) return emptyList()
        val out = ArrayList<ByteArray>((data.size + payload - 1) / payload)
        var off = 0
        while (off < data.size) {
            val end = minOf(data.size, off + payload)
            out.add(data.copyOfRange(off, end))
            off = end
        }
        return out
    }
}

/**
 * Merges byte chunks from several BLE peers into the single byte stream the core link sees,
 * without interleaving one sender's item with another's.
 *
 * Senders split each item into full-size chunks followed by one short chunk. A peer that delivers
 * a full-size chunk "holds the floor" until it delivers a short chunk or stays idle for [idleMs];
 * meanwhile other peers' chunks are buffered (at most [capBytes], oldest dropped first) and are
 * delivered, in arrival order, when the floor frees. Thread-safe; call [tick] periodically while
 * [needsTick] so a silent floor holder is timed out.
 */
class StreamArbiter(
    private val idleMs: Long,
    private val capBytes: Int,
    private val clock: () -> Long,
    private val out: (ByteArray) -> Unit,
) {
    private class Pending(val peer: String, val data: ByteArray, val full: Boolean)

    private val queue = ArrayDeque<Pending>()
    private var queuedBytes = 0
    private var owner: String? = null
    private var ownerLast = 0L
    private val maxSeen = HashMap<String, Int>()

    @Volatile var droppedBytes = 0L
        private set

    val floorHolder: String? @Synchronized get() = owner
    val queuedSize: Int @Synchronized get() = queuedBytes
    val needsTick: Boolean @Synchronized get() = owner != null || queue.isNotEmpty()

    /**
     * Chunk [data] arrived from [peer]; [maxChunkHint] is the largest chunk that peer can send
     * (negotiated MTU-3). A chunk counts as full when it reaches the hint or the largest chunk this
     * peer has sent so far (senders may use a smaller chunk size than the negotiated maximum).
     * Returns false if the chunk had to be dropped.
     */
    @Synchronized
    fun offer(peer: String, data: ByteArray, maxChunkHint: Int): Boolean {
        if (data.isEmpty()) return true
        val seen = maxSeen[peer] ?: 0
        val full = data.size >= maxChunkHint || (seen >= 20 && data.size >= seen)
        if (data.size > seen) maxSeen[peer] = data.size
        expire()
        if (owner == null) drain()
        if (owner == null || owner == peer) {
            emit(peer, data, full)
            drain()
            return true
        }
        if (data.size > capBytes) {
            droppedBytes += data.size
            return false
        }
        while (queuedBytes + data.size > capBytes && queue.isNotEmpty()) {
            val d = queue.removeFirst()
            queuedBytes -= d.data.size
            droppedBytes += d.data.size
        }
        queue.addLast(Pending(peer, data, full))
        queuedBytes += data.size
        return true
    }

    @Synchronized
    fun tick() {
        expire()
        drain()
    }

    /** The peer disconnected: give up its floor (its buffered chunks are still delivered). */
    @Synchronized
    fun release(peer: String) {
        if (owner == peer) owner = null
        maxSeen.remove(peer)
        drain()
    }

    private fun expire() {
        if (owner != null && clock() - ownerLast >= idleMs) owner = null
    }

    private fun emit(peer: String, data: ByteArray, full: Boolean) {
        out(data)
        if (full) {
            owner = peer
            ownerLast = clock()
        } else if (owner == peer) {
            owner = null
        }
    }

    private fun drain() {
        while (queue.isNotEmpty()) {
            val o = owner
            val it = queue.iterator()
            var picked: Pending? = null
            while (it.hasNext()) {
                val p = it.next()
                if (o == null || p.peer == o) { picked = p; it.remove(); break }
            }
            if (picked == null) return
            queuedBytes -= picked.data.size
            emit(picked.peer, picked.data, picked.full)
        }
    }
}

/** Avoids two phones each opening a central connection to the other. */
object TieBreak {
    /** 4-byte big-endian token from the advert's manufacturer data, or null. */
    fun parseToken(b: ByteArray?): Long? {
        if (b == null || b.size != 4) return null
        return ((b[0].toLong() and 0xff) shl 24) or ((b[1].toLong() and 0xff) shl 16) or
            ((b[2].toLong() and 0xff) shl 8) or (b[3].toLong() and 0xff)
    }

    fun tokenBytes(token: Long): ByteArray =
        byteArrayOf((token shr 24).toByte(), (token shr 16).toByte(), (token shr 8).toByte(), token.toByte())

    /**
     * Whether we should connect to a peer as central. The higher token waits for the lower one to
     * connect. No visible token (iOS, other implementations) or we are not advertising (they cannot
     * find us) -> connect anyway; a duplicate path is harmless (the mesh deduplicates frames).
     */
    fun shouldInitiate(ourToken: Long, theirToken: Long?, weAreAdvertising: Boolean): Boolean {
        if (!weAreAdvertising || theirToken == null) return true
        return ourToken <= theirToken
    }
}

/**
 * Android silently stops delivering scan results to an app that starts more than 5 scans in 30 s.
 * Every scan start in the process goes through this budget (keeps one start in reserve).
 */
class ScanBudget(private val maxStarts: Int = 4, private val windowMs: Long = 30_000) {
    private val starts = ArrayDeque<Long>()

    @Synchronized
    fun tryAcquire(now: Long): Boolean {
        prune(now)
        if (starts.size >= maxStarts) return false
        starts.addLast(now)
        return true
    }

    /** Earliest time a start will be allowed. */
    @Synchronized
    fun nextAllowedAt(now: Long): Long {
        prune(now)
        return if (starts.size < maxStarts) now else starts.first() + windowMs
    }

    private fun prune(now: Long) {
        while (starts.isNotEmpty() && now - starts.first() >= windowMs) starts.removeFirst()
    }

    companion object {
        val global = ScanBudget()
    }
}

/** Per-peer reconnect backoff: 2 s, 4 s ... 60 s. */
fun bleBackoffMs(failures: Int): Long = (2_000L shl failures.coerceIn(0, 5)).coerceAtMost(60_000L)
