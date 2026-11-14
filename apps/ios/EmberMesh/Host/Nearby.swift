import Foundation

// Nearby devices (docs/CONNECTIVITY.md §3, RPC_API §11 "Host side"): pure logic, unit-tested with a
// fake clock. The Network.framework side is in Endpoints/NearbyNetwork.swift, the glue in
// Host/ConnectivityController.swift.

enum NearbyTXT {
  static let serviceType = "_embermesh._tcp"
  static let instancePrefix = "EmberMesh-"

  /// Exactly these four keys are advertised; nothing else (no device or user name, node id, model).
  static func record(token: String) -> [String: String] {
    ["ember": "1", "v": "1", "link": "stream", "t": token]
  }

  /// 8 lowercase hex digits.
  static func isToken(_ s: String) -> Bool {
    s.utf8.count == 8 && s.utf8.allSatisfy { (48...57).contains($0) || (97...102).contains($0) }
  }

  /// The peer token of a discovered instance, or nil when the record is not ours or is hostile:
  /// it must carry `ember=1` and `v=1`, and `t` must be a token. The token is taken from `t`, never
  /// from the instance name. Oversized records (more than 16 keys, a value over 64 bytes, more
  /// than 512 bytes in total) are ignored entirely.
  static func peerToken(_ txt: [String: String]) -> String? {
    guard txt.count <= 16 else { return nil }
    var total = 0
    for (k, v) in txt {
      guard k.utf8.count <= 32, v.utf8.count <= 64 else { return nil }
      total += k.utf8.count + v.utf8.count + 2
    }
    guard total <= 512 else { return nil }
    guard txt["ember"] == "1", txt["v"] == "1", let t = txt["t"], isToken(t) else { return nil }
    return t
  }
}

/// On-link / private address rules (CONNECTIVITY rev 2 §3.5): who may connect, whom we dial, and the
/// per-source grouping (an IPv4 address, or an IPv6 /64).
enum LocalNet {
  /// Strips a zone ("fe80::1%en0") and IPv4-mapped prefixes; lowercase.
  static func normalize(_ s: String) -> String { NearbyNetwork.normalizeIP(s) }

  static func ipv4(_ s: String) -> [UInt8]? {
    var a = in_addr()
    return inet_pton(AF_INET, s, &a) == 1 ? withUnsafeBytes(of: a.s_addr) { Array($0) } : nil
  }
  static func ipv6(_ s: String) -> [UInt8]? {
    var a = in6_addr()
    return inet_pton(AF_INET6, s, &a) == 1 ? withUnsafeBytes(of: a) { Array($0) } : nil
  }

  /// RFC 1918, IPv4 link-local, IPv6 link-local fe80::/10 and unique-local fc00::/7. Not loopback,
  /// not CGNAT, not global.
  static func isPrivateOrLinkLocal(_ raw: String) -> Bool {
    let s = normalize(raw)
    if let b = ipv4(s) {
      return b[0] == 10 || (b[0] == 172 && (16...31).contains(b[1])) || (b[0] == 192 && b[1] == 168) || (b[0] == 169 && b[1] == 254)
    }
    if let b = ipv6(s) { return (b[0] == 0xfe && b[1] & 0xc0 == 0x80) || (b[0] & 0xfe == 0xfc) }
    return false
  }

  /// The per-source key for limits: the IPv4 address, or the IPv6 /64 prefix.
  static func sourceKey(_ raw: String) -> String {
    let s = normalize(raw)
    if let b = ipv6(s), ipv4(s) == nil {
      return b[0..<8].map { String(format: "%02x", $0) }.joined() + "::/64"
    }
    return s
  }

  /// Local interface networks (address, mask) of up, non-loopback interfaces.
  static func interfaceNets() -> [([UInt8], [UInt8])] {
    var out: [([UInt8], [UInt8])] = []
    var ifa: UnsafeMutablePointer<ifaddrs>?
    guard getifaddrs(&ifa) == 0, let first = ifa else { return out }
    defer { freeifaddrs(ifa) }
    var p: UnsafeMutablePointer<ifaddrs>? = first
    while let i = p {
      defer { p = i.pointee.ifa_next }
      let f = Int32(i.pointee.ifa_flags)
      guard f & IFF_UP != 0, f & IFF_LOOPBACK == 0, let a = i.pointee.ifa_addr, let m = i.pointee.ifa_netmask else { continue }
      if a.pointee.sa_family == UInt8(AF_INET) {
        let ab = a.withMemoryRebound(to: sockaddr_in.self, capacity: 1) { withUnsafeBytes(of: $0.pointee.sin_addr) { Array($0) } }
        let mb = m.withMemoryRebound(to: sockaddr_in.self, capacity: 1) { withUnsafeBytes(of: $0.pointee.sin_addr) { Array($0) } }
        out.append((ab, mb))
      } else if a.pointee.sa_family == UInt8(AF_INET6) {
        let ab = a.withMemoryRebound(to: sockaddr_in6.self, capacity: 1) { withUnsafeBytes(of: $0.pointee.sin6_addr) { Array($0) } }
        let mb = m.withMemoryRebound(to: sockaddr_in6.self, capacity: 1) { withUnsafeBytes(of: $0.pointee.sin6_addr) { Array($0) } }
        out.append((ab, mb))
      }
    }
    return out
  }

  /// Dialable: private/link-local AND (IPv6 link-local, or inside the subnet of a local interface).
  static func isOnLink(_ raw: String, nets: [([UInt8], [UInt8])] = interfaceNets()) -> Bool {
    let s = normalize(raw)
    guard isPrivateOrLinkLocal(s) else { return false }
    let b = ipv4(s) ?? ipv6(s) ?? []
    if b.count == 16, b[0] == 0xfe, b[1] & 0xc0 == 0x80 { return true }
    return nets.contains { a, m in a.count == b.count && zip(zip(a, m), b).allSatisfy { ($0.0 & $0.1) == ($1 & $0.1) } }
  }

  /// Dial order: IPv6 link-local first, then IPv4 (then the rest), at most `limit`.
  static func dialOrder(_ addrs: [String], limit: Int = 4, nets: [([UInt8], [UInt8])] = interfaceNets()) -> [String] {
    var seen = Set<String>()
    let ok = addrs.filter { isOnLink($0, nets: nets) && seen.insert(normalize($0)).inserted }
    func rank(_ a: String) -> Int {
      let s = normalize(a)
      if s.hasPrefix("fe8") || s.hasPrefix("fe9") || s.hasPrefix("fea") || s.hasPrefix("feb") { return 0 }
      return ipv4(s) != nil ? 1 : 2
    }
    return Array(ok.sorted { rank($0) < rank($1) }.prefix(limit))
  }
}

struct NearbyLimits {
  var maxLinks = 16                  // nearby links in total (the core enforces it too)
  var maxConnectionsPerIP = 2        // simultaneous connections per source (IPv4 address or IPv6 /64)
  var maxInboundPerIPPerMinute = 4   // new connections accepted per source per minute
  var maxDialsPerMinute = 4          // new outgoing nearby dials per minute
  var fallbackDialAfterMs: Double = 8000  // the "waiting" side dials if nothing arrived by then
}

/// The dialing rule and the limits (CONNECTIVITY §3.3, §3.5). Every node listens and browses; for a
/// discovered token T and own token M the node with the lower token dials; the other side dials too
/// if no inbound connection from that peer appeared within 8 s (one-sided discovery). One dial per
/// token at a time. Times are milliseconds on any monotonic clock. Not thread-safe: main queue.
final class NearbyPolicy {
  struct Peer {
    var discoveredMs: Double
    var addresses: Set<String> = []
    var dialing = false
    var linked = false        // a link to this peer (dialed or attributed inbound) is up or being made
    var inboundSeen = false
    var waiting = false       // dial blocked by the per-minute limit: retried by tick()
  }

  let myToken: String
  var limits: NearbyLimits
  private(set) var peers: [String: Peer] = [:]
  private var dialTimes: [Double] = []
  private var inboundTimes: [String: [Double]] = [:]
  private(set) var connectionsPerIP: [String: Int] = [:]
  private(set) var links = 0

  init(myToken: String, limits: NearbyLimits = NearbyLimits()) {
    self.myToken = myToken
    self.limits = limits
  }

  /// Tokens to dial now.
  func discovered(_ token: String, now: Double) -> [String] {
    guard token != myToken, NearbyTXT.isToken(token) else { return [] }
    if peers[token] == nil { peers[token] = Peer(discoveredMs: now) }
    return myToken < token ? tryDial(token, now: now) : []
  }

  func lost(_ token: String) {
    guard var p = peers[token] else { return }
    if !p.linked && !p.dialing { peers[token] = nil; return }
    p.waiting = false
    peers[token] = p
  }

  func resolved(_ token: String, addresses: [String]) {
    peers[token]?.addresses.formUnion(addresses.map(LocalNet.normalize))
  }

  /// Periodic: fallback dials of the waiting side and dials deferred by the rate limit.
  func tick(now: Double) -> [String] {
    var out: [String] = []
    for (t, p) in peers.sorted(by: { $0.key < $1.key }) where !p.linked && !p.dialing {
      let lowerDials = myToken < t
      let fallbackDue = !lowerDials && !p.inboundSeen && now - p.discoveredMs >= limits.fallbackDialAfterMs
      if (lowerDials && p.waiting) || fallbackDue { out += tryDial(t, now: now) }
    }
    return out
  }

  private func tryDial(_ token: String, now: Double) -> [String] {
    guard var p = peers[token], !p.dialing, !p.linked else { return [] }
    dialTimes.removeAll { now - $0 >= 60_000 }
    // The 16-link cap is the core's (rev 2: it evicts an unproven link or refuses with busy).
    guard dialTimes.count < limits.maxDialsPerMinute else {
      p.waiting = true
      peers[token] = p
      return []
    }
    dialTimes.append(now)
    p.dialing = true
    p.waiting = false
    peers[token] = p
    return [token]
  }

  /// The dial to `token` connected (true) or failed (false).
  func dialFinished(_ token: String, connected: Bool, remoteIP: String?) {
    guard var p = peers[token] else { return }
    p.dialing = false
    if connected {
      p.linked = true
      links += 1
      if let ip = remoteIP { connectionsPerIP[LocalNet.sourceKey(ip), default: 0] += 1; p.addresses.insert(LocalNet.normalize(ip)) }
    }
    peers[token] = p
  }

  /// May an inbound connection from `ip` be accepted? Only on-link/private sources; limits per
  /// source (IPv4 address or IPv6 /64). Records the attempt either way.
  func shouldAccept(ip: String, now: Double) -> Bool {
    guard LocalNet.isPrivateOrLinkLocal(ip) else { return false }
    let key = LocalNet.sourceKey(ip)
    var times = inboundTimes[key, default: []].filter { now - $0 < 60_000 }
    defer { inboundTimes[key] = times }
    guard times.count < limits.maxInboundPerIPPerMinute else { return false }
    times.append(now)
    return connectionsPerIP[key, default: 0] < limits.maxConnectionsPerIP
  }

  /// An inbound connection from `ip` became a link. Returns the peer token it is attributed to (by
  /// resolved address), which also cancels that peer's fallback dial.
  @discardableResult
  func accepted(ip: String) -> String? {
    connectionsPerIP[LocalNet.sourceKey(ip), default: 0] += 1
    links += 1
    let n = LocalNet.normalize(ip)
    guard let (t, _) = peers.first(where: { $0.value.addresses.contains(n) && !$0.value.inboundSeen }) else { return nil }
    peers[t]?.inboundSeen = true
    return t
  }

  /// A nearby link closed (either role). The peer may be dialed again (rate limits apply).
  func closed(token: String?, ip: String?) {
    links = max(0, links - 1)
    if let ip { let k = LocalNet.sourceKey(ip); if let n = connectionsPerIP[k] { connectionsPerIP[k] = n > 1 ? n - 1 : nil } }
    if let token, var p = peers[token] {
      p.linked = false
      p.dialing = false
      peers[token] = p
    }
  }
}
