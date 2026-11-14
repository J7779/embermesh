package org.embermesh.app.host

/** An RPC error with one of the codes from RPC_API §1. */
class HostError(val code: String, message: String) : Exception(message) {
    companion object {
        const val BAD_PARAMS = "bad_params"
        const val NOT_FOUND = "not_found"
        const val NOT_IMPLEMENTED = "not_implemented"
        const val UNSUPPORTED = "unsupported"
        const val BUSY = "busy"
        const val DENIED = "denied"
        const val TOO_LARGE = "too_large"
        const val IO_ERROR = "io_error"
        const val INTERNAL = "internal"

        fun badParams(msg: String) = HostError(BAD_PARAMS, msg)
        fun unsupported(msg: String) = HostError(UNSUPPORTED, msg)
    }
}
