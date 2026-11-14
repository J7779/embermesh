package org.embermesh.app.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.media.AudioAttributes
import android.media.RingtoneManager
import android.os.Build
import org.embermesh.app.R
import org.embermesh.app.ui.MainActivity
import org.json.JSONObject
import java.util.concurrent.atomic.AtomicInteger

/** Notification channels and builders (no AndroidX: plain platform Notification.Builder). */
object Notifier {
    const val CH_MESH = "mesh"
    const val CH_MESSAGES = "messages"
    const val CH_SOS = "sos"
    const val ID_SERVICE = 1
    private const val ID_SOS_BASE = 1000
    private val nextId = AtomicInteger(2000)

    fun createChannels(ctx: Context) {
        if (Build.VERSION.SDK_INT < 26) return
        val nm = ctx.getSystemService(NotificationManager::class.java) ?: return
        nm.createNotificationChannel(NotificationChannel(CH_MESH, "Mesh running", NotificationManager.IMPORTANCE_LOW).apply {
            description = "Shown while EmberMesh keeps relaying in the background"
            setShowBadge(false)
        })
        nm.createNotificationChannel(NotificationChannel(CH_MESSAGES, "Messages", NotificationManager.IMPORTANCE_HIGH).apply {
            description = "New direct and channel messages"
            enableVibration(true)
        })
        nm.createNotificationChannel(NotificationChannel(CH_SOS, "SOS alerts", NotificationManager.IMPORTANCE_HIGH).apply {
            description = "Emergency beacons heard on the mesh"
            enableVibration(true)
            vibrationPattern = longArrayOf(0, 600, 200, 600, 200, 600)
            enableLights(true)
            lightColor = Color.RED
            setBypassDnd(true)
            setSound(RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM),
                AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_ALARM).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build())
        })
    }

    @Suppress("DEPRECATION")
    fun builder(ctx: Context, channel: String): Notification.Builder =
        if (Build.VERSION.SDK_INT >= 26) Notification.Builder(ctx, channel) else Notification.Builder(ctx)

    fun pendingFlags(): Int = PendingIntent.FLAG_UPDATE_CURRENT or (if (Build.VERSION.SDK_INT >= 23) PendingIntent.FLAG_IMMUTABLE else 0)

    fun openAppIntent(ctx: Context, extra: String? = null): PendingIntent {
        val i = Intent(ctx, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_NEW_TASK)
        if (extra != null) i.putExtra("open", extra)
        return PendingIntent.getActivity(ctx, extra?.hashCode() ?: 0, i, pendingFlags())
    }

    fun serviceNotification(ctx: Context, text: String): Notification {
        val quit = PendingIntent.getService(ctx, 1, Intent(ctx, MeshService::class.java).setAction(MeshService.ACTION_QUIT), pendingFlags())
        val share = PendingIntent.getActivity(ctx, 2,
            Intent(ctx, MainActivity::class.java).setAction(MainActivity.ACTION_SHARE_APP).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK), pendingFlags())
        @Suppress("DEPRECATION")
        val b = builder(ctx, CH_MESH)
            .setSmallIcon(R.drawable.ic_stat_ember)
            .setContentTitle("EmberMesh is relaying")
            .setContentText(text)
            .setOngoing(true)
            .setShowWhen(false)
            .setContentIntent(openAppIntent(ctx))
            .addAction(Notification.Action.Builder(0, "Share app", share).build())
            .addAction(Notification.Action.Builder(0, "Quit", quit).build())
        if (Build.VERSION.SDK_INT < 26) b.setPriority(Notification.PRIORITY_MIN)
        if (Build.VERSION.SDK_INT >= 31) b.setForegroundServiceBehavior(Notification.FOREGROUND_SERVICE_IMMEDIATE)
        return b.build()
    }

    fun message(ctx: Context, m: JSONObject) {
        val conv = m.optString("conv")
        val from = m.optString("from_name").ifEmpty { m.optString("from").take(8) }
        val text = when (m.optString("kind")) {
            "file" -> "Sent a file"
            "position" -> "Shared a position"
            else -> m.optString("text")
        }
        @Suppress("DEPRECATION")
        val n = builder(ctx, CH_MESSAGES)
            .setSmallIcon(R.drawable.ic_stat_ember)
            .setContentTitle(from.ifEmpty { "New message" })
            .setContentText(text.take(200))
            .setStyle(Notification.BigTextStyle().bigText(text.take(1000)))
            .setAutoCancel(true)
            .setCategory(Notification.CATEGORY_MESSAGE)
            .setContentIntent(openAppIntent(ctx, conv.ifEmpty { null }))
            .setPriority(Notification.PRIORITY_HIGH)
            .setDefaults(Notification.DEFAULT_ALL)
            .build()
        post(ctx, conv.hashCode() and 0x3fffffff or 0x10000, n)
    }

    fun sos(ctx: Context, d: JSONObject) {
        val who = d.optString("name").ifEmpty { d.optString("node_id").take(8) }
        val text = d.optString("text").ifEmpty { "Emergency beacon" }
        val hops = d.optInt("hops", -1)
        @Suppress("DEPRECATION")
        val n = builder(ctx, CH_SOS)
            .setSmallIcon(R.drawable.ic_stat_ember)
            .setContentTitle("SOS from $who")
            .setContentText(if (hops >= 0) "$text · $hops hops away" else text)
            .setStyle(Notification.BigTextStyle().bigText(text))
            .setCategory(Notification.CATEGORY_ALARM)
            .setColor(Color.RED)
            .setAutoCancel(true)
            .setContentIntent(openAppIntent(ctx, "sos"))
            .setPriority(Notification.PRIORITY_MAX)
            .setDefaults(Notification.DEFAULT_ALL)
            .build()
        post(ctx, ID_SOS_BASE + (d.optString("node_id").hashCode() and 0x3ff), n)
    }

    fun generic(ctx: Context, title: String, body: String) {
        @Suppress("DEPRECATION")
        val n = builder(ctx, CH_MESSAGES)
            .setSmallIcon(R.drawable.ic_stat_ember)
            .setContentTitle(title.take(200))
            .setContentText(body.take(500))
            .setStyle(Notification.BigTextStyle().bigText(body.take(2000)))
            .setAutoCancel(true)
            .setContentIntent(openAppIntent(ctx))
            .setPriority(Notification.PRIORITY_HIGH)
            .build()
        post(ctx, nextId.incrementAndGet(), n)
    }

    private fun post(ctx: Context, id: Int, n: Notification) {
        try {
            (ctx.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager).notify(id, n)
        } catch (e: SecurityException) {
            // POST_NOTIFICATIONS not granted (Android 13+): nothing to do.
        }
    }
}
