package org.embermesh.app.services

import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.SurfaceTexture
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.HandlerThread
import android.provider.OpenableColumns
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.embermesh.app.service.MeshService
import org.embermesh.app.util.Perms
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.io.File
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

/** Platform-only implementation of the device services (no Play Services, no AndroidX). */
@SuppressLint("MissingPermission") // every path calls Perms.ensure / checks first
class AndroidDeviceServices(private val ctx: HostContext) : DeviceServices {
    private val app: Context get() = ctx.app
    private val pm: PackageManager get() = app.packageManager

    override fun features(): List<String> {
        val f = ArrayList<String>()
        val lm = app.getSystemService(Context.LOCATION_SERVICE) as? LocationManager
        if (lm != null && (lm.allProviders.contains(LocationManager.GPS_PROVIDER) || lm.allProviders.contains(LocationManager.NETWORK_PROVIDER))) f.add("location")
        if (pm.hasSystemFeature(PackageManager.FEATURE_CAMERA_ANY)) f.add("qr_scan")
        f.add("flash")
        if (pm.hasSystemFeature(PackageManager.FEATURE_CAMERA_FLASH)) f.add("torch")
        f.add("tone")
        f.add("file")
        f.add("share_app")
        return f
    }

    // --- location ----------------------------------------------------------------------------------

    private val locLock = Any()
    private var watchThread: HandlerThread? = null
    private var watchListener: LocationListener? = null

    private fun ensureLocationPermission() {
        val fine = Manifest.permission.ACCESS_FINE_LOCATION
        val coarse = Manifest.permission.ACCESS_COARSE_LOCATION
        if (Perms.missing(ctx, listOf(coarse)).isEmpty() || Perms.missing(ctx, listOf(fine)).isEmpty()) return
        try { Perms.ensure(ctx, listOf(fine, coarse)) } catch (e: HostError) {
            if (Perms.missing(ctx, listOf(coarse)).isNotEmpty()) throw e // approximate-only is acceptable
        }
    }

    private fun providers(lm: LocationManager): List<String> {
        val order = listOf(LocationManager.GPS_PROVIDER, LocationManager.NETWORK_PROVIDER) +
            (if (Build.VERSION.SDK_INT >= 31) listOf(LocationManager.FUSED_PROVIDER) else emptyList())
        return order.filter { p -> try { lm.isProviderEnabled(p) } catch (_: Exception) { false } }
    }

    private fun toPos(l: Location) = ServiceLogic.position(l.latitude, l.longitude,
        if (l.hasAltitude()) l.altitude else null, if (l.hasAccuracy()) l.accuracy else null, l.time.takeIf { it > 0 } ?: System.currentTimeMillis())

    override fun locationGet(timeoutMs: Long): JSONObject {
        if (!features().contains("location")) throw HostError.unsupported("no location hardware")
        ensureLocationPermission()
        val lm = app.getSystemService(Context.LOCATION_SERVICE) as LocationManager
        val provs = providers(lm)
        if (provs.isEmpty()) throw HostError.unsupported("location is off — enable it in Android settings")
        val last = provs.mapNotNull { try { lm.getLastKnownLocation(it) } catch (_: Exception) { null } }.maxByOrNull { it.time }
        val latch = CountDownLatch(1)
        var fix: Location? = null
        val th = HandlerThread("ember-loc").apply { start() }
        val l = object : LocationListener {
            override fun onLocationChanged(location: Location) { if (fix == null) { fix = location; latch.countDown() } }
            @Deprecated("old API") override fun onStatusChanged(provider: String?, status: Int, extras: Bundle?) {}
            override fun onProviderEnabled(provider: String) {}
            override fun onProviderDisabled(provider: String) {}
        }
        try {
            for (p in provs) try { lm.requestLocationUpdates(p, 0L, 0f, l, th.looper) } catch (_: Exception) {}
            latch.await(timeoutMs, TimeUnit.MILLISECONDS)
        } finally {
            try { lm.removeUpdates(l) } catch (_: Exception) {}
            th.quitSafely()
        }
        val got = fix ?: last ?: throw HostError(HostError.BUSY, "no position fix yet (go outside / wait for GPS)")
        return toPos(got)
    }

    override fun locationWatch(enabled: Boolean, onFix: (JSONObject) -> Unit) {
        synchronized(locLock) {
            val lm = app.getSystemService(Context.LOCATION_SERVICE) as LocationManager
            val wasActive = watchListener != null
            watchListener?.let { try { lm.removeUpdates(it) } catch (_: Exception) {} }
            watchThread?.quitSafely()
            watchListener = null
            watchThread = null
            MeshService.locationActive = false
            if (!enabled) { if (wasActive) MeshService.poke(app); return }
            ensureLocationPermission()
            val provs = providers(lm)
            if (provs.isEmpty()) throw HostError.unsupported("location is off — enable it in Android settings")
            val th = HandlerThread("ember-loc-watch").apply { start() }
            val l = object : LocationListener {
                override fun onLocationChanged(location: Location) { try { onFix(toPos(location)) } catch (_: Exception) {} }
                @Deprecated("old API") override fun onStatusChanged(provider: String?, status: Int, extras: Bundle?) {}
                override fun onProviderEnabled(provider: String) {}
                override fun onProviderDisabled(provider: String) {}
            }
            for (p in provs) try { lm.requestLocationUpdates(p, 10_000L, 10f, l, th.looper) } catch (_: Exception) {}
            watchThread = th
            watchListener = l
            MeshService.locationActive = true
            MeshService.poke(app)
        }
    }

    // --- UI round trips ----------------------------------------------------------------------------

    private fun forResult(intent: Intent, timeoutMs: Long = 10 * 60_000L): Pair<Int, Intent?> {
        val ui = ctx.ui ?: throw HostError(HostError.BUSY, "open the app to use this feature")
        val latch = CountDownLatch(1)
        var res: Pair<Int, Intent?> = Activity.RESULT_CANCELED to null
        ctx.main.post { ui.startForResult(intent) { code, data -> res = code to data; latch.countDown() } }
        if (!latch.await(timeoutMs, TimeUnit.MILLISECONDS)) throw HostError(HostError.BUSY, "timed out")
        return res
    }

    override fun qrScan(): String {
        if (!pm.hasSystemFeature(PackageManager.FEATURE_CAMERA_ANY)) throw HostError.unsupported("no camera")
        if (ctx.ui == null) throw HostError(HostError.BUSY, "open the app to scan")
        Perms.ensure(ctx, listOf(Manifest.permission.CAMERA))
        val (code, data) = forResult(Intent(app, QrScanActivity::class.java))
        val err = data?.getStringExtra(QrScanActivity.EXTRA_ERROR)
        if (err != null) throw HostError(HostError.IO_ERROR, err)
        if (code != Activity.RESULT_OK) throw HostError(HostError.DENIED, "cancelled")
        return data?.getStringExtra(QrScanActivity.EXTRA_TEXT) ?: throw HostError(HostError.DENIED, "cancelled")
    }

    // --- files -------------------------------------------------------------------------------------

    override fun fileSave(name: String, mime: String, data: ByteArray, mode: String) {
        val create = Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType(mime.ifEmpty { "application/octet-stream" })
            .putExtra(Intent.EXTRA_TITLE, name)
        if (mode == "share" || ctx.ui == null || create.resolveActivity(pm) == null) {
            share(name, mime, data)
            return
        }
        val (code, res) = forResult(create)
        val uri = res?.data
        if (code != Activity.RESULT_OK || uri == null) throw HostError(HostError.DENIED, "cancelled")
        try {
            app.contentResolver.openOutputStream(uri, "w")?.use { it.write(data) } ?: throw HostError(HostError.IO_ERROR, "cannot write file")
        } catch (e: java.io.IOException) { throw HostError(HostError.IO_ERROR, e.message ?: "write failed") }
    }

    /** Share sheet (Bluetooth, Quick Share / Nearby, messengers, file managers...). */
    fun share(name: String, mime: String, data: ByteArray) {
        val uri = SharedFileProvider.publish(app, ServiceLogic.cleanName(name, "ember-file"), data)
        launchShare(uri, mime.ifEmpty { "application/octet-stream" }, "Share $name")
    }

    private fun launchShare(uri: Uri, mime: String, title: String) {
        val send = Intent(Intent.ACTION_SEND).setType(mime).putExtra(Intent.EXTRA_STREAM, uri)
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        send.clipData = android.content.ClipData.newRawUri("", uri)
        val chooser = Intent.createChooser(send, title).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_GRANT_READ_URI_PERMISSION)
        val ui = ctx.ui
        if (ui is Activity) ctx.main.post { try { ui.startActivity(chooser) } catch (_: Exception) {} }
        else app.startActivity(chooser)
    }

    override fun filePick(accept: String?): PickedFile {
        val mimes = ServiceLogic.acceptToMimes(accept)
        val i = Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE)
        i.type = if (mimes.size == 1) mimes[0] else "*/*"
        if (mimes.size > 1) i.putExtra(Intent.EXTRA_MIME_TYPES, mimes.toTypedArray())
        val intent = if (i.resolveActivity(pm) != null) i else Intent(Intent.ACTION_GET_CONTENT).addCategory(Intent.CATEGORY_OPENABLE).setType(i.type)
        val (code, res) = forResult(intent)
        val uri = res?.data
        if (code != Activity.RESULT_OK || uri == null) throw HostError(HostError.DENIED, "cancelled")
        val cr = app.contentResolver
        var name: String? = null
        try {
            cr.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c -> if (c.moveToFirst()) name = c.getString(0) }
        } catch (_: Exception) {}
        val bytes = try {
            cr.openInputStream(uri)?.use { input ->
                val out = ByteArrayOutputStream()
                val buf = ByteArray(64 * 1024)
                while (true) {
                    val n = input.read(buf)
                    if (n < 0) break
                    out.write(buf, 0, n)
                    if (out.size() > MAX_PICK) throw HostError(HostError.TOO_LARGE, "file larger than ${MAX_PICK / (1024 * 1024)} MB")
                }
                out.toByteArray()
            } ?: throw HostError(HostError.IO_ERROR, "cannot read file")
        } catch (e: java.io.IOException) { throw HostError(HostError.IO_ERROR, e.message ?: "read failed") }
        return PickedFile(ServiceLogic.cleanName(name ?: uri.lastPathSegment, "file"), cr.getType(uri) ?: "application/octet-stream", bytes)
    }

    // --- signalling --------------------------------------------------------------------------------

    private val flashGen = AtomicInteger()
    private val toneGen = AtomicInteger()

    override fun flash(patternMs: LongArray) {
        val gen = flashGen.incrementAndGet()
        Thread({
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_DISPLAY)
            runFlash(gen, patternMs)
        }, "ember-flash").apply { isDaemon = true; priority = Thread.MAX_PRIORITY; start() }
    }

    @Volatile var lastFlashStats: JSONObject? = null
        private set

    override fun signalStats(): JSONObject = JSONObject().put("flash", lastFlashStats ?: JSONObject.NULL)

    private fun runFlash(gen: Int, pattern: LongArray) {
        val torch = openTorch()
        try {
            // Absolute-deadline keyer (no drift). Screen fallback: the switch is posted to the UI
            // thread and becomes visible on the next display frame (up to ~16 ms later at 60 Hz).
            val act: (Boolean) -> Unit = if (torch != null) torch else { on -> ctx.ui?.let { ui -> ctx.main.post { ui.setScreenFlash(on) } } }
            val stats = MorseKeyer().run(pattern, { flashGen.get() != gen }, act)
            lastFlashStats = stats.json().put("light", if (torch != null) "torch" else "screen")
        } finally {
            if (torch != null) try { torch(false) } catch (_: Exception) {}
            ctx.ui?.let { ui -> ctx.main.post { ui.setScreenFlash(false) } }
            closeTorch?.invoke()
            closeTorch = null
        }
    }

    @Volatile private var closeTorch: (() -> Unit)? = null

    /** Returns a torch switch, or null when there is no usable flash (screen fallback). */
    @Suppress("DEPRECATION")
    private fun openTorch(): ((Boolean) -> Unit)? {
        if (!pm.hasSystemFeature(PackageManager.FEATURE_CAMERA_FLASH)) return null
        if (Build.VERSION.SDK_INT >= 23) {
            val cm = app.getSystemService(Context.CAMERA_SERVICE) as CameraManager
            val id = try {
                cm.cameraIdList.firstOrNull { cm.getCameraCharacteristics(it).get(CameraCharacteristics.FLASH_INFO_AVAILABLE) == true }
            } catch (_: Exception) { null } ?: return null
            return { on -> try { cm.setTorchMode(id, on) } catch (_: Exception) {} }
        }
        return try {
            val cam = android.hardware.Camera.open()
            val tex = SurfaceTexture(0)
            cam.setPreviewTexture(tex)
            cam.startPreview()
            closeTorch = { try { cam.stopPreview(); cam.release(); tex.release() } catch (_: Exception) {} }
            val f: (Boolean) -> Unit = { on ->
                try {
                    val p = cam.parameters
                    p.flashMode = if (on) android.hardware.Camera.Parameters.FLASH_MODE_TORCH else android.hardware.Camera.Parameters.FLASH_MODE_OFF
                    cam.parameters = p
                } catch (_: Exception) {}
            }
            f
        } catch (_: Exception) { null }
    }

    override fun tone(patternMs: LongArray, freqHz: Int) {
        val gen = toneGen.incrementAndGet()
        Thread({ runTone(gen, patternMs, freqHz) }, "ember-tone").apply { isDaemon = true; start() }
    }

    private fun runTone(gen: Int, pattern: LongArray, freq: Int) {
        // Sample-accurate: element lengths are exact sample counts on the audio clock.
        android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO)
        val rate = 22050
        val min = AudioTrack.getMinBufferSize(rate, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT).coerceAtLeast(2048)
        val track = try {
            AudioTrack(AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_ALARM).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build(),
                AudioFormat.Builder().setSampleRate(rate).setEncoding(AudioFormat.ENCODING_PCM_16BIT).setChannelMask(AudioFormat.CHANNEL_OUT_MONO).build(),
                min * 2, AudioTrack.MODE_STREAM, 0)
        } catch (_: Exception) { return }
        try {
            track.play()
            val chunk = ShortArray(rate / 50) // 20 ms
            val ramp = rate * 5 / 1000
            for ((i, ms) in pattern.withIndex()) {
                if (toneGen.get() != gen) break
                val total = (ms * rate / 1000).toInt()
                var done = 0
                while (done < total && toneGen.get() == gen) {
                    val n = minOf(chunk.size, total - done)
                    if (i % 2 == 0) ServiceLogic.sineChunk(chunk, n, done, total, rate, freq, ramp) else java.util.Arrays.fill(chunk, 0, n, 0)
                    track.write(chunk, 0, n)
                    done += n
                }
            }
            java.util.Arrays.fill(chunk, 0)
            track.write(chunk, 0, chunk.size)
        } catch (_: Exception) {
        } finally {
            try { track.stop() } catch (_: Exception) {}
            track.release()
        }
    }

    private fun sleepCancellable(ms: Long, cancelled: () -> Boolean) {
        var left = ms
        while (left > 0 && !cancelled()) {
            val s = minOf(left, 20L)
            try { Thread.sleep(s) } catch (_: InterruptedException) { return }
            left -= s
        }
    }

    // --- share the app itself ----------------------------------------------------------------------

    override fun shareApp(mode: String) {
        val info = app.applicationInfo
        if (!info.splitSourceDirs.isNullOrEmpty()) {
            throw HostError.unsupported("this copy was installed as an app bundle; the base APK alone will not install on another phone")
        }
        val ver = try { @Suppress("DEPRECATION") pm.getPackageInfo(app.packageName, 0).versionName } catch (_: Exception) { null } ?: "0"
        val fileName = "EmberMesh-$ver.apk"
        if (mode == "save") {
            val create = Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE)
                .setType(APK_MIME).putExtra(Intent.EXTRA_TITLE, fileName)
            val (code, res) = forResult(create)
            val uri = res?.data
            if (code != Activity.RESULT_OK || uri == null) throw HostError(HostError.DENIED, "cancelled")
            File(info.sourceDir).inputStream().use { input ->
                app.contentResolver.openOutputStream(uri, "w")?.use { input.copyTo(it, 64 * 1024) } ?: throw HostError(HostError.IO_ERROR, "cannot write")
            }
            return
        }
        launchShare(SharedFileProvider.apkUri(fileName), APK_MIME, "Share EmberMesh")
    }

    override fun shutdown() {
        flashGen.incrementAndGet()
        toneGen.incrementAndGet()
        synchronized(locLock) {
            val lm = app.getSystemService(Context.LOCATION_SERVICE) as LocationManager
            watchListener?.let { try { lm.removeUpdates(it) } catch (_: Exception) {} }
            watchThread?.quitSafely()
            watchListener = null; watchThread = null
            MeshService.locationActive = false
        }
    }

    companion object {
        const val MAX_PICK = 32 * 1024 * 1024
        const val APK_MIME = "application/vnd.android.package-archive"
    }
}
