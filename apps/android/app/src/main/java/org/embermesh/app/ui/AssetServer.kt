package org.embermesh.app.ui

import android.content.res.AssetManager
import android.webkit.WebResourceResponse
import java.io.ByteArrayInputStream
import java.io.FileNotFoundException
import java.io.InputStream

/**
 * Serves the bundled UI from APK assets under https://appassets.androidplatform.net/ (a secure
 * context, so ES modules, crypto.subtle, service-worker-free storage etc. behave as on the web).
 *   /                 -> assets/ui/index.html, or the diagnostic page if the UI is not bundled
 *   /<path>           -> assets/ui/<path>
 *   /__ember/<path>   -> assets/ember/<path>   (bridge-test.html and other app-owned pages)
 * Nothing outside assets is reachable; every other origin is refused by MainActivity.
 */
class AssetServer(private val open: (String) -> InputStream?, private val uiPresent: Boolean) {
    /** Markup added before </head> of the UI's index.html (e.g. the launch-logo script tag). */
    @Volatile var headInject: String? = null


    constructor(assets: AssetManager) : this(
        { p -> try { assets.open(p) } catch (_: FileNotFoundException) { null } catch (_: java.io.IOException) { null } },
        hasUi(assets),
    )

    class Resolved(val assetPath: String, val mime: String)

    /** Pure path mapping (unit-tested). Returns null for anything that must 404. */
    fun resolve(rawPath: String?): Resolved? {
        var path = (rawPath ?: "/").substringBefore('?').substringBefore('#')
        path = try { java.net.URLDecoder.decode(path.replace("+", "%2B"), "UTF-8") } catch (_: Exception) { return null }
        if (!path.startsWith("/")) path = "/$path"
        // No traversal, no hidden files, no NULs.
        val segs = path.split('/').filter { it.isNotEmpty() }
        if (segs.any { it == ".." || it == "." || it.startsWith(".") || it.contains('\u0000') || it.contains('\\') }) return null
        if (segs.isEmpty()) {
            return if (uiPresent) Resolved("ui/index.html", "text/html") else Resolved(DIAG_PAGE, "text/html")
        }
        val asset = if (segs[0] == "__ember") {
            if (segs.size == 1) return null
            "ember/" + segs.drop(1).joinToString("/")
        } else {
            "ui/" + segs.joinToString("/") + if (path.endsWith("/")) "/index.html" else ""
        }
        return Resolved(asset, mimeFor(asset))
    }

    fun serve(rawPath: String?): WebResourceResponse {
        val r = resolve(rawPath) ?: return notFound()
        var stream = open(r.assetPath) ?: return notFound()
        val inject = headInject
        if (inject != null && r.assetPath == "ui/index.html") {
            val html = stream.use { it.readBytes().toString(Charsets.UTF_8) }
            val i = html.indexOf("</head>")
            stream = ByteArrayInputStream((if (i < 0) html else html.substring(0, i) + inject + html.substring(i)).toByteArray())
        }
        val encoding = if (r.mime.startsWith("text/") || r.mime == "application/json" || r.mime == "image/svg+xml" ||
            r.mime == "application/manifest+json") "utf-8" else null
        val resp = WebResourceResponse(r.mime, encoding, stream)
        resp.setStatusCodeAndReasonPhrase(200, "OK")
        resp.responseHeaders = mapOf(
            "Cache-Control" to "no-cache",
            "X-Content-Type-Options" to "nosniff",
            "Access-Control-Allow-Origin" to ORIGIN,
        )
        return resp
    }

    companion object {
        const val HOST = "appassets.androidplatform.net"
        const val ORIGIN = "https://$HOST"
        const val DIAG_PAGE = "ember/bridge-test.html"

        fun hasUi(assets: AssetManager): Boolean = try {
            assets.open("ui/index.html").close(); true
        } catch (_: Exception) { false }

        fun notFound(): WebResourceResponse =
            WebResourceResponse("text/plain", "utf-8", 404, "Not Found", mapOf("Cache-Control" to "no-store"),
                ByteArrayInputStream("404".toByteArray()))

        fun forbidden(): WebResourceResponse =
            WebResourceResponse("text/plain", "utf-8", 403, "Forbidden", mapOf("Cache-Control" to "no-store"),
                ByteArrayInputStream("offline app: remote content is blocked".toByteArray()))

        fun mimeFor(path: String): String = when (path.substringAfterLast('.', "").lowercase()) {
            "html", "htm" -> "text/html"
            "js", "mjs" -> "text/javascript"
            "css" -> "text/css"
            "json", "map" -> "application/json"
            "webmanifest" -> "application/manifest+json"
            "svg" -> "image/svg+xml"
            "png" -> "image/png"
            "jpg", "jpeg" -> "image/jpeg"
            "gif" -> "image/gif"
            "webp" -> "image/webp"
            "ico" -> "image/x-icon"
            "wasm" -> "application/wasm"
            "woff2" -> "font/woff2"
            "woff" -> "font/woff"
            "ttf" -> "font/ttf"
            "txt", "md" -> "text/plain"
            "wav" -> "audio/wav"
            "mp3" -> "audio/mpeg"
            "ogg" -> "audio/ogg"
            "pack", "bin" -> "application/octet-stream"
            else -> "application/octet-stream"
        }
    }
}
