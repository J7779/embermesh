import Foundation
import WebKit

/// Locates the bundled web content.
///   <bundle>/www/                 copy of ui/ (build phase bundle-resources.sh)
///   <bundle>/www-diag/            our diagnostic page (bridge-test.html + module test)
///   <bundle>/guides.pack          optional offline library
///   <bundle>/hubweb/              copy of dist/web (WASM web app) served to hub guests, if it was built
struct WebContent {
  let uiRoot: URL
  let diagRoot: URL
  let guidesPack: URL?
  var hubWebRoot: URL? = nil

  /// Web root for the phone hub: dist/web when bundled (guests get a full WASM node), otherwise
  /// the host library's built-in page (nil).
  var hubRoot: URL? {
    guard let r = hubWebRoot, FileManager.default.fileExists(atPath: r.appendingPathComponent("index.html").path) else { return nil }
    return r
  }
  /// Single-file app offered at /download/ (to pass the app on).
  var hubDownloads: [URL] { hubRoot.map { [$0.appendingPathComponent("embermesh.html")] } ?? [] }

  static func bundled(_ bundle: Bundle = .main) -> WebContent {
    let res = bundle.resourceURL ?? URL(fileURLWithPath: "/")
    let pack = res.appendingPathComponent("guides.pack")
    return WebContent(uiRoot: res.appendingPathComponent("www", isDirectory: true),
                      diagRoot: res.appendingPathComponent("www-diag", isDirectory: true),
                      guidesPack: FileManager.default.fileExists(atPath: pack.path) ? pack : nil,
                      hubWebRoot: res.appendingPathComponent("hubweb", isDirectory: true))
  }

  var hasUI: Bool { FileManager.default.fileExists(atPath: uiRoot.appendingPathComponent("index.html").path) }
}

/// Serves the bundled UI at embermesh://app/… so ES modules, fetch() of the guide pack and a stable
/// secure-context origin work fully offline. file:// URLs would break module loading and fetch.
final class AppSchemeHandler: NSObject, WKURLSchemeHandler {
  static let scheme = "embermesh"
  static let host = "app"
  static var origin: String { "\(scheme)://\(host)" }
  static let diagPrefix = "/__diag/"
  static let diagPage = "bridge-test.html"

  let content: WebContent
  private let pack: MappedFile?
  /// Paths answered 404 recently (main thread). WebKit gives navigation delegates a plain
  /// NSURLResponse without the status for custom schemes, so the shell asks here instead.
  private(set) var recentlyNotFound: [String] = []
  func wasNotFound(_ url: URL?) -> Bool { url.map { recentlyNotFound.contains($0.path) } ?? false }

  init(content: WebContent, pack: MappedFile? = nil) {
    self.content = content
    self.pack = pack ?? content.guidesPack.flatMap { MappedFile(url: $0) }
  }

  /// Start URL: the real UI if bundled, otherwise the diagnostic page.
  func startURL(forceDiagnostics: Bool = false) -> URL {
    if forceDiagnostics || !content.hasUI {
      return URL(string: "\(AppSchemeHandler.origin)\(AppSchemeHandler.diagPrefix)\(AppSchemeHandler.diagPage)")!
    }
    return URL(string: "\(AppSchemeHandler.origin)/index.html")!
  }

  enum Resolved: Equatable {
    case file(URL)
    case guidesPack
    case notFound
  }

  /// Maps a request path to a bundled file. Rejects traversal outside the roots.
  func resolve(path rawPath: String) -> Resolved {
    var path = rawPath.removingPercentEncoding ?? rawPath
    if path.isEmpty || path == "/" { path = "/index.html" }
    if path.contains("\0") { return .notFound }
    if path == "/guides.pack" || path == "/guides/guides.pack" { return pack != nil ? .guidesPack : .notFound }

    var root = content.uiRoot
    var rel = String(path.dropFirst())
    if path.hasPrefix(AppSchemeHandler.diagPrefix) {
      root = content.diagRoot
      rel = String(path.dropFirst(AppSchemeHandler.diagPrefix.count))
    } else if path == "/index.html" && !content.hasUI {
      root = content.diagRoot
      rel = AppSchemeHandler.diagPage
    }
    let comps = rel.split(separator: "/", omittingEmptySubsequences: true)
    if comps.contains(where: { $0 == ".." || $0 == "." }) { return .notFound }
    var url = root
    for c in comps { url.appendPathComponent(String(c)) }
    var isDir: ObjCBool = false
    if FileManager.default.fileExists(atPath: url.path, isDirectory: &isDir), isDir.boolValue {
      url.appendPathComponent("index.html")
    }
    let rootPath = root.standardizedFileURL.resolvingSymlinksInPath().path
    let filePath = url.standardizedFileURL.resolvingSymlinksInPath().path
    guard filePath.hasPrefix(rootPath + "/"), FileManager.default.isReadableFile(atPath: filePath) else { return .notFound }
    return .file(url)
  }

  static func mimeType(for ext: String) -> String {
    switch ext.lowercased() {
    case "html", "htm": return "text/html; charset=utf-8"
    case "js", "mjs": return "text/javascript; charset=utf-8"
    case "css": return "text/css; charset=utf-8"
    case "json", "map": return "application/json; charset=utf-8"
    case "webmanifest": return "application/manifest+json"
    case "svg": return "image/svg+xml"
    case "png": return "image/png"
    case "jpg", "jpeg": return "image/jpeg"
    case "gif": return "image/gif"
    case "webp": return "image/webp"
    case "ico": return "image/x-icon"
    case "woff2": return "font/woff2"
    case "woff": return "font/woff"
    case "ttf": return "font/ttf"
    case "wasm": return "application/wasm"
    case "txt", "md": return "text/plain; charset=utf-8"
    case "wav": return "audio/wav"
    default: return "application/octet-stream"
    }
  }

  // MARK: WKURLSchemeHandler

  func webView(_ webView: WKWebView, start task: WKURLSchemeTask) {
    guard let url = task.request.url, url.host == AppSchemeHandler.host else {
      respond(task, status: 404, mime: "text/plain", body: Data("not found".utf8))
      return
    }
    switch resolve(path: url.path) {
    case .file(let file):
      guard let data = try? Data(contentsOf: file, options: .mappedIfSafe) else {
        respond(task, status: 500, mime: "text/plain", body: Data("read error".utf8))
        return
      }
      respond(task, status: 200, mime: AppSchemeHandler.mimeType(for: file.pathExtension), body: data)
    case .guidesPack:
      respond(task, status: 200, mime: "application/octet-stream", body: pack?.data() ?? Data())
    case .notFound:
      recentlyNotFound.append(url.path)
      if recentlyNotFound.count > 32 { recentlyNotFound.removeFirst() }
      respond(task, status: 404, mime: "text/plain", body: Data("not found: \(url.path)".utf8))
    }
  }

  func webView(_ webView: WKWebView, stop task: WKURLSchemeTask) {
    // Responses are delivered synchronously in start(), so there is nothing in flight to cancel.
  }

  private func respond(_ task: WKURLSchemeTask, status: Int, mime: String, body: Data) {
    let headers = [
      "Content-Type": mime,
      "Content-Length": String(body.count),
      "Cache-Control": "no-cache",
      "Access-Control-Allow-Origin": AppSchemeHandler.origin,
      "X-Content-Type-Options": "nosniff",
    ]
    let resp = HTTPURLResponse(url: task.request.url ?? URL(string: AppSchemeHandler.origin)!,
                               statusCode: status, httpVersion: "HTTP/1.1", headerFields: headers)!
    task.didReceive(resp)
    if task.request.httpMethod != "HEAD" { task.didReceive(body) }
    task.didFinish()
  }
}
