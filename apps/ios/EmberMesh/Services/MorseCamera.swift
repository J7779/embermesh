import AVFoundation
import CEmber
import UIKit
import WebKit

// Morse camera reader, native source `capture_camera_luma` (docs/RPC_API.md §12 + §6).
//
// Every camera frame is reduced to a whole-frame brightness grid — the LARGEST luma per cell, taken
// from the full-resolution Y plane (ember_ios_grid.c, so a light covering a single pixel decides its
// cell), plus the cell mean — rotated/mirrored to match what the user sees, and fed to the core's
// tracker as `morse.rx.feed {session, frames:[{t_ms, w, h, max_b64, mean_b64}]}` in batches of at
// most 8 frames / 100 ms. Frames never leave this file except as those grids; nothing is stored or
// logged.

/// How the sensor-oriented grid must be turned to match the screen (quarter turns clockwise, then a
/// horizontal mirror for the front camera).
struct GridOrientation: Equatable {
  var quarterTurnsCW = 1          // portrait UI with the rear camera's landscape sensor
  var mirrored = false

  /// Rear camera buffers come in the sensor's landscape-right orientation.
  static func forInterface(_ o: UIInterfaceOrientation, front: Bool) -> GridOrientation {
    let turns: Int
    switch o {
    case .landscapeRight: turns = 0
    case .portraitUpsideDown: turns = 3
    case .landscapeLeft: turns = 2
    default: turns = 1   // portrait / unknown
    }
    return GridOrientation(quarterTurnsCW: front ? (4 - turns) % 4 : turns, mirrored: front)
  }
}

enum LumaGrid {
  /// Grid size for a sensor frame: the long side 64 cells, the short side in proportion (≥ 1),
  /// w·h ≤ 4096 (16:9 → 64×36, 4:3 → 64×48).
  static func dims(width: Int, height: Int) -> (Int, Int) {
    guard width > 0, height > 0 else { return (1, 1) }
    if width >= height {
      let gh = max(1, min(64, Int((64 * Double(height) / Double(width)).rounded())))
      return (min(64, width), min(gh, height))
    }
    let gw = max(1, min(64, Int((64 * Double(width) / Double(height)).rounded())))
    return (min(gw, width), min(64, height))
  }

  /// Rotates a row-major grid by quarter turns clockwise, then mirrors horizontally.
  static func orient(_ g: [UInt8], w: Int, h: Int, _ o: GridOrientation) -> ([UInt8], Int, Int) {
    var src = g, sw = w, sh = h
    for _ in 0..<((o.quarterTurnsCW % 4 + 4) % 4) {
      var dst = [UInt8](repeating: 0, count: src.count)
      // (x, y) in a sw×sh grid → (sh-1-y, x) in an sh×sw grid
      for y in 0..<sh { for x in 0..<sw { dst[x * sh + (sh - 1 - y)] = src[y * sw + x] } }
      src = dst; swap(&sw, &sh)
    }
    if o.mirrored {
      for y in 0..<sh { src[(y * sw)..<(y * sw + sw)].reverse() }
    }
    return (src, sw, sh)
  }

  /// Max and mean grids of a luma plane, sensor orientation.
  static func reduce(plane: UnsafeRawPointer, width: Int, height: Int, bytesPerRow: Int) -> (max: [UInt8], mean: [UInt8], w: Int, h: Int)? {
    let (gw, gh) = dims(width: width, height: height)
    var mx = [UInt8](repeating: 0, count: gw * gh), mean = [UInt8](repeating: 0, count: gw * gh)
    let rc = mx.withUnsafeMutableBufferPointer { m in
      mean.withUnsafeMutableBufferPointer { a in
        ember_ios_luma_grid(plane.assumingMemoryBound(to: UInt8.self), width, height, bytesPerRow, UInt32(gw), UInt32(gh), m.baseAddress, a.baseAddress)
      }
    }
    return rc == 0 ? (mx, mean, gw, gh) : nil
  }
}

/// Batches grid frames into morse.rx.feed (≤ 8 frames / 100 ms per call, never reordered). While a
/// call is in flight and the next batch is full, newer frames are dropped instead of queued.
final class GridFeeder {
  let session: String
  private weak var sink: MorseFeedSink?
  private var batch: [JSONObject] = []
  private var batchStartMs = 0.0
  private var inFlight = false
  private(set) var sent = 0
  private(set) var dropped = 0
  /// Result of each feed (MorseStatus or error), on the feeder's queue.
  var onResult: ((RPCResult) -> Void)?
  private let queue: DispatchQueue

  init(session: String, sink: MorseFeedSink, queue: DispatchQueue) {
    self.session = session; self.sink = sink; self.queue = queue
  }

  /// Capture queue.
  func add(_ frame: JSONObject, tMs: Double) {
    if batch.count >= 8 {
      if inFlight { dropped += 1; return }   // core busy: drop, do not queue
      flush()
    }
    if batch.isEmpty { batchStartMs = tMs }
    batch.append(frame)
    if !inFlight, batch.count >= 8 || tMs - batchStartMs >= 100 { flush() }
  }

  func flush() {
    guard !batch.isEmpty, !inFlight, let sink else { return }
    let frames = batch
    batch.removeAll(keepingCapacity: true)
    inFlight = true
    sent += frames.count
    sink.morseFeed(["session": session, "frames": frames]) { [weak self] r in
      guard let self else { return }
      self.queue.async {
        self.inFlight = false
        self.onResult?(r)
        if self.batch.count >= 8 { self.flush() }   // a full batch waited for this reply
      }
    }
  }
}

/// The camera: AVCaptureSession on the rear wide camera, 420f full-range frames, settings of §12.
final class CameraLumaCapture: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate {
  enum Mode { case frames, levels }

  let queue = DispatchQueue(label: "org.embermesh.capture.camera", qos: .userInitiated)
  let grids: GridFeeder
  /// Old core without `frames`: whole-frame-centre ROI levels (section 6), decided by the first reply.
  let levels: LightFeeder
  private(set) var mode: Mode = .frames
  private var framesAccepted = false
  private let lock = NSLock()
  private var _orientation = GridOrientation()
  var orientation: GridOrientation {
    get { lock.lock(); defer { lock.unlock() }; return _orientation }
    set { lock.lock(); _orientation = newValue; lock.unlock() }
  }
  private var session: AVCaptureSession?
  private(set) var device: AVCaptureDevice?
  private var lockWBAt: Date?
  private(set) var frameCount = 0
  private(set) var exposureLocked = false
  private(set) var lastTargetState = "searching"
  /// The core closed the session or refused the feed for good.
  var onFatal: ((RPCError) -> Void)?
  /// `MorseStatus.target` of every feed reply + the grid aspect (w/h as shown), capture queue.
  var onTarget: ((JSONObject, Double) -> Void)?
  private(set) var gridAspect = 9.0 / 16.0
  /// Per-frame reduction time in ms (last 64 frames), for measurement.
  private(set) var reduceMs: [Double] = []
  private let roi: CaptureROI
  var front = false

  /// A core whose node.info.features lacks "morse_frames" (RPC_API §12) gets the levels feed from
  /// the start; the first-error fallback below stays as the second line of defence.
  func useLevelsFeed() { queue.sync { mode = .levels } }

  init(session id: String, sink: MorseFeedSink, roi: CaptureROI = CaptureROI()) {
    grids = GridFeeder(session: id, sink: sink, queue: queue)
    levels = LightFeeder(session: id, sink: sink)
    self.roi = roi
    super.init()
    grids.onResult = { [weak self] r in self?.feedResult(r) }
    levels.onError = { [weak self] e in self?.onFatal?(e) }
  }

  static var hasCamera: Bool {
    #if targetEnvironment(simulator)
    return false
    #else
    return AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: .back) != nil
    #endif
  }

  // MARK: start / stop

  /// Starts the camera. `simulated` (tests, no camera): no AVCaptureSession; frames come from process().
  func start(camera: String, simulated: Bool, completion: @escaping (RPCError?) -> Void) {
    front = camera == "front"
    if simulated { completion(nil); return }
    // Asked here, when the user starts reading — never before.
    AVCaptureDevice.requestAccess(for: .video) { ok in
      guard ok else { completion(.denied("Camera permission is off. Turn it on in Settings > EmberMesh > Camera.")); return }
      self.queue.async {
        let pos: AVCaptureDevice.Position = camera == "front" ? .front : .back
        guard let dev = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: pos) else {
          completion(.unsupported("no camera on this device")); return
        }
        let s = AVCaptureSession()
        s.beginConfiguration()
        s.sessionPreset = .inputPriority   // the format chosen below decides
        guard let input = try? AVCaptureDeviceInput(device: dev), s.canAddInput(input) else { completion(.io("cannot open the camera")); return }
        s.addInput(input)
        let out = AVCaptureVideoDataOutput()
        out.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_420YpCbCr8BiPlanarFullRange]
        out.alwaysDiscardsLateVideoFrames = true   // drop, never queue
        out.setSampleBufferDelegate(self, queue: self.queue)
        guard s.canAddOutput(out) else { completion(.io("cannot read camera frames")); return }
        s.addOutput(out)
        CameraLumaCapture.configure(dev)
        s.commitConfiguration()
        self.session = s
        self.device = dev
        self.lockWBAt = Date().addingTimeInterval(0.8)
        s.startRunning()
        completion(nil)
      }
    }
  }

  var captureSession: AVCaptureSession? { queue.sync { session } }

  func stop() {
    queue.sync {
      session?.stopRunning()
      session = nil
      device = nil
      grids.flush()
      levels.flush()
    }
  }

  /// §12 camera settings: ≥ 30 fps (60 where a ≥ 1280×720, ≤ 1920×1080 format allows), continuous
  /// auto-focus, exposure biased down, torch off, no HDR / low-light boost.
  static func configure(_ dev: AVCaptureDevice) {
    guard (try? dev.lockForConfiguration()) != nil else { return }
    defer { dev.unlockForConfiguration() }
    func fullRange(_ f: AVCaptureDevice.Format) -> Bool {
      CMFormatDescriptionGetMediaSubType(f.formatDescription) == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
    }
    var best: (AVCaptureDevice.Format, Double, Int32)?
    for f in dev.formats where fullRange(f) {
      let d = CMVideoFormatDescriptionGetDimensions(f.formatDescription)
      guard d.width <= 1920, d.height <= 1080, d.width >= 640 else { continue }
      let rate = min(60, f.videoSupportedFrameRateRanges.map { $0.maxFrameRate }.max() ?? 0)
      guard rate >= 30 else { continue }
      let score = (rate >= 60 && d.width >= 1280 ? 2.0 : 1.0) * 1e7 + Double(d.width) * Double(d.height)
      if best == nil || score > best!.1 { best = (f, score, Int32(rate >= 60 && d.width >= 1280 ? 60 : 30)) }
    }
    if let (f, _, fps) = best {
      dev.activeFormat = f
      dev.activeVideoMinFrameDuration = CMTime(value: 1, timescale: fps)
      dev.activeVideoMaxFrameDuration = CMTime(value: 1, timescale: fps)
      if f.isVideoHDRSupported { dev.automaticallyAdjustsVideoHDREnabled = false; dev.isVideoHDREnabled = false }
    }
    if dev.isLowLightBoostSupported { dev.automaticallyEnablesLowLightBoostWhenAvailable = false }
    if dev.isFocusModeSupported(.continuousAutoFocus) { dev.focusMode = .continuousAutoFocus }
    if dev.isExposureModeSupported(.continuousAutoExposure) { dev.exposureMode = .continuousAutoExposure }
    dev.setExposureTargetBias(max(dev.minExposureTargetBias, -1.0), completionHandler: nil)
    if dev.hasTorch, dev.isTorchModeSupported(.off) { dev.torchMode = .off }
    dev.videoZoomFactor = 1
  }

  /// 1×, 2×, 5× — those the camera can do.
  var zoomSteps: [Double] {
    guard let d = queue.sync(execute: { device }) else { return [1] }
    return [1.0, 2.0, 5.0].filter { $0 <= Double(d.maxAvailableVideoZoomFactor) }
  }

  func setZoom(_ z: Double) {
    queue.async {
      guard let d = self.device, (try? d.lockForConfiguration()) != nil else { return }
      d.videoZoomFactor = CGFloat(max(1, min(z, Double(d.maxAvailableVideoZoomFactor))))
      d.unlockForConfiguration()
    }
  }

  /// Exposure follows the tracker: locked (biased down) while a light is being read, automatic
  /// (biased down) while searching or lost. White balance is locked once the scene settled.
  private func targetState(_ st: String) {
    guard st != lastTargetState else { return }
    lastTargetState = st
    guard let d = device, (try? d.lockForConfiguration()) != nil else { return }
    defer { d.unlockForConfiguration() }
    if st == "locked", d.isExposureModeSupported(.locked) {
      d.exposureMode = .locked
      exposureLocked = true
    } else if d.isExposureModeSupported(.continuousAutoExposure) {
      d.exposureMode = .continuousAutoExposure
      d.setExposureTargetBias(max(d.minExposureTargetBias, -1.0), completionHandler: nil)
      exposureLocked = false
    }
  }

  private func lockWhiteBalance() {
    guard let d = device, d.isWhiteBalanceModeSupported(.locked), (try? d.lockForConfiguration()) != nil else { return }
    d.whiteBalanceMode = .locked
    d.unlockForConfiguration()
  }

  // MARK: frames (capture queue; tests call process() with synthetic buffers)

  func process(_ sb: CMSampleBuffer) {
    guard let pb = CMSampleBufferGetImageBuffer(sb) else { return }
    // Presentation time is on the host clock (monotonic), in ms.
    let t = CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sb)) * 1000
    guard t.isFinite else { return }
    CVPixelBufferLockBaseAddress(pb, .readOnly)
    defer { CVPixelBufferUnlockBaseAddress(pb, .readOnly) }
    let planar = CVPixelBufferIsPlanar(pb)
    guard let base = planar ? CVPixelBufferGetBaseAddressOfPlane(pb, 0) : CVPixelBufferGetBaseAddress(pb) else { return }
    let w = planar ? CVPixelBufferGetWidthOfPlane(pb, 0) : CVPixelBufferGetWidth(pb)
    let h = planar ? CVPixelBufferGetHeightOfPlane(pb, 0) : CVPixelBufferGetHeight(pb)
    let bpr = planar ? CVPixelBufferGetBytesPerRowOfPlane(pb, 0) : CVPixelBufferGetBytesPerRow(pb)
    frameCount += 1
    if let at = lockWBAt, Date() >= at { lockWBAt = nil; lockWhiteBalance() }
    if mode == .levels {
      levels.add(timeMs: t, level: LumaMath.mean(base: UnsafeRawPointer(base), width: w, height: h, bytesPerRow: bpr, roi: roi))
      return
    }
    let t0 = DispatchTime.now().uptimeNanoseconds
    guard let g = LumaGrid.reduce(plane: UnsafeRawPointer(base), width: w, height: h, bytesPerRow: bpr) else { return }
    let o = orientation
    let (mx, ow, oh) = LumaGrid.orient(g.max, w: g.w, h: g.h, o)
    let (mean, _, _) = LumaGrid.orient(g.mean, w: g.w, h: g.h, o)
    gridAspect = Double(ow) / Double(max(1, oh))
    let frame: JSONObject = ["t_ms": (t * 10).rounded() / 10, "w": ow, "h": oh,
                             "max_b64": Data(mx).base64EncodedString(), "mean_b64": Data(mean).base64EncodedString()]
    reduceMs.append(Double(DispatchTime.now().uptimeNanoseconds - t0) / 1e6)
    if reduceMs.count > 64 { reduceMs.removeFirst() }
    grids.add(frame, tMs: t)
  }

  /// The core's reply to a frames feed (capture queue).
  private func feedResult(_ r: RPCResult) {
    switch r {
    case .success(let st):
      framesAccepted = true
      if let tg = st.object("target") {
        if let s = tg.string("state") { targetState(s) }
        onTarget?(tg, gridAspect)
      }
    case .failure(let e) where !framesAccepted && (e.code == "bad_params" || e.code == "unsupported" || e.code == "not_implemented"):
      // A core from before RPC_API §12: fall back to the section-6 levels feed (centre region).
      mode = .levels
    case .failure(let e):
      onFatal?(e)
    }
  }

  func captureOutput(_ output: AVCaptureOutput, didOutput sb: CMSampleBuffer, from connection: AVCaptureConnection) {
    process(sb)
  }
}

/// The native preview: exactly the page's rect (clipped). Because it is drawn above the page, the
/// host draws the one thing the page cannot: the marker ring on the tracked light (RPC_API §12) —
/// a thin #ff8a3d ring centred on target x, y, diameter max(size × preview width, 28 pt), shown while
/// "locked", dimmed while "lost", absent while "searching", moved smoothly. Nothing else.
final class CapturePreviewView: UIView {
  let previewLayer: AVCaptureVideoPreviewLayer?
  let ring = CAShapeLayer()
  static let accent = UIColor(red: 1, green: 0x8a / 255.0, blue: 0x3d / 255.0, alpha: 1)
  private(set) var targetState = "searching"
  private var target: (x: Double, y: Double, size: Double, aspect: Double)?

  init(session: AVCaptureSession?) {
    previewLayer = session.map { AVCaptureVideoPreviewLayer(session: $0) }
    super.init(frame: .zero)
    backgroundColor = .black
    clipsToBounds = true
    isUserInteractionEnabled = false     // taps go to the page (morse.rx.target)
    isAccessibilityElement = false       // the page describes the reader
    accessibilityIdentifier = "ember.morse.preview"
    if let l = previewLayer {
      l.videoGravity = .resizeAspectFill
      layer.addSublayer(l)
    }
    ring.fillColor = UIColor.clear.cgColor
    ring.strokeColor = CapturePreviewView.accent.cgColor
    ring.lineWidth = 2
    ring.opacity = 0
    layer.addSublayer(ring)
  }

  /// `target` from MorseStatus / morse.target; `aspect` = frame width / height as shown. Main queue.
  func setTarget(_ t: JSONObject, aspect: Double) {
    targetState = t.string("state") ?? "searching"
    if let x = t.number("x"), let y = t.number("y") {
      target = (min(max(x, 0), 1), min(max(y, 0), 1), max(0, t.number("size") ?? 0), aspect > 0 ? aspect : 9.0 / 16.0)
    }
    layoutRing(animated: true)
  }

  /// Frame point (0..1 of the frame as shown) → view point, through the aspect-fill crop.
  static func viewPoint(x: Double, y: Double, aspect: Double, in b: CGRect) -> CGPoint {
    let fw = aspect, fh = 1.0
    let scale = max(Double(b.width) / fw, Double(b.height) / fh)
    let dw = fw * scale, dh = fh * scale
    return CGPoint(x: (Double(b.width) - dw) / 2 + x * dw, y: (Double(b.height) - dh) / 2 + y * dh)
  }

  var ringFrame: CGRect { ring.frame }
  var ringOpacity: Float { ring.opacity }

  private func layoutRing(animated: Bool) {
    CATransaction.begin()
    CATransaction.setDisableActions(!animated)
    CATransaction.setAnimationDuration(0.15)
    let visible: Float = targetState == "locked" ? 1 : (targetState == "lost" ? 0.35 : 0)
    if let t = target, visible > 0 {
      let d = max(CGFloat(t.size) * bounds.width, 28)
      let c = CapturePreviewView.viewPoint(x: t.x, y: t.y, aspect: t.aspect, in: bounds)
      ring.bounds = CGRect(x: 0, y: 0, width: d, height: d)
      ring.position = c
      ring.path = UIBezierPath(ovalIn: ring.bounds.insetBy(dx: 1, dy: 1)).cgPath
      ring.opacity = visible
    } else {
      ring.opacity = 0
    }
    CATransaction.commit()
  }
  required init?(coder: NSCoder) { fatalError("not used") }

  override func layoutSubviews() {
    super.layoutSubviews()
    previewLayer?.frame = bounds
    layoutRing(animated: false)
  }
}

/// Places the preview from the page's `rect` (CSS px of the web view's visible viewport):
///   view rect = rect × (pageZoom × zoomScale), in the WKWebView's own coordinates.
/// The app's web view sits inside the safe area with contentInsetAdjustmentBehavior = .never and the
/// page uses viewport-fit=cover, so the CSS viewport origin IS the web view's origin (no content
/// inset to add); the safe area is already excluded by the web view's frame. The rect was measured at some scroll offset; if
/// the page scrolls before it sends a new rect, the preview follows the content by the scroll delta,
/// so it never slides over the fixed header or bottom bar. Main queue.
final class PreviewPlacer: NSObject {
  let view: CapturePreviewView
  private weak var host: UIView?
  private var css: CGRect
  private var baseOffset: CGPoint = .zero
  private var observations: [NSKeyValueObservation] = []
  var onNavigate: (() -> Void)?

  init(view: CapturePreviewView, host: UIView, css: CGRect) {
    self.view = view
    self.host = host
    self.css = css
    super.init()
    host.addSubview(view)
    if let wv = host as? WKWebView {
      baseOffset = wv.scrollView.contentOffset
      observations.append(wv.scrollView.observe(\.contentOffset, options: [.new]) { [weak self] _, _ in
        DispatchQueue.main.async { self?.layout() }
      })
      observations.append(wv.observe(\.url, options: [.new]) { [weak self] _, _ in
        DispatchQueue.main.async { self?.onNavigate?() }   // the page went away: release the camera
      })
    }
    layout()
  }

  static func frame(css: CGRect, scale: CGFloat, scrollDelta: CGPoint) -> CGRect {
    CGRect(x: css.minX * scale - scrollDelta.x, y: css.minY * scale - scrollDelta.y,
           width: css.width * scale, height: css.height * scale)
  }

  func update(css: CGRect) {
    self.css = css
    if let wv = host as? WKWebView { baseOffset = wv.scrollView.contentOffset }
    layout()
  }

  func layout() {
    guard let host else { return }
    var scale: CGFloat = 1, delta = CGPoint.zero
    if let wv = host as? WKWebView {
      scale = wv.pageZoom * wv.scrollView.zoomScale
      let off = wv.scrollView.contentOffset
      delta = CGPoint(x: off.x - baseOffset.x, y: off.y - baseOffset.y)
    }
    view.frame = PreviewPlacer.frame(css: css, scale: scale, scrollDelta: delta)
  }

  func remove() {
    observations.forEach { $0.invalidate() }
    observations.removeAll()
    view.removeFromSuperview()
  }
}
