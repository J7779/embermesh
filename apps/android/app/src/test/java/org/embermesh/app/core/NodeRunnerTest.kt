package org.embermesh.app.core

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class NodeRunnerTest {
    @Test fun waitFollowsCoreWhenIdle() {
        assertEquals(1000, NodeRunner.computeWait(1000, 10_000, 25, 3000, 0))
        assertEquals(60_000, NodeRunner.computeWait(999_999, 10_000, 25, 3000, 0))
    }

    @Test fun fastCadenceWhileActive() {
        assertEquals(25, NodeRunner.computeWait(1000, 100, 25, 3000, 0))
        assertEquals(10, NodeRunner.computeWait(10, 100, 25, 3000, 0))
    }

    @Test fun capAndNeverZero() {
        assertEquals(20, NodeRunner.computeWait(1000, 10_000, 25, 3000, 20))
        assertEquals(1, NodeRunner.computeWait(0, 10_000, 25, 3000, 0))
        assertEquals(1, NodeRunner.computeWait(-5, 10_000, 25, 3000, 0))
    }

    @Test fun outItemDecode() {
        val raw = byteArrayOf(2, 0, 0, 0, 0x2A, 1, 0, 0, 9, 8)
        val o = OutItem.decode(raw)!!
        assertEquals(EmberNative.OUT_LINK_TX, o.kind); assertEquals(0x12A, o.linkId)
        assertArrayEquals(byteArrayOf(9, 8), o.data)
        assertNull(OutItem.decode(ByteArray(7)))
        assertEquals("{}", OutItem.decode(byteArrayOf(1, 0, 0, 0, 0, 0, 0, 0, '{'.code.toByte(), '}'.code.toByte()))!!.text)
    }
}
