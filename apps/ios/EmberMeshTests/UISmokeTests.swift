import WebKit
import XCTest
@testable import EmberMesh

/// Drives the REAL shared UI (ui/ bundled as www/) in the app's own WKWebView, talking to the app's
/// real node through the bridge: first-run setup → send a message in Public → search the guide
/// library → open a guide. Screenshots of each step are written to $EMBER_SHOT_DIR.
///
/// It uses the app's actual storage, so it needs a fresh install (first run) and is opt-in:
///   xcrun simctl uninstall booted org.embermesh.app
///   TEST_RUNNER_EMBER_SHOT_DIR=/path xcodebuild … test -only-testing:EmberMeshTests/UISmokeTests
final class UISmokeTests: XCTestCase {
  var shotDir: URL!
  var webView: WKWebView!

  override func setUpWithError() throws {
    guard let d = ProcessInfo.processInfo.environment["EMBER_SHOT_DIR"], !d.isEmpty else {
      throw XCTSkip("set TEST_RUNNER_EMBER_SHOT_DIR (and start from a fresh install) to run the UI smoke test")
    }
    shotDir = URL(fileURLWithPath: d, isDirectory: true)
    try FileManager.default.createDirectory(at: shotDir, withIntermediateDirectories: true)
  }

  static func findWebView(_ v: UIView) -> WKWebView? {
    if let w = v as? WKWebView { return w }
    for s in v.subviews { if let w = findWebView(s) { return w } }
    return nil
  }

  var window: UIWindow? {
    UIApplication.shared.connectedScenes.compactMap { ($0 as? UIWindowScene)?.windows.first { $0.isKeyWindow } ?? ($0 as? UIWindowScene)?.windows.first }.first
  }

  /// Runs an async JS function body in the page; returns its JSON-able result.
  func js(_ body: String, timeout: TimeInterval = 20, file: StaticString = #filePath, line: UInt = #line) -> Any? {
    var out: Any?
    var done = false
    webView.callAsyncJavaScript(body, arguments: [:], in: nil, in: .page) { r in
      switch r {
      case .success(let v): out = v
      case .failure(let e): XCTFail("JS failed: \(e)\n\(body)", file: file, line: line)
      }
      done = true
    }
    spin(until: timeout) { done }
    if !done { XCTFail("JS timed out: \(body)", file: file, line: line) }
    return out
  }

  /// Polls until `expr` (a JS expression) is truthy.
  func waitJS(_ expr: String, timeout: Double = 20, file: StaticString = #filePath, line: UInt = #line) -> Bool {
    let r = js("""
      const end = Date.now() + \(Int(timeout * 1000));
      while (Date.now() < end) { try { if (\(expr)) return true; } catch (e) {} await new Promise(r => setTimeout(r, 100)); }
      return false;
      """, timeout: timeout + 5, file: file, line: line) as? Bool ?? false
    if !r { XCTFail("timed out waiting for \(expr)", file: file, line: line) }
    return r
  }

  func shot(_ name: String) {
    guard let w = window else { return }
    spin(until: 0.6) { false }  // let the page paint
    let img = UIGraphicsImageRenderer(bounds: w.bounds).image { _ in w.drawHierarchy(in: w.bounds, afterScreenUpdates: true) }
    let url = shotDir.appendingPathComponent("\(name).png")
    try? img.pngData()?.write(to: url)
    print("screenshot \(url.path)")
  }

  func testFirstRunMessageAndGuide() throws {
    let env = try XCTUnwrap(AppEnvironment.shared)
    XCTAssertTrue(env.content.hasUI, "ui/ bundled")
    XCTAssertTrue(spin(until: 20) { self.window.flatMap { UISmokeTests.findWebView($0) } != nil })
    webView = window.flatMap { UISmokeTests.findWebView($0) }
    XCTAssertTrue(spin(until: 30) { self.webView.url?.absoluteString == "embermesh://app/index.html" && !self.webView.isLoading })

    // 1. First run: the core reports name "" → setup screen.
    guard case .success(let info) = env.runner.callSync("node.info") else { return XCTFail("node.info") }
    XCTAssertEqual(info.string("name"), "", "needs a fresh install (xcrun simctl uninstall booted org.embermesh.app)")
    XCTAssertTrue(waitJS("document.querySelector('#setup-name')"))
    shot("01-setup")
    _ = js("""
      const n = document.querySelector('#setup-name'); n.value = 'Sam (iPhone sim)'; n.dispatchEvent(new Event('input', {bubbles:true}));
      [...document.querySelectorAll('button')].find(b => /Continue/.test(b.textContent)).click(); return true;
      """)
    XCTAssertTrue(waitJS("[...document.querySelectorAll('button')].some(b => /Start using EmberMesh/.test(b.textContent))"))
    shot("02-identity")
    guard case .success(let after) = env.runner.callSync("node.info") else { return XCTFail("node.info") }
    XCTAssertEqual(after.string("name"), "Sam (iPhone sim)")
    _ = js("[...document.querySelectorAll('button')].find(b => /Start using EmberMesh/.test(b.textContent)).click(); return true;")
    XCTAssertTrue(waitJS("location.hash.startsWith('#/chats') && !document.querySelector('#setup-name')"))
    shot("03-chats")

    // 2. Send a message in Public through the composer.
    _ = js("location.hash = '#/chats/' + encodeURIComponent('ch:public'); return true;")
    XCTAssertTrue(waitJS("document.querySelector('textarea[aria-label=\"Message text\"]')"))
    let text = "Hello from the iOS simulator \(Int(Date().timeIntervalSince1970) % 100000)"
    _ = js("""
      const ta = [...document.querySelectorAll('textarea[aria-label="Message text"]')].pop();
      ta.value = \(JSON.encode(text)); ta.dispatchEvent(new Event('input', {bubbles:true}));
      [...document.querySelectorAll('button[aria-label="Send"]')].pop().click(); return true;
      """)
    XCTAssertTrue(waitJS("document.body.innerText.includes(\(JSON.encode(text)))"), "sent message shown")
    guard case .success(let list) = env.runner.callSync("messages.list", ["conv": "ch:public"]) else { return XCTFail("messages.list") }
    XCTAssertTrue(((list["messages"] as? [JSONObject]) ?? []).contains { $0.string("text") == text }, "stored by the core")
    _ = js("document.activeElement && document.activeElement.blur(); return true;")
    shot("04-message-sent")

    // 3. Guide library: search, open the first hit.
    _ = js("location.hash = '#/guides'; return true;")
    XCTAssertTrue(waitJS("document.querySelector('input[type=search]') && document.querySelectorAll('.cat-grid a').length >= 20"),
                  "20 categories from the memory-mapped guides.pack")
    _ = js("""
      const i = document.querySelector('input[type=search]'); i.value = 'purify water'; i.dispatchEvent(new Event('input', {bubbles:true})); return true;
      """)
    XCTAssertTrue(waitJS("document.querySelectorAll('a.item[href^=\"#/guides/read/\"]').length > 0"))
    _ = js("document.activeElement && document.activeElement.blur(); return true;")
    shot("05-guide-search")
    let hits = js("return document.querySelectorAll('a.item[href^=\"#/guides/read/\"]').length;") as? Int ?? 0
    _ = js("document.querySelector('a.item[href^=\"#/guides/read/\"]').click(); return true;")
    XCTAssertTrue(waitJS("location.hash.startsWith('#/guides/read/') && document.body.innerText.length > 800"))
    shot("06-guide-open")
    let title = js("return (document.querySelector('h1') || {}).textContent || '';") as? String ?? ""
    print("ui smoke: \(hits) search hits; opened guide \"\(title)\"")
  }
}
