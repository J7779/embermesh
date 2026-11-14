package org.embermesh.app.util

import org.json.JSONArray
import org.json.JSONObject

/** Small helpers over org.json (available on every Android version; a real jar in JVM unit tests). */
object Json {
    fun obj(vararg pairs: Pair<String, Any?>): JSONObject {
        val o = JSONObject()
        for ((k, v) in pairs) o.put(k, wrap(v))
        return o
    }

    fun arr(items: Iterable<Any?>): JSONArray {
        val a = JSONArray()
        for (i in items) a.put(wrap(i))
        return a
    }

    @Suppress("UNCHECKED_CAST")
    fun wrap(v: Any?): Any = when (v) {
        null -> JSONObject.NULL
        is JSONObject, is JSONArray, is String, is Boolean, is Int, is Long, is Double -> v
        is Float -> v.toDouble()
        is Short -> v.toInt()
        is Byte -> v.toInt()
        is Map<*, *> -> JSONObject().also { o -> v.forEach { (k, x) -> o.put(k.toString(), wrap(x)) } }
        is Iterable<*> -> arr(v as Iterable<Any?>)
        is Array<*> -> arr(v.asIterable())
        is IntArray -> arr(v.asIterable())
        else -> v.toString()
    }

    fun parseObject(text: String?): JSONObject {
        if (text.isNullOrBlank()) return JSONObject()
        return try { JSONObject(text) } catch (e: Exception) { JSONObject() }
    }

    /** JSON text that is also a safe JavaScript expression (U+2028/U+2029 escaped for old WebViews). */
    fun jsSafe(json: String): String =
        if (json.indexOf('\u2028') < 0 && json.indexOf('\u2029') < 0) json
        else json.replace("\u2028", "\\u2028").replace("\u2029", "\\u2029")
}

fun JSONObject.str(key: String, def: String = ""): String = if (has(key) && !isNull(key)) optString(key, def) else def
fun JSONObject.strOrNull(key: String): String? = if (has(key) && !isNull(key)) optString(key) else null
fun JSONObject.int(key: String, def: Int): Int = if (has(key) && !isNull(key)) {
    when (val v = opt(key)) { is Number -> v.toInt(); is String -> v.trim().toDoubleOrNull()?.toInt() ?: def; else -> def }
} else def
fun JSONObject.long(key: String, def: Long): Long = if (has(key) && !isNull(key)) {
    when (val v = opt(key)) { is Number -> v.toLong(); is String -> v.trim().toDoubleOrNull()?.toLong() ?: def; else -> def }
} else def
fun JSONObject.double(key: String, def: Double): Double = if (has(key) && !isNull(key)) {
    when (val v = opt(key)) { is Number -> v.toDouble(); is String -> v.trim().toDoubleOrNull() ?: def; else -> def }
} else def
fun JSONObject.bool(key: String, def: Boolean): Boolean = if (has(key) && !isNull(key)) {
    when (val v = opt(key)) { is Boolean -> v; is String -> v.equals("true", true) || v == "1"; is Number -> v.toInt() != 0; else -> def }
} else def
