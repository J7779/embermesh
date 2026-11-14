package org.embermesh.app.host

import org.json.JSONArray
import org.json.JSONObject

/** Builder for RPC_API §1 Field schema arrays (schema-driven forms). */
class Fields {
    private val arr = JSONArray()

    private fun field(key: String, label: String, type: String, default: Any?, help: String?, required: Boolean, advanced: Boolean, extra: JSONObject.() -> Unit = {}): Fields {
        val f = JSONObject().put("key", key).put("label", label).put("type", type)
        if (default != null) f.put("default", default)
        if (help != null) f.put("help", help)
        if (required) f.put("required", true)
        if (advanced) f.put("advanced", true)
        f.extra()
        arr.put(f)
        return this
    }

    fun text(key: String, label: String, default: String? = null, help: String? = null, required: Boolean = false, advanced: Boolean = false) =
        field(key, label, "text", default, help, required, advanced)

    fun password(key: String, label: String, help: String? = null, advanced: Boolean = false) =
        field(key, label, "password", null, help, false, advanced)

    fun number(key: String, label: String, default: Number? = null, min: Number? = null, max: Number? = null, step: Number? = null,
               unit: String? = null, help: String? = null, required: Boolean = false, advanced: Boolean = false) =
        field(key, label, "number", default, help, required, advanced) {
            if (min != null) put("min", min)
            if (max != null) put("max", max)
            if (step != null) put("step", step)
            if (unit != null) put("unit", unit)
        }

    fun bool(key: String, label: String, default: Boolean = false, help: String? = null, advanced: Boolean = false) =
        field(key, label, "bool", default, help, false, advanced)

    /** options: value to label. */
    fun select(key: String, label: String, options: List<Pair<String, String>>, default: String? = options.firstOrNull()?.first,
               help: String? = null, required: Boolean = false, advanced: Boolean = false) =
        field(key, label, "select", default, help, required, advanced) {
            val o = JSONArray()
            for ((v, l) in options) o.put(JSONObject().put("value", v).put("label", l))
            put("options", o)
        }

    fun build(): JSONArray = arr

    companion object {
        inline fun of(block: Fields.() -> Unit): JSONArray = Fields().apply(block).build()

        /** Defaults of a schema as a config object, overlaid with [config]. */
        fun applyDefaults(schema: JSONArray, config: JSONObject): JSONObject {
            val out = JSONObject()
            for (i in 0 until schema.length()) {
                val f = schema.optJSONObject(i) ?: continue
                if (f.has("default")) out.put(f.getString("key"), f.get("default"))
            }
            val it = config.keys()
            while (it.hasNext()) { val k = it.next(); out.put(k, config.get(k)) }
            return out
        }
    }
}
