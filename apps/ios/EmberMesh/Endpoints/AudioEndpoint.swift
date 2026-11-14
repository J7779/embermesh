import AVFoundation
import Foundation

enum AudioAvailability {
  static func check() -> (Bool, String?) {
    if AVAudioSession.sharedInstance().recordPermission == .denied {
      return (false, "Microphone permission is off. Turn it on in Settings > EmberMesh > Microphone.")
    }
    return (true, nil)
  }

  static func requestPermission(_ done: @escaping (Bool) -> Void) {
    let s = AVAudioSession.sharedInstance()
    switch s.recordPermission {
    case .granted: done(true)
    case .denied: done(false)
    default: s.requestRecordPermission { ok in DispatchQueue.main.async { done(ok) } }
    }
  }

  /// Inputs/outputs for host.endpoints.scan.
  static func devices() -> [JSONObject] {
    let s = AVAudioSession.sharedInstance()
    var out: [JSONObject] = []
    for p in s.availableInputs ?? [] {
      out.append(["id": p.uid, "name": p.portName, "detail": "input: \(portKind(p.portType))",
                  "config": ["input": portKind(p.portType), "input_uid": p.uid]])
    }
    for p in s.currentRoute.outputs {
      out.append(["id": "out:" + p.uid, "name": p.portName, "detail": "output (current): \(portKind(p.portType))",
                  "config": ["output": p.portType == .builtInReceiver ? "receiver" : (p.portType == .builtInSpeaker ? "speaker" : "auto")]])
    }
    return out
  }

  static func portKind(_ t: AVAudioSession.Port) -> String {
    switch t {
    case .builtInMic: return "builtin"
    case .headsetMic, .lineIn: return "wired"
    case .usbAudio: return "usb"
    case .bluetoothHFP: return "bluetooth"
    default: return "other"
    }
  }
}

/// Soft-modem audio endpoint. The modem itself lives in the core ("audio" link driver); this only
/// moves PCM: microphone/line-in → ember_audio_rx, ember_audio_tx → speaker/line-out.
///
/// The session runs in measurement mode (no AGC, no echo cancellation, no noise suppression:
/// those destroy FSK/PSK tones) with voice processing explicitly off. The real hardware sample rate
/// is reported to the core, which accepts any rate >= 8 kHz. Interruptions (calls, Siri), route
/// changes (cable plugged/unplugged) and media-server resets restart the engine.
final class AudioEndpoint: BaseEndpoint, AudioSink {
  private let cfg: EndpointConfig
  private weak var runner: NodeRunner?
  private let engine = AVAudioEngine()
  private var source: AVAudioSourceNode?
  private let ring = SampleRing(capacity: 48_000)  // up to 1 s at 48 kHz
  private let control = DispatchQueue(label: "org.embermesh.audio")
  private var running = false
  private var wantOpen = false
  private var observers: [NSObjectProtocol] = []
  private var gain: Float
  private(set) var inputRate: Double = 0
  private(set) var outputRate: Double = 0
  private var rxScratch = [Int16]()
  private var renderScratch: UnsafeMutableBufferPointer<Int16>?
  private var restartItem: DispatchWorkItem?
  private var startedAt = Date.distantPast
  private(set) var rxSamples: UInt64 = 0
  private let listenerLock = NSLock()
  private var listeners: [Int: (AVAudioPCMBuffer, UInt32) -> Void] = [:]
  private var nextListener = 1

  /// Other consumers of the same input (Morse audio capture while the sound link runs): called on
  /// the tap thread with every captured buffer. Returns a token for removeInputListener.
  func addInputListener(_ fn: @escaping (AVAudioPCMBuffer, UInt32) -> Void) -> Int {
    listenerLock.lock(); defer { listenerLock.unlock() }
    nextListener += 1
    listeners[nextListener] = fn
    return nextListener
  }
  func removeInputListener(_ token: Int) {
    listenerLock.lock(); listeners[token] = nil; listenerLock.unlock()
  }
  private(set) var txSamples: UInt64 = 0

  deinit { renderScratch?.deallocate() }

  init(config: JSONObject, runner: NodeRunner) {
    cfg = EndpointConfig(config)
    self.runner = runner
    gain = Float(max(1, min(100, cfg.double("tx_level", 80)))) / 100
    super.init(kind: "audio")
  }

  // MARK: AudioSink (called on the node queue)

  var outputSampleRate: UInt32 { UInt32(outputRate > 0 ? outputRate : 48_000) }
  var samplesWanted: Int {
    guard running, outputRate > 0 else { return 0 }
    // Keep ~120 ms queued; the node pulls every 20 ms.
    let target = Int(outputRate * 0.12)
    return max(0, target - ring.count)
  }
  func push(_ samples: UnsafeBufferPointer<Int16>) { ring.write(samples) }

  // MARK: lifecycle

  override func open() {
    AudioAvailability.requestPermission { ok in
      guard ok else {
        self.setConnected(false, error: "Microphone permission is off. Turn it on in Settings > EmberMesh > Microphone.")
        return
      }
      self.control.async {
        self.wantOpen = true
        self.installObservers()
        self.start()
      }
    }
  }

  override func close() {
    control.sync {
      wantOpen = false
      removeObservers()
      stopEngine()
      try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
    }
    setConnected(false)
  }

  override var status: JSONObject {
    let s = AVAudioSession.sharedInstance()
    return [
      "input_rate": inputRate, "output_rate": outputRate,
      "input": s.currentRoute.inputs.first?.portName ?? "", "output": s.currentRoute.outputs.first?.portName ?? "",
      "input_latency_ms": Int(s.inputLatency * 1000), "output_latency_ms": Int(s.outputLatency * 1000),
      "rx_samples": rxSamples, "tx_samples": txSamples,
    ]
  }

  private func configureSession() throws {
    let s = AVAudioSession.sharedInstance()
    var opts: AVAudioSession.CategoryOptions = []
    let output = cfg.string("output", "auto")
    if output == "speaker" { opts.insert(.defaultToSpeaker) }
    // Bluetooth HFP is 8/16 kHz with heavy voice DSP: only when explicitly chosen.
    if cfg.string("input", "auto") == "bluetooth" { opts.insert(.allowBluetoothHFP) }
    try s.setCategory(.playAndRecord, mode: .measurement, options: opts)
    if let rate = Double(cfg.string("sample_rate", "48000")) { try? s.setPreferredSampleRate(rate) }
    try? s.setPreferredIOBufferDuration(0.01)
    try s.setActive(true)
    // Input route.
    let wantInput = cfg.string("input", "auto")
    let uid = cfg.string("input_uid")
    if let inputs = s.availableInputs {
      var pick: AVAudioSessionPortDescription?
      if !uid.isEmpty { pick = inputs.first { $0.uid == uid } }
      if pick == nil, wantInput != "auto" { pick = inputs.first { AudioAvailability.portKind($0.portType) == wantInput } }
      if let pick { try? s.setPreferredInput(pick) } else if wantInput == "auto" { try? s.setPreferredInput(nil) }
      // Measurement mode on the built-in mic: prefer the bottom mic, omnidirectional.
      if let mic = s.preferredInput ?? inputs.first(where: { $0.portType == .builtInMic }), mic.portType == .builtInMic,
         let ds = mic.dataSources?.first(where: { $0.orientation == .bottom }) ?? mic.dataSources?.first {
        try? mic.setPreferredDataSource(ds)
      }
    }
    switch output {
    case "speaker": try? s.overrideOutputAudioPort(.speaker)
    default: try? s.overrideOutputAudioPort(.none)
    }
    if s.isInputGainSettable { try? s.setInputGain(0.5) }
  }

  private func start() {
    guard wantOpen, !running else { return }
    do {
      try configureSession()
      try startEngine()
      running = true
      startedAt = Date()
      runner?.addAudioSink(self)
      emberLog("audio: in \(inputRate) Hz, out \(outputRate) Hz, route \(AVAudioSession.sharedInstance().currentRoute)")
      setConnected(true)
      statusChanged()
    } catch {
      setConnected(false, error: "audio start failed: \(error.localizedDescription)")
    }
  }

  private func startEngine() throws {
    let input = engine.inputNode
    if input.isVoiceProcessingEnabled { try? input.setVoiceProcessingEnabled(false) }
    let inFormat = input.outputFormat(forBus: 0)
    guard inFormat.sampleRate >= 8000, inFormat.channelCount > 0 else { throw RPCError.io("no audio input available") }
    inputRate = inFormat.sampleRate
    let frames = AVAudioFrameCount(max(256, Int(inputRate * 0.02)))
    let rate = UInt32(inputRate.rounded())
    input.removeTap(onBus: 0)
    input.installTap(onBus: 0, bufferSize: frames, format: inFormat) { [weak self] buf, _ in
      self?.captured(buf, sampleRate: rate)
    }

    let outRate = engine.outputNode.outputFormat(forBus: 0).sampleRate
    outputRate = outRate >= 8000 ? outRate : AVAudioSession.sharedInstance().sampleRate
    guard let mono = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: outputRate, channels: 1, interleaved: false) else {
      throw RPCError.io("unsupported output format")
    }
    let ring = self.ring
    let scratch = UnsafeMutableBufferPointer<Int16>.allocate(capacity: 1024)
    scratch.initialize(repeating: 0)
    renderScratch?.deallocate()
    renderScratch = scratch
    let src = AVAudioSourceNode(format: mono) { [weak self] _, _, frameCount, abl -> OSStatus in
      let got = AudioEndpoint.render(ring: ring, scratch: scratch, gain: self?.gain ?? 0.8,
                                     frames: Int(frameCount), into: UnsafeMutableAudioBufferListPointer(abl))
      self?.txSamples &+= UInt64(got)
      return noErr
    }
    if let old = source { engine.detach(old) }
    engine.attach(src)
    engine.connect(src, to: engine.mainMixerNode, format: mono)
    engine.mainMixerNode.outputVolume = 1
    source = src
    engine.prepare()
    try engine.start()
  }

  // MARK: sample paths (shared by the engine callbacks and the tests)

  /// Input tap: one captured buffer (Float32, first channel) → Int16 → ember_audio_rx at `sampleRate`.
  func captured(_ buf: AVAudioPCMBuffer, sampleRate rate: UInt32) {
    guard let ch = buf.floatChannelData?[0] else { return }
    let n = Int(buf.frameLength)
    if rxScratch.count < n { rxScratch = [Int16](repeating: 0, count: n) }
    for i in 0..<n {
      let v = max(-1, min(1, ch[i]))
      rxScratch[i] = Int16(v * 32767)
    }
    rxSamples &+= UInt64(n)
    runner?.audioRx(linkId, Array(rxScratch[0..<n]), sampleRate: rate)
    listenerLock.lock(); let ls = Array(listeners.values); listenerLock.unlock()
    for l in ls { l(buf, rate) }
  }

  /// Source-node render: modem samples from the ring (filled on the node queue by NodeRunner from
  /// ember_audio_tx) → Float32 into every buffer of `list`, silence when idle. Never allocates.
  /// Returns the number of modem samples consumed.
  static func render(ring: SampleRing, scratch: UnsafeMutableBufferPointer<Int16>, gain g: Float, frames n: Int,
                     into list: UnsafeMutableAudioBufferListPointer) -> Int {
    var done = 0
    var got = 0
    while done < n {  // fixed scratch, chunked
      let step = min(scratch.count, n - done)
      let r = ring.read(into: UnsafeMutableBufferPointer(rebasing: scratch[0..<step]))
      got += r
      for b in list {
        guard let p = b.mData?.assumingMemoryBound(to: Float.self) else { continue }
        for i in 0..<step { p[done + i] = i < r ? Float(scratch[i]) / 32768 * g : 0 }
      }
      done += step
    }
    return got
  }

  #if DEBUG
  /// Tests: run the endpoint without AVAudioEngine (the Simulator has no radio to loop through).
  /// Registers as the node's audio sink exactly like start() does, with the given rates.
  func startWithoutEngine(inputRate: Double, outputRate: Double) {
    control.sync {
      self.inputRate = inputRate
      self.outputRate = outputRate
      running = true
    }
    runner?.addAudioSink(self)
    setConnected(true)
  }

  /// Tests: what the source node would hand to the hardware for `frames` frames (mono Float32).
  func renderForTesting(frames: Int) -> AVAudioPCMBuffer? {
    guard let f = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: outputRate, channels: 1, interleaved: false),
          let b = AVAudioPCMBuffer(pcmFormat: f, frameCapacity: AVAudioFrameCount(frames)) else { return nil }
    b.frameLength = AVAudioFrameCount(frames)
    let scratch = UnsafeMutableBufferPointer<Int16>.allocate(capacity: 1024)
    defer { scratch.deallocate() }
    scratch.initialize(repeating: 0)
    let got = AudioEndpoint.render(ring: ring, scratch: scratch, gain: gain, frames: frames,
                                   into: UnsafeMutableAudioBufferListPointer(b.mutableAudioBufferList))
    txSamples &+= UInt64(got)
    return b
  }

  /// Tests: whether a real AVAudioEngine could be started here (Simulator: depends on the Mac's devices).
  var engineRunning: Bool { engine.isRunning }
  #endif

  private func stopEngine() {
    guard running || engine.isRunning else { return }
    runner?.removeAudioSink(linkId: linkId)
    engine.inputNode.removeTap(onBus: 0)
    engine.stop()
    if let s = source { engine.detach(s); source = nil }
    ring.clear()
    running = false
  }

  /// Debounced: our own session configuration triggers route/configuration notifications.
  private func restart(reason: String, force: Bool = false) {
    control.async { [self] in
      guard self.wantOpen else { return }
      if !force, self.engine.isRunning, Date().timeIntervalSince(self.startedAt) < 1.5 { return }
      self.restartItem?.cancel()
      let item = DispatchWorkItem { [weak self] in
        guard let self, self.wantOpen else { return }
        emberLog("audio: restarting (\(reason))")
        self.stopEngine()
        self.start()
      }
      self.restartItem = item
      self.control.asyncAfter(deadline: .now() + 0.3, execute: item)
    }
  }

  // MARK: notifications

  private func installObservers() {
    guard observers.isEmpty else { return }
    let nc = NotificationCenter.default
    let s = AVAudioSession.sharedInstance()
    observers.append(nc.addObserver(forName: AVAudioSession.interruptionNotification, object: s, queue: nil) { [weak self] n in
      guard let self, let raw = n.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt,
            let type = AVAudioSession.InterruptionType(rawValue: raw) else { return }
      if type == .began {
        self.control.async { self.stopEngine() }
        self.setConnected(false, error: "audio interrupted (phone call or another app)")
      } else {
        self.restart(reason: "interruption ended", force: true)
      }
    })
    observers.append(nc.addObserver(forName: AVAudioSession.routeChangeNotification, object: s, queue: nil) { [weak self] n in
      guard let raw = n.userInfo?[AVAudioSessionRouteChangeReasonKey] as? UInt,
            let reason = AVAudioSession.RouteChangeReason(rawValue: raw) else { return }
      switch reason {
      case .newDeviceAvailable, .oldDeviceUnavailable:
        self?.restart(reason: "audio device plugged/unplugged", force: true)
      default: break
      }
    })
    observers.append(nc.addObserver(forName: AVAudioSession.mediaServicesWereResetNotification, object: s, queue: nil) { [weak self] _ in
      self?.restart(reason: "media services reset", force: true)
    })
    observers.append(nc.addObserver(forName: .AVAudioEngineConfigurationChange, object: engine, queue: nil) { [weak self] _ in
      self?.restart(reason: "engine configuration change")
    })
  }

  private func removeObservers() {
    observers.forEach { NotificationCenter.default.removeObserver($0) }
    observers.removeAll()
  }
}
