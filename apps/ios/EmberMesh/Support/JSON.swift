import Foundation

typealias JSONObject = [String: Any]

/// Small helpers around JSONSerialization. The app moves JSON text between the WebView and the core
/// and only parses what the host router itself needs.
enum JSON {
  static func encode(_ value: Any, sorted: Bool = false) -> String {
    var opts: JSONSerialization.WritingOptions = [.fragmentsAllowed]
    if sorted { opts.insert(.sortedKeys) }
    guard JSONSerialization.isValidJSONObject([value]),
          let data = try? JSONSerialization.data(withJSONObject: value, options: opts),
          let s = String(data: data, encoding: .utf8) else { return "null" }
    return s
  }

  static func encodeData(_ value: Any) -> Data? {
    guard JSONSerialization.isValidJSONObject(value) else { return nil }
    return try? JSONSerialization.data(withJSONObject: value, options: [.sortedKeys])
  }

  static func parse(_ text: String) -> Any? {
    guard let data = text.data(using: .utf8) else { return nil }
    return try? JSONSerialization.jsonObject(with: data, options: [.fragmentsAllowed])
  }

  static func parseObject(_ text: String) -> JSONObject? { parse(text) as? JSONObject }

  /// JSON text is valid JavaScript except for raw U+2028/U+2029 in strings on older engines.
  static func jsSafe(_ json: String) -> String {
    guard json.contains("\u{2028}") || json.contains("\u{2029}") else { return json }
    return json.replacingOccurrences(of: "\u{2028}", with: "\\u2028")
      .replacingOccurrences(of: "\u{2029}", with: "\\u2029")
  }
}

extension Dictionary where Key == String, Value == Any {
  func string(_ k: String) -> String? {
    if let s = self[k] as? String { return s }
    return nil
  }
  func number(_ k: String) -> Double? {
    if let n = self[k] as? NSNumber {
      if CFGetTypeID(n) == CFBooleanGetTypeID() { return nil }
      return n.doubleValue
    }
    if let s = self[k] as? String, let d = Double(s) { return d }
    return nil
  }
  func int(_ k: String) -> Int? {
    guard let d = number(k), d.isFinite, abs(d) < 9.0e15 else { return nil }
    return Int(d)
  }
  func bool(_ k: String) -> Bool? {
    if let n = self[k] as? NSNumber { return n.boolValue }
    if let s = self[k] as? String { return s == "true" || s == "1" }
    return nil
  }
  func object(_ k: String) -> JSONObject? { self[k] as? JSONObject }
  func array(_ k: String) -> [Any]? { self[k] as? [Any] }
}

/// RPC error as defined in docs/RPC_API.md §1.
struct RPCError: Error, Equatable, CustomStringConvertible {
  let code: String
  let message: String
  init(_ code: String, _ message: String) { self.code = code; self.message = message }

  static func badParams(_ m: String) -> RPCError { RPCError("bad_params", m) }
  static func notFound(_ m: String) -> RPCError { RPCError("not_found", m) }
  static func unsupported(_ m: String) -> RPCError { RPCError("unsupported", m) }
  static func busy(_ m: String) -> RPCError { RPCError("busy", m) }
  static func denied(_ m: String) -> RPCError { RPCError("denied", m) }
  static func tooLarge(_ m: String) -> RPCError { RPCError("too_large", m) }
  static func io(_ m: String) -> RPCError { RPCError("io_error", m) }
  static func internalError(_ m: String) -> RPCError { RPCError("internal", m) }

  var json: JSONObject { ["code": code, "message": message] }
  var description: String { "\(code): \(message)" }
}

typealias RPCResult = Result<JSONObject, RPCError>
typealias RPCCompletion = (RPCResult) -> Void

/// The C API envelope: {"ok":true,"result":{…}} | {"ok":false,"error":{code,message}}.
enum Envelope {
  static func parse(_ text: String) -> RPCResult {
    guard let obj = JSON.parseObject(text) else { return .failure(.internalError("core returned invalid JSON")) }
    if obj.bool("ok") == true { return .success(obj.object("result") ?? [:]) }
    let e = obj.object("error") ?? [:]
    return .failure(RPCError(e.string("code") ?? "internal", e.string("message") ?? "unknown error"))
  }

  static func encode(_ r: RPCResult) -> String {
    switch r {
    case .success(let o): return JSON.encode(["ok": true, "result": o])
    case .failure(let e): return JSON.encode(["ok": false, "error": e.json])
    }
  }
}

func nowUnixMs() -> UInt64 { UInt64(max(0, Date().timeIntervalSince1970 * 1000)) }

/// Tiny logger (os_log is overkill for a shell; `log stream --predicate 'process == "EmberMesh"'`).
func emberLog(_ msg: @autoclosure () -> String) {
  NSLog("[ember] %@", msg())
}

/// Startup timing: milliseconds since the kernel created this process (includes dyld + runtime init).
enum LaunchClock {
  static let processStart: Date = {
    var kp = kinfo_proc()
    var size = MemoryLayout<kinfo_proc>.stride
    var mib: [Int32] = [CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()]
    guard sysctl(&mib, 4, &kp, &size, nil, 0) == 0 else { return Date() }
    let tv = kp.kp_proc.p_un.__p_starttime
    return Date(timeIntervalSince1970: Double(tv.tv_sec) + Double(tv.tv_usec) / 1_000_000)
  }()
  static var sinceStartMs: Int { Int(Date().timeIntervalSince(processStart) * 1000) }
  private static var marked = Set<String>()
  private static let lock = NSLock()
  /// Logs "[ember] startup <what>: <ms> ms" once per label.
  static func mark(_ what: String) {
    lock.lock(); let first = marked.insert(what).inserted; lock.unlock()
    if first { emberLog("startup \(what): \(sinceStartMs) ms after process start") }
  }
}
