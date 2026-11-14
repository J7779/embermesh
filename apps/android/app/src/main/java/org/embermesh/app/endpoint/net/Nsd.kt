package org.embermesh.app.endpoint.net

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import org.embermesh.app.host.HostContext
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.CountDownLatch
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit

/** DNS-SD (`_embermesh._tcp`, ARCHITECTURE §6) discovery and registration via the platform NsdManager. */
object Nsd {
    /** Privacy: the advertised service name never carries the phone model or owner's name. */
    fun anonymousName(): String = "EmberMesh-%04x".format(java.security.SecureRandom().nextInt(0x10000))

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

    fun register(ctx: HostContext, name: String, port: Int): Registration? {
        val nsd = ctx.app.getSystemService(Context.NSD_SERVICE) as NsdManager
        val info = NsdServiceInfo().apply {
            serviceName = name.take(60)
            serviceType = SERVICE_TYPE
            setPort(port)
            setAttribute("proto", "tcp")
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
