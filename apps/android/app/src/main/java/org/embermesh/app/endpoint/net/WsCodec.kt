package org.embermesh.app.endpoint.net

import java.io.ByteArrayOutputStream
import java.io.EOFException
import java.io.IOException
import java.io.InputStream
import java.security.MessageDigest

/** Minimal RFC 6455 client-side codec (pure; unit-tested). */
object WsCodec {
    const val OP_CONT = 0x0
    const val OP_TEXT = 0x1
    const val OP_BINARY = 0x2
    const val OP_CLOSE = 0x8
    const val OP_PING = 0x9
    const val OP_PONG = 0xA
    private const val GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

    class Frame(val fin: Boolean, val opcode: Int, val payload: ByteArray)

    class ProtocolException(msg: String) : IOException(msg)

    fun acceptKey(key: String): String =
        Base64Std.encode(MessageDigest.getInstance("SHA-1").digest((key + GUID).toByteArray(Charsets.US_ASCII)))

    /** Encodes one frame. Clients must mask ([mask] = 4 bytes); servers pass null. */
    fun encode(opcode: Int, payload: ByteArray, mask: ByteArray?, fin: Boolean = true): ByteArray {
        val out = ByteArrayOutputStream(payload.size + 14)
        out.write((if (fin) 0x80 else 0) or (opcode and 0x0F))
        val m = if (mask != null) 0x80 else 0
        val n = payload.size
        when {
            n < 126 -> out.write(m or n)
            n <= 0xFFFF -> { out.write(m or 126); out.write(n ushr 8); out.write(n and 0xFF) }
            else -> {
                out.write(m or 127)
                val l = n.toLong()
                for (i in 7 downTo 0) out.write(((l ushr (8 * i)) and 0xFF).toInt())
            }
        }
        if (mask != null) {
            require(mask.size == 4)
            out.write(mask)
            val masked = ByteArray(n) { (payload[it].toInt() xor mask[it and 3].toInt()).toByte() }
            out.write(masked)
        } else out.write(payload)
        return out.toByteArray()
    }

    /** Reads one frame; throws on protocol violations and payloads larger than [maxPayload]. */
    fun readFrame(input: InputStream, maxPayload: Int): Frame {
        val b0 = input.read()
        if (b0 < 0) throw EOFException()
        val b1 = readByte(input)
        if (b0 and 0x70 != 0) throw ProtocolException("reserved bits set")
        val opcode = b0 and 0x0F
        val fin = b0 and 0x80 != 0
        if (opcode in 3..7 || opcode > 0xA) throw ProtocolException("bad opcode $opcode")
        var len = (b1 and 0x7F).toLong()
        if (len == 126L) len = ((readByte(input) shl 8) or readByte(input)).toLong()
        else if (len == 127L) {
            len = 0
            for (i in 0 until 8) len = (len shl 8) or readByte(input).toLong()
            if (len < 0) throw ProtocolException("bad length")
        }
        val isControl = opcode >= 8
        if (isControl && (len > 125 || !fin)) throw ProtocolException("bad control frame")
        if (len > maxPayload) throw ProtocolException("frame too large ($len)")
        val mask = if (b1 and 0x80 != 0) ByteArray(4).also { readFully(input, it) } else null
        val payload = ByteArray(len.toInt())
        readFully(input, payload)
        if (mask != null) for (i in payload.indices) payload[i] = (payload[i].toInt() xor mask[i and 3].toInt()).toByte()
        return Frame(fin, opcode, payload)
    }

    /** Reassembles fragmented data messages; control frames pass straight through. */
    class Assembler(private val maxMessage: Int) {
        private var buf: ByteArrayOutputStream? = null
        private var op = 0

        /** Returns (opcode, payload) for a complete message or control frame, else null. */
        fun push(f: Frame): Pair<Int, ByteArray>? {
            if (f.opcode >= 8) return f.opcode to f.payload
            if (f.opcode != OP_CONT) {
                if (buf != null) throw ProtocolException("new message inside fragmented message")
                if (f.fin) return f.opcode to f.payload
                buf = ByteArrayOutputStream().also { it.write(f.payload) }
                op = f.opcode
                return null
            }
            val b = buf ?: throw ProtocolException("continuation without start")
            if (b.size() + f.payload.size > maxMessage) throw ProtocolException("message too large")
            b.write(f.payload)
            if (!f.fin) return null
            buf = null
            return op to b.toByteArray()
        }
    }

    fun closePayload(code: Int, reason: String = ""): ByteArray {
        val rb = reason.toByteArray(Charsets.UTF_8)
        val r = rb.copyOf(minOf(rb.size, 123))
        return byteArrayOf((code ushr 8).toByte(), code.toByte()) + r
    }

    fun closeCode(payload: ByteArray): Int = if (payload.size >= 2) ((payload[0].toInt() and 0xFF) shl 8) or (payload[1].toInt() and 0xFF) else 1005

    private fun readByte(i: InputStream): Int { val b = i.read(); if (b < 0) throw EOFException(); return b }

    fun readFully(i: InputStream, b: ByteArray) {
        var o = 0
        while (o < b.size) { val n = i.read(b, o, b.size - o); if (n < 0) throw EOFException(); o += n }
    }

    /** Parses ws[s]://host[:port][/path] → (secure, host, port, pathAndQuery). */
    fun parseUrl(url: String): Url {
        val u = url.trim()
        val secure = when {
            u.startsWith("wss://", true) -> true
            u.startsWith("ws://", true) -> false
            else -> throw IllegalArgumentException("URL must start with ws:// or wss://")
        }
        val rest = u.substring(if (secure) 6 else 5)
        val slash = rest.indexOf('/').let { if (it < 0) rest.length else it }
        val hostPort = rest.substring(0, slash)
        val path = rest.substring(slash).ifEmpty { "/" }
        if (hostPort.isEmpty()) throw IllegalArgumentException("missing host")
        val (host, port) = if (hostPort.startsWith("[")) {
            val e = hostPort.indexOf(']')
            if (e < 0) throw IllegalArgumentException("bad IPv6 host")
            val h = hostPort.substring(1, e)
            val p = hostPort.substring(e + 1).removePrefix(":").toIntOrNull()
            h to (p ?: if (secure) 443 else 80)
        } else {
            val c = hostPort.lastIndexOf(':')
            if (c < 0) hostPort to (if (secure) 443 else 80)
            else hostPort.substring(0, c) to (hostPort.substring(c + 1).toIntOrNull() ?: throw IllegalArgumentException("bad port"))
        }
        if (port !in 1..65535) throw IllegalArgumentException("bad port")
        return Url(secure, host, port, path)
    }

    data class Url(val secure: Boolean, val host: String, val port: Int, val path: String) {
        val hostHeader: String get() {
            val h = if (host.contains(':')) "[$host]" else host
            return if ((secure && port == 443) || (!secure && port == 80)) h else "$h:$port"
        }
    }
}

/** Standard base64 (android.util.Base64 is unavailable in JVM unit tests; java.util.Base64 needs API 26). */
object Base64Std {
    private const val A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    fun encode(b: ByteArray): String {
        val sb = StringBuilder((b.size + 2) / 3 * 4)
        var i = 0
        while (i + 2 < b.size) {
            val n = ((b[i].toInt() and 0xFF) shl 16) or ((b[i + 1].toInt() and 0xFF) shl 8) or (b[i + 2].toInt() and 0xFF)
            sb.append(A[n ushr 18]).append(A[(n ushr 12) and 63]).append(A[(n ushr 6) and 63]).append(A[n and 63])
            i += 3
        }
        val rem = b.size - i
        if (rem == 1) {
            val n = (b[i].toInt() and 0xFF) shl 16
            sb.append(A[n ushr 18]).append(A[(n ushr 12) and 63]).append("==")
        } else if (rem == 2) {
            val n = ((b[i].toInt() and 0xFF) shl 16) or ((b[i + 1].toInt() and 0xFF) shl 8)
            sb.append(A[n ushr 18]).append(A[(n ushr 12) and 63]).append(A[(n ushr 6) and 63]).append('=')
        }
        return sb.toString()
    }
}
