package org.embermesh.app.capture

import android.annotation.SuppressLint
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Build
import android.os.Process
import org.embermesh.app.host.HostError
import org.json.JSONObject

/**
 * `capture_mic_pcm`: microphone → morse.rx.feed as 16-bit PCM at the session's sample rate.
 * Captures with the UNPROCESSED source where the phone has it (no AGC / noise suppression, which
 * smear a keyed tone), else VOICE_RECOGNITION. If the device cannot record at the session rate it
 * records at 16/48 kHz and [Resampler] converts.
 */
class MicPcmSource(private val feeder: MorseFeeder, private val sessionRate: Int) {
    @Volatile private var running = false
    private var record: AudioRecord? = null
    private var thread: Thread? = null
    @Volatile var deviceRate = 0; private set
    @Volatile var peak = 0; private set
    private var resampler: Resampler? = null

    /** Synthetic / test entry: PCM as delivered by the device at [rate]. Same path the record loop uses. */
    fun processPcm(buf: ShortArray, n: Int, rate: Int) {
        for (i in 0 until n) { val v = kotlin.math.abs(buf[i].toInt()); if (v > peak) peak = v }
        if (rate == sessionRate) { feeder.audio(buf, n, sessionRate); return }
        val rs = resampler?.takeIf { it.inRate == rate } ?: Resampler(rate, sessionRate).also { resampler = it }
        val out = rs.process(buf, n)
        feeder.audio(out, out.size, sessionRate)
    }

    @SuppressLint("MissingPermission")
    fun start() {
        val sources = ArrayList<Int>()
        if (Build.VERSION.SDK_INT >= 24) sources.add(MediaRecorder.AudioSource.UNPROCESSED)
        sources.add(MediaRecorder.AudioSource.VOICE_RECOGNITION)
        sources.add(MediaRecorder.AudioSource.MIC)
        var rec: AudioRecord? = null
        loop@ for (rate in listOf(sessionRate, 16000, 48000, 44100)) for (src in sources) {
            val min = AudioRecord.getMinBufferSize(rate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT)
            if (min <= 0) continue
            val r = try { AudioRecord(src, rate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT, maxOf(min * 2, rate / 5 * 2)) } catch (_: Exception) { null } ?: continue
            if (r.state == AudioRecord.STATE_INITIALIZED) { rec = r; break@loop }
            r.release()
        }
        val r = rec ?: throw HostError(HostError.IO_ERROR, "no usable microphone")
        r.startRecording()
        if (r.recordingState != AudioRecord.RECORDSTATE_RECORDING) { r.release(); throw HostError(HostError.BUSY, "microphone is in use") }
        record = r
        deviceRate = r.sampleRate
        running = true
        thread = Thread({
            Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO)
            val buf = ShortArray(deviceRate / 50)
            while (running) {
                val n = try { r.read(buf, 0, buf.size) } catch (_: Throwable) { -1 }
                if (n < 0) break
                if (n > 0) processPcm(buf, n, deviceRate)
            }
        }, "ember-morse-mic").apply { isDaemon = true; start() }
    }

    fun stop() {
        running = false
        thread?.join(500)
        try { record?.stop() } catch (_: Throwable) {}
        try { record?.release() } catch (_: Throwable) {}
        record = null
        feeder.flushAudio()
    }

    fun stats(): JSONObject = feeder.stats().put("device_rate", deviceRate).put("session_rate", sessionRate)
        .put("peak", peak)
}

/** Streaming linear-interpolation resampler with a simple box pre-filter when decimating. Pure. */
class Resampler(val inRate: Int, private val outRate: Int) {
    private var pos = 0.0          // position of the next output sample in input samples (relative to [last])
    private var last = 0.0
    private val step = inRate.toDouble() / outRate
    private val box = maxOf(1, (step).toInt())
    private val hist = DoubleArray(box)
    private var hi = 0

    fun process(buf: ShortArray, n: Int): ShortArray {
        val out = ShortArray(((n + 1) / step).toInt() + 2)
        var k = 0
        for (i in 0 until n) {
            hist[hi] = buf[i].toDouble(); hi = (hi + 1) % box
            val cur = hist.sum() / box
            // output samples falling between last (t=-1) and cur (t=0)
            while (pos <= 1.0) {
                if (k < out.size) out[k++] = (last + (cur - last) * pos).toInt().coerceIn(-32768, 32767).toShort()
                pos += step
            }
            pos -= 1.0
            last = cur
        }
        return out.copyOf(k)
    }
}
