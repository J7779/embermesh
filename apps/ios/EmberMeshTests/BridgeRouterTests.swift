import XCTest
@testable import EmberMesh

final class FakeCoreCaller: CoreCaller {
  var calls: [(String, String)] = []
  var reply = #"{"ok":true,"result":{"x":1}}"#
  func callCore(method: String, paramsJSON: String, completion: @escaping (String) -> Void) {
    calls.append((method, paramsJSON))
    completion(reply)
  }
}

final class FakeHost: HostMethodHandler {
  var calls: [(String, JSONObject)] = []
  func handleHost(method: String, params: JSONObject, completion: @escaping RPCCompletion) {
    calls.append((method, params))
    completion(method == "host.info" ? .success(["platform": "ios"]) : .failure(.unsupported("nope")))
  }
}

final class BridgeRouterTests: XCTestCase {
  var core: FakeCoreCaller!
  var host: FakeHost!
  var js: [String] = []
  var router: BridgeRouter!

  override func setUp() {
    core = FakeCoreCaller()
    host = FakeHost()
    js = []
    router = BridgeRouter(core: core, host: host) { [unowned self] in self.js.append($0) }
  }

  /// Extracts the JSON argument of window.__emberBridge.onReply(...)/onEvent(...).
  func payload(_ s: String, _ fn: String) -> JSONObject? {
    let prefix = "window.__emberBridge&&window.__emberBridge.\(fn)("
    guard s.hasPrefix(prefix), s.hasSuffix(")") else { return nil }
    return JSON.parseObject(String(s.dropFirst(prefix.count).dropLast()))
  }

  func testCoreMethodForwardedAndIdSpliced() {
    router.handle(body: #"{"id":7,"method":"node.info","params":{"a":1}}"#)
    XCTAssertEqual(core.calls.first?.0, "node.info")
    XCTAssertEqual(JSON.parseObject(core.calls.first!.1)?.int("a"), 1)
    let r = payload(js[0], "onReply")!
    XCTAssertEqual(r.int("id"), 7)
    XCTAssertEqual(r.bool("ok"), true)
    XCTAssertEqual(r.object("result")?.int("x"), 1)
  }

  func testCoreErrorEnvelopePassesThrough() {
    core.reply = #"{"ok":false,"error":{"code":"not_implemented","message":"m"}}"#
    router.handle(body: #"{"id":"abc","method":"messages.send"}"#)
    let r = payload(js[0], "onReply")!
    XCTAssertEqual(r.string("id"), "abc")
    XCTAssertEqual(r.bool("ok"), false)
    XCTAssertEqual(r.object("error")?.string("code"), "not_implemented")
    XCTAssertEqual(core.calls.first?.1, "{}")  // missing params → {}
  }

  func testEmptyResultEnvelope() {
    core.reply = #"{ }"#
    router.handle(body: #"{"id":1,"method":"x"}"#)
    XCTAssertEqual(payload(js[0], "onReply")?.int("id"), 1)
  }

  func testGarbageCoreReply() {
    core.reply = "not json"
    router.handle(body: #"{"id":1,"method":"x"}"#)
    XCTAssertEqual(payload(js[0], "onReply")?.object("error")?.string("code"), "internal")
  }

  func testHostMethodRoutedToHost() {
    router.handle(body: #"{"id":2,"method":"host.info"}"#)
    XCTAssertTrue(core.calls.isEmpty)
    XCTAssertEqual(host.calls.first?.0, "host.info")
    XCTAssertEqual(payload(js[0], "onReply")?.object("result")?.string("platform"), "ios")
    router.handle(body: #"{"id":3,"method":"host.whatever"}"#)
    XCTAssertEqual(payload(js[1], "onReply")?.object("error")?.string("code"), "unsupported")
  }

  func testDictionaryBodyTolerated() {
    router.handle(body: ["id": 4, "method": "node.info"] as JSONObject)
    XCTAssertEqual(payload(js[0], "onReply")?.int("id"), 4)
  }

  func testMalformedMessages() {
    router.handle(body: "{not json")
    router.handle(body: 42)
    router.handle(body: #"{"method":""}"#)  // no id → no reply possible
    XCTAssertTrue(js.isEmpty)
    router.handle(body: #"{"id":5}"#)
    XCTAssertEqual(payload(js[0], "onReply")?.object("error")?.string("code"), "bad_params")
    router.handle(body: #"{"id":6,"method":"node.info","params":[1]}"#)
    XCTAssertEqual(payload(js[1], "onReply")?.object("error")?.string("code"), "bad_params")
    XCTAssertTrue(core.calls.isEmpty)
  }

  func testEventsAndLineSeparatorEscaping() {
    router.sendEvent(json: "{\"event\":\"message.new\",\"data\":{\"text\":\"a\u{2028}b\"}}")
    XCTAssertFalse(js[0].contains("\u{2028}"))
    XCTAssertEqual(payload(js[0], "onEvent")?.object("data")?.string("text"), "a\u{2028}b")
    router.sendEvent(name: "host.location", data: ["lat": 1.5])
    XCTAssertEqual(payload(js[1], "onEvent")?.string("event"), "host.location")
  }

  func testSniffId() {
    XCTAssertEqual(BridgeRouter.sniffId(#"{"id": 12, "method":"x"}"#) as? Int, 12)
    XCTAssertEqual(BridgeRouter.sniffId(#"{"id":"q1"}"#) as? String, "q1")
    XCTAssertTrue(BridgeRouter.sniffId("{}") is NSNull)
  }

  func testEnvelopeParse() {
    XCTAssertEqual(try Envelope.parse(#"{"ok":true,"result":{"a":"b"}}"#).get().string("a"), "b")
    if case .failure(let e) = Envelope.parse(#"{"ok":false,"error":{"code":"busy","message":"m"}}"#) {
      XCTAssertEqual(e, RPCError("busy", "m"))
    } else { XCTFail() }
  }
}
