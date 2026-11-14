package org.embermesh.app.endpoint.net

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.EOFException

class WsCodecTest {
    private val mask = byteArrayOf(0x37, 0xFA.toByte(), 0x21, 0x3D)

    @Test fun acceptKeyRfcExample() {
        assertEquals("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", WsCodec.acceptKey("dGhlIHNhbXBsZSBub25jZQ=="))
    }

    @Test fun maskedHelloMatchesRfc() {
        // RFC 6455 §5.7: masked "Hello" = 0x81 0x85 0x37 0xfa 0x21 0x3d 0x7f 0x9f 0x4d 0x51 0x58
        val f = WsCodec.encode(WsCodec.OP_TEXT, "Hello".toByteArray(), mask)
        assertArrayEquals(byteArrayOf(0x81.toByte(), 0x85.toByte(), 0x37, 0xfa.toByte(), 0x21, 0x3d, 0x7f, 0x9f.toByte(), 0x4d, 0x51, 0x58), f)
        val back = WsCodec.readFrame(ByteArrayInputStream(f), 1000)
        assertEquals("Hello", String(back.payload)); assertTrue(back.fin)
    }

    @Test fun lengthEncodings() {
        for (n in listOf(0, 1, 125, 126, 65535, 65536, 70000)) {
            val p = ByteArray(n) { (it * 7).toByte() }
            val enc = WsCodec.encode(WsCodec.OP_BINARY, p, null)
            val hdr = when { n < 126 -> 2; n <= 65535 -> 4; else -> 10 }
            assertEquals(hdr + n, enc.size)
            assertArrayEquals(p, WsCodec.readFrame(ByteArrayInputStream(enc), 1 shl 20).payload)
            val m = WsCodec.encode(WsCodec.OP_BINARY, p, mask)
            assertArrayEquals(p, WsCodec.readFrame(ByteArrayInputStream(m), 1 shl 20).payload)
        }
    }

    @Test fun oversizeRejected() {
        val enc = WsCodec.encode(WsCodec.OP_BINARY, ByteArray(2000), null)
        try { WsCodec.readFrame(ByteArrayInputStream(enc), 1000); fail() } catch (e: WsCodec.ProtocolException) {}
    }

    @Test fun badFramesRejected() {
        for (bad in listOf(byteArrayOf(0xC2.toByte(), 0), byteArrayOf(0x83.toByte(), 0), byteArrayOf(0x09, 0), byteArrayOf(0x89.toByte(), 126, 0, 126))) {
            try { WsCodec.readFrame(ByteArrayInputStream(bad), 1 shl 20); fail("accepted ${bad.toList()}") } catch (e: WsCodec.ProtocolException) {}
        }
        try { WsCodec.readFrame(ByteArrayInputStream(byteArrayOf(0x82.toByte(), 5, 1, 2)), 100); fail() } catch (e: EOFException) {}
    }

    @Test fun fragmentationAndControlInterleave() {
        val asm = WsCodec.Assembler(100)
        assertNull(asm.push(WsCodec.Frame(false, WsCodec.OP_BINARY, byteArrayOf(1, 2))))
        val ping = asm.push(WsCodec.Frame(true, WsCodec.OP_PING, byteArrayOf(9)))!!
        assertEquals(WsCodec.OP_PING, ping.first)
        assertNull(asm.push(WsCodec.Frame(false, WsCodec.OP_CONT, byteArrayOf(3))))
        val msg = asm.push(WsCodec.Frame(true, WsCodec.OP_CONT, byteArrayOf(4)))!!
        assertEquals(WsCodec.OP_BINARY, msg.first); assertArrayEquals(byteArrayOf(1, 2, 3, 4), msg.second)
        try { asm.push(WsCodec.Frame(true, WsCodec.OP_CONT, byteArrayOf(1))); fail() } catch (e: WsCodec.ProtocolException) {}
        val small = WsCodec.Assembler(3)
        small.push(WsCodec.Frame(false, WsCodec.OP_BINARY, byteArrayOf(1, 2)))
        try { small.push(WsCodec.Frame(true, WsCodec.OP_CONT, byteArrayOf(3, 4))); fail() } catch (e: WsCodec.ProtocolException) {}
    }

    @Test fun closePayload() {
        val p = WsCodec.closePayload(1000, "bye")
        assertEquals(1000, WsCodec.closeCode(p)); assertEquals(1005, WsCodec.closeCode(ByteArray(0)))
        assertEquals(125, WsCodec.closePayload(1001, "é".repeat(100)).size)
    }

    @Test fun urlParsing() {
        assertEquals(WsCodec.Url(false, "192.168.43.1", 8437, "/ws"), WsCodec.parseUrl("ws://192.168.43.1:8437/ws"))
        assertEquals(WsCodec.Url(true, "hub.local", 443, "/"), WsCodec.parseUrl("wss://hub.local"))
        assertEquals(WsCodec.Url(false, "fe80::1", 80, "/ws?x=1"), WsCodec.parseUrl("ws://[fe80::1]/ws?x=1"))
        assertEquals("[fe80::1]:81", WsCodec.parseUrl("ws://[fe80::1]:81/").hostHeader)
        assertEquals("h", WsCodec.parseUrl("ws://h:80/").hostHeader)
        for (bad in listOf("http://x", "ws://", "ws://h:0/", "ws://h:abc/")) {
            try { WsCodec.parseUrl(bad); fail(bad) } catch (e: IllegalArgumentException) {}
        }
    }

    @Test fun base64() {
        assertEquals("", Base64Std.encode(ByteArray(0)))
        assertEquals("Zg==", Base64Std.encode("f".toByteArray()))
        assertEquals("Zm8=", Base64Std.encode("fo".toByteArray()))
        assertEquals("Zm9vYmFy", Base64Std.encode("foobar".toByteArray()))
    }
}

class UdpLogicTest {
    @Test fun peers() {
        assertEquals(listOf("10.0.0.2" to 47474, "host" to 5000), UdpEndpoint.parsePeers("10.0.0.2, host:5000", 47474))
        assertEquals(emptyList<Pair<String, Int>>(), UdpEndpoint.parsePeers("", 1))
        assertEquals(emptyList<Pair<String, Int>>(), UdpEndpoint.parsePeers("h:0 h:70000 :5 [::1]:5", 1))
    }

    @Test fun wifiLikeNames() {
        assertTrue(UdpEndpoint.isWifiLike("wlan0")); assertTrue(UdpEndpoint.isWifiLike("ap0")); assertTrue(UdpEndpoint.isWifiLike("swlan0"))
        assertTrue(!UdpEndpoint.isWifiLike("rmnet_data0"))
    }
}
