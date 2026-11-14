import UIKit

/// The iOS host router: answers `host.*` (docs/RPC_API.md §3). Everything else goes to the core.
/// Runs on the main queue; completions are delivered on the main queue.
final class HostRouter: HostMethodHandler {
  let links: LinkManager
  private weak var runner: NodeRunner?
  weak var presenter: Presenter?
  let location = LocationService()
  let notifications: NotificationService
  let signal = SignalService()
  let files = FileService()
  let capture: CaptureService
  let accessibility = AccessibilityMonitor()
  let haptics = HapticService()
  private var scanners: [AnyObject] = []
  /// Events to the UI (host.location …).
  var onEvent: ((String, JSONObject) -> Void)?
  /// endpoint kind → compatible core link types, from the core's links.types (endpoint_kinds).
  private let typesLock = NSLock()
  private var coreLinkTypes: [String: [String]]?

  init(links: LinkManager, runner: NodeRunner?, notifications: NotificationService) {
    self.links = links
    self.runner = runner
    self.notifications = notifications
    capture = CaptureService(sink: runner)
    location.onFix = { [weak self] p in
      self?.runner?.call("position.set", p, completion: nil)
      self?.onEvent?("host.location", p)
    }
    links.compatibleLinkTypes = { [weak self] kind in self?.linkTypes(for: kind) }
    links.didCreateEndpoint = { [weak self] e in if let h = e as? HubEndpoint { self?.configureHub(h) } }
    capture.audioEndpoint = { [weak self] in self?.links.endpoints(of: AudioEndpoint.self).first { $0.isConnected } }
    capture.previewHost = { [weak self] in self?.presenter?.webContentView }
    capture.coreFeatures = { [weak runner] in (try? runner?.callSync("node.info").get())?["features"] as? [String] }
    capture.onEvent = { [weak self] n, d in self?.onEvent?(n, d) }
    signal.cameraBusy = { [weak self] in self?.capture.camera != nil || QRScannerViewController.active > 0 }
    accessibility.onChange = { [weak self] d in self?.onEvent?("host.accessibility", d) }
  }

  // MARK: link types (from the core)

  /// Compatible core link types for an endpoint kind, taken from links.types endpoint_kinds.
  /// nil until the catalogue was loaded (callers then fall back to EndpointCatalog).
  func linkTypes(for kind: String) -> [String]? {
    typesLock.lock(); defer { typesLock.unlock() }
    guard let t = coreLinkTypes else { return nil }
    return t[kind] ?? []
  }

  static func indexLinkTypes(_ types: [JSONObject]) -> [String: [String]] {
    var out: [String: [String]] = [:]
    for t in types {
      guard let type = t.string("type") else { continue }
      for k in (t["endpoint_kinds"] as? [String]) ?? [] where !(out[k] ?? []).contains(type) { out[k, default: []].append(type) }
    }
    return out
  }

  /// Loads links.types once (the catalogue is static for a given core build).
  func loadLinkTypes(completion: (() -> Void)? = nil) {
    if linkTypes(for: "tcp") != nil { completion?(); return }
    guard let runner else { completion?(); return }
    runner.call("links.types") { [weak self] r in
      if case .success(let res) = r, let types = res["types"] as? [JSONObject], !types.isEmpty, let self {
        let idx = HostRouter.indexLinkTypes(types)
        self.typesLock.lock(); self.coreLinkTypes = idx; self.typesLock.unlock()
      } else if case .failure(let e) = r {
        emberLog("links.types failed: \(e)")
      }
      completion?()
    }
  }

  static var appVersion: String { Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "0.0.0" }

  // MARK: availability

  static func availability(_ kind: String) -> (Bool, String?) {
    if let reason = EndpointCatalog.permanentlyUnsupported[kind] { return (false, reason) }
    switch kind {
    case "ble_peer", "ble": return BluetoothAvailability.check()
    case "audio": return AudioAvailability.check()
    case "udp": return MulticastEntitlement.check()
    case "multipeer", "tcp", "tcp_server", "websocket", "rtl_tcp", "loopback", "ws_server": return (true, nil)
    default: return (false, "\(kind) is not available on iOS")
    }
  }

  static func makeEndpoint(kind: String, config: JSONObject, runner: NodeRunner) throws -> Endpoint {
    switch kind {
    case "ble_peer": return BlePeerEndpoint(config: config)
    case "ble": return try BleUartEndpoint(config: config)
    case "multipeer": return MultipeerEndpoint(config: config)
    case "audio": return AudioEndpoint(config: config, runner: runner)
    case "tcp", "rtl_tcp": return try TcpEndpoint(kind: kind, config: config)
    case "tcp_server": return try TcpServerEndpoint(config: config)
    case "ws_server": return try HubEndpoint(config: config, webRoot: WebContent.bundled().hubRoot,
                                             downloads: WebContent.bundled().hubDownloads)
    case "websocket": return try WebSocketEndpoint(config: config)
    case "udp": return try UdpMulticastEndpoint(config: config)
    case "loopback": return LoopbackEndpoint()
    default: throw RPCError.unsupported(EndpointCatalog.permanentlyUnsupported[kind] ?? "\(kind) is not available on iOS")
    }
  }

  var features: [String] {
    var f = ["location", "flash", "tone", "file", "notify", "keep_awake", "background_ble", "hub", "getusermedia"]
    f += CaptureService.features
    if signal.info.string("torch") == "yes" { f.append("torch") }
    f.append("accessibility")
    f += ["nearby", "relay"]   // RPC_API §11: Bonjour/TCP nearby links; TLS WebSocket with subprotocols
    if haptics.supported { f.append("haptic") }
    if QRScannerViewController.hasCamera { f.append("qr_scan") }
    return f
  }

  /// Platform extras the shared UI lists generically (RPC_API §3 `host.info.actions`).
  static let actions: [JSONObject] = [
    ["id": "ios.settings", "title": "iOS permissions",
     "description": "Open the iOS Settings page for EmberMesh (Bluetooth, Local Network, Microphone, Camera, Location, Notifications).",
     "method": "host.ios.open_settings"],
  ]

  func info() -> JSONObject {
    let kinds = EndpointCatalog.all.map { spec -> JSONObject in
      let (ok, reason) = HostRouter.availability(spec.kind)
      return spec.json(available: ok, reason: reason, linkTypes: linkTypes(for: spec.kind).flatMap { $0.isEmpty ? nil : $0 })
    }
    return ["platform": "ios", "app_version": HostRouter.appVersion, "backend": "native", "endpoint_kinds": kinds,
            "features": features, "actions": HostRouter.actions, "accessibility": accessibility.state, "signal": signal.info, "websocket_tls": true,
            // Sound links: iOS has no serial/GPIO lines, so PTT is VOX or keyed by hand only.
            "audio": ["ptt": ["vox", "manual"], "serial_ptt": false, "presets_supported": ["handheld_vox", "acoustic", "sound_card", "custom"],
                      "presets_unsupported": ["handheld_ptt": "iPhone cannot key a PTT line (no serial/GPIO). Use the radio's VOX (handheld_vox) or an interface with its own VOX, or press PTT yourself (acoustic)."]], "os_version": UIDevice.current.systemVersion,
            "device_model": UIDevice.current.model, "core_version": EmberNode.version]
  }

  // MARK: dispatch

  func handleHost(method: String, params: JSONObject, completion: @escaping RPCCompletion) {
    if Thread.isMainThread {
      dispatch(method, params, completion)
    } else {
      DispatchQueue.main.async { self.dispatch(method, params, completion) }
    }
  }

  private func dispatch(_ method: String, _ p: JSONObject, _ done: @escaping RPCCompletion) {
    let c: RPCCompletion = { r in if Thread.isMainThread { done(r) } else { DispatchQueue.main.async { done(r) } } }
    switch method {
    case "host.info": loadLinkTypes { c(.success(self.info())) }
    case "host.endpoints.scan": scan(p, c)
    case "host.link.create": links.create(p, completion: c)
    case "host.link.remove": links.remove(p, completion: c)
    case "host.link.reconnect": links.reconnect(p, completion: c)
    case "host.location.get": location.get(c)
    case "host.location.watch":
      guard let en = p.bool("enabled") else { c(.failure(.badParams("enabled is required"))); return }
      location.watch(en, completion: c)
    case "host.qr.scan":
      guard let presenter else { c(.failure(.unsupported("no window"))); return }
      QRScannerViewController.scan(presenter: presenter, completion: c)
    case "host.file.save": files.save(params: p, presenter: presenter, completion: c)
    case "host.file.pick": files.pick(params: p, presenter: presenter, completion: c)
    case "host.signal.flash":
      let window = presenter?.hostView.window ?? UIApplication.shared.connectedScenes.compactMap { ($0 as? UIWindowScene)?.windows.first { $0.isKeyWindow } }.first
      signal.flash(params: p, window: window, completion: c)
    case "host.signal.stop": signal.stopAll(); c(.success([:]))
    case "host.haptic": c(haptics.perform(p))
    case "host.capture.start": capture.start(p, completion: c)
    case "host.capture.update": capture.update(p, completion: c)
    case "host.capture.stop": capture.stop(p, completion: c)
    case "host.signal.tone":
      let modem = links.boundIds.contains { links.endpoint(for: $0) is AudioEndpoint } || capture.mic != nil
      signal.tone(params: p, audioModemActive: modem, completion: c)
    case "host.notify":
      guard let title = p.string("title") else { c(.failure(.badParams("title is required"))); return }
      notifications.post(title: title, body: p.string("body") ?? "", kind: "host", completion: c)
    case "host.keep_awake":
      guard let en = p.bool("enabled") else { c(.failure(.badParams("enabled is required"))); return }
      UIApplication.shared.isIdleTimerDisabled = en
      c(.success(["enabled": en]))
    case "host.hub.start": hubStart(p, c)
    case "host.hub.stop": hubStop(c)
    case "host.hub.status": c(.success(hubStatus()))
    case "host.ios.open_settings":
      if let u = URL(string: UIApplication.openSettingsURLString) { UIApplication.shared.open(u) }
      c(.success([:]))
    case "host.webrtc.create_offer", "host.webrtc.accept_offer", "host.webrtc.accept_answer":
      c(.failure(.unsupported(EndpointCatalog.permanentlyUnsupported["webrtc"]!)))
    default:
      c(.failure(.unsupported("\(method) is not supported on iOS")))
    }
  }

  // MARK: host.hub.*  (the hub is the core link whose endpoint kind is ws_server)

  private var hubEndpoint: HubEndpoint? { links.endpoints(of: HubEndpoint.self).first }

  func hubStatus() -> JSONObject {
    guard let h = hubEndpoint else { return ["running": false, "urls": [String](), "clients": 0] }
    let s = h.hubStatus
    var out: JSONObject = ["running": s.bool("running") ?? false, "urls": s["urls"] ?? [String](), "clients": s.int("clients") ?? 0,
                           "port": s.int("port") ?? 0, "link_id": Int(h.linkId)]
    if !(s.bool("running") ?? false), let err = h.lastError { out["error"] = err }
    return out
  }

  /// Wires a hub endpoint created by LinkManager (host.hub.start, host.link.create or node.ready).
  func configureHub(_ h: HubEndpoint) {
    h.probe = { [weak self] in
      guard let r = try? self?.runner?.callSync("node.info").get() else { return [:] }
      var p: JSONObject = ["node_id": r.string("node_id") ?? ""]
      if let n = r.string("name"), !n.isEmpty { p["name"] = "\(n) (EmberMesh hub)" }
      return p
    }
    h.onClients = { [weak self] n in
      DispatchQueue.main.async { self?.onEvent?("host.hub.clients", ["clients": n]) }
    }
  }

  private func hubStart(_ p: JSONObject, _ c: @escaping RPCCompletion) {
    let port = p.int("port") ?? 8437
    if let h = hubEndpoint, h.isConnected {
      if (hubStatus().int("port") ?? 0) == port { c(.success(hubStatus())); return }
    }
    let start = {
      self.links.create(["name": "Hub (Wi-Fi guests)", "endpoint": ["kind": "ws_server", "config": ["port": port]],
                         "link": ["type": "raw", "config": ["preset": "websocket"]]]) { r in
        if case .failure(let e) = r { c(.failure(e)); return }
        self.waitForHub(deadline: Date().addingTimeInterval(5), c)
      }
    }
    // One hub at a time: a different port replaces the old hub link.
    if let old = hubEndpoint {
      links.remove(["id": Int(old.linkId)]) { _ in start() }
    } else {
      start()
    }
  }

  private func waitForHub(deadline: Date, _ c: @escaping RPCCompletion) {
    if let h = hubEndpoint, h.isConnected || h.lastError != nil || Date() > deadline {
      let s = hubStatus()
      if s.bool("running") == true { c(.success(s)) } else { c(.failure(.io(h.lastError ?? "the hub did not start"))) }
      return
    }
    if hubEndpoint == nil, Date() > deadline { c(.failure(.io("the hub did not start"))); return }
    DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) { self.waitForHub(deadline: deadline, c) }
  }

  private func hubStop(_ c: @escaping RPCCompletion) {
    let hubs = links.endpoints(of: HubEndpoint.self)
    guard !hubs.isEmpty else { c(.success(hubStatus())); return }
    let group = DispatchGroup()
    for h in hubs {
      group.enter()
      links.remove(["id": Int(h.linkId)]) { _ in group.leave() }
    }
    group.notify(queue: .main) { c(.success(["running": false, "urls": [String](), "clients": 0])) }
  }

  // MARK: host.endpoints.scan

  private func scan(_ p: JSONObject, _ c: @escaping RPCCompletion) {
    guard let kind = p.string("kind") else { c(.failure(.badParams("kind is required"))); return }
    let (ok, reason) = HostRouter.availability(kind)
    guard ok else { c(.failure(.unsupported(reason ?? "unavailable"))); return }
    let timeout = Double(max(500, min(30_000, p.int("timeout_ms") ?? 4000))) / 1000
    let finish: ([JSONObject], String?) -> Void = { devices, err in
      DispatchQueue.main.async {
        if devices.isEmpty, let err { c(.failure(.io(err))) } else { c(.success(["devices": devices])) }
      }
    }
    switch kind {
    case "ble":
      let s = BleScanner(services: nil, timeout: timeout, mapper: BleScanner.uartMapper)
      keep(s, for: timeout)
      s.run(finish)
    case "ble_peer":
      // Peers are connected automatically; list what an open link sees, or do a short scan.
      if let e = links.boundIds.compactMap({ links.endpoint(for: $0) as? BlePeerEndpoint }).first {
        finish(e.nearby(), nil)
        return
      }
      let s = BleScanner(services: [EmberUUID.peerService], timeout: timeout) { per, adv, rssi in
        let name = (adv["kCBAdvDataLocalName"] as? String) ?? per.name ?? "EmberMesh phone"
        return ["id": per.identifier.uuidString, "name": name, "rssi": rssi.intValue, "detail": "nearby", "config": [String: Any]()]
      }
      keep(s, for: timeout)
      s.run(finish)
    case "multipeer":
      if let e = links.boundIds.compactMap({ links.endpoint(for: $0) as? MultipeerEndpoint }).first {
        finish(e.nearby(), nil)
      } else {
        finish([], nil)
      }
    case "audio":
      finish(AudioAvailability.devices(), nil)
    case "tcp":
      let s = BonjourScanner()
      keep(s, for: timeout)
      s.run(kind: kind, timeout: timeout, completion: finish)
    default:
      c(.failure(.unsupported("\(kind) cannot be scanned")))
    }
  }

  private func keep(_ o: AnyObject, for t: TimeInterval) {
    scanners.append(o)
    DispatchQueue.main.asyncAfter(deadline: .now() + t + 1) { [weak self] in self?.scanners.removeAll { $0 === o } }
  }
}
