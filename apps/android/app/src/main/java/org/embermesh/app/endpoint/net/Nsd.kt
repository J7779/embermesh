package org.embermesh.app.endpoint.net

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.os.Build
import org.embermesh.app.connectivity.NearbyManager
import org.embermesh.app.connectivity.NearbyPlatform
import org.embermesh.app.connectivity.NearbyRecord
import org.embermesh.app.host.HostContext
import org.embermesh.app.util.Perms
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit

/** DNS-SD (`_embermesh._tcp`, ARCHITECTURE §6, CONNECTIVITY §3.2) discovery and registration via NsdManager. */
object Nsd {
    /** Privacy: a random 8-hex token, never the phone model, owner's name or a stable identifier. */
    fun randomToken(): String = "%08x".format(java.security.SecureRandom().nextInt())

    const val SERVICE_TYPE = "_embermesh._tcp."

    /**
     * Browses for [timeoutMs] and resolves each service (sequentially: NsdManager allows one resolve
     * at a time before Android 14). [toConfig] turns (host, port, name) into an endpoint config.
     */
    fun discover(ctx: HostContext, timeoutMs: Long, toConfig: (String, Int, String) -> JSONObject): JSONArray {
        val nsd = ctx.app.getSystemService(Context.NSD_SERVICE) as NsdManager
        val found = LinkedBlockingQueue<NsdServiceInfo>()
        val listener = object : NsdManager.DiscoveryListener {
            override fun onDiscoveryStarted(serviceType: String) {}
            override fun onDiscoveryStopped(serviceType: String) {}
            override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {}
            override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {}
            override fun onServiceFound(info: NsdServiceInfo) { found.offer(info) }
            override fun onServiceLost(info: NsdServiceInfo) {}
        }
        val out = JSONArray()
        val seen = HashSet<String>()
        try {
            nsd.discoverServices(SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, listener)
        } catch (e: Exception) {
            return out
        }
        val deadline = System.currentTimeMillis() + timeoutMs
        try {
            while (true) {
                val left = deadline - System.currentTimeMillis()
                if (left <= 0) break
                val info = found.poll(left, TimeUnit.MILLISECONDS) ?: break
                if (!seen.add(info.serviceName)) continue
                val r = resolve(nsd, info, minOf(left, 3000)) ?: continue
                @Suppress("DEPRECATION")
                val host = r.host?.hostAddress ?: continue
                val cfg = toConfig(host, r.port, r.serviceName)
                cfg.remove("_name")
                out.put(JSONObject().put("id", "${host}:${r.port}").put("name", r.serviceName)
                    .put("detail", "$host:${r.port}").put("config", cfg))
            }
        } finally {
            try { nsd.stopServiceDiscovery(listener) } catch (_: Exception) {}
        }
        return out
    }

    @Suppress("DEPRECATION")
    private fun resolve(nsd: NsdManager, info: NsdServiceInfo, timeoutMs: Long): NsdServiceInfo? {
        val latch = CountDownLatch(1)
        var result: NsdServiceInfo? = null
        try {
            nsd.resolveService(info, object : NsdManager.ResolveListener {
                override fun onResolveFailed(serviceInfo: NsdServiceInfo, errorCode: Int) { latch.countDown() }
                override fun onServiceResolved(serviceInfo: NsdServiceInfo) { result = serviceInfo; latch.countDown() }
            })
        } catch (e: Exception) { return null }
        latch.await(timeoutMs, TimeUnit.MILLISECONDS)
        return result
    }

    class Registration(private val nsd: NsdManager, private val listener: NsdManager.RegistrationListener) {
        fun unregister() { try { nsd.unregisterService(listener) } catch (_: Exception) {} }
    }

    /** Registers [name] on [port] with exactly the TXT entries [txt]. */
    fun register(ctx: HostContext, name: String, port: Int, txt: Map<String, String>): Registration? {
        val nsd = ctx.app.getSystemService(Context.NSD_SERVICE) as NsdManager
        val info = NsdServiceInfo().apply {
            serviceName = name.take(60)
            serviceType = SERVICE_TYPE
            setPort(port)
            for ((k, v) in txt) setAttribute(k, v)
        }
        val l = object : NsdManager.RegistrationListener {
            override fun onServiceRegistered(serviceInfo: NsdServiceInfo) {}
            override fun onRegistrationFailed(serviceInfo: NsdServiceInfo, errorCode: Int) {}
            override fun onServiceUnregistered(serviceInfo: NsdServiceInfo) {}
            override fun onUnregistrationFailed(serviceInfo: NsdServiceInfo, errorCode: Int) {}
        }
        return try {
            nsd.registerService(info, NsdManager.PROTOCOL_DNS_SD, l)
            Registration(nsd, l)
        } catch (e: Exception) { null }
    }
}

/**
 * Nearby discovery on NsdManager (CONNECTIVITY §3.2): advertises `EmberMesh-<token>` with TXT
 * exactly ember=1 v=1 link=stream t=<token>, and browses continuously, resolving every instance
 * (all addresses on Android 14+, one before). TXT validation is the manager's (NearbyRecord).
 */
class NsdNearbyPlatform(private val ctx: HostContext, private val log: (String) -> Unit = {}) : NearbyPlatform {
    private val nsd get() = ctx.app.getSystemService(Context.NSD_SERVICE) as NsdManager

    /**
     * Android 17+: without ACCESS_LOCAL_NETWORK, NSD browsing opens a system device picker over the
     * app and advertising is refused — neither may happen behind the user's back, so nearby
     * discovery waits until the permission was granted (asked when Nearby devices is turned on).
     */
    private fun allowed(): Boolean {
        val ok = Perms.has(ctx, Perms.localNetwork())
        if (!ok) log("nearby discovery needs the local network permission")
        return ok
    }

    override fun advertise(token: String, port: Int): AutoCloseable? {
        if (!allowed()) return null
        val r = Nsd.register(ctx, NearbyRecord.instanceName(token), port, NearbyRecord.txt(token)) ?: return null
        return AutoCloseable { r.unregister() }
    }

    override fun browse(listener: NearbyPlatform.Listener): AutoCloseable? {
        if (!allowed()) return null
        val m = nsd
        val resolver = nsdExecutor
        val watched = HashMap<String, Any>() // instance name -> ServiceInfoCallback (API 34+)
        val stop = java.util.concurrent.atomic.AtomicBoolean(false)
        val dl = object : NsdManager.DiscoveryListener {
            override fun onDiscoveryStarted(serviceType: String) {}
            override fun onDiscoveryStopped(serviceType: String) {}
            override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) { log("nsd browse failed ($errorCode)") }
            override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {}
            override fun onServiceFound(info: NsdServiceInfo) {
                resolver.execute { try { found(info) } catch (t: Throwable) { log("nsd: $t") } }
            }
            fun found(info: NsdServiceInfo) {
                run {
                    if (stop.get()) return@run
                    val name = info.serviceName ?: return@run
                    if (Build.VERSION.SDK_INT >= 34) {
                        synchronized(watched) {
                            if (name in watched || watched.size >= NearbyManager.MAX_PEERS) return@run
                            val cb = object : NsdManager.ServiceInfoCallback {
                                override fun onServiceInfoCallbackRegistrationFailed(errorCode: Int) { synchronized(watched) { watched.remove(name) } }
                                override fun onServiceUpdated(si: NsdServiceInfo) {
                                    if (!stop.get()) try { listener.found(name, si.attributes, si.hostAddresses, si.port) } catch (t: Throwable) { log("nsd: $t") }
                                }
                                override fun onServiceLost() { listener.lost(name) }
                                override fun onServiceInfoCallbackUnregistered() {}
                            }
                            try { m.registerServiceInfoCallback(info, resolver, cb); watched[name] = cb } catch (t: Throwable) { log("nsd resolve: $t") }
                        }
                    } else {
                        resolveOld(m, info)?.let { r ->
                            @Suppress("DEPRECATION")
                            val h = r.host
                            if (h != null && !stop.get()) listener.found(name, r.attributes, listOf(h), r.port)
                        }
                    }
                }
            }
            override fun onServiceLost(info: NsdServiceInfo) {
                val name = info.serviceName ?: return
                listener.lost(name)
                if (Build.VERSION.SDK_INT >= 34) synchronized(watched) {
                    (watched.remove(name) as? NsdManager.ServiceInfoCallback)?.let { try { m.unregisterServiceInfoCallback(it) } catch (_: Throwable) {} }
                }
            }
        }
        try { m.discoverServices(Nsd.SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, dl) } catch (t: Throwable) {
            log("nsd browse: $t"); return null
        }
        return AutoCloseable {
            stop.set(true)
            try { m.stopServiceDiscovery(dl) } catch (_: Throwable) {}
            if (Build.VERSION.SDK_INT >= 34) synchronized(watched) {
                for (cb in watched.values) try { m.unregisterServiceInfoCallback(cb as NsdManager.ServiceInfoCallback) } catch (_: Throwable) {}
                watched.clear()
            }
        }
    }

    companion object {
        /**
         * NsdManager keeps delivering to a callback's executor after it is unregistered (the
         * "unregistered" notification itself), and crashes the process if that executor was shut
         * down. One process-wide NSD thread, never shut down.
         */
        private val nsdExecutor = Executors.newSingleThreadExecutor(HostContext.named("ember-nsd"))
    }

    @Suppress("DEPRECATION")
    private fun resolveOld(m: NsdManager, info: NsdServiceInfo): NsdServiceInfo? {
        val latch = CountDownLatch(1)
        var result: NsdServiceInfo? = null
        try {
            m.resolveService(info, object : NsdManager.ResolveListener {
                override fun onResolveFailed(serviceInfo: NsdServiceInfo, errorCode: Int) { latch.countDown() }
                override fun onServiceResolved(serviceInfo: NsdServiceInfo) { result = serviceInfo; latch.countDown() }
            })
        } catch (_: Exception) { return null }
        latch.await(3, TimeUnit.SECONDS)
        return result
    }
}
