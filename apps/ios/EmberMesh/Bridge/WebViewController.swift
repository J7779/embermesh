import UIKit
import WebKit

/// Hosts the shared web UI full screen: dark, safe-area aware, keyboard aware, iPad multitasking
/// friendly. Also the presenter for native sheets (QR scanner, share sheet, document picker).
final class WebViewController: UIViewController, WKNavigationDelegate, WKUIDelegate, Presenter {
  static let background = Brand.background  // #17110f, same as the launch screen
  /// The placeholder is removed at the page's first paint, or after this long whatever happens.
  static var revealTimeout: TimeInterval = 6

  let webHost: WebHost
  private var webView: WKWebView!
  private let forceDiagnostics: Bool
  var webViewForTesting: WKWebView? { webView }
  var revealPending: Bool { revealTimer != nil }
  /// Native still logo over the web view until the page has painted (nil afterwards).
  private(set) var placeholder: LaunchPlaceholderView?
  private var revealTimer: DispatchWorkItem?
  private(set) var showingDiagnostics = false
  private(set) var revealReason: String?
  var statusBarStyle: UIStatusBarStyle = .lightContent {
    didSet { if oldValue != statusBarStyle { setNeedsStatusBarAppearanceUpdate() } }
  }

  /// Tests: start somewhere else (e.g. a missing page) instead of the bundled UI.
  private let startOverride: URL?

  init(webHost: WebHost, forceDiagnostics: Bool, startURL: URL? = nil) {
    self.webHost = webHost
    self.forceDiagnostics = forceDiagnostics
    self.startOverride = startURL
    super.init(nibName: nil, bundle: nil)
  }

  required init?(coder: NSCoder) { fatalError("init(coder:) is not used") }

  override var preferredStatusBarStyle: UIStatusBarStyle { statusBarStyle }

  /// Status bar per UI theme (`theme` setting: dark / night → light text, light → dark text).
  func applyTheme(_ theme: String) {
    statusBarStyle = theme == "light" ? .darkContent : .lightContent
  }

  override func loadView() {
    let root = UIView()
    root.backgroundColor = WebViewController.background
    view = root
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    overrideUserInterfaceStyle = .dark
    let config = webHost.makeConfiguration()
    let wv = WKWebView(frame: view.bounds, configuration: config)
    webHost.attach(wv)
    wv.navigationDelegate = self
    wv.uiDelegate = self
    // Never white: the web view, its scroll view and the area under the page are the launch colour
    // from the first moment (cold start, rotation, return from background, overscroll).
    wv.isOpaque = false
    wv.backgroundColor = WebViewController.background
    wv.scrollView.backgroundColor = WebViewController.background
    wv.underPageBackgroundColor = WebViewController.background
    wv.scrollView.contentInsetAdjustmentBehavior = .never  // we lay the view out inside the safe area
    wv.allowsBackForwardNavigationGestures = false
    wv.allowsLinkPreview = false
    wv.translatesAutoresizingMaskIntoConstraints = false
    view.addSubview(wv)
    // Safe area on top/sides; bottom follows the keyboard (like Android adjustResize) so the
    // composer stays visible. keyboardLayoutGuide equals the safe-area bottom when hidden.
    NSLayoutConstraint.activate([
      wv.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
      wv.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor),
      wv.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor),
      wv.bottomAnchor.constraint(equalTo: view.keyboardLayoutGuide.topAnchor),
    ])
    webView = wv
    showPlaceholder()
    webHost.onPainted = { [weak self] path in DispatchQueue.main.async { self?.pagePainted(path) } }
    webHost.router.onTheme = { [weak self] theme in DispatchQueue.main.async { self?.applyTheme(theme) } }
    WebHost.installContentRules(on: config) { [weak self] in self?.loadStart() }
  }

  // MARK: launch placeholder → web boot animation

  private func showPlaceholder() {
    let p = LaunchPlaceholderView(frame: view.bounds)
    p.translatesAutoresizingMaskIntoConstraints = false
    view.addSubview(p)  // over the whole window, like the launch screen (not just the safe area)
    NSLayoutConstraint.activate([
      p.topAnchor.constraint(equalTo: view.topAnchor), p.bottomAnchor.constraint(equalTo: view.bottomAnchor),
      p.leadingAnchor.constraint(equalTo: view.leadingAnchor), p.trailingAnchor.constraint(equalTo: view.trailingAnchor),
    ])
    placeholder = p
  }

  private func pagePainted(_ path: String) {
    LaunchClock.mark("first paint (\(path))")
    removePlaceholder(reason: "painted")
  }

  /// Instant removal (no fade, also with Reduce Motion): the boot animation's first frame shows the
  /// same logo at the same place, so nothing visibly changes.
  func removePlaceholder(reason: String) {
    revealTimer?.cancel()
    revealTimer = nil
    guard let p = placeholder else { return }
    p.removeFromSuperview()
    placeholder = nil
    revealReason = reason
    LaunchClock.mark("placeholder removed (\(reason))")
    UIAccessibility.post(notification: .screenChanged, argument: webView)
  }

  /// Logo rect in the web view's coordinates (= CSS px of the page viewport).
  func launchLogoRectInWebView() -> CGRect {
    view.layoutIfNeeded()
    return view.convert(LaunchPlaceholderView.logoRect(in: view.bounds.size), to: webView)
  }

  private func loadDiagnostics(_ why: String) {
    guard !showingDiagnostics else { return }
    showingDiagnostics = true
    emberLog("start page failed (\(why)); showing the diagnostic page")
    webHost.pageWillLoad()
    webView.load(URLRequest(url: webHost.schemeHandler.startURL(forceDiagnostics: true)))
  }

  private func loadStart() {
    let url = startOverride ?? webHost.schemeHandler.startURL(forceDiagnostics: forceDiagnostics)
    emberLog("loading \(url.absoluteString)")
    webView.configuration.userContentController.addUserScript(
      WKUserScript(source: WebHost.launchLogoScript(launchLogoRectInWebView()), injectionTime: .atDocumentStart, forMainFrameOnly: true))
    if placeholder != nil, revealTimer == nil {
      // Never leave the app covered: reveal after the timeout whatever happened, and fall back to
      // the diagnostic page if the start page has not loaded by then.
      let item = DispatchWorkItem { [weak self] in
        guard let self else { return }
        self.revealTimer = nil
        if self.webView.isLoading || self.webView.url == nil { self.loadDiagnostics("timeout") }
        self.removePlaceholder(reason: "timeout")
      }
      revealTimer = item
      DispatchQueue.main.asyncAfter(deadline: .now() + WebViewController.revealTimeout, execute: item)
    }
    webHost.pageWillLoad()
    webView.load(URLRequest(url: url))
  }

  // MARK: WKNavigationDelegate — no remote content

  func webView(_ webView: WKWebView, decidePolicyFor action: WKNavigationAction,
               decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
    guard let url = action.request.url else { decisionHandler(.cancel); return }
    switch url.scheme?.lowercased() {
    case AppSchemeHandler.scheme, "about", "data", "blob":
      decisionHandler(.allow)
    case "http", "https", "mailto", "tel", "sms", "geo", "maps":
      // Never load remote pages inside the app. A user-tapped link (e.g. a hub URL) goes to Safari.
      if action.navigationType == .linkActivated { UIApplication.shared.open(url) }
      decisionHandler(.cancel)
    case "ember":
      // Contact cards / invites tapped inside the UI: hand back to the UI as an event.
      AppEnvironment.shared?.handleOpenURL(url)
      decisionHandler(.cancel)
    default:
      decisionHandler(.cancel)
    }
  }

  /// A start page that answers with an error (missing file) shows the diagnostic page instead.
  func webView(_ webView: WKWebView, decidePolicyFor response: WKNavigationResponse,
               decisionHandler: @escaping (WKNavigationResponsePolicy) -> Void) {
    let status = (response.response as? HTTPURLResponse)?.statusCode ?? 0
    if response.isForMainFrame, !showingDiagnostics, status >= 400 || webHost.schemeHandler.wasNotFound(response.response.url) {
      decisionHandler(.cancel)
      let path = response.response.url?.path ?? "?"
      DispatchQueue.main.async { self.loadDiagnostics("not found: \(path)") }
      return
    }
    decisionHandler(.allow)
  }

  func webView(_ webView: WKWebView, didStartProvisionalNavigation navigation: WKNavigation!) {
    webHost.pageWillLoad()
  }

  func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
    LaunchClock.mark("page loaded")
    webHost.pageDidLoad()
  }

  func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) {
    emberLog("navigation failed: \(error.localizedDescription)")
    loadDiagnostics(error.localizedDescription)
  }

  func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) {
    emberLog("navigation failed to start: \(error.localizedDescription)")
    loadDiagnostics(error.localizedDescription)
  }

  func webViewWebContentProcessDidTerminate(_ webView: WKWebView) {
    emberLog("web content process terminated; reloading")
    loadStart()
  }

  // MARK: WKUIDelegate — JS dialogs would otherwise be silently dropped

  func webView(_ webView: WKWebView, runJavaScriptAlertPanelWithMessage message: String,
               initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping () -> Void) {
    let a = UIAlertController(title: nil, message: message, preferredStyle: .alert)
    a.addAction(UIAlertAction(title: "OK", style: .default) { _ in completionHandler() })
    presentSafely(a) { completionHandler() }
  }

  func webView(_ webView: WKWebView, runJavaScriptConfirmPanelWithMessage message: String,
               initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping (Bool) -> Void) {
    let a = UIAlertController(title: nil, message: message, preferredStyle: .alert)
    a.addAction(UIAlertAction(title: "Cancel", style: .cancel) { _ in completionHandler(false) })
    a.addAction(UIAlertAction(title: "OK", style: .default) { _ in completionHandler(true) })
    presentSafely(a) { completionHandler(false) }
  }

  func webView(_ webView: WKWebView, runJavaScriptTextInputPanelWithPrompt prompt: String, defaultText: String?,
               initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping (String?) -> Void) {
    let a = UIAlertController(title: nil, message: prompt, preferredStyle: .alert)
    a.addTextField { $0.text = defaultText }
    a.addAction(UIAlertAction(title: "Cancel", style: .cancel) { _ in completionHandler(nil) })
    a.addAction(UIAlertAction(title: "OK", style: .default) { _ in completionHandler(a.textFields?.first?.text) })
    presentSafely(a) { completionHandler(nil) }
  }

  // MARK: Presenter

  func present(_ vc: UIViewController, sourceRectCenter: Bool) {
    if let pop = vc.popoverPresentationController {  // iPad: share sheets need an anchor
      pop.sourceView = view
      pop.sourceRect = CGRect(x: view.bounds.midX, y: view.bounds.midY, width: 1, height: 1)
      pop.permittedArrowDirections = []
    }
    presentSafely(vc) {}
  }

  var hostView: UIView { view }
  var webContentView: UIView? { webView }

  // MARK: WKUIDelegate — media capture (iOS 15+)

  func webView(_ webView: WKWebView, requestMediaCapturePermissionFor origin: WKSecurityOrigin, initiatedByFrame frame: WKFrameInfo,
               type: WKMediaCaptureType, decisionHandler: @escaping (WKPermissionDecision) -> Void) {
    let d = MediaCapturePolicy.decision(scheme: origin.protocol, host: origin.host)
    emberLog("media capture \(type.rawValue) for \(origin.protocol)://\(origin.host): \(d == .grant ? "granted" : "denied")")
    decisionHandler(d)
  }

  private func presentSafely(_ vc: UIViewController, failed: @escaping () -> Void) {
    var top: UIViewController = self
    while let p = top.presentedViewController { top = p }
    if top.isBeingDismissed || view.window == nil { failed(); return }
    top.present(vc, animated: true)
  }
}

/// Something that can show native UI on behalf of host.* methods.
protocol Presenter: AnyObject {
  func present(_ vc: UIViewController, sourceRectCenter: Bool)
  var hostView: UIView { get }
  /// The web view (native overlays such as the Morse camera preview are placed in its coordinates,
  /// which equal CSS px of the page viewport).
  var webContentView: UIView? { get }
}

/// getUserMedia in our WKWebView (camera / microphone for the Morse reader, microphone for sound
/// links in the web UI): the bundled UI on embermesh://app is granted without WebKit's second,
/// per-page prompt — iOS still shows its own one-time Camera / Microphone permission prompt. Any
/// other origin (there should be none: remote content is blocked) is denied.
enum MediaCapturePolicy {
  static func decision(scheme: String, host: String) -> WKPermissionDecision {
    scheme == AppSchemeHandler.scheme && host == AppSchemeHandler.host ? .grant : .deny
  }
}
