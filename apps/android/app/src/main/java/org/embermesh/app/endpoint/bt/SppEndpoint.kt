package org.embermesh.app.endpoint.bt

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import org.embermesh.app.endpoint.QueuedWriter
import org.embermesh.app.endpoint.StreamReader
import org.embermesh.app.endpoint.serial.UsbSerialEndpoint
import org.embermesh.app.endpoint.writeFlush
import org.embermesh.app.host.Endpoint
import org.embermesh.app.host.EndpointFactory
import org.embermesh.app.host.EndpointSink
import org.embermesh.app.host.Fields
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.embermesh.app.util.str
import org.json.JSONArray
import org.json.JSONObject
import java.io.IOException
import java.util.UUID

/** Bluetooth Classic serial (RFCOMM / SPP): Mobilinkd TNC, Kenwood TH-D74/D75, BT KISS TNCs, HC-05 bridges. */
@SuppressLint("MissingPermission") // permissions are ensured by LinkManager before create()
class SppEndpoint(private val ctx: HostContext, config: JSONObject, sink: EndpointSink) : Endpoint(config, sink) {
    private val address = config.str("address").trim().uppercase()
    @Volatile private var closed = false
    @Volatile private var socket: BluetoothSocket? = null
    private var writer: QueuedWriter? = null
    private var reader: StreamReader? = null

    override fun open() {
        if (!BluetoothAdapter.checkBluetoothAddress(address)) throw HostError.badParams("invalid Bluetooth address '$address'")
        val adapter = adapter(ctx) ?: throw HostError.unsupported("no Bluetooth")
        if (!adapter.isEnabled) throw HostError(HostError.IO_ERROR, "Bluetooth is off")
        ctx.io.execute {
            try {
                val dev = adapter.getRemoteDevice(address)
                try { adapter.cancelDiscovery() } catch (_: Exception) {}
                val s = connect(dev)
                if (closed) { s.close(); return@execute }
                socket = s
                val out = s.outputStream
                writer = QueuedWriter("spp", 256, { out.writeFlush(it) }) { t -> end(t) }
                reader = StreamReader("spp", s.inputStream, { b, n -> sink.onBytes(b, 0, n) }, { t -> end(t) }, 1024)
                sink.onStatus(JSONObject().put("device", dev.name ?: address))
                sink.onState(true)
            } catch (t: Throwable) { end(t) }
        }
    }

    /** Secure SPP, then insecure, then the classic channel-1 reflection fallback for old stacks. */
    private fun connect(dev: BluetoothDevice): BluetoothSocket {
        var last: IOException? = null
        val attempts = listOf<() -> BluetoothSocket>(
            { dev.createRfcommSocketToServiceRecord(SPP) },
            { dev.createInsecureRfcommSocketToServiceRecord(SPP) },
            { dev.javaClass.getMethod("createRfcommSocket", Int::class.javaPrimitiveType).invoke(dev, 1) as BluetoothSocket },
        )
        for (mk in attempts) {
            val s = try { mk() } catch (t: Throwable) { continue }
            try {
                s.connect()
                return s
            } catch (e: IOException) {
                last = e
                try { s.close() } catch (_: Exception) {}
            }
            if (closed) break
        }
        throw last ?: IOException("connect failed")
    }

    private fun end(t: Throwable?) {
        if (closed) return
        close()
        sink.onState(false, t?.message ?: "disconnected")
    }

    override fun write(data: ByteArray) { writer?.offer(data) }

    override fun close() {
        closed = true
        writer?.close()
        reader?.close()
        try { socket?.close() } catch (_: Exception) {}
    }

    companion object {
        val SPP: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

        fun adapter(ctx: HostContext): BluetoothAdapter? =
            (ctx.app.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter
    }

    @SuppressLint("MissingPermission")
    object Factory : EndpointFactory {
        override val kind = "bt_spp"
        override val title = "Bluetooth serial (SPP)"
        override val description = "Bluetooth Classic serial port: Mobilinkd TNC, Kenwood TH-D74/D75, Bluetooth KISS TNCs and serial adapters. Pair the device in Android settings first."
        override val scannable = true
        override val linkTypes = listOf("kiss", "stream", "rnode", "lora_at")

        override fun unavailableReason(ctx: HostContext): String? =
            if (adapter(ctx) == null) "this phone has no Bluetooth" else null

        override fun permissions(ctx: HostContext, config: JSONObject): List<String> = when {
            Build.VERSION.SDK_INT >= 31 -> if (config.has("_scan")) listOf(Manifest.permission.BLUETOOTH_CONNECT, Manifest.permission.BLUETOOTH_SCAN)
                else listOf(Manifest.permission.BLUETOOTH_CONNECT)
            config.has("_scan") && Build.VERSION.SDK_INT >= 23 -> listOf(Manifest.permission.ACCESS_FINE_LOCATION)
            else -> emptyList()
        }

        override fun configSchema(ctx: HostContext): JSONArray {
            val bonded = try { adapter(ctx)?.bondedDevices?.map { it.address to "${it.name ?: it.address} (${it.address})" } } catch (_: SecurityException) { null }
            return Fields.of {
                if (bonded.isNullOrEmpty()) text("address", "Device address", default = "", required = true, help = "AA:BB:CC:DD:EE:FF — use Scan, or pair the device in Android settings")
                else select("address", "Device", bonded, required = true)
            }
        }

        override fun scan(ctx: HostContext, timeoutMs: Long): JSONArray {
            val a = adapter(ctx) ?: throw HostError.unsupported("no Bluetooth")
            if (!a.isEnabled) throw HostError(HostError.IO_ERROR, "Bluetooth is off")
            val found = LinkedHashMap<String, JSONObject>()
            for (d in a.bondedDevices ?: emptySet()) found[d.address] = describe(d, null, true)
            val r = object : BroadcastReceiver() {
                override fun onReceive(c: Context, i: Intent) {
                    @Suppress("DEPRECATION")
                    val d = (if (Build.VERSION.SDK_INT >= 33) i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java)
                    else i.getParcelableExtra<BluetoothDevice>(BluetoothDevice.EXTRA_DEVICE)) ?: return
                    if (d.type == BluetoothDevice.DEVICE_TYPE_LE) return
                    val rssi = i.getShortExtra(BluetoothDevice.EXTRA_RSSI, Short.MIN_VALUE)
                    synchronized(found) { if (!found.containsKey(d.address)) found[d.address] = describe(d, rssi.toInt().takeIf { it != Short.MIN_VALUE.toInt() }, false) }
                }
            }
            UsbSerialEndpoint.register(ctx.app, r, IntentFilter(BluetoothDevice.ACTION_FOUND))
            try {
                a.startDiscovery()
                Thread.sleep(timeoutMs.coerceIn(1000, 12_000))
            } catch (_: InterruptedException) {
            } finally {
                try { a.cancelDiscovery() } catch (_: Exception) {}
                try { ctx.app.unregisterReceiver(r) } catch (_: Exception) {}
            }
            return JSONArray(synchronized(found) { found.values.toList() })
        }

        private fun describe(d: BluetoothDevice, rssi: Int?, bonded: Boolean): JSONObject {
            val o = JSONObject().put("id", d.address).put("name", d.name ?: d.address)
                .put("detail", (if (bonded) "paired · " else "") + d.address)
                .put("config", JSONObject().put("address", d.address))
            if (rssi != null) o.put("rssi", rssi)
            return o
        }

        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink) = SppEndpoint(ctx, config, sink)
    }
}
