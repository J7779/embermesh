package org.embermesh.app.services

import android.content.ContentProvider
import android.content.ContentValues
import android.content.Context
import android.database.Cursor
import android.database.MatrixCursor
import android.net.Uri
import android.os.ParcelFileDescriptor
import android.provider.OpenableColumns
import java.io.File
import java.io.FileNotFoundException

/**
 * Minimal read-only content provider (instead of androidx FileProvider) so the share sheet can hand
 * files to other apps:
 *   content://org.embermesh.app.files/share/<name>  -> cacheDir/share/<name>
 *   content://org.embermesh.app.files/apk/<name>    -> the installed base APK
 */
class SharedFileProvider : ContentProvider() {
    override fun onCreate() = true

    private fun fileFor(uri: Uri): File {
        val ctx = context ?: throw FileNotFoundException()
        val segs = uri.pathSegments
        if (segs.size != 2) throw FileNotFoundException(uri.toString())
        return when (segs[0]) {
            "share" -> {
                val dir = shareDir(ctx)
                val f = File(dir, segs[1])
                if (f.canonicalFile.parentFile != dir.canonicalFile) throw FileNotFoundException("bad path")
                f
            }
            "apk" -> File(ctx.applicationInfo.sourceDir)
            else -> throw FileNotFoundException(uri.toString())
        }
    }

    override fun openFile(uri: Uri, mode: String): ParcelFileDescriptor {
        if (mode != "r") throw SecurityException("read-only")
        val f = fileFor(uri)
        if (!f.isFile) throw FileNotFoundException(uri.toString())
        return ParcelFileDescriptor.open(f, ParcelFileDescriptor.MODE_READ_ONLY)
    }

    override fun query(uri: Uri, projection: Array<out String>?, selection: String?, selectionArgs: Array<out String>?, sortOrder: String?): Cursor {
        val f = fileFor(uri)
        val cols = projection ?: arrayOf(OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE)
        val c = MatrixCursor(cols)
        c.addRow(cols.map { col ->
            when (col) {
                OpenableColumns.DISPLAY_NAME -> uri.lastPathSegment
                OpenableColumns.SIZE -> f.length()
                else -> null
            }
        }.toTypedArray())
        return c
    }

    override fun getType(uri: Uri): String {
        val name = uri.lastPathSegment ?: return "application/octet-stream"
        return when {
            uri.pathSegments.firstOrNull() == "apk" || name.endsWith(".apk") -> "application/vnd.android.package-archive"
            name.endsWith(".json") -> "application/json"
            name.endsWith(".txt") -> "text/plain"
            else -> "application/octet-stream"
        }
    }

    override fun insert(uri: Uri, values: ContentValues?): Uri? = null
    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?) = 0
    override fun update(uri: Uri, values: ContentValues?, selection: String?, selectionArgs: Array<out String>?) = 0

    companion object {
        const val AUTHORITY = "org.embermesh.app.files"

        fun shareDir(ctx: Context): File = File(ctx.cacheDir, "share").apply { mkdirs() }

        /** Writes [data] to the share dir and returns its content URI. */
        fun publish(ctx: Context, name: String, data: ByteArray): Uri {
            val f = File(shareDir(ctx), name)
            f.writeBytes(data)
            return Uri.parse("content://$AUTHORITY/share/" + Uri.encode(name))
        }

        fun apkUri(fileName: String): Uri = Uri.parse("content://$AUTHORITY/apk/" + Uri.encode(fileName))
    }
}
