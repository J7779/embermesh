package org.embermesh.app.endpoint.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattServer
import android.bluetooth.BluetoothGattServerCallback
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.bluetooth.le.AdvertiseCallback
import android.bluetooth.le.AdvertiseData
import android.bluetooth.le.AdvertiseSettings
import android.bluetooth.le.BluetoothLeAdvertiser
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.ParcelUuid
import android.util.Log
import org.embermesh.app.host.Endpoint
import org.embermesh.app.host.EndpointFactory
import org.embermesh.app.host.EndpointSink
import org.embermesh.app.host.Fields
import org.embermesh.app.host.HostContext
import org.embermesh.app.util.bool
import org.embermesh.app.util.int
import org.embermesh.app.util.str
import org.json.JSONArray
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.security.SecureRandom
import java.util.ArrayDeque
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit

/**
 * `ble_peer`: phone<->phone mesh over Bluetooth LE with no infrastructure. Runs at the same time a
 * GATT server + advertiser (others connect to us) and a scanner/central (we connect to them), for
 * up to `max_peers` peers. Wire format: a plain byte stream (ARCHITECTURE §6) — central writes RX
 * (write-without-response), peripheral notifies TX, chunked at MTU-3.
 *
 * Every piece of state is owned by the single [worker] thread; GATT/scan callbacks hop onto it.
 */
@SuppressLint("MissingPermission")
class BlePeerEndpoint(private val ctx: HostContext, config: JSONObject, sink: EndpointSink) : Endpoint(config, sink) {
    private val maxPeers = config.int("max_peers", 6).coerceIn(1, 15)
    private val wantScan = config.bool("scan", true)
    private val wantAdvertise = config.bool("advertise", true)
    private val txPower = when (config.str("tx_power", "high")) {
        "low" -> AdvertiseSettings.ADVERTISE_TX_POWER_LOW
        "medium" -> AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM
        else -> AdvertiseSettings.ADVERTISE_TX_POWER_HIGH
    }

    private val worker = Executors.newSingleThreadScheduledExecutor(HostContext.named("ble-peer"))
    private val token: Long = SecureRandom().nextInt().toLong() and 0xffffffffL
    private val clock: () -> Long = System::currentTimeMillis
    private val arbiter = StreamArbiter(ARBITER_IDLE_MS, ARBITER_CAP, clock) { sink.onBytes(it) }
    private var arbiterTick: ScheduledFuture<*>? = null

    @Volatile private var closed = false
    private var radioUp = false
    private var reportedUp = false
    private var receiverRegistered = false

    // Peripheral role
    private var server: BluetoothGattServer? = null
    private var serverTx: BluetoothGattCharacteristic? = null
    private var advertiser: BluetoothLeAdvertiser? = null
    private var advertising = false
    private var advError: String? = null
    private var centralOnly = false
    private val served = HashMap<String, ServedPeer>()
    private val serverQueue = ArrayDeque<Pair<String, ByteArray>>()
    private var serverInFlight: String? = null
    private var serverTimeout: ScheduledFuture<*>? = null

    // Central role
    private var scanner: BluetoothLeScanner? = null
    private var scanning = false
    private var scanMode = -1
    private var scanError: String? = null
    private var scanRetry: ScheduledFuture<*>? = null
    private val clients = HashMap<String, ClientPeer>()
    private val known = HashMap<String, KnownPeer>()

    private var lastActivity = 0L
    private var housekeeping: ScheduledFuture<*>? = null

    private class ServedPeer(val device: BluetoothDevice) {
        var payload = 20
        var subscribed = false
        val prepared = ByteArrayOutputStream()
        val connectedAt = System.currentTimeMillis()
    }

    private inner class ClientPeer(val address: String) {
        var gatt: BluetoothGatt? = null
        val ops = GattOpQueue(worker)
        var payload = 20
        var rx: BluetoothGattCharacteristic? = null
        var writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        var ready = false
        var connectTimeout: ScheduledFuture<*>? = null
    }

    private class KnownPeer(val address: String) {
        var token: Long? = null
        var name: String? = null
        var rssi = 0
        var lastSeen = 0L
        var failures = 0
        var nextAttempt = 0L
    }

    // --- lifecycle ---------------------------------------------------------------------------------

    override fun open() {
        worker.execute {
            registerReceiver()
            if (BleSupport.isEnabled(ctx)) startRadio() else sink.onState(false, "Bluetooth is off")
        }
    }

    override fun close() {
        if (closed) return
        closed = true
        try {
            worker.submit { stopRadio(); unregisterReceiver() }.get(3, TimeUnit.SECONDS)
        } catch (_: Exception) {}
        worker.shutdownNow()
    }

    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(c: Context, i: Intent) {
            val state = i.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1)
            post {
                when (state) {
                    BluetoothAdapter.STATE_ON -> if (!radioUp) startRadio()
                    BluetoothAdapter.STATE_TURNING_OFF, BluetoothAdapter.STATE_OFF -> if (radioUp || reportedUp) {
                        stopRadio()
                        reportedUp = false
                        sink.onState(false, "Bluetooth is off")
                    }
                }
            }
        }
    }

    private fun registerReceiver() {
        try {
            val f = IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED)
            if (Build.VERSION.SDK_INT >= 33) ctx.app.registerReceiver(receiver, f, Context.RECEIVER_NOT_EXPORTED)
            else ctx.app.registerReceiver(receiver, f)
            receiverRegistered = true
        } catch (t: Throwable) { log("receiver: $t") }
    }

    private fun unregisterReceiver() {
        if (!receiverRegistered) return
        receiverRegistered = false
        try { ctx.app.unregisterReceiver(receiver) } catch (_: Throwable) {}
    }

    private fun post(block: () -> Unit) {
        if (closed) return
        try { worker.execute { if (!closed) try { block() } catch (t: Throwable) { log("error: $t") } } } catch (_: Exception) {}
    }

    private fun startRadio() {
        if (closed || radioUp) return
        val adapter = BleSupport.adapter(ctx)
        if (adapter == null || !adapter.isEnabled) {
            sink.onState(false, "Bluetooth is off")
            return
        }
        radioUp = true
        if (wantAdvertise) startServer(adapter)
        if (wantScan) startScan()
        housekeeping?.cancel(false)
        housekeeping = worker.scheduleWithFixedDelay({ post { housekeep() } }, HOUSEKEEP_MS, HOUSEKEEP_MS, TimeUnit.MILLISECONDS)
        if (server == null) reportUp() // central-only: up as soon as scanning
    }

    private fun reportUp() {
        if (reportedUp || closed) return
        reportedUp = true
        sink.onState(true)
        pushStatus()
    }

    private fun stopRadio() {
        radioUp = false
        housekeeping?.cancel(false)
        scanRetry?.cancel(false)
        arbiterTick?.cancel(false)
        stopScan()
        stopAdvertising()
        for (c in clients.values) closeClient(c)
        clients.clear()
        for (k in served.keys) arbiter.release("s:$k")
        served.clear()
        serverQueue.clear()
        serverInFlight = null
        serverTimeout?.cancel(false)
        try { server?.clearServices() } catch (_: Throwable) {}
        try { server?.close() } catch (_: Throwable) {}
        server = null
        serverTx = null
    }

    // --- peripheral role ---------------------------------------------------------------------------

    private fun startServer(adapter: BluetoothAdapter) {
        val mgr = BleSupport.manager(ctx) ?: return
        val s = try { mgr.openGattServer(ctx.app, serverCallback) } catch (t: Throwable) { log("openGattServer: $t"); null }
        if (s == null) {
            centralOnly = true
            advError = "GATT server unavailable"
            return
        }
        server = s
        val svc = BluetoothGattService(BleUuids.EMBER_SERVICE, BluetoothGattService.SERVICE_TYPE_PRIMARY)
        val rx = BluetoothGattCharacteristic(BleUuids.EMBER_RX,
            BluetoothGattCharacteristic.PROPERTY_WRITE or BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE,
            BluetoothGattCharacteristic.PERMISSION_WRITE)
        val tx = BluetoothGattCharacteristic(BleUuids.EMBER_TX,
            BluetoothGattCharacteristic.PROPERTY_NOTIFY or BluetoothGattCharacteristic.PROPERTY_READ,
            BluetoothGattCharacteristic.PERMISSION_READ)
        tx.addDescriptor(BluetoothGattDescriptor(BleUuids.CCCD,
            BluetoothGattDescriptor.PERMISSION_READ or BluetoothGattDescriptor.PERMISSION_WRITE))
        svc.addCharacteristic(rx)
        svc.addCharacteristic(tx)
        serverTx = tx
        if (!s.addService(svc)) {
            log("addService failed")
            centralOnly = true
            reportUp()
        }
        advertiser = if (adapter.isMultipleAdvertisementSupported) adapter.bluetoothLeAdvertiser else null
        if (advertiser == null) {
            centralOnly = true
            advError = "this phone cannot advertise (BLE peripheral mode unsupported); it can still connect to others"
        }
    }

    private fun startAdvertising(includeName: Boolean = false) {
        val adv = advertiser ?: return
        val settings = AdvertiseSettings.Builder()
            .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_BALANCED)
            .setConnectable(true)
            .setTimeout(0)
            .setTxPowerLevel(txPower)
            .build()
        // flags(3) + 128-bit uuid(18) + manufacturer token(8) = 29 of 31 bytes.
        val data = AdvertiseData.Builder()
            .addServiceUuid(ParcelUuid(BleUuids.EMBER_SERVICE))
            .addManufacturerData(BleUuids.TOKEN_COMPANY_ID, TieBreak.tokenBytes(token))
            .setIncludeDeviceName(false)
            .setIncludeTxPowerLevel(false)
            .build()
        // Privacy: never advertise the phone's Bluetooth name (often the owner's name); peers are
        // recognised by the service UUID and show up as "EmberMesh phone".
        val resp = scanResponse()
        try {
            adv.startAdvertising(settings, data, resp, object : AdvertiseCallback() {
                override fun onStartSuccess(settingsInEffect: AdvertiseSettings?) = post {
                    advertising = true
                    advCallback = this
                    advError = null
                    pushStatus()
                }

                override fun onStartFailure(errorCode: Int) = post {
                    advertising = false
                    if (errorCode == ADVERTISE_FAILED_DATA_TOO_LARGE && includeName) { startAdvertising(false); return@post }
                    if (errorCode == ADVERTISE_FAILED_ALREADY_STARTED) { advertising = true; return@post }
                    advError = "advertising failed ($errorCode)"
                    if (errorCode == ADVERTISE_FAILED_FEATURE_UNSUPPORTED) centralOnly = true
                    pushStatus()
                }
            }.also { advCallback = it })
        } catch (t: Throwable) {
            advError = "advertising failed: ${t.message}"
        }
    }

    private var advCallback: AdvertiseCallback? = null

    private fun stopAdvertising() {
        val cb = advCallback ?: return
        try { advertiser?.stopAdvertising(cb) } catch (_: Throwable) {}
        advCallback = null
        advertising = false
    }

    private val serverCallback = object : BluetoothGattServerCallback() {
        override fun onServiceAdded(status: Int, service: BluetoothGattService?) = post {
            if (status == BluetoothGatt.GATT_SUCCESS) startAdvertising() else { centralOnly = true; advError = "GATT service failed ($status)" }
            reportUp()
        }

        override fun onConnectionStateChange(device: BluetoothDevice, status: Int, newState: Int) = post {
            val addr = device.address
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                if (!served.containsKey(addr)) {
                    if (served.size + clients.size >= maxPeers + 2) {
                        try { server?.cancelConnection(device) } catch (_: Throwable) {}
                        return@post
                    }
                    served[addr] = ServedPeer(device)
                    markActive()
                    pushStatus()
                }
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                if (served.remove(addr) != null) {
                    serverQueue.removeAll { it.first == addr }
                    if (serverInFlight == addr) { serverInFlight = null; serverTimeout?.cancel(false); pumpServer() }
                    arbiter.release("s:$addr")
                    pushStatus()
                }
            }
        }

        override fun onMtuChanged(device: BluetoothDevice, mtu: Int) = post {
            served[device.address]?.payload = BleChunker.payloadFor(mtu)
        }

        override fun onCharacteristicWriteRequest(device: BluetoothDevice, requestId: Int, characteristic: BluetoothGattCharacteristic,
                                                  preparedWrite: Boolean, responseNeeded: Boolean, offset: Int, value: ByteArray?) {
            val s = server ?: return
            if (characteristic.uuid != BleUuids.EMBER_RX) {
                if (responseNeeded) respond(s, device, requestId, BluetoothGatt.GATT_WRITE_NOT_PERMITTED, offset, null)
                return
            }
            val v = value ?: ByteArray(0)
            if (responseNeeded) respond(s, device, requestId, BluetoothGatt.GATT_SUCCESS, offset, if (preparedWrite) v else null)
            post {
                val p = served[device.address] ?: ServedPeer(device).also { served[device.address] = it }
                if (preparedWrite) {
                    if (p.prepared.size() + v.size <= 64 * 1024) p.prepared.write(v)
                } else {
                    receive("s:${device.address}", v, p.payload)
                }
            }
        }

        override fun onExecuteWrite(device: BluetoothDevice, requestId: Int, execute: Boolean) {
            server?.let { respond(it, device, requestId, BluetoothGatt.GATT_SUCCESS, 0, null) }
            post {
                val p = served[device.address] ?: return@post
                val data = p.prepared.toByteArray()
                p.prepared.reset()
                if (execute && data.isNotEmpty()) receive("s:${device.address}", data, Int.MAX_VALUE)
            }
        }

        override fun onDescriptorWriteRequest(device: BluetoothDevice, requestId: Int, descriptor: BluetoothGattDescriptor,
                                              preparedWrite: Boolean, responseNeeded: Boolean, offset: Int, value: ByteArray?) {
            val s = server ?: return
            val ok = descriptor.uuid == BleUuids.CCCD
            if (responseNeeded) respond(s, device, requestId, if (ok) BluetoothGatt.GATT_SUCCESS else BluetoothGatt.GATT_WRITE_NOT_PERMITTED, offset, null)
            if (!ok) return
            val on = value != null && value.isNotEmpty() && (value[0].toInt() and 0x03) != 0
            post {
                val p = served[device.address] ?: ServedPeer(device).also { served[device.address] = it }
                p.subscribed = on
                markActive()
                pushStatus()
            }
        }

        override fun onDescriptorReadRequest(device: BluetoothDevice, requestId: Int, offset: Int, descriptor: BluetoothGattDescriptor) {
            val s = server ?: return
            val sub = served[device.address]?.subscribed == true // racy read is fine for a status value
            respond(s, device, requestId, BluetoothGatt.GATT_SUCCESS, 0,
                if (sub) BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE else BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE)
        }

        override fun onCharacteristicReadRequest(device: BluetoothDevice, requestId: Int, offset: Int, characteristic: BluetoothGattCharacteristic) {
            server?.let { respond(it, device, requestId, BluetoothGatt.GATT_SUCCESS, 0, ByteArray(0)) }
        }

        override fun onNotificationSent(device: BluetoothDevice, status: Int) = post {
            if (serverInFlight == device.address) {
                serverTimeout?.cancel(false)
                serverInFlight = null
                pumpServer()
            }
        }
    }

    private fun respond(s: BluetoothGattServer, d: BluetoothDevice, req: Int, status: Int, offset: Int, value: ByteArray?) {
        try { s.sendResponse(d, req, status, offset, value) } catch (t: Throwable) { log("sendResponse: $t") }
    }

    private fun pumpServer() {
        val s = server ?: return
        val tx = serverTx ?: return
        while (serverInFlight == null && serverQueue.isNotEmpty()) {
            val (addr, chunk) = serverQueue.removeFirst()
            val p = served[addr] ?: continue
            if (!p.subscribed) continue
            val ok = try {
                if (Build.VERSION.SDK_INT >= 33) {
                    s.notifyCharacteristicChanged(p.device, tx, false, chunk) == BluetoothStatusCodes.SUCCESS
                } else {
                    @Suppress("DEPRECATION")
                    tx.value = chunk
                    @Suppress("DEPRECATION")
                    s.notifyCharacteristicChanged(p.device, tx, false)
                }
            } catch (t: Throwable) { false }
            if (ok) {
                serverInFlight = addr
                serverTimeout = worker.schedule({ post { if (serverInFlight == addr) { serverInFlight = null; pumpServer() } } },
                    NOTIFY_TIMEOUT_MS, TimeUnit.MILLISECONDS)
            }
        }
    }

    // --- central role ------------------------------------------------------------------------------

    private fun recentlyActive() = clock() - lastActivity < ACTIVE_WINDOW_MS

    private fun startScan() {
        if (!radioUp || closed || !wantScan) return
        val adapter = BleSupport.adapter(ctx) ?: return
        val sc = try { adapter.bluetoothLeScanner } catch (_: Throwable) { null }
        if (sc == null) { scanError = "Bluetooth scanner unavailable"; return }
        scanner = sc
        val mode = if (recentlyActive()) ScanSettings.SCAN_MODE_LOW_LATENCY else ScanSettings.SCAN_MODE_BALANCED
        if (scanning && mode == scanMode) return
        val now = clock()
        if (!ScanBudget.global.tryAcquire(now)) {
            scheduleScanRetry(ScanBudget.global.nextAllowedAt(now) - now + 100)
            return
        }
        if (scanning) stopScan()
        scanError = if (BleSupport.locationServicesBlockScan(ctx)) BleSupport.LOCATION_OFF_MSG else null
        val filters = listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(BleUuids.EMBER_SERVICE)).build())
        val settings = ScanSettings.Builder().setScanMode(mode).apply {
            if (Build.VERSION.SDK_INT >= 23) {
                setCallbackType(ScanSettings.CALLBACK_TYPE_ALL_MATCHES)
                setMatchMode(ScanSettings.MATCH_MODE_AGGRESSIVE)
            }
        }.build()
        try {
            sc.startScan(filters, settings, scanCallback)
            scanning = true
            scanMode = mode
        } catch (t: Throwable) {
            scanError = "scan failed: ${t.message}"
            scheduleScanRetry(10_000)
        }
        pushStatus()
    }

    private fun scheduleScanRetry(delayMs: Long) {
        scanRetry?.cancel(false)
        scanRetry = try { worker.schedule({ post { startScan() } }, delayMs.coerceIn(100, 60_000), TimeUnit.MILLISECONDS) } catch (_: Exception) { null }
    }

    private fun stopScan() {
        if (!scanning) return
        try { scanner?.stopScan(scanCallback) } catch (_: Throwable) {}
        scanning = false
        scanMode = -1
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) = post { onPeerSeen(result) }
        override fun onBatchScanResults(results: MutableList<ScanResult>) = post { results.forEach { onPeerSeen(it) } }
        override fun onScanFailed(errorCode: Int) = post {
            scanning = false
            scanMode = -1
            scanError = BleSupport.scanFailure(errorCode)
            pushStatus()
            scheduleScanRetry(15_000)
        }
    }

    private fun onPeerSeen(r: ScanResult) {
        val addr = r.device.address ?: return
        val now = clock()
        val k = known.getOrPut(addr) { KnownPeer(addr) }
        k.lastSeen = now
        k.rssi = r.rssi
        k.token = TieBreak.parseToken(r.scanRecord?.getManufacturerSpecificData(BleUuids.TOKEN_COMPANY_ID)) ?: k.token
        BleSupport.deviceName(r)?.let { k.name = it }
        if (clients.containsKey(addr) || served.containsKey(addr)) return
        if (clients.size + served.size >= maxPeers) return
        if (k.token == token) return // our own advert echoed by a multi-role stack
        if (!TieBreak.shouldInitiate(token, k.token, advertising)) return
        if (now < k.nextAttempt) return
        connect(r.device, k)
    }

    private fun connect(device: BluetoothDevice, k: KnownPeer) {
        val c = ClientPeer(device.address)
        clients[device.address] = c
        val cb = ClientCallback(c)
        val g = try {
            if (Build.VERSION.SDK_INT >= 23) device.connectGatt(ctx.app, false, cb, BluetoothDevice.TRANSPORT_LE)
            else device.connectGatt(ctx.app, false, cb)
        } catch (t: Throwable) { log("connectGatt: $t"); null }
        if (g == null) {
            clients.remove(device.address)
            failed(k, false)
            return
        }
        c.gatt = g
        c.connectTimeout = worker.schedule({ post { if (clients[c.address] === c && !c.ready) dropClient(c, "connect timeout") } },
            CONNECT_TIMEOUT_MS, TimeUnit.MILLISECONDS)
    }

    private inner class ClientCallback(val c: ClientPeer) : BluetoothGattCallback() {
        private fun live(g: BluetoothGatt) = clients[c.address] === c && c.gatt === g

        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) = post {
            if (!live(gatt)) { try { gatt.close() } catch (_: Throwable) {}; return@post }
            if (newState == BluetoothProfile.STATE_CONNECTED && status == BluetoothGatt.GATT_SUCCESS) {
                c.ops.enqueue("mtu", 3_000) { if (gatt.requestMtu(517)) GattOpQueue.Start.STARTED else GattOpQueue.Start.FAILED }
                c.ops.enqueue("discover", 10_000) { if (gatt.discoverServices()) GattOpQueue.Start.STARTED else GattOpQueue.Start.FAILED }
            } else {
                dropClient(c, "disconnected ($status)")
            }
        }

        override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) = post {
            if (!live(gatt)) return@post
            if (status == BluetoothGatt.GATT_SUCCESS) c.payload = BleChunker.payloadFor(mtu)
            if (c.ops.currentName == "mtu") c.ops.complete()
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) = post {
            if (!live(gatt)) return@post
            if (c.ops.currentName == "discover") c.ops.complete()
            val svc = gatt.getService(BleUuids.EMBER_SERVICE)
            val rx = svc?.getCharacteristic(BleUuids.EMBER_RX)
            val tx = svc?.getCharacteristic(BleUuids.EMBER_TX)
            if (status != BluetoothGatt.GATT_SUCCESS || rx == null || tx == null) {
                dropClient(c, "no Ember service")
                return@post
            }
            c.rx = rx
            c.writeType = if (rx.properties and BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE != 0)
                BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE else BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            gatt.setCharacteristicNotification(tx, true)
            val d = tx.getDescriptor(BleUuids.CCCD)
            if (d == null) { setReady(c); return@post }
            c.ops.enqueue("cccd", 5_000) { writeDescriptor(gatt, d, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) }
        }

        override fun onDescriptorWrite(gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) = post {
            if (!live(gatt)) return@post
            if (c.ops.currentName == "cccd") c.ops.complete()
            if (status == BluetoothGatt.GATT_SUCCESS) setReady(c) else dropClient(c, "subscribe failed ($status)")
        }

        override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) = post {
            if (live(gatt) && c.ops.currentName == "write") c.ops.complete()
        }

        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            val v = value.copyOf()
            post { if (live(gatt) && characteristic.uuid == BleUuids.EMBER_TX) receive("c:${c.address}", v, c.payload) }
        }

        @Deprecated("pre-Android 13 callback")
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (Build.VERSION.SDK_INT >= 33) return
            @Suppress("DEPRECATION")
            val v = characteristic.value?.copyOf() ?: return
            post { if (live(gatt) && characteristic.uuid == BleUuids.EMBER_TX) receive("c:${c.address}", v, c.payload) }
        }
    }

    private fun writeDescriptor(g: BluetoothGatt, d: BluetoothGattDescriptor, v: ByteArray): GattOpQueue.Start = try {
        if (Build.VERSION.SDK_INT >= 33) {
            when (g.writeDescriptor(d, v)) {
                BluetoothStatusCodes.SUCCESS -> GattOpQueue.Start.STARTED
                BluetoothStatusCodes.ERROR_GATT_WRITE_REQUEST_BUSY -> GattOpQueue.Start.BUSY
                else -> GattOpQueue.Start.FAILED
            }
        } else {
            @Suppress("DEPRECATION")
            d.value = v
            @Suppress("DEPRECATION")
            if (g.writeDescriptor(d)) GattOpQueue.Start.STARTED else GattOpQueue.Start.BUSY
        }
    } catch (t: Throwable) { GattOpQueue.Start.FAILED }

    private fun setReady(c: ClientPeer) {
        c.ready = true
        c.connectTimeout?.cancel(false)
        known[c.address]?.let { it.failures = 0 }
        markActive()
        pushStatus()
    }

    private fun dropClient(c: ClientPeer, why: String) {
        if (clients[c.address] !== c) return
        clients.remove(c.address)
        val wasReady = c.ready
        closeClient(c)
        arbiter.release("c:${c.address}")
        log("peer ${c.address} dropped: $why")
        known[c.address]?.let { failed(it, wasReady) }
        pushStatus()
    }

    private fun failed(k: KnownPeer, wasReady: Boolean) {
        if (wasReady) k.failures = 0 else k.failures++
        k.nextAttempt = clock() + if (wasReady) 1_000 else bleBackoffMs(k.failures - 1)
    }

    private fun closeClient(c: ClientPeer) {
        c.connectTimeout?.cancel(false)
        c.ops.clear()
        c.ready = false
        val g = c.gatt ?: return
        c.gatt = null
        try { g.disconnect() } catch (_: Throwable) {}
        try { g.close() } catch (_: Throwable) {}
    }

    private fun writeChunk(c: ClientPeer, chunk: ByteArray): GattOpQueue.Start {
        val g = c.gatt ?: return GattOpQueue.Start.FAILED
        val rx = c.rx ?: return GattOpQueue.Start.FAILED
        return try {
            if (Build.VERSION.SDK_INT >= 33) {
                when (g.writeCharacteristic(rx, chunk, c.writeType)) {
                    BluetoothStatusCodes.SUCCESS -> GattOpQueue.Start.STARTED
                    BluetoothStatusCodes.ERROR_GATT_WRITE_REQUEST_BUSY -> GattOpQueue.Start.BUSY
                    else -> GattOpQueue.Start.FAILED
                }
            } else {
                rx.writeType = c.writeType
                @Suppress("DEPRECATION")
                rx.value = chunk
                @Suppress("DEPRECATION")
                if (g.writeCharacteristic(rx)) GattOpQueue.Start.STARTED else GattOpQueue.Start.BUSY
            }
        } catch (t: Throwable) { GattOpQueue.Start.FAILED }
    }

    // --- data path ---------------------------------------------------------------------------------

    override fun write(data: ByteArray) {
        if (closed || data.isEmpty()) return
        val copy = data.copyOf()
        post { send(copy) }
    }

    private fun send(data: ByteArray) {
        markActive()
        for (c in clients.values) {
            if (!c.ready) continue
            if (c.ops.size > MAX_QUEUED_OPS) { log("peer ${c.address} backlog full; dropping item"); continue }
            for (chunk in BleChunker.split(data, c.payload)) c.ops.enqueue("write", WRITE_TIMEOUT_MS) { writeChunk(c, chunk) }
        }
        for ((addr, p) in served) {
            if (!p.subscribed) continue
            if (serverQueue.size > MAX_QUEUED_OPS * 2) { log("notify backlog full; dropping item for $addr"); continue }
            for (chunk in BleChunker.split(data, p.payload)) serverQueue.addLast(addr to chunk)
        }
        pumpServer()
    }

    private fun receive(peer: String, data: ByteArray, payload: Int) {
        markActive()
        arbiter.offer(peer, data, payload)
        scheduleArbiterTick()
    }

    private fun scheduleArbiterTick() {
        if (!arbiter.needsTick || arbiterTick?.isDone == false) return
        arbiterTick = try {
            worker.schedule({ post { arbiter.tick(); scheduleArbiterTick() } }, ARBITER_IDLE_MS, TimeUnit.MILLISECONDS)
        } catch (_: Exception) { null }
    }

    private fun markActive() {
        val wasActive = recentlyActive()
        lastActivity = clock()
        if (!wasActive && scanning && scanMode != ScanSettings.SCAN_MODE_LOW_LATENCY) startScan()
    }

    private fun housekeep() {
        if (!radioUp) return
        val now = clock()
        known.values.removeAll { now - it.lastSeen > FORGET_MS && !clients.containsKey(it.address) }
        // Drop to balanced scanning when idle; restart a scan that died.
        if (wantScan) {
            val want = if (recentlyActive()) ScanSettings.SCAN_MODE_LOW_LATENCY else ScanSettings.SCAN_MODE_BALANCED
            if (!scanning || scanMode != want) startScan()
        }
        if (wantAdvertise && advertiser != null && !advertising && advCallback == null && server != null) startAdvertising()
        pushStatus()
    }

    private var lastStatus = ""

    private fun pushStatus() {
        val peers = JSONArray()
        for (c in clients.values) peers.put(JSONObject().put("id", c.address).put("role", "central").put("ready", c.ready)
            .put("mtu", c.payload + 3).put("rssi", known[c.address]?.rssi ?: 0).put("name", known[c.address]?.name ?: JSONObject.NULL))
        for ((a, p) in served) peers.put(JSONObject().put("id", a).put("role", "peripheral").put("ready", p.subscribed)
            .put("mtu", p.payload + 3))
        val st = JSONObject().put("peers", peers).put("peer_count", clients.values.count { it.ready } + served.values.count { it.subscribed })
            .put("advertising", advertising).put("scanning", scanning).put("central_only", centralOnly)
            .put("nearby", known.size)
        advError?.let { st.put("advertise_error", it) }
        scanError?.let { st.put("scan_error", it) }
        val s = st.toString()
        if (s != lastStatus) {
            lastStatus = s
            sink.onStatus(st)
        }
    }

    override fun status(): JSONObject = try { JSONObject(lastStatus) } catch (_: Exception) { JSONObject() }

    private fun log(m: String) {
        sink.log(m)
        try { Log.d(TAG, m) } catch (_: Throwable) {}
    }

    companion object {
        private const val TAG = "EmberBlePeer"
        const val ARBITER_IDLE_MS = 60L
        const val ARBITER_CAP = 64 * 1024
        private const val HOUSEKEEP_MS = 15_000L
        private const val ACTIVE_WINDOW_MS = 30_000L
        private const val FORGET_MS = 5 * 60_000L
        private const val CONNECT_TIMEOUT_MS = 20_000L
        private const val WRITE_TIMEOUT_MS = 1_000L
        private const val NOTIFY_TIMEOUT_MS = 1_000L
        private const val MAX_QUEUED_OPS = 512

        /** Scan response: deliberately empty — never the phone's Bluetooth name (privacy). */
        fun scanResponse(): AdvertiseData = AdvertiseData.Builder().setIncludeDeviceName(false).setIncludeTxPowerLevel(false).build()
    }

    object Factory : EndpointFactory {
        override val kind = "ble_peer"
        override val title = "Bluetooth LE (phone to phone)"
        override val description = "Mesh with nearby phones over Bluetooth LE. No Wi-Fi, SIM or pairing needed; range ~10–50 m."
        override val scannable = true
        override val linkTypes = listOf("stream")

        override fun configSchema(ctx: HostContext): JSONArray = Fields.of {
            number("max_peers", "Max simultaneous peers", default = 6, min = 1, max = 15, step = 1,
                help = "More peers relay more but cost battery")
            bool("advertise", "Let nearby phones connect to me", default = true, advanced = true)
            bool("scan", "Connect to nearby phones", default = true, advanced = true)
            select("tx_power", "Transmit power", listOf("high" to "High (best range)", "medium" to "Medium", "low" to "Low (save battery)"),
                default = "high", advanced = true)
        }

        override fun unavailableReason(ctx: HostContext) = BleSupport.unavailableReason(ctx)
        override fun permissions(ctx: HostContext, config: JSONObject) = BleSupport.peerPermissions()

        override fun scan(ctx: HostContext, timeoutMs: Long): JSONArray {
            val found = LinkedHashMap<String, JSONObject>()
            val filters = listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(BleUuids.EMBER_SERVICE)).build())
            BleSupport.scanOnce(ctx, filters, timeoutMs) { r ->
                val addr = r.device.address ?: return@scanOnce
                synchronized(found) {
                    found[addr] = JSONObject().put("id", addr).put("name", BleSupport.deviceName(r) ?: "EmberMesh phone")
                        .put("detail", "EmberMesh peer").put("rssi", r.rssi).put("config", JSONObject())
                }
            }
            return synchronized(found) { JSONArray(found.values.sortedByDescending { it.optInt("rssi", -127) }) }
        }

        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink): Endpoint = BlePeerEndpoint(ctx, config, sink)
    }
}
