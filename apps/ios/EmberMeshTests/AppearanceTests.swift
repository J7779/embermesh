import WebKit
import XCTest
@testable import EmberMesh

final class AccessibilityHintTests: XCTestCase {
  func testFontScaleMapping() {
    XCTAssertEqual(AccessibilityMonitor.fontScale(.large), 1.0)
    XCTAssertEqual(AccessibilityMonitor.fontScale(.extraSmall), 0.82)
    XCTAssertEqual(AccessibilityMonitor.fontScale(.extraExtraExtraLarge), 1.35)
    XCTAssertEqual(AccessibilityMonitor.fontScale(.accessibilityExtraExtraExtraLarge), 3.12)
    XCTAssertEqual(AccessibilityMonitor.fontScale(.unspecified), 1.0)
    let all: [UIContentSizeCategory] = [.extraSmall, .small, .medium, .large, .extraLarge, .extraExtraLarge, .extraExtraExtraLarge,
                                        .accessibilityMedium, .accessibilityLarge, .accessibilityExtraLarge,
                                        .accessibilityExtraExtraLarge, .accessibilityExtraExtraExtraLarge]
    let scales = all.map(AccessibilityMonitor.fontScale)
    XCTAssertEqual(scales, scales.sorted(), "monotonic")
  }

  func testValuesAndChangeNotifications() {
    let center = NotificationCenter()
    let m = AccessibilityMonitor(center: center)
    var snap = AccessibilityMonitor.Snapshot(reduceMotion: false, category: .large, screenReader: false, highContrast: false, boldText: false)
    m.provider = { snap }
    var events: [JSONObject] = []
    m.onChange = { events.append($0) }
    let s0 = m.state
    XCTAssertEqual(s0.bool("reduce_motion"), false)
    XCTAssertEqual(s0.number("font_scale"), 1.0)
    XCTAssertEqual(Set(s0.keys), ["reduce_motion", "font_scale", "screen_reader", "high_contrast", "bold_text"])
    // Each platform notification re-reads the state; an event is sent only when something changed.
    center.post(name: UIAccessibility.reduceMotionStatusDidChangeNotification, object: nil)
    XCTAssertTrue(events.isEmpty)
    snap.reduceMotion = true
    center.post(name: UIAccessibility.reduceMotionStatusDidChangeNotification, object: nil)
    snap.category = .accessibilityLarge
    center.post(name: UIContentSizeCategory.didChangeNotification, object: nil)
    snap.screenReader = true
    center.post(name: UIAccessibility.voiceOverStatusDidChangeNotification, object: nil)
    snap.highContrast = true
    center.post(name: UIAccessibility.darkerSystemColorsStatusDidChangeNotification, object: nil)
    snap.boldText = true
    center.post(name: UIAccessibility.boldTextStatusDidChangeNotification, object: nil)
    XCTAssertEqual(events.count, 5)
    XCTAssertEqual(events[0].bool("reduce_motion"), true)
    XCTAssertEqual(events[1].number("font_scale"), 1.94)
    XCTAssertEqual(events[2].bool("screen_reader"), true)
    XCTAssertEqual(events[3].bool("high_contrast"), true)
    XCTAssertEqual(events[4].bool("bold_text"), true)
  }

  func testHostInfoCarriesAccessibilityAndEventsReachTheUI() throws {
    let n = TestNode(name: "a11y")
    n.start()
    let info = try XCTUnwrap(n.host("host.info")).get()
    let a = try XCTUnwrap(info.object("accessibility"))
    XCTAssertNotNil(a.number("font_scale")); XCTAssertNotNil(a.bool("reduce_motion"))
    XCTAssertTrue((info["features"] as? [String] ?? []).contains("accessibility"))
    var got: [(String, JSONObject)] = []
    n.host.onEvent = { got.append(($0, $1)) }
    var snap = AccessibilityMonitor.Snapshot.current()
    snap.reduceMotion.toggle()
    n.host.accessibility.provider = { snap }
    NotificationCenter.default.post(name: UIAccessibility.reduceMotionStatusDidChangeNotification, object: nil)
    XCTAssertEqual(got.first?.0, "host.accessibility")
    XCTAssertEqual(got.first?.1.bool("reduce_motion"), snap.reduceMotion)
    n.stop()
  }
}

final class HapticTests: XCTestCase {
  final class Rec: HapticPerformer {
    var log: [String] = []
    func impact() { log.append("impact") }
    func notify(_ t: UINotificationFeedbackGenerator.FeedbackType) { log.append(["success", "warning", "error"][t.rawValue]) }
  }

  func testRouting() {
    let h = HapticService()
    let r = Rec()
    h.performer = r
    h.supported = true
    for k in ["tap", "success", "warning", "error"] { XCTAssertNotNil(try? h.perform(["kind": k]).get(), k) }
    XCTAssertNotNil(try? h.perform([:]).get(), "kind defaults to tap")
    XCTAssertEqual(r.log, ["impact", "success", "warning", "error", "impact"])
    guard case .failure(let e) = h.perform(["kind": "buzz"]) else { return XCTFail() }
    XCTAssertEqual(e.code, "bad_params")
    h.supported = false   // iPad: no Taptic Engine
    guard case .failure(let u) = h.perform(["kind": "tap"]) else { return XCTFail() }
    XCTAssertEqual(u.code, "unsupported")
    XCTAssertEqual(r.log.count, 5)
  }

  func testHostRouterMethod() throws {
    let n = TestNode(name: "haptic")
    n.start()
    let r = Rec()
    n.host.haptics.performer = r
    let res = try XCTUnwrap(n.host("host.haptic", ["kind": "success"]))
    if UIDevice.current.userInterfaceIdiom == .phone {
      XCTAssertNotNil(try? res.get()); XCTAssertEqual(r.log, ["success"])
      XCTAssertTrue((try n.host("host.info")!.get()["features"] as? [String] ?? []).contains("haptic"))
    } else {
      guard case .failure(let e) = res else { return XCTFail() }
      XCTAssertEqual(e.code, "unsupported")
    }
    n.stop()
  }
}

/// The native launch placeholder over the web view: removed at the page's first paint, never left
/// covering the app (failed start page → diagnostic page; hard timeout), no timers left behind.
final class LaunchPlaceholderTests: XCTestCase {
  var window: UIWindow!

  override func setUp() {
    let scene = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first!
    window = UIWindow(windowScene: scene)
    window.frame = scene.coordinateSpace.bounds
  }
  override func tearDown() {
    window.isHidden = true
    window = nil
    WebViewController.revealTimeout = 6
  }

  func present(startURL: URL? = nil) throws -> WebViewController {
    let env = try XCTUnwrap(AppEnvironment.shared)
    let vc = WebViewController(webHost: env.makeWebHost(), forceDiagnostics: false, startURL: startURL)
    window.rootViewController = vc
    window.makeKeyAndVisible()
    return vc
  }

  func testRemovedAtFirstPaintWithMatchingGeometry() throws {
    let t0 = Date()
    let vc = try present()
    weak let ph = vc.placeholder
    XCTAssertNotNil(ph, "shown from the first moment")
    XCTAssertEqual(ph?.accessibilityElementsHidden, true)
    XCTAssertEqual(ph?.isAccessibilityElement, false)
    // Same geometry as the launch storyboard: centred, 30 % of the shorter side.
    vc.view.layoutIfNeeded()
    let expected = LaunchPlaceholderView.logoRect(in: vc.view.bounds.size)
    let actual = ph!.convert(ph!.logo.frame, to: vc.view)
    XCTAssertEqual(actual.width, expected.width, accuracy: 1); XCTAssertEqual(actual.midX, expected.midX, accuracy: 1)
    XCTAssertEqual(actual.midY, expected.midY, accuracy: 1)
    XCTAssertEqual(vc.webViewForTesting?.isOpaque, false)
    XCTAssertEqual(vc.webViewForTesting?.backgroundColor, Brand.background)
    XCTAssertEqual(vc.webViewForTesting?.scrollView.backgroundColor, Brand.background)
    XCTAssertTrue(spin(until: 10) { vc.placeholder == nil })
    let ms = Int(Date().timeIntervalSince(t0) * 1000)
    print("placeholder removed after \(ms) ms (\(vc.revealReason ?? "?"))")
    XCTAssertEqual(vc.revealReason, "painted")
    XCTAssertFalse(vc.revealPending, "timeout cancelled: no native timer keeps running")
    XCTAssertNil(ph?.superview, "placeholder detached from the view tree")
    // The page received where the logo is (CSS px of its viewport).
    var logo: [String: Any]?
    vc.webViewForTesting?.evaluateJavaScript("window.__emberHost && window.__emberHost.launchLogo") { v, _ in logo = v as? [String: Any] }
    XCTAssertTrue(spin(until: 3) { logo != nil })
    let r = vc.launchLogoRectInWebView()
    XCTAssertEqual((logo?["size"] as? Double) ?? 0, Double(r.width), accuracy: 0.5)
    XCTAssertEqual((logo?["y"] as? Double) ?? 0, Double(r.minY), accuracy: 0.5)
  }

  func testFailedStartPageRevealsDiagnostics() throws {
    let vc = try present(startURL: URL(string: "embermesh://app/no-such-page.html")!)
    XCTAssertTrue(spin(until: 10) { vc.placeholder == nil })
    XCTAssertTrue(vc.showingDiagnostics)
    XCTAssertTrue(spin(until: 5) { vc.webViewForTesting?.url?.path.hasPrefix("/__diag/") == true })
    print("failed start page → \(vc.webViewForTesting?.url?.absoluteString ?? "?"), placeholder removed (\(vc.revealReason ?? "?"))")
  }

  func testTimeoutNeverLeavesTheAppCovered() throws {
    WebViewController.revealTimeout = 0.01  // fires before any page can paint
    let vc = try present()
    XCTAssertTrue(spin(until: 3) { vc.placeholder == nil })
    XCTAssertEqual(vc.revealReason, "timeout")
    XCTAssertFalse(vc.revealPending)
  }
}

/// Frame pacing of CSS animation in our WKWebView (rAF deltas). In the simulator the page is drawn
/// by the Mac's GPU, so this checks that nothing in the shell throttles or stalls the web view; it
/// says nothing about a 2015 iPhone.
final class WebFramePacingTests: XCTestCase {
  func testAnimationFramePacing() throws {
    let env = try XCTUnwrap(AppEnvironment.shared)
    let host = env.makeWebHost()
    let wv = WKWebView(frame: CGRect(x: 0, y: 0, width: 390, height: 844), configuration: host.makeConfiguration())
    host.attach(wv)
    let win = UIApplication.shared.connectedScenes.compactMap { ($0 as? UIWindowScene)?.windows.first }.first
    win?.addSubview(wv)
    defer { wv.removeFromSuperview() }
    final class Nav: NSObject, WKNavigationDelegate { var done = false; func webView(_ w: WKWebView, didFinish n: WKNavigation!) { done = true } }
    let nav = Nav(); wv.navigationDelegate = nav
    wv.load(URLRequest(url: host.schemeHandler.startURL(forceDiagnostics: true)))
    XCTAssertTrue(spin(until: 15) { nav.done })
    var result: [String: Any]?
    let js = """
      const d = document.createElement('div');
      d.style.cssText = 'position:fixed;left:40%;top:40%;width:120px;height:120px;background:linear-gradient(#ff8a3d,#e8471c);border-radius:24px;will-change:transform';
      document.body.appendChild(d);
      d.animate([{transform:'perspective(600px) rotateY(0deg) scale(1)', opacity:1},{transform:'perspective(600px) rotateY(180deg) scale(1.4)', opacity:.6}],
                {duration: 1000, iterations: Infinity, direction: 'alternate'});
      const ts = [];
      await new Promise(res => { const f = t => { ts.push(t); if (ts.length < 181) requestAnimationFrame(f); else res(); }; requestAnimationFrame(f); });
      const dt = ts.slice(1).map((t, i) => t - ts[i]).sort((a, b) => a - b);
      const mean = dt.reduce((a, b) => a + b, 0) / dt.length;
      const frame = dt[Math.floor(dt.length / 2)];
      return {frames: dt.length, median: frame, mean, p95: dt[Math.floor(dt.length * 0.95)], max: dt[dt.length - 1], late: dt.filter(x => x > frame * 1.5).length};
      """
    wv.callAsyncJavaScript(js, arguments: [:], in: nil, in: .page) { r in result = (try? r.get()) as? [String: Any] ?? ["error": "\(r)"] }
    XCTAssertTrue(spin(until: 20) { result != nil })
    print("frame pacing (simulator): \(result ?? [:])")
    let median = (result?["median"] as? Double) ?? 0
    let late = (result?["late"] as? Int) ?? 999
    XCTAssertGreaterThan(median, 0)
    XCTAssertLessThan(median, 34, "at least 30 fps")
    XCTAssertLessThanOrEqual(late, 9, "≤ 5 % of frames late")
  }
}

final class ThemeStatusBarTests: XCTestCase {
  final class FakeCore: CoreCaller {
    var theme = "light"
    func callCore(method: String, paramsJSON: String, completion: @escaping (String) -> Void) {
      completion(#"{"ok":true,"result":{"settings":{"theme":"\#(theme)","region":"US"}}}"#)
    }
  }

  func testBridgeReportsThemeAndStatusBarFollows() throws {
    let core = FakeCore()
    let r = BridgeRouter(core: core, host: nil, evaluate: { _ in })
    var themes: [String] = []
    r.onTheme = { themes.append($0) }
    r.handle(body: #"{"id":1,"method":"settings.get","params":{}}"#)
    core.theme = "night"
    r.handle(body: #"{"id":2,"method":"settings.set","params":{"settings":{"theme":"night"}}}"#)
    r.handle(body: #"{"id":3,"method":"node.info","params":{}}"#)   // other methods: no theme parsing
    XCTAssertEqual(themes, ["light", "night"])

    let env = try XCTUnwrap(AppEnvironment.shared)
    let vc = WebViewController(webHost: env.makeWebHost(), forceDiagnostics: false)
    XCTAssertEqual(vc.preferredStatusBarStyle, .lightContent, "launch colour is dark")
    vc.applyTheme("light")
    XCTAssertEqual(vc.preferredStatusBarStyle, .darkContent)
    vc.applyTheme("night")
    XCTAssertEqual(vc.preferredStatusBarStyle, .lightContent)
  }
}
