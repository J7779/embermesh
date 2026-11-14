import CoreBluetooth
import Foundation

/// Meshtastic's serial API wraps each protobuf in 0x94 0xC3 <len hi> <len lo>; its BLE API carries
/// the bare protobufs. The core's "meshtastic" link driver speaks the serial framing on every
/// medium, so the BLE endpoint converts between the two (transport adaptation, no protocol logic).
struct MeshtasticStreamFramer {
  static let start1: UInt8 = 0x94
  static let start2: UInt8 = 0xC3
  static let maxPacket = 512

  private var buf = Data()

  static func frame(_ packet: Data) -> Data {
    var d = Data([start1, start2, UInt8((packet.count >> 8) & 0xFF), UInt8(packet.count & 0xFF)])
    d.append(packet)
    return d
  }

  /// Feed stream bytes (from the core); returns complete packets. Garbage between frames and
  /// frames with an impossible length are skipped.
  mutating func feed(_ data: Data) -> [Data] {
    buf.append(data)
    var out: [Data] = []
    while true {
      guard let i = buf.firstIndex(of: MeshtasticStreamFramer.start1) else { buf.removeAll(); break }
      if i > buf.startIndex { buf.removeSubrange(buf.startIndex..<i) }
      guard buf.count >= 4 else { break }
      let b = [UInt8](buf.prefix(4))
      guard b[1] == MeshtasticStreamFramer.start2 else { buf.removeFirst(); continue }
      let len = Int(b[2]) << 8 | Int(b[3])
      guard len <= MeshtasticStreamFramer.maxPacket else { buf.removeFirst(); continue }
      guard buf.count >= 4 + len else { break }
      out.append(Data(buf.dropFirst(4).prefix(len)))
      buf.removeFirst(4 + len)
    }
    if buf.count > 4096 { buf.removeAll() }  // never grow without bound on garbage
    return out
  }
}

/// Central to a UART-style BLE peripheral. Profiles (GATT services this endpoint supports):
///   nus         Nordic UART 6e400001-… : we write RX 6e400002, the device notifies TX 6e400003 (byte stream)
///   kiss_ble    "BLE KISS API" 00000001-ba2a-46c9-ae49-01b0961f68bb (Mobilinkd TNC3/TNC4): we write
///               00000002-…, the TNC notifies 00000003-… (KISS byte stream, same handling as nus)
///   meshtastic  Meshtastic 6ba1b218-…: bare protobufs over ToRadio/FromRadio/FromNum, adapted here to
///               the serial `0x94 0xC3 len` stream framing so the core's meshtastic link keeps its default
///               framing "stream" (do NOT set framing:"ble" with this endpoint)
///   custom      any service + write + notify characteristic (byte stream)
final class BleUartEndpoint: BaseEndpoint, CBCentralManagerDelegate, CBPeripheralDelegate {
  enum Profile: String { case nus, kiss_ble, meshtastic, custom }

  private let queue = DispatchQueue(label: "org.embermesh.ble.uart")
  private let cfg: EndpointConfig
  let profile: Profile
  private let serviceUUID: CBUUID
  private let rxUUID: CBUUID     // we write
  private let txUUID: CBUUID     // we get notified (NUS/custom) or FromNum (Meshtastic)
  private let deviceId: UUID?
  private var central: CBCentralManager?
  private var peripheral: CBPeripheral?
  private var rx: CBCharacteristic?
  private var fromRadio: CBCharacteristic?
  private var queueOut = ChunkQueue(capBytes: 32 * 1024)
  private var awaiting = false
  private var framer = MeshtasticStreamFramer()
  private var isOpen = false
  private var reading = false

  override var wantsAutoReconnect: Bool { false }  // CoreBluetooth pending connects handle it

  init(config: JSONObject) throws {
    cfg = EndpointConfig(config)
    profile = Profile(rawValue: cfg.string("profile", "nus")) ?? .nus
    switch profile {
    case .nus: serviceUUID = EmberUUID.nusService; rxUUID = EmberUUID.nusRx; txUUID = EmberUUID.nusTx
    case .kiss_ble: serviceUUID = EmberUUID.kissService; rxUUID = EmberUUID.kissRx; txUUID = EmberUUID.kissTx
    case .meshtastic: serviceUUID = EmberUUID.meshService; rxUUID = EmberUUID.meshToRadio; txUUID = EmberUUID.meshFromNum
    case .custom:
      guard let s = BleUartEndpoint.uuid(cfg.string("service_uuid")), let r = BleUartEndpoint.uuid(cfg.string("rx_uuid")),
            let t = BleUartEndpoint.uuid(cfg.string("tx_uuid")) else {
        throw RPCError.badParams("custom profile needs valid service, write and notify UUIDs")
      }
      serviceUUID = s; rxUUID = r; txUUID = t
    }
    deviceId = UUID(uuidString: cfg.string("device_id"))
    if deviceId == nil { throw RPCError.badParams("pick a device by scanning (device_id is not a valid identifier)") }
    super.init(kind: "ble")
  }

  static func uuid(_ s: String) -> CBUUID? {
    let t = s.trimmingCharacters(in: .whitespaces)
    guard !t.isEmpty, t.range(of: "^[0-9A-Fa-f]{4}([0-9A-Fa-f]{4}(-?[0-9A-Fa-f]{4}){3}-?[0-9A-Fa-f]{12})?$",
                              options: .regularExpression) != nil || UUID(uuidString: t) != nil else { return nil }
    return CBUUID(string: t)
  }

  override func open() {
    queue.async {
      guard !self.isOpen else { return }
      self.isOpen = true
      let (ok, reason) = BluetoothAvailability.check()
      if !ok { self.setConnected(false, error: reason); return }
      self.central = CBCentralManager(delegate: self, queue: self.queue, options: [
        CBCentralManagerOptionShowPowerAlertKey: false,
        CBCentralManagerOptionRestoreIdentifierKey: "org.embermesh.ble.uart.\(self.deviceId?.uuidString ?? "x")",
      ])
    }
  }

  override func close() {
    queue.sync {
      isOpen = false
      if let p = peripheral { central?.cancelPeripheralConnection(p) }
      central?.stopScan()
      central?.delegate = nil
      central = nil; peripheral = nil; rx = nil; fromRadio = nil
      queueOut.removeAll()
    }
    setConnected(false)
  }

  override var status: JSONObject {
    queue.sync {
      ["profile": profile.rawValue, "device_id": deviceId?.uuidString ?? "", "device_name": peripheral?.name ?? cfg.string("device_name"),
       "state": peripheral.map { $0.state == .connected ? "connected" : "connecting" } ?? "idle"]
    }
  }

  override func write(_ data: Data) {
    queue.async {
      guard let p = self.peripheral, self.rx != nil else { return }
      if self.profile == .meshtastic {
        for pkt in self.framer.feed(data) { self.queueOut.enqueue(pkt, mtu: ByteChunker.maxChunk) }  // one packet per write
      } else {
        let withResp = self.cfg.bool("write_with_response", false) || !(self.rx!.properties.contains(.writeWithoutResponse))
        self.queueOut.enqueue(data, mtu: p.maximumWriteValueLength(for: withResp ? .withResponse : .withoutResponse))
      }
      self.pump()
    }
  }

  private func pump() {
    guard let p = peripheral, let rx, p.state == .connected else { return }
    let withResp = profile == .meshtastic || cfg.bool("write_with_response", false) || !rx.properties.contains(.writeWithoutResponse)
    while let chunk = queueOut.peek() {
      if withResp {
        if awaiting { return }
        awaiting = true
        p.writeValue(chunk, for: rx, type: .withResponse)
        queueOut.pop()
        return
      }
      guard p.canSendWriteWithoutResponse else { return }
      p.writeValue(chunk, for: rx, type: .withoutResponse)
      queueOut.pop()
    }
  }

  // MARK: central

  func centralManager(_ c: CBCentralManager, willRestoreState dict: [String: Any]) {
    if let p = (dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral])?.first {
      peripheral = p
      p.delegate = self
    }
  }

  func centralManagerDidUpdateState(_ c: CBCentralManager) {
    guard c.state == .poweredOn, isOpen else {
      if c.state == .poweredOff { setConnected(false, error: "Bluetooth is turned off") }
      if c.state == .unauthorized { setConnected(false, error: "Bluetooth permission is off (Settings > EmberMesh)") }
      return
    }
    if let p = peripheral {
      if p.state == .connected { p.discoverServices([serviceUUID]) } else { c.connect(p) }
      return
    }
    if let id = deviceId, let p = c.retrievePeripherals(withIdentifiers: [id]).first {
      peripheral = p
      p.delegate = self
      c.connect(p)
    } else {
      // Not known to this phone yet (e.g. config typed by hand): scan for it.
      c.scanForPeripherals(withServices: profile == .custom ? nil : [serviceUUID], options: nil)
    }
  }

  func centralManager(_ c: CBCentralManager, didDiscover p: CBPeripheral, advertisementData: [String: Any], rssi: NSNumber) {
    guard p.identifier == deviceId else { return }
    c.stopScan()
    peripheral = p
    p.delegate = self
    c.connect(p)
  }

  func centralManager(_ c: CBCentralManager, didConnect p: CBPeripheral) {
    p.discoverServices([serviceUUID])
  }

  func centralManager(_ c: CBCentralManager, didFailToConnect p: CBPeripheral, error: Error?) {
    setConnected(false, error: error?.localizedDescription ?? "connection failed")
    if isOpen { c.connect(p) }
  }

  func centralManager(_ c: CBCentralManager, didDisconnectPeripheral p: CBPeripheral, error: Error?) {
    rx = nil; fromRadio = nil; awaiting = false
    setConnected(false, error: error?.localizedDescription ?? "disconnected")
    if isOpen { c.connect(p) }  // pending connect: reconnects when back in range
  }

  // MARK: peripheral

  func peripheral(_ p: CBPeripheral, didDiscoverServices error: Error?) {
    guard let s = p.services?.first(where: { $0.uuid == serviceUUID }) else {
      setConnected(false, error: "device does not offer the expected Bluetooth service")
      return
    }
    var want = [rxUUID, txUUID]
    if profile == .meshtastic { want.append(EmberUUID.meshFromRadio) }
    p.discoverCharacteristics(want, for: s)
  }

  func peripheral(_ p: CBPeripheral, didDiscoverCharacteristicsFor s: CBService, error: Error?) {
    let chars = s.characteristics ?? []
    rx = chars.first { $0.uuid == rxUUID }
    fromRadio = chars.first { $0.uuid == EmberUUID.meshFromRadio }
    guard rx != nil, let tx = chars.first(where: { $0.uuid == txUUID }) else {
      setConnected(false, error: "device is missing the expected characteristics")
      return
    }
    p.setNotifyValue(true, for: tx)
  }

  func peripheral(_ p: CBPeripheral, didUpdateNotificationStateFor ch: CBCharacteristic, error: Error?) {
    guard ch.uuid == txUUID else { return }
    if let error { setConnected(false, error: error.localizedDescription); return }
    setConnected(true)
    if profile == .meshtastic { drainFromRadio(p) }
    pump()
  }

  private func drainFromRadio(_ p: CBPeripheral) {
    guard let fr = fromRadio, !reading else { return }
    reading = true
    p.readValue(for: fr)
  }

  func peripheral(_ p: CBPeripheral, didUpdateValueFor ch: CBCharacteristic, error: Error?) {
    guard error == nil else { reading = false; return }
    if profile == .meshtastic {
      if ch.uuid == EmberUUID.meshFromNum { drainFromRadio(p); return }
      if ch.uuid == EmberUUID.meshFromRadio {
        reading = false
        guard let v = ch.value, !v.isEmpty else { return }  // empty read = queue drained
        deliver(MeshtasticStreamFramer.frame(v))
        drainFromRadio(p)
      }
      return
    }
    if ch.uuid == txUUID, let v = ch.value { deliver(v) }
  }

  func peripheral(_ p: CBPeripheral, didWriteValueFor ch: CBCharacteristic, error: Error?) {
    awaiting = false
    if profile == .meshtastic { drainFromRadio(p) }
    pump()
  }

  func peripheralIsReady(toSendWriteWithoutResponse p: CBPeripheral) { pump() }
}

/// One-shot BLE scan used by host.endpoints.scan for `ble` (TNCs / radios) and `ble_peer`.
final class BleScanner: NSObject, CBCentralManagerDelegate {
  private let queue = DispatchQueue(label: "org.embermesh.ble.scan")
  private var central: CBCentralManager?
  private var found: [UUID: JSONObject] = [:]
  private let services: [CBUUID]?
  private let mapper: (CBPeripheral, [String: Any], NSNumber) -> JSONObject?
  private var completion: (([JSONObject], String?) -> Void)?
  private let timeout: TimeInterval

  init(services: [CBUUID]?, timeout: TimeInterval, mapper: @escaping (CBPeripheral, [String: Any], NSNumber) -> JSONObject?) {
    self.services = services
    self.timeout = timeout
    self.mapper = mapper
  }

  func run(_ completion: @escaping ([JSONObject], String?) -> Void) {
    let (ok, reason) = BluetoothAvailability.check()
    guard ok else { completion([], reason); return }
    queue.async {
      self.completion = completion
      self.central = CBCentralManager(delegate: self, queue: self.queue, options: [CBCentralManagerOptionShowPowerAlertKey: true])
      self.queue.asyncAfter(deadline: .now() + self.timeout) { self.finish(nil) }
    }
  }

  private func finish(_ error: String?) {
    guard let c = completion else { return }
    completion = nil
    central?.stopScan()
    central = nil
    let list = Array(found.values).sorted { ($0["rssi"] as? Int ?? -200) > ($1["rssi"] as? Int ?? -200) }
    c(list, error)
    withExtendedLifetime(self) {}
  }

  func centralManagerDidUpdateState(_ c: CBCentralManager) {
    switch c.state {
    case .poweredOn: c.scanForPeripherals(withServices: services, options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    case .poweredOff: finish("Bluetooth is turned off")
    case .unauthorized: finish("Bluetooth permission is off (Settings > EmberMesh)")
    case .unsupported: finish("Bluetooth LE is not supported on this device")
    default: break
    }
  }

  func centralManager(_ c: CBCentralManager, didDiscover p: CBPeripheral, advertisementData: [String: Any], rssi: NSNumber) {
    if let d = mapper(p, advertisementData, rssi) { found[p.identifier] = d }
  }

  /// Mapper for TNCs / radios: guesses the profile from advertised services.
  static func uartMapper(_ p: CBPeripheral, _ adv: [String: Any], _ rssi: NSNumber) -> JSONObject? {
    let uuids = (adv[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID]) ?? []
    let name = (adv[CBAdvertisementDataLocalNameKey] as? String) ?? p.name
    let profile: String
    var linkType = "kiss"
    if uuids.contains(EmberUUID.nusService) { profile = "nus" }
    else if uuids.contains(EmberUUID.kissService) { profile = "kiss_ble" }
    else if uuids.contains(EmberUUID.meshService) { profile = "meshtastic"; linkType = "meshtastic" }
    else if uuids.contains(EmberUUID.peerService) { return nil }  // EmberMesh phones belong to ble_peer
    else if name != nil { profile = "custom" } else { return nil }
    let display = name ?? "Unnamed device"
    return ["id": p.identifier.uuidString, "name": display, "rssi": rssi.intValue,
            "detail": ["custom": "unknown service — set UUIDs", "nus": "Nordic UART", "kiss_ble": "BLE KISS TNC"][profile] ?? "Meshtastic",
            "config": ["device_id": p.identifier.uuidString, "device_name": display, "profile": profile],
            "link_type": linkType]
  }
}
