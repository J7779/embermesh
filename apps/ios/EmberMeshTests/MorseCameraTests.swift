import AVFoundation
import WebKit
import XCTest
@testable import EmberMesh

/// A full-resolution 420f frame: luma `bg` everywhere except the given pixels.
enum BigFrames {
  static func make(width: Int, height: Int, bg: UInt8 = 30, pixels: [(Int, Int, UInt8)] = [], timeMs: Double) -> CMSampleBuffer {
    var pb: CVPixelBuffer?
    let attrs = [kCVPixelBufferIOSurfacePropertiesKey as String: [String: Any]()] as CFDictionary
    CVPixelBufferCreate(nil, width, height, kCVPixelFormatType_420YpCbCr8BiPlanarFullRange, attrs, &pb)
    let p = pb!
    CVPixelBufferLockBaseAddress(p, [])
    let base = CVPixelBufferGetBaseAddressOfPlane(p, 0)!.assumingMemoryBound(to: UInt8.self)
    let bpr = CVPixelBufferGetBytesPerRowOfPlane(p, 0)
    for y in 0..<height { memset(base + y * bpr, Int32(bg), width) }
    for (x, y, v) in pixels { base[y * bpr + x] = v }
    CVPixelBufferUnlockBaseAddress(p, [])
    var fd: CMVideoFormatDescription?
    CMVideoFormatDescriptionCreateForImageBuffer(allocator: nil, imageBuffer: p, formatDescriptionOut: &fd)
    var timing = CMSampleTimingInfo(duration: CMTime(value: 1, timescale: 30),
                                    presentationTimeStamp: CMTime(seconds: timeMs / 1000, preferredTimescale: 1_000_000), decodeTimeStamp: .invalid)
    var sb: CMSampleBuffer?
    CMSampleBufferCreateReadyWithImageBuffer(allocator: nil, imageBuffer: p, formatDescription: fd!, sampleTiming: &timing, sampleBufferOut: &sb)
    return sb!
  }

  static func grid(_ f: JSONObject, _ key: String = "max_b64") -> [UInt8] { [UInt8](Data(base64Encoded: f.string(key) ?? "") ?? Data()) }
}

final class MorseCameraGridTests: XCTestCase {
  func feedOne(_ sb: CMSampleBuffer, orientation: GridOrientation) -> JSONObject? {
    let sink = RecordingSink()
    let cam = CameraLumaCapture(session: "g", sink: sink)
    cam.orientation = orientation
    cam.queue.sync { cam.process(sb); cam.grids.flush() }
    return (sink.feeds.first?["frames"] as? [JSONObject])?.first
  }

  /// One bright pixel in a 1920×1080 frame lands in exactly one cell of the max grid, at the cell
  /// that covers it — for the landscape sensor grid and after turning it to portrait.
  func testOneBrightPixelDecidesExactlyOneCell() throws {
    let (px, py) = (1000, 300)
    let sb = BigFrames.make(width: 1920, height: 1080, bg: 30, pixels: [(px, py, 250)], timeMs: 5)
    // Sensor orientation (landscape right): 64×36, cell (1000·64/1920, 300·36/1080) = (33, 10).
    let f0 = try XCTUnwrap(feedOne(sb, orientation: GridOrientation(quarterTurnsCW: 0, mirrored: false)))
    XCTAssertEqual(f0.int("w"), 64); XCTAssertEqual(f0.int("h"), 36)
    let g0 = BigFrames.grid(f0)
    XCTAssertEqual(g0.filter { $0 == 250 }.count, 1, "one cell holds the light")
    XCTAssertEqual(g0.firstIndex(of: 250), 10 * 64 + 33)
    XCTAssertEqual(Set(g0.filter { $0 != 250 }), [30])
    let mean0 = BigFrames.grid(f0, "mean_b64")
    XCTAssertEqual(mean0[10 * 64 + 33], 30, "the mean barely moves: a one-pixel light only shows in max")
    // Portrait (rotated 90° clockwise): 36×64, (x, y) → (35 − 10, 33) = (25, 33).
    let f1 = try XCTUnwrap(feedOne(sb, orientation: GridOrientation(quarterTurnsCW: 1, mirrored: false)))
    XCTAssertEqual(f1.int("w"), 36); XCTAssertEqual(f1.int("h"), 64)
    let g1 = BigFrames.grid(f1)
    XCTAssertEqual(g1.filter { $0 == 250 }.count, 1)
    XCTAssertEqual(g1.firstIndex(of: 250), 33 * 36 + 25)
    // Mirrored (front camera): column 25 → 10.
    let f2 = try XCTUnwrap(feedOne(sb, orientation: GridOrientation(quarterTurnsCW: 1, mirrored: true)))
    XCTAssertEqual(BigFrames.grid(f2).firstIndex(of: 250), 33 * 36 + (35 - 25))
    // A light on the very last pixel.
    let edge = BigFrames.make(width: 1920, height: 1080, pixels: [(1919, 1079, 255)], timeMs: 6)
    let fe = try XCTUnwrap(feedOne(edge, orientation: GridOrientation(quarterTurnsCW: 0, mirrored: false)))
    XCTAssertEqual(BigFrames.grid(fe).firstIndex(of: 255), 36 * 64 - 1)
  }

  func testOrientationTable() {
    XCTAssertEqual(GridOrientation.forInterface(.portrait, front: false), GridOrientation(quarterTurnsCW: 1, mirrored: false))
    XCTAssertEqual(GridOrientation.forInterface(.landscapeRight, front: false).quarterTurnsCW, 0)
    XCTAssertEqual(GridOrientation.forInterface(.landscapeLeft, front: false).quarterTurnsCW, 2)
    XCTAssertEqual(GridOrientation.forInterface(.portraitUpsideDown, front: false).quarterTurnsCW, 3)
    XCTAssertTrue(GridOrientation.forInterface(.portrait, front: true).mirrored)
    XCTAssertEqual(LumaGrid.dims(width: 1920, height: 1080).0 * LumaGrid.dims(width: 1920, height: 1080).1, 64 * 36)
    XCTAssertEqual(LumaGrid.dims(width: 4032, height: 3024).1, 48)
    XCTAssertLessThanOrEqual(LumaGrid.dims(width: 640, height: 640).0 * LumaGrid.dims(width: 640, height: 640).1, 4096)
  }

  /// Per-frame cost of the full-resolution reduction (1920×1080 → 64×36 max + mean, rotate, base64).
  func testPerFrameCost() {
    let sink = RecordingSink()
    let cam = CameraLumaCapture(session: "cost", sink: sink)
    let frames = (0..<60).map { BigFrames.make(width: 1920, height: 1080, pixels: [(Int.random(in: 0..<1920), Int.random(in: 0..<1080), 255)], timeMs: Double($0) * 33.3) }
    let t0 = Date()
    for sb in frames { cam.queue.sync { cam.process(sb) } }
    let total = Date().timeIntervalSince(t0) * 1000 / Double(frames.count)
    let reduce = cam.queue.sync { cam.reduceMs }
    let mean = reduce.reduce(0, +) / Double(max(1, reduce.count))
    let line = String(format: "morse grid cost (simulator, 1920×1080): reduce+orient+base64 mean %.2f ms, max %.2f ms; whole process() %.2f ms per frame",
                      mean, reduce.max() ?? 0, total)
    print(line)
    add(XCTAttachment(string: line))
    XCTAssertLessThan(mean, 33, "well within a 30 fps frame interval even here")
  }

  /// When the core is busy (a feed still in flight), frames beyond one batch of 8 are dropped, not queued.
  func testDropsFramesWhileCoreBusy() {
    final class SlowCore: MorseFeedSink {
      var calls = 0
      var pending: [(RPCResult) -> Void] = []
      func morseFeed(_ p: JSONObject, completion: @escaping (RPCResult) -> Void) { calls += 1; pending.append(completion) }
    }
    let core = SlowCore()
    let cam = CameraLumaCapture(session: "busy", sink: core)
    for i in 0..<40 { let sb = BigFrames.make(width: 640, height: 480, timeMs: Double(i) * 33); cam.queue.sync { cam.process(sb) } }
    XCTAssertEqual(core.calls, 1, "one feed in flight")
    XCTAssertGreaterThan(cam.grids.dropped, 20)
    core.pending.first?(.success(["target": ["state": "searching"]]))
    cam.queue.sync {}
    XCTAssertEqual(core.calls, 2, "the waiting batch goes as soon as the core answered")
  }
}

/// The native preview lands exactly on the page's rect, in window coordinates, for two window
/// sizes / orientations and with the page scrolled; it follows scrolling, moves on update, and is
/// removed at once on stop and on background.
final class MorsePreviewPlacementTests: XCTestCase {
  /// The page's own measurement of its camera panel (getBoundingClientRect, CSS px of the viewport)
  /// — exactly what the UI passes as `rect`.
  static func pageRect(_ wv: WKWebView) -> CGRect? {
    var out: CGRect?
    var done = false
    wv.evaluateJavaScript("(function(){var r=document.getElementById('cam').getBoundingClientRect();return [r.x,r.y,r.width,r.height]})()") { v, _ in
      if let a = v as? [Double], a.count == 4 { out = CGRect(x: a[0], y: a[1], width: a[2], height: a[3]) }
      done = true
    }
    spin(until: 5) { done }
    return out
  }

  static func scroll(_ wv: WKWebView, to y: CGFloat) {
    var done = false
    wv.evaluateJavaScript("window.scrollTo(0, \(y)); 1") { _, _ in done = true }
    spin(until: 5) { done && abs(wv.scrollView.contentOffset.y - y) < 0.5 }
    spin(until: 0.3) { false }
  }

  func run(window size: CGSize, webFrame: CGRect, scrollY: CGFloat, css: CGRect, file: StaticString = #filePath, line: UInt = #line) throws {
    let scene = try XCTUnwrap(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
    let window = UIWindow(windowScene: scene)
    window.frame = CGRect(origin: .zero, size: size)
    let root = UIViewController()
    window.rootViewController = root
    window.isHidden = false
    defer { window.isHidden = true }
    // Configured like the app's web view: inside the safe area, no automatic content insets.
    let wv = WKWebView(frame: webFrame)
    wv.scrollView.contentInsetAdjustmentBehavior = .never
    root.view.addSubview(wv)
    final class Nav: NSObject, WKNavigationDelegate { var done = false; func webView(_ w: WKWebView, didFinish n: WKNavigation!) { done = true } }
    let nav = Nav(); wv.navigationDelegate = nav
    // An in-flow panel (it moves when the page scrolls), placed so that at scrollY it is at `css`.
    wv.loadHTMLString("""
      <meta name=viewport content='width=device-width,initial-scale=1,viewport-fit=cover'>
      <body style='margin:0;height:4000px;position:relative'>
      <div id=cam style='position:absolute;left:\(css.minX)px;top:\(css.minY + scrollY)px;width:\(css.width)px;height:\(css.height)px'></div></body>
      """, baseURL: nil)
    XCTAssertTrue(spin(until: 10) { nav.done }, file: file, line: line)
    MorsePreviewPlacementTests.scroll(wv, to: scrollY)
    let r0 = try XCTUnwrap(MorsePreviewPlacementTests.pageRect(wv), file: file, line: line)
    XCTAssertEqual(r0, css, "the page measures its panel where we put it", file: file, line: line)

    let sink = RecordingSink()   // CaptureService holds its sink weakly (the node runner in the app)
    let svc = CaptureService(sink: sink)
    svc.simulateCamera = true
    svc.previewHost = { wv }
    var started: RPCResult?
    svc.start(["kind": "camera_luma", "session": "p", "rect": ["x": r0.minX, "y": r0.minY, "w": r0.width, "h": r0.height]]) { started = $0 }
    XCTAssertTrue(spin(until: 5) { started != nil }, file: file, line: line)
    XCTAssertNotNil((try? started?.get())?["zoom_steps"], file: file, line: line)
    let placer = try XCTUnwrap(svc.preview, file: file, line: line)
    func onScreen() -> CGRect { placer.view.convert(placer.view.bounds, to: nil) }
    XCTAssertEqual(onScreen(), wv.convert(r0, to: nil), "preview frame in window coordinates", file: file, line: line)
    print("preview placement \(size): page rect \(r0) → window \(onScreen())")
    XCTAssertTrue(placer.view.clipsToBounds)
    XCTAssertTrue(placer.view.superview === wv)
    XCTAssertFalse(placer.view.isUserInteractionEnabled, "taps reach the page")

    // The page scrolls by 60 px before it sends a new rect: the preview follows the panel.
    MorsePreviewPlacementTests.scroll(wv, to: scrollY + 60)
    let r1 = try XCTUnwrap(MorsePreviewPlacementTests.pageRect(wv), file: file, line: line)
    XCTAssertEqual(r1.minY, r0.minY - 60, accuracy: 0.5, file: file, line: line)
    XCTAssertEqual(onScreen(), wv.convert(r1, to: nil), "follows the panel while the page scrolls", file: file, line: line)
    // host.capture.update with the rect measured now: same place, base reset.
    svc.update(["kind": "camera_luma", "rect": ["x": r1.minX, "y": r1.minY, "w": r1.width, "h": r1.height]]) { _ in }
    XCTAssertEqual(onScreen(), wv.convert(r1, to: nil), file: file, line: line)
    // Stop: removed at once.
    svc.stop(["kind": "camera_luma"]) { _ in }
    XCTAssertNil(placer.view.superview, file: file, line: line)
    XCTAssertNil(svc.camera)
    // Background releases a running camera and its preview too.
    svc.start(["kind": "camera_luma", "session": "p2", "rect": ["x": 0, "y": 0, "w": 100, "h": 100]]) { _ in }
    XCTAssertTrue(spin(until: 5) { svc.preview != nil }, file: file, line: line)
    let v2 = svc.preview!.view
    NotificationCenter.default.post(name: UIApplication.didEnterBackgroundNotification, object: nil)
    XCTAssertNil(v2.superview, file: file, line: line)
    XCTAssertNil(svc.camera, file: file, line: line)
  }

  func testPhonePortraitScrolled() throws {
    // iPhone-like portrait: web view inside the safe area (status bar 59, home indicator 34).
    try run(window: CGSize(width: 393, height: 852), webFrame: CGRect(x: 0, y: 59, width: 393, height: 852 - 59 - 34),
            scrollY: 240, css: CGRect(x: 16, y: 96, width: 361, height: 270))
  }

  func testPhoneLandscapeWithSideInsets() throws {
    // Landscape: notch side insets 59 left/right, home indicator 21.
    try run(window: CGSize(width: 852, height: 393), webFrame: CGRect(x: 59, y: 0, width: 852 - 118, height: 393 - 21),
            scrollY: 80, css: CGRect(x: 12, y: 60, width: 420, height: 236))
  }

  func testIPadSizePortrait() throws {
    try run(window: CGSize(width: 820, height: 1180), webFrame: CGRect(x: 0, y: 24, width: 820, height: 1180 - 24 - 20),
            scrollY: 0, css: CGRect(x: 200, y: 140, width: 420, height: 315))
  }
}

/// The marker ring the host draws on its native preview (RPC_API §12).
final class MorsePreviewRingTests: XCTestCase {
  func testRingPositionSizeAndVisibilityPerState() {
    let v = CapturePreviewView(session: nil)
    v.frame = CGRect(x: 0, y: 0, width: 360, height: 270)
    v.layoutIfNeeded()
    XCTAssertEqual(v.ringOpacity, 0, "nothing before a target")
    let portrait = 9.0 / 16.0   // frame as shown: 9:16, aspect-filled into 360×270 (cropped top/bottom)
    v.setTarget(["state": "locked", "x": 0.5, "y": 0.5, "size": 0.1, "confidence": 0.9], aspect: portrait)
    XCTAssertEqual(v.ringOpacity, 1, "shown while locked")
    XCTAssertEqual(v.ringFrame.midX, 180, accuracy: 0.01); XCTAssertEqual(v.ringFrame.midY, 135, accuracy: 0.01)
    XCTAssertEqual(v.ringFrame.width, 36, accuracy: 0.01, "size × preview width")
    // Off-centre: through the crop (frame shown 360×640, offset −185 vertically).
    v.setTarget(["state": "locked", "x": 0.25, "y": 0.4, "size": 0.01], aspect: portrait)
    XCTAssertEqual(v.ringFrame.midX, 90, accuracy: 0.01)
    XCTAssertEqual(v.ringFrame.midY, -185 + 0.4 * 640, accuracy: 0.01)
    XCTAssertEqual(v.ringFrame.width, 28, accuracy: 0.01, "at least 28 pt")
    v.setTarget(["state": "lost", "x": 0.25, "y": 0.4, "size": 0.01], aspect: portrait)
    XCTAssertEqual(v.ringOpacity, 0.35, accuracy: 0.001, "dimmed while lost")
    v.setTarget(["state": "searching", "candidates": 0], aspect: portrait)
    XCTAssertEqual(v.ringOpacity, 0, "absent while searching")
    // Landscape frame in a landscape preview: no crop, straight mapping.
    let w = CapturePreviewView(session: nil)
    w.frame = CGRect(x: 0, y: 0, width: 640, height: 360)
    w.setTarget(["state": "locked", "x": 0.75, "y": 0.25, "size": 0.2], aspect: 16.0 / 9.0)
    XCTAssertEqual(w.ringFrame.midX, 480, accuracy: 0.01); XCTAssertEqual(w.ringFrame.midY, 90, accuracy: 0.01)
    XCTAssertEqual(w.ringFrame.width, 128, accuracy: 0.01)
    XCTAssertFalse(w.isUserInteractionEnabled, "taps still go to the page")
  }

  /// The ring follows the core's replies through the real capture path.
  func testRingFollowsFeedReplies() throws {
    final class TrackerCore: MorseFeedSink {
      func morseFeed(_ p: JSONObject, completion: @escaping (RPCResult) -> Void) {
        completion(.success(["target": ["state": "locked", "x": 0.5, "y": 0.5, "size": 0.05, "confidence": 0.8]]))
      }
    }
    let core = TrackerCore()
    let wv = UIView(frame: CGRect(x: 0, y: 0, width: 393, height: 700))
    let svc = CaptureService(sink: core)
    svc.simulateCamera = true
    svc.previewHost = { wv }
    svc.coreFeatures = { ["morse_frames"] }
    var started = false
    svc.start(["kind": "camera_luma", "session": "r", "rect": ["x": 16, "y": 100, "w": 360, "h": 270]]) { _ in started = true }
    XCTAssertTrue(spin(until: 5) { started && svc.preview != nil })
    let cam = try XCTUnwrap(svc.camera)
    XCTAssertEqual(cam.mode, .frames)
    for i in 0..<10 { let sb = BigFrames.make(width: 640, height: 480, timeMs: Double(i) * 33); cam.queue.sync { cam.process(sb) } }
    cam.queue.sync { cam.grids.flush() }
    let view = try XCTUnwrap(svc.preview?.view)
    XCTAssertTrue(spin(until: 5) { view.ringOpacity == 1 }, "ring shown once the core reports locked")
    XCTAssertEqual(view.ringFrame.midX, 180, accuracy: 0.5)
    svc.stop(["kind": "camera_luma"]) { _ in }
  }

  /// node.info.features decides the feed first; a core without "morse_frames" gets levels from the start.
  func testCoreFeatureCheckChoosesTheFeed() throws {
    for (features, expected) in [(["morse_frames"], CameraLumaCapture.Mode.frames), (["other"], .levels)] {
      let sink = RecordingSink()
      let svc = CaptureService(sink: sink)
      svc.simulateCamera = true
      svc.coreFeatures = { features }
      var started = false
      svc.start(["kind": "camera_luma", "session": "f"]) { _ in started = true }
      XCTAssertTrue(spin(until: 5) { started })
      XCTAssertEqual(try XCTUnwrap(svc.camera).mode, expected, "\(features)")
      svc.stop(["kind": "camera_luma"]) { _ in }
    }
  }
}
