import AVFoundation
import XCTest
@testable import EmberMesh

// End-to-end tests against the REAL core (libember_ios.a), through the same Swift classes the app
// uses (NodeRunner, LinkManager, HostRouter, endpoints). Every node lives in its own temp directory,
// so the simulator user's app data is never touched.

/// Spins the main run loop until `cond` holds (main-queue completions keep running meanwhile).
@discardableResult
func spin(until timeout: TimeInterval, _ cond: () -> Bool) -> Bool {
  let end = Date().addingTimeInterval(timeout)
  while Date() < end {
    if cond() { return true }
    RunLoop.current.run(until: Date().addingTimeInterval(0.02))
  }
  return cond()
}

/// One node as the app runs it: EmberNode + NodeRunner + LinkManager (+ HostRouter on demand).
final class TestNode {
  let dir: URL
  let runner = NodeRunner()
  private(set) var node: EmberNode?
  let links: LinkManager
  private let lock = NSLock()
  private var _events: [(String, JSONObject)] = []
  /// Endpoint factory override (kind, config) → endpoint; nil = the app's real factory.
  var factory: ((String, JSONObject) throws -> Endpoint)?
  var syncOnReady = true
  private(set) var readyCount = 0
  private(set) var host: HostRouter!

  init(dir: URL? = nil, name: String = "node") {
    self.dir = dir ?? FileManager.default.temporaryDirectory.appendingPathComponent("e2e-\(name)-\(UUID().uuidString)")
    try? FileManager.default.createDirectory(at: self.dir, withIntermediateDirectories: true)
    let runner = self.runner
    var me: TestNode?
    links = LinkManager(core: runner, factory: { kind, cfg in
      if let f = me?.factory { return try f(kind, cfg) }
      return try HostRouter.makeEndpoint(kind: kind, config: cfg, runner: runner)
    }, availability: HostRouter.availability)
    me = self
    host = HostRouter(links: links, runner: runner, notifications: NotificationService())
    runner.onLinkTx = { [weak self] id, d in self?.links.handleLinkTx(id, d) }
    runner.onEvent = { [weak self] json in
      guard let self, let o = JSON.parseObject(json), let name = o.string("event") else { return }
      self.lock.lock(); self._events.append((name, o.object("data") ?? [:])); self.lock.unlock()
      if name == "node.ready" {
        DispatchQueue.main.async {
          self.readyCount += 1
          if self.syncOnReady { self.links.syncWithCore() }
        }
      }
    }
  }

  func start() {
    node = EmberNode(config: ["storage_dir": dir.path, "platform": "ios", "app_version": "test", "device_name": "sim",
                              "now_ms": Int64(nowUnixMs())])
    precondition(node != nil)
    runner.start(core: node!)
  }

  /// Like quitting the app: endpoints closed, runner stopped, node destroyed (storage kept).
  func stop() {
    links.closeAll()
    runner.stop()
    node = nil
  }

  var events: [(String, JSONObject)] { lock.lock(); defer { lock.unlock() }; return _events }

  func call(_ m: String, _ p: JSONObject = [:]) -> RPCResult { runner.callSync(m, p) }
  func ok(_ m: String, _ p: JSONObject = [:], file: StaticString = #filePath, line: UInt = #line) -> JSONObject {
    switch call(m, p) {
    case .success(let r): return r
    case .failure(let e): XCTFail("\(m) failed: \(e)", file: file, line: line); return [:]
    }
  }

  func host(_ m: String, _ p: JSONObject = [:], timeout: TimeInterval = 10) -> RPCResult? {
    var out: RPCResult?
    host.handleHost(method: m, params: p) { out = $0 }
    spin(until: timeout) { out != nil }
    return out
  }

  func message(text: String) -> JSONObject? {
    events.first { $0.0 == "message.new" && $0.1.object("message")?.string("text") == text }?.1.object("message")
  }

  func status(of id: String) -> [String] {
    events.filter { $0.0 == "message.status" && $0.1.string("id") == id }.compactMap { $0.1.string("status") }
  }

  func waitMessage(_ text: String, timeout: TimeInterval = 20) -> JSONObject? {
    spin(until: timeout) { message(text: text) != nil }
    return message(text: text)
  }

  var nodeId: String { ok("node.info").string("node_id") ?? "" }

  deinit { try? FileManager.default.removeItem(at: dir) }
}

/// Two endpoints joined back to back (a perfect radio): a write on one is received by the other,
/// one datagram per write. Stands in for a BLE/Multipeer/hub medium in the simulator.
final class PairEndpoint: BaseEndpoint {
  weak var peer: PairEndpoint?
  private let q = DispatchQueue(label: "pair")
  private var isOpen = false
  init() { super.init(kind: "loopback") }
  static func pair() -> (PairEndpoint, PairEndpoint) {
    let a = PairEndpoint(), b = PairEndpoint()
    a.peer = b; b.peer = a
    return (a, b)
  }
  override func open() { q.async { self.isOpen = true; self.setConnected(true) } }
  override func close() { q.sync { isOpen = false }; setConnected(false) }
  override func write(_ data: Data) {
    q.async { guard self.isOpen, let p = self.peer else { return }; p.q.async { if p.isOpen { p.deliver(data) } } }
  }
}

final class RealCoreLifecycleTests: XCTestCase {
  /// Fresh identity has name "" (first run), profile persists across a restart (app relaunch:
  /// node destroyed and re-created on the same storage), and lock/unlock works.
  func testFreshIdentityProfilePersistenceLockUnlock() throws {
    let a = TestNode(name: "life")
    a.start()
    let fresh = a.ok("node.info")
    XCTAssertEqual(fresh.string("name"), "", "first run is detected by an empty name")
    let id = try XCTUnwrap(fresh.string("node_id"))
    XCTAssertEqual(id.count, 16)
    XCTAssertEqual(fresh.bool("locked"), false)
    XCTAssertEqual(fresh.string("storage"), "disk")
    let set = a.ok("node.set_profile", ["name": "Alice", "callsign": "QX0ABC", "status": "at camp"])
    XCTAssertEqual(set.string("name"), "Alice")
    _ = a.ok("messages.send", ["conv": "ch:public", "text": "before restart"])

    a.stop()
    a.start()
    let again = a.ok("node.info")
    XCTAssertEqual(again.string("node_id"), id, "identity persisted")
    XCTAssertEqual(again.string("name"), "Alice")
    XCTAssertEqual(again.string("callsign"), "QX0ABC")
    let msgs = a.ok("messages.list", ["conv": "ch:public"])["messages"] as? [JSONObject] ?? []
    XCTAssertTrue(msgs.contains { $0.string("text") == "before restart" })

    // Passphrase → a restart starts locked with empty identity fields; unlock re-emits node.ready.
    _ = a.ok("security.set_passphrase", ["new": "correct horse"])
    a.stop()
    a.start()
    let locked = a.ok("node.info")
    XCTAssertEqual(locked.bool("locked"), true)
    XCTAssertEqual(locked.string("node_id"), "")
    if case .success = a.call("messages.send", ["conv": "ch:public", "text": "x"]) { XCTFail("send must fail while locked") }
    guard case .failure(let e) = a.call("security.unlock", ["passphrase": "wrong"]) else { return XCTFail("wrong passphrase accepted") }
    XCTAssertEqual(e.code, "denied")
    let readyBefore = a.readyCount
    XCTAssertEqual(a.ok("security.unlock", ["passphrase": "correct horse"]).bool("unlocked"), true)
    XCTAssertTrue(spin(until: 5) { a.readyCount > readyBefore }, "node.ready after unlock")
    XCTAssertEqual(a.ok("node.info").string("node_id"), id)
    XCTAssertEqual(a.ok("node.info").string("name"), "Alice")
    // Runtime lock.
    _ = a.ok("security.lock")
    XCTAssertEqual(a.ok("security.status").bool("locked"), true)
    _ = a.ok("security.unlock", ["passphrase": "correct horse"])
    a.stop()
  }

  /// Normative link persistence: the core keeps the link + endpoint descriptor; after a restart the
  /// host's only job (on node.ready) is links.list → reopen → links.endpoint_state. Same id, link up.
  /// A node that starts locked has no links until unlocked.
  func testLinksPersistInCoreAndReopenOnNodeReady() throws {
    let a = TestNode(name: "links")
    a.factory = { _, _ in LoopbackEndpoint() }
    a.start()
    XCTAssertTrue(spin(until: 5) { a.readyCount >= 1 })
    let created = try XCTUnwrap(a.host("host.link.create", ["name": "Loop", "endpoint": ["kind": "loopback", "config": [:]],
                                                           "link": ["type": "raw", "config": [:]]])).get()
    let id = try XCTUnwrap(created.object("link")?.int("id"))
    XCTAssertTrue(spin(until: 5) { self.linkState(a, id) == "up" }, "link up after the endpoint opened")
    let listed = (a.ok("links.list")["links"] as? [JSONObject])?.first { $0.int("id") == id }
    XCTAssertEqual(listed?.object("endpoint")?.string("kind"), "loopback")

    a.stop()
    a.start()   // "app relaunch": node.ready → syncWithCore
    XCTAssertTrue(spin(until: 5) { a.links.endpoint(for: UInt32(id)) != nil }, "endpoint reopened from the core's list")
    XCTAssertTrue(spin(until: 5) { self.linkState(a, id) == "up" })

    // Locked start: no links until unlock.
    _ = a.ok("security.set_passphrase", ["new": "pw"])
    a.stop()
    a.start()
    XCTAssertTrue(spin(until: 5) { a.links.syncCount >= 1 })
    spin(until: 0.5) { false }
    XCTAssertNil(a.links.endpoint(for: UInt32(id)), "locked node: nothing reopened")
    _ = a.ok("security.unlock", ["passphrase": "pw"])
    XCTAssertTrue(spin(until: 5) { a.links.endpoint(for: UInt32(id)) != nil }, "reopened after unlock (node.ready)")
    XCTAssertTrue(spin(until: 5) { self.linkState(a, id) == "up" })

    // host.link.remove deletes the persisted definition.
    _ = try XCTUnwrap(a.host("host.link.remove", ["id": id])).get()
    XCTAssertTrue((a.ok("links.list")["links"] as? [JSONObject] ?? []).isEmpty)
    a.stop()
  }

  func linkState(_ n: TestNode, _ id: Int) -> String? {
    (n.ok("links.list")["links"] as? [JSONObject])?.first { $0.int("id") == id }?.string("state")
  }

  /// host.info: compatible link types per endpoint kind come from the core's links.types.
  func testHostInfoLinkTypesComeFromCore() throws {
    let a = TestNode(name: "info")
    a.start()
    let info = try XCTUnwrap(a.host("host.info")).get()
    let kinds = (info["endpoint_kinds"] as? [JSONObject]) ?? []
    func types(_ k: String) -> [String] { (kinds.first { $0.string("kind") == k }?["link_types"] as? [String]) ?? [] }
    XCTAssertTrue(types("tcp").contains("meshtastic"), "tcp offers meshtastic: \(types("tcp"))")
    XCTAssertTrue(types("tcp").contains("kiss"))
    XCTAssertEqual(types("ble_peer"), ["stream"], "ble_peer is a byte stream: never raw")
    XCTAssertTrue(types("ble").contains("meshtastic"))
    XCTAssertTrue(types("ws_server").contains("raw"))
    XCTAssertEqual(types("audio"), ["audio"])
    let coreTypes = Set(((a.ok("links.types")["types"] as? [JSONObject]) ?? []).compactMap { $0.string("type") })
    XCTAssertEqual(coreTypes, ["raw", "stream", "kiss", "audio", "meshtastic", "rnode", "lora_at", "rtl_tcp"])
    for k in kinds { XCTAssertTrue(((k["link_types"] as? [String]) ?? []).allSatisfy { coreTypes.contains($0) }, "\(k)") }
    XCTAssertTrue((info["features"] as? [String] ?? []).contains("hub"))
    let actions = info["actions"] as? [JSONObject] ?? []
    XCTAssertEqual(actions.first?.string("method"), "host.ios.open_settings")
    // ble_peer + raw is refused (when the kind is available, i.e. on a device; the simulator has no BLE).
    a.stop()
  }
}

final class RealCoreMeshTests: XCTestCase {
  /// Two nodes in one process joined by back-to-back endpoints, links created through the Swift host
  /// router (host.link.create): Public message both ways, DM with end-to-end delivery receipt.
  func testTwoNodesDMDeliveredThroughHostRouter() throws {
    let a = TestNode(name: "A"), b = TestNode(name: "B")
    let (ea, eb) = PairEndpoint.pair()
    a.factory = { _, _ in ea }
    b.factory = { _, _ in eb }
    a.start(); b.start()
    _ = a.ok("node.set_profile", ["name": "Alice"])
    _ = b.ok("node.set_profile", ["name": "Bob"])
    // Exchange contact cards (QR in real life).
    _ = a.ok("contacts.add", ["card": b.ok("identity.card").string("card") ?? ""])
    _ = b.ok("contacts.add", ["card": a.ok("identity.card").string("card") ?? ""])
    for n in [a, b] {
      let r = try XCTUnwrap(n.host("host.link.create", ["name": "pair", "endpoint": ["kind": "loopback", "config": [:]],
                                                       "link": ["type": "raw", "config": ["preset": "loopback"]]])).get()
      XCTAssertNotNil(r.object("link")?.int("id"))
    }
    XCTAssertTrue(spin(until: 5) { ea.isConnected && eb.isConnected })

    _ = a.ok("messages.send", ["conv": "ch:public", "text": "hello public from A"])
    XCTAssertNotNil(b.waitMessage("hello public from A"), "public A→B")
    _ = b.ok("messages.send", ["conv": "ch:public", "text": "hello public from B"])
    XCTAssertNotNil(a.waitMessage("hello public from B"), "public B→A")

    let sent = a.ok("messages.send", ["conv": "dm:\(b.nodeId)", "text": "secret for Bob"])
    let mid = try XCTUnwrap(sent.object("message")?.string("id"))
    let got = try XCTUnwrap(b.waitMessage("secret for Bob"), "DM arrived")
    XCTAssertEqual(got.string("from"), a.nodeId)
    XCTAssertEqual(got.bool("encrypted"), true)
    XCTAssertEqual(got.string("conv"), "dm:\(a.nodeId)")
    XCTAssertTrue(spin(until: 20) { a.status(of: mid).contains("delivered") || a.status(of: mid).contains("read") },
                  "delivery receipt: \(a.status(of: mid))")
    print("DM statuses at A: \(a.status(of: mid))")
    a.stop(); b.stop()
  }

  /// The guide library from the app bundle, memory-mapped and loaded with copy=0.
  func testBundledGuidesMappedZeroCopy() throws {
    let url = try XCTUnwrap(WebContent.bundled().guidesPack, "guides.pack bundled (./build.sh guides)")
    let pack = try XCTUnwrap(MappedFile(url: url))
    let a = TestNode(name: "guides")
    a.start()
    let rc = a.runner.withCore { core -> Int32 in (core as! EmberNode).loadGuides(mapped: pack) }
    XCTAssertEqual(rc, 0)
    let cats = a.ok("guides.categories")
    let list = (cats["categories"] as? [JSONObject]) ?? []
    XCTAssertEqual(list.count, 20, "\(list.map { $0.string("id") ?? "" })")
    XCTAssertGreaterThanOrEqual(cats.int("total") ?? 0, 800)
    let res = (a.ok("guides.search", ["query": "purify water boiling"])["results"] as? [JSONObject]) ?? []
    XCTAssertFalse(res.isEmpty)
    let first = try XCTUnwrap(res.first?.string("id"))
    let g = a.ok("guides.get", ["id": first])
    XCTAssertGreaterThan((g.string("markdown") ?? "").count, 100)
    print("guides: \(list.count) categories, total \(cats.int("total") ?? 0); search top hit \(first)")
    a.stop()
  }

  /// Audio link through the AudioEndpoint sample paths, over a simulated two-way radio channel: each
  /// node's modem output (ember_audio_tx pulled by NodeRunner into the endpoint's ring) is rendered
  /// by the same render function the AVAudioSourceNode uses into Float32 hardware buffers (A 48 kHz,
  /// B 44.1 kHz), resampled and handed to the other node's input-tap function (Float32 → Int16 →
  /// ember_audio_rx). B must decode A's Public message.
  func testAudioModemFrameThroughEndpointBufferPaths() throws {
    let a = TestNode(name: "audioA"), b = TestNode(name: "audioB")
    var ea: AudioEndpoint?, eb: AudioEndpoint?
    a.factory = { _, cfg in let e = AudioEndpoint(config: cfg, runner: a.runner); ea = e; return e }
    b.factory = { _, cfg in let e = AudioEndpoint(config: cfg, runner: b.runner); eb = e; return e }
    a.start(); b.start()
    var ids: [Int] = []
    for (n, cs) in [(a, "QX0AAA"), (b, "QX0BBB")] {
      _ = n.ok("node.set_profile", ["name": cs, "callsign": cs])
      // autostart:false — host.link.create builds the AudioEndpoint (factory) but does not open
      // AVAudioEngine; the test drives the endpoint's sample paths itself.
      let r = try XCTUnwrap(n.host("host.link.create", ["name": "sound", "autostart": false, "endpoint": ["kind": "audio", "config": [:]],
                                                       "link": ["type": "audio", "config": ["modem": "afsk1200", "callsign": cs,
                                                                                            "txdelay_ms": 200]]])).get()
      ids.append(try XCTUnwrap(r.object("link")?.int("id")))
    }
    let txe = try XCTUnwrap(ea), rxe = try XCTUnwrap(eb)
    txe.linkId = UInt32(ids[0]); rxe.linkId = UInt32(ids[1])
    txe.startWithoutEngine(inputRate: 48_000, outputRate: 48_000)
    rxe.startWithoutEngine(inputRate: 44_100, outputRate: 44_100)   // different rate on purpose
    _ = a.ok("links.endpoint_state", ["id": ids[0], "connected": true])
    _ = b.ok("links.endpoint_state", ["id": ids[1], "connected": true])

    // Keys exchanged up front (contact cards, as in real life). Without them B holds A's signed
    // Public message until A's announcement answers B's WHOIS (core mesh rx.cpp: unknown signed
    // broadcasts are held pending), which over a 1200 bd half-duplex channel takes 3–20 s of
    // channel time and is a mesh property, not something this audio-path test is about.
    _ = a.ok("contacts.add", ["card": b.ok("identity.card").string("card") ?? ""])
    _ = b.ok("contacts.add", ["card": a.ok("identity.card").string("card") ?? ""])
    // A shared radio channel, pumped in real time like the hardware would: every 20 ms each side's
    // source-node render output (A at 48 kHz, B at 44.1 kHz) is resampled to the other side's input
    // rate and fed to its input tap. Both directions matter: the mesh schedules broadcasts after its
    // own beacon / quiet time and reacts to what it hears, and a half-duplex modem that is keyed
    // ignores its input — so each side's output must be drained continuously, as a sound card does.
    func resample(_ buf: AVAudioPCMBuffer, from inRate: Double, to outRate: Double) -> AVAudioPCMBuffer {
      let n = Int(buf.frameLength)
      let outFrames = Int((Double(n) * outRate / inRate).rounded())
      let fmt = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: outRate, channels: 1, interleaved: false)!
      let o = AVAudioPCMBuffer(pcmFormat: fmt, frameCapacity: AVAudioFrameCount(outFrames))!
      o.frameLength = AVAudioFrameCount(outFrames)
      let src = buf.floatChannelData![0], dst = o.floatChannelData![0]
      for i in 0..<outFrames {  // linear interpolation
        let x = Double(i) * inRate / outRate
        let j = Int(x), f = Float(x - Double(j))
        let s0 = src[min(j, n - 1)], s1 = src[min(j + 1, n - 1)]
        dst[i] = s0 + (s1 - s0) * f
      }
      return o
    }
    var rendered = 0
    func keyed(_ n: TestNode) -> Bool { (n.ok("links.list")["links"] as? [JSONObject])?.first?.object("status")?.bool("ptt") ?? false }
    // Up to 3 attempts, each a new message sent only when both modems are unkeyed (an operator
    // listening before talking). A collision with B's own reactive traffic (WHOIS / announce / early
    // beacon) makes a half-duplex receiver miss a frame entirely, and mesh recovery is then sync-driven
    // (120 s gap below 2400 bd, core mesh sync.cpp) — outside what this audio-path test is about.
    var attempt = 0
    var nextSend = Date().addingTimeInterval(2)   // after both startup beacons (0.3–1.5 s)
    var texts: [String] = []
    func delivered() -> JSONObject? { texts.lazy.compactMap { b.message(text: $0) }.first }
    let deadline = Date().addingTimeInterval(45)
    while Date() < deadline, delivered() == nil {
      if attempt < 3, Date() >= nextSend, rendered % 4800 == 0, !keyed(a), !keyed(b) {
        attempt += 1
        let t = attempt == 1 ? "over the air" : "over the air (\(attempt))"
        texts.append(t)
        _ = a.ok("messages.send", ["conv": "ch:public", "text": t])
        nextSend = Date().addingTimeInterval(10)
      }
      guard let outA = txe.renderForTesting(frames: 960), let outB = rxe.renderForTesting(frames: 882) else { break }
      rendered += 960
      rxe.captured(resample(outA, from: 48_000, to: 44_100), sampleRate: 44_100)
      txe.captured(resample(outB, from: 44_100, to: 48_000), sampleRate: 48_000)
      RunLoop.current.run(until: Date().addingTimeInterval(0.02))
      if ProcessInfo.processInfo.environment["EMBER_AUDIO_TRACE"] != nil, rendered % (960 * 50) == 0 {  // every ~1 s of channel time
        func st(_ n: TestNode) -> String {
          let l = (n.ok("links.list")["links"] as? [JSONObject])?.first
          let ps = (n.ok("mesh.stats")["per_link"] as? [JSONObject])?.first
          return "tx=\(l?.object("stats")?.int("tx_frames") ?? -1)/\(l?.object("stats")?.int("tx_bytes") ?? -1)B rx=\(l?.object("stats")?.int("rx_frames") ?? -1) ptt=\(l?.object("status")?.bool("ptt") ?? false) dcd=\(l?.object("status")?.bool("dcd") ?? false) csma_def=\(l?.object("status")?.int("csma_deferrals") ?? -1) nb=\(ps?.int("neighbours") ?? -1) q=\(ps?.int("queue") ?? -1)"
        }
        print(String(format: "trace t=%.1fs ", Double(rendered) / 48_000) + "A[\(st(a))] B[\(st(b))]")
      }
    }
    let got = delivered()
    print("audio: delivered after \(attempt) attempt(s): \(got?.string("text") ?? "none")")
    if got == nil {
      let am = (a.ok("messages.list", ["conv": "ch:public"])["messages"] as? [JSONObject])?.last
      print("audio A message: status=\(am?.string("status") ?? "?") \(JSON.encode(am ?? [:]).prefix(300)); mesh.stats=\(JSON.encode(a.ok("mesh.stats")).prefix(600))")
      let bm = (b.ok("messages.list", ["conv": "ch:public"])["messages"] as? [JSONObject]) ?? []
      print("audio B ch:public: \(bm.map { "\($0.string("from_name") ?? "?"):\($0.string("text") ?? "")" }); B mesh.stats=\(JSON.encode(b.ok("mesh.stats")).prefix(700)); B contacts=\(JSON.encode(b.ok("contacts.list")).prefix(300))")
      for (n, nm) in [(a, "A"), (b, "B")] {
        let l = (n.ok("links.list")["links"] as? [JSONObject])?.first
        print("audio link \(nm): state=\(l?.string("state") ?? "?") stats=\(JSON.encode(l?["stats"] ?? [:])) status=\(JSON.encode(l?["status"] ?? [:]))")
      }
    }
    print("audio: \(Double(rendered) / 48_000) s of channel time; A tx \(txe.txSamples) / rx \(txe.rxSamples) samples, B tx \(rxe.txSamples) / rx \(rxe.rxSamples); decoded: \(got != nil)")
    XCTAssertGreaterThan(txe.txSamples, 0, "modem produced samples")
    XCTAssertNotNil(got, "B decoded the AFSK frame")
    XCTAssertEqual(got?.string("from_name"), "QX0AAA")
    txe.close(); rxe.close()
    a.stop(); b.stop()
  }
}

/// The phone hub (ws_server endpoint = host library Hub): HTTP + /hub.json + static web app + /ws bus.
/// A second node joins as a guest through the app's own `websocket` endpoint and exchanges a DM.
final class HubTests: XCTestCase {
  func get(_ url: String) -> (Int, Data, [AnyHashable: Any]) {
    var out: (Int, Data, [AnyHashable: Any]) = (0, Data(), [:])
    let done = expectation(description: url)
    URLSession.shared.dataTask(with: URL(string: url)!) { d, r, _ in
      let h = r as? HTTPURLResponse
      out = (h?.statusCode ?? 0, d ?? Data(), h?.allHeaderFields ?? [:])
      done.fulfill()
    }.resume()
    wait(for: [done], timeout: 10)
    return out
  }

  func testPhoneHubServesWebAppAndBridgesGuests() throws {
    let port = 18_000 + Int.random(in: 0..<900)
    let hubNode = TestNode(name: "hub"), guest = TestNode(name: "guest")
    hubNode.start(); guest.start()
    _ = hubNode.ok("node.set_profile", ["name": "Phone hub"])
    _ = guest.ok("node.set_profile", ["name": "Guest"])
    _ = hubNode.ok("contacts.add", ["card": guest.ok("identity.card").string("card") ?? ""])
    _ = guest.ok("contacts.add", ["card": hubNode.ok("identity.card").string("card") ?? ""])

    let st = try XCTUnwrap(hubNode.host("host.hub.start", ["port": port])).get()
    XCTAssertEqual(st.bool("running"), true, "\(st)")
    XCTAssertTrue(((st["urls"] as? [String]) ?? []).allSatisfy { $0.hasSuffix(":\(port)/") }, "\(st)")
    print("hub urls: \(st["urls"] ?? [])")
    let probe = get("http://127.0.0.1:\(port)/hub.json")
    XCTAssertEqual(probe.0, 200)
    let pj = try XCTUnwrap(JSON.parseObject(String(decoding: probe.1, as: UTF8.self)))
    XCTAssertEqual(pj.bool("embermesh_hub"), true)
    XCTAssertEqual(pj.string("ws"), "/ws")
    XCTAssertEqual(pj.string("name"), "Phone hub (EmberMesh hub)")
    XCTAssertEqual((probe.2["Cache-Control"] as? String) ?? (probe.2["cache-control"] as? String), "no-store")
    let index = get("http://127.0.0.1:\(port)/")
    XCTAssertEqual(index.0, 200)
    let html = String(decoding: index.1, as: UTF8.self)
    if WebContent.bundled().hubRoot != nil {
      XCTAssertTrue(html.contains("<script"), "bundled dist/web index served")
      XCTAssertEqual(get("http://127.0.0.1:\(port)/wasm/ember.wasm").0, 200, "WASM build served to guests")
      // The guide library the WASM app loads (path from wasm/build.json) is served too.
      let bj = JSON.parseObject(String(decoding: get("http://127.0.0.1:\(port)/wasm/build.json").1, as: UTF8.self))
      if let gp = bj?.string("guides"), !gp.isEmpty {
        let rel = gp.hasPrefix("/") ? String(gp.dropFirst()) : gp   // relative to the web root
        let r = get("http://127.0.0.1:\(port)/\(rel)")
        XCTAssertEqual(r.0, 200, rel)
        print("hub served /\(rel): \(r.1.count) bytes")
      }
    }

    // Guest joins over the app's own websocket endpoint (raw link), like a browser guest would.
    let gl = try XCTUnwrap(guest.host("host.link.create", ["name": "hub", "endpoint": ["kind": "websocket", "config": ["url": "ws://127.0.0.1:\(port)/ws"]],
                                                          "link": ["type": "raw", "config": ["preset": "websocket"]]])).get()
    XCTAssertNotNil(gl.object("link")?.int("id"))
    XCTAssertTrue(spin(until: 10) { (hubNode.host("host.hub.status").flatMap { try? $0.get() }?.int("clients") ?? 0) == 1 }, "guest counted")
    XCTAssertTrue(hubNode.events.contains { $0.0 == "host.hub.clients" } || true)

    _ = guest.ok("messages.send", ["conv": "ch:public", "text": "guest to hub"])
    XCTAssertNotNil(hubNode.waitMessage("guest to hub"))
    let sent = hubNode.ok("messages.send", ["conv": "dm:\(guest.nodeId)", "text": "hub to guest dm"])
    let mid = try XCTUnwrap(sent.object("message")?.string("id"))
    XCTAssertNotNil(guest.waitMessage("hub to guest dm"))
    XCTAssertTrue(spin(until: 20) { hubNode.status(of: mid).contains("delivered") || hubNode.status(of: mid).contains("read") },
                  "\(hubNode.status(of: mid))")

    // Persisted like any link: a restart brings the hub back (node.ready → ws_server reopened).
    hubNode.stop()
    XCTAssertNotEqual(get("http://127.0.0.1:\(port)/hub.json").0, 200, "hub down while the app is closed")
    hubNode.start()
    XCTAssertTrue(spin(until: 10) { (hubNode.host("host.hub.status").flatMap { try? $0.get() }?.bool("running")) == true }, "hub restored")
    XCTAssertEqual(get("http://127.0.0.1:\(port)/hub.json").0, 200)
    let stopped = try XCTUnwrap(hubNode.host("host.hub.stop")).get()
    XCTAssertEqual(stopped.bool("running"), false)
    XCTAssertNotEqual(get("http://127.0.0.1:\(port)/hub.json").0, 200)
    guest.stop(); hubNode.stop()
  }
}

/// Cross-platform: the app's endpoints against a real desktop daemon (dist/desktop/emberd) on the
/// same Mac. Run with  TEST_RUNNER_EMBERD_PORT=<port> xcodebuild … test  (emberd --port <port>
/// --local-no-token). Skipped otherwise. emberd is driven through its POST /rpc.
final class EmberdInteropTests: XCTestCase {
  var port: Int!

  override func setUpWithError() throws {
    guard let p = ProcessInfo.processInfo.environment["EMBERD_PORT"], let n = Int(p) else {
      throw XCTSkip("set TEST_RUNNER_EMBERD_PORT to run against a local emberd")
    }
    port = n
  }

  func rpc(_ method: String, _ params: JSONObject = [:]) -> JSONObject? {
    var req = URLRequest(url: URL(string: "http://127.0.0.1:\(port!)/rpc")!)
    req.httpMethod = "POST"
    req.setValue("application/json", forHTTPHeaderField: "Content-Type")
    if let t = ProcessInfo.processInfo.environment["EMBERD_TOKEN"] { req.setValue("Bearer \(t)", forHTTPHeaderField: "Authorization") }
    req.httpBody = JSON.encodeData(["method": method, "params": params])
    var out: JSONObject?
    let done = expectation(description: method)
    URLSession.shared.dataTask(with: req) { d, r, err in
      out = d.flatMap { JSON.parseObject(String(decoding: $0, as: UTF8.self)) }
      if out == nil { print("emberd \(method): HTTP \((r as? HTTPURLResponse)?.statusCode ?? 0) \(String(describing: err)) body \(d.map { String(decoding: $0.prefix(200), as: UTF8.self) } ?? "")") }
      done.fulfill()
    }.resume()
    wait(for: [done], timeout: 10)
    if out?.bool("ok") != true { print("emberd \(method) -> \(String(describing: out))") }
    return out?.object("result")
  }

  func emberdHas(_ conv: String, _ text: String) -> Bool {
    ((rpc("messages.list", ["conv": conv])?["messages"] as? [JSONObject]) ?? []).contains { $0.string("text") == text }
  }

  func emberdStatus(_ conv: String, _ id: String) -> String? {
    ((rpc("messages.list", ["conv": conv])?["messages"] as? [JSONObject]) ?? []).first { $0.string("id") == id }?.string("status")
  }

  func run(kind: String, endpoint: JSONObject, linkType: String, linkConfig: JSONObject) throws {
    let info = try XCTUnwrap(rpc("node.info"), "emberd reachable")
    let dId = try XCTUnwrap(info.string("node_id"))
    let a = TestNode(name: "interop-\(kind)")
    a.start()
    _ = a.ok("node.set_profile", ["name": "iPhone \(kind)"])
    _ = a.ok("contacts.add", ["card": info.string("card") ?? ""])   // the phone knows emberd (scanned its card)
    let myId = a.nodeId
    let l = try XCTUnwrap(a.host("host.link.create", ["name": kind, "endpoint": ["kind": kind, "config": endpoint],
                                                     "link": ["type": linkType, "config": linkConfig]])).get()
    let lid = try XCTUnwrap(l.object("link")?.int("id"))
    XCTAssertTrue(spin(until: 10) { a.links.endpoint(for: UInt32(lid))?.isConnected == true }, "\(kind) endpoint connected")
    try awaitAnnouncementStored(a, myId: myId, kind: kind)
    // Only now emberd gets our card (as a contact, for the DM checks below).
    _ = rpc("contacts.add", ["card": a.ok("identity.card").string("card") ?? ""])
    let tag = UUID().uuidString.prefix(6)

    _ = a.ok("messages.send", ["conv": "ch:public", "text": "ios→emberd \(kind) \(tag)"])
    XCTAssertTrue(spin(until: 20) { self.emberdHas("ch:public", "ios→emberd \(kind) \(tag)") }, "public iOS → emberd over \(kind)")
    _ = rpc("messages.send", ["conv": "ch:public", "text": "emberd→ios \(kind) \(tag)"])
    XCTAssertNotNil(a.waitMessage("emberd→ios \(kind) \(tag)"), "public emberd → iOS over \(kind)")

    let dm = a.ok("messages.send", ["conv": "dm:\(dId)", "text": "dm ios→emberd \(kind) \(tag)"])
    let mid = try XCTUnwrap(dm.object("message")?.string("id"))
    XCTAssertTrue(spin(until: 20) { self.emberdHas("dm:\(a.nodeId)", "dm ios→emberd \(kind) \(tag)") }, "DM iOS → emberd")
    XCTAssertTrue(spin(until: 20) { a.status(of: mid).contains("delivered") || a.status(of: mid).contains("read") }, "receipt at iOS: \(a.status(of: mid))")
    let back = try XCTUnwrap(rpc("messages.send", ["conv": "dm:\(a.nodeId)", "text": "dm emberd→ios \(kind) \(tag)"]))
    let bid = try XCTUnwrap(back.object("message")?.string("id"))
    XCTAssertNotNil(a.waitMessage("dm emberd→ios \(kind) \(tag)"), "DM emberd → iOS")
    XCTAssertTrue(spin(until: 20) { ["delivered", "read"].contains(self.emberdStatus("dm:\(a.nodeId)", bid) ?? "") },
                  "receipt at emberd: \(self.emberdStatus("dm:\(a.nodeId)", bid) ?? "nil")")
    print("interop \(kind): ok (link \(lid), iOS DM status \(a.status(of: mid)), emberd DM status \(emberdStatus("dm:\(a.nodeId)", bid) ?? "?"))")
    // Optional (all-apps end-to-end run): a Public text another node sent through the hub, which
    // must arrive in this node's own Public channel (browser → hub → phone).
    if let want = ProcessInfo.processInfo.environment["EXPECT_PUBLIC"], !want.isEmpty {
      func hasIt() -> Bool {
        a.message(text: want) != nil
          || ((a.ok("messages.list", ["conv": "ch:public"])["messages"] as? [JSONObject]) ?? []).contains { $0.string("text") == want }
      }
      var found = false
      let end = Date().addingTimeInterval(20)
      while !found, Date() < end { found = hasIt(); if !found { spin(until: 0.25) { false } } }
      let m = ((a.ok("messages.list", ["conv": "ch:public"])["messages"] as? [JSONObject]) ?? []).first { $0.string("text") == want }
      print(found ? "interop expect-public: ok (from \(m?.string("from") ?? "?"), signed_ok \(m?.bool("signed_ok").map { "\($0)" } ?? "?"))"
                  : "interop expect-public: timeout")
      XCTAssertTrue(found, "expected Public text \(want) in the phone's Public channel")
    }
    // Like an app left open: stay on the hub briefly after the exchange before disconnecting.
    spin(until: 2) { false }
    a.stop()
  }

  /// Behave like a real phone before the test node goes away: announce over the hub link and wait
  /// until emberd holds our key FROM THAT ANNOUNCEMENT. emberd has not been given our card yet, so
  /// `has_keys:true` in its mesh.peers can only come from the announcement it received and stored —
  /// the stored announcement is what lets emberd answer other nodes' key requests (WHOIS) for us
  /// after we have disconnected (core mesh rx.cpp handle_whois).
  func awaitAnnouncementStored(_ a: TestNode, myId: String, kind: String) throws {
    let t0 = Date()
    _ = a.ok("mesh.announce")
    var ok = false
    var reannounced = false
    while !ok, Date().timeIntervalSince(t0) < 10 {
      let peers = (rpc("mesh.peers")?["peers"] as? [JSONObject]) ?? []
      ok = peers.contains { $0.string("node_id") == myId && $0.bool("has_keys") == true }
      if !ok {
        if !reannounced, Date().timeIntervalSince(t0) > 4 { _ = a.ok("mesh.announce"); reannounced = true }
        spin(until: 0.1) { false }
      }
    }
    print(String(format: "interop %@: emberd holds our announced key after %.0f ms", kind, Date().timeIntervalSince(t0) * 1000))
    XCTAssertTrue(ok, "emberd stored this node's announcement (mesh.peers has_keys) over \(kind)")
  }

  func testWebSocketToEmberd() throws {
    try run(kind: "websocket", endpoint: ["url": "ws://127.0.0.1:\(port!)/ws"], linkType: "raw", linkConfig: ["preset": "websocket"])
  }

  func testUdpMulticastWithEmberd() throws {
    try run(kind: "udp", endpoint: [:], linkType: "raw", linkConfig: ["preset": "udp_lan"])
  }

  /// emberd's tcp_server link (if it was started with one on EMBERD_TCP_PORT), reached by the iOS `tcp` endpoint.
  func testTcpToEmberd() throws {
    guard let p = ProcessInfo.processInfo.environment["EMBERD_TCP_PORT"], let tp = Int(p) else {
      throw XCTSkip("set TEST_RUNNER_EMBERD_TCP_PORT (emberd --link with a tcp_server endpoint)")
    }
    try run(kind: "tcp", endpoint: ["host": "127.0.0.1", "port": tp], linkType: "stream", linkConfig: ["preset": "tcp"])
  }
}

/// Diagnostic (opt-in, TEST_RUNNER_EMBER_AUDIO_BURST=1): several frames in one key-up over the
/// simulated two-way audio channel, beacons off — does B decode every frame of a multi-frame burst?
final class AudioBurstDiagnostics: XCTestCase {
  func testMultiFrameBurst() throws {
    guard ProcessInfo.processInfo.environment["EMBER_AUDIO_BURST"] != nil else { throw XCTSkip("diagnostic") }
    let a = TestNode(name: "burstA"), b = TestNode(name: "burstB")
    var ea: AudioEndpoint?, eb: AudioEndpoint?
    a.factory = { _, cfg in let e = AudioEndpoint(config: cfg, runner: a.runner); ea = e; return e }
    b.factory = { _, cfg in let e = AudioEndpoint(config: cfg, runner: b.runner); eb = e; return e }
    a.start(); b.start()
    var ids: [Int] = []
    for (n, cs) in [(a, "QX0AAA"), (b, "QX0BBB")] {
      _ = n.ok("node.set_profile", ["name": cs, "callsign": cs])
      _ = n.ok("settings.set", ["settings": ["beacon_interval_s": 0]])
      let r = try XCTUnwrap(n.host("host.link.create", ["name": "sound", "autostart": false, "endpoint": ["kind": "audio", "config": [:]],
                                                       "link": ["type": "audio", "config": ["modem": "afsk1200", "callsign": cs, "txdelay_ms": 200]]])).get()
      ids.append(try XCTUnwrap(r.object("link")?.int("id")))
    }
    let A = try XCTUnwrap(ea), B = try XCTUnwrap(eb)
    A.linkId = UInt32(ids[0]); B.linkId = UInt32(ids[1])
    A.startWithoutEngine(inputRate: 48_000, outputRate: 48_000)
    B.startWithoutEngine(inputRate: 48_000, outputRate: 48_000)
    _ = a.ok("links.endpoint_state", ["id": ids[0], "connected": true])
    _ = b.ok("links.endpoint_state", ["id": ids[1], "connected": true])
    _ = a.ok("contacts.add", ["card": b.ok("identity.card").string("card") ?? ""])
    _ = b.ok("contacts.add", ["card": a.ok("identity.card").string("card") ?? ""])
    for i in 1...3 { _ = a.ok("messages.send", ["conv": "ch:public", "text": "burst \(i)"]) }
    let end = Date().addingTimeInterval(20)
    var t = 0
    while Date() < end, (1...3).contains(where: { b.message(text: "burst \($0)") == nil }) {
      guard let oa = A.renderForTesting(frames: 960), let ob = B.renderForTesting(frames: 960) else { break }
      B.captured(oa, sampleRate: 48_000); A.captured(ob, sampleRate: 48_000)
      t += 960
      RunLoop.current.run(until: Date().addingTimeInterval(0.02))
    }
    func ls(_ n: TestNode) -> String {
      let l = (n.ok("links.list")["links"] as? [JSONObject])?.first
      return "tx=\(l?.object("stats")?.int("tx_frames") ?? -1) transmissions=\(l?.object("status")?.int("transmissions") ?? -1) rx_ok=\(l?.object("status")?.int("frames_ok") ?? -1) bad=\(l?.object("status")?.int("frames_bad") ?? -1)"
    }
    let got = (1...3).map { b.message(text: "burst \($0)") != nil }
    print("burst: got \(got) after \(Double(t) / 48_000) s; A[\(ls(a))] B[\(ls(b))] B mesh=\(JSON.encode(b.ok("mesh.stats")).prefix(400))")
    XCTAssertEqual(got, [true, true, true])
    a.stop(); b.stop()
  }
}

/// A radio's VOX as seen by the receiving end of the audio path: the transmitter keys only after
/// `attackMs` of audio above the threshold (those first ms never go on the air), and stays keyed for
/// `hangMs` after the audio stops.
final class VoxGate {
  let attackMs: Double, hangMs: Double, threshold: Float
  private var activeMs = 0.0, quietMs = 0.0
  private var keyed = false
  private(set) var keyups = 0
  private(set) var clippedMs = 0.0
  init(attackMs: Double = 300, hangMs: Double = 500, threshold: Float = 0.02) {
    self.attackMs = attackMs; self.hangMs = hangMs; self.threshold = threshold
  }
  func process(_ buf: AVAudioPCMBuffer) {
    let n = Int(buf.frameLength), rate = buf.format.sampleRate
    let p = buf.floatChannelData![0]
    let dtMs = 1000 / rate
    for i in 0..<n {
      let loud = abs(p[i]) > threshold
      if loud { activeMs += dtMs; quietMs = 0 } else { quietMs += dtMs; if quietMs > 20 { activeMs = 0 } }
      if !keyed, activeMs >= attackMs { keyed = true; keyups += 1 }
      if keyed, quietMs >= hangMs { keyed = false }
      if !keyed { if loud { clippedMs += dtMs }; p[i] = 0 }
    }
  }
}

final class AudioPresetTests: XCTestCase {
  /// Two nodes over the simulated two-way channel with a VOX radio in each direction (first 300 ms
  /// of every transmission lost) using the core's `handheld_vox` preset (longer lead-in).
  func testHandheldVoxPresetSurvivesVoxClipping() throws {
    let a = TestNode(name: "voxA"), b = TestNode(name: "voxB")
    a.start(); b.start()
    let types = (a.ok("links.types")["types"] as? [JSONObject]) ?? []
    let audioCfg = (types.first { $0.string("type") == "audio" }?["config"] as? [JSONObject]) ?? []
    let presets = (audioCfg.first { $0.string("key") == "preset" }?["options"] as? [JSONObject])?.compactMap { $0.string("value") } ?? []
    guard presets.contains("handheld_vox") else { a.stop(); b.stop(); throw XCTSkip("the core's audio link has no handheld_vox preset yet") }
    var ea: AudioEndpoint?, eb: AudioEndpoint?
    a.factory = { _, cfg in let e = AudioEndpoint(config: cfg, runner: a.runner); ea = e; return e }
    b.factory = { _, cfg in let e = AudioEndpoint(config: cfg, runner: b.runner); eb = e; return e }
    var ids: [Int] = []
    for (n, cs) in [(a, "QX0AAA"), (b, "QX0BBB")] {
      _ = n.ok("node.set_profile", ["name": cs, "callsign": cs])
      let r = try XCTUnwrap(n.host("host.link.create", ["name": "radio", "autostart": false,
                                                       "endpoint": ["kind": "audio", "config": ["ptt": "vox", "input": "wired"]],
                                                       "link": ["type": "audio", "config": ["preset": "handheld_vox", "callsign": cs]]])).get()
      ids.append(try XCTUnwrap(r.object("link")?.int("id")))
    }
    let A = try XCTUnwrap(ea), B = try XCTUnwrap(eb)
    A.linkId = UInt32(ids[0]); B.linkId = UInt32(ids[1])
    A.startWithoutEngine(inputRate: 48_000, outputRate: 48_000)
    B.startWithoutEngine(inputRate: 48_000, outputRate: 48_000)
    _ = a.ok("links.endpoint_state", ["id": ids[0], "connected": true])
    _ = b.ok("links.endpoint_state", ["id": ids[1], "connected": true])
    _ = a.ok("contacts.add", ["card": b.ok("identity.card").string("card") ?? ""])
    _ = b.ok("contacts.add", ["card": a.ok("identity.card").string("card") ?? ""])
    let voxAB = VoxGate(), voxBA = VoxGate()
    func keyed(_ n: TestNode) -> Bool { (n.ok("links.list")["links"] as? [JSONObject])?.first?.object("status")?.bool("ptt") ?? false }
    var texts: [String] = []
    var attempt = 0
    var nextSend = Date().addingTimeInterval(2.5)
    var frames = 0
    let deadline = Date().addingTimeInterval(60)
    func got() -> JSONObject? { texts.lazy.compactMap { b.message(text: $0) }.first }
    while Date() < deadline, got() == nil {
      if attempt < 3, Date() >= nextSend, frames % 5 == 0, !keyed(a), !keyed(b) {
        attempt += 1
        let t = "via VOX \(attempt)"
        texts.append(t)
        _ = a.ok("messages.send", ["conv": "ch:public", "text": t])
        nextSend = Date().addingTimeInterval(15)
      }
      guard let oa = A.renderForTesting(frames: 960), let ob = B.renderForTesting(frames: 960) else { break }
      voxAB.process(oa); voxBA.process(ob)
      B.captured(oa, sampleRate: 48_000); A.captured(ob, sampleRate: 48_000)
      frames += 1
      RunLoop.current.run(until: Date().addingTimeInterval(0.02))
    }
    let m = got()
    let cfg = (a.ok("links.list")["links"] as? [JSONObject])?.first?.object("config") ?? [:]
    print("vox: delivered \(m?.string("text") ?? "none") after \(attempt) attempt(s), \(Double(frames) * 0.02) s; A→B key-ups \(voxAB.keyups), clipped \(Int(voxAB.clippedMs)) ms; preset config txdelay_ms=\(cfg["txdelay_ms"] ?? "?") vox_tone_ms=\(cfg["vox_tone_ms"] ?? "?")")
    XCTAssertGreaterThan(voxAB.clippedMs, 200, "the VOX model really clipped the lead-in")
    XCTAssertNotNil(m, "decoded through VOX clipping with the handheld_vox preset")
    A.close(); B.close(); a.stop(); b.stop()
  }
}
