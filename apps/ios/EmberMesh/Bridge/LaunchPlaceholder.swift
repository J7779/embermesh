import UIKit

/// Brand colours shared by the launch screen, the placeholder and the web view's own background.
enum Brand {
  /// #17110f — the logo's background, the launch screen and every native surface behind the page.
  static let background = UIColor(red: 23 / 255, green: 17 / 255, blue: 15 / 255, alpha: 1)
  /// Logo side as a fraction of the screen's shorter side (LaunchScreen.storyboard uses the same).
  static let logoFraction: CGFloat = 0.3
}

/// The still logo shown over the web view from launch until the page has painted its first frame.
/// It is laid out exactly like LaunchScreen.storyboard (centred in the whole window, side = 30 % of
/// the shorter side), so the system launch image, this view and the web boot animation's first frame
/// coincide. Removal is instantaneous (no fade): the web animation appears to start from the still.
/// Static: no animation, display link or timer of its own. Hidden from VoiceOver.
final class LaunchPlaceholderView: UIView {
  let logo = UIImageView(image: UIImage(named: "LaunchLogo"))

  override init(frame: CGRect) {
    super.init(frame: frame)
    backgroundColor = Brand.background
    isOpaque = true
    isUserInteractionEnabled = false
    isAccessibilityElement = false
    accessibilityElementsHidden = true
    accessibilityIdentifier = "ember.launch.placeholder"
    logo.contentMode = .scaleAspectFit
    logo.translatesAutoresizingMaskIntoConstraints = false
    addSubview(logo)
    let w = logo.widthAnchor
    let c = [
      logo.centerXAnchor.constraint(equalTo: centerXAnchor),
      logo.centerYAnchor.constraint(equalTo: centerYAnchor),
      logo.heightAnchor.constraint(equalTo: w),
      w.constraint(lessThanOrEqualTo: widthAnchor, multiplier: Brand.logoFraction),
      w.constraint(lessThanOrEqualTo: heightAnchor, multiplier: Brand.logoFraction),
      { let x = w.constraint(equalTo: widthAnchor, multiplier: Brand.logoFraction); x.priority = UILayoutPriority(750); return x }(),
      { let x = w.constraint(equalTo: heightAnchor, multiplier: Brand.logoFraction); x.priority = UILayoutPriority(749); return x }(),
    ]
    NSLayoutConstraint.activate(c)
  }
  required init?(coder: NSCoder) { fatalError("not used") }

  /// The logo's frame for a container of `size` (same rule as the constraints).
  static func logoRect(in size: CGSize) -> CGRect {
    let side = (min(size.width, size.height) * Brand.logoFraction).rounded(.toNearestOrEven)
    return CGRect(x: (size.width - side) / 2, y: (size.height - side) / 2, width: side, height: side)
  }
}
