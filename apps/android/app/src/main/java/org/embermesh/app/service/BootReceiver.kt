package org.embermesh.app.service

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import org.embermesh.app.MeshServiceControl

/** Optional start on boot (host.android.settings start_on_boot) and after an app update. */
class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        when (intent.action) {
            Intent.ACTION_BOOT_COMPLETED, "android.intent.action.QUICKBOOT_POWERON", Intent.ACTION_MY_PACKAGE_REPLACED -> {
                val prefs = AppPrefs(context)
                if (prefs.startOnBoot && prefs.runInBackground) MeshServiceControl.start(context)
            }
        }
    }
}
