package org.embermesh.app.ui

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class AssetServerTest {
    private val withUi = AssetServer({ null }, uiPresent = true)
    private val noUi = AssetServer({ null }, uiPresent = false)

    @Test fun rootServesUiOrDiagnostics() {
        assertEquals("ui/index.html", withUi.resolve("/")!!.assetPath)
        assertEquals("ember/bridge-test.html", noUi.resolve("/")!!.assetPath)
        assertEquals("ember/bridge-test.html", noUi.resolve(null)!!.assetPath)
    }

    @Test fun pathsAndMimes() {
        val js = withUi.resolve("/js/backends/native.js?v=1")!!
        assertEquals("ui/js/backends/native.js", js.assetPath); assertEquals("text/javascript", js.mime)
        assertEquals("ui/css/app.css", withUi.resolve("/css/app.css")!!.assetPath)
        assertEquals("ui/sub/index.html", withUi.resolve("/sub/")!!.assetPath)
        assertEquals("ember/bridge-test.html", withUi.resolve("/__ember/bridge-test.html")!!.assetPath)
        assertEquals("ui/a b.html", withUi.resolve("/a%20b.html")!!.assetPath)
        assertEquals("application/wasm", AssetServer.mimeFor("x.wasm"))
        assertEquals("application/manifest+json", AssetServer.mimeFor("m.webmanifest"))
    }

    @Test fun traversalAndHiddenBlocked() {
        for (p in listOf("/../secret", "/js/../../x", "/%2e%2e/x", "/.git/config", "/__ember", "/a\\b", "/%00x/../y", "/x%zz")) {
            assertNull(p, withUi.resolve(p))
        }
    }
}
