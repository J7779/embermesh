package org.embermesh.app.endpoint.audio

import org.json.JSONArray
import org.json.JSONObject

/**
 * "Connect a radio" helper (pure, unit-tested): from the phone's audio devices and attached USB
 * serial adapters, suggest complete audio-endpoint configs with a PTT route and a core `audio` link
 * preset (RPC_API §6). Known interfaces:
 *   AIOC (All-In-One-Cable, USB 1209:7388): one composite USB device = sound card + CDC serial; PTT
 *     is keyed by DTR (with RTS low).
 *   Digirig Mobile: CM108-type sound card + CP2102 serial (10c4:ea60) behind a hub; PTT on RTS.
 *   Other USB sound card + one USB serial adapter: RTS (most radio interface cables).
 */
object RadioInterfaces {
    data class AudioDev(val id: Int, val type: String, val name: String)   // type: usb | wired | builtin | sco
    data class SerialDev(val id: String, val vid: Int, val pid: Int, val name: String)

    fun suggest(inputs: List<AudioDev>, outputs: List<AudioDev>, serials: List<SerialDev>): JSONArray {
        val out = JSONArray()
        fun item(id: String, name: String, detail: String, inp: AudioDev?, outp: AudioDev?, ptt: String, line: String?, preset: String) {
            val cfg = JSONObject().put("input_device", inp?.id?.toString() ?: "default").put("output_device", outp?.id?.toString() ?: "default")
                .put("ptt", ptt)
            if (line != null) cfg.put("ptt_line", line)
            out.put(JSONObject().put("id", id).put("name", name).put("detail", detail).put("config", cfg)
                .put("link", JSONObject().put("type", "audio").put("config", JSONObject().put("preset", preset))))
        }
        val aioc = serials.firstOrNull { it.vid == 0x1209 && it.pid == 0x7388 }
        val cp210x = serials.firstOrNull { it.vid == 0x10c4 && it.pid == 0xea60 }
        for (i in inputs.filter { it.type == "usb" }) {
            val o = outputs.firstOrNull { it.type == "usb" && it.name == i.name } ?: outputs.firstOrNull { it.type == "usb" }
            val isAioc = aioc != null && (i.name.contains("AIOC", true) || i.name.contains("All-In-One", true) || serials.size == 1)
            when {
                isAioc -> item("usb:${i.id}", "AIOC cable (${i.name})", "USB sound card + PTT on the same cable (DTR)", i, o, "usb:${aioc!!.id}", "dtr", "handheld_ptt")
                cp210x != null -> item("usb:${i.id}", "${i.name} + ${cp210x.name}", "USB sound card, PTT by the CP2102 serial port's RTS (Digirig and similar)", i, o, "usb:${cp210x.id}", "rts", "handheld_ptt")
                serials.isNotEmpty() -> item("usb:${i.id}", "${i.name} + ${serials[0].name}", "USB sound card, PTT by the serial adapter's RTS (change to DTR if it does not key)", i, o, "usb:${serials[0].id}", "rts", "handheld_ptt")
                else -> item("usb:${i.id}", i.name, "USB sound card; the radio keys itself with VOX", i, o, "vox", null, "handheld_vox")
            }
        }
        inputs.firstOrNull { it.type == "wired" }?.let { i ->
            val o = outputs.firstOrNull { it.type == "wired" }
            item("wired:${i.id}", "Headset-jack cable (${i.name})", "TRRS audio cable to the radio; the radio keys itself with VOX", i, o, "vox", null, "handheld_vox")
        }
        inputs.firstOrNull { it.type == "builtin" }?.let { i ->
            val o = outputs.firstOrNull { it.type == "builtin" }
            item("acoustic", "No cable: hold the phone to the radio", "Phone speaker to the radio's microphone, radio speaker to the phone's microphone; press PTT yourself", i, o, "vox", null, "acoustic")
        }
        return out
    }
}
