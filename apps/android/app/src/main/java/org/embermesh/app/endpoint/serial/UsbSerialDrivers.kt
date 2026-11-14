package org.embermesh.app.endpoint.serial

import android.hardware.usb.UsbConstants
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbEndpoint
import android.hardware.usb.UsbInterface
import java.io.IOException

/**
 * In-house USB serial drivers. Each driver claims its interface(s), configures the UART and moves
 * bytes over the bulk endpoints. Control-transfer contents come from the pure encoders in
 * SerialProtocols.kt (unit-tested).
 */
abstract class UsbSerialDriver(val device: UsbDevice, protected val conn: UsbDeviceConnection) {
    abstract val name: String
    protected lateinit var epIn: UsbEndpoint
    protected lateinit var epOut: UsbEndpoint
    protected val claimed = ArrayList<UsbInterface>()
    @Volatile var dtr = false
        protected set
    @Volatile var rts = false
        protected set

    abstract fun open(params: LineParams, dtr: Boolean, rts: Boolean)
    abstract fun setLines(dtr: Boolean, rts: Boolean)

    /** Blocking read; returns bytes copied into [dst] (0 on timeout), throws on device loss. */
    open fun read(dst: ByteArray, timeoutMs: Int): Int {
        val n = conn.bulkTransfer(epIn, dst, minOf(dst.size, 16384), timeoutMs)
        return if (n < 0) 0 else n // -1 is also returned on timeout; detach is detected separately
    }

    fun write(data: ByteArray, timeoutMs: Int = 2000) {
        var off = 0
        val chunk = maxOf(epOut.maxPacketSize * 16, 64)
        while (off < data.size) {
            val len = minOf(chunk, data.size - off)
            val n = conn.bulkTransfer(epOut, data, off, len, timeoutMs)
            if (n <= 0) throw IOException("USB write failed ($n)")
            off += n
        }
    }

    open fun close() {
        for (i in claimed) try { conn.releaseInterface(i) } catch (_: Exception) {}
        claimed.clear()
        try { conn.close() } catch (_: Exception) {}
    }

    protected fun ctrl(c: Ctrl, timeoutMs: Int = 1000): Int {
        val d = c.data
        val r = conn.controlTransfer(c.type, c.request, c.value, c.index, d, d?.size ?: 0, timeoutMs)
        if (r < 0) throw IOException("control transfer failed: $c")
        return r
    }

    protected fun ctrlIn(type: Int, request: Int, value: Int, index: Int, len: Int): ByteArray {
        val b = ByteArray(len)
        val r = conn.controlTransfer(type, request, value, index, b, len, 1000)
        if (r < 0) throw IOException("control read failed: req=0x%02x val=0x%04x".format(request, value))
        return b
    }

    protected fun claim(i: UsbInterface) {
        if (!conn.claimInterface(i, true)) throw IOException("cannot claim USB interface ${i.id}")
        claimed.add(i)
    }

    protected fun bulkEndpoints(i: UsbInterface) {
        var inE: UsbEndpoint? = null
        var outE: UsbEndpoint? = null
        for (k in 0 until i.endpointCount) {
            val e = i.getEndpoint(k)
            if (e.type != UsbConstants.USB_ENDPOINT_XFER_BULK) continue
            if (e.direction == UsbConstants.USB_DIR_IN && inE == null) inE = e
            if (e.direction == UsbConstants.USB_DIR_OUT && outE == null) outE = e
        }
        epIn = inE ?: throw IOException("no bulk IN endpoint")
        epOut = outE ?: throw IOException("no bulk OUT endpoint")
    }

    companion object {
        fun hasCdc(d: UsbDevice): Boolean {
            if (d.deviceClass == UsbConstants.USB_CLASS_COMM) return true
            for (i in 0 until d.interfaceCount) {
                val c = d.getInterface(i).interfaceClass
                if (c == UsbConstants.USB_CLASS_COMM || c == UsbConstants.USB_CLASS_CDC_DATA) return true
            }
            return false
        }

        fun familyOf(d: UsbDevice): UsbSerialIds.Family? = UsbSerialIds.familyFor(d.vendorId, d.productId, hasCdc(d))

        fun create(d: UsbDevice, c: UsbDeviceConnection): UsbSerialDriver = when (familyOf(d)) {
            UsbSerialIds.Family.FTDI -> FtdiDriver(d, c)
            UsbSerialIds.Family.CP210X -> Cp210xDriver(d, c)
            UsbSerialIds.Family.CH34X -> Ch34xDriver(d, c)
            UsbSerialIds.Family.PL2303 -> Pl2303Driver(d, c)
            UsbSerialIds.Family.CDC -> CdcAcmDriver(d, c)
            null -> throw IOException("unsupported USB device %04x:%04x".format(d.vendorId, d.productId))
        }
    }
}

class CdcAcmDriver(device: UsbDevice, conn: UsbDeviceConnection) : UsbSerialDriver(device, conn) {
    override val name = "CDC-ACM"
    private var controlIndex = 0

    override fun open(params: LineParams, dtr: Boolean, rts: Boolean) {
        val layout = (0 until device.interfaceCount).map { device.getInterface(it) }.map { itf ->
            var bi = false; var bo = false
            for (k in 0 until itf.endpointCount) {
                val e = itf.getEndpoint(k)
                if (e.type == UsbConstants.USB_ENDPOINT_XFER_BULK) { if (e.direction == UsbConstants.USB_DIR_IN) bi = true else bo = true }
            }
            CdcLayout.Itf(itf.id, itf.interfaceClass, itf.interfaceSubclass, bi && bo)
        }
        val pick = CdcLayout.pick(layout)
        val comm: UsbInterface? = pick.first?.let { idx -> device.getInterface(idx) }
        val data: UsbInterface? = pick.second?.let { idx -> device.getInterface(idx) }
        val d = data ?: throw IOException("no CDC data interface")
        if (comm != null) { claim(comm); controlIndex = comm.id } else controlIndex = d.id
        claim(d)
        bulkEndpoints(d)
        try { ctrl(CdcAcmProtocol.setLineCoding(controlIndex, params)) } catch (e: IOException) {
            if (comm != null) throw e // native-USB boards without a comm interface ignore line coding
        }
        setLines(dtr, rts)
    }

    override fun setLines(dtr: Boolean, rts: Boolean) {
        this.dtr = dtr; this.rts = rts
        try { ctrl(CdcAcmProtocol.setControlLines(controlIndex, dtr, rts)) } catch (_: IOException) {}
    }
}

class Cp210xDriver(device: UsbDevice, conn: UsbDeviceConnection) : UsbSerialDriver(device, conn) {
    override val name = "CP210x"
    private var iface = 0
    private var flow = Flow.NONE

    override fun open(params: LineParams, dtr: Boolean, rts: Boolean) {
        val itf = device.getInterface(0) // port 0 of multi-port CP2105/CP2108
        iface = itf.id
        claim(itf)
        bulkEndpoints(itf)
        ctrl(Cp210xProtocol.enable(iface))
        ctrl(Cp210xProtocol.baud(iface, params.baud))
        ctrl(Cp210xProtocol.line(iface, params))
        flow = params.flow
        try { ctrl(Cp210xProtocol.setFlow(iface, flow, dtr, rts)) } catch (_: IOException) {}
        setLines(dtr, rts)
        try { ctrl(Cp210xProtocol.purge(iface)) } catch (_: IOException) {}
    }

    override fun setLines(dtr: Boolean, rts: Boolean) {
        this.dtr = dtr; this.rts = rts
        ctrl(Cp210xProtocol.lines(iface, dtr, rts))
    }

    override fun close() {
        try { ctrl(Cp210xProtocol.disable(iface)) } catch (_: Exception) {}
        super.close()
    }
}

class Ch34xDriver(device: UsbDevice, conn: UsbDeviceConnection) : UsbSerialDriver(device, conn) {
    override val name = "CH340/CH341"
    private var version = 0

    override fun open(params: LineParams, dtr: Boolean, rts: Boolean) {
        val itf = device.getInterface(0)
        claim(itf)
        bulkEndpoints(itf)
        version = try { ctrlIn(Ch34xProtocol.REQTYPE_IN, Ch34xProtocol.REQ_READ_VERSION, 0, 0, 2)[0].toInt() and 0xFF } catch (_: IOException) { 0 }
        ctrl(Ch34xProtocol.serialInit())
        ctrl(Ch34xProtocol.setBaud(params.baud, version))
        if (version >= 0x30 || version == 0) {
            try { ctrl(Ch34xProtocol.setLcr(params)) } catch (_: IOException) {}
        }
        setLines(dtr, rts)
    }

    override fun setLines(dtr: Boolean, rts: Boolean) {
        this.dtr = dtr; this.rts = rts
        ctrl(Ch34xProtocol.modemCtrl(dtr, rts))
    }
}

class FtdiDriver(device: UsbDevice, conn: UsbDeviceConnection) : UsbSerialDriver(device, conn) {
    override var name = "FTDI"
        private set
    private var port = 1
    private var chip = FtdiProtocol.Chip.FT232R
    private var packet = 64
    private var raw = ByteArray(16384)

    override fun open(params: LineParams, dtr: Boolean, rts: Boolean) {
        val itf = device.getInterface(0)
        port = itf.id + 1
        claim(itf)
        bulkEndpoints(itf)
        packet = epIn.maxPacketSize.coerceAtLeast(64)
        chip = FtdiProtocol.chipFromBcd(bcdDevice(), device.interfaceCount)
        name = "FTDI ${chip.name}"
        ctrl(FtdiProtocol.reset(port))
        ctrl(FtdiProtocol.setBaud(chip, params.baud, port))
        ctrl(FtdiProtocol.setData(params, port))
        try { ctrl(FtdiProtocol.setFlow(params.flow, port)) } catch (_: IOException) {}
        try { ctrl(FtdiProtocol.latency(4, port)) } catch (_: IOException) {}
        setLines(dtr, rts)
    }

    /** bcdDevice lives in the raw device descriptor (bytes 12-13). */
    private fun bcdDevice(): Int {
        val d = conn.rawDescriptors ?: return 0x600
        return if (d.size >= 14) (d[12].toInt() and 0xFF) or ((d[13].toInt() and 0xFF) shl 8) else 0x600
    }

    override fun read(dst: ByteArray, timeoutMs: Int): Int {
        val want = minOf(raw.size, (dst.size / packet + 1) * packet)
        val n = conn.bulkTransfer(epIn, raw, want, timeoutMs)
        if (n <= 2) return 0
        return FtdiProtocol.stripStatus(raw, n, packet, dst)
    }

    override fun setLines(dtr: Boolean, rts: Boolean) {
        this.dtr = dtr; this.rts = rts
        ctrl(FtdiProtocol.setModem(dtr, rts, port))
    }
}

class Pl2303Driver(device: UsbDevice, conn: UsbDeviceConnection) : UsbSerialDriver(device, conn) {
    override var name = "PL2303"
        private set
    private var type = Pl2303Protocol.Type.HX

    override fun open(params: LineParams, dtr: Boolean, rts: Boolean) {
        val itf = device.getInterface(0)
        claim(itf)
        bulkEndpoints(itf)
        val d = conn.rawDescriptors ?: ByteArray(18)
        fun u16(o: Int) = if (d.size > o + 1) (d[o].toInt() and 0xFF) or ((d[o + 1].toInt() and 0xFF) shl 8) else 0
        type = Pl2303Protocol.detect(device.productId, device.deviceClass, if (d.size > 7) d[7].toInt() and 0xFF else 0x40, u16(12), u16(2))
        name = "PL2303 ${type.name}"
        for ((isRead, value, index) in Pl2303Protocol.initSequence(type)) {
            if (isRead) ctrlIn(Pl2303Protocol.VENDOR_IN, Pl2303Protocol.VENDOR_REQ, value, index, 1)
            else ctrl(Ctrl(Pl2303Protocol.VENDOR_OUT, Pl2303Protocol.VENDOR_REQ, value, index))
        }
        if (type == Pl2303Protocol.Type.HXN) try { ctrl(Pl2303Protocol.hxnReset()) } catch (_: IOException) {}
        ctrl(Pl2303Protocol.setLineCoding(params, type))
        try { ctrl(Pl2303Protocol.setFlow(type, params.flow)) } catch (_: IOException) {}
        setLines(dtr, rts)
    }

    override fun setLines(dtr: Boolean, rts: Boolean) {
        this.dtr = dtr; this.rts = rts
        ctrl(Pl2303Protocol.setControlLines(dtr, rts))
    }
}

/**
 * Which interfaces a CDC-ACM serial port uses (pure, unit-tested). Composite devices such as the AIOC
 * (USB audio + CDC serial in one device) also carry audio-class interfaces: only the CDC comm
 * (class 2/subclass 2) and its data interface (class 10) are claimed, so Android's USB audio keeps
 * the audio interfaces and the audio endpoint and serial PTT work at the same time.
 */
object CdcLayout {
    data class Itf(val id: Int, val cls: Int, val sub: Int, val bulkInOut: Boolean)

    /** Returns (comm index, data index) as positions in [itfs]. */
    fun pick(itfs: List<Itf>): Pair<Int?, Int?> {
        val comm = itfs.indexOfFirst { it.cls == 2 && it.sub == 2 }.takeIf { it >= 0 }
        var data = itfs.indices.firstOrNull { i -> itfs[i].cls == 10 && (comm == null || itfs[i].id > itfs[comm].id) && itfs[i].bulkInOut }
            ?: itfs.indices.firstOrNull { i -> itfs[i].cls == 10 && (comm == null || itfs[i].id > itfs[comm].id) }
        if (data == null) data = itfs.indices.firstOrNull { i -> itfs[i].bulkInOut && itfs[i].cls != 1 } // never an audio interface
        return comm to data
    }
}
