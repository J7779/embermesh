package org.embermesh.app.endpoint.serial

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Test

class SerialProtocolsTest {
    private fun b(vararg v: Int) = ByteArray(v.size) { v[it].toByte() }

    @Test fun cdcLineCoding9600_8N1() {
        assertArrayEquals(b(0x80, 0x25, 0, 0, 0, 0, 8), CdcAcmProtocol.lineCoding(LineParams(9600)))
    }

    @Test fun cdcLineCoding115200_7E2() {
        assertArrayEquals(b(0x00, 0xC2, 0x01, 0, 2, 2, 7),
            CdcAcmProtocol.lineCoding(LineParams(115200, 7, Parity.EVEN, StopBits.TWO)))
    }

    @Test fun cdcControlLines() {
        assertEquals(Ctrl(0x21, 0x22, 3, 1), CdcAcmProtocol.setControlLines(1, dtr = true, rts = true))
        assertEquals(0, CdcAcmProtocol.controlLineState(false, false))
        assertEquals(2, CdcAcmProtocol.controlLineState(false, true))
    }

    @Test fun cp210xLineCtlAndMhs() {
        assertEquals(0x0800, Cp210xProtocol.lineCtl(LineParams(9600)))
        assertEquals(0x0722, Cp210xProtocol.lineCtl(LineParams(9600, 7, Parity.EVEN, StopBits.TWO)))
        assertEquals(0x0301, Cp210xProtocol.mhs(dtr = true, rts = false))
        assertEquals(0x0303, Cp210xProtocol.mhs(dtr = true, rts = true))
        assertEquals(Ctrl(0x41, 0x1E, 0, 0, b(0x00, 0xC2, 0x01, 0x00)), Cp210xProtocol.baud(0, 115200))
    }

    @Test fun cp210xFlowStruct() {
        val rtscts = Cp210xProtocol.flowStruct(Flow.RTS_CTS, dtr = true, rts = false)
        assertEquals(16, rtscts.size)
        assertEquals(0x09, rtscts[0].toInt())       // DTR active | CTS handshake
        assertEquals(0x80, rtscts[4].toInt() and 0xFF) // RTS flow control
        val none = Cp210xProtocol.flowStruct(Flow.NONE, dtr = false, rts = true)
        assertEquals(0x00, none[0].toInt()); assertEquals(0x40, none[4].toInt())
    }

    @Test fun ch34xDivisors() {
        assertEquals(0xCC03, Ch34xProtocol.divisor(115200))
        assertEquals(0xB202, Ch34xProtocol.divisor(9600))
        val c = Ch34xProtocol.setBaud(115200, 0x30)
        assertEquals(0x1312, c.value); assertEquals(0xCC83, c.index) // bit 7 set for version > 0x27
        assertEquals(0xCC03, Ch34xProtocol.setBaud(115200, 0x27).index)
    }

    @Test fun ch34xLcrAndModem() {
        assertEquals(0xC3, Ch34xProtocol.lcr(LineParams(9600)))
        assertEquals(0xC3 or 0x08 or 0x10, Ch34xProtocol.lcr(LineParams(9600, parity = Parity.EVEN)))
        assertEquals(0xC2 or 0x04, Ch34xProtocol.lcr(LineParams(9600, 7, stopBits = StopBits.TWO)))
        assertEquals(0xFF9F, Ch34xProtocol.modemCtrlValue(dtr = true, rts = true))
        assertEquals(0xFFFF, Ch34xProtocol.modemCtrlValue(dtr = false, rts = false))
    }

    @Test fun ftdiDivisorsBm() {
        assertEquals(0x4138, FtdiProtocol.divisor232bm(9600))
        assertEquals(0x001A, FtdiProtocol.divisor232bm(115200))
        assertEquals(0x0000, FtdiProtocol.divisor232bm(3_000_000))
        assertEquals(0x0001, FtdiProtocol.divisor232bm(2_000_000))
        assertEquals(0x2710, FtdiProtocol.divisor232bm(300))
    }

    @Test fun ftdiDivisorsHType() {
        // 12 MHz base: 115200 -> 104.17 -> 104 + 1/8 ≈ ; bit 17 set.
        val d = FtdiProtocol.divisor2232h(115200)
        assertEquals(0x20000, d and 0x20000)
        assertEquals(104, d and 0x3FFF)
        val c = FtdiProtocol.setBaud(FtdiProtocol.Chip.FT2232H, 115200, 1)
        assertEquals(d and 0xFFFF, c.value)
        assertEquals(((d ushr 16) shl 8) or 1, c.index)
        // Below 1200 baud H chips use the 3 MHz path.
        assertEquals(FtdiProtocol.divisor232bm(300), FtdiProtocol.divisor(FtdiProtocol.Chip.FT232H, 300))
    }

    @Test fun ftdiSingleInterfaceIndexAndData() {
        val c = FtdiProtocol.setBaud(FtdiProtocol.Chip.FT232R, 9600, 1)
        assertEquals(0x4138, c.value); assertEquals(0, c.index)
        assertEquals(0x0008, FtdiProtocol.dataValue(LineParams(9600)))
        assertEquals(0x1107, FtdiProtocol.dataValue(LineParams(9600, 7, Parity.ODD, StopBits.TWO)))
        assertEquals(0x0303, FtdiProtocol.modemValue(true, true))
        assertEquals(0x0301, FtdiProtocol.modemValue(true, false))
        assertEquals(Ctrl(0x40, 2, 0x1311, 0x0401), FtdiProtocol.setFlow(Flow.XON_XOFF, 1))
    }

    @Test fun ftdiChipDetect() {
        assertEquals(FtdiProtocol.Chip.FT232R, FtdiProtocol.chipFromBcd(0x0600, 1))
        assertEquals(FtdiProtocol.Chip.FT232H, FtdiProtocol.chipFromBcd(0x0900, 1))
        assertEquals(FtdiProtocol.Chip.FTX, FtdiProtocol.chipFromBcd(0x1000, 1))
    }

    @Test fun ftdiStripStatusAcrossPackets() {
        val src = ByteArray(64 + 10)
        src[0] = 0x31; src[1] = 0x60
        for (i in 2 until 64) src[i] = i.toByte()
        src[64] = 0x31; src[65] = 0x60
        for (i in 66 until 74) src[i] = (100 + i).toByte()
        val dst = ByteArray(128)
        val n = FtdiProtocol.stripStatus(src, src.size, 64, dst)
        assertEquals(62 + 8, n)
        assertEquals(2.toByte(), dst[0]); assertEquals(63.toByte(), dst[61]); assertEquals(166.toByte(), dst[62])
        assertEquals(0, FtdiProtocol.stripStatus(byteArrayOf(1, 2), 2, 64, dst)) // status-only packet
    }

    @Test fun pl2303StandardAndDivisorBaud() {
        assertArrayEquals(b(0x80, 0x25, 0, 0), Pl2303Protocol.encodeBaud(9600, Pl2303Protocol.Type.HX))
        // 250000 is not standard: mantissa = 384000000/250000 = 1536 -> >>2 = 384, exp 1.
        assertArrayEquals(b(0x80, 0x03, 0x00, 0x80), Pl2303Protocol.encodeBaud(250000, Pl2303Protocol.Type.HX))
        // Type H snaps to the nearest standard rate.
        assertArrayEquals(b(0x00, 0xC2, 0x01, 0), Pl2303Protocol.encodeBaud(110000, Pl2303Protocol.Type.H))
    }

    @Test fun pl2303DetectionAndInit() {
        assertEquals(Pl2303Protocol.Type.HXN, Pl2303Protocol.detect(0x23A3, 0, 0x40, 0x100, 0x200))
        assertEquals(Pl2303Protocol.Type.H, Pl2303Protocol.detect(0x2303, 0x02, 0x40, 0x300, 0x110))
        assertEquals(Pl2303Protocol.Type.TA, Pl2303Protocol.detect(0x2303, 0, 0x40, 0x300, 0x200))
        assertEquals(Pl2303Protocol.Type.HX, Pl2303Protocol.detect(0x2303, 0, 0x40, 0x400, 0x110))
        assertEquals(11, Pl2303Protocol.initSequence(Pl2303Protocol.Type.HX).size)
        assertEquals(0x44, Pl2303Protocol.initSequence(Pl2303Protocol.Type.HX).last().third)
        assertEquals(0x24, Pl2303Protocol.initSequence(Pl2303Protocol.Type.H).last().third)
        assertEquals(0, Pl2303Protocol.initSequence(Pl2303Protocol.Type.HXN).size)
        assertEquals(Ctrl(0x40, 0x80, 0x0A, 0x18), Pl2303Protocol.setFlow(Pl2303Protocol.Type.HXN, Flow.RTS_CTS))
    }

    @Test fun proberFamilies() {
        assertEquals(UsbSerialIds.Family.FTDI, UsbSerialIds.familyFor(0x0403, 0x6001, false))
        assertEquals(UsbSerialIds.Family.FTDI, UsbSerialIds.familyFor(0x0403, 0x1234, false))
        assertEquals(UsbSerialIds.Family.CP210X, UsbSerialIds.familyFor(0x10C4, 0xEA60, false))
        assertEquals(UsbSerialIds.Family.CH34X, UsbSerialIds.familyFor(0x1A86, 0x7523, false))
        assertEquals(UsbSerialIds.Family.PL2303, UsbSerialIds.familyFor(0x067B, 0x23C3, false))
        assertEquals(UsbSerialIds.Family.CDC, UsbSerialIds.familyFor(0x303A, 0x1001, true))
        assertEquals(null, UsbSerialIds.familyFor(0x1234, 0x5678, false))
    }

    @Test fun paramParsing() {
        assertEquals(StopBits.ONE_HALF, StopBits.parse("1.5"))
        assertEquals(Parity.MARK, Parity.parse("mark"))
        assertEquals(Flow.RTS_CTS, Flow.parse("rts_cts"))
        assertEquals(Parity.NONE, Parity.parse("garbage"))
    }

    @Test(expected = IllegalArgumentException::class) fun badDataBits() { LineParams(9600, 9) }
}

class CdcLayoutTest {
    private fun i(id: Int, cls: Int, sub: Int, bulk: Boolean = false) = CdcLayout.Itf(id, cls, sub, bulk)

    @Test fun aiocCompositeAudioPlusCdc() {
        // AIOC-like: audio control, 2 audio streaming, HID, CDC comm, CDC data.
        val l = listOf(i(0, 1, 1), i(1, 1, 2), i(2, 1, 2), i(3, 3, 0), i(4, 2, 2), i(5, 10, 0, true))
        org.junit.Assert.assertEquals(4 to 5, CdcLayout.pick(l))
    }

    @Test fun nativeUsbBoardWithoutComm() {
        val l = listOf(i(0, 1, 1), i(1, 255, 0, true))
        org.junit.Assert.assertEquals(null to 1, CdcLayout.pick(l))
        org.junit.Assert.assertEquals(null to null, CdcLayout.pick(listOf(i(0, 1, 2, true)))) // audio streaming is never taken
    }
}
