package org.embermesh.app.endpoint.audio

import org.junit.Assert.assertEquals
import org.junit.Test

class AudioLogicTest {
    @Test fun autoPrefersNativeRate() {
        assertEquals(listOf(44100, 48000, 22050, 16000, 11025, 8000), AudioEndpoint.rateCandidates("auto", 44100, false))
        assertEquals(AudioEndpoint.STANDARD_RATES, AudioEndpoint.rateCandidates("auto", null, false))
    }

    @Test fun fixedRateFirstAndScoRates() {
        assertEquals(16000, AudioEndpoint.rateCandidates("16000", 48000, false).first())
        assertEquals(listOf(16000, 8000), AudioEndpoint.rateCandidates("auto", 48000, true))
        assertEquals(48000, AudioEndpoint.rateCandidates("100", 48000, false).first()) // < 8000 ignored
    }

    @Test fun sourcesNeverEmptyAndUnprocessedFirst() {
        val s = AudioEndpoint.sourceCandidates("auto", unprocessed = true, sco = false)
        assertEquals(9, s.first())
        assertEquals(listOf(6, 1), AudioEndpoint.sourceCandidates("auto", unprocessed = false, sco = false))
        assertEquals(7, AudioEndpoint.sourceCandidates("auto", false, sco = true).first())
    }

    @Test fun gainClips() {
        val b = shortArrayOf(1000, -1000, 30000, -30000)
        AudioEndpoint.applyGain(b, 4, 2f)
        assertEquals(listOf<Short>(2000, -2000, 32767, -32768), b.toList())
    }
}
