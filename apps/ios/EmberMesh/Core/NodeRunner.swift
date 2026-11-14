import Foundation

/// Audio endpoints register as sinks: the runner pulls modem samples from the core on its queue and
/// pushes them into the endpoint's ring buffer, so the real-time render thread never takes the
/// core's lock.
protocol AudioSink: AnyObject {
  var linkId: UInt32 { get }
  var outputSampleRate: UInt32 { get }
  /// Number of samples the sink wants now (0 when its buffer is full enough or it is stopped).
  var samplesWanted: Int { get }
  func push(_ samples: UnsafeBufferPointer<Int16>)
}

/// Drives one core node on a private serial queue:
///   * ember_tick + drain the outbox after every input and whenever ember_next_wakeup_ms says so
///     (adaptive timer, so an idle node wakes at most about once a minute);
///   * RPC calls (ember_call) are serialized on the same queue;
///   * outbox events go to `onEvent`, link bytes to `onLinkTx` (both called on the node queue).
final class NodeRunner {
  let queue = DispatchQueue(label: "org.embermesh.node", qos: .userInitiated)
  private(set) var core: CoreAPI?
  private var timer: DispatchSourceTimer?
  private var audioSinks: [UInt32: AudioSink] = [:]
  private var scratch = [Int16](repeating: 0, count: 8192)
  private var pumpScheduled = false

  /// Called on the node queue.
  var onEvent: ((String) -> Void)?
  var onLinkTx: ((UInt32, Data) -> Void)?

  /// Upper bound for the tick interval while something latency sensitive (audio modem) is active.
  var activeIntervalMs: UInt32 = 20
  /// Lower bound to avoid a busy loop if the core keeps asking for 0 ms.
  var minIntervalMs: UInt32 = 5

  private(set) var ticks: UInt64 = 0

  init() {}

  func start(core: CoreAPI) {
    queue.sync {
      self.core = core
      let t = DispatchSource.makeTimerSource(queue: queue)
      t.setEventHandler { [weak self] in self?.pumpOnQueue() }
      timer = t
      t.schedule(deadline: .now())
      t.resume()
    }
  }

  /// Stops the timer and releases the core (on the node queue).
  func stop() {
    queue.sync {
      timer?.cancel()
      timer = nil
      audioSinks.removeAll()
      core = nil
    }
  }

  // MARK: RPC

  /// Raw call: envelope text delivered on the node queue.
  func callRaw(_ method: String, paramsJSON: String?, completion: @escaping (String) -> Void) {
    queue.async {
      guard let core = self.core else {
        completion(#"{"ok":false,"error":{"code":"busy","message":"node is not running"}}"#)
        return
      }
      let reply = core.call(method, paramsJSON)
      self.pumpOnQueue()  // a call may have produced events / link bytes
      completion(reply)
    }
  }

  /// Parsed call; completion on the main queue.
  func call(_ method: String, _ params: JSONObject = [:], completion: RPCCompletion? = nil) {
    callRaw(method, paramsJSON: JSON.encode(params)) { text in
      let r = Envelope.parse(text)
      if let completion { DispatchQueue.main.async { completion(r) } }
    }
  }

  /// Synchronous call; never call from the node queue.
  func callSync(_ method: String, _ params: JSONObject = [:]) -> RPCResult {
    dispatchPrecondition(condition: .notOnQueue(queue))
    return queue.sync {
      guard let core else { return .failure(.busy("node is not running")) }
      let r = Envelope.parse(core.call(method, JSON.encode(params)))
      pumpOnQueue()
      return r
    }
  }

  // MARK: input

  func linkRx(_ linkId: UInt32, _ data: Data) {
    guard !data.isEmpty else { return }
    queue.async {
      self.core?.linkRx(linkId, data)
      self.schedulePump()
    }
  }

  func audioRx(_ linkId: UInt32, _ samples: [Int16], sampleRate: UInt32) {
    queue.async {
      samples.withUnsafeBufferPointer { self.core?.audioRx(linkId, $0, sampleRate: sampleRate) }
      self.schedulePump()
    }
  }

  func addAudioSink(_ sink: AudioSink) { queue.async { self.audioSinks[sink.linkId] = sink; self.schedulePump() } }
  func removeAudioSink(linkId: UInt32) { queue.async { self.audioSinks[linkId] = nil } }

  /// Run a tick soon (e.g. app returned to foreground, a background task started).
  func kick() { queue.async { self.pumpOnQueue() } }

  /// Runs `body` with the core on the node queue (tests, guides loading).
  func withCore<T>(_ body: (CoreAPI?) -> T) -> T {
    dispatchPrecondition(condition: .notOnQueue(queue))
    return queue.sync { body(core) }
  }

  // MARK: pump

  /// Coalesces bursts of input (BLE chunks, audio buffers) into one tick.
  private func schedulePump() {
    if pumpScheduled { return }
    pumpScheduled = true
    queue.async { self.pumpOnQueue() }
  }

  private func pumpOnQueue() {
    pumpScheduled = false
    guard let core else { return }
    core.tick(nowUnixMs())
    ticks &+= 1
    drain(core)
    pullAudio(core)
    reschedule(core)
  }

  private func drain(_ core: CoreAPI) {
    var n = 0
    while n < 4096, let item = core.poll() {  // bounded so a flood cannot starve the queue
      n += 1
      switch item {
      case .event(let json): onEvent?(json)
      case .linkTx(let id, let data): onLinkTx?(id, data)
      }
    }
    if n == 4096 { schedulePump() }
  }

  private func pullAudio(_ core: CoreAPI) {
    for sink in audioSinks.values {
      var want = min(sink.samplesWanted, scratch.count)
      var guardRounds = 4
      while want > 0, guardRounds > 0 {
        guardRounds -= 1
        let rate = sink.outputSampleRate
        let n = scratch.withUnsafeMutableBufferPointer { buf -> Int in
          core.audioTx(sink.linkId, UnsafeMutableBufferPointer(rebasing: buf[0..<want]), sampleRate: rate)
        }
        if n <= 0 { break }
        scratch.withUnsafeBufferPointer { sink.push(UnsafeBufferPointer(rebasing: $0[0..<n])) }
        want = min(sink.samplesWanted, scratch.count)
      }
    }
  }

  private func reschedule(_ core: CoreAPI) {
    var ms = max(core.nextWakeupMs(), minIntervalMs)
    if !audioSinks.isEmpty { ms = min(ms, activeIntervalMs) }
    ms = min(ms, 60_000)
    // Leeway lets iOS batch wakeups when the node is idle (battery).
    let leeway = DispatchTimeInterval.milliseconds(Int(max(1, ms / 10)))
    timer?.schedule(deadline: .now() + .milliseconds(Int(ms)), leeway: leeway)
  }
}
