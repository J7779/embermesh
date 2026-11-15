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

  /// RPC_API §8 / ui/README.md "Launch screen": every cold start plays the FULL start-up animation,
  /// the same as the web, although the native core makes the app ready long before it ends. Frame 0
  /// comes only after the native placeholder is gone, and the whole minimum plays from there. Run
  /// twice: the second start (preferences and storage already written) is still full. Also full when
  /// the system's Reduce Motion is on (owner decision; only the app's own settings change it) — the
  /// page reports what it saw (`osReduce`), checked against UIAccessibility; run it both ways.
  func testEveryColdStartPlaysTheFullAnimationFromFrameZero() throws {
    for launch in 1...2 {
      let vc = try present()
      XCTAssertTrue(spin(until: 10) { vc.placeholder == nil }, "launch \(launch): placeholder removed")
      XCTAssertEqual(vc.revealReason, "painted")
      var boot: [String: Any]?
      let js = "JSON.stringify(window.__emberBoot ? {variant: __emberBoot.variant, active: __emberBoot.active, reason: __emberBoot.reason, canvas: __emberBoot.canvas, osReduce: __emberBoot.osReduce, at: __emberBoot.at} : null)"
      XCTAssertTrue(spin(until: 10) {
        vc.webViewForTesting?.evaluateJavaScript(js) { v, _ in
          if let s = v as? String, let d = s.data(using: .utf8) { boot = try? JSONSerialization.jsonObject(with: d) as? [String: Any] }
        }
        return (boot?["active"] as? Bool) == false
      }, "launch \(launch): the animation finished by itself")
      let at = (boot?["at"] as? [String: Double]) ?? [:]
      let reduce = UIAccessibility.isReduceMotionEnabled
      print("launch \(launch) start-up animation (system Reduce Motion \(reduce ? "ON" : "off")): \(boot ?? [:])")
      XCTAssertEqual(boot?["osReduce"] as? Bool, reduce, "the page sees the system Reduce Motion setting")
      XCTAssertEqual(boot?["variant"] as? String, "full", "launch \(launch): full animation by default, Reduce Motion \(reduce ? "on" : "off")")
      XCTAssertEqual(boot?["canvas"] as? Bool, true, "WKWebView has a 2D canvas")
      XCTAssertEqual(boot?["reason"] as? String, "ready", "launch \(launch): handed over by itself")
      let frame0 = at["frame0"] ?? 0, removed = at["removed"] ?? -1, out = at["out"] ?? 0, ready = at["ready"] ?? .infinity
      XCTAssertGreaterThanOrEqual(frame0, removed, "frame 0 is not drawn behind the native placeholder")
      XCTAssertLessThan(frame0 - removed, 120, "frame 0 follows the placeholder's removal at once")
      XCTAssertGreaterThanOrEqual(out - frame0, 2400 - 20, "launch \(launch): the whole 2.4 s animation played")
      XCTAssertLessThan(ready, out, "the app was ready before the animation ended (it did not wait for the app)")
      window.rootViewController = nil
    }
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
/// says nothing about a 2015 iPhone. The control is a plain WKWebView (default configuration, no
/// host attached) measured in the same window on the same machine: the test fails only when ours
/// misses the limits in every round while the plain one meets them. When the plain one misses them
/// too (a loaded machine, a VM without a GPU) the measurement says nothing about the shell and the
/// test is SKIPPED with the numbers, never passed.
final class WebFramePacingTests: XCTestCase {
  struct Pacing: CustomStringConvertible {
    var median = 0.0, p95 = 0.0, max = 0.0, late = 999, frames = 0
    var ok: Bool { median > 0 && median < 34 && late <= 9 }   // at least 30 fps, ≤ 5 % of frames late
    var description: String { "median \(median) ms, p95 \(p95) ms, max \(max) ms, \(late)/\(frames) late" }
  }

  static let script = """
    const d = document.createElement('div');
    d.style.cssText = 'position:fixed;left:40%;top:40%;width:120px;height:120px;background:linear-gradient(#ff8a3d,#e8471c);border-radius:24px;will-change:transform';
    document.body.appendChild(d);
    d.animate([{transform:'perspective(600px) rotateY(0deg) scale(1)', opacity:1},{transform:'perspective(600px) rotateY(180deg) scale(1.4)', opacity:.6}],
              {duration: 1000, iterations: Infinity, direction: 'alternate'});
    const ts = [];
    await new Promise(res => { const f = t => { ts.push(t); if (ts.length < 181) requestAnimationFrame(f); else res(); }; requestAnimationFrame(f); });
    d.remove();
    const dt = ts.slice(1).map((t, i) => t - ts[i]).sort((a, b) => a - b);
    const frame = dt[Math.floor(dt.length / 2)];
    return {frames: dt.length, median: frame, p95: dt[Math.floor(dt.length * 0.95)], max: dt[dt.length - 1], late: dt.filter(x => x > frame * 1.5).length};
    """

  final class Nav: NSObject, WKNavigationDelegate {
    var done = false
    func webView(_ w: WKWebView, didFinish n: WKNavigation!) { done = true }
  }

  /// 180 animation frames in `wv`, which is put in front for the measurement.
  func measure(_ wv: WKWebView, in win: UIWindow?) -> Pacing {
    win?.addSubview(wv)
    defer { wv.removeFromSuperview() }
    var result: [String: Any]?
    wv.callAsyncJavaScript(Self.script, arguments: [:], in: nil, in: .page) { r in result = (try? r.get()) as? [String: Any] ?? ["error": "\(r)"] }
    XCTAssertTrue(spin(until: 30) { result != nil }, "the page finished measuring")
    var p = Pacing()
    p.median = (result?["median"] as? Double) ?? 0
    p.p95 = (result?["p95"] as? Double) ?? 0
    p.max = (result?["max"] as? Double) ?? 0
    p.late = (result?["late"] as? Int) ?? 999
    p.frames = (result?["frames"] as? Int) ?? 0
    return p
  }

  func testAnimationFramePacing() throws {
    let env = try XCTUnwrap(AppEnvironment.shared)
    let host = env.makeWebHost()
    let frame = CGRect(x: 0, y: 0, width: 390, height: 844)
    let win = UIApplication.shared.connectedScenes.compactMap { ($0 as? UIWindowScene)?.windows.first }.first
    let ours = WKWebView(frame: frame, configuration: host.makeConfiguration())
    host.attach(ours)
    let plain = WKWebView(frame: frame, configuration: WKWebViewConfiguration())
    let navOurs = Nav(), navPlain = Nav()
    ours.navigationDelegate = navOurs; plain.navigationDelegate = navPlain
    win?.addSubview(ours); win?.addSubview(plain)
    ours.load(URLRequest(url: host.schemeHandler.startURL(forceDiagnostics: true)))
    plain.loadHTMLString("<!doctype html><meta name=viewport content='width=device-width'><body></body>", baseURL: nil)
    XCTAssertTrue(spin(until: 15) { navOurs.done && navPlain.done })
    ours.removeFromSuperview(); plain.removeFromSuperview()

    var oursRounds: [Pacing] = [], plainRounds: [Pacing] = []
    for round in 1...3 {
      let o = measure(ours, in: win)
      oursRounds.append(o)
      print("frame pacing (simulator), round \(round), our web view: \(o)")
      if o.ok { return }
      let c = measure(plain, in: win)
      plainRounds.append(c)
      print("frame pacing (simulator), round \(round), plain WKWebView: \(c)")
    }
    let numbers = "ours [\(oursRounds.map(\.description).joined(separator: "; "))], plain WKWebView [\(plainRounds.map(\.description).joined(separator: "; "))]; needs median < 34 ms and ≤ 9/180 late"
    guard plainRounds.contains(where: \.ok) else {
      throw XCTSkip("this machine does not pace a plain WKWebView either, so nothing can be said about the shell: \(numbers)")
    }
    XCTFail("our web view missed the pacing limits in every round while a plain WKWebView on the same machine met them: \(numbers)")
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
