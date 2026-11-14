import Foundation

/// Field schema for schema-driven forms (docs/RPC_API.md §1 "Field schema").
struct Field {
  enum Kind: String, CaseIterable { case number, text, select, bool, textarea, password }
  var key: String
  var label: String
  var type: Kind
  var defaultValue: Any? = nil
  var unit: String? = nil
  var min: Double? = nil
  var max: Double? = nil
  var step: Double? = nil
  var options: [(value: String, label: String)] = []
  var help: String? = nil
  var required = false
  var advanced = false

  var json: JSONObject {
    var o: JSONObject = ["key": key, "label": label, "type": type.rawValue, "required": required, "advanced": advanced]
    if let d = defaultValue { o["default"] = d }
    if let unit { o["unit"] = unit }
    if let min { o["min"] = min }
    if let max { o["max"] = max }
    if let step { o["step"] = step }
    if !options.isEmpty { o["options"] = options.map { ["value": $0.value, "label": $0.label] } }
    if let help { o["help"] = help }
    return o
  }

  static func text(_ key: String, _ label: String, _ def: String = "", help: String? = nil, required: Bool = false, advanced: Bool = false) -> Field {
    Field(key: key, label: label, type: .text, defaultValue: def, help: help, required: required, advanced: advanced)
  }
  static func number(_ key: String, _ label: String, _ def: Double, min: Double? = nil, max: Double? = nil, step: Double? = 1,
                     unit: String? = nil, help: String? = nil, required: Bool = false, advanced: Bool = false) -> Field {
    Field(key: key, label: label, type: .number, defaultValue: def, unit: unit, min: min, max: max, step: step, help: help,
          required: required, advanced: advanced)
  }
  static func bool(_ key: String, _ label: String, _ def: Bool, help: String? = nil, advanced: Bool = false) -> Field {
    Field(key: key, label: label, type: .bool, defaultValue: def, help: help, advanced: advanced)
  }
  static func select(_ key: String, _ label: String, _ def: String, _ options: [(String, String)], help: String? = nil,
                     advanced: Bool = false) -> Field {
    Field(key: key, label: label, type: .select, defaultValue: def, options: options.map { (value: $0.0, label: $0.1) },
          help: help, advanced: advanced)
  }
}

/// Static description of one endpoint kind (availability is decided at runtime).
struct EndpointKindSpec {
  let kind: String
  let title: String
  let description: String
  let scannable: Bool
  let config: [Field]
  /// Fallback only: at runtime the compatible link types come from the core's `links.types`
  /// (`endpoint_kinds`), see HostRouter.linkTypes(for:). Mirrors core/src/link/README.md.
  let linkTypes: [String]
  /// Preferred default; used when it is among the compatible types, otherwise the first one is.
  let preferredLinkType: String

  func defaultLinkType(_ compatible: [String]) -> String {
    compatible.contains(preferredLinkType) ? preferredLinkType : (compatible.first ?? preferredLinkType)
  }

  func json(available: Bool, reason: String?, linkTypes compatible: [String]? = nil) -> JSONObject {
    let lt = compatible ?? linkTypes
    var o: JSONObject = [
      "kind": kind, "title": title, "description": description, "available": available, "scannable": scannable && available,
      "config": config.map { $0.json }, "link_types": lt, "default_link_type": defaultLinkType(lt),
    ]
    if let reason, !available { o["unavailable_reason"] = reason }
    return o
  }

  /// Fills defaults for keys the form did not send. Unknown keys are kept (forward compatible).
  func applyDefaults(_ cfg: JSONObject) -> JSONObject {
    var out = cfg
    for f in config where out[f.key] == nil || out[f.key] is NSNull {
      if let d = f.defaultValue { out[f.key] = d }
    }
    return out
  }

  /// Required fields must be present and non-empty.
  func validate(_ cfg: JSONObject) -> RPCError? {
    for f in config where f.required {
      let v = cfg[f.key]
      if v == nil || v is NSNull || (v as? String)?.trimmingCharacters(in: .whitespaces).isEmpty == true {
        return .badParams("\(f.label) is required")
      }
    }
    return nil
  }
}

/// Every endpoint kind of ARCHITECTURE.md §6, as the iOS app implements (or cannot implement) it.
enum EndpointCatalog {
  static let all: [EndpointKindSpec] = [
    EndpointKindSpec(
      kind: "ble_peer", title: "Bluetooth (nearby phones)",
      description: "Chat with nearby iPhones and Android phones over Bluetooth. No Wi-Fi, SIM or internet needed. Range is usually 10–50 m.",
      scannable: true,
      config: [
        .text("local_name", "Name shown to nearby phones", "", help: "Leave empty to use \"Ember\"."),
        .bool("advertise", "Let other phones find me", true),
        .bool("scan", "Look for other phones", true),
        .number("max_peers", "Maximum phones at once", 7, min: 1, max: 15, help: "More phones use more battery.", advanced: true),
      ],
      linkTypes: ["stream"], preferredLinkType: "stream"),
    EndpointKindSpec(
      kind: "multipeer", title: "Apple peer-to-peer (iPhone, iPad, Mac)",
      description: "Connects directly to nearby Apple devices over peer-to-peer Wi-Fi and Bluetooth, without a Wi-Fi network. Faster than Bluetooth. App must be open.",
      scannable: true,
      config: [
        .text("display_name", "Name shown to nearby devices", "", help: "Leave empty to use an anonymous name such as \"EmberMesh-3F2A\"."),
        .bool("auto_connect", "Connect automatically to every EmberMesh device", true),
        .number("max_peers", "Maximum devices at once", 7, min: 1, max: 7, advanced: true),
        .select("encryption", "Transport encryption", "required",
                [("required", "Required"), ("optional", "Optional"), ("none", "None (faster; messages are still end-to-end encrypted)")],
                advanced: true),
      ],
      linkTypes: ["raw"], preferredLinkType: "raw"),
    EndpointKindSpec(
      kind: "audio", title: "Sound (radio cable or speaker)",
      description: "Sends messages as sound: plug a radio into a headset-jack adapter (TRRS) or a USB-C / Lightning audio interface, or hold the phone next to a radio. iPhone cannot key a radio's PTT line: use the radio's VOX, or press PTT yourself (acoustic).",
      scannable: true,
      config: [
        .select("input", "Listen with", "auto",
                [("auto", "Automatic (cable / USB if plugged in)"), ("wired", "Headset jack adapter (TRRS)"),
                 ("usb", "USB-C / Lightning audio interface"), ("builtin", "Built-in microphone (acoustic)"),
                 ("bluetooth", "Bluetooth headset — not suitable for data (8/16 kHz, voice processing)")]),
        .select("output", "Play through", "auto",
                [("auto", "Automatic (cable if plugged in)"), ("speaker", "Loudspeaker (acoustic)"), ("receiver", "Earpiece")]),
        .select("ptt", "Transmitter keying (PTT)", "vox",
                [("vox", "VOX on the radio (the radio keys itself on our audio)"),
                 ("manual", "I press PTT myself (acoustic / hand-held)")],
                help: "iPhone and iPad have no serial or GPIO lines, so a cable cannot key PTT. Turn VOX on in the radio's menu, or hold PTT while sending."),
        .number("tx_level", "Transmit volume", 80, min: 1, max: 100, step: 1, unit: "%",
                help: "Lower this if the radio distorts; raise it if the other side cannot decode."),
        .select("sample_rate", "Sample rate", "48000",
                [("48000", "48 kHz"), ("44100", "44.1 kHz"), ("16000", "16 kHz"), ("8000", "8 kHz")],
                help: "The hardware may pick a different rate; the real rate is reported to the modem.", advanced: true),
        .text("input_uid", "Specific input device", "", help: "Filled in by scanning.", advanced: true),
      ],
      linkTypes: ["audio"], preferredLinkType: "audio"),
    EndpointKindSpec(
      kind: "tcp", title: "TCP connection",
      description: "Connect to a computer, a KISS TNC server (e.g. Direwolf on port 8001) or another EmberMesh device on the same network.",
      scannable: true,
      config: [
        .text("host", "Address", "", help: "IP address or name, e.g. 192.168.4.1", required: true),
        .number("port", "Port", 8001, min: 1, max: 65535, required: true),
        .text("bonjour", "Bonjour service name", "", help: "Filled in by scanning; used instead of the address when set.", advanced: true),
      ],
      linkTypes: ["stream", "kiss", "meshtastic", "rnode", "lora_at", "rtl_tcp"], preferredLinkType: "stream"),
    EndpointKindSpec(
      kind: "tcp_server", title: "Local network listener",
      description: "Let other EmberMesh devices on the same Wi-Fi, hotspot or Apple peer-to-peer Wi-Fi connect to this phone. Found automatically with Bonjour.",
      scannable: false,
      config: [
        .number("port", "Port", 47475, min: 0, max: 65535, help: "0 picks a free port automatically."),
        .bool("advertise", "Announce on the network (Bonjour)", true),
        .bool("peer_to_peer", "Also accept Apple peer-to-peer Wi-Fi", true, advanced: true),
      ],
      linkTypes: ["stream"], preferredLinkType: "stream"),
    EndpointKindSpec(
      kind: "websocket", title: "Hub (WebSocket)",
      description: "Join a hub: a computer or phone running EmberMesh that shares its Wi-Fi hotspot.",
      scannable: false,
      config: [
        .text("url", "Hub address", "ws://192.168.4.1:8437/ws", help: "Shown on the hub's screen, e.g. ws://10.0.0.2:8437/ws", required: true),
      ],
      linkTypes: ["raw", "relay"], preferredLinkType: "raw"),
    EndpointKindSpec(
      kind: "udp", title: "Wi-Fi network broadcast (UDP)",
      description: "Talk to every EmberMesh device on the same Wi-Fi or hotspot at once.",
      scannable: false,
      config: [
        .text("group", "Multicast group", "239.83.77.69", advanced: true),
        .number("port", "Port", 47474, min: 1, max: 65535, advanced: true),
      ],
      linkTypes: ["raw"], preferredLinkType: "raw"),
    EndpointKindSpec(
      kind: "ble", title: "Bluetooth radio modem / TNC",
      description: "Connect to a Bluetooth LE TNC, Meshtastic radio or serial bridge (Nordic UART style).",
      scannable: true,
      config: [
        .text("device_id", "Device", "", help: "Pick a device by scanning.", required: true),
        .text("device_name", "Device name", "", advanced: true),
        .select("profile", "Device type", "nus",
                [("nus", "Serial bridge / KISS TNC (Nordic UART)"), ("kiss_ble", "KISS TNC with the BLE KISS service (Mobilinkd TNC3/TNC4)"),
                 ("meshtastic", "Meshtastic radio"), ("custom", "Custom UUIDs")],
                help: "Meshtastic radios are adapted to the serial framing the meshtastic link expects; leave its framing at \"stream\"."),
        .text("service_uuid", "Service UUID", "", advanced: true),
        .text("rx_uuid", "Write characteristic UUID", "", advanced: true),
        .text("tx_uuid", "Notify characteristic UUID", "", advanced: true),
        .bool("write_with_response", "Wait for acknowledgement on every write", false, advanced: true),
      ],
      linkTypes: ["stream", "kiss", "meshtastic", "rnode", "lora_at"], preferredLinkType: "kiss"),
    EndpointKindSpec(
      kind: "rtl_tcp", title: "SDR receiver (rtl_tcp)",
      description: "Receive with an RTL-SDR dongle shared over the network by an rtl_tcp server.",
      scannable: false,
      config: [
        .text("host", "Address", "", required: true),
        .number("port", "Port", 1234, min: 1, max: 65535),
      ],
      linkTypes: ["rtl_tcp"], preferredLinkType: "rtl_tcp"),
    EndpointKindSpec(
      kind: "loopback", title: "Loopback (testing)",
      description: "Echoes everything back to this phone. For testing only.",
      scannable: false, config: [], linkTypes: ["raw"], preferredLinkType: "raw"),
    EndpointKindSpec(
      kind: "serial", title: "USB serial cable",
      description: "USB serial adapters and radios with USB serial ports.",
      scannable: false, config: [], linkTypes: ["stream", "kiss", "meshtastic", "rnode", "lora_at"], preferredLinkType: "kiss"),
    EndpointKindSpec(
      kind: "bt_spp", title: "Classic Bluetooth serial (SPP)",
      description: "Bluetooth Classic serial devices.", scannable: false, config: [],
      linkTypes: ["stream", "kiss", "rnode", "lora_at"], preferredLinkType: "kiss"),
    EndpointKindSpec(
      kind: "wifi_direct", title: "Wi-Fi Direct", description: "Android Wi-Fi Direct groups.", scannable: false,
      config: [], linkTypes: ["raw"], preferredLinkType: "raw"),
    EndpointKindSpec(
      kind: "webrtc", title: "WebRTC (paired by QR)", description: "Browser-style peer connection paired by QR code.",
      scannable: false, config: [], linkTypes: ["raw"], preferredLinkType: "raw"),
    EndpointKindSpec(
      kind: "ws_server", title: "Hub (Wi-Fi guests)",
      description: "Serve the web app on port 8437 so phones and laptops on this phone's Personal Hotspot or Wi-Fi join the mesh from a browser, with no install. The app must stay open.",
      scannable: false,
      config: [
        .number("port", "Port", 8437, min: 1024, max: 65535),
      ],
      linkTypes: ["raw"], preferredLinkType: "raw"),
  ]

  /// Kinds that can never work in a third-party iOS app, with a reason for non-technical users.
  static let permanentlyUnsupported: [String: String] = [
    "serial": "iPhone and iPad apps cannot use USB serial adapters. Use a Bluetooth LE TNC or serial bridge (\"Bluetooth radio modem / TNC\") or an audio cable (\"Sound\") instead.",
    "bt_spp": "iOS only allows Classic Bluetooth serial for MFi-certified accessories. Use a Bluetooth LE TNC (\"Bluetooth radio modem / TNC\") instead.",
    "wifi_direct": "iPhone does not support Wi-Fi Direct. Use \"Apple peer-to-peer\" with Apple devices or \"Bluetooth (nearby phones)\" with Android phones.",
    "webrtc": "WebRTC links are not available in the iOS app (it would need a large third-party library). Use Apple peer-to-peer, Bluetooth or a hub.",
  ]

  static func spec(_ kind: String) -> EndpointKindSpec? { all.first { $0.kind == kind } }
}
