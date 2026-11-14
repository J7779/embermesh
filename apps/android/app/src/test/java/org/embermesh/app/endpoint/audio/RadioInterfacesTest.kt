package org.embermesh.app.endpoint.audio

import org.embermesh.app.endpoint.audio.RadioInterfaces.AudioDev
import org.embermesh.app.endpoint.audio.RadioInterfaces.SerialDev
import org.junit.Assert.assertEquals
import org.junit.Test

class RadioInterfacesTest {
    private val builtinIn = AudioDev(1, "builtin", "mic")
    private val builtinOut = AudioDev(2, "builtin", "speaker")

    @Test fun aiocCompositeGetsDtrPttOnSameDevice() {
        val r = RadioInterfaces.suggest(listOf(AudioDev(10, "usb", "All-In-One-Cable"), builtinIn), listOf(AudioDev(11, "usb", "All-In-One-Cable"), builtinOut),
            listOf(SerialDev("1209:7388:A1", 0x1209, 0x7388, "All-In-One-Cable")))
        val a = r.getJSONObject(0)
        assertEquals("usb:1209:7388:A1", a.getJSONObject("config").getString("ptt"))
        assertEquals("dtr", a.getJSONObject("config").getString("ptt_line"))
        assertEquals("10", a.getJSONObject("config").getString("input_device"))
        assertEquals("11", a.getJSONObject("config").getString("output_device"))
        assertEquals("handheld_ptt", a.getJSONObject("link").getJSONObject("config").getString("preset"))
        assertEquals("acoustic", r.getJSONObject(1).getJSONObject("link").getJSONObject("config").getString("preset"))
    }

    @Test fun digirigUsesRtsAndWiredUsesVox() {
        val r = RadioInterfaces.suggest(listOf(AudioDev(10, "usb", "USB Audio Device"), AudioDev(20, "wired", "Headset")),
            listOf(AudioDev(11, "usb", "USB Audio Device"), AudioDev(21, "wired", "Headset")),
            listOf(SerialDev("10c4:ea60", 0x10c4, 0xea60, "CP2102"), SerialDev("0403:6001", 0x0403, 0x6001, "FT232R")))
        assertEquals("rts", r.getJSONObject(0).getJSONObject("config").getString("ptt_line"))
        assertEquals("usb:10c4:ea60", r.getJSONObject(0).getJSONObject("config").getString("ptt"))
        assertEquals("vox", r.getJSONObject(1).getJSONObject("config").getString("ptt"))
        assertEquals("handheld_vox", r.getJSONObject(1).getJSONObject("link").getJSONObject("config").getString("preset"))
    }

    @Test fun usbSoundCardWithoutSerialIsVox() {
        val r = RadioInterfaces.suggest(listOf(AudioDev(10, "usb", "CM108")), listOf(AudioDev(11, "usb", "CM108")), emptyList())
        assertEquals("vox", r.getJSONObject(0).getJSONObject("config").getString("ptt"))
    }
}
