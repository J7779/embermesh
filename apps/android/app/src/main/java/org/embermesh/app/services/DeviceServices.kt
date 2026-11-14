package org.embermesh.app.services

import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import org.json.JSONObject

/*
 * Device services behind host.location.*, host.qr.scan, host.file.*, host.signal.*, host.app.share.
 * Every blocking method is called on an I/O thread (never the main thread) and may throw HostError.
 * Implemented by AndroidDeviceServices (AndroidDeviceServices.kt).
 */

/** A picked file. */
class PickedFile(val name: String, val mime: String, val data: ByteArray)

interface DeviceServices {
    /** host.info feature strings this device can actually do (e.g. "location","qr_scan","flash","tone","file","share_app"). */
    fun features(): List<String>

    /** One GPS/network fix as an RPC Position {lat, lon, alt?, accuracy?, ts}; blocks up to timeoutMs. */
    fun locationGet(timeoutMs: Long): JSONObject
    /** Start/stop continuous fixes; each fix is delivered to [onFix] (runtime pushes position.set + host.location). */
    fun locationWatch(enabled: Boolean, onFix: (JSONObject) -> Unit)

    /** Camera QR scan; returns the decoded text. Requires the UI (throws busy/denied/unsupported). */
    fun qrScan(): String

    /** mode "save" (Storage Access Framework, default) or "share" (share sheet: Bluetooth, Quick Share, ...). */
    fun fileSave(name: String, mime: String, data: ByteArray, mode: String = "save")
    fun filePick(accept: String?): PickedFile

    /** host.signal.flash (RPC_API §10): blocks until the pattern finished or was stopped; {used, stopped, timing}. */
    fun flash(patternMs: LongArray, mode: String = "auto"): JSONObject
    fun tone(patternMs: LongArray, freqHz: Int)
    /** host.signal.stop: ends a running flash or tone at once (light off, screen restored). */
    fun stopSignals() {}
    /** host.info.signal: {torch:"yes"|"no", screen:true}. */
    fun signalInfo(): JSONObject = JSONObject().put("torch", "no").put("screen", true)

    /** Share (mode "share") or save via SAF (mode "save") the installed APK. */
    fun shareApp(mode: String)

    fun shutdown() {}

    /** Timing statistics of the last host.signal.flash (Android extension host.signal.stats). */
    fun signalStats(): JSONObject = JSONObject()
}

/** Used until / where a service is missing: everything unsupported. */
open class NoDeviceServices : DeviceServices {
    override fun features(): List<String> = emptyList()
    override fun locationGet(timeoutMs: Long): JSONObject = throw HostError.unsupported("location unavailable")
    override fun locationWatch(enabled: Boolean, onFix: (JSONObject) -> Unit) = throw HostError.unsupported("location unavailable")
    override fun qrScan(): String = throw HostError.unsupported("no QR scanner")
    override fun fileSave(name: String, mime: String, data: ByteArray, mode: String) = throw HostError.unsupported("file save unavailable")
    override fun filePick(accept: String?): PickedFile = throw HostError.unsupported("file pick unavailable")
    override fun flash(patternMs: LongArray, mode: String): JSONObject = throw HostError.unsupported("no flash")
    override fun tone(patternMs: LongArray, freqHz: Int) = throw HostError.unsupported("no tone")
    override fun shareApp(mode: String) = throw HostError.unsupported("app sharing unavailable")
}

/** Factory the runtime calls; replaced by the real implementation (AndroidDeviceServices). */
object DeviceServicesFactory {
    fun create(ctx: HostContext): DeviceServices = AndroidDeviceServices(ctx)
}
