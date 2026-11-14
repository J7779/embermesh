import UIKit
import UserNotifications

/// Local notifications for incoming messages and SOS alerts (core events), plus host.notify.
/// Permission is requested the first time something needs to be shown, not at launch.
final class NotificationService: NSObject, UNUserNotificationCenterDelegate {
  private let center = UNUserNotificationCenter.current()
  private var requested = false
  /// Set while the app is in the foreground (the UI itself shows new messages then).
  var appActive = false

  override init() {
    super.init()
    center.delegate = self
  }

  private func ensureAuthorized(_ then: @escaping (Bool) -> Void) {
    center.getNotificationSettings { s in
      switch s.authorizationStatus {
      case .authorized, .provisional, .ephemeral: then(true)
      case .denied: then(false)
      default:
        self.center.requestAuthorization(options: [.alert, .sound, .badge]) { ok, _ in then(ok) }
      }
    }
  }

  func post(title: String, body: String, kind: String, threadId: String? = nil, completion: RPCCompletion? = nil) {
    ensureAuthorized { ok in
      guard ok else { completion?(.failure(.denied("Notifications are off. Turn them on in Settings > EmberMesh > Notifications."))); return }
      let c = UNMutableNotificationContent()
      c.title = String(title.prefix(120))
      c.body = String(body.prefix(400))
      c.sound = .default
      c.userInfo = ["kind": kind]
      if let threadId { c.threadIdentifier = threadId }
      if kind == "sos", #available(iOS 15.0, *) { c.interruptionLevel = .timeSensitive }
      let req = UNNotificationRequest(identifier: UUID().uuidString, content: c, trigger: nil)
      self.center.add(req) { err in
        if let err { completion?(.failure(.io(err.localizedDescription))) } else { completion?(.success([:])) }
      }
    }
  }

  /// Inspects a core event and notifies for incoming messages (when backgrounded) and SOS (always).
  func handleCoreEvent(name: String, data: JSONObject) {
    switch name {
    case "message.new":
      guard let m = data.object("message"), m.bool("outgoing") != true else { return }
      let kind = m.string("kind") ?? "text"
      if kind == "sos" {
        post(title: "SOS from \(m.string("from_name") ?? "someone")", body: m.string("text") ?? "", kind: "sos")
        return
      }
      guard !appActive, kind != "system" else { return }
      post(title: m.string("from_name") ?? "New message", body: m.string("text") ?? "", kind: "message", threadId: m.string("conv"))
    case "sos.received":
      post(title: "SOS from \(data.string("name") ?? "someone")", body: data.string("text") ?? "Emergency beacon received", kind: "sos")
    default:
      break
    }
  }

  // MARK: delegate

  func userNotificationCenter(_ c: UNUserNotificationCenter, willPresent n: UNNotification,
                              withCompletionHandler done: @escaping (UNNotificationPresentationOptions) -> Void) {
    let kind = n.request.content.userInfo["kind"] as? String
    if kind == "sos" || kind == "host" { done([.banner, .list, .sound]) } else { done([.list]) }
  }

  func userNotificationCenter(_ c: UNUserNotificationCenter, didReceive r: UNNotificationResponse,
                              withCompletionHandler done: @escaping () -> Void) {
    done()
  }
}
