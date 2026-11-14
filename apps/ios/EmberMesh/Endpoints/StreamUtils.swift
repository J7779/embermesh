import Foundation

/// MTU-aware chunking for BLE characteristic writes / notifications.
enum ByteChunker {
  /// ATT payload limits: 20 bytes is the BLE 4.0 minimum, 512 the maximum attribute length.
  static let minChunk = 20
  static let maxChunk = 512

  static func clampMTU(_ mtu: Int) -> Int { Swift.min(Swift.max(mtu, minChunk), maxChunk) }

  static func split(_ data: Data, mtu: Int) -> [Data] {
    guard !data.isEmpty else { return [] }
    let size = clampMTU(mtu)
    var out: [Data] = []
    out.reserveCapacity((data.count + size - 1) / size)
    var i = data.startIndex
    while i < data.endIndex {
      let j = data.index(i, offsetBy: size, limitedBy: data.endIndex) ?? data.endIndex
      out.append(Data(data[i..<j]))
      i = j
    }
    return out
  }
}

/// Per-peer outgoing queue of chunks with whole-write boundaries. When the peer cannot keep up the
/// oldest whole writes that have not started are dropped (the mesh retransmits), never half a frame.
struct ChunkQueue {
  private(set) var writes: [[Data]] = []  // each element = the chunks of one core write
  private var headIndex = 0               // next chunk within writes[0]
  private(set) var bytes = 0              // bytes not yet popped
  let capBytes: Int
  private(set) var droppedWrites = 0

  init(capBytes: Int = 64 * 1024) { self.capBytes = capBytes }

  var isEmpty: Bool { writes.isEmpty }
  var chunkCount: Int { writes.reduce(0) { $0 + $1.count } - headIndex }

  mutating func enqueue(_ data: Data, mtu: Int) {
    let chunks = ByteChunker.split(data, mtu: mtu)
    guard !chunks.isEmpty else { return }
    writes.append(chunks)
    bytes += data.count
    while bytes > capBytes {
      let first = headIndex > 0 ? 1 : 0       // never drop the write in progress
      guard first < writes.count - 1 else { break }  // always keep the newest write
      let removed = writes.remove(at: first)
      bytes -= removed.reduce(0) { $0 + $1.count }
      droppedWrites += 1
    }
  }

  func peek() -> Data? { writes.first.map { $0[headIndex] } }

  @discardableResult
  mutating func pop() -> Data? {
    guard let first = writes.first else { return nil }
    let c = first[headIndex]
    headIndex += 1
    bytes -= c.count
    if headIndex >= first.count {
      writes.removeFirst()
      headIndex = 0
    }
    return c
  }

  mutating func removeAll() { writes.removeAll(); headIndex = 0; bytes = 0 }
}

/// Merges byte streams from several peers into the single stream a core link expects without
/// interleaving them mid-frame. Only one peer ("owner") is forwarded at a time; chunks from others
/// are buffered until the owner has been quiet for `gapMs` (a burst of chunks = one frame). This
/// needs no extra header on the air, so it stays interoperable with any plain byte-stream peer.
/// Not thread-safe: use from one queue.
final class RxArbiter<Key: Hashable> {
  let gapMs: UInt64
  let capBytesPerPeer: Int
  private var owner: Key?
  private var ownerLast: UInt64 = 0
  private var order: [Key] = []
  private var pending: [Key: (chunks: [Data], bytes: Int, last: UInt64)] = [:]
  private(set) var droppedBytes = 0

  init(gapMs: UInt64 = 150, capBytesPerPeer: Int = 64 * 1024) {
    self.gapMs = gapMs
    self.capBytesPerPeer = capBytesPerPeer
  }

  var hasPending: Bool { !pending.isEmpty }
  var currentOwner: Key? { owner }

  /// Returns the data to forward now, in order. `now` must be monotonic milliseconds.
  func receive(_ key: Key, _ data: Data, now: UInt64) -> [Data] {
    var out = advance(now: now)
    if owner == nil && pending[key] == nil { owner = key }
    if owner == key {
      ownerLast = now
      out.append(data)
      return out
    }
    var p = pending[key] ?? (chunks: [], bytes: 0, last: now)
    if pending[key] == nil { order.append(key) }
    p.chunks.append(data)
    p.bytes += data.count
    p.last = now
    while p.bytes > capBytesPerPeer, p.chunks.count > 1 {
      let d = p.chunks.removeFirst()
      p.bytes -= d.count
      droppedBytes += d.count
    }
    pending[key] = p
    return out
  }

  /// Hands ownership over when the owner is idle. Call periodically while `hasPending`.
  func advance(now: UInt64) -> [Data] {
    var out: [Data] = []
    while true {
      if owner != nil {
        guard now >= ownerLast, now - ownerLast >= gapMs else { break }
        owner = nil
      }
      guard !order.isEmpty else { break }
      let next = order.removeFirst()
      guard let p = pending.removeValue(forKey: next) else { continue }
      out.append(contentsOf: p.chunks)
      owner = next
      ownerLast = p.last
    }
    return out
  }

  /// A peer went away. Its buffered chunks (if any) are still delivered in turn.
  func remove(_ key: Key, now: UInt64) -> [Data] {
    if owner == key { owner = nil }
    return advance(now: now)
  }
}

func monotonicMs() -> UInt64 { DispatchTime.now().uptimeNanoseconds / 1_000_000 }

/// Lock-protected ring buffer of PCM samples between the node queue (producer) and the audio
/// render thread (consumer). The critical section is a memcpy, so contention is negligible.
final class SampleRing {
  private var buf: [Int16]
  private var head = 0   // read index
  private var count_ = 0
  private let lock: UnsafeMutablePointer<os_unfair_lock>

  init(capacity: Int) {
    buf = [Int16](repeating: 0, count: max(1, capacity))
    lock = .allocate(capacity: 1)
    lock.initialize(to: os_unfair_lock())
  }
  deinit { lock.deinitialize(count: 1); lock.deallocate() }

  var capacity: Int { buf.count }
  var count: Int { os_unfair_lock_lock(lock); defer { os_unfair_lock_unlock(lock) }; return count_ }

  /// Writes as many samples as fit; returns how many were written.
  @discardableResult
  func write(_ s: UnsafeBufferPointer<Int16>) -> Int {
    os_unfair_lock_lock(lock); defer { os_unfair_lock_unlock(lock) }
    let n = min(s.count, buf.count - count_)
    var w = (head + count_) % buf.count
    for i in 0..<n {
      buf[w] = s[i]
      w += 1
      if w == buf.count { w = 0 }
    }
    count_ += n
    return n
  }

  /// Reads up to out.count samples; returns how many were read (rest untouched).
  func read(into out: UnsafeMutableBufferPointer<Int16>) -> Int {
    os_unfair_lock_lock(lock); defer { os_unfair_lock_unlock(lock) }
    let n = min(out.count, count_)
    for i in 0..<n {
      out[i] = buf[head]
      head += 1
      if head == buf.count { head = 0 }
    }
    count_ -= n
    return n
  }

  func clear() { os_unfair_lock_lock(lock); head = 0; count_ = 0; os_unfair_lock_unlock(lock) }
}
