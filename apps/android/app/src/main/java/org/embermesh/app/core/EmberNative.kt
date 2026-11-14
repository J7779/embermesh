package org.embermesh.app.core

import android.content.res.AssetManager

/** Raw JNI binding of core/include/ember/ember.h (see src/main/cpp/ember_jni.cpp). Use [EmberNode]. */
object EmberNative {
    const val OUT_NONE = 0
    const val OUT_EVENT = 1
    const val OUT_LINK_TX = 2

    @Volatile private var loaded = false

    @Synchronized
    fun load() {
        if (!loaded) {
            System.loadLibrary("ember_jni")
            loaded = true
        }
    }

    @JvmStatic external fun version(): String
    @JvmStatic external fun create(configJson: String, entropy: ByteArray): Long
    @JvmStatic external fun destroy(handle: Long)
    @JvmStatic external fun addEntropy(handle: Long, data: ByteArray)
    @JvmStatic external fun call(handle: Long, method: String, paramsJson: String): String
    @JvmStatic external fun tick(handle: Long, nowMs: Long)
    @JvmStatic external fun nextWakeupMs(handle: Long): Int
    @JvmStatic external fun linkRx(handle: Long, linkId: Int, data: ByteArray, off: Int, len: Int)
    @JvmStatic external fun audioRx(handle: Long, linkId: Int, samples: ShortArray, count: Int, sampleRate: Int)
    @JvmStatic external fun audioTx(handle: Long, linkId: Int, out: ShortArray, max: Int, sampleRate: Int): Int
    /** null when empty, else [kind:int32 LE][link_id:uint32 LE][payload]. */
    @JvmStatic external fun poll(handle: Long): ByteArray?
    @JvmStatic external fun guidesLoad(handle: Long, pack: ByteArray): Int
    @JvmStatic external fun guidesLoadAsset(handle: Long, assets: AssetManager, path: String): Int
    @JvmStatic external fun guidesLoadFile(handle: Long, path: String): Int
}
