import UIKit

/// `host.info.accessibility` and the `host.accessibility` event (RPC_API §8).
///
/// Dynamic Type: the page is NOT scaled natively (no pageZoom / text-size adjustment, and the UI
/// does not use the `-apple-system-body` font, which WebKit would scale by itself). The preferred
/// content size is passed as `font_scale` and the UI multiplies its own `--text-scale` by it, so
/// text is scaled exactly once.
final class AccessibilityMonitor {
  struct Snapshot: Equatable {
    var reduceMotion: Bool
    var category: UIContentSizeCategory
    var screenReader: Bool
    var highContrast: Bool
    var boldText: Bool

    static func current() -> Snapshot {
      Snapshot(reduceMotion: UIAccessibility.isReduceMotionEnabled,
               category: UIApplication.shared.preferredContentSizeCategory,
               screenReader: UIAccessibility.isVoiceOverRunning,
               highContrast: UIAccessibility.isDarkerSystemColorsEnabled,
               boldText: UIAccessibility.isBoldTextEnabled)
    }

    var json: JSONObject {
      ["reduce_motion": reduceMotion, "font_scale": AccessibilityMonitor.fontScale(category), "screen_reader": screenReader,
       "high_contrast": highContrast, "bold_text": boldText]
    }
  }

  /// Body text size of each content size category relative to the default (Large = 17 pt):
  /// 14, 15, 16, 17, 19, 21, 23 pt, then the accessibility sizes 28, 33, 40, 47, 53 pt.
  static func fontScale(_ c: UIContentSizeCategory) -> Double {
    let pt: Double
    switch c {
    case .extraSmall: pt = 14
    case .small: pt = 15
    case .medium: pt = 16
    case .large: pt = 17
    case .extraLarge: pt = 19
    case .extraExtraLarge: pt = 21
    case .extraExtraExtraLarge: pt = 23
    case .accessibilityMedium: pt = 28
    case .accessibilityLarge: pt = 33
    case .accessibilityExtraLarge: pt = 40
    case .accessibilityExtraExtraLarge: pt = 47
    case .accessibilityExtraExtraExtraLarge: pt = 53
    default: pt = 17
    }
    return (pt / 17 * 100).rounded() / 100
  }

  static let notificationNames: [Notification.Name] = [
    UIAccessibility.reduceMotionStatusDidChangeNotification,
    UIAccessibility.voiceOverStatusDidChangeNotification,
    UIAccessibility.darkerSystemColorsStatusDidChangeNotification,
    UIAccessibility.boldTextStatusDidChangeNotification,
    UIContentSizeCategory.didChangeNotification,
  ]

  /// Current values (replaceable in tests).
  var provider: () -> Snapshot = { Snapshot.current() }
  /// `host.accessibility` payload, on the main queue, only when something actually changed.
  var onChange: ((JSONObject) -> Void)?
  private var last: Snapshot?
  private var observers: [NSObjectProtocol] = []
  private let center: NotificationCenter

  init(center: NotificationCenter = .default) {
    self.center = center
    for n in AccessibilityMonitor.notificationNames {
      observers.append(center.addObserver(forName: n, object: nil, queue: .main) { [weak self] _ in self?.changed() })
    }
  }

  deinit { observers.forEach { center.removeObserver($0) } }

  var state: JSONObject {
    let s = provider()
    last = s
    return s.json
  }

  private func changed() {
    let s = provider()
    guard s != last else { return }
    last = s
    onChange?(s.json)
  }
}

/// `host.haptic {kind}`. iPhones have a Taptic Engine; iPads (and the Mac running iPad apps) do
/// not, so the method is `unsupported` there.
protocol HapticPerformer: AnyObject {
  func impact()
  func notify(_ type: UINotificationFeedbackGenerator.FeedbackType)
}

final class SystemHaptics: HapticPerformer {
  private let impactGen = UIImpactFeedbackGenerator(style: .light)
  private let notifyGen = UINotificationFeedbackGenerator()
  func impact() { impactGen.impactOccurred() }
  func notify(_ type: UINotificationFeedbackGenerator.FeedbackType) { notifyGen.notificationOccurred(type) }
}

final class HapticService {
  var performer: HapticPerformer = SystemHaptics()
  var supported: Bool = UIDevice.current.userInterfaceIdiom == .phone && !ProcessInfo.processInfo.isiOSAppOnMac

  func perform(_ p: JSONObject) -> RPCResult {
    guard supported else { return .failure(.unsupported("this device has no haptic engine")) }
    switch p.string("kind") ?? "tap" {
    case "tap": performer.impact()
    case "success": performer.notify(.success)
    case "warning": performer.notify(.warning)
    case "error": performer.notify(.error)
    case let k: return .failure(.badParams("kind must be tap, success, warning or error (got \(k))"))
    }
    return .success([:])
  }
}
