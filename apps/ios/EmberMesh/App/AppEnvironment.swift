import BackgroundTasks
import UIKit

/// Process-wide wiring: node + runner + host router + bridge. Created at launch (also when iOS
/// relaunches the app in the background for Bluetooth state restoration, before any UI exists).
final class AppEnvironment {
  static private(set) var shared: AppEnvironment?
  static let refreshTaskId = "org.embermesh.app.refresh"

  let storageDir: URL
  let runner = NodeRunner()
  let node: EmberNode?
  let links: LinkManager
  let host: HostRouter
  let notifications = NotificationService()
  let content = WebContent.bundled()
  let guidesPack: MappedFile?
  private var _webHosts: [WebHost] = []
  private let hostsLock = NSLock()
  var webHosts: [WebHost] { hostsLock.lock(); defer { hostsLock.unlock() }; return _webHosts }
  private var bgTask: UIBackgroundTaskIdentifier = .invalid
  let isTesting = ProcessInfo.processInfo.environment["XCTestConfigurationFilePath"] != nil

  static func start() -> AppEnvironment {
    if let s = shared { return s }
    let env = AppEnvironment()
    shared = env
    return env
  }

  /// Application Support/EmberMesh/node — protected until first unlock (so Bluetooth wake-ups work
  /// while the phone is locked) and excluded from iCloud/iTunes backups (identity keys stay on the device).
  static func makeStorageDir() -> URL {
    let fm = FileManager.default
    let base = (try? fm.url(for: .applicationSupportDirectory, in: .userDomainMask, appropriateFor: nil, create: true))
      ?? fm.temporaryDirectory
    var dir = base.appendingPathComponent("EmberMesh", isDirectory: true)
    try? fm.createDirectory(at: dir.appendingPathComponent("node"), withIntermediateDirectories: true,
                            attributes: [.protectionKey: FileProtectionType.completeUntilFirstUserAuthentication])
    try? fm.setAttributes([.protectionKey: FileProtectionType.completeUntilFirstUserAuthentication], ofItemAtPath: dir.path)
    var rv = URLResourceValues()
    rv.isExcludedFromBackup = true
    try? dir.setResourceValues(rv)
    return dir
  }

  private init() {
    storageDir = AppEnvironment.makeStorageDir()
    // device_name is the generic model ("iPhone" / "iPad"), never UIDevice.name ("<Owner>'s iPhone").
    let config: JSONObject = [
      "storage_dir": storageDir.appendingPathComponent("node").path, "platform": "ios", "app_version": HostRouter.appVersion,
      "device_name": UIDevice.current.model, "now_ms": Int64(nowUnixMs()),
    ]
    node = EmberNode(config: config)
    guidesPack = content.guidesPack.flatMap { MappedFile(url: $0) }
    LaunchClock.mark("ember_create done")
    if let node, let pack = guidesPack {
      let rc = node.loadGuides(mapped: pack)
      emberLog("guides.pack \(pack.count) bytes memory-mapped, ember_guides_load(copy=0) rc=\(rc)")
      LaunchClock.mark("guides loaded")
    }
    if let node, let info = JSON.parseObject(node.call("node.info", nil))?.object("result") {
      emberLog("node \(info.string("node_id") ?? "?") name '\(info.string("name") ?? "")' locked=\(info.bool("locked") ?? false) storage=\(info.string("storage") ?? "?")")
    }
    // Links used to be saved by the shell in links.json; the core is now the only source of truth.
    try? FileManager.default.removeItem(at: storageDir.appendingPathComponent("links.json"))
    let runner = self.runner
    links = LinkManager(core: runner,
                        factory: { kind, cfg in try HostRouter.makeEndpoint(kind: kind, config: cfg, runner: runner) },
                        availability: HostRouter.availability)
    host = HostRouter(links: links, runner: runner, notifications: notifications)
    emberLog("core \(EmberNode.version), storage \(storageDir.path), ui bundled: \(content.hasUI), hub web app: \(content.hubRoot?.path ?? "none")")

    runner.onEvent = { [weak self] json in self?.coreEvent(json) }
    runner.onLinkTx = { [weak self] id, data in self?.links.handleLinkTx(id, data) }
    links.onEvent = { [weak self] name, data in self?.broadcast(name: name, data: data) }
    host.onEvent = { [weak self] name, data in self?.broadcast(name: name, data: data) }
    // The first node.ready (emitted by ember_create) triggers the startup link sync in coreEvent.
    if let node { runner.start(core: node) } else { emberLog("ember_create failed") }
    host.loadLinkTypes()
  }

  /// Startup step on every node.ready (RPC_API "Core behaviour notes"): links.list → reopen
  /// endpoints → links.endpoint_state. Skipped inside the XCTest host app so unit tests never
  /// reopen the simulator user's saved links (tests drive LinkManager.syncWithCore directly).
  var autoSyncLinks: Bool { !isTesting }

  private func nodeReady() {
    DispatchQueue.main.async { [self] in
      guard self.autoSyncLinks else { return }
      self.links.syncWithCore { [weak self] in
        guard let self else { return }
        emberLog("node.ready: links synced (\(self.links.boundIds.count) endpoints open)")
        for h in self.links.endpoints(of: HubEndpoint.self) { h.refreshProbe() }
      }
    }
  }

  func makeWebHost() -> WebHost {
    let h = WebHost(content: content, pack: guidesPack, core: node != nil ? runner : nil, host: host)
    hostsLock.lock()
    _webHosts.removeAll { $0.webView == nil }
    _webHosts.append(h)
    hostsLock.unlock()
    return h
  }

  /// Core outbox event (node queue).
  private func coreEvent(_ json: String) {
    for h in webHosts { h.router.sendEvent(json: json) }
    if json.hasPrefix("{\"event\":\"node.ready\"") || (json.contains("\"node.ready\"") && JSON.parseObject(json)?.string("event") == "node.ready") {
      nodeReady()
    }
    // Only events that may need a notification are parsed.
    if json.contains("\"message.new\"") || json.contains("\"sos.received\"") {
      if let o = JSON.parseObject(json), let name = o.string("event") {
        notifications.handleCoreEvent(name: name, data: o.object("data") ?? [:])
      }
    }
  }

  func broadcast(name: String, data: JSONObject) {
    let run = { for h in self.webHosts { h.router.sendEvent(name: name, data: data) } }
    if Thread.isMainThread { run() } else { DispatchQueue.main.async(execute: run) }
  }

  /// ember: URIs (contact cards, channel invites) opened from another app (Camera, Messages, AirDrop).
  /// The UI has no handler for such links, so they go straight to the core: a contact card is
  /// added with contacts.add, otherwise it is tried as a channel invite; the result is posted as a
  /// local notification. (The earlier host.open_url event was dropped: nothing consumed it.)
  func handleOpenURL(_ url: URL) {
    let text = url.absoluteString
    guard text.lowercased().hasPrefix("ember:") else { return }
    runner.call("contacts.add", ["card": text]) { [weak self] r in
      guard let self else { return }
      if case .success(let res) = r {
        let name = res.object("contact")?.string("name") ?? "contact"
        self.notifications.post(title: "Contact added", body: name.isEmpty ? "New contact" : name, kind: "host", completion: { _ in })
        return
      }
      self.runner.call("channels.join", ["invite": text]) { r2 in
        switch r2 {
        case .success(let res):
          self.notifications.post(title: "Joined channel", body: res.object("channel")?.string("name") ?? "", kind: "host", completion: { _ in })
        case .failure(let e):
          emberLog("ember: link not understood (\(e))")
          self.notifications.post(title: "EmberMesh link not recognised", body: e.message, kind: "host", completion: { _ in })
        }
      }
    }
  }

  // MARK: lifecycle / background

  func didBecomeActive() {
    notifications.appActive = true
    runner.kick()
  }

  func didEnterBackground() {
    notifications.appActive = false
    // Finish in-flight work (flush outbox, let BLE writes drain) before suspension.
    let app = UIApplication.shared
    if bgTask == .invalid {
      bgTask = app.beginBackgroundTask(withName: "ember.flush") { [weak self] in self?.endBackgroundTask() }
    }
    runner.kick()
    DispatchQueue.main.asyncAfter(deadline: .now() + 20) { [weak self] in self?.endBackgroundTask() }
    scheduleRefresh()
  }

  private func endBackgroundTask() {
    guard bgTask != .invalid else { return }
    UIApplication.shared.endBackgroundTask(bgTask)
    bgTask = .invalid
  }

  static func registerBackgroundTasks() {
    BGTaskScheduler.shared.register(forTaskWithIdentifier: refreshTaskId, using: nil) { task in
      AppEnvironment.shared?.handleRefresh(task as! BGAppRefreshTask)
    }
  }

  func scheduleRefresh() {
    let req = BGAppRefreshTaskRequest(identifier: AppEnvironment.refreshTaskId)
    req.earliestBeginDate = Date(timeIntervalSinceNow: 15 * 60)
    do { try BGTaskScheduler.shared.submit(req) } catch { emberLog("BG refresh not scheduled: \(error.localizedDescription)") }
  }

  /// Periodic background slot: run the node for a few seconds so store-and-forward, beacons and
  /// retries go out over whatever links are still connected (BLE keeps running in the background).
  private func handleRefresh(_ task: BGAppRefreshTask) {
    scheduleRefresh()
    var finished = false
    task.expirationHandler = {
      DispatchQueue.main.async { if !finished { finished = true; task.setTaskCompleted(success: false) } }
    }
    runner.kick()
    DispatchQueue.main.asyncAfter(deadline: .now() + 8) {
      if !finished { finished = true; task.setTaskCompleted(success: true) }
    }
  }
}
