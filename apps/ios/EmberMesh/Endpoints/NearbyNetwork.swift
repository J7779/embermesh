import Foundation
import Network

/// What ConnectivityController needs from the nearby transport (Network.framework in the app, a
/// fake in tests). All callbacks on the main queue.
protocol NearbyTransport: AnyObject {
  var delegate: NearbyTransportDelegate? { get set }
  var isRunning: Bool { get }
  /// Listen (any port), advertise `EmberMesh-<token>` with the contract TXT, and browse.
  func start(token: String)
  /// Stop advertising, browsing and listening; cancel pending dials. Established connections belong
  /// to their links and are closed by the controller.
  func stop()
  func dial(token: String)
}

protocol NearbyTransportDelegate: AnyObject {
  func nearbyDiscovered(_ token: String)
  func nearbyLost(_ token: String)
  func nearbyResolved(_ token: String, addresses: [String])
  /// Asked before an inbound connection is accepted (per-address limits).
  func nearbyShouldAccept(remoteIP: String) -> Bool
  /// A connection is established: role "dial" (token known) or "accept".
  func nearbyConnected(_ endpoint: Endpoint, role: String, token: String?, remoteIP: String)
  func nearbyDialFailed(_ token: String)
}

/// One TCP connection = one nearby link (the core's `stream` link, preset `tcp`). Wraps an already
/// established NWConnection; never reconnects (the controller dials again when the peer is still
/// advertised).
final class NearbyConnectionEndpoint: BaseEndpoint {
  let connection: NWConnection
  let remoteIP: String
  let token: String?
  let role: String
  private let queue: DispatchQueue
  private var opened = false

  init(connection: NWConnection, queue: DispatchQueue, remoteIP: String, token: String?, role: String) {
    self.connection = connection
    self.queue = queue
    self.remoteIP = remoteIP
    self.token = token
    self.role = role
    super.init(kind: "tcp")
  }

  override var wantsAutoReconnect: Bool { false }

  override func open() {
    queue.async { [self] in
      guard !self.opened else { return }
      self.opened = true
      self.connection.stateUpdateHandler = { [weak self] st in
        switch st {
        case .failed(let e): self?.setConnected(false, error: EmberNet.describe(e))
        case .cancelled: self?.setConnected(false)
        default: break
        }
      }
      self.setConnected(true)
      self.receive()
    }
  }

  private func receive() {
    connection.receive(minimumIncompleteLength: 1, maximumLength: 64 * 1024) { [weak self] data, _, done, err in
      guard let self else { return }
      if let data, !data.isEmpty { self.deliver(data) }
      if done || err != nil { self.setConnected(false, error: done ? "closed by the other side" : err.map(EmberNet.describe)); self.connection.cancel(); return }
      self.receive()
    }
  }

  override func close() {
    queue.sync { connection.stateUpdateHandler = nil; connection.cancel() }
    setConnected(false)
  }

  override func write(_ data: Data) {
    queue.async { self.connection.send(content: data, completion: .contentProcessed { _ in }) }
  }

  override var status: JSONObject { ["nearby": true, "role": role, "remote": remoteIP] }
}

/// Network.framework transport: NWListener (any free port, 47475 preferred) advertising
/// `_embermesh._tcp` instance `EmberMesh-<token>` with TXT exactly ember=1 v=1 link=stream t=<token>,
/// NWBrowser for the same type, NetService resolution of peer addresses (attribution of inbound
/// connections), and dialing the service endpoint: Network.framework then tries every resolved
/// address itself with Happy Eyeballs (IPv6 first, IPv4 after ~250 ms; link-local included).
final class NearbyNetwork: NSObject, NearbyTransport, NetServiceDelegate {
  weak var delegate: NearbyTransportDelegate?
  private let queue = DispatchQueue(label: "org.embermesh.nearby")
  private var listener: NWListener?
  private var browser: NWBrowser?
  private var token = ""
  private var instances: [String: (name: String, iface: NWInterface?)] = [:]   // token -> service
  private var addresses: [String: [String]] = [:]   // token -> resolved addresses (with zone for link-local)
  private var ports: [String: UInt16] = [:]         // token -> resolved port
  private var pendingDials: Set<String> = []        // dial asked before the instance was resolved
  private var resolving: [(service: NetService, token: String)] = []   // token from the browsed TXT `t`
  private var dials: [String: NWConnection] = [:]
  private(set) var isRunning = false

  /// TCP on the local network only: never cellular, never Apple peer-to-peer (AWDL), never loopback.
  /// (Tests running two nodes in one process reach their own host's address over loopback:
  /// `allowLoopbackRoute`.)
  static var allowLoopbackRoute = false
  static func params() -> NWParameters {
    let p = EmberNet.tcpParameters(peerToPeer: false)
    p.allowLocalEndpointReuse = true
    p.prohibitedInterfaceTypes = allowLoopbackRoute ? [.cellular] : [.cellular, .loopback]
    p.prohibitExpensivePaths = true
    return p
  }

  func start(token: String) {
    guard NearbyTXT.isToken(token) else { return }
    stop()
    self.token = token
    isRunning = true
    startListener(token: token, preferredPort: true)

    let b = NWBrowser(for: .bonjourWithTXTRecord(type: NearbyTXT.serviceType, domain: nil), using: NWParameters())
    b.browseResultsChangedHandler = { [weak self] results, _ in self?.browsed(results) }
    browser = b
    b.start(queue: queue)
  }

  /// Listens on 47475 if free, otherwise on any free port (re-advertised with the same instance).
  private func startListener(token: String, preferredPort: Bool) {
    let name = NearbyTXT.instancePrefix + token
    let txt = NWTXTRecord(NearbyTXT.record(token: token))
    let l: NWListener
    do {
      if preferredPort, let p = try? NWListener(using: NearbyNetwork.params(), on: 47475) { l = p }
      else { l = try NWListener(using: NearbyNetwork.params()) }
    } catch {
      emberLog("nearby: cannot listen (\(error.localizedDescription))")
      return
    }
    l.service = NWListener.Service(name: name, type: NearbyTXT.serviceType, domain: nil, txtRecord: txt)
    l.newConnectionHandler = { [weak self] c in self?.inbound(c) }
    l.stateUpdateHandler = { [weak self, weak l] st in
      guard let self, let l, case .failed(let e) = st else { return }
      DispatchQueue.main.async {
        guard self.isRunning, self.listener === l, self.token == token else { return }
        l.cancel()
        self.listener = nil
        if preferredPort {
          self.startListener(token: token, preferredPort: false)   // 47475 taken: any free port
        } else {
          emberLog("nearby listener failed: \(EmberNet.describe(e))")
        }
      }
    }
    listener = l
    l.start(queue: queue)
  }

  func stop() {
    isRunning = false
    listener?.newConnectionHandler = nil
    listener?.cancel(); listener = nil
    browser?.cancel(); browser = nil
    queue.sync { for c in dials.values { c.cancel() }; dials.removeAll(); instances.removeAll(); addresses.removeAll(); ports.removeAll(); pendingDials.removeAll() }
    for r in resolving { r.service.stop() }
    resolving.removeAll()
  }

  // MARK: browse (queue)

  private func browsed(_ results: Set<NWBrowser.Result>) {
    var seen: [String: (String, NWInterface?)] = [:]
    for r in results {
      guard case .service(let name, _, _, let iface) = r.endpoint, case .bonjour(let txt) = r.metadata else { continue }
      guard let t = NearbyTXT.peerToken(txt.dictionary), t != token else { continue }
      seen[t] = (name, iface)
    }
    let added = seen.keys.filter { instances[$0] == nil }
    let removed = instances.keys.filter { seen[$0] == nil }
    instances = seen.mapValues { (name: $0.0, iface: $0.1) }
    DispatchQueue.main.async {
      for t in removed { self.delegate?.nearbyLost(t) }
      for t in added.sorted() {
        self.delegate?.nearbyDiscovered(t)
        if let name = seen[t]?.0 { self.resolve(token: t, name: name) }
      }
    }
  }

  /// Addresses of a peer (main queue; NetService needs a run loop).
  private func resolve(token: String, name: String) {
    let s = NetService(domain: "local.", type: NearbyTXT.serviceType + ".", name: name)
    s.delegate = self
    resolving.append((s, token))
    s.resolve(withTimeout: 5)
  }

  func netServiceDidResolveAddress(_ sender: NetService) {
    let scoped = (sender.addresses ?? []).compactMap { NearbyNetwork.ip(fromSockaddr: $0, keepZone: true) }
    if let t = resolving.first(where: { $0.service === sender })?.token {
      let port = UInt16(max(0, min(65535, sender.port)))
      queue.async {
        self.addresses[t] = scoped; self.ports[t] = port
        if self.pendingDials.remove(t) != nil { self.dial(token: t) }
      }
      delegate?.nearbyResolved(t, addresses: scoped.map(NearbyNetwork.normalizeIP))
    }
    sender.stop()
    resolving.removeAll { $0.service === sender }
  }

  func netService(_ sender: NetService, didNotResolve errorDict: [String: NSNumber]) {
    if let t = resolving.first(where: { $0.service === sender })?.token {
      queue.async { if self.pendingDials.remove(t) != nil { DispatchQueue.main.async { self.delegate?.nearbyDialFailed(t) } } }
    }
    resolving.removeAll { $0.service === sender }
  }

  static func ip(fromSockaddr d: Data, keepZone: Bool = false) -> String? {
    d.withUnsafeBytes { raw -> String? in
      guard let sa = raw.baseAddress?.assumingMemoryBound(to: sockaddr.self) else { return nil }
      var host = [CChar](repeating: 0, count: Int(NI_MAXHOST))
      guard getnameinfo(sa, socklen_t(d.count), &host, socklen_t(host.count), nil, 0, NI_NUMERICHOST) == 0 else { return nil }
      let s = String(cString: host)
      return keepZone ? s : normalizeIP(s)
    }
  }

  /// "fe80::1%en0" → "fe80::1"; "::ffff:1.2.3.4" → "1.2.3.4".
  static func normalizeIP(_ s: String) -> String {
    var v = s
    if let i = v.firstIndex(of: "%") { v = String(v[..<i]) }
    if v.lowercased().hasPrefix("::ffff:"), v.contains(".") { v = String(v.dropFirst(7)) }
    return v.lowercased()
  }

  static func remoteIP(_ e: NWEndpoint?) -> String? {
    guard case .hostPort(let host, _)? = e else { return nil }
    switch host {
    case .ipv4(let a): return normalizeIP("\(a)")
    case .ipv6(let a): return normalizeIP("\(a)")
    case .name(let n, _): return normalizeIP(n)
    @unknown default: return nil
    }
  }

  // MARK: dial / accept

  /// Dials only on-link/private addresses on an interface subnet, at most 4 per instance, IPv6
  /// link-local first then IPv4, 250 ms apart; the first to connect wins, the others are cancelled.
  func dial(token t: String) {
    queue.async { [self] in
      guard self.isRunning, self.dials[t] == nil else { return }
      if self.addresses[t] == nil, self.instances[t] != nil { self.pendingDials.insert(t); return }   // dial once resolved
      let addrs = LocalNet.dialOrder(self.addresses[t] ?? [])
      guard !addrs.isEmpty else {
        DispatchQueue.main.async { self.delegate?.nearbyDialFailed(t) }   // not resolved (yet) or nothing on-link
        return
      }
      let port = self.ports[t] ?? 0
      guard let p = NWEndpoint.Port(rawValue: port), port != 0 else { DispatchQueue.main.async { self.delegate?.nearbyDialFailed(t) }; return }
      var attempts: [NWConnection] = []
      var finished = false
      var failed = 0
      let marker = NWConnection(to: .hostPort(host: "0.0.0.0", port: 9), using: .tcp)   // placeholder for dials[t]
      self.dials[t] = marker
      func done(_ winner: NWConnection?) {
        guard !finished else { return }
        finished = true
        self.dials[t] = nil
        for c in attempts where c !== winner { c.stateUpdateHandler = nil; c.cancel() }
        guard let c = winner else { DispatchQueue.main.async { self.delegate?.nearbyDialFailed(t) }; return }
        c.stateUpdateHandler = nil
        let ip = NearbyNetwork.remoteIP(c.currentPath?.remoteEndpoint) ?? "?"
        let ep = NearbyConnectionEndpoint(connection: c, queue: self.queue, remoteIP: ip, token: t, role: "dial")
        DispatchQueue.main.async { self.delegate?.nearbyConnected(ep, role: "dial", token: t, remoteIP: ip) }
      }
      for (k, a) in addrs.enumerated() {
        self.queue.asyncAfter(deadline: .now() + .milliseconds(250 * k)) {
          guard !finished, self.isRunning else { return }
          let c = NWConnection(to: .hostPort(host: NWEndpoint.Host(a), port: p), using: NearbyNetwork.params())
          attempts.append(c)
          c.stateUpdateHandler = { st in
            switch st {
            case .ready: done(c)
            case .failed, .waiting:
              c.cancel()
              failed += 1
              if failed == addrs.count { done(nil) }
            default: break
            }
          }
          c.start(queue: self.queue)
        }
      }
    }
  }

  private func inbound(_ c: NWConnection) {
    guard let ip = NearbyNetwork.remoteIP(c.endpoint) else { c.cancel(); return }
    DispatchQueue.main.async { [self] in
      guard self.isRunning, self.delegate?.nearbyShouldAccept(remoteIP: ip) == true else { c.cancel(); return }
      self.queue.async { [self, c] in
        var done = false
        c.stateUpdateHandler = { [weak self, weak c] st in
          guard let self, let c, !done else { return }
          switch st {
          case .ready:
            done = true
            c.stateUpdateHandler = nil
            let ep = NearbyConnectionEndpoint(connection: c, queue: self.queue, remoteIP: ip, token: nil, role: "accept")
            DispatchQueue.main.async { self.delegate?.nearbyConnected(ep, role: "accept", token: nil, remoteIP: ip) }
          case .failed, .cancelled: done = true; c.cancel()
          default: break
          }
        }
        c.start(queue: self.queue)
      }
    }
  }
}
