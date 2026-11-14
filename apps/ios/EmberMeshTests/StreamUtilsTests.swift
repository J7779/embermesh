import XCTest
@testable import EmberMesh

final class ByteChunkerTests: XCTestCase {
  func testEmpty() { XCTAssertEqual(ByteChunker.split(Data(), mtu: 100), []) }

  func testOneByte() { XCTAssertEqual(ByteChunker.split(Data([7]), mtu: 100), [Data([7])]) }

  func testExactlyMTU() {
    let d = Data((0..<100).map { UInt8($0) })
    XCTAssertEqual(ByteChunker.split(d, mtu: 100), [d])
  }

  func testMTUPlusOne() {
    let d = Data((0..<101).map { UInt8($0) })
    let c = ByteChunker.split(d, mtu: 100)
    XCTAssertEqual(c.count, 2)
    XCTAssertEqual(c[0].count, 100)
    XCTAssertEqual(c[1], Data([100]))
    XCTAssertEqual(c.reduce(Data(), +), d)
  }

  func testMTUClamped() {
    let d = Data(repeating: 1, count: 2000)
    XCTAssertEqual(ByteChunker.split(d, mtu: 0).first?.count, 20)      // BLE 4.0 minimum
    XCTAssertEqual(ByteChunker.split(d, mtu: 5000).first?.count, 512)  // ATT maximum
  }

  func testSliceIndicesDoNotLeak() {
    let big = Data((0..<50).map { UInt8($0) })
    let slice = big[10..<45]  // non-zero startIndex
    XCTAssertEqual(ByteChunker.split(slice, mtu: 20).reduce(Data(), +), Data(slice))
  }
}

final class ChunkQueueTests: XCTestCase {
  func testFIFOAndBoundaries() {
    var q = ChunkQueue()
    q.enqueue(Data(repeating: 1, count: 50), mtu: 20)
    q.enqueue(Data([2]), mtu: 20)
    XCTAssertEqual(q.chunkCount, 4)
    XCTAssertEqual(q.pop()?.count, 20)
    XCTAssertEqual(q.pop()?.count, 20)
    XCTAssertEqual(q.pop()?.count, 10)
    XCTAssertEqual(q.pop(), Data([2]))
    XCTAssertNil(q.pop())
    XCTAssertTrue(q.isEmpty)
    XCTAssertEqual(q.bytes, 0)
  }

  func testCapDropsWholeOldWritesNotInProgress() {
    var q = ChunkQueue(capBytes: 100)
    q.enqueue(Data(repeating: 1, count: 60), mtu: 20)
    _ = q.pop()  // write 1 in progress (40 bytes left)
    q.enqueue(Data(repeating: 2, count: 50), mtu: 20)  // 90
    q.enqueue(Data(repeating: 3, count: 50), mtu: 20)  // 140 > cap: drop write 2, not write 1
    XCTAssertEqual(q.droppedWrites, 1)
    var out = Data()
    while let c = q.pop() { out += c }
    XCTAssertEqual(out, Data(repeating: 1, count: 40) + Data(repeating: 3, count: 50))
  }

  func testNewestWriteAlwaysKept() {
    var q = ChunkQueue(capBytes: 10)
    q.enqueue(Data(repeating: 9, count: 30), mtu: 20)
    XCTAssertEqual(q.chunkCount, 2)
  }
}

final class RxArbiterTests: XCTestCase {
  func testSinglePeerPassesThrough() {
    let a = RxArbiter<Int>(gapMs: 100)
    XCTAssertEqual(a.receive(1, Data([1]), now: 0), [Data([1])])
    XCTAssertEqual(a.receive(1, Data([2]), now: 10), [Data([2])])
    XCTAssertFalse(a.hasPending)
  }

  func testSecondPeerWaitsForGap() {
    let a = RxArbiter<Int>(gapMs: 100)
    _ = a.receive(1, Data("A1".utf8), now: 0)
    XCTAssertEqual(a.receive(2, Data("B1".utf8), now: 20), [])   // buffered
    XCTAssertEqual(a.receive(1, Data("A2".utf8), now: 40), [Data("A2".utf8)])
    XCTAssertEqual(a.receive(2, Data("B2".utf8), now: 60), [])
    XCTAssertEqual(a.advance(now: 100), [])                     // A last spoke at 40
    XCTAssertEqual(a.advance(now: 140), [Data("B1".utf8), Data("B2".utf8)])
    XCTAssertEqual(a.currentOwner, 2)
    // A speaks again while B owns: buffered until B is idle.
    XCTAssertEqual(a.receive(1, Data("A3".utf8), now: 150), [])
    XCTAssertEqual(a.advance(now: 161), [Data("A3".utf8)])  // B's last chunk was at 60
  }

  func testNoInterleavingAcrossManyPeers() {
    let a = RxArbiter<Int>(gapMs: 50)
    var out = Data()
    var t: UInt64 = 0
    // Three peers each send a 5-chunk frame, chunks interleaved in time.
    for i in 0..<5 {
      for p in 0..<3 {
        t += 5
        for d in a.receive(p, Data([UInt8(p), UInt8(i)]), now: t) { out += d }
      }
    }
    for d in a.advance(now: t + 1000) { out += d }
    for d in a.advance(now: t + 2000) { out += d }
    // Every peer's chunks are contiguous.
    let pairs = stride(from: 0, to: out.count, by: 2).map { (out[$0], out[$0 + 1]) }
    XCTAssertEqual(pairs.count, 15)
    for p in 0..<3 {
      let idx = pairs.indices.filter { pairs[$0].0 == UInt8(p) }
      XCTAssertEqual(idx.count, 5)
      XCTAssertEqual(idx.last! - idx.first!, 4, "peer \(p) interleaved")
      XCTAssertEqual(idx.map { pairs[$0].1 }, [0, 1, 2, 3, 4])
    }
  }

  func testRemoveOwnerHandsOver() {
    let a = RxArbiter<Int>(gapMs: 1000)
    _ = a.receive(1, Data([1]), now: 0)
    _ = a.receive(2, Data([2]), now: 1)
    XCTAssertEqual(a.remove(1, now: 2), [Data([2])])
    XCTAssertEqual(a.currentOwner, 2)
  }

  func testPerPeerCap() {
    let a = RxArbiter<Int>(gapMs: 1000, capBytesPerPeer: 10)
    _ = a.receive(1, Data([0]), now: 0)
    for _ in 0..<5 { _ = a.receive(2, Data(repeating: 1, count: 4), now: 1) }
    XCTAssertEqual(a.droppedBytes, 12)
  }

  func testClockGoingBackwardsDoesNotHandOver() {
    let a = RxArbiter<Int>(gapMs: 10)
    _ = a.receive(1, Data([1]), now: 1000)
    _ = a.receive(2, Data([2]), now: 1001)
    XCTAssertEqual(a.advance(now: 5), [])
  }
}

final class SampleRingTests: XCTestCase {
  func testWrapAround() {
    let r = SampleRing(capacity: 5)
    var out = [Int16](repeating: 0, count: 5)
    [1, 2, 3, 4].map(Int16.init).withUnsafeBufferPointer { XCTAssertEqual(r.write($0), 4) }
    XCTAssertEqual(out.withUnsafeMutableBufferPointer { r.read(into: UnsafeMutableBufferPointer(rebasing: $0[0..<3])) }, 3)
    [5, 6, 7, 8].map(Int16.init).withUnsafeBufferPointer { XCTAssertEqual(r.write($0), 4) }  // full: 4,5,6,7,8
    [9].map(Int16.init).withUnsafeBufferPointer { XCTAssertEqual(r.write($0), 0) }
    XCTAssertEqual(r.count, 5)
    XCTAssertEqual(out.withUnsafeMutableBufferPointer { r.read(into: $0) }, 5)
    XCTAssertEqual(out, [4, 5, 6, 7, 8])
    XCTAssertEqual(out.withUnsafeMutableBufferPointer { r.read(into: $0) }, 0)
  }
}

final class MeshtasticFramerTests: XCTestCase {
  func testRoundTripAndSplitFeeds() {
    let p1 = Data([1, 2, 3]), p2 = Data(repeating: 9, count: 300)
    let stream = MeshtasticStreamFramer.frame(p1) + MeshtasticStreamFramer.frame(p2)
    var f = MeshtasticStreamFramer()
    var got: [Data] = []
    for b in stream { got += f.feed(Data([b])) }  // one byte at a time
    XCTAssertEqual(got, [p1, p2])
  }

  func testSkipsGarbageAndOversizedLength() {
    var f = MeshtasticStreamFramer()
    let bogus = Data([0x00, 0x94, 0x00, 0x94, 0xC3, 0xFF, 0xFF])  // 65535 > max
    XCTAssertEqual(f.feed(bogus + MeshtasticStreamFramer.frame(Data([5]))), [Data([5])])
  }

  func testEmptyPacket() {
    var f = MeshtasticStreamFramer()
    XCTAssertEqual(f.feed(MeshtasticStreamFramer.frame(Data())), [Data()])
  }
}
