import Foundation

/// Callbacks from an endpoint to whoever owns it (LinkManager). May be called on any queue.
protocol EndpointDelegate: AnyObject {
  /// Bytes from the medium: a stream chunk, or exactly one datagram/message.
  func endpoint(_ e: Endpoint, received data: Data)
  /// The endpoint became usable (connected=true) or stopped being usable.
  func endpoint(_ e: Endpoint, connected: Bool, error: String?)
  /// Something in the endpoint's status changed (peer count etc.).
  func endpointStatusChanged(_ e: Endpoint)
}

/// A platform byte (or audio) pipe underneath one core link. Endpoints know nothing about framing
/// or protocols (docs/ARCHITECTURE.md §1): they open, close, move bytes and report state.
protocol Endpoint: AnyObject {
  var kind: String { get }
  var delegate: EndpointDelegate? { get set }
  /// The link this endpoint serves (set before open()).
  var linkId: UInt32 { get set }
  var isConnected: Bool { get }
  /// Start the endpoint. State is reported asynchronously through the delegate.
  func open()
  /// Stop and release all resources. No callbacks after close() returns (best effort).
  func close()
  /// One link_io_write from the core. Thread-safe.
  func write(_ data: Data)
  /// Endpoint specific status for the UI (peers, port, sample rate …).
  var status: JSONObject { get }
  /// Client endpoints that should be re-opened with backoff after an error.
  var wantsAutoReconnect: Bool { get }
}

/// Reconnect timing for client endpoints (LinkManager): exponential from 2 s up to `capSeconds`,
/// optional ±jitter, backoff reset immediately on connect (resetAfterSeconds 0) or only after the
/// link stayed up that long (relays: CONNECTIVITY §4.9 "2 s … 5 min, ±20 %, reset after 2 min").
struct ReconnectPolicy: Equatable {
  var capSeconds: Double = 60
  var jitter: Double = 0
  var resetAfterSeconds: Double = 0
  static let standard = ReconnectPolicy()
  static let relay = ReconnectPolicy(capSeconds: 300, jitter: 0.2, resetAfterSeconds: 120)

  func delay(attempt n: Int, random: Double = Double.random(in: -1...1)) -> Double {
    let base = min(capSeconds, pow(2.0, Double(min(n, 20))) * 2)
    return max(1, base * (1 + jitter * random))
  }
}

extension Endpoint {
  var reconnectPolicy: ReconnectPolicy { (self as? WebSocketEndpoint)?.isRelay == true ? .relay : .standard }
}

/// Shared plumbing: thread-safe state + delegate calls.
class BaseEndpoint: NSObject, Endpoint {
  let kind: String
  weak var delegate: EndpointDelegate?
  var linkId: UInt32 = 0
  private let lock = NSLock()
  private var _connected = false
  private var _lastError: String?
  var wantsAutoReconnect: Bool { false }

  init(kind: String) {
    self.kind = kind
    super.init()
  }

  var isConnected: Bool { lock.lock(); defer { lock.unlock() }; return _connected }
  var lastError: String? { lock.lock(); defer { lock.unlock() }; return _lastError }

  func setConnected(_ c: Bool, error: String? = nil) {
    lock.lock()
    let changed = c != _connected || error != _lastError
    _connected = c
    _lastError = error
    lock.unlock()
    if changed { delegate?.endpoint(self, connected: c, error: error) }
  }

  func deliver(_ data: Data) {
    guard !data.isEmpty else { return }
    delegate?.endpoint(self, received: data)
  }

  func statusChanged() { delegate?.endpointStatusChanged(self) }

  func open() {}
  func close() {}
  func write(_ data: Data) {}
  var status: JSONObject { [:] }
}

/// Echoes every write back as received bytes — for testing link plumbing without hardware.
final class LoopbackEndpoint: BaseEndpoint {
  private let queue = DispatchQueue(label: "org.embermesh.loopback")
  private var open_ = false
  private(set) var written = 0

  init() { super.init(kind: "loopback") }

  override func open() { queue.async { self.open_ = true; self.setConnected(true) } }
  override func close() { queue.sync { self.open_ = false }; setConnected(false) }
  override func write(_ data: Data) {
    queue.async {
      guard self.open_ else { return }
      self.written += data.count
      self.deliver(data)
    }
  }
  override var status: JSONObject { ["echoed_bytes": queue.sync { written }] }
}

/// Typed access to an endpoint config (values from the UI form, defaults applied).
struct EndpointConfig {
  let raw: JSONObject
  init(_ raw: JSONObject) { self.raw = raw }
  func string(_ k: String, _ def: String = "") -> String {
    if let s = raw.string(k) { return s.trimmingCharacters(in: .whitespacesAndNewlines) }
    if let n = raw.number(k) { return n == n.rounded() ? String(Int(n)) : String(n) }
    return def
  }
  func int(_ k: String, _ def: Int) -> Int { raw.int(k) ?? def }
  func double(_ k: String, _ def: Double) -> Double { raw.number(k) ?? def }
  func bool(_ k: String, _ def: Bool) -> Bool { raw.bool(k) ?? def }
}
