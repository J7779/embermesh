import WebKit
import XCTest
@testable import EmberMesh

/// Runs against the real static library (libember_core.a) linked into the app.
final class CoreIntegrationTests: XCTestCase {
  var dir: URL!
  override func setUp() {
    dir = FileManager.default.temporaryDirectory.appendingPathComponent("node-\(UUID().uuidString)")
    try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
  }
  override func tearDown() { try? FileManager.default.removeItem(at: dir) }

  func makeNode() -> EmberNode {
    EmberNode(config: ["storage_dir": dir.path, "platform": "ios", "app_version": "test", "device_name": "test",
                       "now_ms": Int64(nowUnixMs())])!
  }

  func testVersionAndEntropy() {
    XCTAssertFalse(EmberNode.version.isEmpty)
    let a = EmberNode.entropy(), b = EmberNode.entropy()
    XCTAssertEqual(a.count, 64)
    XCTAssertNotEqual(a, b)
  }

  func testCreateRequiresEntropy() {
    XCTAssertNil(EmberNode(config: [:], entropy: Data(count: 8)))
  }

  func testNodeInfoAndErrors() throws {
    let n = makeNode()
    let info = try Envelope.parse(n.call("node.info", nil)).get()
    XCTAssertEqual(info.string("node_id")?.count, 16)
    XCTAssertNotNil(info.string("version"))
    // Unknown methods must return an error envelope, never crash.
    guard case .failure(let e) = Envelope.parse(n.call("definitely.not.a.method", "{}")) else { return XCTFail("expected error") }
    XCTAssertEqual(e.code, "not_implemented")
    guard case .failure(let h) = Envelope.parse(n.call("host.info", "{}")) else { return XCTFail("expected error") }
    XCTAssertEqual(h.code, "unsupported")
    if case .success = Envelope.parse(n.call("node.info", "garbage")) {} // tolerated either way
  }

  func testTickPollWakeup() {
    let n = makeNode()
    n.tick(nowUnixMs())
    var events: [String] = []
    while let o = n.poll() { if case .event(let e) = o { events.append(e) } }
    XCTAssertTrue(events.contains { JSON.parseObject($0)?.string("event") == "node.ready" }, "\(events)")
    XCTAssertLessThanOrEqual(n.nextWakeupMs(), 60_000)
  }

  func testLinkAndAudioPlumbingDoesNotCrash() {
    let n = makeNode()
    n.linkRx(12345, Data([0xC0, 1, 2, 0xC0]))
    n.linkRx(1, Data())
    var pcm = [Int16](repeating: 100, count: 480)
    pcm.withUnsafeBufferPointer { n.audioRx(1, $0, sampleRate: 48_000) }
    let got = pcm.withUnsafeMutableBufferPointer { n.audioTx(1, $0, sampleRate: 48_000) }
    XCTAssertTrue((0...480).contains(got))
    n.addEntropy(EmberNode.entropy(16))
    n.tick(nowUnixMs())
    while n.poll() != nil {}
  }

  func testMappedGuidesPack() throws {
    let url = dir.appendingPathComponent("guides.pack")
    try Data(repeating: 0x5A, count: 4096).write(to: url)
    let m = try XCTUnwrap(MappedFile(url: url))
    XCTAssertEqual(m.count, 4096)
    XCTAssertEqual(m.data().first, 0x5A)
    let n = makeNode()
    _ = n.loadGuides(mapped: m)  // garbage pack: real core returns <0, stub 0 — must not crash
    XCTAssertNil(MappedFile(url: dir.appendingPathComponent("missing")))
  }

  func testRunnerEventsCallsAndLinkTxDispatch() {
    let runner = NodeRunner()
    let gotReady = expectation(description: "node.ready")
    gotReady.assertForOverFulfill = false
    runner.onEvent = { if $0.contains("node.ready") { gotReady.fulfill() } }
    runner.start(core: makeNode())
    wait(for: [gotReady], timeout: 5)

    let call = expectation(description: "call")
    runner.call("node.info") { r in XCTAssertNotNil(try? r.get().string("node_id")); call.fulfill() }
    wait(for: [call], timeout: 5)
    XCTAssertNotNil(try? runner.callSync("node.info").get())
    XCTAssertGreaterThan(runner.ticks, 0)
    runner.stop()
    if case .success = runner.callSync("node.info") { XCTFail("stopped runner must fail") }
  }

  /// Fake outbox: verifies LINK_TX items reach the endpoint and endpoint bytes reach ember_link_rx.
  func testRunnerLinkTxToLoopbackToLinkRx() {
    final class Recording: CoreAPI {
      let real: EmberNode
      var queued: [CoreOut] = []
      var rx: [(UInt32, Data)] = []
      var onRx: (() -> Void)?
      init(_ n: EmberNode) { real = n }
      func call(_ m: String, _ p: String?) -> String {
        if m == "links.add" { return #"{"ok":true,"result":{"link":{"id":3,"type":"raw"}}}"# }
        return real.call(m, p)
      }
      func tick(_ now: UInt64) { real.tick(now) }
      func nextWakeupMs() -> UInt32 { 50 }
      func poll() -> CoreOut? { queued.isEmpty ? real.poll() : queued.removeFirst() }
      func linkRx(_ id: UInt32, _ d: Data) { rx.append((id, d)); real.linkRx(id, d); onRx?() }
      func audioRx(_ id: UInt32, _ s: UnsafeBufferPointer<Int16>, sampleRate: UInt32) {}
      func audioTx(_ id: UInt32, _ o: UnsafeMutableBufferPointer<Int16>, sampleRate: UInt32) -> Int { 0 }
      func addEntropy(_ d: Data) {}
      func loadGuides(_ b: UnsafeRawBufferPointer, copy: Bool) -> Int32 { 0 }
    }
    let rec = Recording(makeNode())
    let runner = NodeRunner()
    let mgr = LinkManager(core: runner, factory: { _, _ in LoopbackEndpoint() },
                          availability: { _ in (true, nil) })
    runner.onLinkTx = { mgr.handleLinkTx($0, $1) }
    runner.start(core: rec)
    // host.link.create through the real runner (links.add answered by the recording core).
    let created = expectation(description: "created")
    mgr.create(["endpoint": ["kind": "loopback"], "link": ["type": "raw"]]) { r in
      XCTAssertEqual((try? r.get())?.object("link")?.int("id"), 3)
      created.fulfill()
    }
    wait(for: [created], timeout: 3)
    let rx = expectation(description: "rx")
    rec.onRx = { rx.fulfill() }
    // Core outbox item EMBER_OUT_LINK_TX(link 3) → runner → LinkManager → loopback → ember_link_rx(3).
    runner.withCore { _ in rec.queued.append(.linkTx(3, Data([1, 2, 3]))) }
    runner.kick()
    wait(for: [rx], timeout: 3)
    XCTAssertEqual(rec.rx.first?.0, 3)
    XCTAssertEqual(rec.rx.first?.1, Data([1, 2, 3]))
    mgr.closeAll()
    runner.stop()
  }
}

/// Loads the bundled diagnostic page in a real WKWebView through the custom scheme and runs its
/// self-test: JS → messageHandlers.ember → BridgeRouter → (core | host router) → onReply.
final class WebBridgeTests: XCTestCase {
  func testDiagnosticPageRoundTrip() throws {
    let env = try XCTUnwrap(AppEnvironment.shared, "tests run inside the app host")
    let host = env.makeWebHost()
    let config = host.makeConfiguration()
    let rules = expectation(description: "content rules")
    WebHost.installContentRules(on: config) { rules.fulfill() }  // same as WebViewController
    wait(for: [rules], timeout: 10)
    let wv = WKWebView(frame: CGRect(x: 0, y: 0, width: 390, height: 800), configuration: config)
    host.attach(wv)
    let window = UIApplication.shared.connectedScenes.compactMap { ($0 as? UIWindowScene)?.windows.first }.first
    window?.addSubview(wv)
    defer { wv.removeFromSuperview() }

    final class NavDelegate: NSObject, WKNavigationDelegate {
      let done: () -> Void
      let host: WebHost
      init(_ h: WebHost, _ d: @escaping () -> Void) { host = h; done = d }
      func webView(_ w: WKWebView, didFinish n: WKNavigation!) { host.pageDidLoad(); done() }
    }
    let loaded = expectation(description: "loaded")
    let nav = NavDelegate(host) { loaded.fulfill() }
    wv.navigationDelegate = nav
    wv.load(URLRequest(url: host.schemeHandler.startURL(forceDiagnostics: true)))
    wait(for: [loaded], timeout: 15)

    let done = expectation(description: "selftest")
    var result: [String: Any] = [:]
    wv.callAsyncJavaScript("return await emberDiag.selfTest();", arguments: [:], in: nil, in: .page) { r in
      if case .success(let v) = r { result = v as? [String: Any] ?? [:] }
      if case .failure(let e) = r { XCTFail("JS failed: \(e)") }
      done.fulfill()
    }
    wait(for: [done], timeout: 20)
    print("diagnostic self-test: \(result)")
    XCTAssertEqual(result["bridge"] as? Bool, true)
    XCTAssertEqual(result["secure_context"] as? Bool, true)
    XCTAssertEqual(result["origin"] as? String, "embermesh://app")
    XCTAssertEqual(result["module_import"] as? Bool, true)
    XCTAssertEqual(result["remote_blocked"] as? Bool, true)
    XCTAssertNotNil((result["node_info"] as? [String: Any])?["node_id"])
    XCTAssertEqual((result["host_info"] as? [String: Any])?["platform"] as? String, "ios")
    XCTAssertEqual(result["unknown_host_method"] as? String, "unsupported")
    XCTAssertNotNil(result["unknown_method"] as? String)
    _ = nav
  }
}
