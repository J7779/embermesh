import Foundation

/// What LinkManager needs from the core (NodeRunner in the app, a fake in tests).
protocol LinkCore: AnyObject {
  func call(_ method: String, _ params: JSONObject, completion: RPCCompletion?)
  func linkRx(_ linkId: UInt32, _ data: Data)
}

extension NodeRunner: LinkCore {}

/// The endpoint half of a core link, as stored (opaquely) by the core in `Link.endpoint`.
struct EndpointDescriptor: Equatable {
  var kind: String
  var config: JSONObject
  /// host.link.create `autostart` (default true); false = the endpoint is only opened on reconnect.
  var autostart: Bool

  var json: JSONObject { ["kind": kind, "config": config, "autostart": autostart] }

  init(kind: String, config: JSONObject, autostart: Bool = true) {
    self.kind = kind; self.config = config; self.autostart = autostart
  }

  init?(json o: JSONObject?) {
    guard let o, let kind = o.string("kind"), !kind.isEmpty else { return nil }
    self.kind = kind
    config = o.object("config") ?? [:]
    autostart = o.bool("autostart") ?? true
  }

  /// Same endpoint (kind + config); autostart does not matter for an endpoint that is already open.
  func sameEndpoint(_ o: EndpointDescriptor) -> Bool {
    kind == o.kind && JSON.encode(config, sorted: true) == JSON.encode(o.config, sorted: true)
  }

  static func == (a: EndpointDescriptor, b: EndpointDescriptor) -> Bool { a.sameEndpoint(b) && a.autostart == b.autostart }
}

/// Owns the endpoints under core links. The core is the single source of truth for which links
/// exist (docs/RPC_API.md "Link persistence (normative)"): links.add persists the link together
/// with the opaque endpoint descriptor {kind, config, autostart}, and on every `node.ready` the
/// manager runs `syncWithCore()`: links.list → (re)open the endpoint of every link whose kind this
/// device provides → links.endpoint_state. There is no separate saved-link list on iOS.
///
/// Also: host.link.create / remove / reconnect, endpoint ↔ core byte plumbing, endpoint state
/// reporting and reconnect-with-backoff for client endpoints. Public methods are called on the main
/// queue (completions on the main queue).
final class LinkManager: EndpointDelegate {
  typealias Factory = (_ kind: String, _ config: JSONObject) throws -> Endpoint
  typealias Availability = (_ kind: String) -> (Bool, String?)

  private weak var core: LinkCore?
  private let factory: Factory
  private let availability: Availability
  /// Core link types usable on an endpoint kind (from links.types endpoint_kinds); nil = not known yet.
  var compatibleLinkTypes: (String) -> [String]? = { _ in nil }
  /// UI events (host.endpoint.state). Called on the main queue.
  var onEvent: ((String, JSONObject) -> Void)?
  /// Called (main queue) for every endpoint the factory creates, before it is opened.
  var didCreateEndpoint: ((Endpoint) -> Void)?

  private let lock = NSLock()
  private var bound: [UInt32: Endpoint] = [:]
  /// Runtime mirror of the core's Link.endpoint for links this manager knows (refreshed by sync).
  private(set) var descriptors: [UInt32: EndpointDescriptor] = [:]
  private var backoff: [UInt32: Int] = [:]
  private var pendingReconnect: Set<UInt32> = []
  private(set) var syncCount = 0

  init(core: LinkCore, factory: @escaping Factory, availability: @escaping Availability) {
    self.core = core
    self.factory = factory
    self.availability = availability
  }

  func endpoint(for id: UInt32) -> Endpoint? { lock.lock(); defer { lock.unlock() }; return bound[id] }
  var boundIds: [UInt32] { lock.lock(); defer { lock.unlock() }; return Array(bound.keys).sorted() }
  func endpoints<T>(of type: T.Type) -> [T] { boundIds.compactMap { endpoint(for: $0) as? T } }

  /// EMBER_OUT_LINK_TX from the node queue.
  func handleLinkTx(_ id: UInt32, _ data: Data) {
    lock.lock(); let e = bound[id]; lock.unlock()
    e?.write(data)
  }

  // MARK: host.link.create

  func create(_ p: JSONObject, completion: @escaping RPCCompletion) {
    guard let ep = p.object("endpoint"), let kind = ep.string("kind"), !kind.isEmpty else {
      completion(.failure(.badParams("endpoint.kind is required"))); return
    }
    guard let spec = EndpointCatalog.spec(kind) else { completion(.failure(.unsupported("unknown endpoint kind \(kind)"))); return }
    let (ok, reason) = availability(kind)
    guard ok else { completion(.failure(.unsupported(reason ?? "\(kind) is not available on this device"))); return }
    let link = p.object("link") ?? [:]
    let compatible = compatibleLinkTypes(kind) ?? spec.linkTypes
    let type = link.string("type").flatMap { $0.isEmpty ? nil : $0 } ?? spec.defaultLinkType(compatible)
    guard compatible.contains(type) else {
      completion(.failure(.badParams("link type \(type) cannot run on a \(kind) endpoint (use \(compatible.joined(separator: ", ")))")))
      return
    }
    let epCfg = spec.applyDefaults(ep.object("config") ?? [:])
    if let e = spec.validate(epCfg) { completion(.failure(e)); return }
    // Relay links: TLS only (RPC_API §11); plain ws:// only with an explicit per-relay allow_plaintext.
    if type == "relay" {
      guard kind == "websocket" else { completion(.failure(.badParams("relay links run on a websocket endpoint"))); return }
      if let e = RelaySocketPolicy.check(url: URL(string: EndpointConfig(epCfg).string("url")), config: epCfg) { completion(.failure(e)); return }
    }
    let endpoint: Endpoint
    do { endpoint = try factory(kind, epCfg) } catch {
      completion(.failure((error as? RPCError) ?? .badParams(error.localizedDescription))); return
    }
    didCreateEndpoint?(endpoint)
    let name = (p.string("name")?.isEmpty == false ? p.string("name") : nil) ?? spec.title
    let linkCfg = link.object("config") ?? [:]
    let desc = EndpointDescriptor(kind: kind, config: epCfg, autostart: p.bool("autostart") ?? true)
    guard let core else { completion(.failure(.busy("node is not running"))); return }
    var add: JSONObject = ["type": type, "name": name, "config": linkCfg, "endpoint": desc.json]
    if let persist = p.bool("persist") { add["persist"] = persist }
    LinkManager.copyAuto(p, into: &add)
    core.call("links.add", add) { [weak self] r in
      guard let self else { return }
      switch r {
      case .failure(let e): completion(.failure(e))
      case .success(let res):
        guard let l = res.object("link"), let id = l.int("id"), id > 0, id <= Int(UInt32.max) else {
          completion(.failure(.internalError("links.add returned no link id"))); return
        }
        let lid = UInt32(id)
        self.descriptors[lid] = desc
        if p.string("auto")?.isEmpty == false { self.autoLinks.insert(lid) }
        self.unbindAndClose(lid, report: false)
        if desc.autostart {
          self.bind(endpoint, id: lid)
          endpoint.open()
        }
        completion(.success(["link": l]))
      }
    }
  }

  /// `auto` / `auto_role` / `auto_key` (RPC_API §11) are passed through; auto links are never persisted.
  static func copyAuto(_ p: JSONObject, into add: inout JSONObject) {
    guard let auto = p.string("auto"), !auto.isEmpty else { return }
    add["auto"] = auto
    if let r = p.string("auto_role"), !r.isEmpty { add["auto_role"] = r }
    if let k = p.string("auto_key"), !k.isEmpty { add["auto_key"] = String(k.prefix(64)) }
    add["persist"] = false
  }

  /// A link around an endpoint that already exists (an accepted or dialed nearby connection):
  /// links.add with the auto fields, then bind and open. On failure the caller closes the endpoint.
  func createAuto(endpoint: Endpoint, descriptor: EndpointDescriptor, params p: JSONObject, completion: @escaping RPCCompletion) {
    guard let core else { completion(.failure(.busy("node is not running"))); return }
    let link = p.object("link") ?? [:]
    var add: JSONObject = ["type": link.string("type") ?? "stream", "name": p.string("name") ?? "Nearby device",
                           "config": link.object("config") ?? [:], "endpoint": descriptor.json, "persist": false]
    LinkManager.copyAuto(p, into: &add)
    core.call("links.add", add) { [weak self] r in
      guard let self else { return }
      switch r {
      case .failure(let e): completion(.failure(e))
      case .success(let res):
        guard let l = res.object("link"), let id = l.int("id"), id > 0, id <= Int(UInt32.max) else {
          completion(.failure(.internalError("links.add returned no link id"))); return
        }
        let lid = UInt32(id)
        self.descriptors[lid] = descriptor
        self.autoLinks.insert(lid)
        self.bind(endpoint, id: lid)
        endpoint.open()
        completion(.success(["link": l]))
      }
    }
  }

  /// link.removed from the core (duplicate nearby link, relay links after consent was withdrawn):
  /// the link is already gone in the core, just close the endpoint.
  func coreRemoved(_ id: UInt32) {
    unbindAndClose(id, report: false)
    descriptors[id] = nil
    backoff[id] = nil
    autoLinks.remove(id)
  }

  /// Links created with `auto` (never persisted, owned by ConnectivityController).
  private(set) var autoLinks: Set<UInt32> = []
  /// Called (main queue) when a bound endpoint reports down.
  var onEndpointDown: ((UInt32, Endpoint) -> Void)?

  // MARK: host.link.remove

  func remove(_ p: JSONObject, completion: @escaping RPCCompletion) {
    guard let raw = p.int("id"), raw > 0, raw <= Int(UInt32.max) else { completion(.failure(.badParams("id is required"))); return }
    let id = UInt32(raw)
    unbindAndClose(id, report: false)
    descriptors[id] = nil
    backoff[id] = nil
    autoLinks.remove(id)
    guard let core else { completion(.failure(.busy("node is not running"))); return }
    core.call("links.remove", ["id": Int(id)]) { r in
      switch r {
      case .success: completion(.success([:]))
      case .failure(let e): completion(.failure(e))
      }
    }
  }

  // MARK: host.link.reconnect

  func reconnect(_ p: JSONObject, completion: @escaping RPCCompletion) {
    guard let raw = p.int("id"), raw > 0, raw <= Int(UInt32.max) else { completion(.failure(.badParams("id is required"))); return }
    let id = UInt32(raw)
    let go = { (desc: EndpointDescriptor) in
      self.backoff[id] = 0
      do { try self.reopen(id, desc) } catch {
        completion(.failure((error as? RPCError) ?? .io(error.localizedDescription))); return
      }
      self.linkInfo(id: id, completion: completion)
    }
    if let d = descriptors[id] { go(d); return }
    // Not seen yet (e.g. created by another host router before a sync): ask the core.
    core?.call("links.list", [:]) { [weak self] r in
      guard let self else { return }
      guard case .success(let res) = r, let l = (res["links"] as? [JSONObject])?.first(where: { $0.int("id") == Int(id) }) else {
        completion(.failure(.notFound("link \(id) not found"))); return
      }
      guard let d = EndpointDescriptor(json: l.object("endpoint")) else {
        completion(.failure(.notFound("link \(id) has no endpoint this device can open"))); return
      }
      let (ok, reason) = self.availability(d.kind)
      guard ok else { completion(.failure(.unsupported(reason ?? "\(d.kind) is not available on this device"))); return }
      self.descriptors[id] = d
      go(d)
    }
  }

  private func reopen(_ id: UInt32, _ d: EndpointDescriptor) throws {
    unbindAndClose(id, report: false)
    let spec = EndpointCatalog.spec(d.kind)
    let cfg = spec?.applyDefaults(d.config) ?? d.config
    let e = try factory(d.kind, cfg)
    didCreateEndpoint?(e)
    bind(e, id: id)
    e.open()
  }

  private func linkInfo(id: UInt32, completion: @escaping RPCCompletion) {
    core?.call("links.list", [:]) { r in
      if case .success(let res) = r, let links = res["links"] as? [JSONObject],
         let l = links.first(where: { $0.int("id") == Int(id) }) {
        completion(.success(["link": l]))
        return
      }
      completion(.failure(.notFound("link \(id) not found")))
    }
  }

  // MARK: startup / node.ready

  /// The normative host startup step, run on EVERY node.ready (after ember_create, unlock, duress
  /// unlock, wipe): links.list → reopen each endpoint from link.endpoint → links.endpoint_state.
  /// Endpoints whose link no longer exists (wipe, locked store) are closed. Endpoints that are already
  /// open with the same descriptor are kept (no needless reconnect).
  func syncWithCore(completion: (() -> Void)? = nil) {
    guard let core else { completion?(); return }
    core.call("links.list", [:]) { [weak self] r in
      guard let self else { return }
      self.syncCount += 1
      guard case .success(let res) = r, let links = res["links"] as? [JSONObject] else {
        if case .failure(let e) = r { emberLog("links.list failed during startup sync: \(e)") }
        completion?()
        return
      }
      var seen = Set<UInt32>()
      for l in links {
        guard let raw = l.int("id"), raw > 0, raw <= Int(UInt32.max) else { continue }
        let id = UInt32(raw)
        seen.insert(id)
        // Auto links (nearby, relay) are never persisted and are owned by ConnectivityController.
        if l.string("auto")?.isEmpty == false { continue }
        guard let d = EndpointDescriptor(json: l.object("endpoint")) else { continue }  // added without endpoint (links.add by hand)
        let previous = self.descriptors[id]
        self.descriptors[id] = d
        if let e = self.endpoint(for: id), let previous, previous.sameEndpoint(d) {
          // Already open: just make sure the core knows its state (a fresh node starts links Down).
          var st: JSONObject = ["id": Int(id), "connected": e.isConnected]
          if let be = e as? BaseEndpoint, let err = be.lastError, !e.isConnected { st["error"] = err }
          core.call("links.endpoint_state", st, completion: nil)
          continue
        }
        guard EndpointCatalog.spec(d.kind) != nil else {
          self.unbindAndClose(id, report: false)
          core.call("links.endpoint_state", ["id": Int(id), "connected": false,
                                             "error": "\(d.kind) endpoints are not available on iOS"], completion: nil)
          continue
        }
        let (ok, reason) = self.availability(d.kind)
        guard ok else {
          self.unbindAndClose(id, report: false)
          core.call("links.endpoint_state", ["id": Int(id), "connected": false, "error": reason ?? "\(d.kind) is not available"],
                    completion: nil)
          continue
        }
        guard d.autostart else { self.unbindAndClose(id, report: true); continue }
        do { try self.reopen(id, d) } catch {
          let msg = (error as? RPCError)?.message ?? error.localizedDescription
          core.call("links.endpoint_state", ["id": Int(id), "connected": false, "error": msg], completion: nil)
        }
      }
      for id in self.boundIds where !seen.contains(id) {
        self.unbindAndClose(id, report: false)
        self.descriptors[id] = nil
      }
      for id in Array(self.descriptors.keys) where !seen.contains(id) { self.descriptors[id] = nil }
      completion?()
    }
  }

  func closeAll() {
    for id in boundIds { unbindAndClose(id, report: false) }
  }

  // MARK: binding

  private func bind(_ e: Endpoint, id: UInt32) {
    e.linkId = id
    e.delegate = self
    lock.lock(); bound[id] = e; lock.unlock()
  }

  private func unbindAndClose(_ id: UInt32, report: Bool) {
    lock.lock(); let e = bound.removeValue(forKey: id); lock.unlock()
    guard let e else { return }
    e.delegate = nil
    e.close()
    if report { core?.call("links.endpoint_state", ["id": Int(id), "connected": false], completion: nil) }
    onEvent?("host.endpoint.state", ["link_id": Int(id), "connected": false, "kind": e.kind])
  }

  // MARK: EndpointDelegate (any queue)

  func endpoint(_ e: Endpoint, received data: Data) {
    core?.linkRx(e.linkId, data)
  }

  func endpoint(_ e: Endpoint, connected: Bool, error: String?) {
    let id = e.linkId
    DispatchQueue.main.async {
      guard self.endpoint(for: id) === e else { return }
      var p: JSONObject = ["id": Int(id), "connected": connected]
      if let error { p["error"] = error }
      self.core?.call("links.endpoint_state", p, completion: nil)
      var ev: JSONObject = ["link_id": Int(id), "connected": connected, "kind": e.kind]
      if let error { ev["error"] = error }
      self.onEvent?("host.endpoint.state", ev)
      if connected {
        self.connectedSince[id] = Date()
        if e.reconnectPolicy.resetAfterSeconds == 0 { self.backoff[id] = 0 }
      } else {
        // Relay policy: the backoff resets only after the link stayed up long enough (2 min).
        if let since = self.connectedSince.removeValue(forKey: id),
           Date().timeIntervalSince(since) >= e.reconnectPolicy.resetAfterSeconds { self.backoff[id] = 0 }
        if e.wantsAutoReconnect { self.scheduleReconnect(id, endpoint: e) }
        self.onEndpointDown?(id, e)
      }
    }
  }
  private var connectedSince: [UInt32: Date] = [:]

  func endpointStatusChanged(_ e: Endpoint) {
    let id = e.linkId
    DispatchQueue.main.async {
      guard self.endpoint(for: id) === e else { return }
      let status = e.status  // read off the endpoint's own queue
      var ev: JSONObject = ["link_id": Int(id), "connected": e.isConnected, "kind": e.kind, "status": status]
      if let be = e as? BaseEndpoint, let err = be.lastError { ev["error"] = err }
      self.onEvent?("host.endpoint.state", ev)
    }
  }

  private func scheduleReconnect(_ id: UInt32, endpoint e: Endpoint) {
    guard !pendingReconnect.contains(id), let d = descriptors[id] else { return }
    let n = backoff[id, default: 0]
    backoff[id] = min(n + 1, 12)
    let delay = e.reconnectPolicy.delay(attempt: n)  // 2, 4, 8 … cap (60 s; relays 300 s ±20 %)
    pendingReconnect.insert(id)
    DispatchQueue.main.asyncAfter(deadline: .now() + delay) { [weak self] in
      guard let self else { return }
      self.pendingReconnect.remove(id)
      guard self.endpoint(for: id) === e, !e.isConnected else { return }
      emberLog("reconnecting link \(id) (\(d.kind)) after \(Int(delay)) s")
      try? self.reopen(id, d)
    }
  }
}
