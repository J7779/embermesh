package org.embermesh.app.endpoint.ble

import java.util.UUID

/**
 * Meshtastic's BLE API adapted to the byte stream the core's `meshtastic` link expects with its
 * default `framing:"stream"` (the same adaptation as the iOS app):
 *   device -> core: each FromRadio read (one protobuf) becomes `94 C3 len_hi len_lo <protobuf>`;
 *   core -> device: the stream is re-framed and each protobuf is written to ToRadio on its own;
 *   bytes outside frames (the driver's 32 x 0xC3 serial wake-up) are dropped — BLE needs no wake-up.
 */
object MeshtasticBle {
    val SERVICE: UUID = UUID.fromString("6ba1b218-15a8-461f-9fa8-5dcae273eafd")
    val TO_RADIO: UUID = UUID.fromString("f75c76d2-129e-4dad-a1dd-7866124401e7")
    val FROM_RADIO: UUID = UUID.fromString("2c55e69e-4993-11ed-b878-0242ac120002")
    val FROM_NUM: UUID = UUID.fromString("ed9da18c-a800-4f66-a670-aa7547e34453")
    const val MAX_PROTOBUF = 512

    fun wrap(protobuf: ByteArray): ByteArray {
        val n = protobuf.size.coerceAtMost(0xFFFF)
        return byteArrayOf(0x94.toByte(), 0xC3.toByte(), (n shr 8).toByte(), n.toByte()) + protobuf.copyOf(n)
    }

    /** Re-frames the core's outgoing stream into protobufs (stateful; not thread-safe). */
    class Unframer {
        private var pending = ByteArray(0)

        fun feed(data: ByteArray): List<ByteArray> {
            pending += data
            val out = ArrayList<ByteArray>()
            var i = 0
            while (true) {
                // find 94 C3
                while (i + 1 < pending.size && !(pending[i] == 0x94.toByte() && pending[i + 1] == 0xC3.toByte())) i++
                if (i + 4 > pending.size) break
                val len = ((pending[i + 2].toInt() and 0xff) shl 8) or (pending[i + 3].toInt() and 0xff)
                if (len == 0 || len > MAX_PROTOBUF) { i += 1; continue } // not a frame: resync
                if (i + 4 + len > pending.size) break
                out.add(pending.copyOfRange(i + 4, i + 4 + len))
                i += 4 + len
            }
            pending = if (i >= pending.size) ByteArray(0) else pending.copyOfRange(i, pending.size)
            if (pending.size > 4 + MAX_PROTOBUF) pending = pending.copyOfRange(pending.size - 4 - MAX_PROTOBUF, pending.size)
            return out
        }
    }
}
