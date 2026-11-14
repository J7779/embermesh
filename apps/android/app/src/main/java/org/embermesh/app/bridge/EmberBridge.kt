package org.embermesh.app.bridge

import android.webkit.JavascriptInterface

/** Exposed to the page as window.EmberNative (RPC_API §5). */
class EmberBridge(private val dispatcher: BridgeDispatcher, private val onAppearance: (theme: String, color: String) -> Unit = { _, _ -> }) {
    /** The page's theme changed (data-theme / theme-color): light or dark system-bar icons. */
    @JavascriptInterface
    fun appearance(theme: String?, color: String?) = onAppearance(theme ?: "", color ?: "")

    @JavascriptInterface
    fun postMessage(message: String?) = dispatcher.onMessage(message)

    /** Lets the page detect the bridge flavour without a round trip. */
    @JavascriptInterface
    fun platform(): String = "android"
}
