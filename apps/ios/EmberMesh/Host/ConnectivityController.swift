import Network
import UIKit

/// Follows the core's connectivity state (RPC_API §11, revision 2) and makes links appear by
/// themselves:
///   * Nearby devices, while `nearby.active` (= enabled AND the core's `ready`: setup done and
///     unlocked) and the app is in the foreground: listen on the local network, advertise
///     `EmberMesh-<token>`, browse, dial per the rule; every connection becomes a `stream` link with
///     `preset:"nearby"` (the core sends and requires the 8-byte hello), `auto:"nearby"`,
///     `auto_key` `"<token>@<address>"` (dial) / `"@<address>"` (accept). v1 Nearby is Wi-Fi/LAN only:
///     there is no automatic Bluetooth link (a user adds `ble_peer` by hand on the Links screen), so
///     the Nearby switch never triggers the Bluetooth permission.
///   * Internet relay, while `internet.active` and in the foreground: one `relay` link per enabled
///     relay of the core's list (`auto:"relay"`, `auto_key` = url); credentials are the core's.
///   * `link.removed` closes the endpoint; `ready:false` (lock, wipe, duress), Nearby off and the
///     background stop everything; a network path change calls `connectivity.network_changed` and
///     re-advertises under the new token.
/// The Local Network prompt therefore appears when the UI applies setup
/// (`connectivity.set {setup_done:true}`), never at a fresh launch.
/// Main queue only.
final class ConnectivityController: NearbyTransportDelegate {
  private weak var core: LinkCore?
  private let links: LinkManager
  let transport: NearbyTransport
  /// Monotonic ms (injectable).
  var now: () -> Double = { Double(DispatchTime.now().uptimeNanoseconds) / 1e6 }
  /// Whether this build can run relay links (TLS WebSocket with subprotocols).
  var relaySupported = true

  private(set) var state: JSONObject?
  private(set) var foreground = true
  private(set) var policy: NearbyPolicy?
  private(set) var nearbyLinks: [UInt32: (token: String?, ip: String)] = [:]
  private(set) var relayLinks: [String: UInt32] = [:]   // url -> link id
  private var relayPending: Set<String> = []
  private var timer: Timer?
  private var observers: [NSObjectProtocol] = []

  private var pathMonitor: NWPathMonitor?
  private var lastPathKey: String?

  init(core: LinkCore, links: LinkManager, transport: NearbyTransport) {
    self.core = core
    self.links = links
    self.transport = transport
    transport.delegate = self
    links.onEndpointDown = { [weak self] id, e in self?.endpointDown(id, e) }
  }

  func observeApp() {
    // Network changes (Wi-Fi joined/left, interface addresses): new token, re-advertise.
    let m = NWPathMonitor()
    m.pathUpdateHandler = { [weak self] path in
      let key = path.availableInterfaces.filter { $0.type == .wifi || $0.type == .wiredEthernet }.map { "\($0.name)" }.sorted().joined(separator: ",")
        + "|" + "\(path.status)"
      DispatchQueue.main.async { self?.pathChanged(key) }
    }
    m.start(queue: DispatchQueue(label: "org.embermesh.path"))
    pathMonitor = m
    let nc = NotificationCenter.default
    observers.append(nc.addObserver(forName: UIApplication.didEnterBackgroundNotification, object: nil, queue: .main) { [weak self] _ in self?.setForeground(false) })
    observers.append(nc.addObserver(forName: UIApplication.willEnterForegroundNotification, object: nil, queue: .main) { [weak self] _ in self?.setForeground(true) })
  }

  /// A network path change: ask the core for a new nearby token (connectivity.network_changed);
  /// apply() re-advertises when the token differs.
  func pathChanged(_ key: String) {
    defer { lastPathKey = key }
    guard let old = lastPathKey, old != key, policy != nil else { return }
    core?.call("connectivity.network_changed", [:]) { [weak self] r in
      guard let self, case .success(let res) = r, let t = res.string("token"), NearbyTXT.isToken(t) else { return }
      if var st = self.state, var nb = st.object("nearby") {
        nb["token"] = t; st["nearby"] = nb
        self.apply(st, fromUser: false)
      }
    }
  }

  // MARK: inputs

  /// node.ready (and foreground): ask the core.
  func refresh() {
    core?.call("connectivity.get", [:]) { [weak self] r in
      switch r {
      case .success(let c): self?.apply(c, fromUser: false)
      case .failure(let e) where e.code == "locked": self?.apply(nil, fromUser: false)
      case .failure: break   // core without connectivity (older build): nothing to do
      }
    }
  }

  /// Core events relevant here (main queue).
  func coreEvent(_ name: String, _ data: JSONObject) {
    switch name {
    case "connectivity.changed": apply(data, fromUser: true)
    case "link.removed":
      if let raw = data.int("id"), raw > 0 { removedByCore(UInt32(raw)) }
    case "node.ready": refresh()
    default: break
    }
  }

  func setForeground(_ fg: Bool) {
    foreground = fg
    if fg { refresh() } else { apply(state, fromUser: false) }
  }

  // MARK: reconcile

  /// `fromUser` is kept for callers; gating is entirely the core's `ready` / `*.active`.
  func apply(_ c: JSONObject?, fromUser: Bool) {
    state = c
    let ready = c?.bool("ready") == true
    let nearby = c?.object("nearby")
    let token = nearby?.string("token") ?? ""
    let wantNearby = ready && nearby?.bool("active") == true && foreground && NearbyTXT.isToken(token)
    if wantNearby {
      if policy?.myToken != token { startNearby(token: token) }
    } else {
      stopNearby()
    }
    let internet = c?.object("internet")
    let relays = (internet?["relays"] as? [JSONObject]) ?? []
    var want: [String: JSONObject] = [:]
    if relaySupported, foreground, ready, internet?.bool("active") == true {
      for r in relays where r.bool("enabled") == true { if let u = r.string("url"), !u.isEmpty { want[u] = r } }
    }
    syncRelays(want)
  }

  // MARK: nearby

  private func startNearby(token: String) {
    stopNearby()
    policy = NearbyPolicy(myToken: token)
    transport.start(token: token)
    let t = Timer(timeInterval: 1, repeats: true) { [weak self] _ in self?.tick() }
    RunLoop.main.add(t, forMode: .common)
    timer = t
  }

  private func stopNearby() {
    timer?.invalidate(); timer = nil
    if transport.isRunning { transport.stop() }
    policy = nil
    for id in Array(nearbyLinks.keys) { removeLink(id) }
    nearbyLinks.removeAll()
  }

  func tick() {
    guard let p = policy else { return }
    for t in p.tick(now: now()) { transport.dial(token: t) }
  }

  func nearbyDiscovered(_ token: String) {
    guard let p = policy else { return }
    for t in p.discovered(token, now: now()) { transport.dial(token: t) }
  }

  func nearbyLost(_ token: String) { policy?.lost(token) }

  func nearbyResolved(_ token: String, addresses: [String]) { policy?.resolved(token, addresses: addresses) }

  func nearbyShouldAccept(remoteIP: String) -> Bool { policy?.shouldAccept(ip: remoteIP, now: now()) ?? false }

  func nearbyDialFailed(_ token: String) { policy?.dialFinished(token, connected: false, remoteIP: nil) }

  func nearbyConnected(_ endpoint: Endpoint, role: String, token: String?, remoteIP: String) {
    guard let p = policy else { endpoint.close(); return }
    var attributed = token
    if role == "dial", let t = token { p.dialFinished(t, connected: true, remoteIP: remoteIP) }
    else { attributed = p.accepted(ip: remoteIP) }
    let addr = LocalNet.normalize(remoteIP)
    let key = String((role == "dial" ? "\(token ?? "")@\(addr)" : "@\(addr)").prefix(64))
    let params: JSONObject = ["name": "Nearby device", "link": ["type": "stream", "config": ["preset": "nearby"]],
                              "auto": "nearby", "auto_role": role, "auto_key": key]
    links.createAuto(endpoint: endpoint, descriptor: EndpointDescriptor(kind: "tcp", config: ["nearby": true], autostart: true),
                     params: params) { [weak self] r in
      guard let self else { return }
      switch r {
      case .success(let res):
        guard let id = res.object("link")?.int("id") else { return }
        if self.policy !== p { self.removeLink(UInt32(id)); return }   // nearby went off meanwhile
        self.nearbyLinks[UInt32(id)] = (attributed, remoteIP)
      case .failure:
        endpoint.close()   // refused by the core (busy duplicate, 16 links, locked)
        p.closed(token: attributed, ip: remoteIP)
      }
    }
  }

  /// A nearby connection dropped: the link cannot come back (it is a socket), remove it; the peer
  /// is dialed again by the rule if it is still advertised.
  private func endpointDown(_ id: UInt32, _ e: Endpoint) {
    guard let info = nearbyLinks.removeValue(forKey: id) else { return }
    policy?.closed(token: info.token, ip: info.ip)
    removeLink(id)
  }

  private func removedByCore(_ id: UInt32) {
    links.coreRemoved(id)
    if let info = nearbyLinks.removeValue(forKey: id) { policy?.closed(token: info.token, ip: info.ip) }
    if let (u, _) = relayLinks.first(where: { $0.value == id }) { relayLinks[u] = nil }
  }

  private func removeLink(_ id: UInt32) {
    links.remove(["id": Int(id)]) { _ in }
  }

  // MARK: relay

  private func syncRelays(_ want: [String: JSONObject]) {
    for (u, id) in relayLinks where want[u] == nil { relayLinks[u] = nil; removeLink(id) }
    for (u, r) in want where relayLinks[u] == nil && !relayPending.contains(u) {
      relayPending.insert(u)
      var ep: JSONObject = ["url": u, "protocols": ["mqtt"]]
      if r.bool("allow_plaintext") == true { ep["allow_plaintext"] = true }
      let cfg: JSONObject = ["url": u]   // credentials are attached by the core
      links.create(["name": "Internet relay", "endpoint": ["kind": "websocket", "config": ep],
                    "link": ["type": "relay", "config": cfg], "persist": false,
                    "auto": "relay", "auto_key": u]) { [weak self] res in
        guard let self else { return }
        self.relayPending.remove(u)
        guard case .success(let o) = res, let id = o.object("link")?.int("id") else {
          if case .failure(let e) = res { emberLog("relay link not created: \(e.code)") }
          return
        }
        // Still wanted? (consent withdrawn or background meanwhile)
        let still = self.foreground && self.relaySupported && self.state?.bool("ready") == true
          && self.state?.object("internet")?.bool("active") == true
          && ((self.state?.object("internet")?["relays"] as? [JSONObject]) ?? []).contains { $0.string("url") == u && $0.bool("enabled") == true }
        if still { self.relayLinks[u] = UInt32(id) } else { self.removeLink(UInt32(id)) }
      }
    }
  }
}
