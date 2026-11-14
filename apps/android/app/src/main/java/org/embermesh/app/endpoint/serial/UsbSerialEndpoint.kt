package org.embermesh.app.endpoint.serial

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.os.Build
import org.embermesh.app.endpoint.QueuedWriter
import org.embermesh.app.host.Endpoint
import org.embermesh.app.host.EndpointFactory
import org.embermesh.app.host.EndpointSink
import org.embermesh.app.host.Fields
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.embermesh.app.util.bool
import org.embermesh.app.util.int
import org.embermesh.app.util.str
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/**
 * USB host serial (USB OTG): radios with USB CAT/data ports, KISS TNCs, Meshtastic / RNode / LoRa
 * boards, programming cables. setPtt() keys RTS or DTR for `link.ptt` events.
 */
class UsbSerialEndpoint(private val ctx: HostContext, config: JSONObject, sink: EndpointSink) : Endpoint(config, sink) {
    private val usb = ctx.app.getSystemService(Context.USB_SERVICE) as UsbManager
    private val deviceSel = config.str("device", "auto")
    private val params = paramsFrom(config)
    private val dtrIdle = config.bool("dtr", true)
    private val rtsIdle = config.bool("rts", false)
    private val pttLine = config.str("ptt_line", "none")
    private val pttInvert = config.bool("ptt_invert", false)

    @Volatile private var running = false
    @Volatile private var driver: UsbSerialDriver? = null
    private var writer: QueuedWriter? = null
    private var reader: Thread? = null
    private var detachReceiver: BroadcastReceiver? = null
    @Volatile private var ptt = false

    override fun open() {
        running = true
        ctx.io.execute {
            try {
                val dev = findDevice(usb, deviceSel) ?: throw HostError(HostError.NOT_FOUND,
                    if (deviceSel == "auto" || deviceSel.isEmpty()) "no supported USB serial device attached" else "USB device $deviceSel not attached")
                if (!usb.hasPermission(dev) && !requestPermission(dev)) throw HostError(HostError.DENIED, "USB permission denied")
                val conn = usb.openDevice(dev) ?: throw HostError(HostError.IO_ERROR, "cannot open USB device")
                val drv = UsbSerialDriver.create(dev, conn)
                try { drv.open(params, dtrIdle, rtsIdle) } catch (t: Throwable) { drv.close(); throw t }
                if (!running) { drv.close(); return@execute }
                driver = drv
                watchDetach(dev)
                writer = QueuedWriter("usb", 256, { drv.write(it) }) { t -> fail("write failed: ${t.message}") }
                reader = Thread({ readLoop(drv) }, "ember-usb-rx").apply { isDaemon = true; start() }
                sink.onStatus(status())
                sink.onState(true)
            } catch (e: HostError) {
                fail(e.message ?: e.code)
            } catch (t: Throwable) {
                fail(t.message ?: t.toString())
            }
        }
    }

    private fun readLoop(drv: UsbSerialDriver) {
        val buf = ByteArray(16384)
        while (running) {
            val n = try { drv.read(buf, 200) } catch (t: Throwable) { if (running) fail("read failed: ${t.message}"); return }
            if (n > 0) sink.onBytes(buf, 0, n)
        }
    }

    private fun requestPermission(dev: UsbDevice): Boolean = requestPermission(ctx, usb, dev)

    private fun watchDetach(dev: UsbDevice) {
        val r = object : BroadcastReceiver() {
            override fun onReceive(c: Context, i: Intent) {
                @Suppress("DEPRECATION")
                val d = if (Build.VERSION.SDK_INT >= 33) i.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
                else i.getParcelableExtra<UsbDevice>(UsbManager.EXTRA_DEVICE)
                if (d == null || d.deviceName == dev.deviceName) fail("device unplugged")
            }
        }
        detachReceiver = r
        register(ctx.app, r, IntentFilter(UsbManager.ACTION_USB_DEVICE_DETACHED))
    }

    private fun fail(msg: String) {
        if (!running) return
        close()
        sink.onState(false, msg)
    }

    override fun write(data: ByteArray) { writer?.offer(data) }

    override fun setPtt(on: Boolean) {
        val drv = driver ?: return
        ptt = on
        val level = on != pttInvert
        ctx.io.execute {
            try {
                when (pttLine) {
                    "rts" -> drv.setLines(drv.dtr, level)
                    "dtr" -> drv.setLines(level, drv.rts)
                }
            } catch (t: Throwable) { sink.log("PTT failed: $t") }
        }
    }

    /** Another link (an audio modem) keys its radio through this adapter's RTS or DTR. */
    override fun keyLine(line: String, invert: Boolean, on: Boolean) {
        val drv = driver ?: throw HostError(HostError.IO_ERROR, "serial port not open")
        val level = on != invert
        ptt = on
        ctx.io.execute {
            try { if (line == "dtr") drv.setLines(level, drv.rts) else drv.setLines(drv.dtr, level) } catch (t: Throwable) { sink.log("PTT failed: $t") }
        }
    }

    override fun close() {
        running = false
        writer?.close()
        reader?.interrupt()
        detachReceiver?.let { try { ctx.app.unregisterReceiver(it) } catch (_: Exception) {} }
        detachReceiver = null
        driver?.let { d -> try { d.close() } catch (_: Exception) {} }
        driver = null
    }

    override fun status(): JSONObject {
        val d = driver ?: return JSONObject().put("device", deviceSel)
        return JSONObject().put("driver", d.name).put("device", deviceId(d.device)).put("product", d.device.productName ?: "")
            .put("baud", params.baud).put("dtr", d.dtr).put("rts", d.rts).put("ptt", ptt)
    }

    companion object {
        fun paramsFrom(c: JSONObject) = LineParams(
            baud = c.int("baud", 9600).coerceIn(50, 12_000_000),
            dataBits = c.int("data_bits", 8).coerceIn(5, 8),
            parity = Parity.parse(c.str("parity", "none")),
            stopBits = StopBits.parse(c.str("stop_bits", "1")),
            flow = Flow.parse(c.str("flow", "none")),
        )

        fun deviceId(d: UsbDevice): String {
            val serial = try { d.serialNumber } catch (_: SecurityException) { null } // needs permission on 29+
            return "%04x:%04x".format(d.vendorId, d.productId) + (serial?.takeIf { it.isNotBlank() }?.let { ":$it" } ?: "")
        }

        /** "auto", "vid:pid[:serial]" or a /dev/bus/usb device name. */
        fun findDevice(usb: UsbManager, sel: String): UsbDevice? {
            val all = usb.deviceList.values.filter { UsbSerialDriver.familyOf(it) != null }
            if (sel.isEmpty() || sel == "auto") return all.firstOrNull()
            all.firstOrNull { it.deviceName == sel }?.let { return it }
            val parts = sel.split(':')
            val vid = parts.getOrNull(0)?.toIntOrNull(16)
            val pid = parts.getOrNull(1)?.toIntOrNull(16)
            val serial = parts.getOrNull(2)
            val matches = all.filter { it.vendorId == vid && it.productId == pid }
            if (serial != null) matches.firstOrNull { (try { it.serialNumber } catch (_: SecurityException) { null }) == serial }?.let { return it }
            return matches.firstOrNull()
        }

        /** Asks the user for access to [dev] (blocks up to 2 min). */
        fun requestPermission(ctx: HostContext, usb: UsbManager, dev: UsbDevice): Boolean {
            val action = "org.embermesh.app.USB_PERMISSION"
            val latch = CountDownLatch(1)
            var granted = false
            val r = object : BroadcastReceiver() {
                override fun onReceive(c: Context, i: Intent) {
                    granted = i.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)
                    latch.countDown()
                }
            }
            register(ctx.app, r, IntentFilter(action))
            try {
                val flags = if (Build.VERSION.SDK_INT >= 31) PendingIntent.FLAG_MUTABLE else 0
                val pi = PendingIntent.getBroadcast(ctx.app, dev.deviceId, Intent(action).setPackage(ctx.app.packageName), flags)
                usb.requestPermission(dev, pi)
                latch.await(120, TimeUnit.SECONDS)
            } finally {
                try { ctx.app.unregisterReceiver(r) } catch (_: Exception) {}
            }
            return granted || usb.hasPermission(dev)
        }

        /** Attached USB serial adapters as (deviceId, label) for pickers. */
        fun attached(ctx: HostContext): List<Pair<String, String>> {
            if (!ctx.hasContext) return emptyList()
            return try {
                val usb = ctx.app.getSystemService(Context.USB_SERVICE) as UsbManager
                usb.deviceList.values.mapNotNull { d ->
                    val f = UsbSerialDriver.familyOf(d) ?: return@mapNotNull null
                    deviceId(d) to "${d.productName ?: UsbSerialIds.familyName(f)} (${UsbSerialIds.familyName(f)})"
                }
            } catch (_: Exception) { emptyList() }
        }

        fun register(c: Context, r: BroadcastReceiver, f: IntentFilter) {
            if (Build.VERSION.SDK_INT >= 33) c.registerReceiver(r, f, Context.RECEIVER_NOT_EXPORTED)
            else @Suppress("UnspecifiedRegisterReceiverFlag") c.registerReceiver(r, f)
        }
    }

    object Factory : EndpointFactory {
        override val kind = "serial"
        override val title = "USB serial"
        override val description = "USB OTG serial: radio data/CAT ports, KISS TNCs, Meshtastic / RNode / LoRa boards. Supports CDC-ACM, CP210x, CH340/CH341, FTDI and PL2303 adapters."
        override val scannable = true
        override val linkTypes = listOf("kiss", "stream", "meshtastic", "rnode", "lora_at")

        override fun unavailableReason(ctx: HostContext): String? =
            if (!ctx.app.packageManager.hasSystemFeature(PackageManager.FEATURE_USB_HOST)) "this phone has no USB host (OTG) support" else null

        override fun configSchema(ctx: HostContext): JSONArray {
            val devs = arrayListOf("auto" to "First supported device")
            if (ctx.hasContext) try {
                val usb = ctx.app.getSystemService(Context.USB_SERVICE) as UsbManager
                for (d in usb.deviceList.values) {
                    val f = UsbSerialDriver.familyOf(d) ?: continue
                    devs.add(deviceId(d) to "${d.productName ?: UsbSerialIds.familyName(f)} (${UsbSerialIds.familyName(f)})")
                }
            } catch (_: Exception) {}
            val bauds = listOf(1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600)
            return Fields.of {
                select("device", "Device", devs, default = "auto", required = true)
                select("baud", "Baud rate", bauds.map { it.toString() to "$it" }, default = "9600",
                    help = "KISS TNCs: usually 9600 or 57600. Meshtastic / RNode boards: 115200.")
                select("data_bits", "Data bits", listOf("8" to "8", "7" to "7", "6" to "6", "5" to "5"), default = "8", advanced = true)
                select("parity", "Parity", listOf("none" to "None", "even" to "Even", "odd" to "Odd", "mark" to "Mark", "space" to "Space"), default = "none", advanced = true)
                select("stop_bits", "Stop bits", listOf("1" to "1", "1.5" to "1.5", "2" to "2"), default = "1", advanced = true)
                select("flow", "Flow control", listOf("none" to "None", "rts_cts" to "RTS/CTS", "dtr_dsr" to "DTR/DSR", "xon_xoff" to "XON/XOFF"), default = "none", advanced = true)
                bool("dtr", "Assert DTR", default = true, advanced = true, help = "Most boards need DTR on to send data. Some ESP32 boards reset when DTR/RTS toggle.")
                bool("rts", "Assert RTS", default = false, advanced = true)
                select("ptt_line", "PTT keying line", listOf("none" to "None (VOX / TNC keys)", "rts" to "RTS", "dtr" to "DTR"), default = "none",
                    help = "For a radio interface cable that keys the transmitter with RTS or DTR.")
                bool("ptt_invert", "Invert PTT", default = false, advanced = true)
            }
        }

        override fun scan(ctx: HostContext, timeoutMs: Long): JSONArray {
            val usb = ctx.app.getSystemService(Context.USB_SERVICE) as UsbManager
            val out = JSONArray()
            for (d in usb.deviceList.values) {
                val f = UsbSerialDriver.familyOf(d) ?: continue
                val id = deviceId(d)
                out.put(JSONObject().put("id", id).put("name", d.productName ?: UsbSerialIds.familyName(f))
                    .put("detail", "${UsbSerialIds.familyName(f)} · ${"%04x:%04x".format(d.vendorId, d.productId)}" +
                        (d.manufacturerName?.let { " · $it" } ?: ""))
                    .put("config", JSONObject().put("device", id)))
            }
            return out
        }

        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink) = UsbSerialEndpoint(ctx, config, sink)
    }
}
