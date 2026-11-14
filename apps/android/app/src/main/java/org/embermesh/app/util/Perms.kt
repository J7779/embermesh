package org.embermesh.app.util

import android.content.pm.PackageManager
import android.os.Build
import org.embermesh.app.host.HostContext
import org.embermesh.app.host.HostError
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

object Perms {
    /**
     * Android 17 (API 37) local network protection: apps targeting 37 cannot send to or receive from
     * LAN addresses (hotspot clients, 192.168.x, 10.x, multicast) without this runtime permission —
     * connections simply time out. Needed by every Wi-Fi/LAN endpoint and the hub.
     */
    const val LOCAL_NETWORK = "android.permission.ACCESS_LOCAL_NETWORK"
    fun localNetwork(): List<String> = if (Build.VERSION.SDK_INT >= 37) listOf(LOCAL_NETWORK) else emptyList()

    fun missing(ctx: HostContext, perms: List<String>): List<String> {
        if (Build.VERSION.SDK_INT < 23) return emptyList()
        return perms.filter { ctx.app.checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
    }

    fun has(ctx: HostContext, perms: List<String>) = missing(ctx, perms).isEmpty()

    /**
     * Blocks the calling (non-main) thread until [perms] are granted, prompting through the UI if it
     * is in the foreground. Throws HostError(denied) otherwise.
     */
    fun ensure(ctx: HostContext, perms: List<String>, timeoutMs: Long = 120_000) {
        val need = missing(ctx, perms)
        if (need.isEmpty()) return
        val ui = ctx.ui ?: throw HostError(HostError.DENIED, "permission required: ${need.joinToString()} (open the app to grant)")
        val latch = CountDownLatch(1)
        var ok = false
        ctx.main.post { ui.requestPermissions(need.toTypedArray()) { granted -> ok = granted; latch.countDown() } }
        if (!latch.await(timeoutMs, TimeUnit.MILLISECONDS) || !ok) {
            throw HostError(HostError.DENIED, "permission denied: ${need.joinToString()}")
        }
    }
}
