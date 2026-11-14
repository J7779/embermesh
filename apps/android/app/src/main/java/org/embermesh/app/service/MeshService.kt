package org.embermesh.app.service

import android.Manifest
import android.app.Service
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.util.Log
import org.embermesh.app.EmberRuntime

/**
 * Foreground service that keeps the node and its endpoints alive with the UI closed and the screen
 * off. The node itself lives in [EmberRuntime]; this service only pins the process, shows the
 * persistent notification and declares the foreground-service types the live links need.
 */
class MeshService : Service() {
    private val handler = Handler(Looper.getMainLooper())
    private var currentTypes = -1
    private val refresher = object : Runnable {
        override fun run() {
            refresh()
            handler.postDelayed(this, REFRESH_MS)
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        running = true
        EmberRuntime.foregroundChanged()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_QUIT) {
            handler.removeCallbacks(refresher)
            EmberRuntime.stop() // first: "service stopped" must imply "node stopped"
            running = false
            stopForegroundCompat()
            stopSelf()
            return START_NOT_STICKY
        }
        if (!EmberRuntime.start(this)) {
            stopSelf()
            return START_NOT_STICKY
        }
        goForeground(force = true)
        handler.removeCallbacks(refresher)
        handler.postDelayed(refresher, REFRESH_MS)
        return START_STICKY
    }

    private fun refresh() {
        if (!EmberRuntime.isStarted) return
        goForeground(force = false)
    }

    private fun goForeground(force: Boolean) {
        val n = Notifier.serviceNotification(this, try { EmberRuntime.summary() } catch (_: Throwable) { "Running" })
        val types = wantedTypes()
        try {
            if (Build.VERSION.SDK_INT >= 29) {
                if (force || types != currentTypes) {
                    startForeground(Notifier.ID_SERVICE, n, types)
                    currentTypes = types
                } else {
                    (getSystemService(NOTIFICATION_SERVICE) as android.app.NotificationManager).notify(Notifier.ID_SERVICE, n)
                }
            } else {
                startForeground(Notifier.ID_SERVICE, n)
            }
        } catch (e: Exception) {
            // e.g. microphone type requested while the app is in the background: fall back.
            Log.w(TAG, "startForeground(types=$types) failed: $e")
            if (Build.VERSION.SDK_INT >= 29) {
                try {
                    startForeground(Notifier.ID_SERVICE, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
                    currentTypes = ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE
                } catch (e2: Exception) {
                    Log.e(TAG, "cannot enter foreground: $e2")
                }
            }
        }
    }

    private fun wantedTypes(): Int {
        if (Build.VERSION.SDK_INT < 29) return 0
        var t = ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE
        val kinds = try { EmberRuntime.links.activeKinds() } catch (_: Throwable) { emptySet() }
        if (Build.VERSION.SDK_INT >= 30 && "audio" in kinds && granted(Manifest.permission.RECORD_AUDIO)) {
            t = t or ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
        }
        if (locationActive && (granted(Manifest.permission.ACCESS_FINE_LOCATION) || granted(Manifest.permission.ACCESS_COARSE_LOCATION))) {
            t = t or ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION
        }
        return t
    }

    private fun granted(p: String) = Build.VERSION.SDK_INT < 23 || checkSelfPermission(p) == PackageManager.PERMISSION_GRANTED

    private fun stopForegroundCompat() {
        if (Build.VERSION.SDK_INT >= 24) stopForeground(STOP_FOREGROUND_REMOVE) else @Suppress("DEPRECATION") stopForeground(true)
    }

    override fun onDestroy() {
        handler.removeCallbacks(refresher)
        running = false
        EmberRuntime.foregroundChanged()
        super.onDestroy()
    }

    override fun onTimeout(startId: Int, fgsType: Int) {
        // Android 15+ time limits do not apply to connectedDevice, but be safe: stop cleanly.
        Log.w(TAG, "foreground service timeout for type $fgsType")
        stopSelf()
    }

    companion object {
        private const val TAG = "MeshService"
        const val ACTION_QUIT = "org.embermesh.app.QUIT"
        private const val REFRESH_MS = 30_000L
        @Volatile var running = false
            private set
        /** Set by the location service while a watch is active (adds the location FGS type). */
        @Volatile var locationActive = false

        /** Ask a running service to re-evaluate its notification / types now. */
        fun poke(ctx: android.content.Context) {
            if (running) org.embermesh.app.MeshServiceControl.start(ctx)
        }
    }
}
