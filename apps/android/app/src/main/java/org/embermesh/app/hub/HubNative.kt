package org.embermesh.app.hub

import org.embermesh.app.core.EmberNative

/** JNI binding of src/main/cpp/hub_jni.cpp (the host library's hub embedded in libember_jni.so). */
object HubNative {
    /** Called on the hub's native loop thread. Keep it short. */
    interface Callback {
        fun onFrame(frame: ByteArray)
        fun onClients(clients: Int)
    }

    /** False when the APK was built with -PemberHub=false (no hub code in the native library). */
    val available: Boolean by lazy {
        try {
            EmberNative.load()
            create(object : Callback { override fun onFrame(frame: ByteArray) {}; override fun onClients(clients: Int) {} }).also { if (it != 0L) destroy(it) } != 0L
        } catch (_: UnsatisfiedLinkError) { false }
    }

    @JvmStatic external fun create(cb: Callback): Long
    @JvmStatic external fun destroy(handle: Long)
    /** ember_hub_start options JSON; null on success, else the error. */
    @JvmStatic external fun start(handle: Long, optionsJson: String): String?
    @JvmStatic external fun stop(handle: Long)
    @JvmStatic external fun broadcast(handle: Long, frame: ByteArray)
    /** {running, port, urls, clients, stats}. */
    @JvmStatic external fun status(handle: Long): String
    @JvmStatic external fun setProbe(handle: Long, json: String)
}
