package org.embermesh.app.endpoint.serial

/*
 * Pure encoders for the USB serial chips we drive ourselves (no third-party library). Values follow
 * the vendor datasheets / application notes and the Linux kernel drivers (cdc-acm, cp210x, ch341,
 * ftdi_sio, pl2303), re-implemented from the protocol descriptions.
 */

enum class Parity(val code: Int) { NONE(0), ODD(1), EVEN(2), MARK(3), SPACE(4);
    companion object { fun parse(s: String) = values().firstOrNull { it.name.equals(s, true) } ?: NONE }
}

enum class StopBits(val code: Int) { ONE(0), ONE_HALF(1), TWO(2);
    companion object {
        fun parse(s: String) = when (s.trim()) { "1.5" -> ONE_HALF; "2" -> TWO; else -> ONE }
    }
}

enum class Flow { NONE, RTS_CTS, DTR_DSR, XON_XOFF;
    companion object { fun parse(s: String) = values().firstOrNull { it.name.equals(s, true) } ?: NONE }
}

data class LineParams(
    val baud: Int = 9600,
    val dataBits: Int = 8,
    val parity: Parity = Parity.NONE,
    val stopBits: StopBits = StopBits.ONE,
    val flow: Flow = Flow.NONE,
) {
    init {
        require(baud in 50..12_000_000) { "baud rate out of range" }
        require(dataBits in 5..8) { "data bits must be 5..8" }
    }
}

/** A control transfer to issue: requestType, request, value, index, optional data. */
data class Ctrl(val type: Int, val request: Int, val value: Int, val index: Int, val data: ByteArray? = null) {
    override fun equals(other: Any?) = other is Ctrl && other.type == type && other.request == request &&
        other.value == value && other.index == index && (data?.contentEquals(other.data ?: ByteArray(0)) ?: (other.data == null))
    override fun hashCode() = (type * 31 + request) * 31 + value * 17 + index
    override fun toString() = "Ctrl(0x%02x,0x%02x,0x%04x,0x%04x,%s)".format(type, request, value, index, data?.joinToString(",") { "%02x".format(it) })
}

private fun le32(v: Long) = byteArrayOf(v.toByte(), (v shr 8).toByte(), (v shr 16).toByte(), (v shr 24).toByte())

/** USB CDC-ACM (PSTN subclass). */
object CdcAcmProtocol {
    const val REQTYPE_CLASS_OUT = 0x21
    const val SET_LINE_CODING = 0x20
    const val SET_CONTROL_LINE_STATE = 0x22
    const val SEND_BREAK = 0x23

    /** 7-byte line coding: dwDTERate LE32, bCharFormat (0=1,1=1.5,2=2), bParityType (0..4), bDataBits. */
    fun lineCoding(p: LineParams): ByteArray = le32(p.baud.toLong()) + byteArrayOf(p.stopBits.code.toByte(), p.parity.code.toByte(), p.dataBits.toByte())

    fun setLineCoding(iface: Int, p: LineParams) = Ctrl(REQTYPE_CLASS_OUT, SET_LINE_CODING, 0, iface, lineCoding(p))
    fun controlLineState(dtr: Boolean, rts: Boolean) = (if (dtr) 1 else 0) or (if (rts) 2 else 0)
    fun setControlLines(iface: Int, dtr: Boolean, rts: Boolean) = Ctrl(REQTYPE_CLASS_OUT, SET_CONTROL_LINE_STATE, controlLineState(dtr, rts), iface)
}

/** Silicon Labs CP210x (AN571). */
object Cp210xProtocol {
    const val REQTYPE_OUT = 0x41
    const val IFC_ENABLE = 0x00
    const val SET_LINE_CTL = 0x03
    const val SET_MHS = 0x07
    const val PURGE = 0x12
    const val SET_FLOW = 0x13
    const val SET_BAUDRATE = 0x1E
    const val UART_ENABLE = 0x0001
    const val UART_DISABLE = 0x0000

    /** bits 0-3 stop (0=1, 1=1.5, 2=2), bits 4-7 parity (0 none,1 odd,2 even,3 mark,4 space), bits 8-15 data bits. */
    fun lineCtl(p: LineParams): Int = p.stopBits.code or (p.parity.code shl 4) or (p.dataBits shl 8)

    /** SET_MHS: DTR bit0, RTS bit1, with write-mask bits 8/9 so both lines are always set. */
    fun mhs(dtr: Boolean, rts: Boolean): Int = (if (dtr) 0x0001 else 0) or (if (rts) 0x0002 else 0) or 0x0100 or 0x0200

    fun enable(iface: Int) = Ctrl(REQTYPE_OUT, IFC_ENABLE, UART_ENABLE, iface)
    fun disable(iface: Int) = Ctrl(REQTYPE_OUT, IFC_ENABLE, UART_DISABLE, iface)
    fun baud(iface: Int, baud: Int) = Ctrl(REQTYPE_OUT, SET_BAUDRATE, 0, iface, le32(baud.toLong()))
    fun line(iface: Int, p: LineParams) = Ctrl(REQTYPE_OUT, SET_LINE_CTL, lineCtl(p), iface)
    fun lines(iface: Int, dtr: Boolean, rts: Boolean) = Ctrl(REQTYPE_OUT, SET_MHS, mhs(dtr, rts), iface)
    fun purge(iface: Int) = Ctrl(REQTYPE_OUT, PURGE, 0x000F, iface)

    /**
     * SET_FLOW 16-byte structure: ulControlHandshake, ulFlowReplace, ulXonLimit, ulXoffLimit (LE32).
     * DTR is kept under manual control (SERIAL_DTR_ACTIVE) except for DTR/DSR flow.
     */
    fun flowStruct(flow: Flow, dtr: Boolean, rts: Boolean): ByteArray {
        var control = if (dtr) 0x01L else 0x00L           // SERIAL_DTR_ACTIVE
        var replace = if (rts) 0x40L else 0x00L           // SERIAL_RTS_ACTIVE (1 << 6)
        when (flow) {
            Flow.RTS_CTS -> { control = control or 0x08; replace = (replace and 0xC0L.inv()) or 0x80 } // CTS handshake, RTS flow control
            Flow.DTR_DSR -> { control = (control and 0x3L.inv()) or 0x02 or 0x10 }                     // DTR handshake, DSR handshake
            Flow.XON_XOFF -> { replace = replace or 0x01 or 0x02 }                                     // auto transmit / receive
            Flow.NONE -> {}
        }
        return le32(control) + le32(replace) + le32(128) + le32(128)
    }

    fun setFlow(iface: Int, flow: Flow, dtr: Boolean, rts: Boolean) = Ctrl(REQTYPE_OUT, SET_FLOW, 0, iface, flowStruct(flow, dtr, rts))
}

/** WCH CH340 / CH341. */
object Ch34xProtocol {
    const val REQTYPE_OUT = 0x40
    const val REQTYPE_IN = 0xC0
    const val REQ_READ_VERSION = 0x5F
    const val REQ_WRITE_REG = 0x9A
    const val REQ_READ_REG = 0x95
    const val REQ_SERIAL_INIT = 0xA1
    const val REQ_MODEM_CTRL = 0xA4
    const val REG_PRESCALER = 0x12
    const val REG_DIVISOR = 0x13
    const val REG_LCR = 0x18
    const val REG_LCR2 = 0x25
    const val BIT_DTR = 0x20
    const val BIT_RTS = 0x40
    private const val CLKRATE = 48_000_000L

    private fun clkDiv(ps: Int, fact: Int) = 1L shl (12 - 3 * ps - fact)
    private fun minRate(ps: Int) = CLKRATE / (clkDiv(ps, 1) * 512)
    private val MIN_BPS = minRate(0)
    private val MAX_BPS = CLKRATE / (clkDiv(3, 0) * 2)

    /** Returns the 16-bit prescaler/divisor register pair value: (0x100 - div) << 8 | fact << 2 | ps. */
    fun divisor(baud: Int): Int {
        val speed = baud.toLong().coerceIn(MIN_BPS, MAX_BPS)
        var fact = 1
        var ps = 3
        while (ps >= 0) { if (speed > minRate(ps)) break; ps-- }
        require(ps >= 0) { "unsupported baud rate" }
        var cd = clkDiv(ps, fact)
        var div = CLKRATE / (cd * speed)
        if (div < 9 || div > 255) { div /= 2; cd *= 2; fact = 0 }
        require(div >= 2) { "unsupported baud rate" }
        // Pick the next divisor if the resulting rate is closer to the requested one.
        if (16 * CLKRATE / (cd * div) - 16 * speed >= 16 * speed - 16 * CLKRATE / (cd * (div + 1))) div++
        // Prefer the lower base clock for even divisors (more receiver tolerance).
        if (fact == 1 && div % 2 == 0L) { div /= 2; fact = 0 }
        return (((0x100 - div) shl 8) or (fact.toLong() shl 2) or ps.toLong()).toInt()
    }

    fun lcr(p: LineParams): Int {
        var v = 0x80 or 0x40 // enable RX, TX
        v = v or when (p.dataBits) { 5 -> 0x00; 6 -> 0x01; 7 -> 0x02; else -> 0x03 }
        when (p.parity) {
            Parity.NONE -> {}
            Parity.ODD -> v = v or 0x08
            Parity.EVEN -> v = v or 0x08 or 0x10
            Parity.MARK -> v = v or 0x08 or 0x20
            Parity.SPACE -> v = v or 0x08 or 0x20 or 0x10
        }
        if (p.stopBits == StopBits.TWO) v = v or 0x04
        return v
    }

    /** Modem control lines are active-low on the wire: send the inverted bits. */
    fun modemCtrlValue(dtr: Boolean, rts: Boolean): Int = ((if (dtr) BIT_DTR else 0) or (if (rts) BIT_RTS else 0)).inv() and 0xFFFF

    fun setBaud(baud: Int, version: Int): Ctrl {
        var v = divisor(baud)
        if (version > 0x27) v = v or 0x80 // no buffering of the last bytes (CH341A quirk)
        return Ctrl(REQTYPE_OUT, REQ_WRITE_REG, (REG_DIVISOR shl 8) or REG_PRESCALER, v)
    }

    fun setLcr(p: LineParams) = Ctrl(REQTYPE_OUT, REQ_WRITE_REG, (REG_LCR2 shl 8) or REG_LCR, lcr(p))
    fun serialInit() = Ctrl(REQTYPE_OUT, REQ_SERIAL_INIT, 0, 0)
    fun modemCtrl(dtr: Boolean, rts: Boolean) = Ctrl(REQTYPE_OUT, REQ_MODEM_CTRL, modemCtrlValue(dtr, rts), 0)
}

/** FTDI FT232/FT2232/FT4232/FT232H/FT-X. */
object FtdiProtocol {
    const val REQTYPE_OUT = 0x40
    const val SIO_RESET = 0
    const val SIO_MODEM_CTRL = 1
    const val SIO_SET_FLOW_CTRL = 2
    const val SIO_SET_BAUD_RATE = 3
    const val SIO_SET_DATA = 4
    const val SIO_SET_LATENCY_TIMER = 9

    enum class Chip { AM, BM, FT2232C, FT232R, FT2232H, FT4232H, FT232H, FTX;
        val isHType get() = this == FT2232H || this == FT4232H || this == FT232H
        val multiInterface get() = this == FT2232C || this == FT2232H || this == FT4232H || this == FT232H
    }

    /** Chip type from bcdDevice (and interface count for the 0x500/0x700 families). */
    fun chipFromBcd(bcd: Int, interfaces: Int): Chip = when (bcd and 0xFF00) {
        0x0200 -> Chip.AM
        0x0400 -> Chip.BM
        0x0500 -> Chip.FT2232C
        0x0600 -> Chip.FT232R
        0x0700 -> Chip.FT2232H
        0x0800 -> Chip.FT4232H
        0x0900 -> Chip.FT232H
        0x1000 -> Chip.FTX
        else -> if (interfaces > 1) Chip.FT2232C else Chip.FT232R
    }

    private val DIVFRAC = intArrayOf(0, 3, 2, 4, 1, 5, 6, 7)

    /** FT232BM/R/X style: 3 MHz base clock, fractions in 1/8 steps. */
    fun divisor232bm(baud: Int, base: Int = 48_000_000): Int {
        val d3 = ((base.toLong() + baud) / (2L * baud)).toInt() // DIV_ROUND_CLOSEST(base, 2*baud)
        var div = (d3 shr 3) or (DIVFRAC[d3 and 7] shl 14)
        if (div == 1) div = 0 else if (div == 0x4001) div = 1
        return div
    }

    /** H-type chips: 120 MHz / 10 = 12 MHz base, bit 17 disables the /2.5 prescaler. */
    fun divisor2232h(baud: Int, base: Int = 120_000_000): Int {
        val num = 8L * base
        val den = 10L * baud
        val d3 = ((num + den / 2) / den).toInt()
        var div = (d3 shr 3) or (DIVFRAC[d3 and 7] shl 14)
        if (div == 1) div = 0 else if (div == 0x4001) div = 1
        return div or 0x00020000
    }

    fun divisor(chip: Chip, baud: Int): Int = if (chip.isHType && baud >= 1200) divisor2232h(baud) else divisor232bm(baud)

    /** wValue/wIndex for SIO_SET_BAUD_RATE; [port] is 1-based interface index for multi-interface chips. */
    fun setBaud(chip: Chip, baud: Int, port: Int): Ctrl {
        val div = divisor(chip, baud)
        val value = div and 0xFFFF
        var index = (div ushr 16) and 0xFFFF
        if (chip.multiInterface) index = (index shl 8) or port
        return Ctrl(REQTYPE_OUT, SIO_SET_BAUD_RATE, value, index)
    }

    fun dataValue(p: LineParams): Int = p.dataBits or (p.parity.code shl 8) or (p.stopBits.code shl 11)
    fun setData(p: LineParams, port: Int) = Ctrl(REQTYPE_OUT, SIO_SET_DATA, dataValue(p), port)

    /** DTR = bit0, RTS = bit1, with enable masks 0x0100/0x0200. */
    fun modemValue(dtr: Boolean, rts: Boolean): Int = 0x0300 or (if (dtr) 1 else 0) or (if (rts) 2 else 0)
    fun setModem(dtr: Boolean, rts: Boolean, port: Int) = Ctrl(REQTYPE_OUT, SIO_MODEM_CTRL, modemValue(dtr, rts), port)

    fun setFlow(flow: Flow, port: Int): Ctrl = when (flow) {
        Flow.NONE -> Ctrl(REQTYPE_OUT, SIO_SET_FLOW_CTRL, 0, port)
        Flow.RTS_CTS -> Ctrl(REQTYPE_OUT, SIO_SET_FLOW_CTRL, 0, 0x0100 or port)
        Flow.DTR_DSR -> Ctrl(REQTYPE_OUT, SIO_SET_FLOW_CTRL, 0, 0x0200 or port)
        Flow.XON_XOFF -> Ctrl(REQTYPE_OUT, SIO_SET_FLOW_CTRL, (0x13 shl 8) or 0x11, 0x0400 or port)
    }

    fun reset(port: Int) = Ctrl(REQTYPE_OUT, SIO_RESET, 0, port)
    fun latency(ms: Int, port: Int) = Ctrl(REQTYPE_OUT, SIO_SET_LATENCY_TIMER, ms.coerceIn(1, 255), port)

    /**
     * FTDI prefixes every max-size USB packet with 2 modem-status bytes. Copies the payload of
     * [len] bytes read into [src] (consecutive packets of [packetSize]) into [dst]; returns its length.
     */
    fun stripStatus(src: ByteArray, len: Int, packetSize: Int, dst: ByteArray): Int {
        var o = 0
        var i = 0
        while (i < len) {
            val end = minOf(i + packetSize, len)
            val payload = end - i - 2
            if (payload > 0) { System.arraycopy(src, i + 2, dst, o, payload); o += payload }
            i = end
        }
        return o
    }
}

/** Prolific PL2303 (H / HX / TA / TB / HXN "G" series). */
object Pl2303Protocol {
    const val VENDOR_OUT = 0x40
    const val VENDOR_IN = 0xC0
    const val VENDOR_REQ = 0x01
    const val HXN_VENDOR_WRITE = 0x80
    const val HXN_VENDOR_READ = 0x81
    const val HXN_RESET_REG = 0x07
    const val HXN_FLOWCTRL_REG = 0x0A

    enum class Type { H, HX, TA, TB, HXN }

    val HXN_PIDS = setOf(0x23A3, 0x23B3, 0x23C3, 0x23D3, 0x23E3, 0x23F3)

    val STANDARD_BAUDS = intArrayOf(75, 150, 300, 600, 1200, 1800, 2400, 3600, 4800, 7200, 9600, 14400, 19200, 28800, 38400,
        57600, 115200, 230400, 460800, 614400, 921600, 1228800, 2457600, 3000000, 6000000)

    fun detect(pid: Int, deviceClass: Int, maxPacket0: Int, bcdDevice: Int, bcdUsb: Int): Type = when {
        pid in HXN_PIDS -> Type.HXN
        deviceClass == 0x02 -> Type.H
        maxPacket0 != 0x40 -> Type.H
        bcdDevice == 0x300 && bcdUsb == 0x200 -> Type.TA
        bcdDevice == 0x500 -> Type.TB
        else -> Type.HX
    }

    /** The legacy vendor init sequence (not for HXN): list of (isRead, value, index). */
    fun initSequence(type: Type): List<Triple<Boolean, Int, Int>> {
        if (type == Type.HXN) return emptyList()
        return listOf(
            Triple(true, 0x8484, 0), Triple(false, 0x0404, 0), Triple(true, 0x8484, 0), Triple(true, 0x8383, 0),
            Triple(true, 0x8484, 0), Triple(false, 0x0404, 1), Triple(true, 0x8484, 0), Triple(true, 0x8383, 0),
            Triple(false, 0x0000, 1), Triple(false, 0x0001, 0), Triple(false, 0x0002, if (type == Type.H) 0x24 else 0x44),
        )
    }

    /**
     * 4-byte baud field of the line coding. Standard rates are sent directly; other rates (HX/TA/TB)
     * use the divisor format: baud = 12 MHz * 32 / (mantissa * 4^exponent).
     */
    fun encodeBaud(baud: Int, type: Type): ByteArray {
        if (type == Type.H || baud in STANDARD_BAUDS) {
            val b = if (type == Type.H) STANDARD_BAUDS.minByOrNull { kotlin.math.abs(it - baud) } ?: baud else baud
            return le32(b.toLong())
        }
        val baseline = 12_000_000L * 32
        var mantissa = baseline / baud
        if (mantissa == 0L) mantissa = 1
        var exponent = 0
        while (mantissa >= 512) {
            if (exponent < 7) { mantissa = mantissa shr 2; exponent++ } else { mantissa = 511; break }
        }
        return byteArrayOf((mantissa and 0xFF).toByte(), ((exponent shl 1) or (mantissa shr 8).toInt()).toByte(), 0, 0x80.toByte())
    }

    fun lineCoding(p: LineParams, type: Type): ByteArray =
        encodeBaud(p.baud, type) + byteArrayOf(p.stopBits.code.toByte(), p.parity.code.toByte(), p.dataBits.toByte())

    fun setLineCoding(p: LineParams, type: Type) = Ctrl(CdcAcmProtocol.REQTYPE_CLASS_OUT, CdcAcmProtocol.SET_LINE_CODING, 0, 0, lineCoding(p, type))
    fun setControlLines(dtr: Boolean, rts: Boolean) = Ctrl(CdcAcmProtocol.REQTYPE_CLASS_OUT, CdcAcmProtocol.SET_CONTROL_LINE_STATE,
        CdcAcmProtocol.controlLineState(dtr, rts), 0)

    /** Flow control: legacy types write vendor register 0; HXN uses register 0x0A (mask 0x1C). */
    fun setFlow(type: Type, flow: Flow): Ctrl = if (type == Type.HXN) {
        Ctrl(VENDOR_OUT, HXN_VENDOR_WRITE, HXN_FLOWCTRL_REG, if (flow == Flow.RTS_CTS) 0x18 else 0x1C)
    } else {
        Ctrl(VENDOR_OUT, VENDOR_REQ, 0, if (flow == Flow.RTS_CTS) (if (type == Type.H) 0x41 else 0x61) else 0)
    }

    fun hxnReset() = Ctrl(VENDOR_OUT, HXN_VENDOR_WRITE, HXN_RESET_REG, 0x03)
}

/** VID/PID → driver family. */
object UsbSerialIds {
    enum class Family { FTDI, CP210X, CH34X, PL2303, CDC }

    private val table: Map<Int, Map<Int, Family>> = mapOf(
        0x0403 to mapOf(0x6001 to Family.FTDI, 0x6010 to Family.FTDI, 0x6011 to Family.FTDI, 0x6014 to Family.FTDI, 0x6015 to Family.FTDI),
        0x10C4 to mapOf(0xEA60 to Family.CP210X, 0xEA61 to Family.CP210X, 0xEA63 to Family.CP210X, 0xEA70 to Family.CP210X,
            0xEA71 to Family.CP210X, 0xEA80 to Family.CP210X),
        0x1A86 to mapOf(0x7523 to Family.CH34X, 0x7522 to Family.CH34X, 0x5523 to Family.CH34X, 0xE523 to Family.CH34X),
        0x067B to mapOf(0x2303 to Family.PL2303, 0x23A3 to Family.PL2303, 0x23B3 to Family.PL2303, 0x23C3 to Family.PL2303,
            0x23D3 to Family.PL2303, 0x23E3 to Family.PL2303, 0x23F3 to Family.PL2303),
        0x0557 to mapOf(0x2008 to Family.PL2303), // ATEN UC-232A
        0x04BB to mapOf(0x0A03 to Family.PL2303), // IO-DATA
    )

    /** Known vendor-specific family, or CDC when the device exposes a CDC interface, else null. */
    fun familyFor(vid: Int, pid: Int, hasCdcInterface: Boolean): Family? =
        table[vid]?.get(pid) ?: (if (vid == 0x0403) Family.FTDI else null) ?: (if (hasCdcInterface) Family.CDC else null)

    fun familyName(f: Family) = when (f) {
        Family.FTDI -> "FTDI"
        Family.CP210X -> "CP210x"
        Family.CH34X -> "CH340/CH341"
        Family.PL2303 -> "PL2303"
        Family.CDC -> "CDC-ACM"
    }
}
