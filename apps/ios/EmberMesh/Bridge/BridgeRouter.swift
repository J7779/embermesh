import Foundation

/// Forwards non-host RPC to the core. Completion receives the raw C-API envelope text.
protocol CoreCaller: AnyObject {
  func callCore(method: String, paramsJSON: String, completion: @escaping (String) -> Void)
}

/// Handles `host.*` (docs/RPC_API.md §3). Completion may be called on any queue.
protocol HostMethodHandler: AnyObject {
  func handleHost(method: String, params: JSONObject, completion: @escaping RPCCompletion)
}

extension NodeRunner: CoreCaller {
  func callCore(method: String, paramsJSON: String, completion: @escaping (String) -> Void) {
    callRaw(method, paramsJSON: paramsJSON, completion: completion)
  }
}

/// The native side of the WebView bridge (RPC_API §5):
///   JS → native: messageHandlers.ember.postMessage(JSON.stringify({id, method, params}))
///   native → JS: window.__emberBridge.onReply({id, ok, result?, error?}) / onEvent({event, data})
/// Pure logic: it produces JavaScript snippets through `evaluate`, which the WebView layer runs on
/// the main thread. Fully unit tested without a WebView.
final class BridgeRouter {
  private weak var core: CoreCaller?
  private weak var host: HostMethodHandler?
  /// Receives JavaScript source to evaluate in the page. Called on arbitrary queues.
  var evaluate: (String) -> Void
  /// The UI's `theme` setting whenever the page reads or changes settings (status bar style).
  var onTheme: ((String) -> Void)?

  /// Upper bound on a single request (files.send carries base64 file content).
  static let maxMessageBytes = 48 * 1024 * 1024

  init(core: CoreCaller?, host: HostMethodHandler?, evaluate: @escaping (String) -> Void) {
    self.core = core
    self.host = host
    self.evaluate = evaluate
  }

  /// Entry point for WKScriptMessage.body (a JSON string per spec; a dictionary is tolerated).
  func handle(body: Any) {
    let request: JSONObject
    if let s = body as? String {
      if s.utf8.count > BridgeRouter.maxMessageBytes {
        // We cannot know the id without parsing; extract it cheaply if possible.
        reply(id: BridgeRouter.sniffId(s), .failure(.tooLarge("request exceeds \(BridgeRouter.maxMessageBytes) bytes")))
        return
      }
      guard let o = JSON.parseObject(s) else {
        emberLog("bridge: dropping malformed message")
        return
      }
      request = o
    } else if let o = body as? JSONObject {
      request = o
    } else {
      emberLog("bridge: dropping message of unexpected type \(type(of: body))")
      return
    }

    let id = request["id"] ?? NSNull()
    guard let method = request.string("method"), !method.isEmpty else {
      if !(id is NSNull) { reply(id: id, .failure(.badParams("missing method"))) }
      return
    }
    let params: JSONObject
    if let p = request["params"] as? JSONObject { params = p }
    else if request["params"] == nil || request["params"] is NSNull { params = [:] }
    else { reply(id: id, .failure(.badParams("params must be an object"))); return }

    if method == "node.info" { LaunchClock.mark("first node.info from the UI") }
    if method == "conversations.list" || method == "guides.categories" { LaunchClock.mark("UI rendering (\(method))") }
    if method.hasPrefix("host.") {
      guard let host else { reply(id: id, .failure(.unsupported("no host router"))); return }
      host.handleHost(method: method, params: params) { [weak self] r in self?.reply(id: id, r) }
    } else {
      guard let core else { reply(id: id, .failure(.busy("node is not running"))); return }
      core.callCore(method: method, paramsJSON: JSON.encode(params)) { [weak self] envelope in
        if method == "settings.get" || method == "settings.set",
           let theme = (try? Envelope.parse(envelope).get())?.object("settings")?.string("theme") {
          self?.onTheme?(theme)
        }
        self?.replyEnvelope(id: id, envelope: envelope)
      }
    }
  }

  /// Push a core / host event ({"event":…,"data":…} JSON text) to the page.
  func sendEvent(json: String) {
    evaluate("window.__emberBridge&&window.__emberBridge.onEvent(\(JSON.jsSafe(json)))")
  }

  func sendEvent(name: String, data: JSONObject) {
    sendEvent(json: JSON.encode(["event": name, "data": data]))
  }

  // MARK: replies

  func reply(id: Any, _ r: RPCResult) {
    var o: JSONObject = ["id": id]
    switch r {
    case .success(let res): o["ok"] = true; o["result"] = res
    case .failure(let e): o["ok"] = false; o["error"] = e.json
    }
    evaluate("window.__emberBridge&&window.__emberBridge.onReply(\(JSON.jsSafe(JSON.encode(o))))")
  }

  /// The core envelope is already {"ok":…}; splice in the id instead of re-encoding (guide texts
  /// and file payloads can be large).
  func replyEnvelope(id: Any, envelope: String) {
    let trimmed = envelope.drop(while: { $0 == " " || $0 == "\n" || $0 == "\t" || $0 == "\r" })
    guard trimmed.first == "{" else {
      reply(id: id, .failure(.internalError("core returned invalid JSON")))
      return
    }
    let body = trimmed.dropFirst()
    let sep = body.drop(while: { $0 == " " || $0 == "\n" }).first == "}" ? "" : ","
    let text = "{\"id\":\(JSON.encode(id))\(sep)\(body)"
    evaluate("window.__emberBridge&&window.__emberBridge.onReply(\(JSON.jsSafe(text)))")
  }

  /// Best effort `"id":123` / `"id":"abc"` extraction from an oversized message.
  static func sniffId(_ s: String) -> Any {
    let head = String(s.prefix(256))
    guard let r = head.range(of: "\"id\"") else { return NSNull() }
    var rest = head[r.upperBound...].drop(while: { $0 == " " || $0 == ":" })
    if rest.first == "\"" {
      rest = rest.dropFirst()
      return String(rest.prefix(while: { $0 != "\"" }))
    }
    let num = rest.prefix(while: { $0.isNumber || $0 == "-" })
    if let n = Int(num) { return n }
    return NSNull()
  }
}
