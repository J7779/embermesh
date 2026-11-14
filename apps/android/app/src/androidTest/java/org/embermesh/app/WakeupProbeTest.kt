package org.embermesh.app

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.embermesh.app.core.EmberNode
import org.junit.Test
import org.junit.runner.RunWith
import java.security.SecureRandom

/** Diagnostics: what an idle node asks for (logged; asserts nothing beyond sanity). */
@RunWith(AndroidJUnit4::class)
class WakeupProbeTest {
    @Test fun idleWakeups() {
        val n = EmberNode.create(TestNode.cfg(null), ByteArray(32).also { SecureRandom().nextBytes(it) })!!
        try {
            val seq = ArrayList<Int>()
            val t0 = System.currentTimeMillis()
            while (System.currentTimeMillis() - t0 < 15_000) {
                n.tick(System.currentTimeMillis())
                while (n.poll() != null) {}
                val w = n.nextWakeupMs()
                seq.add(w)
                Thread.sleep(w.toLong().coerceIn(1, 60_000))
            }
            android.util.Log.i("WakeupProbe", "idle node: ${seq.size} ticks in 15 s; next_wakeup_ms=$seq")
        } finally { n.close() }
    }
}
