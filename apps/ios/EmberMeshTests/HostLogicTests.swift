import UIKit
import XCTest
@testable import EmberMesh

/// Records core calls and keeps links like the real core does (links.add persists the opaque
/// endpoint descriptor, links.list echoes it, ids increase and are never reused).
final class FakeLinkCore: LinkCore {
  var calls: [(String, JSONObject)] = []
  var rx: [(UInt32, Data)] = []
  var nextId = 1
  var links: [JSONObject] = []
  var failAdd = false
  var locked = false
  var onRx: (() -> Void)?

  /// Answers for other methods (e.g. connectivity.get); nil = default handling.
  var answer: ((String, JSONObject) -> RPCResult?)?

  func call(_ method: String, _ params: JSONObject, completion: RPCCompletion?) {
    calls.append((method, params))
    var r: RPCResult = .success([:])
    if let a = answer?(method, params) {
      if let completion { DispatchQueue.main.async { completion(a) } }
      return
    }
    switch method {
    case "links.add":
      if failAdd { r = .failure(RPCError("bad_params", "rejected")) } else {
        var l: JSONObject = ["id": nextId, "type": params.string("type") ?? "", "name": params.string("name") ?? "", "state": "down"]
        if let e = params.object("endpoint") { l["endpoint"] = e }
        l["config"] = params.object("config") ?? [:]
        links.append(l)
        r = .success(["link": l])
        nextId += 1
      }
    case "links.list": r = .success(["links": locked ? [] : links])
    case "links.remove":
      let id = params.int("id")
      if links.contains(where: { $0.int("id") == id }) { links.removeAll { $0.int("id") == id } } else { r = .failure(.notFound("no link")) }
    default: break
    }
    if let completion { DispatchQueue.main.async { completion(r) } }
  }

  func linkRx(_ linkId: UInt32, _ data: Data) {
    DispatchQueue.main.async { self.rx.append((linkId, data)); self.onRx?() }
  }
  func methods() -> [String] { calls.map { $0.0 } }
}

final class EndpointCatalogTests: XCTestCase {
  func testAllArchitectureKindsListed() {
    let kinds = Set(EndpointCatalog.all.map { $0.kind })
    for k in ["websocket", "ws_server", "tcp", "tcp_server", "udp", "serial", "ble", "ble_peer", "bt_spp", "audio",
              "webrtc", "multipeer", "wifi_direct", "rtl_tcp", "loopback"] {
      XCTAssertTrue(kinds.contains(k), k)
    }
    XCTAssertEqual(kinds.count, EndpointCatalog.all.count, "duplicate kinds")
  }

  func testFieldSchemasValid() {
    let allowed = Set(Field.Kind.allCases.map { $0.rawValue })
    for spec in EndpointCatalog.all {
      XCTAssertFalse(spec.title.isEmpty)
      XCTAssertTrue(spec.linkTypes.contains(spec.preferredLinkType), spec.kind)
      var keys = Set<String>()
      for f in spec.config {
        let j = f.json
        XCTAssertTrue(allowed.contains(j.string("type")!), "\(spec.kind).\(f.key)")
        XCTAssertTrue(keys.insert(f.key).inserted, "duplicate key \(spec.kind).\(f.key)")
        XCTAssertFalse(f.label.isEmpty)
        switch f.type {
        case .select:
          XCTAssertFalse(f.options.isEmpty)
          XCTAssertTrue(f.options.contains { $0.value == f.defaultValue as? String }, "\(spec.kind).\(f.key) default not an option")
        case .number: XCTAssertNotNil(f.defaultValue as? Double, "\(spec.kind).\(f.key)")
        case .bool: XCTAssertNotNil(f.defaultValue as? Bool)
        default: break
        }
      }
      let j = spec.json(available: false, reason: "r")
      XCTAssertTrue(JSONSerialization.isValidJSONObject(j))
      XCTAssertEqual(j.string("unavailable_reason"), "r")
      XCTAssertEqual(j.bool("scannable"), false)
    }
  }

  func testDefaultsAndValidation() {
    let tcp = EndpointCatalog.spec("tcp")!
    let cfg = tcp.applyDefaults(["host": "10.0.0.1", "extra": 1])
    XCTAssertEqual(cfg.int("port"), 8001)
    XCTAssertEqual(cfg.int("extra"), 1)
    XCTAssertNil(tcp.validate(cfg))
    XCTAssertEqual(tcp.validate(tcp.applyDefaults(["host": "  "]))?.code, "bad_params")
  }

  func testAvailabilityAndHostInfo() {
    for (k, _) in EndpointCatalog.permanentlyUnsupported { XCTAssertFalse(HostRouter.availability(k).0, k) }
    XCTAssertTrue(HostRouter.availability("loopback").0)
    XCTAssertTrue(HostRouter.availability("tcp").0)
    #if targetEnvironment(simulator)
    XCTAssertFalse(HostRouter.availability("ble_peer").0)
    XCTAssertNotNil(HostRouter.availability("ble_peer").1)
    #endif
  }

  func testEndpointConfigValidation() {
    XCTAssertThrowsError(try TcpEndpoint(kind: "tcp", config: ["host": "", "port": 1]))
    XCTAssertThrowsError(try TcpEndpoint(kind: "tcp", config: ["host": "a", "port": 70000]))
    XCTAssertNoThrow(try TcpEndpoint(kind: "tcp", config: ["host": "a", "port": 1]))
    XCTAssertNoThrow(try TcpEndpoint(kind: "tcp", config: ["bonjour": "Sam's iPhone"]))
    XCTAssertThrowsError(try WebSocketEndpoint(config: ["url": "http://x/ws"]))
    XCTAssertNoThrow(try WebSocketEndpoint(config: ["url": "ws://192.168.4.1:8437/ws"]))
    XCTAssertThrowsError(try UdpMulticastEndpoint(config: ["group": "10.0.0.1"]))
    XCTAssertNoThrow(try UdpMulticastEndpoint(config: [:]))
    XCTAssertThrowsError(try BleUartEndpoint(config: ["device_id": "nope"]))
    XCTAssertThrowsError(try BleUartEndpoint(config: ["device_id": UUID().uuidString, "profile": "custom", "service_uuid": "zz"]))
    XCTAssertNoThrow(try BleUartEndpoint(config: ["device_id": UUID().uuidString, "profile": "custom", "service_uuid": "FFE0",
                                                  "rx_uuid": "FFE1", "tx_uuid": "FFE1"]))
  }
}

final class LinkManagerTests: XCTestCase {
  var core: FakeLinkCore!
  var mgr: LinkManager!
  var events: [(String, JSONObject)] = []
  var opened: [String] = []

  override func setUp() {
    core = FakeLinkCore()
    events = []
    opened = []
    mgr = makeManager()
  }
  override func tearDown() { mgr.closeAll() }

  func makeManager() -> LinkManager {
    let m = LinkManager(core: core,
                        factory: { [unowned self] kind, cfg in
                          self.opened.append(kind)
                          if kind == "loopback" { return LoopbackEndpoint() }
                          return try TcpEndpoint(kind: kind, config: cfg)
                        },
                        availability: HostRouter.availability)
    m.onEvent = { [unowned self] n, d in self.events.append((n, d)) }
    return m
  }

  func create(_ p: JSONObject) -> RPCResult {
    let e = expectation(description: "create")
    var out: RPCResult!
    mgr.create(p) { out = $0; e.fulfill() }
    wait(for: [e], timeout: 5)
    return out
  }

  func sync(_ m: LinkManager? = nil) {
    let e = expectation(description: "sync")
    (m ?? mgr).syncWithCore { e.fulfill() }
    wait(for: [e], timeout: 5)
  }

  func settle(_ s: Double = 0.3) {
    let st = expectation(description: "settle")
    DispatchQueue.main.asyncAfter(deadline: .now() + s) { st.fulfill() }
    wait(for: [st], timeout: s + 2)
  }

  func testCreateLoopbackWiresTxToRxAndStoresEndpointInCore() throws {
    let r = try create(["name": "loop", "endpoint": ["kind": "loopback", "config": [:]], "link": ["type": "raw", "config": ["k": 1]]]).get()
    XCTAssertEqual(r.object("link")?.int("id"), 1)
    let add = core.calls.first { $0.0 == "links.add" }!.1
    XCTAssertEqual(add.string("type"), "raw")
    XCTAssertEqual(add.string("name"), "loop")
    XCTAssertEqual(add.object("endpoint")?.string("kind"), "loopback")
    XCTAssertEqual(add.object("endpoint")?.bool("autostart"), true)
    XCTAssertEqual(add.object("config")?.int("k"), 1)
    XCTAssertFalse(core.methods().contains("settings.set"), "no host-side link list (links_autostart) any more")

    settle()
    XCTAssertTrue(core.calls.contains { $0.0 == "links.endpoint_state" && $0.1.bool("connected") == true && $0.1.int("id") == 1 })
    XCTAssertTrue(events.contains { $0.0 == "host.endpoint.state" && $0.1.int("link_id") == 1 })

    let rx = expectation(description: "rx")
    core.onRx = { rx.fulfill() }
    mgr.handleLinkTx(1, Data([1, 2, 3]))
    mgr.handleLinkTx(99, Data([9]))  // unknown link: ignored
    wait(for: [rx], timeout: 2)
    XCTAssertEqual(core.rx.first?.0, 1)
    XCTAssertEqual(core.rx.first?.1, Data([1, 2, 3]))
  }

  func testCreateErrors() {
    XCTAssertEqual(errCode(create([:])), "bad_params")
    XCTAssertEqual(errCode(create(["endpoint": ["kind": "serial"]])), "unsupported")
    XCTAssertEqual(errCode(create(["endpoint": ["kind": "nonsense"]])), "unsupported")
    XCTAssertEqual(errCode(create(["endpoint": ["kind": "tcp", "config": ["host": ""]]])), "bad_params")
    core.failAdd = true
    XCTAssertEqual(errCode(create(["endpoint": ["kind": "loopback"]])), "bad_params")
    XCTAssertTrue(mgr.boundIds.isEmpty)
  }

  func testLinkTypeMustBeCompatibleWithEndpointKind() {
    mgr.compatibleLinkTypes = { kind in kind == "loopback" ? ["raw"] : (kind == "tcp" ? ["stream", "kiss", "meshtastic"] : nil) }
    XCTAssertEqual(errCode(create(["endpoint": ["kind": "loopback"], "link": ["type": "stream"]])), "bad_params")
    XCTAssertNotNil(try? create(["endpoint": ["kind": "tcp", "config": ["host": "127.0.0.1"]], "link": ["type": "meshtastic"]]).get())
  }

  func testDefaultLinkTypeAndEndpointDefaults() throws {
    _ = try create(["endpoint": ["kind": "tcp", "config": ["host": "127.0.0.1"]]]).get()
    let add = core.calls.first { $0.0 == "links.add" }!.1
    XCTAssertEqual(add.string("type"), "stream")
    XCTAssertEqual(add.object("endpoint")?.object("config")?.int("port"), 8001)
  }

  func testRemoveAndReconnect() throws {
    _ = try create(["endpoint": ["kind": "loopback"], "link": ["type": "raw"]]).get()
    let e1 = mgr.endpoint(for: 1)
    var ex = expectation(description: "reconnect")
    mgr.reconnect(["id": 1]) { r in XCTAssertEqual((try? r.get())?.object("link")?.int("id"), 1); ex.fulfill() }
    wait(for: [ex], timeout: 2)
    XCTAssertFalse(mgr.endpoint(for: 1) === e1, "reconnect creates a fresh endpoint")
    ex = expectation(description: "remove")
    mgr.remove(["id": 1]) { r in XCTAssertNotNil(try? r.get()); ex.fulfill() }
    wait(for: [ex], timeout: 2)
    XCTAssertNil(mgr.endpoint(for: 1))
    XCTAssertTrue(core.methods().contains("links.remove"))
    XCTAssertTrue(core.links.isEmpty)
    ex = expectation(description: "reconnect missing")
    mgr.reconnect(["id": 1]) { r in XCTAssertEqual(self.errCode(r), "not_found"); ex.fulfill() }
    wait(for: [ex], timeout: 2)
  }

  /// node.ready → links.list → reopen every endpoint from link.endpoint (fresh manager = app relaunch).
  func testSyncReopensEndpointsFromCoreList() throws {
    _ = try create(["endpoint": ["kind": "loopback"], "link": ["type": "raw"]]).get()
    _ = try create(["endpoint": ["kind": "loopback"], "link": ["type": "raw"], "autostart": false]).get()
    core.links.append(["id": 7, "type": "raw", "endpoint": ["kind": "serial", "config": [:]]])     // another platform's link
    core.links.append(["id": 8, "type": "raw"])                                                    // links.add without endpoint
    mgr.closeAll()
    let relaunched = makeManager()
    opened = []
    core.calls = []
    sync(relaunched)
    settle()
    XCTAssertNotNil(relaunched.endpoint(for: 1))
    XCTAssertNil(relaunched.endpoint(for: 2), "autostart:false is not opened")
    XCTAssertNil(relaunched.endpoint(for: 7))
    XCTAssertEqual(opened, ["loopback"])
    XCTAssertTrue(core.calls.contains { $0.0 == "links.endpoint_state" && $0.1.int("id") == 1 && $0.1.bool("connected") == true })
    XCTAssertTrue(core.calls.contains { $0.0 == "links.endpoint_state" && $0.1.int("id") == 7 && $0.1.bool("connected") == false
                                        && !($0.1.string("error") ?? "").isEmpty })
    XCTAssertFalse(core.calls.contains { $0.0 == "links.add" }, "the host never re-adds links")

    // A second node.ready (e.g. after unlock) keeps the open endpoint instead of reconnecting it.
    let e1 = relaunched.endpoint(for: 1)
    sync(relaunched)
    XCTAssertTrue(relaunched.endpoint(for: 1) === e1)
    relaunched.closeAll()
  }

  /// A node that starts locked lists no links; after unlock (node.ready again) they come back. A wipe
  /// (links gone) closes what was open.
  func testLockedStartThenUnlockThenWipe() throws {
    _ = try create(["endpoint": ["kind": "loopback"], "link": ["type": "raw"]]).get()
    mgr.closeAll()
    core.locked = true
    let m = makeManager()
    sync(m)
    XCTAssertTrue(m.boundIds.isEmpty)
    core.locked = false
    sync(m)
    XCTAssertEqual(m.boundIds, [1])
    core.links.removeAll()  // security.wipe
    sync(m)
    XCTAssertTrue(m.boundIds.isEmpty)
    m.closeAll()
  }

  func errCode(_ r: RPCResult) -> String? { if case .failure(let e) = r { return e.code }; return nil }
}

final class ServiceLogicTests: XCTestCase {
  func testSignalPatternParsing() {
    // RPC_API §10 limits: 1–4096 entries, each 10 ms – 60 s, ≤ 10 min total.
    XCTAssertEqual(try SignalService.parsePattern(["pattern_ms": [100, 200]]).get(), [100, 200])
    XCTAssertThrowsError(try SignalService.parsePattern(["pattern_ms": []]).get())
    XCTAssertThrowsError(try SignalService.parsePattern([:]).get())
    XCTAssertThrowsError(try SignalService.parsePattern(["pattern_ms": [-1]]).get())
    XCTAssertThrowsError(try SignalService.parsePattern(["pattern_ms": [0]]).get())
    XCTAssertThrowsError(try SignalService.parsePattern(["pattern_ms": ["x"]]).get())
    XCTAssertThrowsError(try SignalService.parsePattern(["pattern_ms": [60_001]]).get())
    XCTAssertThrowsError(try SignalService.parsePattern(["pattern_ms": Array(repeating: 10, count: 4097)]).get())
  }

  func testToneWav() {
    let w = SignalService.wav(pattern: [100, 100, 100], freq: 700, rate: 8000)
    XCTAssertEqual(String(decoding: w.prefix(4), as: UTF8.self), "RIFF")
    XCTAssertEqual(w.count, 44 + 2400 * 2)
  }

  func testFileHelpers() {
    XCTAssertEqual(FileService.sanitize("../../etc/passwd"), "_.._etc_passwd")
    XCTAssertEqual(FileService.sanitize(""), "embermesh-file")
    XCTAssertEqual(FileService.contentTypes(accept: nil), [.item])
    XCTAssertEqual(FileService.contentTypes(accept: "*/*"), [.item])
    XCTAssertEqual(FileService.contentTypes(accept: "application/json, image/*"), [.json, .image])
    XCTAssertEqual(FileService.contentTypes(accept: ".txt").first, .plainText)
  }

  func testPositionShape() {
    let p = LocationService.position(.init(coordinate: .init(latitude: 1, longitude: 2), altitude: 3, horizontalAccuracy: 4,
                                           verticalAccuracy: 5, timestamp: Date(timeIntervalSince1970: 10)))
    XCTAssertEqual(p.number("lat"), 1); XCTAssertEqual(p.number("lon"), 2); XCTAssertEqual(p.number("alt"), 3)
    XCTAssertEqual(p.number("accuracy"), 4); XCTAssertEqual(p.int("ts"), 10_000)
  }
}

final class SchemeHandlerTests: XCTestCase {
  var root: URL!
  override func setUp() {
    root = FileManager.default.temporaryDirectory.appendingPathComponent("www-\(UUID().uuidString)")
    let fm = FileManager.default
    try! fm.createDirectory(at: root.appendingPathComponent("ui/js"), withIntermediateDirectories: true)
    try! fm.createDirectory(at: root.appendingPathComponent("diag"), withIntermediateDirectories: true)
    try! Data("ui".utf8).write(to: root.appendingPathComponent("ui/index.html"))
    try! Data("m".utf8).write(to: root.appendingPathComponent("ui/js/app.js"))
    try! Data("d".utf8).write(to: root.appendingPathComponent("diag/bridge-test.html"))
    try! Data("secret".utf8).write(to: root.appendingPathComponent("secret.txt"))
  }
  override func tearDown() { try? FileManager.default.removeItem(at: root) }

  func handler(withUI: Bool = true) -> AppSchemeHandler {
    if !withUI { try? FileManager.default.removeItem(at: root.appendingPathComponent("ui/index.html")) }
    return AppSchemeHandler(content: WebContent(uiRoot: root.appendingPathComponent("ui"), diagRoot: root.appendingPathComponent("diag"),
                                                guidesPack: nil))
  }

  func name(_ r: AppSchemeHandler.Resolved) -> String? { if case .file(let u) = r { return u.lastPathComponent }; return nil }

  func testResolve() {
    let h = handler()
    XCTAssertEqual(name(h.resolve(path: "/")), "index.html")
    XCTAssertEqual(name(h.resolve(path: "/js/app.js")), "app.js")
    XCTAssertEqual(name(h.resolve(path: "/js")), nil)  // directory without index
    XCTAssertEqual(name(h.resolve(path: "/__diag/bridge-test.html")), "bridge-test.html")
    XCTAssertEqual(h.resolve(path: "/../secret.txt"), .notFound)
    XCTAssertEqual(h.resolve(path: "/js/%2E%2E/%2E%2E/secret.txt"), .notFound)
    XCTAssertEqual(h.resolve(path: "/guides.pack"), .notFound)
    XCTAssertEqual(h.startURL().absoluteString, "embermesh://app/index.html")
  }

  func testFallbackToDiagnostics() {
    let h = handler(withUI: false)
    XCTAssertEqual(name(h.resolve(path: "/index.html")), "bridge-test.html")
    XCTAssertEqual(h.startURL().path, "/__diag/bridge-test.html")
  }

  func testMime() {
    XCTAssertTrue(AppSchemeHandler.mimeType(for: "js").hasPrefix("text/javascript"))
    XCTAssertTrue(AppSchemeHandler.mimeType(for: "MJS").hasPrefix("text/javascript"))
    XCTAssertEqual(AppSchemeHandler.mimeType(for: "wasm"), "application/wasm")
    XCTAssertEqual(AppSchemeHandler.mimeType(for: "pack"), "application/octet-stream")
  }
}

/// Names shown to strangers nearby must never be the owner's device name by default.
final class DefaultPeerNameTests: XCTestCase {
  private var suite: String!
  private var defaults: UserDefaults!

  override func setUp() {
    suite = "ember.tests.peername.\(UUID().uuidString)"
    defaults = UserDefaults(suiteName: suite)
  }

  override func tearDown() {
    defaults.removePersistentDomain(forName: suite)
  }

  func testDefaultIsAnonymousAndStable() {
    let n = DefaultPeerName.name(defaults: defaults)
    XCTAssertTrue(n.hasPrefix("EmberMesh-"), n)
    XCTAssertTrue(DefaultPeerName.isValidSuffix(String(n.dropFirst(DefaultPeerName.prefix.count))), n)
    XCTAssertEqual(DefaultPeerName.name(defaults: defaults), n, "kept across calls / launches")
    XCTAssertNotEqual(n, UIDevice.current.name)
    XCTAssertFalse(n.contains("iPhone") || n.contains("iPad") || n.contains("'s "))
  }

  func testInvalidStoredSuffixIsReplaced() {
    defaults.set("Sam's iPhone", forKey: "ember.default_peer_name.suffix")
    let n = DefaultPeerName.name(defaults: defaults)
    XCTAssertTrue(n.hasPrefix("EmberMesh-") && !n.contains("Sam"), n)
    XCTAssertFalse(DefaultPeerName.isValidSuffix("abcd"))
    XCTAssertFalse(DefaultPeerName.isValidSuffix("12345"))
    XCTAssertTrue(DefaultPeerName.isValidSuffix("0A9F"))
  }

  func testExplicitNameWins() {
    XCTAssertEqual(DefaultPeerName.resolve("  Base camp ", defaults: defaults), "Base camp")
    XCTAssertEqual(DefaultPeerName.resolve("", defaults: defaults), DefaultPeerName.name(defaults: defaults))
    XCTAssertEqual(DefaultPeerName.resolve(" \n", defaults: defaults), DefaultPeerName.name(defaults: defaults))
  }

  func testCatalogDoesNotPromiseTheDeviceName() {
    let help = EndpointCatalog.all.first { $0.kind == "multipeer" }?.config.first { $0.key == "display_name" }?.help ?? ""
    XCTAssertTrue(help.contains("anonymous"), help)
  }
}
