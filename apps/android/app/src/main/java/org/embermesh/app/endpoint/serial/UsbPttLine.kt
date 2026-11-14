package org.embermesh.app.endpoint.serial

import android.content.Context
import android.hardware.usb.UsbManager
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError

/**
 * A USB serial adapter used only to key a radio (RTS or DTR), for audio-modem links whose PTT goes
 * through a cheap USB-serial "PTT cable" that carries no data. Opened lazily on first key-up and kept
 * open; [close] releases it. Blocking calls; run them off the audio threads.
 */
class UsbPttLine(private val ctx: HostContext, private val deviceSel: String, private val line: String, private val invert: Boolean) {
    @Volatile private var driver: UsbSerialDriver? = null

    @Synchronized
    fun key(on: Boolean) {
        val drv = driver ?: open()
        val level = on != invert
        if (line == "dtr") drv.setLines(level, drv.rts) else drv.setLines(drv.dtr, level)
    }

    @Synchronized
    private fun open(): UsbSerialDriver {
        val usb = ctx.app.getSystemService(Context.USB_SERVICE) as UsbManager
        val dev = UsbSerialEndpoint.findDevice(usb, deviceSel) ?: throw HostError(HostError.NOT_FOUND, "PTT adapter $deviceSel not attached")
        if (!usb.hasPermission(dev) && !UsbSerialEndpoint.requestPermission(ctx, usb, dev)) throw HostError(HostError.DENIED, "USB permission for the PTT adapter was denied")
        val conn = usb.openDevice(dev) ?: throw HostError(HostError.IO_ERROR, "cannot open the PTT adapter")
        val d = UsbSerialDriver.create(dev, conn)
        try {
            // Idle: PTT line released (honouring inversion), the other line low.
            val idle = invert
            d.open(LineParams(9600, 8, Parity.NONE, StopBits.ONE, Flow.NONE), if (line == "dtr") idle else false, if (line == "rts") idle else false)
        } catch (t: Throwable) { d.close(); throw t }
        driver = d
        return d
    }

    @Synchronized
    fun close() {
        val d = driver ?: return
        try { if (line == "dtr") d.setLines(invert, d.rts) else d.setLines(d.dtr, invert) } catch (_: Throwable) {} // unkey
        driver = null
        try { d.close() } catch (_: Throwable) {}
    }
}
