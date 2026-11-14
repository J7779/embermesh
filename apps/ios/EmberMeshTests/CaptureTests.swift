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
  func stats(_ tr: [SignalService.PatternPlayer.Transition]) -> (mean: Double, p95: Double, max: Double) {
    let errs = tr.map { abs($0.actualMs - $0.scheduledMs) }.sorted()
    guard !errs.isEmpty else { return (0, 0, 0) }
    return (errs.reduce(0, +) / Double(errs.count), errs[min(errs.count - 1, Int(Double(errs.count) * 0.95))], errs.last!)
  }

  /// Edge timing of the flash scheduler at 5 and 20 WPM (the torch path's scheduling; the torch
  /// LED's own switching latency can only be measured on a device).
  func testFlashSchedulerJitter() {
    for wpm in [20.0, 5.0] {
      let text = wpm >= 20 ? "PARIS PARIS" : "SOS"
      let pat = MorseGen.pattern(text, wpm: wpm).map { Int($0.rounded()) }
      let p = SignalService.PatternPlayer()
      p.recordTransitions = true
      var finished = false
      var states: [Bool] = []
      let lock = NSLock()
      p.play(pat, apply: { on in lock.lock(); states.append(on); lock.unlock() }, done: { finished = true })
      XCTAssertTrue(spin(until: Double(pat.reduce(0, +)) / 1000 + 3) { finished })
      let s = stats(p.recorded)
      print(String(format: "flash timing %.0f WPM (dot %.0f ms, %d edges): mean %.3f ms, p95 %.3f ms, max %.3f ms", wpm, 1200 / wpm, p.recorded.count, s.mean, s.p95, s.max))
      XCTAssertEqual(p.recorded.count, pat.count + 1)
      XCTAssertEqual(states.first, true); XCTAssertEqual(states.last, false)
      XCTAssertLessThan(s.p95, 5.0, "edges within 5 ms (p95; ~1 ms when idle, ~2 ms under full-suite load)")
      XCTAssertLessThan(s.max, 15.0)
    }
  }

  /// No torch in the simulator → screen flash: the white overlay toggles and ends hidden.
  func testScreenFlashFallback() throws {
    let svc = SignalService()
    let window = try XCTUnwrap(UIApplication.shared.connectedScenes.compactMap { ($0 as? UIWindowScene)?.windows.first }.first)
    var mode: String?
    let pat = MorseGen.pattern("E E", wpm: 10).map { Int($0) }   // 120 ms on, 840 ms off, 120 ms on
    svc.flash(params: ["pattern_ms": pat], hostView: window.rootViewController?.view ?? window) { r in mode = try? r.get().string("mode") }
    XCTAssertEqual(mode, SignalService.hasTorch ? "torch" : "screen")
    var sawOn = false
    let end = Date().addingTimeInterval(Double(pat.reduce(0, +)) / 1000 + 1)
    while Date() < end { if svc.screenOverlayVisible { sawOn = true }; RunLoop.current.run(until: Date().addingTimeInterval(0.005)) }
    XCTAssertTrue(sawOn, "white overlay was shown")
    XCTAssertFalse(svc.screenOverlayVisible, "and hidden at the end")
    // An empty pattern stops a running one.
    svc.flash(params: ["pattern_ms": [5000, 100]], hostView: window) { _ in }
    svc.flash(params: ["pattern_ms": []], hostView: window) { r in XCTAssertEqual(try? r.get().string("mode"), "off") }
    spin(until: 0.3) { false }
    XCTAssertFalse(svc.screenOverlayVisible)
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
