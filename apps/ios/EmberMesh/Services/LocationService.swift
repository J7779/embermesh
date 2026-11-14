import CoreLocation
import Foundation

/// host.location.get / host.location.watch via CoreLocation (GPS works with no network).
/// Every fix is pushed to the core with position.set and to the UI as a host.location event.
final class LocationService: NSObject, CLLocationManagerDelegate {
  private let manager = CLLocationManager()
  private var waiters: [RPCCompletion] = []
  private var authWaiters: [(Bool) -> Void] = []
  private var watching = false
  private var lastPushed = Date.distantPast
  var onFix: ((JSONObject) -> Void)?

  override init() {
    super.init()
    manager.delegate = self
    manager.desiredAccuracy = kCLLocationAccuracyBest
    manager.distanceFilter = 10
    manager.pausesLocationUpdatesAutomatically = true
  }

  static var available: Bool { CLLocationManager.locationServicesEnabled() }

  static func position(_ l: CLLocation) -> JSONObject {
    var p: JSONObject = ["lat": l.coordinate.latitude, "lon": l.coordinate.longitude,
                         "accuracy": max(0, l.horizontalAccuracy), "ts": Int64(l.timestamp.timeIntervalSince1970 * 1000)]
    if l.verticalAccuracy >= 0 { p["alt"] = l.altitude }
    return p
  }

  private func authorized(_ done: @escaping (Bool) -> Void) {
    switch manager.authorizationStatus {
    case .authorizedAlways, .authorizedWhenInUse: done(true)
    case .denied, .restricted: done(false)
    default:
      authWaiters.append(done)
      manager.requestWhenInUseAuthorization()
    }
  }

  func get(_ completion: @escaping RPCCompletion) {
    authorized { ok in
      guard ok else { completion(.failure(.denied("Location permission is off. Turn it on in Settings > EmberMesh > Location."))); return }
      if let l = self.manager.location, -l.timestamp.timeIntervalSinceNow < 10, l.horizontalAccuracy >= 0, l.horizontalAccuracy < 100 {
        let p = LocationService.position(l)
        self.onFix?(p)
        completion(.success(p))
        return
      }
      self.waiters.append(completion)
      if self.waiters.count == 1 {
        self.manager.requestLocation()
        // A cold GPS fix outdoors can take a minute without network assistance (no A-GPS).
        DispatchQueue.main.asyncAfter(deadline: .now() + 90) { self.timeoutWaiters() }
      }
    }
  }

  private func timeoutWaiters() {
    guard !waiters.isEmpty else { return }
    let w = waiters; waiters.removeAll()
    if let l = manager.location {
      let p = LocationService.position(l)
      w.forEach { $0(.success(p)) }
    } else {
      w.forEach { $0(.failure(.io("No GPS fix yet. Go outside with a clear view of the sky and try again."))) }
    }
  }

  func watch(_ enabled: Bool, completion: @escaping RPCCompletion) {
    if !enabled {
      watching = false
      manager.stopUpdatingLocation()
      completion(.success([:]))
      return
    }
    authorized { ok in
      guard ok else { completion(.failure(.denied("Location permission is off. Turn it on in Settings > EmberMesh > Location."))); return }
      self.watching = true
      self.manager.startUpdatingLocation()
      completion(.success([:]))
    }
  }

  // MARK: delegate

  func locationManagerDidChangeAuthorization(_ m: CLLocationManager) {
    let st = m.authorizationStatus
    guard st != .notDetermined else { return }
    let ok = st == .authorizedAlways || st == .authorizedWhenInUse
    let w = authWaiters; authWaiters.removeAll()
    w.forEach { $0(ok) }
  }

  func locationManager(_ m: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
    guard let l = locations.last, l.horizontalAccuracy >= 0 else { return }
    let p = LocationService.position(l)
    if !waiters.isEmpty {
      let w = waiters; waiters.removeAll()
      w.forEach { $0(.success(p)) }
      onFix?(p)
      lastPushed = Date()
    } else if watching, Date().timeIntervalSince(lastPushed) >= 5 {
      lastPushed = Date()
      onFix?(p)
    }
  }

  func locationManager(_ m: CLLocationManager, didFailWithError error: Error) {
    if (error as? CLError)?.code == .locationUnknown { return }  // keeps trying
    let w = waiters; waiters.removeAll()
    let denied = (error as? CLError)?.code == .denied
    w.forEach { $0(.failure(denied ? .denied("Location permission is off.") : .io(error.localizedDescription))) }
  }
}
