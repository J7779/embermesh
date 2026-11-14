import CEmber
import Foundation
import Security

/// One item drained from the core outbox (ember_poll).
enum CoreOut: Equatable {
  case event(String)              // {"event":"…","data":{…}}
  case linkTx(UInt32, Data)       // bytes for the endpoint under link_id
}

/// Everything the shell needs from the core. `EmberNode` is the real implementation; tests may use
/// fakes. All methods are thread-safe in the C API, but the app only calls them from NodeRunner's
/// serial queue (audio sample transfer included) so ordering is deterministic.
protocol CoreAPI: AnyObject {
  func call(_ method: String, _ paramsJSON: String?) -> String
  func tick(_ nowMs: UInt64)
  func nextWakeupMs() -> UInt32
  func poll() -> CoreOut?
  func linkRx(_ linkId: UInt32, _ data: Data)
  func audioRx(_ linkId: UInt32, _ samples: UnsafeBufferPointer<Int16>, sampleRate: UInt32)
  func audioTx(_ linkId: UInt32, _ out: UnsafeMutableBufferPointer<Int16>, sampleRate: UInt32) -> Int
  func addEntropy(_ data: Data)
  /// Zero-copy load: `bytes` must stay valid until the node is destroyed.
  func loadGuides(_ bytes: UnsafeRawBufferPointer, copy: Bool) -> Int32
}

/// Swift wrapper over the whole C API in core/include/ember/ember.h.
final class EmberNode: CoreAPI {
  private let handle: OpaquePointer
  /// Memory the core references without copying (memory-mapped guides.pack); released after destroy.
  private var retained: [AnyObject] = []

  static var version: String { String(cString: ember_version()) }

  /// Cryptographically secure random bytes from the platform CSPRNG.
  static func entropy(_ count: Int = 64) -> Data {
    var d = Data(count: count)
    let rc = d.withUnsafeMutableBytes { SecRandomCopyBytes(kSecRandomDefault, count, $0.baseAddress!) }
    precondition(rc == errSecSuccess, "SecRandomCopyBytes failed")
    return d
  }

  /// config: {"storage_dir", "platform", "app_version", "device_name", "now_ms"} (ember.h).
  init?(config: JSONObject, entropy: Data = EmberNode.entropy()) {
    let json = JSON.encode(config)
    let h: OpaquePointer? = entropy.withUnsafeBytes { raw in
      ember_create(json, raw.bindMemory(to: UInt8.self).baseAddress, raw.count)
    }
    guard let h else { return nil }
    handle = h
  }

  deinit {
    ember_destroy(handle)
    retained.removeAll()  // unmap only after the core can no longer touch it
  }

  func call(_ method: String, _ paramsJSON: String?) -> String {
    guard let p = ember_call(handle, method, paramsJSON ?? "{}") else {
      return #"{"ok":false,"error":{"code":"internal","message":"ember_call returned NULL"}}"#
    }
    defer { ember_free(p) }
    return String(cString: p)
  }

  func tick(_ nowMs: UInt64) { ember_tick(handle, nowMs) }
  func nextWakeupMs() -> UInt32 { ember_next_wakeup_ms(handle) }

  func poll() -> CoreOut? {
    var out = ember_out()
    guard ember_poll(handle, &out) != 0 else { return nil }
    let bytes: UnsafeBufferPointer<UInt8> = out.data.map { UnsafeBufferPointer(start: $0, count: out.len) }
      ?? UnsafeBufferPointer(start: nil, count: 0)
    switch Int(out.kind) {
    case Int(EMBER_OUT_EVENT): return .event(String(decoding: bytes, as: UTF8.self))
    case Int(EMBER_OUT_LINK_TX): return .linkTx(out.link_id, Data(buffer: bytes))
    default: return poll()  // unknown kinds are skipped (forward compatible)
    }
  }

  func linkRx(_ linkId: UInt32, _ data: Data) {
    data.withUnsafeBytes { raw in
      ember_link_rx(handle, linkId, raw.bindMemory(to: UInt8.self).baseAddress, raw.count)
    }
  }

  func audioRx(_ linkId: UInt32, _ samples: UnsafeBufferPointer<Int16>, sampleRate: UInt32) {
    ember_audio_rx(handle, linkId, samples.baseAddress, samples.count, sampleRate)
  }

  func audioTx(_ linkId: UInt32, _ out: UnsafeMutableBufferPointer<Int16>, sampleRate: UInt32) -> Int {
    guard let base = out.baseAddress, out.count > 0 else { return 0 }
    return min(out.count, ember_audio_tx(handle, linkId, base, out.count, sampleRate))
  }

  func addEntropy(_ data: Data) {
    data.withUnsafeBytes { raw in ember_add_entropy(handle, raw.bindMemory(to: UInt8.self).baseAddress, raw.count) }
  }

  func loadGuides(_ bytes: UnsafeRawBufferPointer, copy: Bool) -> Int32 {
    ember_guides_load(handle, bytes.bindMemory(to: UInt8.self).baseAddress, bytes.count, copy ? 1 : 0)
  }

  /// Memory-maps the pack and hands it to the core with copy=0. The mapping lives as long as the node.
  @discardableResult
  func loadGuides(mapped file: MappedFile) -> Int32 {
    retained.append(file)
    return loadGuides(file.bytes, copy: false)
  }
}

/// Read-only memory-mapped file (mmap). Pages are loaded lazily and can be dropped by the OS under
/// memory pressure without counting against the app's dirty memory — important on 2 GB phones.
final class MappedFile {
  let url: URL
  let count: Int
  private let base: UnsafeMutableRawPointer?

  init?(url: URL) {
    self.url = url
    let fd = open(url.path, O_RDONLY)
    guard fd >= 0 else { return nil }
    defer { close(fd) }
    var st = stat()
    guard fstat(fd, &st) == 0, st.st_size > 0, st.st_size < Int64(Int32.max) else { return nil }
    count = Int(st.st_size)
    let p = mmap(nil, count, PROT_READ, MAP_PRIVATE, fd, 0)
    guard let p, p != MAP_FAILED else { return nil }
    base = p
  }

  var bytes: UnsafeRawBufferPointer { UnsafeRawBufferPointer(start: base, count: count) }

  /// Data view without copying; valid while this object is alive.
  func data() -> Data {
    guard let base else { return Data() }
    return Data(bytesNoCopy: base, count: count, deallocator: .none)
  }

  deinit { if let base { munmap(base, count) } }
}
