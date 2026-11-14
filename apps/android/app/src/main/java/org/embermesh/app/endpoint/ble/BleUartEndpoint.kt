package org.embermesh.app.endpoint.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.os.Build
import android.os.ParcelUuid
import android.util.Log
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
import java.util.UUID
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit

/** Parsed `ble` endpoint config (pure, unit-tested). */
data class BleUartConfig(
    val address: String,
    val service: UUID,
    val rx: UUID,
    val tx: UUID,
    val withResponse: Boolean,
    val mtu: Int,
    /** "uart" (Nordic-UART style byte pipe) or "meshtastic" (Meshtastic GATT API, see [MeshtasticBle]). */
    val profile: String = "uart",
) {
    val meshtastic: Boolean get() = profile == "meshtastic"

    companion object {
        /** Mobilinkd TNC3/TNC4 "BLE KISS API": KISS byte stream; we write ...0002, the TNC notifies ...0003. */
        val KISS_SERVICE: UUID = UUID.fromString("00000001-ba2a-46c9-ae49-01b0961f68bb")
        val KISS_RX: UUID = UUID.fromString("00000002-ba2a-46c9-ae49-01b0961f68bb")
        val KISS_TX: UUID = UUID.fromString("00000003-ba2a-46c9-ae49-01b0961f68bb")
        private val MAC = Regex("^[0-9A-F]{2}(:[0-9A-F]{2}){5}$")

        fun from(c: JSONObject): BleUartConfig {
            val addr = c.str("address").trim().uppercase().replace('-', ':')
            if (!MAC.matches(addr)) throw HostError.badParams("Bluetooth address must look like AA:BB:CC:DD:EE:FF")
            fun uuid(key: String, def: UUID): UUID {
                val t = c.str(key).trim()
                if (t.isEmpty()) return def
                return BleUuids.parse(t) ?: throw HostError.badParams("$key is not a UUID")
            }
            if (c.str("profile") == "kiss_ble") return BleUartConfig(addr, KISS_SERVICE, KISS_RX, KISS_TX,
                c.bool("write_with_response", false), c.int("mtu", 247).coerceIn(23, 517), "kiss_ble")
            if (c.str("profile") == "meshtastic") return BleUartConfig(addr, MeshtasticBle.SERVICE, MeshtasticBle.TO_RADIO,
                MeshtasticBle.FROM_NUM, true, c.int("mtu", 512).coerceIn(23, 517), "meshtastic")
            return BleUartConfig(
                address = addr,
                service = uuid("service_uuid", BleUuids.NUS_SERVICE),
                rx = uuid("rx_uuid", BleUuids.NUS_RX),
                tx = uuid("tx_uuid", BleUuids.NUS_TX),
                withResponse = c.bool("write_with_response", false),
                mtu = c.int("mtu", 247).coerceIn(23, 517),
            )
        }
    }
}

/**
 * `ble`: central connection to a Nordic-UART-style peripheral (BLE KISS TNCs, serial bridges,
 * LoRa boards). One device per link; on disconnect it reports down and LinkManager recreates it.
 */
@SuppressLint("MissingPermission")
class BleUartEndpoint(private val ctx: HostContext, config: JSONObject, sink: EndpointSink) : Endpoint(config, sink) {
    private val cfg = BleUartConfig.from(config)
    private val worker = Executors.newSingleThreadScheduledExecutor(HostContext.named("ble-uart"))
    private val ops = GattOpQueue(worker)
    @Volatile private var closed = false
    private var gatt: BluetoothGatt? = null
    private var rx: BluetoothGattCharacteristic? = null
    private var writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
    private var payload = 20
    private var up = false
    private var downReported = false
    private var timeout: ScheduledFuture<*>? = null
    private var fromRadio: BluetoothGattCharacteristic? = null
    private val unframer = MeshtasticBle.Unframer()
    private var draining = false

    private fun post(block: () -> Unit) {
        if (closed) return
        try { worker.execute { if (!closed) try { block() } catch (t: Throwable) { log("error: $t") } } } catch (_: Exception) {}
    }

    override fun open() = post {
        val adapter = BleSupport.adapter(ctx)
        if (adapter == null || !adapter.isEnabled) { down("Bluetooth is off"); return@post }
        if (!BluetoothAdapter.checkBluetoothAddress(cfg.address)) { down("bad address"); return@post }
        val dev = adapter.getRemoteDevice(cfg.address)
        val g = try {
            if (Build.VERSION.SDK_INT >= 23) dev.connectGatt(ctx.app, false, callback, BluetoothDevice.TRANSPORT_LE)
            else dev.connectGatt(ctx.app, false, callback)
        } catch (e: SecurityException) { down("Bluetooth permission missing"); return@post }
        if (g == null) { down("connect failed"); return@post }
        gatt = g
        timeout = worker.schedule({ post { if (!up) down("device not reachable (connect timeout)") } }, 25, TimeUnit.SECONDS)
    }

    override fun close() {
        if (closed) return
        try { worker.submit { release() }.get(2, TimeUnit.SECONDS) } catch (_: Exception) {}
        closed = true
        worker.shutdownNow()
    }

    private fun release() {
        timeout?.cancel(false)
        ops.clear()
        val g = gatt ?: return
        gatt = null
        try { g.disconnect() } catch (_: Throwable) {}
        try { g.close() } catch (_: Throwable) {}
    }

    private fun down(why: String) {
        release()
        up = false
        if (!downReported) {
            downReported = true
            sink.onState(false, why)
        }
    }

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) = post {
            if (g !== gatt) return@post
            if (newState == BluetoothProfile.STATE_CONNECTED && status == BluetoothGatt.GATT_SUCCESS) {
                ops.enqueue("mtu", 3_000) { if (g.requestMtu(cfg.mtu)) GattOpQueue.Start.STARTED else GattOpQueue.Start.FAILED }
                ops.enqueue("discover", 10_000) { if (g.discoverServices()) GattOpQueue.Start.STARTED else GattOpQueue.Start.FAILED }
            } else {
                down(if (status == 133) "connection failed (GATT 133: out of range or busy)" else "disconnected ($status)")
            }
        }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) = post {
            if (g !== gatt) return@post
            if (status == BluetoothGatt.GATT_SUCCESS) payload = BleChunker.payloadFor(mtu)
            if (ops.currentName == "mtu") ops.complete()
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) = post {
            if (g !== gatt) return@post
            if (ops.currentName == "discover") ops.complete()
            val svc = g.getService(cfg.service)
            val r = svc?.getCharacteristic(cfg.rx)
            val t = svc?.getCharacteristic(cfg.tx)
            if (r == null || t == null) { down(if (cfg.meshtastic) "device has no Meshtastic BLE service" else "device has no UART service ${cfg.service}"); return@post }
            rx = r
            if (cfg.meshtastic) {
                fromRadio = svc.getCharacteristic(MeshtasticBle.FROM_RADIO) ?: run { down("Meshtastic service without FromRadio"); return@post }
            }
            writeType = if (!cfg.withResponse && r.properties and BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE != 0)
                BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE else BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            g.setCharacteristicNotification(t, true)
            val d = t.getDescriptor(BleUuids.CCCD)
            if (d == null) { ready(); return@post }
            val value = if (t.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY != 0)
                BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE else BluetoothGattDescriptor.ENABLE_INDICATION_VALUE
            ops.enqueue("cccd", 5_000) {
                try {
                    if (Build.VERSION.SDK_INT >= 33) {
                        when (g.writeDescriptor(d, value)) {
                            BluetoothStatusCodes.SUCCESS -> GattOpQueue.Start.STARTED
                            BluetoothStatusCodes.ERROR_GATT_WRITE_REQUEST_BUSY -> GattOpQueue.Start.BUSY
                            else -> GattOpQueue.Start.FAILED
                        }
                    } else {
                        @Suppress("DEPRECATION")
                        d.value = value
                        @Suppress("DEPRECATION")
                        if (g.writeDescriptor(d)) GattOpQueue.Start.STARTED else GattOpQueue.Start.BUSY
                    }
                } catch (_: Throwable) { GattOpQueue.Start.FAILED }
            }
        }

        override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) = post {
            if (g !== gatt) return@post
            if (ops.currentName == "cccd") ops.complete()
            if (status == BluetoothGatt.GATT_SUCCESS) ready() else down("subscribe failed ($status)")
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) = post {
            if (g === gatt && ops.currentName == "write") ops.complete()
            if (cfg.meshtastic) drain() // the device answers ToRadio through FromRadio
        }

        override fun onCharacteristicRead(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray, status: Int) =
            onRead(g, value, status)

        @Deprecated("pre-Android 13 callback")
        override fun onCharacteristicRead(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (Build.VERSION.SDK_INT >= 33) return
            @Suppress("DEPRECATION")
            onRead(g, characteristic.value ?: ByteArray(0), status)
        }

        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            if (cfg.meshtastic) { if (characteristic.uuid == MeshtasticBle.FROM_NUM) post { drain() }; return }
            if (characteristic.uuid == cfg.tx && value.isNotEmpty()) sink.onBytes(value.copyOf())
        }

        @Deprecated("pre-Android 13 callback")
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (Build.VERSION.SDK_INT >= 33) return
            @Suppress("DEPRECATION")
            val v = characteristic.value ?: return
            if (cfg.meshtastic) { if (characteristic.uuid == MeshtasticBle.FROM_NUM) post { drain() }; return }
            if (characteristic.uuid == cfg.tx && v.isNotEmpty()) sink.onBytes(v.copyOf())
        }
    }

    /** Meshtastic: one FromRadio read finished; an empty value means the device queue is drained. */
    private fun onRead(g: BluetoothGatt, value: ByteArray, status: Int) = post {
        if (g !== gatt) return@post
        if (ops.currentName == "read") ops.complete()
        if (status == BluetoothGatt.GATT_SUCCESS && value.isNotEmpty()) {
            sink.onBytes(MeshtasticBle.wrap(value))
            enqueueRead()
        } else draining = false
    }

    /** Meshtastic: read FromRadio until it returns empty (after FromNum notifies, a write, or connect). */
    private fun drain() {
        if (!cfg.meshtastic || !up || draining) return
        draining = true
        enqueueRead()
    }

    private fun enqueueRead() {
        ops.enqueue("read", 3_000) {
            val g = gatt; val f = fromRadio
            if (g == null || f == null) GattOpQueue.Start.FAILED
            else try { if (g.readCharacteristic(f)) GattOpQueue.Start.STARTED else GattOpQueue.Start.BUSY } catch (_: Throwable) { GattOpQueue.Start.FAILED }
        }
    }

    private fun ready() {
        timeout?.cancel(false)
        up = true
        sink.onState(true)
        if (cfg.meshtastic) drain()
        sink.onStatus(JSONObject().put("address", cfg.address).put("mtu", payload + 3).put("profile", cfg.profile)
            .put("write", if (writeType == BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE) "no_response" else "with_response"))
    }

    override fun write(data: ByteArray) {
        if (closed || data.isEmpty()) return
        val copy = data.copyOf()
        post {
            if (!up) return@post
            if (ops.size > 1024) { log("backlog full; dropping ${copy.size} bytes"); return@post }
            // Meshtastic: one protobuf per ToRadio write (long write if larger than the MTU allows).
            val pieces = if (cfg.meshtastic) unframer.feed(copy) else BleChunker.split(copy, payload)
            for (chunk in pieces) ops.enqueue("write", if (writeType == BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) 3_000 else 1_000) {
                writeChunk(chunk)
            }
        }
    }

    private fun writeChunk(chunk: ByteArray): GattOpQueue.Start {
        val g = gatt ?: return GattOpQueue.Start.FAILED
        val r = rx ?: return GattOpQueue.Start.FAILED
        return try {
            if (Build.VERSION.SDK_INT >= 33) {
                when (g.writeCharacteristic(r, chunk, writeType)) {
                    BluetoothStatusCodes.SUCCESS -> GattOpQueue.Start.STARTED
                    BluetoothStatusCodes.ERROR_GATT_WRITE_REQUEST_BUSY -> GattOpQueue.Start.BUSY
                    else -> GattOpQueue.Start.FAILED
                }
            } else {
                r.writeType = writeType
                @Suppress("DEPRECATION")
                r.value = chunk
                @Suppress("DEPRECATION")
                if (g.writeCharacteristic(r)) GattOpQueue.Start.STARTED else GattOpQueue.Start.BUSY
            }
        } catch (_: Throwable) { GattOpQueue.Start.FAILED }
    }

    private fun log(m: String) {
        sink.log(m)
        try { Log.d("EmberBleUart", m) } catch (_: Throwable) {}
    }

    object Factory : EndpointFactory {
        override val kind = "ble"
        override val title = "Bluetooth LE serial device"
        override val description = "BLE TNCs, LoRa boards and serial bridges with a Nordic-UART style service, or a Meshtastic radio over Bluetooth."
        override val scannable = true
        override val linkTypes = listOf("kiss", "stream", "rnode", "lora_at", "meshtastic")

        override fun configSchema(ctx: HostContext): JSONArray = Fields.of {
            text("address", "Device address", help = "Pick the device with Scan", required = true)
            select("profile", "Device type", listOf("uart" to "Serial (Nordic UART / custom UUIDs): TNCs, RNode, LoRa bridges",
                "kiss_ble" to "Mobilinkd TNC3/TNC4 (BLE KISS API)",
                "meshtastic" to "Meshtastic radio (Meshtastic Bluetooth API)"), default = "uart",
                help = "Meshtastic radios use their own Bluetooth service; pick it for a `meshtastic` link (keep its framing at \"stream\").")
            text("service_uuid", "Service UUID", default = BleUuids.NUS_SERVICE.toString(), advanced = true)
            text("rx_uuid", "RX characteristic (we write)", default = BleUuids.NUS_RX.toString(), advanced = true)
            text("tx_uuid", "TX characteristic (device notifies)", default = BleUuids.NUS_TX.toString(), advanced = true)
            bool("write_with_response", "Write with response", default = false, advanced = true,
                help = "Slower but some devices drop unacknowledged writes")
            number("mtu", "Requested MTU", default = 247, min = 23, max = 517, step = 1, advanced = true)
        }

        override fun unavailableReason(ctx: HostContext) = BleSupport.unavailableReason(ctx)
        override fun permissions(ctx: HostContext, config: JSONObject) = BleSupport.centralPermissions()

        override fun scan(ctx: HostContext, timeoutMs: Long): JSONArray {
            val found = LinkedHashMap<String, JSONObject>()
            val nus = ParcelUuid(BleUuids.NUS_SERVICE)
            val mesh = ParcelUuid(MeshtasticBle.SERVICE)
            BleSupport.scanOnce(ctx, null, timeoutMs) { r ->
                val addr = r.device.address ?: return@scanOnce
                val isNus = r.scanRecord?.serviceUuids?.contains(nus) == true
                val isMesh = r.scanRecord?.serviceUuids?.contains(mesh) == true
                val isKiss = r.scanRecord?.serviceUuids?.contains(ParcelUuid(BleUartConfig.KISS_SERVICE)) == true
                val name = BleSupport.deviceName(r)
                if (name.isNullOrEmpty() && !isNus && !isMesh && !isKiss) return@scanOnce
                synchronized(found) {
                    val prev = found[addr]
                    val nusKnown = isNus || prev?.optBoolean("_nus") == true
                    found[addr] = JSONObject().put("id", addr).put("name", name ?: prev?.optString("name") ?: addr)
                        .put("detail", if (isKiss) "Bluetooth KISS TNC (Mobilinkd)" else if (isMesh) "Meshtastic radio" else if (nusKnown) "Nordic UART service" else "BLE device")
                        .put("rssi", r.rssi).put("_nus", nusKnown || isMesh || isKiss)
                        .put("config", JSONObject().put("address", addr).apply { if (isMesh) put("profile", "meshtastic") else if (isKiss) put("profile", "kiss_ble") })
                }
            }
            val list = synchronized(found) { found.values.toList() }
                .sortedWith(compareByDescending<JSONObject> { it.optBoolean("_nus") }.thenByDescending { it.optInt("rssi", -127) })
            list.forEach { it.remove("_nus") }
            return JSONArray(list)
        }

        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink): Endpoint = BleUartEndpoint(ctx, config, sink)
    }
}
