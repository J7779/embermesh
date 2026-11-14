package org.embermesh.app.endpoint.ble

import org.json.JSONObject
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class MeshtasticBleTest {
    private fun b(vararg v: Int) = ByteArray(v.size) { v[it].toByte() }

    @Test fun wrapAddsStreamHeader() {
        assertArrayEquals(b(0x94, 0xC3, 0x00, 0x03, 1, 2, 3), MeshtasticBle.wrap(b(1, 2, 3)))
        assertEquals(0x01, MeshtasticBle.wrap(ByteArray(300))[2].toInt())
    }

    @Test fun unframerDropsWakeBytesAndSplitsFrames() {
        val u = MeshtasticBle.Unframer()
        val wake = ByteArray(32) { 0xC3.toByte() }
        val f1 = MeshtasticBle.wrap(b(0x18, 0x2a))
        val f2 = MeshtasticBle.wrap(ByteArray(200) { it.toByte() })
        val all = wake + f1 + f2
        // arbitrary chunking
        val out = ArrayList<ByteArray>()
        var i = 0
        for (n in listOf(5, 30, 1, 3, 2, 100, 1000)) { if (i >= all.size) break; val e = minOf(all.size, i + n); out += u.feed(all.copyOfRange(i, e)); i = e }
        assertEquals(2, out.size)
        assertArrayEquals(b(0x18, 0x2a), out[0])
        assertEquals(200, out[1].size)
        // garbage and a bogus oversized length resync
        assertTrue(u.feed(b(0x94, 0xC3, 0xFF, 0xFF, 0x00)).isEmpty())
        assertArrayEquals(b(7), u.feed(MeshtasticBle.wrap(b(7))).single())
    }

    @Test fun configProfile() {
        val c = BleUartConfig.from(JSONObject().put("address", "aa:bb:cc:dd:ee:ff").put("profile", "meshtastic"))
        assertTrue(c.meshtastic)
        assertEquals(MeshtasticBle.TO_RADIO, c.rx)
        assertEquals(MeshtasticBle.FROM_NUM, c.tx)
        assertEquals("uart", BleUartConfig.from(JSONObject().put("address", "AA:BB:CC:DD:EE:FF")).profile)
    }
}

class BleProfilesTest {
    @Test fun kissBleProfile() {
        val c = BleUartConfig.from(JSONObject().put("address", "AA:BB:CC:DD:EE:01").put("profile", "kiss_ble"))
        assertEquals(BleUartConfig.KISS_RX, c.rx)
        assertEquals(BleUartConfig.KISS_TX, c.tx)
        assertTrue(!c.meshtastic)
    }
}
