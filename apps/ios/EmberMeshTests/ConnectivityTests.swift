import Network
import XCTest
@testable import EmberMesh

// Nearby devices + internet relay, host side (RPC_API §11, CONNECTIVITY §3–4).

final class NearbyTXTTests: XCTestCase {
  func testRecordIsExactlyTheContract() {
    XCTAssertEqual(NearbyTXT.record(token: "0a1b2c3d"), ["ember": "1", "v": "1", "link": "stream", "t": "0a1b2c3d"])
  }

  func testHostileAndForeignRecords() {
    let ok: [String: String] = ["ember": "1", "v": "1", "link": "stream", "t": "0a1b2c3d"]
    XCTAssertEqual(NearbyTXT.peerToken(ok), "0a1b2c3d")
    XCTAssertEqual(NearbyTXT.peerToken(ok.merging(["extra": "x"]) { $1 }), "0a1b2c3d", "unknown keys are tolerated")
    XCTAssertNil(NearbyTXT.peerToken(["ember": "1", "link": "stream", "v": "1"]), "old iOS listener: no t")
    XCTAssertNil(NearbyTXT.peerToken(["proto": "tcp"]), "old Android listener")
    XCTAssertNil(NearbyTXT.peerToken(ok.merging(["v": "2"]) { $1 }), "wrong version")
    XCTAssertNil(NearbyTXT.peerToken(ok.merging(["ember": "0"]) { $1 }))
    for bad in ["0A1B2C3D", "0a1b2c3", "0a1b2c3d4", "0a1b2c3g", "", "0a1b 2c3", "０a1b2c3d"] {
      XCTAssertNil(NearbyTXT.peerToken(ok.merging(["t": bad]) { $1 }), "token \(bad)")
    }
    XCTAssertNil(NearbyTXT.peerToken(ok.merging(["n": String(repeating: "x", count: 65)]) { $1 }), "oversized value")
    var many = ok
    for i in 0..<20 { many["k\(i)"] = "1" }
    XCTAssertNil(NearbyTXT.peerToken(many), "too many keys")
    var big = ok
    for i in 0..<10 { big["key\(i)"] = String(repeating: "y", count: 60) }
    XCTAssertNil(NearbyTXT.peerToken(big), "over 512 bytes")
  }
}

final class NearbyPolicyTests: XCTestCase {
  func testLowerTokenDialsOtherSideWaitsEightSeconds() {
    let p = NearbyPolicy(myToken: "50000000")
    XCTAssertEqual(p.discovered("60000000", now: 0), ["60000000"], "M < T: dial at once")
    XCTAssertEqual(p.discovered("40000000", now: 0), [], "M > T: wait")
    XCTAssertEqual(p.discovered("60000000", now: 1), [], "one dial per token")
    XCTAssertEqual(p.discovered("50000000", now: 0), [], "own token")
    XCTAssertEqual(p.discovered("xyz", now: 0), [])
    XCTAssertEqual(p.tick(now: 7_999), [])
    XCTAssertEqual(p.tick(now: 8_000), ["40000000"], "nothing arrived in 8 s: dial too")
    XCTAssertEqual(p.tick(now: 9_000), [], "already dialing")
  }

  func testInboundFromThePeerCancelsTheFallbackDial() {
    let p = NearbyPolicy(myToken: "50000000")
    _ = p.discovered("40000000", now: 0)
    p.resolved("40000000", addresses: ["fe80::1", "192.168.1.20"])
    XCTAssertTrue(p.shouldAccept(ip: "192.168.1.20", now: 100))
    XCTAssertEqual(p.accepted(ip: "192.168.1.20"), "40000000")
    XCTAssertEqual(p.tick(now: 10_000), [], "the peer dialed us")
  }

  func testDialFailureAllowsRedialAndClosedLinkToo() {
    let p = NearbyPolicy(myToken: "10000000")
    XCTAssertEqual(p.discovered("20000000", now: 0), ["20000000"])
    p.dialFinished("20000000", connected: false, remoteIP: nil)
    XCTAssertEqual(p.discovered("20000000", now: 1), ["20000000"], "may dial again after a failure")
    p.dialFinished("20000000", connected: true, remoteIP: "10.0.0.2")
    XCTAssertEqual(p.links, 1)
    p.closed(token: "20000000", ip: "10.0.0.2")
    XCTAssertEqual(p.links, 0)
    XCTAssertEqual(p.connectionsPerIP["10.0.0.2"], nil)
    XCTAssertEqual(p.discovered("20000000", now: 2), ["20000000"])
  }

  func testAtMostFourNewDialsPerMinute() {
    let p = NearbyPolicy(myToken: "00000000")
    var dialed: [String] = []
    for i in 1...6 { dialed += p.discovered(String(format: "%08x", i), now: Double(i)) }
    XCTAssertEqual(dialed.count, 4)
    XCTAssertEqual(p.tick(now: 30_000), [], "still inside the minute")
    let later = p.tick(now: 61_000)
    XCTAssertEqual(later.count, 2, "deferred dials go out when the window allows")
  }

  func testPerSourceLimitsOnLinkOnly() {
    let q = NearbyPolicy(myToken: "00000000")
    XCTAssertFalse(q.shouldAccept(ip: "8.8.8.8", now: 0), "public source refused")
    XCTAssertFalse(q.shouldAccept(ip: "127.0.0.1", now: 0), "loopback refused")
    XCTAssertFalse(q.shouldAccept(ip: "100.64.1.2", now: 0), "carrier NAT is not on-link")
    XCTAssertFalse(q.shouldAccept(ip: "2001:db8::1", now: 0), "global IPv6 refused")
    XCTAssertTrue(q.shouldAccept(ip: "10.0.0.9", now: 0)); q.accepted(ip: "10.0.0.9")
    XCTAssertTrue(q.shouldAccept(ip: "10.0.0.9", now: 1)); q.accepted(ip: "10.0.0.9")
    XCTAssertFalse(q.shouldAccept(ip: "10.0.0.9", now: 2), "2 connections per IPv4 address")
    q.closed(token: nil, ip: "10.0.0.9")
    XCTAssertTrue(q.shouldAccept(ip: "10.0.0.9", now: 3), "attempts so far: 3 + this one")
    XCTAssertFalse(q.shouldAccept(ip: "10.0.0.9", now: 4), "at most 4 new per source per minute")
    XCTAssertTrue(q.shouldAccept(ip: "10.0.0.9", now: 61_000), "a minute later")
    // IPv6: the /64 is one source.
    XCTAssertTrue(q.shouldAccept(ip: "fd00:1:2:3::a", now: 0)); q.accepted(ip: "fd00:1:2:3::a")
    XCTAssertTrue(q.shouldAccept(ip: "fd00:1:2:3::b", now: 1)); q.accepted(ip: "fd00:1:2:3::b")
    XCTAssertFalse(q.shouldAccept(ip: "fd00:1:2:3::c", now: 2), "same /64")
    XCTAssertTrue(q.shouldAccept(ip: "fd00:1:2:4::c", now: 2), "another /64")
    XCTAssertTrue(q.shouldAccept(ip: "fe80::1%en0", now: 3), "link-local")
  }

  func testOnLinkDialOrder() {
    let nets: [([UInt8], [UInt8])] = [([192, 168, 1, 10], [255, 255, 255, 0])]
    let order = LocalNet.dialOrder(["192.168.1.20", "8.8.8.8", "fe80::2%en0", "192.168.2.5", "fd00::5", "192.168.1.21", "192.168.1.22", "192.168.1.23", "192.168.1.24"], nets: nets)
    XCTAssertEqual(order.first, "fe80::2%en0", "IPv6 link-local first")
    XCTAssertEqual(order.count, 4, "at most 4 addresses per instance")
    XCTAssertFalse(order.contains("8.8.8.8")); XCTAssertFalse(order.contains("192.168.2.5"), "other subnet")
    XCTAssertFalse(order.contains("fd00::5"), "ULA outside the interface prefixes")
    XCTAssertEqual(LocalNet.sourceKey("fd00:1:2:3::a"), "fd00000100020003::/64")
    XCTAssertEqual(LocalNet.sourceKey("10.0.0.9"), "10.0.0.9")
  }
}

/// Records what the controller asks of the transport.
final class FakeTransport: NearbyTransport {
  weak var delegate: NearbyTransportDelegate?
  var isRunning = false
  var starts: [String] = []
  var stops = 0
  var dials: [String] = []
  func start(token: String) { isRunning = true; starts.append(token) }
  func stop() { isRunning = false; stops += 1 }
  func dial(token: String) { dials.append(token) }
}

/// The Mac's (simulator's) private IPv4 address, for tests that need a LAN host.
func lanIPv4() -> String? {
  for (a, m) in LocalNet.interfaceNets() where a.count == 4 && m.count == 4 {
    let ip = a.map(String.init).joined(separator: ".")
    if LocalNet.isPrivateOrLinkLocal(ip), !ip.hasPrefix("169.254") { return ip }
  }
  return nil
}

final class ConnectivityControllerTests: XCTestCase {
  var core: FakeLinkCore!
  var mgr: LinkManager!
  var transport: FakeTransport!
  var ctl: ConnectivityController!
  var made: [(String, JSONObject)] = []

  override func setUp() {
    core = FakeLinkCore()
    made = []
    mgr = LinkManager(core: core, factory: { [unowned self] kind, cfg in
      self.made.append((kind, cfg))
      return LoopbackEndpoint()
    }, availability: { _ in (true, nil) })
    mgr.compatibleLinkTypes = { k in k == "websocket" ? ["raw", "relay"] : ["stream", "raw"] }
    transport = FakeTransport()
    ctl = ConnectivityController(core: core, links: mgr, transport: transport)
  }
  override func tearDown() { mgr.closeAll() }

  /// A Connectivity object (rev 2): `ready` and the per-feature `active` come from the core.
  static func conn(ready: Bool = true, nearby: Bool = true, token: String = "11111111", internet: Bool = false, relays: [JSONObject]? = nil) -> JSONObject {
    let rs = relays ?? [["url": "wss://relay-a.example:8884/mqtt", "enabled": true],
                        ["url": "wss://relay-b.example:8084/mqtt", "enabled": true],
                        ["url": "wss://relay-c.example:8081/mqtt", "enabled": false, "note": "lets anyone watch topics"]]
    return ["ready": ready, "setup_done": ready,
            "nearby": ["enabled": nearby, "active": nearby && ready, "token": token, "notice": "…", "links": 0],
            "internet": ["enabled": internet, "active": internet && ready, "notice": "…", "relays": rs],
            "available_link_types": ["stream", "relay"]]
  }

  func settle() { spin(until: 0.3) { false } }
  func adds() -> [JSONObject] { core.calls.filter { $0.0 == "links.add" }.map { $0.1 } }

  /// Nothing listens, browses or connects unless the core says `ready` and the feature is active.
  func testGatingFollowsCoreReadyAndActive() {
    ctl.apply(Self.conn(ready: false, internet: true), fromUser: false)   // fresh install: setup not applied yet
    settle()
    XCTAssertTrue(transport.starts.isEmpty, "no Local Network prompt before setup is applied")
    XCTAssertTrue(adds().isEmpty, "no relay before ready")
    ctl.apply(Self.conn(ready: true), fromUser: false)                    // the UI applied setup
    XCTAssertEqual(transport.starts, ["11111111"])
    settle()
    XCTAssertFalse(adds().contains { $0.object("endpoint")?.string("kind")?.hasPrefix("ble") == true },
                   "v1: Nearby never creates a Bluetooth link (no Bluetooth prompt from the switch)")
    settle()
    XCTAssertTrue(adds().isEmpty, "internet off: no relay")
    ctl.apply(Self.conn(ready: true, internet: true), fromUser: false)
    settle()
    XCTAssertEqual(ctl.relayLinks.count, 2)
    // Lock / wipe / duress: ready goes false → everything stops and closes.
    ctl.apply(Self.conn(ready: false, internet: true), fromUser: false)
    settle()
    XCTAssertFalse(transport.isRunning)
    XCTAssertTrue(ctl.relayLinks.isEmpty)
    XCTAssertEqual(core.calls.filter { $0.0 == "links.remove" }.count, 2)
    ctl.apply(Self.conn(ready: true, nearby: false), fromUser: false)
    XCTAssertFalse(transport.isRunning, "Nearby off")
  }

  func testRelayLinksFollowTheCoreList() throws {
    ctl.apply(Self.conn(nearby: false, internet: true), fromUser: true)
    settle()
    let a = adds()
    XCTAssertEqual(a.count, 2, "one link per enabled relay")
    for add in a {
      XCTAssertEqual(add.string("type"), "relay")
      XCTAssertEqual(add.string("auto"), "relay")
      XCTAssertEqual(add.bool("persist"), false)
      let ep = try XCTUnwrap(add.object("endpoint"))
      XCTAssertEqual(ep.string("kind"), "websocket")
      XCTAssertEqual(ep.object("config")?["protocols"] as? [String], ["mqtt"])
      XCTAssertEqual(add.string("auto_key"), ep.object("config")?.string("url"))
      XCTAssertEqual(add.object("config")?.string("url"), add.string("auto_key"))
      XCTAssertNil(add.object("config")?["username"], "credentials are the core's")
      XCTAssertNil(add.object("config")?["password"])
    }
    ctl.apply(Self.conn(nearby: false, internet: true, relays: [["url": "wss://relay-a.example:8884/mqtt", "enabled": true],
                                                                 ["url": "wss://relay-b.example:8084/mqtt", "enabled": false]]), fromUser: true)
    settle()
    XCTAssertEqual(adds().count, 2)
    XCTAssertEqual(core.calls.filter { $0.0 == "links.remove" }.count, 1)
    XCTAssertEqual(Array(ctl.relayLinks.keys), ["wss://relay-a.example:8884/mqtt"])
    ctl.apply(Self.conn(nearby: false, internet: false), fromUser: true)
    settle()
    XCTAssertTrue(ctl.relayLinks.isEmpty)
  }

  func testRelaySocketRules() throws {
    let lan = "192.168.1.5"
    func code(_ url: String, _ extra: JSONObject = [:]) -> String? {
      var cfg: JSONObject = ["url": url, "protocols": ["mqtt"]]
      for (k, v) in extra { cfg[k] = v }
      do { _ = try WebSocketEndpoint(config: cfg); return nil } catch { return (error as? RPCError)?.code ?? "?" }
    }
    XCTAssertNil(code("wss://broker.example:8884/mqtt"))
    XCTAssertEqual(code("ws://\(lan):8437/mqtt"), "denied", "plaintext needs allow_plaintext")
    XCTAssertNil(code("ws://\(lan):8437/mqtt", ["allow_plaintext": true]))
    XCTAssertNil(code("ws://hub.local:8437/mqtt", ["allow_plaintext": true]))
    XCTAssertNil(code("ws://[fe80::1]:8437/mqtt", ["allow_plaintext": true]))
    XCTAssertEqual(code("ws://broker.example:1883/mqtt", ["allow_plaintext": true]), "denied", "never plaintext to an internet host")
    XCTAssertEqual(code("ws://8.8.8.8/mqtt", ["allow_plaintext": true]), "denied")
    XCTAssertEqual(code("wss://broker.example/mqtt", ["insecure": true]), "denied", "no insecure option")
    XCTAssertNoThrow(try WebSocketEndpoint(config: ["url": "ws://8.8.8.8:8437/ws"]), "a hub websocket (no mqtt) is not a relay socket")
    // Through the controller: a plaintext public relay is never created.
    ctl.apply(Self.conn(nearby: false, internet: true, relays: [["url": "ws://broker.example:1883/mqtt", "enabled": true, "allow_plaintext": true]]), fromUser: true)
    settle()
    XCTAssertTrue(adds().isEmpty)
    ctl.apply(Self.conn(nearby: false, internet: true, relays: [["url": "ws://\(lan):8437/mqtt", "enabled": true, "allow_plaintext": true]]), fromUser: true)
    settle()
    XCTAssertEqual(adds().count, 1)
  }

  func testBackgroundClosesEverythingForegroundRestores() {
    core.answer = { m, _ in m == "connectivity.get" ? .success(Self.conn(internet: true)) : nil }
    ctl.refresh()
    settle()
    XCTAssertTrue(transport.isRunning)
    XCTAssertEqual(ctl.relayLinks.count, 2)
    ctl.setForeground(false)
    settle()
    XCTAssertFalse(transport.isRunning, "no advertising/browsing in the background")
    XCTAssertTrue(ctl.relayLinks.isEmpty, "relay sockets closed in the background")
    ctl.setForeground(true)
    settle()
    XCTAssertTrue(transport.isRunning)
    XCTAssertEqual(ctl.relayLinks.count, 2, "restored on return")
  }

  func testNetworkChangeReadvertisesUnderTheNewToken() {
    ctl.apply(Self.conn(token: "11111111"), fromUser: false)
    core.answer = { m, _ in m == "connectivity.network_changed" ? .success(["token": "22222222"]) : nil }
    ctl.pathChanged("en0|satisfied")         // first observation: no change
    settle()
    XCTAssertFalse(core.calls.contains { $0.0 == "connectivity.network_changed" })
    ctl.pathChanged("en1|satisfied")         // joined another network
    settle()
    XCTAssertTrue(core.calls.contains { $0.0 == "connectivity.network_changed" })
    XCTAssertEqual(transport.starts, ["11111111", "22222222"], "re-advertised under the new token")
  }

  func testNearbyLinkPresetKeysAndRemoval() throws {
    ctl.apply(Self.conn(token: "11111111"), fromUser: false)
    ctl.nearbyDiscovered("22222222")
    XCTAssertEqual(transport.dials, ["22222222"], "lower token dials")
    let ep = LoopbackEndpoint()
    ctl.nearbyConnected(ep, role: "dial", token: "22222222", remoteIP: "fe80::2%en0")
    settle()
    let add = try XCTUnwrap(adds().last)
    XCTAssertEqual(add.string("type"), "stream")
    XCTAssertEqual(add.object("config")?.string("preset"), "nearby", "the core sends and requires the hello")
    XCTAssertEqual(add.string("auto"), "nearby")
    XCTAssertEqual(add.string("auto_role"), "dial")
    XCTAssertEqual(add.string("auto_key"), "22222222@fe80::2")
    XCTAssertEqual(add.bool("persist"), false)
    let id = try XCTUnwrap(ctl.nearbyLinks.keys.first)
    XCTAssertTrue(mgr.endpoint(for: id) === ep)
    // hello_failed (or any reason): the core removed the link → close the socket.
    ctl.coreEvent("link.removed", ["id": Int(id), "reason": "hello_failed"])
    XCTAssertNil(mgr.endpoint(for: id))
    XCTAssertTrue(ctl.nearbyLinks.isEmpty)
    // Accepted connection: "@<address>".
    let ep2 = LoopbackEndpoint()
    ctl.nearbyConnected(ep2, role: "accept", token: nil, remoteIP: "192.168.1.30")
    settle()
    XCTAssertEqual(adds().last?.string("auto_key"), "@192.168.1.30")
    XCTAssertEqual(adds().last?.string("auto_role"), "accept")
    // The core refuses: the connection is closed, the slot freed.
    core.failAdd = true
    let ep3 = LoopbackEndpoint(); ep3.open()
    XCTAssertTrue(spin(until: 1) { ep3.isConnected })
    ctl.nearbyConnected(ep3, role: "accept", token: nil, remoteIP: "10.0.0.7")
    settle()
    XCTAssertFalse(ep3.isConnected)
  }

  func testSyncOnNodeReadyLeavesAutoLinksAlone() {
    core.links.append(["id": 9, "type": "stream", "auto": "nearby", "endpoint": ["kind": "tcp", "config": ["nearby": true]]])
    let e = expectation(description: "sync")
    mgr.syncWithCore { e.fulfill() }
    wait(for: [e], timeout: 5)
    XCTAssertTrue(made.isEmpty, "auto links are never reopened from their descriptor")
  }

  func testHostInfoFeaturesAndRelayBackoff() throws {
    let n = TestNode(name: "conn-info")
    n.start()
    let info = try XCTUnwrap(n.host("host.info")).get()
    let f = info["features"] as? [String] ?? []
    XCTAssertTrue(f.contains("nearby")); XCTAssertTrue(f.contains("relay"))
    XCTAssertEqual(info.bool("websocket_tls"), true)
    n.stop()
    let relay = try WebSocketEndpoint(config: ["url": "wss://relay.example/mqtt", "protocols": ["mqtt"]])
    XCTAssertEqual(relay.reconnectPolicy, .relay)
    XCTAssertEqual(try WebSocketEndpoint(config: ["url": "ws://192.168.4.1:8437/ws"]).reconnectPolicy, .standard)
    let p = ReconnectPolicy.relay
    XCTAssertEqual(p.delay(attempt: 0, random: 0), 2)
    XCTAssertEqual(p.delay(attempt: 20, random: 0), 300, "cap 5 min")
    XCTAssertEqual(p.delay(attempt: 20, random: 1), 360, accuracy: 0.001, "+20 %")
    XCTAssertEqual(p.delay(attempt: 20, random: -1), 240, accuracy: 0.001, "−20 %")
    XCTAssertEqual(p.resetAfterSeconds, 120)
  }
}

/// The websocket endpoint sends the subprotocol (Sec-WebSocket-Protocol: mqtt) — checked against an
/// in-test WebSocket server (Network.framework) that only accepts "mqtt" and echoes.
final class WebSocketSubprotocolTests: XCTestCase {
  func testRelaySubprotocolHandshakeAndEcho() throws {
    let ws = NWProtocolWebSocket.Options()
    var offered: [String] = []
    var userAgent = ""
    ws.setClientRequestHandler(DispatchQueue(label: "ws-test")) { protocols, headers in
      offered = protocols
      userAgent = headers.first { $0.name.lowercased() == "user-agent" }?.value ?? ""
      return protocols.contains("mqtt") ? NWProtocolWebSocket.Response(status: .accept, subprotocol: "mqtt")
                                         : NWProtocolWebSocket.Response(status: .reject, subprotocol: nil)
    }
    let params = NWParameters.tcp
    params.defaultProtocolStack.applicationProtocols.insert(ws, at: 0)
    let listener = try NWListener(using: params, on: .any)
    let q = DispatchQueue(label: "ws-server")
    listener.newConnectionHandler = { c in
      func loop() {
        c.receiveMessage { data, ctx, _, err in
          guard err == nil, let data else { return }
          let meta = NWProtocolWebSocket.Metadata(opcode: .binary)
          c.send(content: data, contentContext: NWConnection.ContentContext(identifier: "echo", metadata: [meta]), completion: .idempotent)
          loop()
        }
      }
      c.start(queue: q); loop()
    }
    var port: UInt16 = 0
    listener.stateUpdateHandler = { if case .ready = $0 { port = listener.port?.rawValue ?? 0 } }
    listener.start(queue: q)
    XCTAssertTrue(spin(until: 5) { port != 0 })
    defer { listener.cancel() }

    final class Sink: EndpointDelegate {
      var got: [Data] = []; var up = false
      func endpoint(_ e: Endpoint, received data: Data) { DispatchQueue.main.async { self.got.append(data) } }
      func endpoint(_ e: Endpoint, connected: Bool, error: String?) { DispatchQueue.main.async { self.up = connected } }
      func endpointStatusChanged(_ e: Endpoint) {}
    }
    let sink = Sink()
    let lan = try XCTUnwrap(lanIPv4(), "a private IPv4 address on this host")
    let ep = try WebSocketEndpoint(config: ["url": "ws://\(lan):\(port)/mqtt", "protocols": ["mqtt"], "allow_plaintext": true])
    ep.delegate = sink
    ep.open()
    XCTAssertTrue(spin(until: 10) { sink.up }, "handshake with subprotocol mqtt")
    XCTAssertEqual(offered, ["mqtt"])
    XCTAssertEqual(userAgent, "mqtt", "generic User-Agent")
    XCTAssertFalse(userAgent.lowercased().contains("ember"), "nothing naming EmberMesh")
    ep.write(Data([0x10, 0x02, 0x00, 0x04]))
    XCTAssertTrue(spin(until: 10) { !sink.got.isEmpty })
    XCTAssertEqual(sink.got.first, Data([0x10, 0x02, 0x00, 0x04]))
    ep.write(Data(repeating: 1, count: 8 * 1024 + 1))   // over the 8 KiB relay cap: not sent
    ep.write(Data([0xC0, 0x00]))
    XCTAssertTrue(spin(until: 10) { sink.got.count >= 2 })
    XCTAssertEqual(sink.got.last, Data([0xC0, 0x00]))
    XCTAssertEqual(sink.got.count, 2, "the oversized message never went out")
    ep.close()
  }
}

/// Minimal MQTT 3.1.1 broker over WebSocket (subprotocol "mqtt") for tests: CONNECT, SUBSCRIBE,
/// UNSUBSCRIBE, PUBLISH (QoS 0, forwarded to every subscriber of the topic), PINGREQ, DISCONNECT.
/// One WebSocket message may hold several packets and a packet may span messages.
final class MiniMQTTBroker {
  let listener: NWListener
  private let q = DispatchQueue(label: "mini-mqtt")
  private var subs: [ObjectIdentifier: Set<String>] = [:]
  private var conns: [ObjectIdentifier: NWConnection] = [:]
  private var bufs: [ObjectIdentifier: Data] = [:]
  private(set) var published = 0
  private(set) var connects = 0
  private(set) var refused = 0
  private(set) var topics: Set<String> = []
  static func isTopic(_ t: String) -> Bool { t.utf8.count == 32 && t.utf8.allSatisfy { (48...57).contains($0) || (97...102).contains($0) } }
  var port: UInt16 { listener.port?.rawValue ?? 0 }

  init() throws {
    let ws = NWProtocolWebSocket.Options()
    ws.setClientRequestHandler(DispatchQueue(label: "mini-mqtt-hs")) { protocols, _ in
      protocols.contains("mqtt") ? NWProtocolWebSocket.Response(status: .accept, subprotocol: "mqtt")
                                 : NWProtocolWebSocket.Response(status: .reject, subprotocol: nil)
    }
    let params = NWParameters.tcp
    params.defaultProtocolStack.applicationProtocols.insert(ws, at: 0)
    listener = try NWListener(using: params, on: .any)
    listener.newConnectionHandler = { [weak self] c in self?.accept(c) }
    listener.start(queue: q)
  }

  func stop() { listener.cancel(); q.sync { conns.values.forEach { $0.cancel() } } }

  private func accept(_ c: NWConnection) {
    let id = ObjectIdentifier(c)
    conns[id] = c; subs[id] = []; bufs[id] = Data()
    c.start(queue: q)
    read(c, id)
  }

  private func read(_ c: NWConnection, _ id: ObjectIdentifier) {
    c.receiveMessage { [weak self] data, _, _, err in
      guard let self else { return }
      if err != nil { self.drop(id); return }
      if let data { self.bufs[id, default: Data()].append(data); self.parse(id) }
      self.read(c, id)
    }
  }

  private func drop(_ id: ObjectIdentifier) { conns[id]?.cancel(); conns[id] = nil; subs[id] = nil; bufs[id] = nil }

  private func send(_ id: ObjectIdentifier, _ bytes: Data) {
    let meta = NWProtocolWebSocket.Metadata(opcode: .binary)
    conns[id]?.send(content: bytes, contentContext: NWConnection.ContentContext(identifier: "mqtt", metadata: [meta]), completion: .idempotent)
  }

  private func parse(_ id: ObjectIdentifier) {
    while var b = bufs[id], b.count >= 2 {
      let bytes = [UInt8](b)
      var len = 0, mul = 1, i = 1
      while true {
        guard i < bytes.count, i <= 4 else { return }   // need more bytes
        len += Int(bytes[i] & 0x7f) * mul; mul *= 128
        i += 1
        if bytes[i - 1] & 0x80 == 0 { break }
      }
      guard bytes.count >= i + len else { return }
      let type = bytes[0] >> 4
      let body = Array(bytes[i..<(i + len)])
      let whole = Data(bytes[0..<(i + len)])
      b.removeFirst(i + len); bufs[id] = b
      func str(_ at: Int) -> (String, Int)? {
        guard at + 2 <= body.count else { return nil }
        let n = Int(body[at]) << 8 | Int(body[at + 1])
        guard at + 2 + n <= body.count else { return nil }
        return (String(decoding: body[(at + 2)..<(at + 2 + n)], as: UTF8.self), at + 2 + n)
      }
      switch type {
      case 1: connects += 1; send(id, Data([0x20, 0x02, 0x00, 0x00]))
      case 8:  // SUBSCRIBE
        guard body.count >= 2 else { return }
        var at = 2, granted: [UInt8] = []
        while at < body.count, let (t, next) = str(at) {
          // Like the hub's /mqtt: exactly 32 lowercase hex, no wildcards; otherwise refused (0x80).
          if MiniMQTTBroker.isTopic(t) { subs[id]?.insert(t); granted.append(0) } else { refused += 1; granted.append(0x80) }
          at = next + 1
        }
        send(id, Data([0x90, UInt8(2 + granted.count), body[0], body[1]] + granted))
      case 10:  // UNSUBSCRIBE
        guard body.count >= 2 else { return }
        var at = 2
        while at < body.count, let (t, next) = str(at) { subs[id]?.remove(t); at = next }
        send(id, Data([0xB0, 0x02, body[0], body[1]]))
      case 3:  // PUBLISH (QoS 0): forwarded to every subscriber — the publisher too, like real brokers
        guard let (t, _) = str(0), MiniMQTTBroker.isTopic(t) else { refused += 1; return }
        published += 1
        topics.insert(t)
        for (other, ts) in subs where ts.contains(t) { send(other, whole) }
      case 12: send(id, Data([0xD0, 0x00]))
      case 14: drop(id); return
      default: break
      }
    }
  }
}

final class RelayEndToEndTests: XCTestCase {
  /// Two nodes reachable only through a relay (an in-test MQTT-over-WebSocket broker): internet
  /// consent through connectivity.set, relay links created by ConnectivityController from
  /// connectivity.get, the core's relay driver seals and publishes, a DM arrives each way.
  func testDMThroughLocalRelay() throws {
    let probe = TestNode(name: "relay-probe")
    probe.start()
    if case .failure(let e) = probe.call("connectivity.get") { probe.stop(); throw XCTSkip("core without connectivity (\(e.code))") }
    let types = Set(((probe.ok("links.types")["types"] as? [JSONObject]) ?? []).compactMap { $0.string("type") })
    probe.stop()
    guard types.contains("relay") else { throw XCTSkip("core has no relay link type yet") }

    let broker = try MiniMQTTBroker()
    defer { broker.stop() }
    XCTAssertTrue(spin(until: 5) { broker.port != 0 })
    let lan = try XCTUnwrap(lanIPv4(), "a private IPv4 address on this host")
    let url = "ws://\(lan):\(broker.port)/mqtt"   // plaintext only to a LAN host, with allow_plaintext
    let a = TestNode(name: "relA"), b = TestNode(name: "relB")
    a.start(); b.start()
    a.syncOnReady = false; b.syncOnReady = false
    _ = a.ok("node.set_profile", ["name": "Far A"]); _ = b.ok("node.set_profile", ["name": "Far B"])
    _ = a.ok("contacts.add", ["card": b.ok("identity.card").string("card") ?? ""])
    _ = b.ok("contacts.add", ["card": a.ok("identity.card").string("card") ?? ""])
    var ctls: [ConnectivityController] = []
    for n in [a, b] {
      let set = n.call("connectivity.set", ["setup_done": true, "nearby": false, "internet": true,
                                            "relays": [["url": url, "enabled": true, "allow_plaintext": true]]])
      guard case .success = set else { a.stop(); b.stop(); throw XCTSkip("connectivity.set refused: \(set)") }
      let c = ConnectivityController(core: n.runner, links: n.links, transport: FakeTransport())
      n.host.loadLinkTypes()   // compatible types from the core's links.types (relay on websocket)
      c.refresh()
      ctls.append(c)
    }
    defer { for c in ctls { c.apply(nil, fromUser: false) } }
    XCTAssertTrue(spin(until: 15) { ctls.allSatisfy { $0.relayLinks.count == 1 } }, "one relay link each")
    func relayUp(_ n: TestNode) -> Bool {
      ((n.ok("links.list")["links"] as? [JSONObject]) ?? []).contains { $0.string("type") == "relay" && $0.string("state") == "up" }
    }
    XCTAssertTrue(spin(until: 20) { relayUp(a) && relayUp(b) }, "relay links up (MQTT connected)")
    print("relay: broker connects \(broker.connects); A link \(((a.ok("links.list")["links"] as? [JSONObject])?.first { $0.string("type") == "relay" }).map { JSON.encode($0["status"] ?? [:]).prefix(300) } ?? "-")")
    let dm = a.ok("messages.send", ["conv": "dm:\(b.nodeId)", "text": "hello over the relay"])
    XCTAssertNotNil(b.waitMessage("hello over the relay", timeout: 30), "A → relay → B")
    _ = b.ok("messages.send", ["conv": "dm:\(a.nodeId)", "text": "relay reply"])
    XCTAssertNotNil(a.waitMessage("relay reply", timeout: 30), "B → relay → A")
    let mid = dm.object("message")?.string("id") ?? ""
    XCTAssertTrue(spin(until: 30) { a.status(of: mid).contains("delivered") || a.status(of: mid).contains("read") }, "\(a.status(of: mid))")
    print("relay: broker saw \(broker.published) publishes on \(broker.topics.count) topics, \(broker.refused) refused; DM statuses \(a.status(of: mid))")
    XCTAssertEqual(broker.refused, 0, "every topic is 32 lowercase hex")
    let st = ((a.ok("links.list")["links"] as? [JSONObject])?.first { $0.string("type") == "relay" })?.object("status") ?? [:]
    print("relay: A status rx_own_echo \(st["rx_own_echo"] ?? "?"), sessions \(st["sessions"] ?? "?")")
    a.stop(); b.stop()
  }
}

/// Two nodes in this process, each with its own ConnectivityController and the real Network.framework
/// transport: Bonjour discovery (advertise `EmberMesh-<token>` with the contract TXT, browse) →
/// dial by the lower token → `stream` links → a DM each way. The node tokens are injected through
/// `apply` (the core's connectivity.get supplies them once it exists).
final class NearbyEndToEndTests: XCTestCase {
  func testTwoNodesFindEachOtherAndExchangeMessages() throws {
    NearbyNetwork.allowLoopbackRoute = true   // both nodes on this one host
    defer { NearbyNetwork.allowLoopbackRoute = false }
    let a = TestNode(name: "nearA"), b = TestNode(name: "nearB")
    a.start(); b.start()
    _ = a.ok("node.set_profile", ["name": "Near A"]); _ = b.ok("node.set_profile", ["name": "Near B"])
    _ = a.ok("contacts.add", ["card": b.ok("identity.card").string("card") ?? ""])
    _ = b.ok("contacts.add", ["card": a.ok("identity.card").string("card") ?? ""])
    let tokA = String(format: "%08x", UInt32.random(in: 0x1000_0000...0x7fff_ffff))
    let tokB = String(format: "%08x", UInt32.random(in: 0x8000_0000...0xffff_ffff))
    var ctls: [ConnectivityController] = []
    for (n, tok) in [(a, tokA), (b, tokB)] {
      _ = n.call("connectivity.set", ["setup_done": true, "nearby": true])   // what the UI does when setup is applied
      let c = ConnectivityController(core: n.runner, links: n.links, transport: NearbyNetwork())
      c.relaySupported = false
      if case .success(let conn) = n.call("connectivity.get"), NearbyTXT.isToken(conn.object("nearby")?.string("token") ?? "") {
        c.apply(conn, fromUser: false)      // the core's own token
      } else {
        c.apply(["ready": true, "nearby": ["enabled": true, "active": true, "token": tok]], fromUser: false)
      }
      ctls.append(c)
    }
    defer { for c in ctls { c.apply(nil, fromUser: false) } }
    XCTAssertTrue(spin(until: 20) { !ctls[0].nearbyLinks.isEmpty && !ctls[1].nearbyLinks.isEmpty }, "nearby links on both sides")
    print("nearby: A links \(ctls[0].nearbyLinks), B links \(ctls[1].nearbyLinks)")
    XCTAssertEqual(ctls[0].nearbyLinks.count, 1)
    let la = (a.ok("links.list")["links"] as? [JSONObject])?.first
    print("nearby: A link \(la.map { JSON.encode($0).prefix(300) } ?? "none")")
    let dm1 = a.ok("messages.send", ["conv": "dm:\(b.nodeId)", "text": "hello over nearby"])
    XCTAssertNotNil(b.waitMessage("hello over nearby"), "A → B")
    _ = b.ok("messages.send", ["conv": "dm:\(a.nodeId)", "text": "hello back"])
    XCTAssertNotNil(a.waitMessage("hello back"), "B → A")
    let mid = dm1.object("message")?.string("id") ?? ""
    XCTAssertTrue(spin(until: 20) { a.status(of: mid).contains("delivered") || a.status(of: mid).contains("read") })
    a.stop(); b.stop()
  }
}
