import AVFoundation
import UIKit

// Native Morse capture sources (docs/RPC_API.md §6 "Capture"): `capture_camera_luma` and
// `capture_mic_pcm`. The host captures itself and calls `morse.rx.feed`; the UI follows the
// decoder through `morse.text` events / `morse.rx.status`.
//
// Why native on iOS even though getUserMedia also works in our WKWebView: the native camera path
// locks exposure, focus and white balance once the scene is metered (auto-exposure "breathing" is
// the main error source when reading a flashlight), reads luma straight from the Y plane of
// 420f frames, and uses real frame timestamps at up to 60 fps.

/// Where feeds go (the node in the app; a recorder in tests).
protocol MorseFeedSink: AnyObject {
  /// `morse.rx.feed` params; completion gets the RPC result (any queue).
  func morseFeed(_ params: JSONObject, completion: @escaping (RPCResult) -> Void)
}

extension NodeRunner: MorseFeedSink {
  func morseFeed(_ params: JSONObject, completion: @escaping (RPCResult) -> Void) {
    callRaw("morse.rx.feed", paramsJSON: JSON.encode(params)) { completion(Envelope.parse($0)) }
  }
}

/// Region of interest in 0..1 frame fractions (default: the centre fifth).
struct CaptureROI: Equatable {
  var x = 0.4, y = 0.4, w = 0.2, h = 0.2
  init() {}
  init?(json o: JSONObject?) {
    guard let o else { return nil }
    let v = (o.number("x") ?? 0.4, o.number("y") ?? 0.4, o.number("w") ?? 0.2, o.number("h") ?? 0.2)
    guard [v.0, v.1, v.2, v.3].allSatisfy({ $0.isFinite }), v.2 > 0, v.3 > 0 else { return nil }
    x = min(max(v.0, 0), 1); y = min(max(v.1, 0), 1)
    w = min(v.2, 1 - x); h = min(v.3, 1 - y)
    guard w > 0, h > 0 else { return nil }
  }
}

enum LumaMath {
  /// Mean of an 8-bit luma plane over `roi`, normalised to 0..1. Samples every `step`-th pixel in
  /// both directions (a 640×480 frame's centre fifth is ~12 k pixels; step 2 keeps it cheap).
  static func mean(base: UnsafeRawPointer, width: Int, height: Int, bytesPerRow: Int, roi: CaptureROI, step: Int = 2) -> Double {
    let x0 = max(0, min(width - 1, Int(roi.x * Double(width))))
    let y0 = max(0, min(height - 1, Int(roi.y * Double(height))))
    let x1 = max(x0 + 1, min(width, Int(((roi.x + roi.w) * Double(width)).rounded())))
    let y1 = max(y0 + 1, min(height, Int(((roi.y + roi.h) * Double(height)).rounded())))
    let p = base.assumingMemoryBound(to: UInt8.self)
    var sum = 0, n = 0
    var y = y0
    while y < y1 {
      let row = p + y * bytesPerRow
      var x = x0
      while x < x1 { sum += Int(row[x]); n += 1; x += step }
      y += step
    }
    return n == 0 ? 0 : Double(sum) / Double(n) / 255
  }
}

/// Batches light samples (frame time + level) into morse.rx.feed calls (~every 100 ms) so the
/// bridge/core is not called 60 times a second, while keeping per-frame timestamps.
final class LightFeeder {
  let session: String
  private weak var sink: MorseFeedSink?
  private var times: [Double] = []
  private var levels: [Double] = []
  private var firstMs: Double = 0
  let flushMs: Double
  /// Called once when the core reports the session is gone (not_found) or another error.
  var onError: ((RPCError) -> Void)?
  private(set) var fed = 0

  init(session: String, sink: MorseFeedSink, flushMs: Double = 100) {
    self.session = session; self.sink = sink; self.flushMs = flushMs
  }

  func add(timeMs: Double, level: Double) {
    if times.isEmpty { firstMs = timeMs }
    times.append((timeMs * 10).rounded() / 10)
    levels.append((max(0, min(1, level)) * 10_000).rounded() / 10_000)
    if timeMs - firstMs >= flushMs || times.count >= 64 { flush() }
  }

  func flush() {
    guard !times.isEmpty, let sink else { return }
    let p: JSONObject = ["session": session, "times_ms": times, "levels": levels]
    fed += times.count
    times.removeAll(keepingCapacity: true); levels.removeAll(keepingCapacity: true)
    sink.morseFeed(p) { [weak self] r in if case .failure(let e) = r { self?.onError?(e) } }
  }
}

/// Mono PCM → signed 16-bit at the session rate (boxcar-averaging decimator: an exact long-run rate
/// for any in/out pair, enough low-pass for 300–1500 Hz tones), base64 batches of ~100 ms.
final class PcmFeeder {
  let session: String
  let outRate: Double
  private weak var sink: MorseFeedSink?
  private var phase = 0.0
  private var acc = 0.0
  private var accN = 0
  private var last = 0.0
  private var out: [Int16] = []
  var onError: ((RPCError) -> Void)?
  private(set) var fedSamples = 0

  init(session: String, sampleRate: Double, sink: MorseFeedSink) {
    self.session = session; self.outRate = sampleRate; self.sink = sink
    out.reserveCapacity(Int(sampleRate / 5))
  }

  /// Float samples (-1..1) at `inRate`.
  func add(_ samples: UnsafeBufferPointer<Float>, inRate: Double) {
    guard inRate > 0 else { return }
    for s in samples {
      acc += Double(s); accN += 1; last = Double(s)
      phase += outRate
      while phase >= inRate {
        let v = accN > 0 ? acc / Double(accN) : last
        out.append(Int16(max(-1, min(1, v)) * 32767))
        acc = 0; accN = 0
        phase -= inRate
        if Double(out.count) >= outRate / 10 { flush() }   // ~100 ms per feed
      }
    }
  }

  func flush() {
    guard !out.isEmpty, let sink else { return }
    let data = out.withUnsafeBufferPointer { b -> Data in
      var d = Data(capacity: b.count * 2)
      for v in b { var le = v.littleEndian; withUnsafeBytes(of: &le) { d.append(contentsOf: $0) } }
      return d
    }
    fedSamples += out.count
    out.removeAll(keepingCapacity: true)
    sink.morseFeed(["session": session, "pcm_b64": data.base64EncodedString()]) { [weak self] r in
      if case .failure(let e) = r { self?.onError?(e) }
    }
  }
}

/// Camera → luma of the ROI per frame, with a native preview shown where the UI asks.
final class CameraLumaCapture: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate {
  let feeder: LightFeeder
  private let queue = DispatchQueue(label: "org.embermesh.capture.camera", qos: .userInitiated)
  private var session: AVCaptureSession?
  private var device: AVCaptureDevice?
  private(set) var roi: CaptureROI
  private var preview: CapturePreviewView?
  private var lockAt: Date?
  private(set) var frames = 0
  private(set) var exposureLocked = false

  init(feeder: LightFeeder, roi: CaptureROI) {
    self.feeder = feeder
    self.roi = roi
  }

  static var hasCamera: Bool {
    #if targetEnvironment(simulator)
    return false
    #else
    return AVCaptureDevice.default(for: .video) != nil
    #endif
  }

  /// Starts the camera (back by default). Throws `unsupported` without a camera, `denied` without permission.
  func start(camera: String, previewIn host: UIView?, rect: CGRect?, completion: @escaping (RPCError?) -> Void) {
    AVCaptureDevice.requestAccess(for: .video) { ok in
      guard ok else { completion(.denied("Camera permission is off. Turn it on in Settings > EmberMesh > Camera.")); return }
      self.queue.async {
        let pos: AVCaptureDevice.Position = camera == "front" ? .front : .back
        guard let dev = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: pos) ?? AVCaptureDevice.default(for: .video) else {
          completion(.unsupported("no camera on this device")); return
        }
        let s = AVCaptureSession()
        s.beginConfiguration()
        if s.canSetSessionPreset(.vga640x480) { s.sessionPreset = .vga640x480 }
        guard let input = try? AVCaptureDeviceInput(device: dev), s.canAddInput(input) else {
          completion(.io("cannot open the camera")); return
        }
        s.addInput(input)
        let out = AVCaptureVideoDataOutput()
        out.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_420YpCbCr8BiPlanarFullRange]
        out.alwaysDiscardsLateVideoFrames = true
        out.setSampleBufferDelegate(self, queue: self.queue)
        guard s.canAddOutput(out) else { completion(.io("cannot read camera frames")); return }
        s.addOutput(out)
        s.commitConfiguration()
        CameraLumaCapture.configureFastFrames(dev)
        self.session = s
        self.device = dev
        self.lockAt = Date().addingTimeInterval(0.8)   // meter the scene first, then lock
        s.startRunning()
        DispatchQueue.main.async {
          if let host, let rect { self.showPreview(in: host, rect: rect) }
          completion(nil)
        }
      }
    }
  }

  /// Highest frame rate ≤ 60 fps at ≤ 640×480 (more samples per Morse element), continuous AF/AE
  /// until the lock.
  static func configureFastFrames(_ dev: AVCaptureDevice) {
    guard (try? dev.lockForConfiguration()) != nil else { return }
    defer { dev.unlockForConfiguration() }
    var best: (AVCaptureDevice.Format, Double)?
    for f in dev.formats {
      let d = CMVideoFormatDescriptionGetDimensions(f.formatDescription)
      guard d.width <= 1280, d.height <= 720 else { continue }
      let maxRate = f.videoSupportedFrameRateRanges.map { $0.maxFrameRate }.max() ?? 0
      let rate = min(60, maxRate)
      if best == nil || rate > best!.1 { best = (f, rate) }
    }
    if let (f, rate) = best, rate >= 30 {
      dev.activeFormat = f
      let dur = CMTime(value: 1, timescale: CMTimeScale(rate))
      dev.activeVideoMinFrameDuration = dur
      dev.activeVideoMaxFrameDuration = dur
    }
    if dev.isExposureModeSupported(.continuousAutoExposure) { dev.exposureMode = .continuousAutoExposure }
    if dev.isFocusModeSupported(.continuousAutoFocus) { dev.focusMode = .continuousAutoFocus }
  }

  private func lockExposure() {
    guard let dev = device, (try? dev.lockForConfiguration()) != nil else { return }
    if dev.isExposureModeSupported(.locked) { dev.exposureMode = .locked }
    if dev.isFocusModeSupported(.locked) { dev.focusMode = .locked }
    if dev.isWhiteBalanceModeSupported(.locked) { dev.whiteBalanceMode = .locked }
    dev.unlockForConfiguration()
    exposureLocked = true
  }

  func update(rect: CGRect?, roi: CaptureROI?, host: UIView?) {
    if let roi { queue.async { self.roi = roi } }
    DispatchQueue.main.async {
      if let roi { self.preview?.roi = roi }
      if let rect {
        if let p = self.preview { p.frame = rect } else if let host { self.showPreview(in: host, rect: rect) }
      }
    }
  }

  func stop() {
    queue.sync {
      session?.stopRunning()
      session = nil
      device = nil
      feeder.flush()
    }
    DispatchQueue.main.async { self.preview?.removeFromSuperview(); self.preview = nil }
  }

  private func showPreview(in host: UIView, rect: CGRect) {
    guard let s = session else { return }
    let v = CapturePreviewView(session: s, roi: roi)
    v.frame = rect
    host.addSubview(v)
    preview = v
  }

  // MARK: frames (camera queue; also called by tests with synthetic buffers)

  func process(_ sb: CMSampleBuffer) {
    guard let pb = CMSampleBufferGetImageBuffer(sb) else { return }
    let t = CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sb)) * 1000
    guard t.isFinite else { return }
    CVPixelBufferLockBaseAddress(pb, .readOnly)
    defer { CVPixelBufferUnlockBaseAddress(pb, .readOnly) }
    let planar = CVPixelBufferIsPlanar(pb)
    guard let base = planar ? CVPixelBufferGetBaseAddressOfPlane(pb, 0) : CVPixelBufferGetBaseAddress(pb) else { return }
    let w = planar ? CVPixelBufferGetWidthOfPlane(pb, 0) : CVPixelBufferGetWidth(pb)
    let h = planar ? CVPixelBufferGetHeightOfPlane(pb, 0) : CVPixelBufferGetHeight(pb)
    let bpr = planar ? CVPixelBufferGetBytesPerRowOfPlane(pb, 0) : CVPixelBufferGetBytesPerRow(pb)
    let level = LumaMath.mean(base: UnsafeRawPointer(base), width: w, height: h, bytesPerRow: bpr, roi: roi)
    frames += 1
    feeder.add(timeMs: t, level: level)
    if let at = lockAt, Date() >= at { lockAt = nil; lockExposure() }
  }

  func captureOutput(_ output: AVCaptureOutput, didOutput sb: CMSampleBuffer, from connection: AVCaptureConnection) {
    process(sb)
  }
}

/// Native camera preview with the watched region outlined.
final class CapturePreviewView: UIView {
  private let layerPreview: AVCaptureVideoPreviewLayer
  private let box = CAShapeLayer()
  var roi: CaptureROI { didSet { setNeedsLayout() } }

  init(session: AVCaptureSession, roi: CaptureROI) {
    layerPreview = AVCaptureVideoPreviewLayer(session: session)
    self.roi = roi
    super.init(frame: .zero)
    layerPreview.videoGravity = .resizeAspectFill
    layer.addSublayer(layerPreview)
    box.strokeColor = UIColor.systemOrange.cgColor
    box.fillColor = UIColor.clear.cgColor
    box.lineWidth = 2
    layer.addSublayer(box)
    clipsToBounds = true
    layer.cornerRadius = 8
    isUserInteractionEnabled = false
    accessibilityLabel = "Camera preview for reading Morse light"
  }
  required init?(coder: NSCoder) { fatalError() }

  override func layoutSubviews() {
    super.layoutSubviews()
    layerPreview.frame = bounds
    // ROI is in frame fractions; map through the preview layer's own aspect-fill conversion.
    let r = layerPreview.layerRectConverted(fromMetadataOutputRect: CGRect(x: roi.x, y: roi.y, width: roi.w, height: roi.h))
    box.path = UIBezierPath(rect: r.isEmpty ? bounds.insetBy(dx: bounds.width * 0.4, dy: bounds.height * 0.4) : r).cgPath
  }
}

/// Microphone → PCM at the session rate. Shares the running sound link's input when there is one
/// (two engines would fight over the one input); otherwise runs its own AVAudioEngine with the same
/// measurement-mode session the audio endpoint uses.
final class MicPcmCapture {
  let feeder: PcmFeeder
  private let engine = AVAudioEngine()
  private var ownEngine = false
  private weak var shared: AudioEndpoint?
  private var token: Int?

  init(feeder: PcmFeeder) { self.feeder = feeder }

  func start(sharing endpoint: AudioEndpoint?, completion: @escaping (RPCError?) -> Void) {
    if let ep = endpoint, ep.isConnected {
      shared = ep
      token = ep.addInputListener { [weak self] buf, rate in self?.process(buf, rate: Double(rate)) }
      completion(nil)
      return
    }
    AudioAvailability.requestPermission { [self] ok in
      guard ok else { completion(.denied("Microphone permission is off. Turn it on in Settings > EmberMesh > Microphone.")); return }
      do {
        let s = AVAudioSession.sharedInstance()
        if s.category != .playAndRecord {
          try s.setCategory(.playAndRecord, mode: .measurement, options: [.defaultToSpeaker, .mixWithOthers])
        }
        try s.setActive(true)
        let input = self.engine.inputNode
        let fmt = input.outputFormat(forBus: 0)
        guard fmt.sampleRate >= 8000, fmt.channelCount > 0 else { completion(.io("no microphone input available")); return }
        input.installTap(onBus: 0, bufferSize: AVAudioFrameCount(max(256, Int(fmt.sampleRate * 0.02))), format: fmt) { [weak self] b, _ in
          self?.process(b, rate: fmt.sampleRate)
        }
        self.engine.prepare()
        try self.engine.start()
        self.ownEngine = true
        completion(nil)
      } catch {
        completion(.io("microphone start failed: \(error.localizedDescription)"))
      }
    }
  }

  /// Tap path (also used by tests with synthetic buffers).
  func process(_ buf: AVAudioPCMBuffer, rate: Double) {
    guard let ch = buf.floatChannelData?[0] else { return }
    feeder.add(UnsafeBufferPointer(start: ch, count: Int(buf.frameLength)), inRate: rate)
  }

  func stop() {
    if let t = token { shared?.removeInputListener(t); token = nil }
    if ownEngine {
      engine.inputNode.removeTap(onBus: 0)
      engine.stop()
      ownEngine = false
    }
    feeder.flush()
  }
}

/// host.capture.start / update / stop.
final class CaptureService {
  private weak var sink: MorseFeedSink?
  /// The running sound link's endpoint, if any (mic sharing).
  var audioEndpoint: () -> AudioEndpoint? = { nil }
  /// Where the camera preview goes (the WKWebView: CSS px == its points).
  var previewHost: () -> UIView? = { nil }
  var onEvent: ((String, JSONObject) -> Void)?
  private(set) var camera: CameraLumaCapture?
  private(set) var mic: MicPcmCapture?

  init(sink: MorseFeedSink?) { self.sink = sink }

  static var features: [String] {
    var f = ["capture_mic_pcm"]
    if CameraLumaCapture.hasCamera { f.insert("capture_camera_luma", at: 0) }
    return f
  }

  static func rect(_ o: JSONObject?) -> CGRect? {
    guard let o, let w = o.number("w"), let h = o.number("h"), w > 0, h > 0 else { return nil }
    return CGRect(x: o.number("x") ?? 0, y: o.number("y") ?? 0, width: w, height: h)
  }

  func start(_ p: JSONObject, completion: @escaping RPCCompletion) {
    guard let kind = p.string("kind") else { completion(.failure(.badParams("kind is required"))); return }
    guard let session = p.string("session") ?? p.int("session").map(String.init), !session.isEmpty else {
      completion(.failure(.badParams("session is required (from morse.rx.start)"))); return
    }
    guard let sink else { completion(.failure(.busy("node is not running"))); return }
    switch kind {
    case "camera_luma":
      guard CameraLumaCapture.hasCamera else { completion(.failure(.unsupported("this device has no camera"))); return }
      camera?.stop()
      let feeder = LightFeeder(session: session, sink: sink)
      let cam = CameraLumaCapture(feeder: feeder, roi: CaptureROI(json: p.object("roi")) ?? CaptureROI())
      feeder.onError = { [weak self] e in self?.failed("camera_luma", e) }
      camera = cam
      cam.start(camera: p.string("camera") ?? "back", previewIn: previewHost(), rect: CaptureService.rect(p.object("rect"))) { err in
        DispatchQueue.main.async {
          if let err { self.camera = nil; completion(.failure(err)) } else { completion(.success([:])) }
        }
      }
    case "mic_pcm":
      mic?.stop()
      let rate = max(4000, min(48_000, p.number("sample_rate") ?? 8000))
      let feeder = PcmFeeder(session: session, sampleRate: rate, sink: sink)
      let m = MicPcmCapture(feeder: feeder)
      feeder.onError = { [weak self] e in self?.failed("mic_pcm", e) }
      mic = m
      m.start(sharing: audioEndpoint()) { err in
        DispatchQueue.main.async {
          if let err { self.mic = nil; completion(.failure(err)) } else { completion(.success(["sample_rate": rate])) }
        }
      }
    default:
      completion(.failure(.badParams("kind must be camera_luma or mic_pcm")))
    }
  }

  func update(_ p: JSONObject, completion: RPCCompletion) {
    guard p.string("kind") == "camera_luma" else { completion(.success([:])); return }  // nothing to update for the mic
    guard let cam = camera else { completion(.failure(.notFound("camera capture is not running"))); return }
    cam.update(rect: CaptureService.rect(p.object("rect")), roi: CaptureROI(json: p.object("roi")), host: previewHost())
    completion(.success([:]))
  }

  func stop(_ p: JSONObject, completion: RPCCompletion) {
    let kind = p.string("kind")
    if kind == nil || kind == "camera_luma" { camera?.stop(); camera = nil }
    if kind == nil || kind == "mic_pcm" { mic?.stop(); mic = nil }
    completion(.success([:]))
  }

  /// The core closed the session (60 s without feed, morse.rx.stop) or rejected a feed.
  private func failed(_ kind: String, _ e: RPCError) {
    DispatchQueue.main.async {
      if kind == "camera_luma" { self.camera?.stop(); self.camera = nil } else { self.mic?.stop(); self.mic = nil }
      emberLog("capture \(kind) stopped: \(e)")
      self.onEvent?("host.capture.state", ["kind": kind, "running": false, "error": e.message])
    }
  }
}
