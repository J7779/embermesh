import AVFoundation
import UIKit

// host.signal.flash / host.signal.stop / host.signal.tone (docs/RPC_API.md §10 "Morse sending by
// light"). Patterns are [on, off, on, …] in ms, built by the shared UI.
//
// Rules this file implements:
//   * the light always goes off — pattern end, stop, error, resign-active / background / screen
//     lock, web view torn down;
//   * least privilege — the torch is switched with AVCaptureDevice.lockForConfiguration + torchMode
//     only: no camera authorisation is requested and no capture session is ever started;
//   * nothing about the pattern (or the text it encodes) is logged or stored;
//   * drift-free timing — each edge is scheduled at start + Σ(previous durations) on a monotonic
//     clock, on a dedicated thread; a late edge shortens the next interval.

// MARK: - clock

/// Monotonic clock in nanoseconds with an interruptible wait (real: DispatchTime uptime; tests: virtual).
protocol KeyerClock: AnyObject {
  func nowNs() -> UInt64
  /// Waits until `deadlineNs` or until `cancel` is signalled. Returns false when cancelled.
  func wait(until deadlineNs: UInt64, cancel: DispatchSemaphore) -> Bool
}

/// DispatchTime uptime (mach absolute time). Waits are interruptible until 3 ms before the deadline
/// (semaphore: stop is immediate), then finish with mach_wait_until, which is not subject to timer
/// coalescing (≈1 ms instead of ≈4 ms lateness).
final class MonotonicClock: KeyerClock {
  private static let timebase: mach_timebase_info_data_t = { var t = mach_timebase_info_data_t(); mach_timebase_info(&t); return t }()
  func nowNs() -> UInt64 { DispatchTime.now().uptimeNanoseconds }
  func wait(until deadlineNs: UInt64, cancel: DispatchSemaphore) -> Bool {
    let coarse = deadlineNs > 3_000_000 ? deadlineNs - 3_000_000 : 0
    if coarse > nowNs(), cancel.wait(timeout: DispatchTime(uptimeNanoseconds: coarse)) == .success { return false }
    if cancel.wait(timeout: .now()) == .success { return false }
    let tb = MonotonicClock.timebase
    mach_wait_until(deadlineNs * UInt64(tb.denom) / UInt64(tb.numer))
    return true
  }
}

// MARK: - lights

/// Something that can be switched on and off for the length of one pattern.
protocol SignalLight: AnyObject {
  /// Prepares the light (e.g. takes the torch configuration lock). Throws RPCError.
  func begin() throws
  /// Called on the keyer thread at each edge; must be fast.
  func set(_ on: Bool)
  /// Light off and everything restored. Called exactly once after begin() succeeded.
  func end()
}

/// The back camera's torch. Only `lockForConfiguration` + `torchMode`: switching the torch does not
/// require camera authorisation on iOS (it is not "using the camera"), so no permission is
/// requested and no AVCaptureSession is created.
final class SystemTorch: SignalLight {
  static func device() -> AVCaptureDevice? {
    #if targetEnvironment(simulator)
    return nil
    #else
    let dev = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: .back) ?? AVCaptureDevice.default(for: .video)
    return dev?.hasTorch == true ? dev : nil
    #endif
  }
  static var available: Bool { device() != nil }

  private var dev: AVCaptureDevice?

  func begin() throws {
    guard let d = SystemTorch.device() else { throw RPCError.unsupported("this device has no flashlight") }
    guard d.isTorchAvailable else { throw RPCError.busy("the flashlight is not available right now (too hot, or in use)") }
    do { try d.lockForConfiguration() } catch {
      throw RPCError("permission_denied", "the flashlight could not be used: \(error.localizedDescription)")
    }
    dev = d
    d.torchMode = .off
  }

  func set(_ on: Bool) {
    guard let d = dev else { return }
    if on { try? d.setTorchModeOn(level: AVCaptureDevice.maxAvailableTorchLevel) } else { d.torchMode = .off }
  }

  func end() {
    guard let d = dev else { return }
    d.torchMode = .off
    d.unlockForConfiguration()
    dev = nil
  }
}

/// Full-screen flash: a white (on) / black (off) cover over the whole window at full brightness, the
/// idle timer held off. Brightness and idle-timer state are restored exactly at the end. Tapping the
/// cover stops the flash (the UI's own Stop button is underneath it).
final class ScreenLight: SignalLight {
  private weak var window: UIWindow?
  private var cover: UIView?
  private var savedBrightness: CGFloat?
  private var savedIdle: Bool?
  var onTap: (() -> Void)?
  private(set) var isOn = false

  init(window: UIWindow?) { self.window = window }

  func begin() throws {
    let work = {
      guard let w = self.window else { return }
      self.savedBrightness = w.screen.brightness
      self.savedIdle = UIApplication.shared.isIdleTimerDisabled
      UIApplication.shared.isIdleTimerDisabled = true
      w.screen.brightness = 1
      let v = UIView(frame: w.bounds)
      v.backgroundColor = .black
      v.autoresizingMask = [.flexibleWidth, .flexibleHeight]
      v.accessibilityIdentifier = "ember.flash.cover"
      v.isAccessibilityElement = true
      v.accessibilityLabel = "Flashing Morse code. Tap to stop."
      v.accessibilityTraits = .button
      v.addGestureRecognizer(UITapGestureRecognizer(target: self, action: #selector(self.tapped)))
      w.addSubview(v)
      self.cover = v
    }
    if Thread.isMainThread { work() } else { DispatchQueue.main.sync(execute: work) }
    guard window != nil else { throw RPCError.unsupported("no screen to flash") }
  }

  @objc private func tapped() { onTap?() }

  func set(_ on: Bool) {
    DispatchQueue.main.async {
      self.isOn = on
      self.cover?.backgroundColor = on ? .white : .black
    }
  }

  func end() {
    let work = {
      self.isOn = false
      self.cover?.removeFromSuperview()
      self.cover = nil
      if let b = self.savedBrightness { self.window?.screen.brightness = b }
      if let i = self.savedIdle { UIApplication.shared.isIdleTimerDisabled = i }
      self.savedBrightness = nil
      self.savedIdle = nil
    }
    if Thread.isMainThread { work() } else { DispatchQueue.main.sync(execute: work) }
  }

  var isCovering: Bool { cover?.superview != nil }
}

// MARK: - keyer

/// Plays one pattern on a dedicated thread against a monotonic clock. Edge k is due at
/// start + Σ pattern[0..<k]; lateness never accumulates.
final class Keyer {
  struct Result { let stopped: Bool; let worstLateMs: Double; let meanLateMs: Double; let edges: Int }
  private let clock: KeyerClock
  private let cancel = DispatchSemaphore(value: 0)
  private let lock = NSLock()
  private var stopped = false
  private(set) var edgeTimesNs: [UInt64] = []   // actual edge times relative to start (tests)
  private(set) var scheduledNs: [UInt64] = []

  init(clock: KeyerClock) { self.clock = clock }

  func stop() {
    lock.lock(); let first = !stopped; stopped = true; lock.unlock()
    if first { cancel.signal() }
  }
  var isStopped: Bool { lock.lock(); defer { lock.unlock() }; return stopped }

  /// Runs synchronously on the calling thread (SignalService puts it on its own thread).
  func run(_ pattern: [Int], light: SignalLight) -> Result {
    let start = clock.nowNs() + 2_000_000  // 2 ms lead so the first edge is on time too
    var at: UInt64 = 0
    var late: [Double] = []
    var cancelled = false
    for (i, ms) in pattern.enumerated() {
      if isStopped || !clock.wait(until: start + at, cancel: cancel) { cancelled = true; break }
      light.set(i % 2 == 0)
      let now = clock.nowNs()
      scheduledNs.append(at)
      edgeTimesNs.append(now &- start)
      late.append(Double(Int64(bitPattern: now &- (start + at))) / 1e6)
      at += UInt64(ms) * 1_000_000
    }
    if !cancelled, isStopped || !clock.wait(until: start + at, cancel: cancel) { cancelled = true }
    light.set(false)
    let positive = late.map { max(0, $0) }
    return Result(stopped: cancelled, worstLateMs: positive.max() ?? 0,
                  meanLateMs: positive.isEmpty ? 0 : positive.reduce(0, +) / Double(positive.count), edges: late.count)
  }
}

// MARK: - service

final class SignalService {
  static let maxSteps = 4096
  static let minStepMs = 10
  static let maxStepMs = 60_000
  static let maxTotalMs = 10 * 60 * 1000

  private var player: AVAudioPlayer?
  private var running: (keyer: Keyer, used: String)?
  private var observers: [NSObjectProtocol] = []

  // Injectable for tests.
  var clock: KeyerClock = MonotonicClock()
  var torchAvailable: () -> Bool = { SystemTorch.available }
  var makeTorch: () -> SignalLight = { SystemTorch() }
  var makeScreen: (UIWindow?) -> SignalLight = { ScreenLight(window: $0) }
  /// True while this app itself is using the camera (QR scanner, Morse camera reader): the torch
  /// belongs to that capture session then.
  var cameraBusy: () -> Bool = { false }

  init() {
    let nc = NotificationCenter.default
    for n in [UIApplication.willResignActiveNotification, UIApplication.didEnterBackgroundNotification,
              UIApplication.protectedDataWillBecomeUnavailableNotification] {
      observers.append(nc.addObserver(forName: n, object: nil, queue: .main) { [weak self] _ in self?.stopAll() })
    }
  }
  deinit { observers.forEach { NotificationCenter.default.removeObserver($0) }; running?.keyer.stop() }

  /// For host.info.signal (never "ask" on iOS: the torch needs no permission).
  var info: JSONObject { ["torch": torchAvailable() ? "yes" : "no", "screen": true] }

  /// Validates pattern_ms against the contract limits (bad_params otherwise).
  static func parsePattern(_ params: JSONObject) -> Result<[Int], RPCError> {
    guard let raw = params["pattern_ms"] as? [Any] else { return .failure(.badParams("pattern_ms must be an array of milliseconds")) }
    guard !raw.isEmpty else { return .failure(.badParams("pattern_ms is empty (use host.signal.stop to stop)")) }
    guard raw.count <= maxSteps else { return .failure(.badParams("pattern_ms has more than \(maxSteps) entries")) }
    var out: [Int] = []
    var total = 0
    for v in raw {
      guard let num = v as? NSNumber, CFGetTypeID(num) != CFBooleanGetTypeID() else {
        return .failure(.badParams("pattern_ms entries must be numbers"))
      }
      let n = num.doubleValue
      guard n.isFinite, n >= Double(minStepMs), n <= Double(maxStepMs) else {
        return .failure(.badParams("pattern_ms entries must be between \(minStepMs) and \(maxStepMs) ms"))
      }
      out.append(Int(n.rounded()))
      total += Int(n.rounded())
    }
    guard total <= maxTotalMs else { return .failure(.badParams("pattern is longer than 10 minutes")) }
    return .success(out)
  }

  // MARK: flash

  /// Resolves when the pattern has finished or was stopped. `completion` on the main queue.
  func flash(params: JSONObject, window: UIWindow?, completion: @escaping RPCCompletion) {
    let pattern: [Int]
    switch SignalService.parsePattern(params) {
    case .failure(let e): completion(.failure(e)); return
    case .success(let p): pattern = p
    }
    let mode = params.string("mode") ?? "auto"
    guard ["auto", "torch", "screen"].contains(mode) else { completion(.failure(.badParams("mode must be auto, torch or screen"))); return }
    stopAll()  // a second flash (or a running tone) stops first

    var light: SignalLight?
    var used = "screen"
    if mode != "screen" {
      if !torchAvailable() {
        if mode == "torch" { completion(.failure(.unsupported("this device has no flashlight"))); return }
      } else if cameraBusy() {
        if mode == "torch" { completion(.failure(.busy("the camera (and its flashlight) is in use by the QR scanner or the Morse reader"))); return }
      } else {
        let t = makeTorch()
        do { try t.begin(); light = t; used = "torch" } catch {
          if mode == "torch" { completion(.failure((error as? RPCError) ?? .io("the flashlight could not be used"))); return }
        }
      }
    }
    if light == nil {
      let s = makeScreen(window)
      do { try s.begin() } catch { completion(.failure((error as? RPCError) ?? .unsupported("no screen to flash"))); return }
      light = s
      used = "screen"
    }
    let keyer = Keyer(clock: clock)
    (light as? ScreenLight)?.onTap = { [weak keyer] in keyer?.stop() }
    running = (keyer, used)
    let lit = light!
    let t = Thread {
      let r = keyer.run(pattern, light: lit)
      lit.end()
      DispatchQueue.main.async {
        if self.running?.keyer === keyer { self.running = nil }
        completion(.success(["used": used, "stopped": r.stopped,
                             "timing": ["worst_late_ms": (r.worstLateMs * 100).rounded() / 100,
                                        "mean_late_ms": (r.meanLateMs * 100).rounded() / 100, "edges": r.edges]]))
      }
    }
    t.qualityOfService = .userInteractive
    t.name = "org.embermesh.signal"
    t.start()
  }

  var isFlashing: Bool { running != nil }
  var flashingWith: String? { running?.used }

  /// host.signal.stop and every "light must go off" situation: stops a flash (its completion then
  /// reports stopped:true) and a tone. No-op when nothing runs.
  func stopAll() {
    running?.keyer.stop()
    player?.stop()
    player = nil
  }

  // MARK: tone

  /// 16-bit mono WAV of the pattern with 5 ms raised-cosine edges (no key clicks).
  static func wav(pattern: [Int], freq: Double, rate: Int = 22_050) -> Data {
    let total = pattern.reduce(0, +) * rate / 1000
    var samples = [Int16](repeating: 0, count: max(total, 1))
    var pos = 0
    let ramp = max(1, rate * 5 / 1000)
    for (i, ms) in pattern.enumerated() {
      let n = ms * rate / 1000
      if i % 2 == 0 {
        for k in 0..<n where pos + k < samples.count {
          var env = 1.0
          if k < ramp { env = 0.5 - 0.5 * cos(Double.pi * Double(k) / Double(ramp)) }
          if n - k < ramp { env = min(env, 0.5 - 0.5 * cos(Double.pi * Double(n - k) / Double(ramp))) }
          samples[pos + k] = Int16(sin(2 * Double.pi * freq * Double(pos + k) / Double(rate)) * env * 0.8 * 32767)
        }
      }
      pos += n
    }
    var d = Data()
    func u32(_ v: UInt32) { var x = v.littleEndian; withUnsafeBytes(of: &x) { d.append(contentsOf: $0) } }
    func u16(_ v: UInt16) { var x = v.littleEndian; withUnsafeBytes(of: &x) { d.append(contentsOf: $0) } }
    let dataBytes = UInt32(samples.count * 2)
    d.append(contentsOf: Array("RIFF".utf8)); u32(36 + dataBytes); d.append(contentsOf: Array("WAVE".utf8))
    d.append(contentsOf: Array("fmt ".utf8)); u32(16); u16(1); u16(1); u32(UInt32(rate)); u32(UInt32(rate * 2)); u16(2); u16(16)
    d.append(contentsOf: Array("data".utf8)); u32(dataBytes)
    for s in samples { u16(UInt16(bitPattern: s)) }
    return d
  }

  func tone(params: JSONObject, audioModemActive: Bool, completion: RPCCompletion) {
    let pattern: [Int]
    switch SignalService.parsePattern(params) {
    case .failure(let e): completion(.failure(e)); return
    case .success(let p): pattern = p
    }
    stopAll()  // a running flash or tone stops first
    let freq = max(100, min(4000, params.number("freq_hz") ?? 700))
    if !audioModemActive {
      // Playback category sounds even with the silent switch on: this is a signalling tool.
      try? AVAudioSession.sharedInstance().setCategory(.playback, mode: .default, options: [.mixWithOthers])
      try? AVAudioSession.sharedInstance().setActive(true)
    }
    do {
      let p = try AVAudioPlayer(data: SignalService.wav(pattern: pattern, freq: freq))
      p.volume = 1
      p.prepareToPlay()
      p.play()
      player = p
      completion(.success(["duration_ms": pattern.reduce(0, +)]))
    } catch {
      completion(.failure(.io("could not play tone")))
    }
  }
}
