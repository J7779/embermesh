package org.embermesh.app.service

import android.content.Context
import org.json.JSONObject

/** Android-only settings (host.android.settings.*). Mesh settings live in the core (settings.*). */
class AppPrefs(context: Context) {
    private val sp = context.getSharedPreferences("ember_android", Context.MODE_PRIVATE)

    var startOnBoot: Boolean
        get() = sp.getBoolean(START_ON_BOOT, false)
        set(v) = sp.edit().putBoolean(START_ON_BOOT, v).apply()

    /** Keep the foreground service (and the mesh) running when the UI is closed. */
    var runInBackground: Boolean
        get() = sp.getBoolean(RUN_IN_BACKGROUND, true)
        set(v) = sp.edit().putBoolean(RUN_IN_BACKGROUND, v).apply()

    /** "auto" (wake lock while CPU-polled links are up), "always", "never". */
    var keepCpuAwake: String
        get() = sp.getString(KEEP_CPU_AWAKE, "auto") ?: "auto"
        set(v) = sp.edit().putString(KEEP_CPU_AWAKE, if (v in KEEP_VALUES) v else "auto").apply()

    var notifyMessages: Boolean
        get() = sp.getBoolean(NOTIFY_MESSAGES, true)
        set(v) = sp.edit().putBoolean(NOTIFY_MESSAGES, v).apply()

    fun toJson(): JSONObject = JSONObject()
        .put(START_ON_BOOT, startOnBoot)
        .put(RUN_IN_BACKGROUND, runInBackground)
        .put(KEEP_CPU_AWAKE, keepCpuAwake)
        .put(NOTIFY_MESSAGES, notifyMessages)

    fun update(o: JSONObject) {
        if (o.has(START_ON_BOOT)) startOnBoot = o.optBoolean(START_ON_BOOT)
        if (o.has(RUN_IN_BACKGROUND)) runInBackground = o.optBoolean(RUN_IN_BACKGROUND)
        if (o.has(KEEP_CPU_AWAKE)) keepCpuAwake = o.optString(KEEP_CPU_AWAKE)
        if (o.has(NOTIFY_MESSAGES)) notifyMessages = o.optBoolean(NOTIFY_MESSAGES)
    }

    companion object {
        const val START_ON_BOOT = "start_on_boot"
        const val RUN_IN_BACKGROUND = "run_in_background"
        const val KEEP_CPU_AWAKE = "keep_cpu_awake"
        const val NOTIFY_MESSAGES = "notify_messages"
        val KEEP_VALUES = setOf("auto", "always", "never")
    }
}
