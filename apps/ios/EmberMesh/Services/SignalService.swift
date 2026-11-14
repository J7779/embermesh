import AVFoundation
import UIKit

/// host.signal.flash (torch, or a full-screen white flash when there is no torch) and
/// host.signal.tone (audible Morse, generated PCM). Patterns are [on, off, on, …] in milliseconds.
final class SignalService {
  static let maxSteps = 4000
  static let maxTotalMs = 15 * 60 * 1000

  private var overlay: UIView?
  private var player: AVAudioPlayer?

  static var hasTorch: Bool {
    #if targetEnvironment(simulator)
    return false
    #else
    return AVCaptureDevice.default(for: .video)?.hasTorch ?? false
    #endif
  }

  static func parsePattern(_ params: JSONObject) -> Result<[Int], RPCError> {
    guard let raw = params["pattern_ms"] as? [Any] else { return .failure(.badParams("pattern_ms must be an array of milliseconds")) }
    guard raw.count <= maxSteps else { return .failure(.tooLarge("pattern has more than \(maxSteps) steps")) }
    var out: [Int] = []
    var total = 0
    for v in raw {
      guard let n = (v as? NSNumber)?.doubleValue, n.isFinite, n >= 0, n <= 60_000 else {
        return .failure(.badParams("pattern_ms values must be numbers between 0 and 60000"))
      }
      out.append(Int(n))
      total += Int(n)
    }
    guard total <= maxTotalMs else { return .failure(.tooLarge("pattern is longer than 15 minutes")) }
    return .success(out)
  }

  // MARK: flash

  /// Plays on/off patterns against absolute deadlines (start + cumulative ms) on a dedicated
  /// real-time-ish thread with mach_wait_until, so errors never accumulate across a long message and
  /// main-thread load cannot stretch elements. `apply` runs on that thread (torch) and should be fast.
  final class PatternPlayer {
    struct Transition { let on: Bool; let scheduledMs: Double; let actualMs: Double }
    private let lock = NSLock()
    private var generation = 0
    private(set) var transitions: [Transition] = []
    var recordTransitions = false

    private static let timebase: mach_timebase_info_data_t = { var t = mach_timebase_info_data_t(); mach_timebase_info(&t); return t }()
    static func nowNs() -> UInt64 { mach_absolute_time() * UInt64(timebase.numer) / UInt64(timebase.denom) }
    static func ticks(ns: UInt64) -> UInt64 { ns * UInt64(timebase.denom) / UInt64(timebase.numer) }

    /// Cancels any running pattern.
    func cancel() { lock.lock(); generation += 1; lock.unlock() }

    func play(_ pattern: [Int], apply: @escaping (Bool) -> Void, done: (() -> Void)? = nil) {
      lock.lock(); generation += 1; let gen = generation; transitions = []; lock.unlock()
      let t = Thread { [weak self] in
        guard let self else { return }
        let start = PatternPlayer.nowNs() + 5_000_000  // 5 ms lead so the first edge is on time too
        var at: UInt64 = 0  // ns since start
        var steps = pattern.enumerated().map { ($0.offset % 2 == 0, $0.element) }
        steps.append((false, 0))
        for (on, ms) in steps {
          let deadline = start + at
          mach_wait_until(PatternPlayer.ticks(ns: deadline))
          self.lock.lock(); let live = self.generation == gen; self.lock.unlock()
          guard live else { return }
          apply(on)
          if self.recordTransitions {
            let actual = PatternPlayer.nowNs()
            self.lock.lock()
            self.transitions.append(Transition(on: on, scheduledMs: Double(at) / 1e6, actualMs: Double(actual &- start) / 1e6))
            self.lock.unlock()
          }
          at += UInt64(ms) * 1_000_000
        }
        done?()
      }
      t.qualityOfService = .userInteractive
      t.name = "org.embermesh.signal"
      t.start()
    }

    var recorded: [Transition] { lock.lock(); defer { lock.unlock() }; return transitions }
  }

  let flashPlayer = PatternPlayer()

  func flash(params: JSONObject, hostView: UIView?, completion: RPCCompletion) {
    let pattern: [Int]
    switch SignalService.parsePattern(params) {
    case .failure(let e): completion(.failure(e)); return
    case .success(let p): pattern = p
    }
    flashPlayer.cancel()
    let torch = SignalService.hasTorch ? AVCaptureDevice.default(for: .video) : nil
    releaseTorch()
    setScreen(false, hostView: hostView)
    guard !pattern.isEmpty else { completion(.success(["mode": "off"])); return }  // [] stops
    let total = pattern.reduce(0, +)
    if let dev = torch, (try? dev.lockForConfiguration()) != nil {
      // Hold the configuration lock for the whole pattern: each edge is then one fast call.
      lockedTorch = dev
      flashPlayer.play(pattern, apply: { on in
        if on { try? dev.setTorchModeOn(level: AVCaptureDevice.maxAvailableTorchLevel) } else { dev.torchMode = .off }
      }, done: { [weak self] in DispatchQueue.main.async { self?.releaseTorch() } })
      completion(.success(["mode": "torch", "duration_ms": total]))
    } else {
      // Screen flash: edges are shown at the next display refresh (≤ 16.7 ms quantisation at 60 Hz).
      flashPlayer.play(pattern, apply: { [weak self] on in
        DispatchQueue.main.async { self?.setScreen(on, hostView: hostView) }
      }, done: { [weak self] in
        DispatchQueue.main.async { self?.restoreBrightness(hostView?.window) }
      })
      completion(.success(["mode": "screen", "duration_ms": total]))
    }
  }

  private var lockedTorch: AVCaptureDevice?
  private func releaseTorch() {
    guard let dev = lockedTorch else { return }
    lockedTorch = nil
    dev.torchMode = .off
    dev.unlockForConfiguration()
  }

  /// Screen flash fallback: a white overlay on the whole window at full brightness.
  func setScreen(_ on: Bool, hostView: UIView?) {
    guard let window = hostView?.window else { return }
    if overlay == nil {
      let v = UIView(frame: window.bounds)
      v.backgroundColor = .white
      v.isUserInteractionEnabled = false
      v.autoresizingMask = [.flexibleWidth, .flexibleHeight]
      v.isHidden = true
      v.accessibilityIdentifier = "ember.flash.overlay"
      window.addSubview(v)
      overlay = v
    }
    if on, savedBrightness == nil { savedBrightness = window.screen.brightness; window.screen.brightness = 1 }
    overlay?.isHidden = !on
    if on { window.bringSubviewToFront(overlay!) }
  }
  private var savedBrightness: CGFloat?
  var screenOverlayVisible: Bool { overlay.map { !$0.isHidden } ?? false }

  func restoreBrightness(_ window: UIWindow?) {
    if let b = savedBrightness { window?.screen.brightness = b; savedBrightness = nil }
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
    player?.stop()
    player = nil
    guard !pattern.isEmpty else { completion(.success([:])); return }
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
      completion(.failure(.io("could not play tone: \(error.localizedDescription)")))
    }
  }
}
