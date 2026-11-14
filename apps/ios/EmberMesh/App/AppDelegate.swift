import UIKit

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
  func application(_ application: UIApplication,
                   didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
    // Background task identifiers must be registered before launch completes.
    LaunchClock.mark("didFinishLaunching")
    AppEnvironment.registerBackgroundTasks()
    // The node starts here, not in the scene: iOS may relaunch us in the background (Bluetooth
    // state restoration) with no UI at all, and the restored managers must find their delegates.
    _ = AppEnvironment.start()
    LaunchClock.mark("node running")
    return true
  }

  func application(_ application: UIApplication, configurationForConnecting session: UISceneSession,
                   options: UIScene.ConnectionOptions) -> UISceneConfiguration {
    let c = UISceneConfiguration(name: "Default", sessionRole: session.role)
    c.delegateClass = SceneDelegate.self
    return c
  }
}

final class SceneDelegate: UIResponder, UIWindowSceneDelegate {
  var window: UIWindow?

  func scene(_ scene: UIScene, willConnectTo session: UISceneSession, options: UIScene.ConnectionOptions) {
    guard let ws = scene as? UIWindowScene else { return }
    let env = AppEnvironment.start()
    let w = UIWindow(windowScene: ws)
    w.overrideUserInterfaceStyle = .dark
    w.backgroundColor = WebViewController.background
    let diag = ProcessInfo.processInfo.arguments.contains("-EmberDiagnostics")
      || UserDefaults.standard.bool(forKey: "EmberDiagnostics")
    let vc = WebViewController(webHost: env.makeWebHost(), forceDiagnostics: diag)
    env.host.presenter = vc
    w.rootViewController = vc
    w.makeKeyAndVisible()
    window = w
    if let url = options.urlContexts.first?.url {
      DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) { env.handleOpenURL(url) }
    }
  }

  func scene(_ scene: UIScene, openURLContexts contexts: Set<UIOpenURLContext>) {
    for c in contexts { AppEnvironment.shared?.handleOpenURL(c.url) }
  }

  func sceneDidBecomeActive(_ scene: UIScene) { AppEnvironment.shared?.didBecomeActive() }
  func sceneWillResignActive(_ scene: UIScene) { AppEnvironment.shared?.notifications.appActive = false }
  func sceneDidEnterBackground(_ scene: UIScene) { AppEnvironment.shared?.didEnterBackground() }
}
