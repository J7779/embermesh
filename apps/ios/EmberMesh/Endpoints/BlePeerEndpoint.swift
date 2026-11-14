import CoreBluetooth
import Foundation

/// UUIDs from docs/ARCHITECTURE.md §6.
enum EmberUUID {
  static let peerService = CBUUID(string: "8e3b0001-5d1f-4c6a-9b7e-0d2a6f1c4e10")
  static let peerRx = CBUUID(string: "8e3b0002-5d1f-4c6a-9b7e-0d2a6f1c4e10")   // write / write-without-response
  static let peerTx = CBUUID(string: "8e3b0003-5d1f-4c6a-9b7e-0d2a6f1c4e10")   // notify
  static let nusService = CBUUID(string: "6e400001-b5a3-f393-e0a9-e50e24dcca9e")
  static let nusRx = CBUUID(string: "6e400002-b5a3-f393-e0a9-e50e24dcca9e")
  static let nusTx = CBUUID(string: "6e400003-b5a3-f393-e0a9-e50e24dcca9e")
  // "BLE KISS API" (Mobilinkd TNC3/TNC4 and TNCs that copied it): KISS byte stream, like NUS.
  // Characteristic 0002 is written by us (to the TNC), 0003 notifies (from the TNC).
  static let kissService = CBUUID(string: "00000001-ba2a-46c9-ae49-01b0961f68bb")
  static let kissRx = CBUUID(string: "00000002-ba2a-46c9-ae49-01b0961f68bb")
  static let kissTx = CBUUID(string: "00000003-ba2a-46c9-ae49-01b0961f68bb")
  // Meshtastic BLE profile (meshtastic.org/docs/development/device/client-api)
  static let meshService = CBUUID(string: "6ba1b218-15a8-461f-9fa8-5dcae273eafd")
  static let meshToRadio = CBUUID(string: "f75c76d2-129e-4dad-a1dd-7866124401e7")
  static let meshFromRadio = CBUUID(string: "2c55e69e-4993-11ed-b878-0242ac120002")
  static let meshFromNum = CBUUID(string: "ed9da18c-a800-4f66-a670-aa7547e34453")
}

enum BluetoothAvailability {
  /// Availability without creating a manager (creating one triggers the permission prompt).
  static func check() -> (Bool, String?) {
    #if targetEnvironment(simulator)
    return (false, "Bluetooth is not available in the iOS Simulator. Use a real iPhone or iPad.")
    #else
    switch CBManager.authorization {
    case .denied: return (false, "Bluetooth permission is off. Turn it on in Settings > EmberMesh > Bluetooth.")
    case .restricted: return (false, "Bluetooth is restricted on this device (Screen Time or device management).")
    default: return (true, nil)
    }
    #endif
  }
}

/// Phone↔phone Bluetooth LE byte stream, interoperable with the Android app.
///
/// Runs both roles at once on one serial queue:
///   * peripheral: GATT service 8e3b0001 with RX (write/write-without-response) and TX (notify),
///     advertised with the service UUID;
///   * central: scans for the same service, connects to many peers, subscribes to TX, writes RX.
/// Every core write is sent to every connected peer, chunked to that peer's ATT payload size.
/// Bytes from different peers are merged without mid-frame interleaving by RxArbiter.
/// Two phones that both connect to each other form two paths; the mesh drops the duplicates.
/// State restoration keeps connections alive when iOS relaunches the app in the background.
final class BlePeerEndpoint: BaseEndpoint, CBPeripheralManagerDelegate, CBCentralManagerDelegate, CBPeripheralDelegate {
  static let centralRestoreId = "org.embermesh.ble.peer.central"
  static let peripheralRestoreId = "org.embermesh.ble.peer.peripheral"
  /// Only one instance may own the restore identifiers.
  private static var activeInstance = false
  private static let instanceLock = NSLock()

  private let queue = DispatchQueue(label: "org.embermesh.ble.peer")
  private let cfg: EndpointConfig
  private var central: CBCentralManager?
  private var peripheral: CBPeripheralManager?
  private var service: CBMutableService?
  private var txChar: CBMutableCharacteristic?
  private var advertisingStarted = false

  /// Peers we are connected to as a central.
  private final class RemotePeripheral {
    let p: CBPeripheral
    var rx: CBCharacteristic?
    var ready = false
    var queue = ChunkQueue()
    var awaitingResponse = false
    var lastSeen = Date()
    var rssi: Int?
    init(_ p: CBPeripheral) { self.p = p }
  }
  private var remotes: [UUID: RemotePeripheral] = [:]
  /// Centrals subscribed to our TX characteristic.
  private final class Subscriber {
    let c: CBCentral
    var queue = ChunkQueue()
    init(_ c: CBCentral) { self.c = c }
  }
  private var subscribers: [UUID: Subscriber] = [:]
  private var discovered: [UUID: (name: String, rssi: Int, seen: Date)] = [:]
  private let arbiter = RxArbiter<UUID>(gapMs: 150)
  private var arbiterTimer: DispatchSourceTimer?
  private var isOpen = false
  private var ownsInstance = false

  var maxPeers: Int { max(1, min(15, cfg.int("max_peers", 7))) }

  init(config: JSONObject) {
    cfg = EndpointConfig(config)
    super.init(kind: "ble_peer")
  }

  override func open() {
    BlePeerEndpoint.instanceLock.lock()
    if BlePeerEndpoint.activeInstance && !ownsInstance {
      BlePeerEndpoint.instanceLock.unlock()
      setConnected(false, error: "Bluetooth peer link already exists")
      return
    }
    BlePeerEndpoint.activeInstance = true
    ownsInstance = true
    BlePeerEndpoint.instanceLock.unlock()
    queue.async {
      guard !self.isOpen else { return }
      self.isOpen = true
      let (ok, reason) = BluetoothAvailability.check()
      if !ok { self.setConnected(false, error: reason); return }
      if self.cfg.bool("scan", true) {
        self.central = CBCentralManager(delegate: self, queue: self.queue, options: [
          CBCentralManagerOptionRestoreIdentifierKey: BlePeerEndpoint.centralRestoreId,
          CBCentralManagerOptionShowPowerAlertKey: false,
        ])
      }
      if self.cfg.bool("advertise", true) {
        self.peripheral = CBPeripheralManager(delegate: self, queue: self.queue, options: [
          CBPeripheralManagerOptionRestoreIdentifierKey: BlePeerEndpoint.peripheralRestoreId,
          CBPeripheralManagerOptionShowPowerAlertKey: false,
        ])
      }
    }
  }

  override func close() {
    queue.sync {
      isOpen = false
      arbiterTimer?.cancel(); arbiterTimer = nil
      if let c = central {
        c.stopScan()
        for r in remotes.values { c.cancelPeripheralConnection(r.p) }
        c.delegate = nil
      }
      if let p = peripheral {
        p.stopAdvertising()
        p.removeAllServices()
        p.delegate = nil
      }
      remotes.removeAll(); subscribers.removeAll(); discovered.removeAll()
      central = nil; peripheral = nil; service = nil; txChar = nil; advertisingStarted = false
    }
    if ownsInstance {
      BlePeerEndpoint.instanceLock.lock(); BlePeerEndpoint.activeInstance = false; BlePeerEndpoint.instanceLock.unlock()
      ownsInstance = false
    }
    setConnected(false)
  }

  override var status: JSONObject {
    queue.sync {
      [
        "peers": remotes.values.filter { $0.ready }.count + subscribers.count,
        "outgoing": remotes.values.filter { $0.ready }.count,
        "incoming": subscribers.count,
        "advertising": advertisingStarted,
        "scanning": central?.isScanning ?? false,
        "bluetooth": stateName(central?.state ?? peripheral?.state ?? .unknown),
      ]
    }
  }

  /// Peers seen recently (for host.endpoints.scan).
  func nearby() -> [JSONObject] {
    queue.sync {
      let cutoff = Date().addingTimeInterval(-60)
      return discovered.filter { $0.value.seen > cutoff }.map { id, v in
        ["id": id.uuidString, "name": v.name, "rssi": v.rssi,
         "detail": remotes[id]?.ready == true ? "connected" : "nearby", "config": [String: Any]()]
      }
    }
  }

  // MARK: write

  override func write(_ data: Data) {
    queue.async {
      guard self.isOpen, !data.isEmpty else { return }
      for r in self.remotes.values where r.ready {
        r.queue.enqueue(data, mtu: r.p.maximumWriteValueLength(for: .withoutResponse))
        self.pump(r)
      }
      for s in self.subscribers.values {
        s.queue.enqueue(data, mtu: s.c.maximumUpdateValueLength)
      }
      self.pumpSubscribers()
    }
  }

  private func pump(_ r: RemotePeripheral) {
    guard let rx = r.rx, r.p.state == .connected else { return }
    let noResp = rx.properties.contains(.writeWithoutResponse)
    while let chunk = r.queue.peek() {
      if noResp {
        guard r.p.canSendWriteWithoutResponse else { return }  // resumes in peripheralIsReady
        r.p.writeValue(chunk, for: rx, type: .withoutResponse)
        r.queue.pop()
      } else {
        guard !r.awaitingResponse else { return }
        r.awaitingResponse = true
        r.p.writeValue(chunk, for: rx, type: .withResponse)
        r.queue.pop()
        return
      }
    }
  }

  private func pumpSubscribers() {
    guard let pm = peripheral, let tx = txChar else { return }
    for s in subscribers.values {
      while let chunk = s.queue.peek() {
        if pm.updateValue(chunk, for: tx, onSubscribedCentrals: [s.c]) {
          s.queue.pop()
        } else {
          return  // transmit queue full: peripheralManagerIsReady(toUpdateSubscribers:) resumes
        }
      }
    }
  }

  // MARK: receive

  private func received(from id: UUID, _ data: Data) {
    for d in arbiter.receive(id, data, now: monotonicMs()) { deliver(d) }
    armArbiterTimer()
  }

  private func armArbiterTimer() {
    guard arbiter.hasPending, arbiterTimer == nil else { return }
    let t = DispatchSource.makeTimerSource(queue: queue)
    t.schedule(deadline: .now() + .milliseconds(50), repeating: .milliseconds(50))
    t.setEventHandler { [weak self] in
      guard let self else { return }
      for d in self.arbiter.advance(now: monotonicMs()) { self.deliver(d) }
      if !self.arbiter.hasPending { self.arbiterTimer?.cancel(); self.arbiterTimer = nil }
    }
    arbiterTimer = t
    t.resume()
  }

  private func peerLeft(_ id: UUID) {
    for d in arbiter.remove(id, now: monotonicMs()) { deliver(d) }
    statusChanged()
  }

  private func updateConnected() {
    let on = (central?.state == .poweredOn) || (peripheral?.state == .poweredOn)
    if on { setConnected(true) } else {
      let st = central?.state ?? peripheral?.state ?? .unknown
      setConnected(false, error: stateMessage(st))
    }
  }

  // MARK: CBPeripheralManagerDelegate

  func peripheralManager(_ pm: CBPeripheralManager, willRestoreState dict: [String: Any]) {
    if let services = dict[CBPeripheralManagerRestoredStateServicesKey] as? [CBMutableService],
       let s = services.first(where: { $0.uuid == EmberUUID.peerService }) {
      service = s
      txChar = s.characteristics?.first(where: { $0.uuid == EmberUUID.peerTx }) as? CBMutableCharacteristic
    }
    if dict[CBPeripheralManagerRestoredStateAdvertisementDataKey] != nil { advertisingStarted = true }
  }

  func peripheralManagerDidUpdateState(_ pm: CBPeripheralManager) {
    updateConnected()
    guard pm.state == .poweredOn, isOpen else {
      if pm.state != .poweredOn { subscribers.removeAll(); advertisingStarted = false }
      return
    }
    if service == nil || txChar == nil {
      let rx = CBMutableCharacteristic(type: EmberUUID.peerRx, properties: [.write, .writeWithoutResponse],
                                       value: nil, permissions: [.writeable])
      let tx = CBMutableCharacteristic(type: EmberUUID.peerTx, properties: [.notify], value: nil, permissions: [.readable])
      let s = CBMutableService(type: EmberUUID.peerService, primary: true)
      s.characteristics = [rx, tx]
      service = s
      txChar = tx
      pm.add(s)  // advertising starts in didAdd
    } else if !advertisingStarted {
      startAdvertising(pm)
    }
  }

  func peripheralManager(_ pm: CBPeripheralManager, didAdd service: CBService, error: Error?) {
    if let error { emberLog("ble_peer: add service failed: \(error.localizedDescription)"); return }
    startAdvertising(pm)
  }

  private func startAdvertising(_ pm: CBPeripheralManager) {
    let name = cfg.string("local_name").isEmpty ? "Ember" : String(cfg.string("local_name").prefix(20))
    pm.startAdvertising([CBAdvertisementDataServiceUUIDsKey: [EmberUUID.peerService], CBAdvertisementDataLocalNameKey: name])
  }

  func peripheralManagerDidStartAdvertising(_ pm: CBPeripheralManager, error: Error?) {
    advertisingStarted = error == nil
    if let error { emberLog("ble_peer: advertising failed: \(error.localizedDescription)") }
    statusChanged()
  }

  func peripheralManager(_ pm: CBPeripheralManager, central: CBCentral, didSubscribeTo characteristic: CBCharacteristic) {
    guard characteristic.uuid == EmberUUID.peerTx else { return }
    if subscribers.count >= maxPeers { return }
    subscribers[central.identifier] = Subscriber(central)
    emberLog("ble_peer: central subscribed (\(subscribers.count) incoming)")
    statusChanged()
  }

  func peripheralManager(_ pm: CBPeripheralManager, central: CBCentral, didUnsubscribeFrom characteristic: CBCharacteristic) {
    subscribers[central.identifier] = nil
    peerLeft(central.identifier)
  }

  func peripheralManagerIsReady(toUpdateSubscribers pm: CBPeripheralManager) { pumpSubscribers() }

  func peripheralManager(_ pm: CBPeripheralManager, didReceiveWrite requests: [CBATTRequest]) {
    var ok = true
    for r in requests {
      guard r.characteristic.uuid == EmberUUID.peerRx, r.offset == 0, let v = r.value else { ok = false; continue }
      received(from: r.central.identifier, v)
    }
    // Respond once to the first request (CoreBluetooth contract) when a response is expected.
    if let first = requests.first { pm.respond(to: first, withResult: ok ? .success : .invalidOffset) }
  }

  // MARK: CBCentralManagerDelegate

  func centralManager(_ c: CBCentralManager, willRestoreState dict: [String: Any]) {
    for p in (dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral]) ?? [] {
      let r = RemotePeripheral(p)
      p.delegate = self
      remotes[p.identifier] = r
    }
  }

  func centralManagerDidUpdateState(_ c: CBCentralManager) {
    updateConnected()
    guard c.state == .poweredOn, isOpen else {
      if c.state != .poweredOn { for r in remotes.values { r.ready = false } }
      return
    }
    for r in remotes.values {  // restored peripherals
      if r.p.state == .connected { r.p.discoverServices([EmberUUID.peerService]) } else { c.connect(r.p) }
    }
    startScan(c)
  }

  private func startScan(_ c: CBCentralManager) {
    guard c.state == .poweredOn, !c.isScanning else { return }
    // Service-filtered scanning also works in the background.
    c.scanForPeripherals(withServices: [EmberUUID.peerService], options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
  }

  func centralManager(_ c: CBCentralManager, didDiscover p: CBPeripheral, advertisementData: [String: Any], rssi: NSNumber) {
    let name = (advertisementData[CBAdvertisementDataLocalNameKey] as? String) ?? p.name ?? "Ember"
    discovered[p.identifier] = (name, rssi.intValue, Date())
    if let r = remotes[p.identifier] { r.lastSeen = Date(); r.rssi = rssi.intValue; return }
    guard remotes.count < maxPeers, rssi.intValue != 127 else { return }
    let r = RemotePeripheral(p)
    r.rssi = rssi.intValue
    remotes[p.identifier] = r
    p.delegate = self
    c.connect(p, options: nil)
  }

  func centralManager(_ c: CBCentralManager, didConnect p: CBPeripheral) {
    p.delegate = self
    p.discoverServices([EmberUUID.peerService])
  }

  func centralManager(_ c: CBCentralManager, didFailToConnect p: CBPeripheral, error: Error?) {
    remotes[p.identifier] = nil
    peerLeft(p.identifier)
  }

  func centralManager(_ c: CBCentralManager, didDisconnectPeripheral p: CBPeripheral, error: Error?) {
    let wasReady = remotes[p.identifier]?.ready ?? false
    remotes[p.identifier] = nil
    peerLeft(p.identifier)
    guard isOpen, c.state == .poweredOn else { return }
    if wasReady, remotes.count < maxPeers {
      // A pending connect never times out: iOS reconnects as soon as the peer is back in range,
      // even while we are in the background.
      let r = RemotePeripheral(p)
      remotes[p.identifier] = r
      c.connect(p, options: nil)
    }
    // Rediscover peers that were filtered as duplicates.
    c.stopScan()
    startScan(c)
  }

  // MARK: CBPeripheralDelegate

  func peripheral(_ p: CBPeripheral, didDiscoverServices error: Error?) {
    guard let s = p.services?.first(where: { $0.uuid == EmberUUID.peerService }) else {
      central?.cancelPeripheralConnection(p)
      return
    }
    p.discoverCharacteristics([EmberUUID.peerRx, EmberUUID.peerTx], for: s)
  }

  func peripheral(_ p: CBPeripheral, didDiscoverCharacteristicsFor s: CBService, error: Error?) {
    guard let r = remotes[p.identifier] else { return }
    let chars = s.characteristics ?? []
    r.rx = chars.first { $0.uuid == EmberUUID.peerRx }
    if let tx = chars.first(where: { $0.uuid == EmberUUID.peerTx }) { p.setNotifyValue(true, for: tx) }
    if r.rx == nil { central?.cancelPeripheralConnection(p) }
  }

  func peripheral(_ p: CBPeripheral, didUpdateNotificationStateFor ch: CBCharacteristic, error: Error?) {
    guard let r = remotes[p.identifier], ch.uuid == EmberUUID.peerTx else { return }
    if error == nil, ch.isNotifying {
      r.ready = true
      emberLog("ble_peer: connected to \(p.identifier) mtu=\(p.maximumWriteValueLength(for: .withoutResponse))")
      statusChanged()
      pump(r)
    } else {
      central?.cancelPeripheralConnection(p)
    }
  }

  func peripheral(_ p: CBPeripheral, didUpdateValueFor ch: CBCharacteristic, error: Error?) {
    guard ch.uuid == EmberUUID.peerTx, error == nil, let v = ch.value, !v.isEmpty else { return }
    remotes[p.identifier]?.lastSeen = Date()
    received(from: p.identifier, v)
  }

  func peripheral(_ p: CBPeripheral, didWriteValueFor ch: CBCharacteristic, error: Error?) {
    guard let r = remotes[p.identifier] else { return }
    r.awaitingResponse = false
    pump(r)
  }

  func peripheralIsReady(toSendWriteWithoutResponse p: CBPeripheral) {
    if let r = remotes[p.identifier] { pump(r) }
  }

  // MARK: helpers

  private func stateName(_ s: CBManagerState) -> String {
    switch s {
    case .poweredOn: return "on"
    case .poweredOff: return "off"
    case .unauthorized: return "unauthorized"
    case .unsupported: return "unsupported"
    case .resetting: return "resetting"
    default: return "unknown"
    }
  }

  private func stateMessage(_ s: CBManagerState) -> String {
    switch s {
    case .poweredOff: return "Bluetooth is turned off"
    case .unauthorized: return "Bluetooth permission is off (Settings > EmberMesh)"
    case .unsupported: return "This device does not support Bluetooth LE"
    case .resetting: return "Bluetooth is restarting"
    default: return "Waiting for Bluetooth"
    }
  }
}
