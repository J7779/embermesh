package org.embermesh.app

import android.content.Intent
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.embermesh.app.ui.MainActivity
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/** Full WebView round trip: page JS -> window.EmberNative -> core/host router -> __emberBridge.onReply. */
@RunWith(AndroidJUnit4::class)
class BridgeInstrumentedTest {
    private val activities = java.util.WeakHashMap<ActivityScenario<MainActivity>, MainActivity>()

    /**
     * Evaluates JS in the page. The activity is fetched once through onActivity; afterwards the call is a
     * plain post to the main thread, because ActivityScenario.onActivity first waits for the main looper
     * to be idle, which never happens while the page plays a camera stream (a redraw on every frame).
     */
    private fun eval(s: ActivityScenario<MainActivity>, js: String): String {
        val a = activities[s] ?: run { var x: MainActivity? = null; s.onActivity { x = it }; x!!.also { activities[s] = it } }
        var out = "null"
        val latch = CountDownLatch(1)
        a.runOnUiThread { val wv = a.webViewForTest(); if (wv == null) latch.countDown() else wv.evaluateJavascript(js) { r -> out = r ?: "null"; latch.countDown() } }
        assertTrue(latch.await(10, TimeUnit.SECONDS))
        return out
    }

    private fun waitFor(s: ActivityScenario<MainActivity>, js: String, timeoutMs: Long = 15_000): String {
        val end = System.currentTimeMillis() + timeoutMs
        while (System.currentTimeMillis() < end) {
            val r = eval(s, js)
            if (r != "null" && r != "false" && r != "undefined") return r
            Thread.sleep(200)
        }
        throw AssertionError("timed out waiting for $js")
    }

    @Test fun diagnosticsPageRoundTrips() {
        val ctx = InstrumentationRegistry.getInstrumentation().targetContext
        val intent = Intent(ctx, MainActivity::class.java).putExtra(MainActivity.EXTRA_DIAGNOSTICS, true)
        ActivityScenario.launch<MainActivity>(intent).use { s ->
            waitFor(s, "!!(window.__emberTest)")
            assertEquals("\"https:\"", eval(s, "location.protocol"))
            assertEquals("true", eval(s, "window.isSecureContext"))
            eval(s, "__emberTest.run('a','node.info',{}); __emberTest.run('b','host.info',{}); __emberTest.run('c','host.does.not.exist',{}); 1")
            val a = JSONObject(JSONObject("{\"v\":${waitFor(s, "__emberTest.results.a && __emberTest.results.a.done && JSON.stringify(__emberTest.results.a)")}}").getString("v"))
            assertTrue(a.toString(), a.getBoolean("ok"))
            assertEquals(16, a.getJSONObject("result").getString("node_id").length)
            val b = JSONObject(JSONObject("{\"v\":${waitFor(s, "__emberTest.results.b && __emberTest.results.b.done && JSON.stringify(__emberTest.results.b)")}}").getString("v"))
            assertEquals("android", b.getJSONObject("result").getString("platform"))
            val c = JSONObject(JSONObject("{\"v\":${waitFor(s, "__emberTest.results.c && __emberTest.results.c.done && JSON.stringify(__emberTest.results.c)")}}").getString("v"))
            assertEquals("unsupported", c.getString("code"))
            // Native -> JS events reach the page.
            EmberRuntime.notifyUser("bridge test", "event check")
            EmberRuntime.hostContext.emit("host.test.ping", JSONObject().put("n", 1))
            waitFor(s, "__emberTest.events.some(function(e){return e.event==='host.test.ping' && e.data.n===1})")
            // Remote content is blocked.
            assertEquals("\"blocked\"", waitFor(s, "(function(){ if(!window.__f){window.__f='pending'; fetch('https://example.com/').then(function(){window.__f='loaded'},function(){window.__f='blocked'});} return window.__f==='pending'?null:window.__f; })()"))
        }
    }

    @Test fun startUrlServesUiOrDiagnostics() {
        val ctx = InstrumentationRegistry.getInstrumentation().targetContext
        ActivityScenario.launch<MainActivity>(Intent(ctx, MainActivity::class.java)).use { s ->
            val href = waitFor(s, "document.readyState==='complete' && location.href")
            assertEquals("\"https://appassets.androidplatform.net/\"", href)
            assertEquals("true", eval(s, "!!window.EmberNative"))
            val hasUi = try { ctx.assets.open("ui/index.html").close(); true } catch (_: Exception) { false }
            if (!hasUi) waitFor(s, "!!window.__emberTest")
        }
    }

    /**
     * getUserMedia inside the WebView for the shared UI's Morse reader: the camera stream delivers
     * frames whose brightness the page samples per frame (the reader's own method), and the
     * microphone track is live. On the emulator the camera is its virtual scene and the mic is silent.
     */
    @Test fun webViewCameraAndMicCapture() {
        val inst = InstrumentationRegistry.getInstrumentation()
        val ctx = inst.targetContext
        inst.uiAutomation.grantRuntimePermission(ctx.packageName, android.Manifest.permission.CAMERA)
        inst.uiAutomation.grantRuntimePermission(ctx.packageName, android.Manifest.permission.RECORD_AUDIO)
        if (android.os.Build.VERSION.SDK_INT >= 33) inst.uiAutomation.grantRuntimePermission(ctx.packageName, android.Manifest.permission.POST_NOTIFICATIONS)
        ActivityScenario.launch<MainActivity>(Intent(ctx, MainActivity::class.java)).use { s ->
            waitFor(s, "document.readyState==='complete' && location.href", 30_000)
            eval(s, """(function(){
              window.__cam={frames:0,levels:[],t:[],err:null,mic:null};
              navigator.mediaDevices.getUserMedia({video:{facingMode:'environment',width:320,height:240},audio:false}).then(function(st){
                var v=document.createElement('video'); v.muted=true; v.playsInline=true; v.srcObject=st; document.body.appendChild(v); v.play();
                var c=document.createElement('canvas'); c.width=32; c.height=24; var g=c.getContext('2d',{willReadFrequently:true});
                function tick(now){ if(v.readyState>=2){ g.drawImage(v,0,0,32,24); var d=g.getImageData(12,9,8,6).data, sum=0;
                    for(var i=0;i<d.length;i+=4) sum+=d[i]+d[i+1]+d[i+2]; __cam.levels.push(sum/(d.length/4*765)); __cam.t.push(now); __cam.frames++; }
                  if(__cam.frames<90) (v.requestVideoFrameCallback?v.requestVideoFrameCallback(function(n){tick(n)}):requestAnimationFrame(tick)); }
                tick(performance.now());
              }).catch(function(e){ __cam.err=String(e); });
              navigator.mediaDevices.getUserMedia({audio:{echoCancellation:false,noiseSuppression:false,autoGainControl:false}}).then(function(st){
                __cam.ms=st; var tr=st.getAudioTracks()[0]; var ac=new AudioContext(); var an=ac.createAnalyser(); ac.createMediaStreamSource(st).connect(an);
                setTimeout(function(){ __cam.mic={state:tr.readyState, rate:ac.sampleRate, ctx:ac.state}; }, 800);
              }).catch(function(e){ __cam.mic={err:String(e)}; });
              return 1; })()""")
            val r = waitFor(s, "(window.__cam.err || (window.__cam.frames>=60 && window.__cam.mic)) && JSON.stringify({err:__cam.err,frames:__cam.frames,lv:__cam.levels.slice(0,60),t0:__cam.t[0],t1:__cam.t[__cam.frames-1],mic:__cam.mic})", 30_000)
            val o = JSONObject(JSONObject("{\"v\":$r}").getString("v"))
            android.util.Log.i("BridgeTest", "webview capture: frames=${o.optInt("frames")} span_ms=${o.optDouble("t1") - o.optDouble("t0")} mic=${o.opt("mic")} levels=${o.optJSONArray("lv")?.toString()?.take(200)}")
            assertTrue(o.toString(), o.isNull("err"))
            val lv = o.getJSONArray("lv")
            for (i in 0 until lv.length()) assertTrue(lv.getDouble(i) in 0.0..1.0)
            val fps = (o.getInt("frames") - 1) * 1000.0 / (o.getDouble("t1") - o.getDouble("t0"))
            assertTrue("camera frame rate $fps", fps > 5)
            assertEquals("live", o.getJSONObject("mic").getString("state"))
            // Release camera and microphone before the activity is closed.
            eval(s, "document.querySelectorAll('video').forEach(function(v){if(v.srcObject)v.srcObject.getTracks().forEach(function(t){t.stop()});v.remove();});if(window.__cam&&__cam.ms)__cam.ms.getTracks().forEach(function(t){t.stop()});1")
        }
    }

    /** The page learns the native launch logo's square before its first paint (same names as on iOS). */
    @Test fun launchLogoRectReachesThePage() {
        val inst = InstrumentationRegistry.getInstrumentation()
        val ctx = inst.targetContext
        if (android.os.Build.VERSION.SDK_INT >= 33) inst.uiAutomation.grantRuntimePermission(ctx.packageName, android.Manifest.permission.POST_NOTIFICATIONS)
        ActivityScenario.launch<MainActivity>(Intent(ctx, MainActivity::class.java)).use { s ->
            waitFor(s, "document.readyState==='complete' && location.href", 30_000)
            val js = waitFor(s, "window.__emberHost && window.__emberHost.launchLogo && JSON.stringify(window.__emberHost.launchLogo)")
            val o = JSONObject(JSONObject("{\"v\":$js}").getString("v"))
            var native = FloatArray(0)
            s.onActivity { a -> native = a.launchLogoRect() }
            assertEquals(native[0].toDouble(), o.getDouble("x"), 0.6)
            assertEquals(native[1].toDouble(), o.getDouble("y"), 0.6)
            assertEquals(native[2].toDouble(), o.getDouble("size"), 0.6)
            val css = eval(s, "getComputedStyle(document.documentElement).getPropertyValue('--launch-logo-size').trim()")
            assertEquals("\"%.2fpx\"".format(java.util.Locale.ROOT, o.getDouble("size")), css)
            // The square is centred in the page viewport and is 30 % of its shorter side.
            val vw = eval(s, "innerWidth").toDouble(); val vh = eval(s, "innerHeight").toDouble()
            assertEquals(0.30 * minOf(vw, vh), o.getDouble("size"), 1.5)
            assertEquals(vw / 2, o.getDouble("x") + o.getDouble("size") / 2, 1.5)
            assertEquals(vh / 2, o.getDouble("y") + o.getDouble("size") / 2, 1.5)
            android.util.Log.i("BridgeTest", "launch logo $o viewport ${vw}x$vh")
        }
    }
}
