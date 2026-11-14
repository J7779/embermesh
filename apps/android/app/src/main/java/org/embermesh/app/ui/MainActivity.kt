package org.embermesh.app.ui

import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.content.ActivityNotFoundException
import android.content.Intent
import android.content.pm.ApplicationInfo
import android.content.pm.PackageManager
import android.graphics.Color
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.View
import android.view.WindowInsetsController
import android.view.HapticFeedbackConstants
import android.view.ViewGroup
import android.view.WindowInsets
import android.view.WindowManager
import android.webkit.ConsoleMessage
import android.webkit.PermissionRequest
import android.webkit.RenderProcessGoneDetail
import android.webkit.ValueCallback
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.FrameLayout
import android.widget.TextView
import android.widget.Toast
import org.embermesh.app.EmberRuntime
import org.embermesh.app.MeshServiceControl
import org.embermesh.app.bridge.BridgeDispatcher
import org.embermesh.app.bridge.EmberBridge
import org.embermesh.app.host.UiHost
import org.embermesh.app.util.Json
import org.json.JSONObject
import java.util.concurrent.atomic.AtomicInteger

/**
 * Hosts the shared web UI in a WebView with the native bridge (RPC_API §5). The node keeps running in
 * [EmberRuntime] / MeshService when this activity goes away.
 */
class MainActivity : Activity(), UiHost {
    private var webView: WebView? = null
    private lateinit var root: FrameLayout
    private var dispatcher: BridgeDispatcher? = null
    private val main = Handler(Looper.getMainLooper())
    private val pendingJs = StringBuilder()
    private var flushPosted = false
    private val eventListener: (String) -> Unit = { json -> evalJs(BridgeDispatcher.eventJs(json)) }
    private var fileCallback: ValueCallback<Array<Uri>>? = null
    private val nextCode = AtomicInteger(100)
    private val permCallbacks = HashMap<Int, (Boolean) -> Unit>()
    private val resultCallbacks = HashMap<Int, (Int, Intent?) -> Unit>()
    private var backCallback: Any? = null
    @Volatile var pageLoaded = false
        private set
    private var handover: LaunchHandover? = null

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setupWindow()
        root = FrameLayout(this).apply { setBackgroundColor(BG) }
        setContentView(root)
        applyInsets()

        if (!EmberRuntime.start(applicationContext)) {
            showFatal("The EmberMesh core could not start on this device (native library missing for this CPU?).")
            return
        }
        if (EmberRuntime.prefs.runInBackground) MeshServiceControl.start(this)
        EmberRuntime.hostContext.ui = this

        createWebView()
        if (handover == null) handover = LaunchHandover(this, root, main, BG) {
            // ui/README.md "Launch screen": the native placeholder is gone, start the animation now.
            evalJs("window.dispatchEvent(new Event('ember:launch-removed'));")
        }.also { it.install() }
        registerBack()
        askNotificationPermission()
        handleIntent(intent)
    }

    private fun setupWindow() {
        window.decorView.setBackgroundColor(BG)
        if (Build.VERSION.SDK_INT >= 30) {
            window.setDecorFitsSystemWindows(false)
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
        }
        if (Build.VERSION.SDK_INT < 35) {
            @Suppress("DEPRECATION")
            window.statusBarColor = Color.TRANSPARENT
            @Suppress("DEPRECATION")
            window.navigationBarColor = if (Build.VERSION.SDK_INT >= 26) Color.TRANSPARENT else BG
        }
        if (Build.VERSION.SDK_INT >= 28) {
            window.attributes = window.attributes.apply {
                layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            }
        }
        @Suppress("DEPRECATION")
        window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE)
    }

    /** Edge-to-edge: pad the content by the system bars and the soft keyboard, expose them to CSS too. */
    private fun applyInsets() {
        root.setOnApplyWindowInsetsListener { v, insets ->
            val l: Int; val t: Int; val r: Int; val b: Int
            if (Build.VERSION.SDK_INT >= 30) {
                val bars = insets.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout())
                val ime = insets.getInsets(WindowInsets.Type.ime())
                l = bars.left; t = bars.top; r = bars.right; b = maxOf(bars.bottom, ime.bottom)
            } else {
                @Suppress("DEPRECATION")
                l = insets.systemWindowInsetLeft
                @Suppress("DEPRECATION")
                t = insets.systemWindowInsetTop
                @Suppress("DEPRECATION")
                r = insets.systemWindowInsetRight
                @Suppress("DEPRECATION")
                b = insets.systemWindowInsetBottom // includes the IME with adjustResize
            }
            v.setPadding(l, t, r, b)
            if (Build.VERSION.SDK_INT >= 30) WindowInsets.CONSUMED else @Suppress("DEPRECATION") insets.consumeSystemWindowInsets()
        }
    }

    @SuppressLint("SetJavaScriptEnabled", "JavascriptInterface")
    private fun createWebView() {
        val debuggable = (applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) != 0
        WebView.setWebContentsDebuggingEnabled(debuggable)
        val wv = WebView(this)
        wv.setBackgroundColor(BG)
        wv.isFocusable = true
        wv.isFocusableInTouchMode = true
        wv.settings.apply {
            javaScriptEnabled = true
            domStorageEnabled = true
            @Suppress("DEPRECATION")
            databaseEnabled = true
            allowFileAccess = false
            allowContentAccess = false
            @Suppress("DEPRECATION")
            allowFileAccessFromFileURLs = false
            @Suppress("DEPRECATION")
            allowUniversalAccessFromFileURLs = false
            mixedContentMode = WebSettings.MIXED_CONTENT_NEVER_ALLOW
            setGeolocationEnabled(false) // location goes through host.location.*
            setSupportMultipleWindows(false)
            javaScriptCanOpenWindowsAutomatically = false
            mediaPlaybackRequiresUserGesture = false
            cacheMode = WebSettings.LOAD_NO_CACHE
            // Pinch-zoom when the page's viewport allows it; no on-screen zoom buttons.
            setSupportZoom(true)
            builtInZoomControls = true
            displayZoomControls = false
            // Font size: the system font scale reaches the UI as host.info.accessibility.font_scale and the
            // UI multiplies it with its own --text-scale; the web view's text zoom stays at 100 % so text is
            // never scaled twice.
            textZoom = 100
            if (Build.VERSION.SDK_INT >= 26) safeBrowsingEnabled = false // needs the network; content is local
            userAgentString = "$userAgentString EmberMesh/${EmberRuntime.appVersion} (android)"
        }
        val server = AssetServer(assets)
        // A classic same-origin script at the end of <head> runs before the first paint (the CSP allows
        // 'self' but no inline scripts): it hands the page the native launch logo's rectangle.
        server.headInject = "<script src=\"/__ember/launch-logo.js\"></script>"
        wv.webViewClient = object : WebViewClient() {
            override fun shouldInterceptRequest(view: WebView, request: WebResourceRequest): WebResourceResponse? {
                val u = request.url
                if (u.scheme == "https" && u.host == AssetServer.HOST && u.encodedPath == "/__ember/launch-logo.js") {
                    return WebResourceResponse("text/javascript", "utf-8", java.io.ByteArrayInputStream(launchLogoScript().toByteArray())).apply {
                        responseHeaders = mapOf("Cache-Control" to "no-store", "X-Content-Type-Options" to "nosniff")
                    }
                }
                return if (u.scheme == "https" && u.host == AssetServer.HOST) server.serve(u.encodedPath)
                else AssetServer.forbidden() // no remote content, ever
            }

            override fun shouldOverrideUrlLoading(view: WebView, request: WebResourceRequest): Boolean {
                val u = request.url
                if (u.scheme == "https" && u.host == AssetServer.HOST) return false
                openExternal(u)
                return true
            }

            override fun onPageCommitVisible(view: WebView, url: String?) {
                handover?.webCommitted(view)
            }

            override fun onPageFinished(view: WebView, url: String?) {
                pageLoaded = true
                pushInsetsToPage()
                watchAppearance()
                if (Build.VERSION.SDK_INT < 23) handover?.webCommitted(view)
            }

            override fun onRenderProcessGone(view: WebView, detail: RenderProcessGoneDetail?): Boolean {
                Log.w(TAG, "WebView renderer gone (crash=${detail?.didCrash()}); recreating")
                root.removeView(view)
                view.destroy()
                if (webView === view) webView = null
                main.post { if (!isFinishing) createWebView() }
                return true
            }
        }
        wv.webChromeClient = object : WebChromeClient() {
            override fun onConsoleMessage(m: ConsoleMessage): Boolean {
                Log.println(if (m.messageLevel() == ConsoleMessage.MessageLevel.ERROR) Log.ERROR else Log.INFO, "EmberWeb",
                    "${m.message()} (${m.sourceId()?.substringAfterLast('/')}:${m.lineNumber()})")
                return true
            }

            override fun onShowFileChooser(view: WebView, cb: ValueCallback<Array<Uri>>, params: FileChooserParams): Boolean {
                fileCallback?.onReceiveValue(null)
                fileCallback = cb
                val i = try { params.createIntent() } catch (_: Exception) { Intent(Intent.ACTION_GET_CONTENT).setType("*/*") }
                i.addCategory(Intent.CATEGORY_OPENABLE)
                if (params.mode == FileChooserParams.MODE_OPEN_MULTIPLE) i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
                startForResult(i) { code, data ->
                    val uris = if (code == RESULT_OK && data != null) {
                        FileChooserParams.parseResult(code, data) ?: data.clipData?.let { c -> Array(c.itemCount) { c.getItemAt(it).uri } }
                    } else null
                    fileCallback?.onReceiveValue(uris)
                    fileCallback = null
                }
                return true
            }

            override fun onPermissionRequest(request: PermissionRequest) {
                // getUserMedia (camera for in-page QR, microphone) only for our own origin.
                if (request.origin?.host != AssetServer.HOST) { request.deny(); return }
                val want = ArrayList<String>()
                val res = request.resources.filter {
                    it == PermissionRequest.RESOURCE_VIDEO_CAPTURE || it == PermissionRequest.RESOURCE_AUDIO_CAPTURE
                }
                if (res.isEmpty()) { request.deny(); return }
                if (PermissionRequest.RESOURCE_VIDEO_CAPTURE in res) want.add(Manifest.permission.CAMERA)
                if (PermissionRequest.RESOURCE_AUDIO_CAPTURE in res) want.add(Manifest.permission.RECORD_AUDIO)
                requestPermissions(want.toTypedArray()) { ok -> if (ok) request.grant(res.toTypedArray()) else request.deny() }
            }
        }
        val d = EmberRuntime.newDispatcher { js -> evalJs(js) }
        dispatcher = d
        wv.addJavascriptInterface(EmberBridge(d) { theme, color -> main.post { applySystemBars(theme, color) } }, "EmberNative")
        EmberRuntime.eventListeners.add(eventListener)
        root.addView(wv, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        // Rotation / inset changes move the logo square: keep the page's copy current.
        wv.addOnLayoutChangeListener { _, l, t, r, b, ol, ot, orr, ob ->
            if ((r - l != orr - ol || b - t != ob - ot) && pageLoaded) evalJs(launchLogoScript())
        }
        webView = wv
        pageLoaded = false
        wv.loadUrl(startUrl())
    }

    private fun startUrl(): String {
        val diag = intent?.getBooleanExtra(EXTRA_DIAGNOSTICS, false) == true || intent?.action == ACTION_DIAGNOSTICS
        return if (diag) "${AssetServer.ORIGIN}/__ember/bridge-test.html" else "${AssetServer.ORIGIN}/"
    }

    /**
     * The native launch logo's square in CSS px of the page viewport (same contract as the iOS shell):
     * `window.__emberHost.launchLogo = {x, y, size}` and `--launch-logo-x/-y/-size` on <html>. The logo
     * is LOGO_FRACTION of the web view's shorter side, centred in the web view (which sits inside the
     * system-bar insets).
     */
    fun launchLogoRect(): FloatArray {
        val wv = webView
        val dm = resources.displayMetrics
        var w = wv?.width ?: 0; var h = wv?.height ?: 0 // View.getWidth/Height: plain fields, any thread
        if (w <= 0 || h <= 0) { // not laid out yet: the window minus the padding the insets will apply
            w = (root.width.takeIf { it > 0 } ?: dm.widthPixels) - root.paddingLeft - root.paddingRight
            h = (root.height.takeIf { it > 0 } ?: dm.heightPixels) - root.paddingTop - root.paddingBottom
        }
        val side = (minOf(w, h) * LaunchHandover.LOGO_FRACTION).toInt()
        // The page uses width=device-width at scale 1, so 1 CSS px = 1 dp. (No WebView API here: this
        // also runs on the web view's request thread, and WebView methods are main-thread only.)
        val k = dm.density
        return floatArrayOf((w - side) / 2f / k, (h - side) / 2f / k, side / k)
    }

    fun launchLogoScript(): String {
        val r = launchLogoRect()
        val (x, y, s) = Triple("%.2f".format(java.util.Locale.ROOT, r[0]), "%.2f".format(java.util.Locale.ROOT, r[1]), "%.2f".format(java.util.Locale.ROOT, r[2]))
        return "(function(){var h=window.__emberHost||(window.__emberHost={});h.launchLogo={x:$x,y:$y,size:$s};" +
            "var st=document.documentElement&&document.documentElement.style;" +
            "if(st){st.setProperty('--launch-logo-x','${x}px');st.setProperty('--launch-logo-y','${y}px');st.setProperty('--launch-logo-size','${s}px');}})();"
    }

    /** Reports the page's theme (data-theme / theme-color) whenever it changes → system-bar icon colours. */
    private fun watchAppearance() {
        evalJs("""(function(){if(window.__emberAppearance)return;window.__emberAppearance=1;
            function send(){try{var m=document.querySelector('meta[name="theme-color"]');
              EmberNative.appearance(document.documentElement.dataset.theme||'',m?m.getAttribute('content'):'');}catch(e){}}
            send();new MutationObserver(send).observe(document.documentElement,{attributes:true,attributeFilter:['data-theme']});
            var m=document.querySelector('meta[name="theme-color"]');if(m)new MutationObserver(send).observe(m,{attributes:true});})();""")
    }

    @Volatile var lightBars = false
        private set

    /** Dark icons on light pages, light icons on dark pages (the launch screen is dark). */
    fun applySystemBars(theme: String, color: String) {
        val c = try { Color.parseColor(color) } catch (_: Exception) { null }
        val light = theme == "light" || (c != null && (Color.red(c) * 299 + Color.green(c) * 587 + Color.blue(c) * 114) / 1000 > 160)
        if (light == lightBars) return
        lightBars = light
        if (Build.VERSION.SDK_INT >= 30) {
            val mask = WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS or WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS
            window.insetsController?.setSystemBarsAppearance(if (light) mask else 0, mask)
        } else if (Build.VERSION.SDK_INT >= 23) {
            @Suppress("DEPRECATION")
            var f = window.decorView.systemUiVisibility
            @Suppress("DEPRECATION")
            f = if (light) f or View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR else f and View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR.inv()
            if (Build.VERSION.SDK_INT >= 26) {
                @Suppress("DEPRECATION")
                f = if (light) f or View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR else f and View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR.inv()
            }
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = f
        }
    }

    override fun haptic(kind: String): Boolean {
        val v = webView ?: root
        val c = when (kind) {
            "success" -> if (Build.VERSION.SDK_INT >= 30) HapticFeedbackConstants.CONFIRM else HapticFeedbackConstants.VIRTUAL_KEY
            "error" -> if (Build.VERSION.SDK_INT >= 30) HapticFeedbackConstants.REJECT else HapticFeedbackConstants.LONG_PRESS
            "warning" -> HapticFeedbackConstants.LONG_PRESS
            else -> HapticFeedbackConstants.KEYBOARD_TAP
        }
        var done = false
        val latch = java.util.concurrent.CountDownLatch(1)
        main.post { done = v.performHapticFeedback(c); latch.countDown() } // honours the system touch-feedback setting
        latch.await(500, java.util.concurrent.TimeUnit.MILLISECONDS)
        return done
    }

    override fun onEnterAnimationComplete() {
        super.onEnterAnimationComplete()
        handover?.enterAnimationComplete()
    }

    override fun onConfigurationChanged(newConfig: android.content.res.Configuration) {
        super.onConfigurationChanged(newConfig)
        webView?.settings?.textZoom = 100
        EmberRuntime.a11y?.check(newConfig)
    }

    private fun pushInsetsToPage() {
        val dp = resources.displayMetrics.density
        evalJs("document.documentElement.style.setProperty('--ember-native-density','$dp');" +
            "document.documentElement.classList.add('ember-android');")
    }

    /** Queues JS and flushes it in one evaluateJavascript per main-loop turn (cheap for event bursts). */
    fun evalJs(js: String) {
        synchronized(pendingJs) {
            pendingJs.append(js).append('\n')
            if (flushPosted) return
            flushPosted = true
        }
        main.post {
            val batch: String
            synchronized(pendingJs) {
                batch = pendingJs.toString()
                pendingJs.setLength(0)
                flushPosted = false
            }
            webView?.evaluateJavascript(batch, null)
        }
    }

    // --- back button -------------------------------------------------------------------------------

    private fun registerBack() {
        if (Build.VERSION.SDK_INT >= 33) {
            val cb = android.window.OnBackInvokedCallback { handleBack() }
            onBackInvokedDispatcher.registerOnBackInvokedCallback(android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT, cb)
            backCallback = cb
        }
    }

    @Deprecated("Pre-Android 13 back handling")
    override fun onBackPressed() = handleBack()

    /** The page may consume back (close a dialog) via window.__emberBridge.onBack() returning true. */
    private fun handleBack() {
        val wv = webView ?: run { moveTaskToBack(true); return }
        wv.evaluateJavascript("(function(){try{return !!(window.__emberBridge&&window.__emberBridge.onBack&&window.__emberBridge.onBack());}catch(e){return false;}})()") { r ->
            if (r == "true") return@evaluateJavascript
            if (wv.canGoBack()) wv.goBack() else moveTaskToBack(true) // keep the node running
        }
    }

    // --- intents -----------------------------------------------------------------------------------

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleIntent(intent)
    }

    private fun handleIntent(intent: Intent?) {
        intent ?: return
        when {
            intent.action == android.hardware.usb.UsbManager.ACTION_USB_DEVICE_ATTACHED -> EmberRuntime.links.kick("serial")
            intent.action == ACTION_SHARE_APP -> EmberRuntime.hostContext.io.execute {
                try { EmberRuntime.services.shareApp("share") } catch (e: Exception) {
                    main.post { Toast.makeText(this, "Cannot share the app: ${e.message}", Toast.LENGTH_LONG).show() }
                }
            }
            intent.action == Intent.ACTION_VIEW && intent.data?.scheme == "ember" -> {
                // Contact cards / channel invites: forwarded to the UI as an Android extension event.
                val ev = BridgeDispatcher.eventJson("host.open_uri", JSONObject().put("uri", intent.data.toString()))
                deliverWhenLoaded(BridgeDispatcher.eventJs(ev))
            }
            intent.getStringExtra("open") != null -> {
                val ev = BridgeDispatcher.eventJson("host.open", JSONObject().put("target", intent.getStringExtra("open")))
                deliverWhenLoaded(BridgeDispatcher.eventJs(ev))
            }
        }
    }

    private fun deliverWhenLoaded(js: String, tries: Int = 0) {
        if (pageLoaded || tries > 40) evalJs(js) else main.postDelayed({ deliverWhenLoaded(js, tries + 1) }, 250)
    }

    private fun openExternal(u: Uri) {
        when (u.scheme) {
            "ember" -> {
                val ev = BridgeDispatcher.eventJson("host.open_uri", JSONObject().put("uri", u.toString()))
                evalJs(BridgeDispatcher.eventJs(ev))
            }
            "http", "https", "geo", "tel", "mailto", "sms" -> try {
                startActivity(Intent(Intent.ACTION_VIEW, u).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
            } catch (_: ActivityNotFoundException) {}
            else -> Log.w(TAG, "blocked navigation to ${u.scheme}:")
        }
    }

    // --- UiHost ------------------------------------------------------------------------------------

    override fun requestPermissions(perms: Array<String>, cb: (Boolean) -> Unit) {
        if (Build.VERSION.SDK_INT < 23) { cb(true); return }
        val need = perms.filter { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
        if (need.isEmpty()) { cb(true); return }
        val code = nextCode.incrementAndGet() and 0xffff
        permCallbacks[code] = cb
        requestPermissions(need.toTypedArray(), code)
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        val cb = permCallbacks.remove(requestCode) ?: return
        cb(grantResults.isNotEmpty() && grantResults.all { it == PackageManager.PERMISSION_GRANTED })
    }

    override fun startForResult(intent: Intent, cb: (Int, Intent?) -> Unit) {
        val code = nextCode.incrementAndGet() and 0xffff
        resultCallbacks[code] = cb
        try {
            @Suppress("DEPRECATION")
            startActivityForResult(intent, code)
        } catch (e: ActivityNotFoundException) {
            resultCallbacks.remove(code)
            cb(RESULT_CANCELED, null)
        }
    }

    @Deprecated("platform API")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        @Suppress("DEPRECATION")
        super.onActivityResult(requestCode, resultCode, data)
        resultCallbacks.remove(requestCode)?.invoke(resultCode, data)
    }

    override fun setKeepScreenOn(on: Boolean) {
        if (on) window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        else window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }

    private var captureOverlay: View? = null

    override fun setCaptureOverlay(view: View?, rectCss: JSONObject?) {
        val wv = webView
        if (view == null || rectCss == null || wv == null) {
            captureOverlay?.let { root.removeView(it) }
            captureOverlay = null
            return
        }
        // CSS px -> device px: the page uses width=device-width, so 1 CSS px = density * zoom.
        @Suppress("DEPRECATION")
        val k = resources.displayMetrics.density * (wv.scale / resources.displayMetrics.density).let { if (it > 0) it else 1f }
        val lp = FrameLayout.LayoutParams((rectCss.optDouble("w", 0.0) * k).toInt().coerceAtLeast(1),
            (rectCss.optDouble("h", 0.0) * k).toInt().coerceAtLeast(1))
        lp.leftMargin = (wv.left + rectCss.optDouble("x", 0.0) * k).toInt()
        lp.topMargin = (wv.top + rectCss.optDouble("y", 0.0) * k).toInt()
        if (captureOverlay !== view) {
            captureOverlay?.let { root.removeView(it) }
            root.addView(view, lp)
            captureOverlay = view
        } else view.layoutParams = lp
    }

    override fun setScreenFlash(on: Boolean) {
        window.attributes = window.attributes.apply {
            screenBrightness = if (on) 1f else WindowManager.LayoutParams.BRIGHTNESS_OVERRIDE_NONE
        }
        root.foreground = if (on) android.graphics.drawable.ColorDrawable(Color.WHITE) else null
    }

    // --- lifecycle ---------------------------------------------------------------------------------

    override fun onResume() {
        super.onResume()
        EmberRuntime.uiVisible = true
        if (EmberRuntime.isStarted) {
            EmberRuntime.hostContext.ui = this
            EmberRuntime.retryPermissionBlockedLinks()
        }
        webView?.onResume()
        if (pageLoaded) evalJs("window.__emberBridge&&window.__emberBridge.onEvent(${Json.jsSafe(BridgeDispatcher.eventJson("host.app.visible", JSONObject().put("visible", true)))});")
    }

    override fun onPause() {
        EmberRuntime.uiVisible = false
        webView?.onPause()
        super.onPause()
    }

    // JS timers pause only while the app is NOT visible (onStop), never on onPause: a permission
    // dialog or another app in split screen pauses the activity while the page must keep loading.
    override fun onStart() {
        super.onStart()
        webView?.resumeTimers()
    }

    override fun onStop() {
        webView?.pauseTimers() // no UI polling while hidden (battery); events still reach the page
        super.onStop()
    }

    override fun onDestroy() {
        EmberRuntime.eventListeners.remove(eventListener)
        if (EmberRuntime.isStarted && EmberRuntime.hostContext.ui === this) EmberRuntime.hostContext.ui = null
        if (Build.VERSION.SDK_INT >= 33) {
            (backCallback as? android.window.OnBackInvokedCallback)?.let { onBackInvokedDispatcher.unregisterOnBackInvokedCallback(it) }
        }
        fileCallback?.onReceiveValue(null)
        webView?.let { root.removeView(it); it.destroy() }
        webView = null
        // Without the background service the node stops with the UI.
        if (isFinishing && EmberRuntime.isStarted && !EmberRuntime.prefs.runInBackground) EmberRuntime.stop()
        super.onDestroy()
    }

    private fun askNotificationPermission() {
        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS)) { ok -> if (ok) MeshServiceControlPoke.poke(this) }
        }
    }

    private fun showFatal(msg: String) {
        root.addView(TextView(this).apply {
            text = msg
            setTextColor(Color.WHITE)
            setPadding(48, 48, 48, 48)
            textSize = 18f
        })
    }

    /** Test hook: the live WebView (instrumented tests). */
    fun webViewForTest(): WebView? = webView

    companion object {
        private const val TAG = "EmberUI"
        const val ACTION_SHARE_APP = "org.embermesh.app.SHARE_APP"
        const val EXTRA_DIAGNOSTICS = "diagnostics"
        const val ACTION_DIAGNOSTICS = "org.embermesh.app.DIAGNOSTICS"
        /** Brand background (docs/assets/logo.svg): window, splash, web view and launch overlay. */
        val BG = Color.rgb(0x17, 0x11, 0x0f)
    }
}

private object MeshServiceControlPoke {
    fun poke(ctx: android.content.Context) = org.embermesh.app.service.MeshService.poke(ctx)
}
