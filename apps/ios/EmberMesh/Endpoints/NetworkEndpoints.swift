import Foundation
import Network

enum EmberNet {
  static let bonjourType = "_embermesh._tcp"
  static let multicastGroup = "239.83.77.69"
  static let multicastPort: UInt16 = 47474

  static func tcpParameters(peerToPeer: Bool) -> NWParameters {
    let tcp = NWProtocolTCP.Options()
    tcp.noDelay = true
    tcp.enableKeepalive = true
    tcp.keepaliveIdle = 30
    tcp.connectionTimeout = 15
    let p = NWParameters(tls: nil, tcp: tcp)
    p.includePeerToPeer = peerToPeer
    return p
  }

  static func describe(_ e: NWError) -> String {
    switch e {
    case .posix(let c) where c == .ECONNREFUSED: return "connection refused (is the other side listening?)"
    case .posix(let c) where c == .EHOSTUNREACH || c == .ENETUNREACH: return "network unreachable (join the same Wi-Fi or hotspot)"
    case .posix(let c) where c == .ETIMEDOUT: return "timed out"
    case .dns: return "address not found"
    default: return e.localizedDescription
    }
  }
}

/// TCP client (stream). Also used for rtl_tcp (an SDR server is just a TCP stream).
final class TcpEndpoint: BaseEndpoint {
  private let queue = DispatchQueue(label: "org.embermesh.tcp")
  private let endpoint: NWEndpoint
  private var conn: NWConnection?
  override var wantsAutoReconnect: Bool { true }

  init(kind: String, config: JSONObject) throws {
    let cfg = EndpointConfig(config)
    let bonjour = cfg.string("bonjour")
    if !bonjour.isEmpty {
      endpoint = .service(name: bonjour, type: EmberNet.bonjourType, domain: "local.", interface: nil)
    } else {
      let host = cfg.string("host")
      let port = cfg.int("port", kind == "rtl_tcp" ? 1234 : 8001)
      guard !host.isEmpty else { throw RPCError.badParams("Address is required") }
      guard (1...65535).contains(port), let p = NWEndpoint.Port(rawValue: UInt16(port)) else { throw RPCError.badParams("port must be 1–65535") }
      endpoint = .hostPort(host: NWEndpoint.Host(host), port: p)
    }
    super.init(kind: kind)
  }

  override func open() {
    queue.async { [self] in
      self.conn?.cancel()
      let c = NWConnection(to: self.endpoint, using: EmberNet.tcpParameters(peerToPeer: true))
      c.stateUpdateHandler = { [weak self, weak c] st in
        guard let self, let c, c === self.conn else { return }
        switch st {
        case .ready: self.setConnected(true); self.receive(c)
        case .waiting(let e): self.setConnected(false, error: EmberNet.describe(e)); c.cancel()
        case .failed(let e): self.setConnected(false, error: EmberNet.describe(e))
        case .cancelled: self.setConnected(false, error: self.lastError)
        default: break
        }
      }
      self.conn = c
      c.start(queue: self.queue)
    }
  }

  private func receive(_ c: NWConnection) {
    c.receive(minimumIncompleteLength: 1, maximumLength: 64 * 1024) { [weak self] data, _, done, err in
      guard let self else { return }
      if let data, !data.isEmpty { self.deliver(data) }
      if done { self.setConnected(false, error: "closed by the other side"); c.cancel(); return }
      if let err { self.setConnected(false, error: EmberNet.describe(err)); c.cancel(); return }
      self.receive(c)
    }
  }

  override func close() {
    queue.sync { conn?.stateUpdateHandler = nil; conn?.cancel(); conn = nil }
    setConnected(false)
  }

  override func write(_ data: Data) {
    queue.async { self.conn?.send(content: data, completion: .contentProcessed { _ in }) }
  }

  override var status: JSONObject { ["remote": "\(endpoint)"] }
}

/// TCP listener (stream) with Bonjour `_embermesh._tcp` advertising and peer-to-peer Wi-Fi: the
/// entitlement-free LAN path between EmberMesh devices. All clients share one link; their streams
/// are merged without interleaving by RxArbiter and every write goes to every client.
final class TcpServerEndpoint: BaseEndpoint {
  private let queue = DispatchQueue(label: "org.embermesh.tcpserver")
  private let cfg: EndpointConfig
  private var listener: NWListener?
  private var clients: [ObjectIdentifier: NWConnection] = [:]
  private let arbiter = RxArbiter<ObjectIdentifier>(gapMs: 60)
  private var arbiterTimer: DispatchSourceTimer?
  private(set) var boundPort: UInt16 = 0
  static let maxClients = 16

  init(config: JSONObject) throws {
    cfg = EndpointConfig(config)
    let port = cfg.int("port", 47475)
    guard (0...65535).contains(port) else { throw RPCError.badParams("port must be 0–65535") }
    super.init(kind: "tcp_server")
  }

  override func open() {
    queue.async { [self] in
      self.listener?.cancel()
      let port = UInt16(self.cfg.int("port", 47475))
      let params = EmberNet.tcpParameters(peerToPeer: self.cfg.bool("peer_to_peer", true))
      params.allowLocalEndpointReuse = true
      let l: NWListener
      do {
        l = port == 0 ? try NWListener(using: params) : try NWListener(using: params, on: NWEndpoint.Port(rawValue: port)!)
      } catch {
        self.setConnected(false, error: "cannot listen on port \(port): \(error.localizedDescription)")
        return
      }
      if self.cfg.bool("advertise", true) {
        let txt = NWTXTRecord(["ember": "1", "link": "stream", "v": "1"])
        // Never name: nil: the system would publish the device name ("<Owner>'s iPhone") on the LAN.
        let instance = DefaultPeerName.resolve(self.cfg.string("service_name"))
        l.service = NWListener.Service(name: String(instance.prefix(63)), type: EmberNet.bonjourType, domain: nil, txtRecord: txt)
      }
      l.stateUpdateHandler = { [weak self, weak l] st in
        guard let self, let l, l === self.listener else { return }
        switch st {
        case .ready:
          self.boundPort = l.port?.rawValue ?? 0
          self.setConnected(true)
          self.statusChanged()
        case .failed(let e): self.setConnected(false, error: EmberNet.describe(e))
        case .waiting(let e): self.setConnected(false, error: EmberNet.describe(e))
        default: break
        }
      }
      l.newConnectionHandler = { [weak self] c in self?.accept(c) }
      self.listener = l
      l.start(queue: self.queue)
    }
  }

  private func accept(_ c: NWConnection) {
    guard clients.count < TcpServerEndpoint.maxClients else { c.cancel(); return }
    let key = ObjectIdentifier(c)
    clients[key] = c
    c.stateUpdateHandler = { [weak self] st in
      switch st {
      case .ready: self?.statusChanged()
      case .failed, .cancelled: self?.drop(key)
      default: break
      }
    }
    c.start(queue: queue)
    receive(c, key)
  }

  private func receive(_ c: NWConnection, _ key: ObjectIdentifier) {
    c.receive(minimumIncompleteLength: 1, maximumLength: 64 * 1024) { [weak self] data, _, done, err in
      guard let self else { return }
      if let data, !data.isEmpty {
        for d in self.arbiter.receive(key, data, now: monotonicMs()) { self.deliver(d) }
        self.armTimer()
      }
      if done || err != nil { c.cancel(); self.drop(key); return }
      self.receive(c, key)
    }
  }

  private func armTimer() {
    guard arbiter.hasPending, arbiterTimer == nil else { return }
    let t = DispatchSource.makeTimerSource(queue: queue)
    t.schedule(deadline: .now() + .milliseconds(30), repeating: .milliseconds(30))
    t.setEventHandler { [weak self] in
      guard let self else { return }
      for d in self.arbiter.advance(now: monotonicMs()) { self.deliver(d) }
      if !self.arbiter.hasPending { self.arbiterTimer?.cancel(); self.arbiterTimer = nil }
    }
    arbiterTimer = t
    t.resume()
  }

  private func drop(_ key: ObjectIdentifier) {
    guard clients.removeValue(forKey: key) != nil else { return }
    for d in arbiter.remove(key, now: monotonicMs()) { deliver(d) }
    statusChanged()
  }

  override func close() {
    queue.sync {
      listener?.stateUpdateHandler = nil
      listener?.cancel(); listener = nil
      for c in clients.values { c.cancel() }
      clients.removeAll()
      arbiterTimer?.cancel(); arbiterTimer = nil
    }
    setConnected(false)
  }

  override func write(_ data: Data) {
    queue.async { for c in self.clients.values { c.send(content: data, completion: .contentProcessed { _ in }) } }
  }

  override var status: JSONObject { queue.sync { ["port": Int(boundPort), "clients": clients.count, "bonjour": cfg.bool("advertise", true)] } }
}

/// Whether this build may use IPv4 multicast (com.apple.developer.networking.multicast).
enum MulticastEntitlement {
  static func check() -> (Bool, String?) {
    #if targetEnvironment(simulator)
    return (true, nil)  // the Simulator uses the Mac's network stack and does not enforce it
    #else
    guard let url = Bundle.main.url(forResource: "embedded", withExtension: "mobileprovision"),
          let data = try? Data(contentsOf: url) else {
      return (false, "This build is not signed with Apple's multicast networking permission. Use \"Local network listener\" + \"TCP connection\" (found by scanning) instead.")
    }
    let text = String(decoding: data, as: UTF8.self)
    if text.contains("com.apple.developer.networking.multicast") { return (true, nil) }
    return (false, "This build is not signed with Apple's multicast networking permission (it must be requested from Apple). Use \"Local network listener\" + \"TCP connection\" instead.")
    #endif
  }
}

/// UDP multicast 239.83.77.69:47474, one mesh frame per datagram (ARCHITECTURE §6).
final class UdpMulticastEndpoint: BaseEndpoint {
  private let queue = DispatchQueue(label: "org.embermesh.udp")
  private let group: String
  private let port: UInt16
  private var conn: NWConnectionGroup?
  override var wantsAutoReconnect: Bool { true }
  static let maxDatagram = 65_507

  init(config: JSONObject) throws {
    let cfg = EndpointConfig(config)
    group = cfg.string("group", EmberNet.multicastGroup)
    let p = cfg.int("port", Int(EmberNet.multicastPort))
    guard (1...65535).contains(p) else { throw RPCError.badParams("port must be 1–65535") }
    port = UInt16(p)
    guard IPv4Address(group) != nil, let first = Int(group.split(separator: ".").first ?? ""), (224...239).contains(first) else {
      throw RPCError.badParams("group must be an IPv4 multicast address (224.0.0.0–239.255.255.255)")
    }
    super.init(kind: "udp")
  }

  override func open() {
    queue.async { [self] in
      self.conn?.cancel()
      let desc: NWMulticastGroup
      do {
        desc = try NWMulticastGroup(for: [.hostPort(host: NWEndpoint.Host(self.group), port: NWEndpoint.Port(rawValue: self.port)!)])
      } catch {
        self.setConnected(false, error: "multicast group error: \(error.localizedDescription)")
        return
      }
      let params = NWParameters.udp
      params.allowLocalEndpointReuse = true
      let g = NWConnectionGroup(with: desc, using: params)
      g.setReceiveHandler(maximumMessageSize: UdpMulticastEndpoint.maxDatagram, rejectOversizedMessages: true) { [weak self] _, content, _ in
        if let content, !content.isEmpty { self?.deliver(content) }
      }
      g.stateUpdateHandler = { [weak self, weak g] st in
        guard let self, let g, g === self.conn else { return }
        switch st {
        case .ready: self.setConnected(true)
        case .failed(let e): self.setConnected(false, error: "multicast unavailable: \(EmberNet.describe(e))")
        case .waiting(let e): self.setConnected(false, error: EmberNet.describe(e))
        case .cancelled: self.setConnected(false, error: self.lastError)
        default: break
        }
      }
      self.conn = g
      g.start(queue: self.queue)
    }
  }

  override func close() {
    queue.sync { conn?.stateUpdateHandler = nil; conn?.cancel(); conn = nil }
    setConnected(false)
  }

  override func write(_ data: Data) {
    guard data.count <= UdpMulticastEndpoint.maxDatagram else { return }
    queue.async { self.conn?.send(content: data) { _ in } }
  }

  override var status: JSONObject { ["group": group, "port": Int(port)] }
}

/// WebSocket client for hub guest mode: ws://<hub>:8437/ws, one binary message per mesh frame.
final class WebSocketEndpoint: BaseEndpoint, URLSessionWebSocketDelegate {
  private let url: URL
  private var session: URLSession?
  private var task: URLSessionWebSocketTask?
  private let opQueue: OperationQueue = { let q = OperationQueue(); q.maxConcurrentOperationCount = 1; return q }()
  private var pingTimer: DispatchSourceTimer?
  override var wantsAutoReconnect: Bool { true }

  init(config: JSONObject) throws {
    let s = EndpointConfig(config).string("url")
    guard let u = URL(string: s), let scheme = u.scheme?.lowercased(), scheme == "ws" || scheme == "wss", u.host != nil else {
      throw RPCError.badParams("Hub address must look like ws://192.168.4.1:8437/ws")
    }
    url = u
    super.init(kind: "websocket")
  }

  override func open() {
    opQueue.addOperation {
      self.task?.cancel(with: .goingAway, reason: nil)
      let cfg = URLSessionConfiguration.ephemeral
      cfg.waitsForConnectivity = false
      cfg.timeoutIntervalForRequest = 15
      let s = URLSession(configuration: cfg, delegate: self, delegateQueue: self.opQueue)
      let t = s.webSocketTask(with: self.url)
      t.maximumMessageSize = 1 << 20
      self.session = s
      self.task = t
      t.resume()
    }
  }

  private func receive(_ t: URLSessionWebSocketTask) {
    t.receive { [weak self, weak t] result in
      guard let self, let t, t === self.task else { return }
      switch result {
      case .success(.data(let d)): self.deliver(d); self.receive(t)
      case .success(.string(let s)): self.deliver(Data(s.utf8)); self.receive(t)
      case .success: self.receive(t)
      case .failure(let e): self.setConnected(false, error: e.localizedDescription)
      }
    }
  }

  func urlSession(_ session: URLSession, webSocketTask: URLSessionWebSocketTask, didOpenWithProtocol protocol: String?) {
    guard webSocketTask === task else { return }
    setConnected(true)
    receive(webSocketTask)
    let t = DispatchSource.makeTimerSource(queue: .global(qos: .utility))
    t.schedule(deadline: .now() + 20, repeating: 20)
    t.setEventHandler { [weak webSocketTask] in webSocketTask?.sendPing { _ in } }
    pingTimer?.cancel()
    pingTimer = t
    t.resume()
  }

  func urlSession(_ session: URLSession, webSocketTask: URLSessionWebSocketTask, didCloseWith code: URLSessionWebSocketTask.CloseCode, reason: Data?) {
    guard webSocketTask === task else { return }
    setConnected(false, error: "hub closed the connection (\(code.rawValue))")
  }

  func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: Error?) {
    guard task === self.task else { return }
    pingTimer?.cancel(); pingTimer = nil
    setConnected(false, error: error?.localizedDescription ?? "disconnected")
  }

  override func close() {
    opQueue.addOperation {
      self.pingTimer?.cancel(); self.pingTimer = nil
      self.task?.cancel(with: .goingAway, reason: nil)
      self.task = nil
      self.session?.invalidateAndCancel()
      self.session = nil
    }
    opQueue.waitUntilAllOperationsAreFinished()
    setConnected(false)
  }

  override func write(_ data: Data) {
    opQueue.addOperation { self.task?.send(.data(data)) { _ in } }
  }

  override var status: JSONObject { ["url": url.absoluteString] }
}

/// Bonjour browse for `_embermesh._tcp` (EmberMesh tcp_server listeners and desktop hubs).
/// Multipeer sessions also use `_embermesh._tcp` (service name "embermesh"); entries without our
/// TXT record are reported as such so the UI can tell them apart.
final class BonjourScanner {
  private let queue = DispatchQueue(label: "org.embermesh.bonjour")
  private var browser: NWBrowser?
  private var results: [JSONObject] = []

  func run(kind: String, timeout: TimeInterval, completion: @escaping ([JSONObject], String?) -> Void) {
    let params = NWParameters()
    params.includePeerToPeer = true
    let b = NWBrowser(for: .bonjourWithTXTRecord(type: EmberNet.bonjourType, domain: nil), using: params)
    var failure: String?
    b.stateUpdateHandler = { st in
      if case .failed(let e) = st { failure = EmberNet.describe(e) }
      if case .waiting(let e) = st { failure = "Local network permission is needed (Settings > EmberMesh > Local Network): \(e.localizedDescription)" }
    }
    b.browseResultsChangedHandler = { [weak self] set, _ in
      guard let self else { return }
      self.results = set.compactMap { r -> JSONObject? in
        guard case .service(let name, _, _, _) = r.endpoint else { return nil }
        var isEmber = false
        if case .bonjour(let txt) = r.metadata { isEmber = txt["ember"] == "1" }
        if !isEmber { return nil }  // Multipeer advertisements share the type; skip them
        var cfg: JSONObject = ["bonjour": name, "host": "", "port": 0]
        if kind == "websocket" { cfg = ["url": ""] }
        return ["id": name, "name": name, "detail": "EmberMesh on the local network", "config": cfg]
      }
    }
    browser = b
    b.start(queue: queue)
    queue.asyncAfter(deadline: .now() + timeout) { [self] in
      b.cancel()
      self.browser = nil
      completion(self.results, self.results.isEmpty ? failure : nil)
    }
  }
}
