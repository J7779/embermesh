package org.embermesh.app.core

/**
 * What the host side (router, link manager, endpoints) needs from the core. Implemented by
 * [EmberNode] over JNI; JVM unit tests use a fake.
 */
interface CoreApi {
    /** JSON-RPC call; returns the envelope text {"ok":true,"result":{}} / {"ok":false,"error":{}}. */
    fun call(method: String, paramsJson: String): String

    /** Bytes from the endpoint under [linkId] (stream: any chunking; datagram: exactly one). */
    fun linkRx(linkId: Int, data: ByteArray, off: Int = 0, len: Int = data.size)

    fun audioRx(linkId: Int, samples: ShortArray, count: Int, sampleRate: Int)

    /** Fills [out] with up to [max] samples to play; returns samples written (0 = idle). */
    fun audioTx(linkId: Int, out: ShortArray, max: Int, sampleRate: Int): Int

    /** Ask the node thread to tick + drain the outbox soon (after feeding input or calling RPC). */
    fun wake()
}
