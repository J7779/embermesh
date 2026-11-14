package org.embermesh.app.endpoint.ble

import org.embermesh.app.host.HostError
import org.json.JSONObject
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import java.util.UUID
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

class BleChunkerTest {
    private fun bytes(n: Int) = ByteArray(n) { it.toByte() }

    @Test fun emptyGivesNoChunks() = assertTrue(BleChunker.split(ByteArray(0), 20).isEmpty())

    @Test fun oneByte() {
        val c = BleChunker.split(byteArrayOf(7), 20)
        assertEquals(1, c.size)
        assertArrayEquals(byteArrayOf(7), c[0])
    }

    @Test fun exactMultiple() {
        val c = BleChunker.split(bytes(60), 20)
        assertEquals(3, c.size)
        assertTrue(c.all { it.size == 20 })
    }

    @Test fun mtuEdges() {
        assertEquals(1, BleChunker.split(bytes(20), 20).size)
        val c = BleChunker.split(bytes(21), 20)
        assertEquals(listOf(20, 1), c.map { it.size })
        val big = bytes(1000)
        val parts = BleChunker.split(big, 514)
        assertEquals(listOf(514, 486), parts.map { it.size })
        assertArrayEquals(big, parts.reduce { a, b -> a + b })
    }

    @Test fun payloadForMtu() {
        assertEquals(20, BleChunker.payloadFor(23))
        assertEquals(20, BleChunker.payloadFor(10))
        assertEquals(244, BleChunker.payloadFor(247))
        assertEquals(512, BleChunker.payloadFor(517))
    }

    @Test(expected = IllegalArgumentException::class)
    fun zeroPayloadRejected() { BleChunker.split(bytes(3), 0) }
}

class StreamArbiterTest {
    private var now = 1_000L
    private val out = ArrayList<String>()
    private fun arb(cap: Int = 1024) = StreamArbiter(60, cap, { now }) { out.add(String(it)) }
    private fun s(t: String) = t.toByteArray()

    @Test fun singlePeerPassesThrough() {
        val a = arb()
        a.offer("A", s("aaaa"), 4)
        a.offer("A", s("a1"), 4)
        assertEquals(listOf("aaaa", "a1"), out)
        assertNull(a.floorHolder)
    }

    @Test fun preventsInterleaving() {
        val a = arb()
        a.offer("A", s("AAAA"), 4)          // full: A holds the floor
        a.offer("B", s("bb"), 4)            // buffered
        a.offer("A", s("AAAA"), 4)
        assertEquals(listOf("AAAA", "AAAA"), out)
        a.offer("A", s("A."), 4)            // short: A done, B flushed
        assertEquals(listOf("AAAA", "AAAA", "A.", "bb"), out)
        assertNull(a.floorHolder)
    }

    @Test fun bufferedPeerTakesFloorInOrder() {
        val a = arb()
        a.offer("A", s("AAAA"), 4)
        a.offer("B", s("BBBB"), 4)
        a.offer("C", s("cc"), 4)
        a.offer("B", s("b."), 4)
        a.offer("A", s("a."), 4)
        assertEquals(listOf("AAAA", "a.", "BBBB", "b.", "cc"), out)
    }

    @Test fun timeoutReleasesSilentHolder() {
        val a = arb()
        a.offer("A", s("AAAA"), 4)
        a.offer("B", s("bb"), 4)
        assertTrue(a.needsTick)
        now += 59; a.tick()
        assertEquals(listOf("AAAA"), out)
        now += 1; a.tick()
        assertEquals(listOf("AAAA", "bb"), out)
        assertFalse(a.needsTick)
    }

    @Test fun capDropsOldest() {
        val a = arb(cap = 6)
        a.offer("A", s("AAAA"), 4)
        a.offer("B", s("b1b"), 4)
        a.offer("C", s("c2c"), 4)
        a.offer("D", s("d3d"), 4)           // evicts b1b
        assertEquals(6, a.queuedSize)
        assertEquals(3L, a.droppedBytes)
        assertFalse(a.offer("E", s("toolarge"), 99))
        a.release("A")
        assertEquals(listOf("AAAA", "c2c", "d3d"), out)
    }

    @Test fun smallerSenderChunksCountAsFull() {
        val a = arb()
        a.offer("A", s("x".repeat(30)), 182)  // learns A sends 30-byte chunks
        a.offer("A", s("y".repeat(10)), 182)  // short -> releases
        a.offer("A", s("z".repeat(30)), 182)  // 30 >= maxSeen -> full -> holds
        assertEquals("A", a.floorHolder)
        a.offer("B", s("b"), 182)
        assertEquals(3, out.size)
    }

    @Test fun releaseOnDisconnect() {
        val a = arb()
        a.offer("A", s("AAAA"), 4)
        a.offer("B", s("bb"), 4)
        a.release("A")
        assertEquals(listOf("AAAA", "bb"), out)
    }
}

class TieBreakTest {
    @Test fun tokenRoundTrip() {
        for (t in listOf(0L, 1L, 0x7fffffffL, 0x80000000L, 0xffffffffL, 0x12345678L)) {
            assertEquals(t, TieBreak.parseToken(TieBreak.tokenBytes(t)))
        }
        assertNull(TieBreak.parseToken(null))
        assertNull(TieBreak.parseToken(ByteArray(3)))
        assertNull(TieBreak.parseToken(ByteArray(5)))
    }

    @Test fun lowerTokenInitiates() {
        assertTrue(TieBreak.shouldInitiate(5, 9, true))
        assertFalse(TieBreak.shouldInitiate(9, 5, true))
        assertTrue(TieBreak.shouldInitiate(7, 7, true))
    }

    @Test fun exactlyOneSideInitiates() {
        val a = 0x9000_0000L; val b = 0x1000_0000L
        assertTrue(TieBreak.shouldInitiate(a, b, true) xor TieBreak.shouldInitiate(b, a, true))
    }

    @Test fun noTokenOrNotAdvertisingConnectsAnyway() {
        assertTrue(TieBreak.shouldInitiate(9, null, true))
        assertTrue(TieBreak.shouldInitiate(9, 5, false))
    }
}

class ScanBudgetTest {
    @Test fun limitsStartsPerWindow() {
        val b = ScanBudget(maxStarts = 4, windowMs = 30_000)
        repeat(4) { assertTrue(b.tryAcquire(1_000L + it)) }
        assertFalse(b.tryAcquire(2_000))
        assertEquals(31_000L, b.nextAllowedAt(2_000))
        assertTrue(b.tryAcquire(31_000))
    }

    @Test fun backoffGrowsAndCaps() {
        assertEquals(2_000L, bleBackoffMs(0))
        assertEquals(4_000L, bleBackoffMs(1))
        assertEquals(60_000L, bleBackoffMs(10))
        assertEquals(2_000L, bleBackoffMs(-3))
    }
}

class BleUuidsTest {
    @Test fun parsesFullAndShort() {
        assertEquals(BleUuids.NUS_SERVICE, BleUuids.parse("6E400001-B5A3-F393-E0A9-E50E24DCCA9E"))
        assertEquals(UUID.fromString("0000180f-0000-1000-8000-00805f9b34fb"), BleUuids.parse("180F"))
        assertEquals(UUID.fromString("00002a37-0000-1000-8000-00805f9b34fb"), BleUuids.parse("0x2a37"))
        assertEquals(UUID.fromString("6e400001-0000-1000-8000-00805f9b34fb"), BleUuids.parse("6e400001"))
        assertEquals(BleUuids.CCCD, BleUuids.parse("2902"))
    }

    @Test fun rejectsGarbage() {
        for (t in listOf(null, "", "xyz", "12345", "zz0f", "6e400001-b5a3-f393-e0a9-e50e24dcca9")) assertNull(t, BleUuids.parse(t))
    }

    @Test fun emberUuidsShareSuffix() {
        val suffix = "-5d1f-4c6a-9b7e-0d2a6f1c4e10"
        assertTrue(BleUuids.EMBER_SERVICE.toString().endsWith(suffix))
        assertTrue(BleUuids.EMBER_RX.toString().startsWith("8e3b0002"))
        assertTrue(BleUuids.EMBER_TX.toString().startsWith("8e3b0003"))
    }
}

class BleUartConfigTest {
    @Test fun defaultsToNus() {
        val c = BleUartConfig.from(JSONObject().put("address", "aa:bb:cc:dd:ee:0f"))
        assertEquals("AA:BB:CC:DD:EE:0F", c.address)
        assertEquals(BleUuids.NUS_SERVICE, c.service)
        assertEquals(BleUuids.NUS_RX, c.rx)
        assertEquals(BleUuids.NUS_TX, c.tx)
        assertFalse(c.withResponse)
        assertEquals(247, c.mtu)
    }

    @Test fun overridesAndClamps() {
        val c = BleUartConfig.from(JSONObject().put("address", "AA-BB-CC-DD-EE-FF").put("service_uuid", "ffe0")
            .put("rx_uuid", "ffe1").put("tx_uuid", "ffe1").put("write_with_response", true).put("mtu", 9999))
        assertEquals(BleUuids.parse("ffe0"), c.service)
        assertTrue(c.withResponse)
        assertEquals(517, c.mtu)
    }

    @Test fun badAddressOrUuid() {
        for (cfg in listOf(JSONObject(), JSONObject().put("address", "nope"),
            JSONObject().put("address", "AA:BB:CC:DD:EE:FF").put("rx_uuid", "bogus"))) {
            try { BleUartConfig.from(cfg); fail("expected bad_params for $cfg") } catch (e: HostError) {
                assertEquals(HostError.BAD_PARAMS, e.code)
            }
        }
    }
}

class GattOpQueueTest {
    @Test fun serialisesAndTimesOut() {
        val exec = Executors.newSingleThreadScheduledExecutor()
        try {
            val log = ArrayList<String>()
            val q = GattOpQueue(exec)
            exec.submit {
                q.enqueue("a", 50) { log.add("start a"); GattOpQueue.Start.STARTED }
                q.enqueue("b", 5_000) { log.add("start b"); GattOpQueue.Start.STARTED }
                q.enqueue("c", 5_000) { log.add("start c"); GattOpQueue.Start.FAILED }
                q.enqueue("d", 5_000) { log.add("start d"); GattOpQueue.Start.STARTED }
            }.get()
            assertEquals(listOf("start a"), exec.submit<List<String>> { log.toList() }.get())
            Thread.sleep(200) // a times out -> b starts
            assertEquals(listOf("start a", "start b"), exec.submit<List<String>> { log.toList() }.get())
            exec.submit { q.complete() }.get() // b done -> c fails -> d starts
            assertEquals(listOf("start a", "start b", "start c", "start d"), exec.submit<List<String>> { log.toList() }.get())
            assertEquals("d", exec.submit<String?> { q.currentName }.get())
        } finally { exec.shutdownNow(); exec.awaitTermination(1, TimeUnit.SECONDS) }
    }

    @Test fun busyIsRetried() {
        val exec = Executors.newSingleThreadScheduledExecutor()
        try {
            var tries = 0
            val q = GattOpQueue(exec)
            exec.submit { q.enqueue("w", 1_000) { if (++tries < 3) GattOpQueue.Start.BUSY else GattOpQueue.Start.STARTED } }.get()
            Thread.sleep(100)
            assertEquals(3, exec.submit<Int> { tries }.get())
            assertEquals("w", exec.submit<String?> { q.currentName }.get())
        } finally { exec.shutdownNow() }
    }
}
