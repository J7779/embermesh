import CEmber
import Foundation

/// `ws_server` — the hub on the phone (ARCHITECTURE §5.2, §6). Embeds the host library's Hub
/// (host/src/host/hub.cpp through apps/ios/native/ember_ios_hub.cpp): HTTP on port 8437 serving the
/// bundled web app (dist/web: the WASM build + guides, so guests run their own node in the
/// browser), the `/hub.json` probe and the `/ws` mesh bus (binary WebSocket, one mesh frame per
/// message). All guests share this ONE core link of type `raw`; the core relays between them.
///
/// Works over Personal Hotspot (the phone is the hotspot's gateway, usually 172.20.10.1) and on
/// any Wi-Fi the phone has joined. iOS suspends the app (and the hub) shortly after it leaves the
/// screen, so the hub needs the app in the foreground (keep awake on a charger).
final class HubEndpoint: BaseEndpoint {
  struct Options {
    var port: Int
    var webRoot: URL?
    var downloads: [URL] = []
    var version: String = HostRouter.appVersion
  }

  private let queue = DispatchQueue(label: "org.embermesh.hub")
  /// Created once in init, destroyed in deinit (start/stop in between).
  private var hub: OpaquePointer!
  private let opts: Options
  private(set) var clients = 0
  /// Name / node id shown in /hub.json (filled in by HostRouter from node.info).
  var probe: () -> JSONObject = { [:] }
  /// host.hub.clients events.
  var onClients: ((Int) -> Void)?

  init(config: JSONObject, webRoot: URL?, downloads: [URL] = []) throws {
    let port = EndpointConfig(config).int("port", 8437)
    guard (1...65535).contains(port) else { throw RPCError.badParams("port must be 1–65535") }
    opts = Options(port: port, webRoot: webRoot, downloads: downloads)
    super.init(kind: "ws_server")
    let user = Unmanaged.passUnretained(self).toOpaque()
    hub = ember_ios_hub_create({ user, data, len in
      guard let user, let data, len > 0 else { return }
      let me = Unmanaged<HubEndpoint>.fromOpaque(user).takeUnretainedValue()
      me.deliver(Data(bytes: data, count: len))  // one guest message == one mesh frame
    }, { user, guests in
      guard let user else { return }
      Unmanaged<HubEndpoint>.fromOpaque(user).takeUnretainedValue().clientsChanged(Int(guests))
    }, user)
  }

  deinit { if let hub { ember_ios_hub_destroy(hub) } }

  override func open() {
    queue.async {
      var o: JSONObject = ["port": self.opts.port, "bind": "0.0.0.0", "version": self.opts.version, "max_guests": 200]
      if let w = self.opts.webRoot { o["web_root"] = w.path }
      let files = self.opts.downloads.filter { FileManager.default.fileExists(atPath: $0.path) }.map { $0.path }
      if !files.isEmpty { o["download_files"] = files }
      for (k, v) in self.probe() { o[k] = v }
      var err = [CChar](repeating: 0, count: 256)
      let rc = ember_ios_hub_start(self.hub, JSON.encode(o), &err, err.count)
      if rc == 0 {
        emberLog("hub listening on port \(self.opts.port): \(self.readStatus()["urls"] ?? [])")
        self.setConnected(true)
        self.statusChanged()
      } else {
        let msg = String(cString: err)
        let friendly = msg.contains("in use") || msg.lowercased().contains("addr")
          ? "port \(self.opts.port) is in use (is another hub running?): \(msg)" : msg
        emberLog("hub failed: \(friendly)")
        self.setConnected(false, error: friendly)
      }
    }
  }

  override func close() {
    queue.sync {
      ember_ios_hub_stop(hub)
      clients = 0
    }
    setConnected(false)
  }

  /// Core → every guest (thread-safe in the wrapper).
  override func write(_ data: Data) {
    data.withUnsafeBytes { raw in
      ember_ios_hub_broadcast(hub, raw.bindMemory(to: UInt8.self).baseAddress, raw.count)
    }
  }

  private func clientsChanged(_ n: Int) {
    queue.async {
      self.clients = n
      self.onClients?(n)
      self.statusChanged()
    }
  }

  func refreshProbe() {
    queue.async {
      var p = self.probe()
      p["platform"] = "ios"
      ember_ios_hub_set_probe(self.hub, JSON.encode(p))
    }
  }

  /// {running, port, urls, clients, stats}
  var hubStatus: JSONObject { queue.sync { readStatus() } }

  private func readStatus() -> JSONObject {
    guard let p = ember_ios_hub_status(hub) else { return ["running": false, "urls": [String](), "clients": 0] }
    defer { ember_ios_hub_free(p) }
    return JSON.parseObject(String(cString: p)) ?? ["running": false, "urls": [String](), "clients": 0]
  }

  var urls: [String] { (hubStatus["urls"] as? [String]) ?? [] }

  override var status: JSONObject {
    let s = hubStatus
    return ["port": s.int("port") ?? opts.port, "urls": s["urls"] ?? [String](), "clients": s.int("clients") ?? 0,
            "web_root": opts.webRoot?.lastPathComponent ?? "built-in page"]
  }
}
