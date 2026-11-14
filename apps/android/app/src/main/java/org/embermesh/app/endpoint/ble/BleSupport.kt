package org.embermesh.app.endpoint.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.pm.PackageManager
import android.location.LocationManager
import android.os.Build
import android.provider.Settings
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/** Android BLE plumbing shared by the ble_peer and ble endpoints. */
@SuppressLint("MissingPermission")
object BleSupport {
    fun manager(ctx: HostContext): BluetoothManager? = ctx.app.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
    fun adapter(ctx: HostContext): BluetoothAdapter? = try { manager(ctx)?.adapter } catch (_: Throwable) { null }

    fun unavailableReason(ctx: HostContext): String? {
        if (!ctx.hasContext) return "no Android context"
        if (!ctx.app.packageManager.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) return "This device has no Bluetooth LE"
        if (adapter(ctx) == null) return "No Bluetooth adapter"
        return null
    }

    /** Permissions to scan and connect as a central. */
    fun centralPermissions(): List<String> =
        if (Build.VERSION.SDK_INT >= 31) listOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        else listOf(Manifest.permission.ACCESS_FINE_LOCATION)

    /** Central + peripheral (GATT server and advertising). */
    fun peerPermissions(): List<String> =
        if (Build.VERSION.SDK_INT >= 31) centralPermissions() + Manifest.permission.BLUETOOTH_ADVERTISE
        else centralPermissions()

    fun isEnabled(ctx: HostContext): Boolean = try { adapter(ctx)?.isEnabled == true } catch (_: Throwable) { false }

    /** Android 6–11 deliver no scan results while system Location is switched off. */
    fun locationServicesBlockScan(ctx: HostContext): Boolean {
        if (Build.VERSION.SDK_INT < 23 || Build.VERSION.SDK_INT >= 31) return false
        return try {
            if (Build.VERSION.SDK_INT >= 28) {
                !(ctx.app.getSystemService(Context.LOCATION_SERVICE) as LocationManager).isLocationEnabled
            } else {
                @Suppress("DEPRECATION")
                Settings.Secure.getInt(ctx.app.contentResolver, Settings.Secure.LOCATION_MODE, 0) == Settings.Secure.LOCATION_MODE_OFF
            }
        } catch (_: Throwable) { false }
    }

    const val LOCATION_OFF_MSG = "Turn on Location in system settings: Android 6-11 needs it for Bluetooth scanning"

    fun scanFailure(code: Int): String = when (code) {
        ScanCallback.SCAN_FAILED_ALREADY_STARTED -> "scan already started"
        ScanCallback.SCAN_FAILED_APPLICATION_REGISTRATION_FAILED -> "scanner registration failed (too many scans?)"
        ScanCallback.SCAN_FAILED_FEATURE_UNSUPPORTED -> "BLE scanning unsupported"
        ScanCallback.SCAN_FAILED_INTERNAL_ERROR -> "internal Bluetooth error"
        else -> "scan failed ($code)"
    }

    fun deviceName(r: ScanResult): String? =
        r.scanRecord?.deviceName ?: try { r.device.name } catch (_: SecurityException) { null }

    /**
     * Blocking one-off scan (host.endpoints.scan). [onResult] runs on a binder thread.
     */
    fun scanOnce(ctx: HostContext, filters: List<ScanFilter>?, timeoutMs: Long, onResult: (ScanResult) -> Unit) {
        val adapter = adapter(ctx) ?: throw HostError.unsupported("No Bluetooth adapter")
        if (!adapter.isEnabled) throw HostError(HostError.IO_ERROR, "Bluetooth is off")
        if (locationServicesBlockScan(ctx)) throw HostError(HostError.IO_ERROR, LOCATION_OFF_MSG)
        val scanner = adapter.bluetoothLeScanner ?: throw HostError(HostError.IO_ERROR, "Bluetooth scanner unavailable")
        if (!ScanBudget.global.tryAcquire(System.currentTimeMillis())) {
            throw HostError(HostError.BUSY, "Too many Bluetooth scans; try again in a few seconds")
        }
        val done = CountDownLatch(1)
        var failure: String? = null
        val cb = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) = onResult(result)
            override fun onBatchScanResults(results: MutableList<ScanResult>) = results.forEach(onResult)
            override fun onScanFailed(errorCode: Int) { failure = scanFailure(errorCode); done.countDown() }
        }
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        try {
            scanner.startScan(filters, settings, cb)
        } catch (e: SecurityException) {
            throw HostError(HostError.DENIED, "Bluetooth permission missing")
        }
        try {
            done.await(timeoutMs, TimeUnit.MILLISECONDS)
        } finally {
            try { scanner.stopScan(cb) } catch (_: Throwable) {}
        }
        failure?.let { throw HostError(HostError.IO_ERROR, it) }
    }
}
