import AVFoundation
import WebKit
import XCTest
@testable import EmberMesh

/// Morse helpers for synthetic signals.
enum MorseGen {
  static let table: [Character: String] = [
    "A": ".-", "B": "-...", "C": "-.-.", "D": "-..", "E": ".", "F": "..-.", "G": "--.", "H": "....", "I": "..", "J": ".---",
    "K": "-.-", "L": ".-..", "M": "--", "N": "-.", "O": "---", "P": ".--.", "Q": "--.-", "R": ".-.", "S": "...", "T": "-",
    "U": "..-", "V": "...-", "W": ".--", "X": "-..-", "Y": "-.--", "Z": "--..",
    "0": "-----", "1": ".----", "2": "..---", "3": "...--", "4": "....-", "5": ".....", "6": "-....", "7": "--...",
    "8": "---..", "9": "----.", "/": "-..-.",
  ]
  /// [on, off, on, …] in ms (PARIS standard: dot = 1200 / wpm).
  static func pattern(_ text: String, wpm: Double) -> [Double] {
    let dot = 1200 / wpm
    var out: [Double] = []
    for (wi, word) in text.uppercased().split(separator: " ").enumerated() {
      if wi > 0, !out.isEmpty { out[out.count - 1] = 7 * dot }
      for (ci, ch) in word.enumerated() {
        if ci > 0, !out.isEmpty { out[out.count - 1] = 3 * dot }
        for el in table[ch] ?? "" { out.append(el == "." ? dot : 3 * dot); out.append(dot) }
      }
    }
    return out
  }
}

/// Records morse.rx.feed calls (and optionally forwards them to a real node).
final class RecordingSink: MorseFeedSink {
  var feeds: [JSONObject] = []
  let lock = NSLock()
  var forward: NodeRunner?
  func morseFeed(_ params: JSONObject, completion: @escaping (RPCResult) -> Void) {
    lock.lock(); feeds.append(params); lock.unlock()
    if let f = forward { f.morseFeed(params, completion: completion) } else { completion(.success([:])) }
  }
}

enum SyntheticFrames {
  /// A 420f (bi-planar full range) 640×480 frame whose ROI has luma `roiLevel` and the rest `bg`.
  static func sampleBuffer(roiLevel: UInt8, bg: UInt8 = 40, timeMs: Double, roi: CaptureROI = CaptureROI()) -> CMSampleBuffer {
    var pb: CVPixelBuffer?
    let attrs = [kCVPixelBufferIOSurfacePropertiesKey as String: [String: Any]()] as CFDictionary
    CVPixelBufferCreate(nil, 640, 480, kCVPixelFormatType_420YpCbCr8BiPlanarFullRange, attrs, &pb)
    let p = pb!
    CVPixelBufferLockBaseAddress(p, [])
    let base = CVPixelBufferGetBaseAddressOfPlane(p, 0)!.assumingMemoryBound(to: UInt8.self)
    let bpr = CVPixelBufferGetBytesPerRowOfPlane(p, 0)
    for y in 0..<480 {
      let inY = Double(y) / 480 >= roi.y && Double(y) / 480 < roi.y + roi.h
      for x in 0..<640 {
        let inX = Double(x) / 640 >= roi.x && Double(x) / 640 < roi.x + roi.w
        base[y * bpr + x] = inX && inY ? roiLevel : bg
      }
    }
    CVPixelBufferUnlockBaseAddress(p, [])
    var fd: CMVideoFormatDescription?
    CMVideoFormatDescriptionCreateForImageBuffer(allocator: nil, imageBuffer: p, formatDescriptionOut: &fd)
    var timing = CMSampleTimingInfo(duration: CMTime(value: 1, timescale: 30),
                                    presentationTimeStamp: CMTime(seconds: timeMs / 1000, preferredTimescale: 1_000_000),
                                    decodeTimeStamp: .invalid)
    var sb: CMSampleBuffer?
    CMSampleBufferCreateReadyWithImageBuffer(allocator: nil, imageBuffer: p, formatDescription: fd!, sampleTiming: &timing, sampleBufferOut: &sb)
    return sb!
  }
}

final class CaptureTests: XCTestCase {
  func testLumaMeanOverROI() {
    let roi = CaptureROI()
    let sb = SyntheticFrames.sampleBuffer(roiLevel: 230, bg: 10, timeMs: 0, roi: roi)
    let feeder = LightFeeder(session: "s", sink: RecordingSink())
    let cam = CameraLumaCapture(feeder: feeder, roi: roi)
    let sink = RecordingSink()
    let f2 = LightFeeder(session: "s", sink: sink, flushMs: 0)
    let c2 = CameraLumaCapture(feeder: f2, roi: roi)
    c2.process(sb)
    let lv = (sink.feeds.first?["levels"] as? [Double])?.first ?? -1
    XCTAssertEqual(lv, 230.0 / 255, accuracy: 0.01)
    _ = cam
    XCTAssertNil(CaptureROI(json: ["x": 0.2, "y": 0.2, "w": 0, "h": 0.1]))
    XCTAssertEqual(CaptureROI(json: ["x": 0.9, "y": 0.9, "w": 0.5, "h": 0.5])?.w ?? 0, 0.1, accuracy: 1e-9)
  }

  /// Synthetic camera frames (30 fps, flashlight Morse with noise and exposure drift) through the
  /// exact frame path the AVCaptureVideoDataOutput delegate uses; feeds are batched ~100 ms with
  /// per-frame timestamps.
  func testCameraFramePathBatchesFeeds() {
    let sink = RecordingSink()
    let cam = CameraLumaCapture(feeder: LightFeeder(session: "light1", sink: sink), roi: CaptureROI())
    let frames = CaptureTests.lightFrames(text: "SOS", wpm: 10, fps: 30)
    for (t, lv) in frames { cam.process(SyntheticFrames.sampleBuffer(roiLevel: lv, timeMs: t)) }
    cam.feeder.flush()
    XCTAssertEqual(cam.frames, frames.count)
    let times = sink.feeds.flatMap { ($0["times_ms"] as? [Double]) ?? [] }
    let levels = sink.feeds.flatMap { ($0["levels"] as? [Double]) ?? [] }
    XCTAssertEqual(times.count, frames.count)
    XCTAssertEqual(zip(times, times.dropFirst()).allSatisfy { $0 < $1 }, true)
    XCTAssertTrue(sink.feeds.allSatisfy { $0.string("session") == "light1" })
    XCTAssertGreaterThan(sink.feeds.count, frames.count / 8)          // batched
    XCTAssertLessThanOrEqual(sink.feeds.count, frames.count / 2)
    XCTAssertGreaterThan(levels.max() ?? 0, 0.7); XCTAssertLessThan(levels.min() ?? 1, 0.3)
  }

  /// 30 fps frames of a lamp keying `text`, with ±4 luma noise, slow brightness drift and frame jitter.
  static func lightFrames(text: String, wpm: Double, fps: Double, lead: Double = 800) -> [(Double, UInt8)] {
    let pat = MorseGen.pattern(text, wpm: wpm)
    var edges: [(Double, Bool)] = []
    var t = lead
    for (i, d) in pat.enumerated() { edges.append((t, i % 2 == 0)); t += d }
    let end = t + 1500
    var rng = SystemRandomNumberGenerator()
    var out: [(Double, UInt8)] = []
    var ft = 0.0
    while ft < end {
      let on = edges.last(where: { $0.0 <= ft })?.1 ?? false
      let drift = 15 * sin(ft / 3000)
      let base = on ? 200.0 : 60.0
      let v = base + drift + Double(Int.random(in: -4...4, using: &rng))
      out.append((ft, UInt8(max(0, min(255, v)))))
      ft += 1000 / fps + Double.random(in: -2...2, using: &rng)
    }
    return out
  }

  /// Keyed 700 Hz tone at 48 kHz with 5 ms ramps (and light noise).
  static func tone(text: String, wpm: Double, rate: Double = 48_000, hz: Double = 700) -> [Float] {
    let pat = MorseGen.pattern(text, wpm: wpm)
    var out = [Float](repeating: 0, count: Int(0.5 * rate))
    var phase = 0.0
    for (i, ms) in pat.enumerated() {
      let n = Int(ms / 1000 * rate)
      let ramp = Int(0.005 * rate)
      for k in 0..<n {
        var env = 0.0
        if i % 2 == 0 { env = min(1, Double(k) / Double(ramp), Double(n - k) / Double(ramp)) }
        phase += 2 * .pi * hz / rate
        out.append(Float(0.5 * env * sin(phase)) + Float.random(in: -0.01...0.01))
      }
    }
    out += [Float](repeating: 0, count: Int(1.5 * rate))
    return out
  }

  func testPcmFeederResamplesToSessionRate() {
    let sink = RecordingSink()
    let f = PcmFeeder(session: "a1", sampleRate: 8000, sink: sink)
    let src = [Float](repeating: 0.25, count: 48_000)
    src.withUnsafeBufferPointer { f.add($0, inRate: 48_000) }
    f.flush()
    let bytes = sink.feeds.compactMap { $0.string("pcm_b64").flatMap { Data(base64Encoded: $0) } }.reduce(0) { $0 + $1.count }
    XCTAssertEqual(bytes / 2, 8000, accuracy: 1)
    let first = sink.feeds.first.flatMap { $0.string("pcm_b64").flatMap { Data(base64Encoded: $0) } }!
    XCTAssertEqual(Int16(littleEndian: first.withUnsafeBytes { $0.load(as: Int16.self) }), Int16(0.25 * 32767))
    XCTAssertEqual(sink.feeds.count, 10)  // ~100 ms batches
    // 44.1 kHz → 8 kHz keeps the exact long-run rate.
    let s2 = RecordingSink(); let f2 = PcmFeeder(session: "a2", sampleRate: 8000, sink: s2)
    let src2 = [Float](repeating: 0, count: 44_100 * 3)
    src2.withUnsafeBufferPointer { f2.add($0, inRate: 44_100) }
    f2.flush()
    XCTAssertEqual(f2.fedSamples, 24_000, accuracy: 1)
  }

  func testMicPathThroughAudioBuffers() {
    let sink = RecordingSink()
    let mic = MicPcmCapture(feeder: PcmFeeder(session: "m", sampleRate: 8000, sink: sink))
    let fmt = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: 48_000, channels: 1, interleaved: false)!
    let samples = CaptureTests.tone(text: "E", wpm: 20)
    var i = 0
    while i < samples.count {
      let n = min(960, samples.count - i)
      let b = AVAudioPCMBuffer(pcmFormat: fmt, frameCapacity: AVAudioFrameCount(n))!
      b.frameLength = AVAudioFrameCount(n)
      for k in 0..<n { b.floatChannelData![0][k] = samples[i + k] }
      mic.process(b, rate: 48_000)
      i += n
    }
    mic.stop()
    XCTAssertEqual(mic.feeder.fedSamples, samples.count / 6, accuracy: 2)
  }

  /// While a sound link runs, Morse mic capture shares its input (one engine, one input) instead of
  /// starting a second AVAudioEngine.
  func testMicCaptureSharesRunningSoundLink() {
    let n = TestNode(name: "share")
    n.start()
    let ep = AudioEndpoint(config: [:], runner: n.runner)
    ep.startWithoutEngine(inputRate: 48_000, outputRate: 48_000)
    let sink = RecordingSink()
    let mic = MicPcmCapture(feeder: PcmFeeder(session: "s", sampleRate: 8000, sink: sink))
    var err: RPCError?? = .none
    mic.start(sharing: ep) { err = .some($0) }
    XCTAssertTrue(spin(until: 2) { err != nil })
    XCTAssertNil(err!, "shared start needs no permission prompt or engine")
    let fmt = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: 48_000, channels: 1, interleaved: false)!
    let b = AVAudioPCMBuffer(pcmFormat: fmt, frameCapacity: 4800)!
    b.frameLength = 4800
    for _ in 0..<5 { ep.captured(b, sampleRate: 48_000) }   // the sound link's tap path
    mic.stop()
    XCTAssertEqual(mic.feeder.fedSamples, 4000, accuracy: 1, "5 × 100 ms at 48 kHz → 4000 samples at 8 kHz")
    ep.captured(b, sampleRate: 48_000)  // after stop: no longer listening
    XCTAssertEqual(mic.feeder.fedSamples, 4000, accuracy: 1)
    ep.close(); n.stop()
  }

  // MARK: real core (morse.rx.*)

  func coreNode() throws -> TestNode {
    let n = TestNode(name: "morse")
    n.start()
    if case .failure(let e) = n.call("morse.rx.start", ["source": "light"]), e.code == "not_implemented" {
      n.stop()
      throw XCTSkip("the core does not implement morse.rx yet")
    }
    return n
  }

  /// Synthetic flashlight frames → CameraLumaCapture.process → LightFeeder → morse.rx.feed (real core).
  func testCameraLumaDecodesWithRealCore() throws {
    let n = try coreNode()
    for (text, wpm) in [("SOS SOS", 8.0), ("PARIS", 15.0)] {
      let sess = try XCTUnwrap(n.ok("morse.rx.start", ["source": "light"])["session"])
      let sid = (sess as? String) ?? "\(sess)"
      let sink = RecordingSink(); sink.forward = n.runner
      let cam = CameraLumaCapture(feeder: LightFeeder(session: sid, sink: sink), roi: CaptureROI())
      for (t, lv) in CaptureTests.lightFrames(text: text, wpm: wpm, fps: 30) {
        cam.process(SyntheticFrames.sampleBuffer(roiLevel: lv, timeMs: 10_000 + t))
      }
      cam.feeder.flush()
      spin(until: 1) { false }
      let st = n.ok("morse.rx.status", ["session": sess])
      let tr = (st.string("transcript") ?? "").replacingOccurrences(of: " ", with: "")
      print("camera luma \(text) @\(wpm) wpm → \"\(st.string("transcript") ?? "")\" wpm=\(st.number("wpm") ?? 0) quality=\(st.number("quality") ?? 0)")
      XCTAssertTrue(tr.contains(text.replacingOccurrences(of: " ", with: "")), "transcript: \(st.string("transcript") ?? "")")
      _ = n.ok("morse.rx.stop", ["session": sess])
    }
    n.stop()
  }

  /// Synthetic 48 kHz tone → MicPcmCapture.process → PcmFeeder (8 kHz) → morse.rx.feed (real core).
  func testMicPcmDecodesWithRealCore() throws {
    let n = try coreNode()
    let sess = try XCTUnwrap(n.ok("morse.rx.start", ["source": "audio", "sample_rate": 8000])["session"])
    let sid = (sess as? String) ?? "\(sess)"
    let sink = RecordingSink(); sink.forward = n.runner
    let mic = MicPcmCapture(feeder: PcmFeeder(session: sid, sampleRate: 8000, sink: sink))
    let fmt = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: 48_000, channels: 1, interleaved: false)!
    let samples = CaptureTests.tone(text: "CQ CQ DE QX0ABC", wpm: 18)
    var i = 0
    while i < samples.count {
      let c = min(960, samples.count - i)
      let b = AVAudioPCMBuffer(pcmFormat: fmt, frameCapacity: AVAudioFrameCount(c))!
      b.frameLength = AVAudioFrameCount(c)
      for k in 0..<c { b.floatChannelData![0][k] = samples[i + k] }
      mic.process(b, rate: 48_000)
      i += c
    }
    mic.stop()
    spin(until: 1) { false }
    let st = n.ok("morse.rx.status", ["session": sess])
    print("mic pcm → \"\(st.string("transcript") ?? "")\" wpm=\(st.number("wpm") ?? 0) tone=\(st.number("tone_hz") ?? 0)")
    XCTAssertTrue((st.string("transcript") ?? "").replacingOccurrences(of: " ", with: "").contains("CQCQDEQX0ABC"))
    n.stop()
  }

  /// host.capture.* through HostRouter: errors are clean in the simulator (no camera), and mic_pcm
  /// starts / stops (it needs microphone permission, which the simulator may not have: then denied).
  func testHostCaptureMethods() throws {
    let n = TestNode(name: "cap")
    n.start()
    let info = try XCTUnwrap(n.host("host.info")).get()
    let feats = info["features"] as? [String] ?? []
    XCTAssertTrue(feats.contains("capture_mic_pcm"))
    XCTAssertTrue(feats.contains("getusermedia"))
    #if targetEnvironment(simulator)
    XCTAssertFalse(feats.contains("capture_camera_luma"), "no camera in the simulator")
    guard case .failure(let e) = n.host("host.capture.start", ["kind": "camera_luma", "session": "1"]) else { return XCTFail("expected unsupported") }
    XCTAssertEqual(e.code, "unsupported")
    #endif
    XCTAssertEqual((info["audio"] as? JSONObject)?.bool("serial_ptt"), false)
    if case .success = n.host("host.capture.start", ["kind": "mic_pcm"]) { XCTFail("session is required") }
    if case .success = n.host("host.capture.start", ["kind": "nope", "session": "1"]) { XCTFail("bad kind") }
    XCTAssertNotNil(try? n.host("host.capture.stop", ["kind": "mic_pcm"])?.get())
    n.stop()
  }
}

/// getUserMedia inside our WKWebView on the embermesh:// scheme.
final class WebMediaTests: XCTestCase {
  final class Delegate: NSObject, WKUIDelegate, WKNavigationDelegate {
    var asked: [String] = []
    var loaded = false
    func webView(_ webView: WKWebView, requestMediaCapturePermissionFor origin: WKSecurityOrigin, initiatedByFrame frame: WKFrameInfo,
                 type: WKMediaCaptureType, decisionHandler: @escaping (WKPermissionDecision) -> Void) {
      asked.append("\(origin.protocol)://\(origin.host) type=\(type.rawValue)")
      decisionHandler(MediaCapturePolicy.decision(scheme: origin.protocol, host: origin.host))
    }
    func webView(_ w: WKWebView, didFinish n: WKNavigation!) { loaded = true }
  }

  func testPolicy() {
    XCTAssertEqual(MediaCapturePolicy.decision(scheme: "embermesh", host: "app"), .grant)
    XCTAssertEqual(MediaCapturePolicy.decision(scheme: "https", host: "example.com"), .deny)
    XCTAssertEqual(MediaCapturePolicy.decision(scheme: "embermesh", host: "evil"), .deny)
  }

  func testGetUserMediaAvailableOnCustomScheme() throws {
    let env = try XCTUnwrap(AppEnvironment.shared)
    let host = env.makeWebHost()
    let wv = WKWebView(frame: CGRect(x: 0, y: 0, width: 390, height: 600), configuration: host.makeConfiguration())
    host.attach(wv)
    let d = Delegate()
    wv.uiDelegate = d; wv.navigationDelegate = d
    UIApplication.shared.connectedScenes.compactMap { ($0 as? UIWindowScene)?.windows.first }.first?.addSubview(wv)
    defer { wv.removeFromSuperview() }
    wv.load(URLRequest(url: host.schemeHandler.startURL(forceDiagnostics: true)))
    XCTAssertTrue(spin(until: 15) { d.loaded })
    func run(_ js: String) -> Any? {
      var out: Any?; var done = false
      wv.callAsyncJavaScript(js, arguments: [:], in: nil, in: .page) { r in
        if case .success(let v) = r { out = v } else if case .failure(let e) = r { out = "JS error: \(e)" }
        done = true
      }
      spin(until: 20) { done }
      return out
    }
    let api = run("return {secure: window.isSecureContext, md: !!navigator.mediaDevices, gum: !!(navigator.mediaDevices && navigator.mediaDevices.getUserMedia)};") as? [String: Any]
    print("getUserMedia API: \(api ?? [:])")
    XCTAssertEqual(api?["secure"] as? Bool, true)
    XCTAssertEqual(api?["md"] as? Bool, true, "navigator.mediaDevices exists on embermesh://")
    XCTAssertEqual(api?["gum"] as? Bool, true)
    // Actual capture: the simulator has no camera; the microphone is the Mac's (TCC permitting).
    let attempt = """
      const tryOne = async (c) => { try { const s = await Promise.race([navigator.mediaDevices.getUserMedia(c), new Promise((_, r) => setTimeout(() => r(new Error('timeout')), 8000))]);
        const t = s.getTracks().map(t => t.kind + ':' + t.readyState); s.getTracks().forEach(t => t.stop()); return 'ok ' + t.join(','); }
        catch (e) { return 'error ' + (e.name || '') + ' ' + (e.message || ''); } };
      return {video: await tryOne({video: {facingMode: 'environment'}}), audio: await tryOne({audio: true})};
      """
    let r = run(attempt) as? [String: Any]
    print("getUserMedia in simulator: \(r ?? [:]); permission requests: \(d.asked)")
    XCTAssertNotNil(r)
  }
}

final class FlashTimingTests: XCTestCase {
  /// Real monotonic clock + dedicated thread. On a shared or virtualised host a wake-up can be tens or
  /// hundreds of ms late, so lateness itself is not a property of the code; what must hold anywhere:
  /// edges in order, never early, the pattern never shorter than requested, and lateness measured
  /// against each edge's own deadline (start + Σ previous durations), so it cannot accumulate. The
  /// "no drift" proof is the deterministic VirtualClock test below; the measured lateness is logged
  /// and attached. The single numeric bound (2 s for the last edge) is far above any scheduling delay
  /// and only catches accumulation: naive "sleep(duration) after each edge" scheduling adds every
  /// wake-up delay to all later edges.
  func testRealClockEdgeTimingInvariants() {
    for wpm in [20.0, 5.0] {
      let pat = MorseGen.pattern(wpm >= 20 ? "PARIS PARIS" : "SOS", wpm: wpm).map { Int($0.rounded()) }
      let light = FakeLight()
      let k = Keyer(clock: MonotonicClock())
      let done = expectation(description: "pattern \(wpm)")
      var r: Keyer.Result?
      let t = Thread { r = k.run(pat, light: light); done.fulfill() }
      t.start()
      wait(for: [done], timeout: Double(pat.reduce(0, +)) / 1000 + 60)
      guard let r else { return XCTFail("keyer did not finish") }
      let sched = k.scheduledNs, actual = k.edgeTimesNs
      XCTAssertFalse(r.stopped)
      XCTAssertEqual(r.edges, pat.count)
      XCTAssertEqual(sched.count, pat.count)
      XCTAssertEqual(light.states, (0..<pat.count).map { $0 % 2 == 0 } + [false], "edges in order, light off at the end")
      for i in 0..<actual.count { XCTAssertGreaterThanOrEqual(actual[i], sched[i], "edge \(i) never early") }
      XCTAssertTrue(zip(actual, actual.dropFirst()).allSatisfy { $0 <= $1 }, "edge times increase")
      XCTAssertEqual(k.endDueNs, UInt64(pat.reduce(0, +)) * 1_000_000)
      XCTAssertGreaterThanOrEqual(k.endNs, k.endDueNs, "not shorter than the pattern")
      let lastLateMs = Double(actual.last! &- sched.last!) / 1e6
      XCTAssertLessThan(lastLateMs, 2000, "no accumulated drift (generous: see above)")
      let line = String(format: "flash timing %.0f WPM (dot %.0f ms, %d edges): max %.3f ms late, mean %.3f ms, last edge %.3f ms, end %.3f ms after due",
                        wpm, 1200 / wpm, r.edges, r.worstLateMs, r.meanLateMs, lastLateMs, Double(k.endNs - k.endDueNs) / 1e6)
      print(line)
      let a = XCTAttachment(string: line); a.lifetime = .keepAlways; add(a)
    }
  }
}

/// Records what the keyer switched (thread-safe).
final class FakeLight: SignalLight {
  private let lock = NSLock()
  private var _states: [Bool] = []
  private var _began = 0, _ended = 0
  var failBegin: RPCError?
  var states: [Bool] { lock.lock(); defer { lock.unlock() }; return _states }
  var began: Int { lock.lock(); defer { lock.unlock() }; return _began }
  var ended: Int { lock.lock(); defer { lock.unlock() }; return _ended }
  var isOn: Bool { states.last ?? false }
  func begin() throws { if let e = failBegin { throw e }; lock.lock(); _began += 1; lock.unlock() }
  func set(_ on: Bool) { lock.lock(); _states.append(on); lock.unlock() }
  func end() { lock.lock(); _states.append(false); _ended += 1; lock.unlock() }
}

/// Virtual monotonic clock: waits return instantly with time advanced to the deadline plus an
/// injected lateness for chosen edges; `stopAfter` cancels at a given wait.
final class VirtualClock: KeyerClock {
  var t: UInt64 = 1_000_000_000
  var lateness: [Int: UInt64] = [:]   // wait index -> extra ns
  var waits = 0
  var onWait: ((Int) -> Void)?
  func nowNs() -> UInt64 { t }
  func wait(until d: UInt64, cancel: DispatchSemaphore) -> Bool {
    let i = waits; waits += 1
    onWait?(i)
    if cancel.wait(timeout: .now()) == .success { return false }
    t = max(t, d) + (lateness[i] ?? 0)
    return true
  }
}

final class SignalContractTests: XCTestCase {
  /// A late edge shortens the next interval: later edges stay on their schedule (no drift).
  func testDriftFreeScheduling() {
    let clock = VirtualClock()
    clock.lateness = [3: 7_000_000]   // edge 3 woke 7 ms late
    let k = Keyer(clock: clock)
    let light = FakeLight()
    let pat = [60, 60, 180, 60, 60, 60, 180, 420]
    let r = k.run(pat, light: light)
    XCTAssertFalse(r.stopped)
    let actualMs = k.edgeTimesNs.map { Double($0) / 1e6 }
    let schedMs = k.scheduledNs.map { Double($0) / 1e6 }
    XCTAssertEqual(actualMs[3] - schedMs[3], 7, accuracy: 0.001, "edge 3 late")
    for i in 4..<pat.count { XCTAssertEqual(actualMs[i], schedMs[i], accuracy: 0.001, "edge \(i) back on schedule") }
    XCTAssertEqual(r.worstLateMs, 7, accuracy: 0.001)
    XCTAssertEqual(light.states, [true, false, true, false, true, false, true, false, false])
    XCTAssertEqual(k.endNs, k.endDueNs, "ends exactly when due")
  }

  /// One very late wake-up (a descheduled thread, 500 ms — longer than the next three intervals):
  /// the edges whose deadlines passed meanwhile fire at once, in order; every later edge and the end
  /// are at their ORIGINAL deadlines (nothing shifted by the delay).
  func testVeryLateWakeupDoesNotShiftLaterEdges() {
    let clock = VirtualClock()
    clock.lateness = [2: 500_000_000]
    let k = Keyer(clock: clock)
    let light = FakeLight()
    let pat = [60, 60, 180, 60, 60, 60, 180, 420, 60, 60, 1000, 60]
    let r = k.run(pat, light: light)
    XCTAssertFalse(r.stopped)
    let sched = k.scheduledNs, actual = k.edgeTimesNs
    let lateAt = sched[2] + 500_000_000
    for i in 0..<actual.count {
      XCTAssertGreaterThanOrEqual(actual[i], sched[i], "edge \(i) never early")
      if i < 2 { XCTAssertEqual(actual[i], sched[i]) }
      else if sched[i] <= lateAt { XCTAssertEqual(actual[i], lateAt, "edge \(i) overdue: switched at once when the thread woke") }
      else { XCTAssertEqual(actual[i], sched[i], "edge \(i) at its original deadline") }
    }
    XCTAssertTrue(sched.contains { $0 > lateAt }, "the pattern continues past the late wake-up")
    XCTAssertEqual(light.states, (0..<pat.count).map { $0 % 2 == 0 } + [false], "every edge switched, in order")
    XCTAssertEqual(k.endNs, k.endDueNs, "ends at the original time")
    XCTAssertEqual(r.worstLateMs, 500, accuracy: 0.001)
  }

  func testStopMidPatternLeavesLightOff() {
    let clock = VirtualClock()
    let k = Keyer(clock: clock)
    clock.onWait = { i in if i == 3 { k.stop() } }
    let light = FakeLight()
    let r = k.run([100, 100, 100, 100, 100, 100], light: light)
    XCTAssertTrue(r.stopped)
    XCTAssertEqual(light.states.last, false)
    XCTAssertEqual(r.edges, 3)
  }

  func testLimits() {
    func code(_ v: Any) -> String? { if case .failure(let e) = SignalService.parsePattern(["pattern_ms": v]) { return e.code }; return nil }
    XCTAssertNil(code([10, 60_000]))
    XCTAssertEqual(code([]), "bad_params")
    XCTAssertEqual(code([9]), "bad_params")
    XCTAssertEqual(code([60_001]), "bad_params")
    XCTAssertEqual(code(["x"]), "bad_params")
    XCTAssertEqual(code([true]), "bad_params")
    XCTAssertEqual(code(Array(repeating: 100, count: 4097)), "bad_params")
    XCTAssertNil(code(Array(repeating: 100, count: 4096)))
    XCTAssertEqual(code(Array(repeating: 60_000, count: 11)), "bad_params", "over 10 minutes")
    XCTAssertNil(code(Array(repeating: 60_000, count: 10)))
    if case .success = SignalService.parsePattern([:]) { XCTFail() }
  }

  /// A SignalService with fake torch / screen and a virtual clock.
  func service(torch: Bool, busy: Bool = false, torchFails: RPCError? = nil) -> (SignalService, FakeLight, FakeLight) {
    let s = SignalService()
    let t = FakeLight(); t.failBegin = torchFails
    let sc = FakeLight()
    s.clock = VirtualClock()
    s.torchAvailable = { torch }
    s.cameraBusy = { busy }
    s.makeTorch = { t }
    s.makeScreen = { _ in sc }
    return (s, t, sc)
  }

  func flash(_ s: SignalService, _ p: JSONObject) -> RPCResult {
    var out: RPCResult?
    s.flash(params: p, window: nil) { out = $0 }
    XCTAssertTrue(spin(until: 30) { out != nil })
    return out ?? .failure(.internalError("no result"))
  }

  func testModeSelectionFallbackAndBusy() throws {
    let pat: JSONObject = ["pattern_ms": [100, 100, 300]]
    // Torch present.
    var (s, t, sc) = service(torch: true)
    var r = try flash(s, pat).get()
    XCTAssertEqual(r.string("used"), "torch"); XCTAssertEqual(r.bool("stopped"), false)
    XCTAssertNil(r["fallback"])
    XCTAssertNotNil(r.object("timing")?.number("max_late_ms"))
    XCTAssertEqual(t.began, 1); XCTAssertEqual(t.ended, 1); XCTAssertFalse(t.isOn); XCTAssertEqual(sc.began, 0)
    XCTAssertNotNil(r.object("timing")?.number("max_late_ms"))
    r = try flash(s, pat.merging(["mode": "screen"]) { $1 }).get()
    XCTAssertEqual(r.string("used"), "screen"); XCTAssertEqual(sc.ended, 1); XCTAssertFalse(sc.isOn)
    // No torch: auto → screen, torch → unsupported.
    (s, t, sc) = service(torch: false)
    let nt = try flash(s, pat).get()
    XCTAssertEqual(nt.string("used"), "screen"); XCTAssertEqual(nt.string("fallback"), "no_torch")
    guard case .failure(let e1) = flash(s, pat.merging(["mode": "torch"]) { $1 }) else { return XCTFail() }
    XCTAssertEqual(e1.code, "unsupported")
    XCTAssertEqual(t.began, 0)
    // Torch busy (our own QR scanner / Morse camera): auto → screen, torch → busy.
    (s, t, sc) = service(torch: true, busy: true)
    let bz = try flash(s, pat).get()
    XCTAssertEqual(bz.string("used"), "screen"); XCTAssertEqual(bz.string("fallback"), "busy")
    guard case .failure(let e2) = flash(s, pat.merging(["mode": "torch"]) { $1 }) else { return XCTFail() }
    XCTAssertEqual(e2.code, "busy")
    XCTAssertEqual(t.began, 0, "busy torch never touched")
    // Torch refused by the system: auto → screen, torch → that error.
    (s, t, sc) = service(torch: true, torchFails: RPCError("permission_denied", "x"))
    let pd = try flash(s, pat).get()
    XCTAssertEqual(pd.string("used"), "screen"); XCTAssertEqual(pd.string("fallback"), "permission_denied")
    guard case .failure(let e3) = flash(s, pat.merging(["mode": "torch"]) { $1 }) else { return XCTFail() }
    XCTAssertEqual(e3.code, "permission_denied")
    // Bad mode / bad pattern.
    guard case .failure(let e4) = flash(s, ["pattern_ms": [100], "mode": "laser"]) else { return XCTFail() }
    XCTAssertEqual(e4.code, "bad_params")
  }

  /// Real clock: stop (host.signal.stop), a second flash, resign-active, background and screen lock
  /// each end the running flash early with stopped:true, its completion called exactly once, and the
  /// light off. Nothing is read "immediately": every condition is awaited (slow, virtualised hosts).
  func testStopSecondFlashAndLifecycle() {
    for trigger in ["stop", "second", "resign", "background", "lock"] {
      let (s, t, _) = service(torch: true)
      s.clock = MonotonicClock()
      var firstCalls = 0
      var first: RPCResult?
      let firstDone = expectation(description: "first completion (\(trigger))")
      s.flash(params: ["pattern_ms": [30_000, 100]], window: nil) { first = $0; firstCalls += 1; firstDone.fulfill() }
      XCTAssertTrue(spin(until: 10) { t.isOn }, "torch on (\(trigger))")
      var secondDone: XCTestExpectation?
      var second: RPCResult?
      switch trigger {
      case "stop": s.stopAll()
      case "second":
        let e = expectation(description: "second completion"); secondDone = e
        s.flash(params: ["pattern_ms": [20]], window: nil) { second = $0; e.fulfill() }
      case "resign": NotificationCenter.default.post(name: UIApplication.willResignActiveNotification, object: nil)
      case "background": NotificationCenter.default.post(name: UIApplication.didEnterBackgroundNotification, object: nil)
      default: NotificationCenter.default.post(name: UIApplication.protectedDataWillBecomeUnavailableNotification, object: nil)
      }
      // Well before the 30 s pattern would end.
      wait(for: [firstDone] + (secondDone.map { [$0] } ?? []), timeout: 15)
      XCTAssertEqual((try? first?.get())?.bool("stopped"), true, trigger)
      if trigger == "second" { XCTAssertEqual((try? second?.get())?.bool("stopped"), false, "the second flash ran to its end") }
      XCTAssertTrue(spin(until: 10) { t.ended >= 1 && !t.isOn }, "torch released and off (\(trigger))")
      XCTAssertFalse(s.isFlashing)
      spin(until: 0.2) { false }
      XCTAssertEqual(firstCalls, 1, "completion called exactly once (\(trigger))")
    }
  }

  /// Product race (slow scheduler): a second flash must not take the light before the first one has
  /// switched it off and restored its state — otherwise the screen fallback saves "brightness 1.0,
  /// idle timer disabled" as the state to restore, and a torch flash is switched off/unlocked by the
  /// first flash's late teardown. Deterministic: the first light's end() blocks until released.
  func testSecondFlashWaitsForFirstTeardown() {
    final class Gated: SignalLight {
      let name: String; let log: (String) -> Void; let gate: DispatchSemaphore?
      init(_ n: String, _ l: @escaping (String) -> Void, gate: DispatchSemaphore? = nil) { name = n; log = l; self.gate = gate }
      func begin() throws { log("\(name).begin") }
      func set(_ on: Bool) {}
      func end() { gate?.wait(); log("\(name).end") }
    }
    let s = SignalService()
    s.clock = MonotonicClock()
    s.torchAvailable = { false }
    let lock = NSLock()
    var events: [String] = []
    let log: (String) -> Void = { e in lock.lock(); events.append(e); lock.unlock() }
    let gate = DispatchSemaphore(value: 0)
    var n = 0
    // Lights are named in the order they are created: L1 for flash A, L2 for whichever flash runs next.
    s.makeScreen = { _ in n += 1; return Gated("L\(n)", log, gate: n == 1 ? gate : nil) }
    var a: [RPCResult] = [], b: [RPCResult] = [], c: [RPCResult] = []
    let ea = expectation(description: "A"), eb = expectation(description: "B"), ec = expectation(description: "C")
    s.flash(params: ["pattern_ms": [30_000]], window: nil) { a.append($0); ea.fulfill() }
    s.flash(params: ["pattern_ms": [20]], window: nil) { b.append($0); eb.fulfill() }   // queued behind A
    s.flash(params: ["pattern_ms": [20]], window: nil) { c.append($0); ec.fulfill() }   // replaces B before it starts
    spin(until: 0.3) { false }
    lock.lock(); let before = events; lock.unlock()
    XCTAssertEqual(before, ["L1.begin"], "nothing else took the light while A is still tearing down")
    gate.signal()   // A's teardown finishes
    wait(for: [ea, eb, ec], timeout: 15)
    lock.lock(); let all = events; lock.unlock()
    XCTAssertEqual(all, ["L1.begin", "L1.end", "L2.begin", "L2.end"], "the next light began only after A's ended; only two lights were ever made (B never touched one)")
    XCTAssertEqual(a.count, 1); XCTAssertEqual(b.count, 1); XCTAssertEqual(c.count, 1)
    XCTAssertEqual((try? a.first?.get())?.bool("stopped"), true)
    XCTAssertEqual((try? b.first?.get())?.bool("stopped"), true, "replaced before starting: resolved once as stopped")
    XCTAssertEqual((try? c.first?.get())?.bool("stopped"), false)
  }

  /// The real screen fallback, back to back: brightness / idle-timer state ends exactly as before.
  func testBackToBackScreenFlashesRestoreState() throws {
    let window = try XCTUnwrap(UIApplication.shared.connectedScenes.compactMap { ($0 as? UIWindowScene)?.windows.first }.first)
    let s = SignalService()
    s.clock = MonotonicClock()
    s.torchAvailable = { false }
    let idle = UIApplication.shared.isIdleTimerDisabled
    let e1 = expectation(description: "1"), e2 = expectation(description: "2")
    s.flash(params: ["pattern_ms": [5000]], window: window) { _ in e1.fulfill() }
    s.flash(params: ["pattern_ms": [50]], window: window) { _ in e2.fulfill() }
    wait(for: [e1, e2], timeout: 20)
    XCTAssertEqual(UIApplication.shared.isIdleTimerDisabled, idle, "idle timer restored exactly")
    XCTAssertFalse(window.subviews.contains { $0.accessibilityIdentifier == "ember.flash.cover" }, "no cover left")
  }

  /// Through HostRouter: host.info.signal + features, mode contract, host.signal.stop.
  func testRouterContract() throws {
    let n = TestNode(name: "signal")
    n.start()
    let info = try XCTUnwrap(n.host("host.info")).get()
    let sig = try XCTUnwrap(info.object("signal"))
    XCTAssertEqual(sig.bool("screen"), true)
    let feats = info["features"] as? [String] ?? []
    XCTAssertTrue(feats.contains("flash"))
    #if targetEnvironment(simulator)
    XCTAssertEqual(sig.string("torch"), "no")
    XCTAssertFalse(feats.contains("torch"))
    guard case .failure(let e) = n.host("host.signal.flash", ["pattern_ms": [100], "mode": "torch"]) else { return XCTFail() }
    XCTAssertEqual(e.code, "unsupported")
    #endif
    XCTAssertNotEqual(sig.string("torch"), "ask", "iOS never needs a permission for the torch")
    XCTAssertNotNil(try n.host("host.signal.stop")?.get(), "no-op when idle")
    // A real screen flash (simulator: no torch) on the key window, stopped early.
    var res: RPCResult?
    n.host.handleHost(method: "host.signal.flash", params: ["pattern_ms": [3000, 100]]) { res = $0 }
    XCTAssertTrue(spin(until: 10) { n.host.signal.isFlashing })
    XCTAssertEqual(n.host.signal.flashingWith, SystemTorch.available ? "torch" : "screen")
    XCTAssertNotNil(try n.host("host.signal.stop")?.get())
    XCTAssertTrue(spin(until: 15) { res != nil })
    let r = try XCTUnwrap(res).get()
    XCTAssertEqual(r.bool("stopped"), true)
    XCTAssertEqual(r.string("used"), SystemTorch.available ? "torch" : "screen")
    if case .success = n.host("host.signal.flash", ["pattern_ms": [5]]) { XCTFail("limits enforced") }
    n.stop()
  }

  /// The screen fallback restores brightness and the idle timer exactly, and removes its cover.
  func testScreenLightRestoresState() throws {
    let window = try XCTUnwrap(UIApplication.shared.connectedScenes.compactMap { ($0 as? UIWindowScene)?.windows.first }.first)
    let idle = UIApplication.shared.isIdleTimerDisabled
    let light = ScreenLight(window: window)
    try light.begin()
    XCTAssertTrue(light.isCovering)
    XCTAssertTrue(UIApplication.shared.isIdleTimerDisabled)
    light.set(true)
    XCTAssertTrue(spin(until: 10) { light.isOn })
    light.end()
    XCTAssertFalse(light.isCovering)
    XCTAssertFalse(light.isOn)
    XCTAssertEqual(UIApplication.shared.isIdleTimerDisabled, idle)
    XCTAssertThrowsError(try ScreenLight(window: nil).begin())
  }
}

/// Guide library at scale: memory-mapped load time, memory footprint and query latency.
/// Uses $EMBER_BIG_PACK (TEST_RUNNER_EMBER_BIG_PACK, e.g. a 40–60 MB pack) or the bundled pack.
final class GuidePackScaleTests: XCTestCase {
  static func footprintMB() -> Double {
    var info = task_vm_info_data_t()
    var count = mach_msg_type_number_t(MemoryLayout<task_vm_info_data_t>.size / MemoryLayout<integer_t>.size)
    let kr = withUnsafeMutablePointer(to: &info) { $0.withMemoryRebound(to: integer_t.self, capacity: Int(count)) { task_info(mach_task_self_, task_flavor_t(TASK_VM_INFO), $0, &count) } }
    return kr == KERN_SUCCESS ? Double(info.phys_footprint) / 1_048_576 : -1
  }

  func testMappedLoadScales() throws {
    let path = ProcessInfo.processInfo.environment["EMBER_BIG_PACK"].flatMap { $0.isEmpty ? nil : URL(fileURLWithPath: $0) }
      ?? WebContent.bundled().guidesPack
    let url = try XCTUnwrap(path, "no guides.pack")
    let n = TestNode(name: "bigpack")
    n.start()
    let before = GuidePackScaleTests.footprintMB()
    let t0 = Date()
    let pack = try XCTUnwrap(MappedFile(url: url))
    let rc = n.runner.withCore { ($0 as! EmberNode).loadGuides(mapped: pack) }
    let loadMs = Date().timeIntervalSince(t0) * 1000
    XCTAssertEqual(rc, 0)
    let afterLoad = GuidePackScaleTests.footprintMB()
    var t = Date()
    let cats = n.ok("guides.categories")
    let catMs = Date().timeIntervalSince(t) * 1000
    var searchMs: [Double] = []
    for q in ["purify water", "splint broken leg", "antenna dipole", "fire without matches", "hypothermia", "solar panel battery",
              "morse code", "shelter tarp", "radio repeater", "first aid bleeding"] {
      t = Date()
      let r = (n.ok("guides.search", ["query": q, "limit": 30])["results"] as? [JSONObject]) ?? []
      searchMs.append(Date().timeIntervalSince(t) * 1000)
      XCTAssertFalse(r.isEmpty, q)
    }
    let first = ((n.ok("guides.search", ["query": "water"])["results"] as? [JSONObject])?.first?.string("id")) ?? ""
    t = Date()
    let g = n.ok("guides.get", ["id": first])
    let getMs = Date().timeIntervalSince(t) * 1000
    let afterQueries = GuidePackScaleTests.footprintMB()
    print(String(format: "guides scale: %.1f MB pack, %d guides, %d categories; mmap+ember_guides_load(copy=0) %.1f ms; categories %.1f ms; search avg %.1f ms max %.1f ms; get %.1f ms (%d chars); footprint %.1f → %.1f (load) → %.1f MB (after queries)",
                 Double(pack.count) / 1_048_576, cats.int("total") ?? 0, (cats["categories"] as? [Any])?.count ?? 0, loadMs, catMs,
                 searchMs.reduce(0, +) / Double(searchMs.count), searchMs.max() ?? 0, getMs, (g.string("markdown") ?? "").count,
                 before, afterLoad, afterQueries))
    XCTAssertLessThan(loadMs, 500, "zero-copy load stays fast")
    XCTAssertLessThan(afterLoad - before, 30, "mapping does not dirty the pack's pages")
    n.stop()
  }
}
