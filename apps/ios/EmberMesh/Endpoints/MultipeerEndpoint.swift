import MultipeerConnectivity
import UIKit

/// MultipeerConnectivity (service type "embermesh", ARCHITECTURE §6): iPhone ↔ iPhone/iPad/Mac over
/// peer-to-peer Wi-Fi and Bluetooth with no access point. Each core write is one reliable message
/// to every connected peer, so message boundaries survive and peers never interleave.
/// Limits: MCSession holds at most 8 peers (incl. us); advertising stops in the background.
final class MultipeerEndpoint: BaseEndpoint, MCSessionDelegate, MCNearbyServiceAdvertiserDelegate, MCNearbyServiceBrowserDelegate {
  static let serviceType = "embermesh"
  private static let peerIdKey = "ember.multipeer.peerid"

  private let queue = DispatchQueue(label: "org.embermesh.multipeer")
  private let cfg: EndpointConfig
  private var peerID: MCPeerID!
  private var session: MCSession?
  private var advertiser: MCNearbyServiceAdvertiser?
  private var browser: MCNearbyServiceBrowser?
  /// Random per-launch token: of two peers that find each other, only the lower token invites.
  private let token = UUID().uuidString
  private var found: [MCPeerID: (token: String?, seen: Date)] = [:]
  private var isOpen = false

  init(config: JSONObject) {
    cfg = EndpointConfig(config)
    super.init(kind: "multipeer")
  }

  var maxPeers: Int { max(1, min(7, cfg.int("max_peers", 7))) }

  /// A stable MCPeerID across launches (Apple's recommendation) unless the name changes.
  static func loadPeerID(displayName: String) -> MCPeerID {
    let name = String(displayName.prefix(60)).isEmpty ? "Ember" : String(displayName.prefix(60))
    let d = UserDefaults.standard
    if let data = d.data(forKey: peerIdKey),
       let p = try? NSKeyedUnarchiver.unarchivedObject(ofClass: MCPeerID.self, from: data), p.displayName == name {
      return p
    }
    let p = MCPeerID(displayName: name)
    if let data = try? NSKeyedArchiver.archivedData(withRootObject: p, requiringSecureCoding: true) { d.set(data, forKey: peerIdKey) }
    return p
  }

  override func open() {
    let name = DefaultPeerName.resolve(cfg.string("display_name"))
    queue.async {
      guard !self.isOpen else { return }
      self.isOpen = true
      self.peerID = MultipeerEndpoint.loadPeerID(displayName: name)
      let enc: MCEncryptionPreference
      switch self.cfg.string("encryption", "required") {
      case "none": enc = .none
      case "optional": enc = .optional
      default: enc = .required
      }
      let s = MCSession(peer: self.peerID, securityIdentity: nil, encryptionPreference: enc)
      s.delegate = self
      self.session = s
      let adv = MCNearbyServiceAdvertiser(peer: self.peerID, discoveryInfo: ["t": self.token, "v": "1"], serviceType: MultipeerEndpoint.serviceType)
      adv.delegate = self
      adv.startAdvertisingPeer()
      self.advertiser = adv
      if self.cfg.bool("auto_connect", true) {
        let b = MCNearbyServiceBrowser(peer: self.peerID, serviceType: MultipeerEndpoint.serviceType)
        b.delegate = self
        b.startBrowsingForPeers()
        self.browser = b
      }
      self.setConnected(true)  // the medium is usable; peers come and go
    }
  }

  override func close() {
    queue.sync {
      isOpen = false
      advertiser?.stopAdvertisingPeer(); advertiser?.delegate = nil; advertiser = nil
      browser?.stopBrowsingForPeers(); browser?.delegate = nil; browser = nil
      session?.disconnect(); session?.delegate = nil; session = nil
      found.removeAll()
    }
    setConnected(false)
  }

  override var status: JSONObject {
    queue.sync { ["peers": session?.connectedPeers.count ?? 0, "names": session?.connectedPeers.map { $0.displayName } ?? []] }
  }

  func nearby() -> [JSONObject] {
    queue.sync {
      found.map { p, v in
        ["id": p.displayName, "name": p.displayName,
         "detail": session?.connectedPeers.contains(p) == true ? "connected" : "nearby", "config": [String: Any]()]
      }
    }
  }

  override func write(_ data: Data) {
    queue.async {
      guard let s = self.session, !s.connectedPeers.isEmpty, !data.isEmpty else { return }
      do { try s.send(data, toPeers: s.connectedPeers, with: .reliable) } catch {
        emberLog("multipeer send failed: \(error.localizedDescription)")
      }
    }
  }

  private func shouldInvite(_ peer: MCPeerID) -> Bool {
    guard let s = session, isOpen, !s.connectedPeers.contains(peer), s.connectedPeers.count < maxPeers else { return false }
    guard let theirs = found[peer]?.token else { return true }
    return token < theirs
  }

  // MARK: browser

  func browser(_ b: MCNearbyServiceBrowser, foundPeer peer: MCPeerID, withDiscoveryInfo info: [String: String]?) {
    queue.async {
      self.found[peer] = (info?["t"], Date())
      if self.shouldInvite(peer), let s = self.session { b.invitePeer(peer, to: s, withContext: nil, timeout: 20) }
      self.statusChanged()
    }
  }

  func browser(_ b: MCNearbyServiceBrowser, lostPeer peer: MCPeerID) {
    queue.async { self.found[peer] = nil; self.statusChanged() }
  }

  func browser(_ b: MCNearbyServiceBrowser, didNotStartBrowsingForPeers error: Error) {
    emberLog("multipeer browse failed: \(error.localizedDescription)")
    setConnected(false, error: "Local network permission is needed (Settings > EmberMesh > Local Network)")
  }

  // MARK: advertiser

  func advertiser(_ a: MCNearbyServiceAdvertiser, didReceiveInvitationFromPeer peer: MCPeerID, withContext context: Data?,
                  invitationHandler: @escaping (Bool, MCSession?) -> Void) {
    queue.async {
      guard let s = self.session, self.isOpen, s.connectedPeers.count < self.maxPeers else { invitationHandler(false, nil); return }
      invitationHandler(true, s)
    }
  }

  func advertiser(_ a: MCNearbyServiceAdvertiser, didNotStartAdvertisingPeer error: Error) {
    emberLog("multipeer advertise failed: \(error.localizedDescription)")
    setConnected(false, error: "Local network permission is needed (Settings > EmberMesh > Local Network)")
  }

  // MARK: session

  func session(_ s: MCSession, peer: MCPeerID, didChange state: MCSessionState) {
    queue.async {
      emberLog("multipeer \(peer.displayName): \(state.rawValue)")
      if state == .notConnected, self.isOpen, self.found[peer] != nil, self.shouldInvite(peer) {
        self.browser?.invitePeer(peer, to: s, withContext: nil, timeout: 20)
      }
      self.statusChanged()
    }
  }

  func session(_ s: MCSession, didReceive data: Data, fromPeer peer: MCPeerID) { deliver(data) }
  func session(_ s: MCSession, didReceive stream: InputStream, withName: String, fromPeer: MCPeerID) { stream.close() }
  func session(_ s: MCSession, didStartReceivingResourceWithName: String, fromPeer: MCPeerID, with progress: Progress) {}
  func session(_ s: MCSession, didFinishReceivingResourceWithName: String, fromPeer: MCPeerID, at: URL?, withError: Error?) {}
  func session(_ s: MCSession, didReceiveCertificate certificate: [Any]?, fromPeer peer: MCPeerID,
               certificateHandler: @escaping (Bool) -> Void) { certificateHandler(true) }
}

/// The name this phone shows to strangers nearby (Multipeer peer name, Bonjour instance name) when
/// the user has not chosen one: "EmberMesh-" + 4 random hex digits, generated once and kept, so the
/// peer stays recognisable across launches without revealing the owner's device name
/// (UIDevice.name is "<Owner>'s iPhone" on iOS 15). An explicit name in the link config wins.
enum DefaultPeerName {
  static let prefix = "EmberMesh-"
  private static let suffixKey = "ember.default_peer_name.suffix"

  static var current: String { name(defaults: .standard) }

  static func name(defaults: UserDefaults) -> String { prefix + suffix(defaults: defaults) }

  static func suffix(defaults: UserDefaults) -> String {
    if let s = defaults.string(forKey: suffixKey), isValidSuffix(s) { return s }
    let s = String(format: "%04X", UInt16.random(in: .min ... .max))
    defaults.set(s, forKey: suffixKey)
    return s
  }

  static func isValidSuffix(_ s: String) -> Bool {
    s.count == 4 && s.allSatisfy { $0.isHexDigit && !$0.isLowercase }
  }

  /// `configured` if set (trimmed), else the anonymous default.
  static func resolve(_ configured: String, defaults: UserDefaults = .standard) -> String {
    let t = configured.trimmingCharacters(in: .whitespacesAndNewlines)
    return t.isEmpty ? name(defaults: defaults) : t
  }
}
