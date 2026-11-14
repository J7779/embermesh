package org.embermesh.app.endpoint.audio

import android.Manifest
import android.annotation.SuppressLint
import android.content.Context
import android.media.AudioAttributes
import android.media.AudioDeviceInfo
import android.media.AudioFocusRequest
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioRecord
import android.media.AudioTrack
import android.media.MediaRecorder
import android.media.audiofx.AcousticEchoCanceler
import android.media.audiofx.AutomaticGainControl
import android.media.audiofx.NoiseSuppressor
import android.os.Build
import android.os.Process
import org.embermesh.app.host.Endpoint
import org.embermesh.app.host.EndpointFactory
import org.embermesh.app.host.EndpointSink
import org.embermesh.app.host.Fields
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.embermesh.app.host.PttTarget
import org.embermesh.app.endpoint.serial.UsbPttLine
import org.embermesh.app.endpoint.serial.UsbSerialEndpoint
import org.embermesh.app.util.int
import org.embermesh.app.util.str
import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.abs
import kotlin.math.log10

/** Mono 16-bit PCM capture (AudioRecord, or a virtual cable in tests). */
interface PcmIn {
    val sampleRate: Int
    fun start() {}
    /** Blocks until up to [n] samples are read; <0 on error. */
    fun read(buf: ShortArray, n: Int): Int
    fun close()
}

/** Mono 16-bit PCM playback (AudioTrack, or a virtual cable in tests). */
interface PcmOut {
    val sampleRate: Int
    /** Time from write() to the sample leaving the speaker / jack. */
    val latencyMs: Long
    fun play()
    fun pause()
    /** Blocks while the buffer is full (paces the TX loop at the sample rate). */
    fun write(buf: ShortArray, n: Int): Int
    fun close()
}

/**
 * Soft-modem audio pipe: captures unprocessed mono PCM (no AGC / noise suppression / echo
 * cancellation, which destroy modem tones) and plays what the core's audio link driver produces.
 * Works through the headset jack (radio audio cable), USB audio interfaces, Bluetooth SCO headsets
 * and plain speaker → microphone acoustic coupling.
 */
class AudioEndpoint(
    private val ctx: HostContext,
    config: JSONObject,
    sink: EndpointSink,
    /** Test hook: replaces AudioRecord/AudioTrack with other PCM pipes (the loops stay the same). */
    private val ioOverride: (() -> Pair<PcmIn, PcmOut>)? = null,
) : Endpoint(config, sink) {
    private val am: AudioManager? = if (ioOverride == null) ctx.app.getSystemService(Context.AUDIO_SERVICE) as AudioManager else null
    private val ratePref = config.str("sample_rate", "auto")
    private val sourcePref = config.str("source", "auto")
    private val inputId = config.str("input_device", "default")
    private val outputId = config.str("output_device", "default")
    @Volatile private var txGain = gainOf(config)
    @Volatile private var pttCfg: JSONObject = config

    @Volatile private var running = false
    @Volatile private var muted = false
    private var pcmIn: PcmIn? = null
    private var pcmOut: PcmOut? = null
    private var rxThread: Thread? = null
    private var txThread: Thread? = null
    private var focusRequest: Any? = null
    private val focusListener = AudioManager.OnAudioFocusChangeListener { change ->
        muted = change == AudioManager.AUDIOFOCUS_LOSS || change == AudioManager.AUDIOFOCUS_LOSS_TRANSIENT
        sink.onStatus(status())
    }
    private var usingSco = false
    @Volatile private var rate = 0
    @Volatile private var sourceName = ""
    @Volatile private var rxLevelDb = -120.0
    @Volatile private var clipping = false
    @Volatile private var transmitting = false
    @Volatile private var pttOn = false
    @Volatile private var pttError: String? = null
    private var usbPtt: UsbPttLine? = null
    /** PTT keying is ordered and off the audio threads. */
    private val pttExec = java.util.concurrent.Executors.newSingleThreadExecutor(HostContext.named("ember-ptt"))
    private val effects = ArrayList<android.media.audiofx.AudioEffect>()
    @Volatile var samplesIn = 0L
        private set
    @Volatile var samplesOut = 0L
        private set

    override fun open() {
        running = true
        ctx.io.execute {
            try {
                start()
                sink.onState(true)
                sink.onStatus(status())
            } catch (t: Throwable) {
                stopAll()
                sink.onState(false, t.message ?: t.toString())
            }
        }
    }

    private fun start() {
        val (inp, out) = ioOverride?.invoke() ?: openHardware()
        pcmIn = inp
        pcmOut = out
        rate = inp.sampleRate
        if (ioOverride == null) sourceName = sourceName.ifEmpty { "hardware" } else sourceName = "virtual"
        inp.start()
        rxThread = Thread({ guarded("input") { rxLoop(inp) } }, "ember-audio-rx").apply { isDaemon = true; start() }
        txThread = Thread({ guarded("output") { txLoop(out) } }, "ember-audio-tx").apply { isDaemon = true; start() }
    }

    /** An audio thread must never take the process down: close() interrupts them, devices can throw. */
    private inline fun guarded(what: String, body: () -> Unit) {
        try { body() } catch (_: InterruptedException) {
        } catch (t: Throwable) {
            if (running) { running = false; sink.onState(false, "audio $what failed: ${t.message ?: t}") }
        }
    }

    @SuppressLint("MissingPermission")
    private fun openHardware(): Pair<PcmIn, PcmOut> {
        val am = this.am!!
        val inDev = findDevice(inputId, AudioManager.GET_DEVICES_INPUTS)
        val outDev = findDevice(outputId, AudioManager.GET_DEVICES_OUTPUTS)
        val sco = isSco(inDev) || isSco(outDev)
        if (sco) startSco()

        val nativeRate = am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toIntOrNull()
        val sources = sourceCandidates(sourcePref, unprocessedSupported(), sco)
        var rec: AudioRecord? = null
        loop@ for (r in rateCandidates(ratePref, nativeRate, sco)) {
            for (src in sources) {
                val min = AudioRecord.getMinBufferSize(r, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT)
                if (min <= 0) continue
                val a = try {
                    AudioRecord(src, r, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT, maxOf(min * 2, r / 5 * 2))
                } catch (_: Exception) { null } ?: continue
                if (a.state == AudioRecord.STATE_INITIALIZED) { rec = a; sourceName = sourceLabel(src); break@loop }
                a.release()
            }
        }
        val record = rec ?: throw HostError(HostError.IO_ERROR, "no usable microphone / audio input")
        val r = record.sampleRate // the rate the hardware actually delivers
        if (Build.VERSION.SDK_INT >= 23 && inDev != null) record.preferredDevice = inDev
        disableEffects(record.audioSessionId)

        val usage = if (sco) AudioAttributes.USAGE_VOICE_COMMUNICATION else AudioAttributes.USAGE_MEDIA
        val attrs = AudioAttributes.Builder().setUsage(usage).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build()
        val fmt = AudioFormat.Builder().setSampleRate(r).setEncoding(AudioFormat.ENCODING_PCM_16BIT).setChannelMask(AudioFormat.CHANNEL_OUT_MONO).build()
        val minOut = AudioTrack.getMinBufferSize(r, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT).coerceAtLeast(1024)
        val t = if (Build.VERSION.SDK_INT >= 23) {
            val b = AudioTrack.Builder().setAudioAttributes(attrs).setAudioFormat(fmt).setBufferSizeInBytes(minOut * 2)
                .setTransferMode(AudioTrack.MODE_STREAM)
            if (Build.VERSION.SDK_INT >= 26) b.setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)
            b.build()
        } else {
            AudioTrack(attrs, fmt, minOut * 2, AudioTrack.MODE_STREAM, AudioManager.AUDIO_SESSION_ID_GENERATE)
        }
        if (t.state != AudioTrack.STATE_INITIALIZED) { t.release(); record.release(); throw HostError(HostError.IO_ERROR, "audio output unavailable") }
        if (Build.VERSION.SDK_INT >= 23 && outDev != null) t.preferredDevice = outDev
        requestFocus(attrs)
        return RecordIn(record) to TrackOut(t, r, minOut / 2)
    }

    /** AudioRecord as a [PcmIn]. */
    private class RecordIn(private val r: AudioRecord) : PcmIn {
        override val sampleRate = r.sampleRate
        override fun start() {
            r.startRecording()
            if (r.recordingState != AudioRecord.RECORDSTATE_RECORDING) throw HostError(HostError.BUSY, "microphone is in use by another app")
        }
        override fun read(buf: ShortArray, n: Int) = r.read(buf, 0, n)
        override fun close() { try { r.stop() } catch (_: Exception) {}; try { r.release() } catch (_: Exception) {} }
    }

    /** AudioTrack as a [PcmOut]. */
    private class TrackOut(private val t: AudioTrack, override val sampleRate: Int, private val bufferFrames: Int) : PcmOut {
        override val latencyMs: Long get() {
            val frames = if (Build.VERSION.SDK_INT >= 23) t.bufferSizeInFrames else bufferFrames
            return frames * 1000L / sampleRate.coerceAtLeast(1)
        }
        override fun play() = t.play()
        override fun pause() { t.pause(); t.flush() }
        override fun write(buf: ShortArray, n: Int) = t.write(buf, 0, n)
        override fun close() { try { t.pause(); t.flush(); t.release() } catch (_: Exception) {} }
    }

    private fun rxLoop(r: PcmIn) {
        Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO)
        val frame = (rate / 50).coerceAtLeast(160) // 20 ms
        val buf = ShortArray(frame)
        var peak = 0
        var count = 0
        while (running) {
            val n = try { r.read(buf, frame) } catch (t: Throwable) { if (running) { running = false; sink.onState(false, "audio input failed: ${t.message}") }; return }
            if (n < 0) { if (running) { running = false; sink.onState(false, "audio input error $n") }; return }
            if (n == 0) continue
            for (i in 0 until n) { val v = abs(buf[i].toInt()); if (v > peak) peak = v }
            count += n
            if (count >= rate / 5) { // 5x a second: a usable "set your radio volume" meter in host.link.status
                rxLevelDb = if (peak == 0) -120.0 else 20 * log10(peak / 32768.0)
                clipping = peak >= 32700
                peak = 0; count = 0
                sink.onStatus(status())
            }
            samplesIn += n
            sink.onAudio(buf, n, rate)
        }
    }

    private fun txLoop(t: PcmOut) {
        Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO)
        val frame = (rate / 100).coerceAtLeast(80) // 10 ms
        val buf = ShortArray(frame)
        var playing = false
        var idleFrames = 0
        while (running) {
            val n = sink.pullAudio(buf, frame, rate)
            if (n <= 0) {
                if (playing) {
                    // Keep writing silence briefly (flushes the tail), then pause to save power.
                    java.util.Arrays.fill(buf, 0)
                    t.write(buf, frame)
                    if (++idleFrames > 50) { t.pause(); playing = false; transmitting = false }
                } else {
                    try { Thread.sleep(10) } catch (_: InterruptedException) { return }
                }
                continue
            }
            idleFrames = 0
            if (!playing) { t.play(); playing = true; transmitting = true }
            val g = txGain
            if (muted) java.util.Arrays.fill(buf, 0, n, 0) else if (g != 1f) applyGain(buf, n, g)
            samplesOut += n
            t.write(buf, n)
        }
    }

    private fun requestFocus(attrs: AudioAttributes) {
        val am = this.am ?: return
        try {
            if (Build.VERSION.SDK_INT >= 26) {
                val req = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN).setAudioAttributes(attrs)
                    .setOnAudioFocusChangeListener(focusListener).setWillPauseWhenDucked(false).build()
                focusRequest = req
                am.requestAudioFocus(req)
            } else {
                @Suppress("DEPRECATION")
                am.requestAudioFocus(focusListener, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN)
            }
        } catch (_: Exception) {}
    }

    private fun abandonFocus() {
        val am = this.am ?: return
        try {
            if (Build.VERSION.SDK_INT >= 26) (focusRequest as? AudioFocusRequest)?.let { am.abandonAudioFocusRequest(it) }
            else @Suppress("DEPRECATION") am.abandonAudioFocus(focusListener)
        } catch (_: Exception) {}
    }

    private fun disableEffects(session: Int) {
        try { if (AutomaticGainControl.isAvailable()) AutomaticGainControl.create(session)?.let { it.enabled = false; effects.add(it) } } catch (_: Throwable) {}
        try { if (NoiseSuppressor.isAvailable()) NoiseSuppressor.create(session)?.let { it.enabled = false; effects.add(it) } } catch (_: Throwable) {}
        try { if (AcousticEchoCanceler.isAvailable()) AcousticEchoCanceler.create(session)?.let { it.enabled = false; effects.add(it) } } catch (_: Throwable) {}
    }

    @SuppressLint("MissingPermission")
    private fun startSco() {
        val am = this.am ?: return
        usingSco = true
        try {
            am.mode = AudioManager.MODE_IN_COMMUNICATION
            if (Build.VERSION.SDK_INT >= 31) {
                am.availableCommunicationDevices.firstOrNull { it.type == AudioDeviceInfo.TYPE_BLUETOOTH_SCO }?.let { am.setCommunicationDevice(it) }
            } else {
                @Suppress("DEPRECATION")
                am.startBluetoothSco()
                @Suppress("DEPRECATION")
                am.isBluetoothScoOn = true
            }
        } catch (_: Exception) {}
    }

    private fun stopSco() {
        val am = this.am ?: return
        if (!usingSco) return
        usingSco = false
        try {
            if (Build.VERSION.SDK_INT >= 31) am.clearCommunicationDevice()
            else {
                @Suppress("DEPRECATION")
                am.isBluetoothScoOn = false
                @Suppress("DEPRECATION")
                am.stopBluetoothSco()
            }
            am.mode = AudioManager.MODE_NORMAL
        } catch (_: Exception) {}
    }

    private fun findDevice(id: String, flag: Int): AudioDeviceInfo? {
        if (Build.VERSION.SDK_INT < 23 || id.isEmpty() || id == "default") return null
        return am?.getDevices(flag)?.firstOrNull { it.id.toString() == id }
    }

    private fun isSco(d: AudioDeviceInfo?) = Build.VERSION.SDK_INT >= 23 && d?.type == AudioDeviceInfo.TYPE_BLUETOOTH_SCO

    private fun unprocessedSupported(): Boolean =
        Build.VERSION.SDK_INT >= 24 && am?.getProperty(AudioManager.PROPERTY_SUPPORT_AUDIO_SOURCE_UNPROCESSED) == "true"

    private fun stopAll() {
        running = false
        rxThread?.interrupt(); txThread?.interrupt()
        pcmIn?.close(); pcmOut?.close()
        pcmIn = null; pcmOut = null
        effects.forEach { try { it.release() } catch (_: Throwable) {} }
        effects.clear()
        if (am != null) { abandonFocus(); stopSco() }
        val u = usbPtt
        usbPtt = null
        try { pttExec.execute { try { u?.close() } catch (_: Throwable) {} }; pttExec.shutdown() } catch (_: Exception) {}
    }

    override fun close() = stopAll()

    override fun write(data: ByteArray) { /* audio links exchange PCM through pullAudio, not bytes */ }

    /** VOX: nothing to key. A dedicated USB PTT adapter is keyed here; "link:<id>" is routed by LinkManager. */
    override fun setPtt(on: Boolean) {
        pttOn = on
        val target = PttTarget.parse(pttCfg)
        if (target !is PttTarget.Usb) return
        try {
            pttExec.execute {
                try {
                    val line = usbPtt ?: UsbPttLine(ctx, target.device, PttTarget.line(pttCfg), PttTarget.invert(pttCfg)).also { usbPtt = it }
                    line.key(on)
                    pttError = null
                } catch (t: Throwable) {
                    pttError = t.message ?: t.toString()
                    sink.log("PTT: $pttError")
                }
                sink.onStatus(status())
            }
        } catch (_: Exception) {}
    }

    /** Output buffer latency + a 40 ms margin: the core sends on:false when the last sample was handed out. */
    override fun pttReleaseDelayMs(): Long = (pcmOut?.latencyMs ?: 0L) + 40L

    /** TX level and PTT routing change live; anything else (devices, rate, source) needs a reopen. */
    override fun reconfigure(newConfig: JSONObject): Boolean {
        val live = setOf("tx_gain", "ptt", "ptt_line", "ptt_invert", "ptt_link_id")
        val keys = (config.keys().asSequence() + newConfig.keys().asSequence()).toSet()
        if (keys.any { it !in live && config.opt(it)?.toString() != newConfig.opt(it)?.toString() }) return false
        txGain = gainOf(newConfig)
        val oldUsb = usbPtt
        usbPtt = null
        pttCfg = newConfig
        try { pttExec.execute { try { oldUsb?.close() } catch (_: Throwable) {} } } catch (_: Exception) {}
        sink.onStatus(status())
        return true
    }

    override fun status(): JSONObject = JSONObject()
        .put("sample_rate", rate).put("source", sourceName).put("muted", muted).put("transmitting", transmitting)
        .put("rx_level_dbfs", Math.round(rxLevelDb)).put("rx_clipping", clipping).put("bluetooth_sco", usingSco)
        .put("ptt", pttCfg.str("ptt").ifEmpty { if (pttCfg.int("ptt_link_id", 0) > 0) "link:${pttCfg.int("ptt_link_id", 0)}" else "vox" })
        .put("ptt_on", pttOn).put("ptt_error", pttError ?: JSONObject.NULL)
        .put("output_latency_ms", pcmOut?.latencyMs ?: 0)

    companion object {
        fun gainOf(c: JSONObject): Float = c.int("tx_gain", 100).coerceIn(0, 400) / 100f

        val STANDARD_RATES = listOf(48000, 44100, 22050, 16000, 11025, 8000)

        /** Rates to try, best first (pure; unit-tested). */
        fun rateCandidates(pref: String, nativeRate: Int?, sco: Boolean): List<Int> {
            val fixed = pref.toIntOrNull()
            val base = if (sco) listOf(16000, 8000) else STANDARD_RATES
            val out = LinkedHashSet<Int>()
            if (fixed != null && fixed >= 8000) out.add(fixed)
            if (!sco && nativeRate != null && nativeRate >= 8000) out.add(nativeRate)
            out.addAll(base)
            return out.toList()
        }

        /** Capture sources to try, best first (pure; unit-tested). */
        fun sourceCandidates(pref: String, unprocessed: Boolean, sco: Boolean): List<Int> {
            val explicit = when (pref) {
                "unprocessed" -> if (Build.VERSION.SDK_INT >= 24) MediaRecorder.AudioSource.UNPROCESSED else MediaRecorder.AudioSource.VOICE_RECOGNITION
                "voice_recognition" -> MediaRecorder.AudioSource.VOICE_RECOGNITION
                "mic" -> MediaRecorder.AudioSource.MIC
                "camcorder" -> MediaRecorder.AudioSource.CAMCORDER
                else -> null
            }
            val out = LinkedHashSet<Int>()
            if (explicit != null) out.add(explicit)
            if (sco) out.add(MediaRecorder.AudioSource.VOICE_COMMUNICATION)
            if (unprocessed) out.add(UNPROCESSED)
            out.add(MediaRecorder.AudioSource.VOICE_RECOGNITION)
            out.add(MediaRecorder.AudioSource.MIC)
            return out.toList()
        }

        private const val UNPROCESSED = 9 // MediaRecorder.AudioSource.UNPROCESSED (API 24)

        fun sourceLabel(src: Int) = when (src) {
            UNPROCESSED -> "unprocessed"
            MediaRecorder.AudioSource.VOICE_RECOGNITION -> "voice_recognition"
            MediaRecorder.AudioSource.VOICE_COMMUNICATION -> "voice_communication"
            MediaRecorder.AudioSource.CAMCORDER -> "camcorder"
            MediaRecorder.AudioSource.MIC -> "mic"
            else -> src.toString()
        }

        fun applyGain(buf: ShortArray, n: Int, gain: Float) {
            for (i in 0 until n) buf[i] = (buf[i] * gain).toInt().coerceIn(-32768, 32767).toShort()
        }

        fun deviceTypeName(type: Int): String = when (type) {
            AudioDeviceInfo.TYPE_BUILTIN_MIC -> "Built-in microphone"
            AudioDeviceInfo.TYPE_BUILTIN_SPEAKER -> "Built-in speaker"
            AudioDeviceInfo.TYPE_BUILTIN_EARPIECE -> "Earpiece"
            AudioDeviceInfo.TYPE_WIRED_HEADSET -> "Wired headset (jack)"
            AudioDeviceInfo.TYPE_WIRED_HEADPHONES -> "Wired headphones (jack)"
            AudioDeviceInfo.TYPE_USB_DEVICE -> "USB audio"
            AudioDeviceInfo.TYPE_USB_HEADSET -> "USB headset"
            AudioDeviceInfo.TYPE_USB_ACCESSORY -> "USB accessory"
            AudioDeviceInfo.TYPE_BLUETOOTH_SCO -> "Bluetooth headset (SCO)"
            AudioDeviceInfo.TYPE_BLUETOOTH_A2DP -> "Bluetooth audio (A2DP, output only)"
            AudioDeviceInfo.TYPE_LINE_ANALOG -> "Line (analog)"
            AudioDeviceInfo.TYPE_TELEPHONY -> "Telephony"
            else -> "Audio device ($type)"
        }
    }

    object Factory : EndpointFactory {
        override val kind = "audio"
        override val title = "Audio (soft modem)"
        override val description = "Radio audio through the headset jack, a USB sound card, a Bluetooth headset, or by holding the phone to the radio (acoustic coupling)."
        override val linkTypes = listOf("audio")
        override val scannable = true

        /** "Connect a radio": the phone's likely radio interfaces with ready configs (see [RadioInterfaces]). */
        override fun scan(ctx: HostContext, timeoutMs: Long): JSONArray {
            if (Build.VERSION.SDK_INT < 23) return JSONArray()
            val am = ctx.app.getSystemService(Context.AUDIO_SERVICE) as AudioManager
            fun kind(t: Int) = when (t) {
                AudioDeviceInfo.TYPE_USB_DEVICE, AudioDeviceInfo.TYPE_USB_HEADSET, AudioDeviceInfo.TYPE_USB_ACCESSORY -> "usb"
                AudioDeviceInfo.TYPE_WIRED_HEADSET, AudioDeviceInfo.TYPE_WIRED_HEADPHONES, AudioDeviceInfo.TYPE_LINE_ANALOG -> "wired"
                AudioDeviceInfo.TYPE_BUILTIN_MIC, AudioDeviceInfo.TYPE_BUILTIN_SPEAKER -> "builtin"
                AudioDeviceInfo.TYPE_BLUETOOTH_SCO -> "sco"
                else -> "other"
            }
            fun devs(flag: Int) = am.getDevices(flag).map { RadioInterfaces.AudioDev(it.id, kind(it.type), it.productName?.toString() ?: deviceTypeName(it.type)) }
            val serials = try {
                val usb = ctx.app.getSystemService(Context.USB_SERVICE) as android.hardware.usb.UsbManager
                usb.deviceList.values.filter { org.embermesh.app.endpoint.serial.UsbSerialDriver.familyOf(it) != null }
                    .map { RadioInterfaces.SerialDev(UsbSerialEndpoint.deviceId(it), it.vendorId, it.productId, it.productName ?: "USB serial") }
            } catch (_: Exception) { emptyList() }
            return RadioInterfaces.suggest(devs(AudioManager.GET_DEVICES_INPUTS), devs(AudioManager.GET_DEVICES_OUTPUTS), serials)
        }
        override val transport = org.embermesh.app.host.Transport.AUDIO

        override fun unavailableReason(ctx: HostContext): String? =
            if (!ctx.app.packageManager.hasSystemFeature(android.content.pm.PackageManager.FEATURE_MICROPHONE)) "no microphone" else null

        override fun permissions(ctx: HostContext, config: JSONObject): List<String> {
            val p = arrayListOf(Manifest.permission.RECORD_AUDIO)
            if (Build.VERSION.SDK_INT >= 31 && (config.str("input_device") != "default" || config.str("output_device") != "default")) {
                p.add(Manifest.permission.BLUETOOTH_CONNECT)
            }
            return p
        }

        private fun devices(ctx: HostContext, flag: Int): List<Pair<String, String>> {
            val list = arrayListOf("default" to "System default")
            if (Build.VERSION.SDK_INT >= 23) {
                val am = ctx.app.getSystemService(Context.AUDIO_SERVICE) as AudioManager
                for (d in am.getDevices(flag)) {
                    if (d.type == AudioDeviceInfo.TYPE_TELEPHONY || (Build.VERSION.SDK_INT >= 28 && d.type == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER_SAFE)) continue
                    val name = d.productName?.toString()?.takeIf { it.isNotBlank() && it != android.os.Build.MODEL }
                    list.add(d.id.toString() to (deviceTypeName(d.type) + (name?.let { " · $it" } ?: "")))
                }
            }
            return list
        }

        override fun configSchema(ctx: HostContext): JSONArray {
            val inputs = if (ctx.hasContext) devices(ctx, AudioManager.GET_DEVICES_INPUTS) else listOf("default" to "System default")
            val outputs = if (ctx.hasContext) devices(ctx, AudioManager.GET_DEVICES_OUTPUTS) else listOf("default" to "System default")
            return Fields.of {
                select("input_device", "Input (from radio)", inputs, default = "default",
                    help = "Plug the radio's speaker output into the headset jack / USB sound card, or use the built-in mic for acoustic coupling.")
                select("output_device", "Output (to radio)", outputs, default = "default")
                select("sample_rate", "Sample rate", listOf("auto" to "Automatic (device native)") + STANDARD_RATES.map { it.toString() to "$it Hz" },
                    default = "auto", advanced = true)
                number("tx_gain", "Transmit level", default = 100, min = 0, max = 400, step = 5, unit = "%",
                    help = "Lower this if the radio's audio is distorted; raise it for acoustic coupling.")
                select("source", "Capture source", listOf("auto" to "Automatic (unprocessed)", "unprocessed" to "Unprocessed",
                    "voice_recognition" to "Voice recognition", "mic" to "Microphone (may apply AGC)", "camcorder" to "Camcorder"),
                    default = "auto", advanced = true)
                select("ptt", "Push-to-talk (PTT)", pttOptions(ctx), default = "vox",
                    help = "How the radio is keyed: its VOX, or the RTS/DTR line of a USB serial cable (an open serial connection, or an adapter used only for PTT).")
                select("ptt_line", "PTT line", listOf("rts" to "RTS", "dtr" to "DTR"), default = "rts", advanced = true,
                    help = "Which control line of the serial cable keys the radio.")
                bool("ptt_invert", "Invert PTT", default = false, advanced = true)
            }
        }

        /** VOX, every open serial connection, and every attached USB serial adapter. */
        fun pttOptions(ctx: HostContext): List<Pair<String, String>> {
            val out = arrayListOf("vox" to "VOX / none (the radio keys itself)")
            for ((id, name) in ctx.pttLinks()) out.add("link:$id" to "Serial connection “$name” (RTS/DTR)")
            for ((dev, label) in UsbSerialEndpoint.attached(ctx)) out.add("usb:$dev" to "USB serial adapter $label, PTT only")
            return out
        }

        override fun create(ctx: HostContext, config: JSONObject, sink: EndpointSink) = AudioEndpoint(ctx, config, sink)
    }
}
