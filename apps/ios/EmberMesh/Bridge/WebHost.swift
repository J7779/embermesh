import UIKit
import WebKit

/// Builds WKWebViews wired to the bridge. Shared by the app's view controller and the tests.
final class WebHost: NSObject, WKScriptMessageHandler {
  static let messageHandlerName = "ember"
  /// First-paint signal for the native launch placeholder (see paintedScript).
  static let paintedHandlerName = "emberPainted"
  /// Called on the main queue when the page has painted its first frame (path of the page).
  var onPainted: ((String) -> Void)?

  let schemeHandler: AppSchemeHandler
  let router: BridgeRouter
  private(set) weak var webView: WKWebView?
  /// Native → JS messages produced before the page finished loading.
  private var pending: [String] = []
  private var pageReady = false

  init(content: WebContent, pack: MappedFile?, core: CoreCaller?, host: HostMethodHandler?) {
    schemeHandler = AppSchemeHandler(content: content, pack: pack)
    router = BridgeRouter(core: core, host: host, evaluate: { _ in })
    super.init()
    router.evaluate = { [weak self] js in
      DispatchQueue.main.async { self?.run(js) }
    }
  }

  /// Installs a buffer for onReply/onEvent so messages that arrive before ui/js/backends/native.js
  /// has assigned window.__emberBridge are replayed instead of lost.
  static let bootstrapScript = """
  (function(){
    if (window.__emberBridgeBootstrap) return;
    window.__emberBridgeBootstrap = true;
    var q = [], real = null, handlers = {};
    function stub(name){ var f = function(m){ if (q.length < 2000) q.push([name, m]); }; f.__emberStub = true; return f; }
    handlers.onReply = stub('onReply'); handlers.onEvent = stub('onEvent');
    function target(){ return real || holder; }
    function flush(){
      var rest = [], b = target();
      for (var i = 0; i < q.length; i++) {
        var f = b[q[i][0]];
        if (typeof f === 'function' && !f.__emberStub) { try { f.call(b, q[i][1]); } catch (e) { console.error(e); } }
        else rest.push(q[i]);
      }
      q = rest;
    }
    var holder = {};
    ['onReply', 'onEvent'].forEach(function(n){
      Object.defineProperty(holder, n, { enumerable: true, configurable: true,
        get: function(){ return handlers[n]; },
        set: function(v){ handlers[n] = v; setTimeout(flush, 0); } });
    });
    Object.defineProperty(window, '__emberBridge', { enumerable: true, configurable: true,
      get: function(){ return target(); },
      set: function(v){ real = (v === holder) ? null : v; setTimeout(flush, 0); } });
    window.__emberHost = { platform: 'ios', origin: location.origin };
  })();
  """

  /// Two animation frames after DOMContentLoaded (module scripts, the boot overlay included, have
  /// run by then) the first frame is on screen: tell the shell so it can drop the native placeholder.
  static let paintedScript = """
  (function(){
    function sig(){ requestAnimationFrame(function(){ requestAnimationFrame(function(){
      try { window.webkit.messageHandlers.emberPainted.postMessage(location.pathname); } catch (e) {}
    }); }); }
    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', sig, {once: true}); else sig();
  })();
  """

  /// Where the native launch logo is, in CSS px of this page's viewport, so the boot animation can
  /// start from exactly the still image: window.__emberHost.launchLogo = {x, y, size} and the CSS
  /// variables --launch-logo-x / -y / -size on <html>.
  static func launchLogoScript(_ r: CGRect) -> String {
    let x = String(format: "%.2f", r.minX), y = String(format: "%.2f", r.minY), s = String(format: "%.2f", r.width)
    return """
    (function(){ var h = window.__emberHost || (window.__emberHost = {});
      h.launchLogo = {x: \(x), y: \(y), size: \(s)};
      var st = document.documentElement && document.documentElement.style;
      if (st) { st.setProperty('--launch-logo-x', '\(x)px'); st.setProperty('--launch-logo-y', '\(y)px'); st.setProperty('--launch-logo-size', '\(s)px'); }
    })();
    """
  }

  /// Everything but our own scheme (and inline data/blob) is blocked: no remote content, ever.
  static let blockRemoteRules = """
  [{"trigger":{"url-filter":".*"},"action":{"type":"block"}},
   {"trigger":{"url-filter":"^embermesh:"},"action":{"type":"ignore-previous-rules"}},
   {"trigger":{"url-filter":"^data:"},"action":{"type":"ignore-previous-rules"}},
   {"trigger":{"url-filter":"^blob:"},"action":{"type":"ignore-previous-rules"}},
   {"trigger":{"url-filter":"^about:"},"action":{"type":"ignore-previous-rules"}}]
  """

  func makeConfiguration() -> WKWebViewConfiguration {
    let config = WKWebViewConfiguration()
    config.setURLSchemeHandler(schemeHandler, forURLScheme: AppSchemeHandler.scheme)
    let ucc = WKUserContentController()
    ucc.addUserScript(WKUserScript(source: WebHost.bootstrapScript, injectionTime: .atDocumentStart, forMainFrameOnly: true))
    ucc.add(WeakScriptHandler(self), name: WebHost.messageHandlerName)
    ucc.addUserScript(WKUserScript(source: WebHost.paintedScript, injectionTime: .atDocumentEnd, forMainFrameOnly: true))
    ucc.add(WeakScriptHandler(self), name: WebHost.paintedHandlerName)
    config.userContentController = ucc
    config.allowsInlineMediaPlayback = true      // camera preview <video playsinline> stays in the page
    config.allowsAirPlayForMediaPlayback = false
    config.mediaTypesRequiringUserActionForPlayback = []
    config.preferences.javaScriptCanOpenWindowsAutomatically = false
    config.dataDetectorTypes = []
    config.websiteDataStore = .default()  // localStorage/IndexedDB for UI preferences persist
    return config
  }

  func makeWebView(frame: CGRect = .zero) -> WKWebView {
    let wv = WKWebView(frame: frame, configuration: makeConfiguration())
    attach(wv)
    return wv
  }

  func attach(_ wv: WKWebView) {
    webView = wv
    pageReady = false
    #if DEBUG
    if #available(iOS 16.4, *) { wv.isInspectable = true }
    #endif
  }

  /// Compiles and installs the remote-content block list, then calls `done` (main thread).
  static func installContentRules(on config: WKWebViewConfiguration, done: @escaping () -> Void) {
    guard let store = WKContentRuleListStore.default() else { done(); return }
    store.compileContentRuleList(forIdentifier: "ember-block-remote", encodedContentRuleList: blockRemoteRules) { list, err in
      if let list { config.userContentController.add(list) }
      if let err { emberLog("content rules failed: \(err)") }
      DispatchQueue.main.async(execute: done)
    }
  }

  func pageDidLoad() {
    pageReady = true
    let items = pending
    pending.removeAll()
    for js in items { webView?.evaluateJavaScript(js, completionHandler: nil) }
  }

  func pageWillLoad() { pageReady = false }

  private func run(_ js: String) {
    guard let wv = webView, pageReady else {
      if pending.count < 5000 { pending.append(js) }
      return
    }
    wv.evaluateJavaScript(js) { _, err in
      if let err { emberLog("evaluateJavaScript failed: \(err.localizedDescription)") }
    }
  }

  // MARK: WKScriptMessageHandler

  func userContentController(_ ucc: WKUserContentController, didReceive message: WKScriptMessage) {
    if message.name == WebHost.paintedHandlerName {
      if message.frameInfo.request.url?.scheme == AppSchemeHandler.scheme { onPainted?(message.body as? String ?? "") }
      return
    }
    guard message.name == WebHost.messageHandlerName else { return }
    // Only our own origin may talk to the node.
    if let origin = message.frameInfo.request.url, origin.scheme != AppSchemeHandler.scheme { return }
    router.handle(body: message.body)
  }
}

/// WKUserContentController retains its handlers; this proxy breaks the cycle.
final class WeakScriptHandler: NSObject, WKScriptMessageHandler {
  weak var target: WKScriptMessageHandler?
  init(_ t: WKScriptMessageHandler) { target = t }
  func userContentController(_ ucc: WKUserContentController, didReceive message: WKScriptMessage) {
    target?.userContentController(ucc, didReceive: message)
  }
}
