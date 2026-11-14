package org.embermesh.app

import android.os.Build
import android.provider.Settings
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.endpoint.ble.BlePeerEndpoint
import org.embermesh.app.endpoint.net.Nsd
import org.json.JSONObject
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Nothing the app shows to other devices by default carries the phone's model, device name or Bluetooth name. */
@RunWith(AndroidJUnit4::class)
class PrivacyDefaultsTest {
    private val app get() = InstrumentationRegistry.getInstrumentation().targetContext

    private fun identifying(): List<String> {
        val out = ArrayList<String>()
        Build.MODEL?.takeIf { it.length >= 3 }?.let { out.add(it) }
        try { Settings.Global.getString(app.contentResolver, "device_name") } catch (_: Exception) { null }?.takeIf { it.length >= 3 }?.let { out.add(it) }
        try {
            @Suppress("MissingPermission")
            (app.getSystemService(android.content.Context.BLUETOOTH_SERVICE) as android.bluetooth.BluetoothManager).adapter?.name
        } catch (_: Throwable) { null }?.takeIf { it.length >= 3 }?.let { out.add(it) }
        return out
    }

    private fun assertAnonymous(what: String, value: String) {
        for (id in identifying()) assertFalse("$what '$value' contains '$id'", value.contains(id, ignoreCase = true))
    }

    @Test fun defaultsAreAnonymous() {
        assertTrue(EmberRuntime.start(app))
        val info = JSONObject(EmberRuntime.core.call("node.info", "{}")).getJSONObject("result")
        assertAnonymous("core device_name", info.optString("device_name"))
        if (info.optString("name").isEmpty()) {
            val probe = EmberRuntime.hubProbe()
            assertAnonymous("hub name", probe.getString("name"))
            assertTrue(probe.getString("name").startsWith("EmberMesh hub"))
        }
        repeat(5) { assertAnonymous("NSD service name", Nsd.anonymousName()) }
        assertFalse("BLE scan response must not include the device name", BlePeerEndpoint.scanResponse().includeDeviceName)
    }
}
