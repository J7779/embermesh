package org.embermesh.app.capture

import java.nio.ByteBuffer

/**
 * RPC_API §12 brightness grid: reduces a full-resolution 8-bit luma (Y) plane of [width]×[height]
 * (sensor orientation) to [gw]×[gh] cells over the WHOLE frame — `max` (the largest luma of the
 * pixels a cell covers, so a light that covers one pixel still decides its cell) and `mean`. Every
 * pixel is read; nothing is downscaled before the max. Row and pixel stride are respected. All
 * buffers are allocated once; [reduce] allocates nothing. Not thread-safe (one camera thread).
 */
class LumaGrid(val width: Int, val height: Int) {
    val gw: Int
    val gh: Int

    init {
        require(width > 0 && height > 0)
        // 64 cells on the long side (64×36 for 16:9, 64×48 for 4:3), as on iOS; w·h ≤ 4096.
        if (width >= height) { gw = minOf(LONG_SIDE, width); gh = maxOf(1, Math.round(gw.toDouble() * height / width).toInt()) }
        else { gh = minOf(LONG_SIDE, height); gw = maxOf(1, Math.round(gh.toDouble() * width / height).toInt()) }
    }

    private val colEdge = IntArray(gw + 1) { (it.toLong() * width / gw).toInt() }
    private val rowCell = IntArray(height).also { rc ->
        for (cy in 0 until gh) {
            val r0 = (cy.toLong() * height / gh).toInt(); val r1 = ((cy + 1).toLong() * height / gh).toInt()
            for (r in r0 until r1) rc[r] = cy
        }
    }
    private val count = IntArray(gw * gh).also { c ->
        for (r in 0 until height) for (cx in 0 until gw) c[rowCell[r] * gw + cx] += colEdge[cx + 1] - colEdge[cx]
    }
    private val mx = IntArray(gw * gh)
    private val sum = IntArray(gw * gh)
    private var row = ByteArray(width)

    /** Cell maxima (0–255) of the last [reduce], sensor orientation, row by row. */
    val max: IntArray get() = mx

    /** Reduces one Y plane. [y]'s position is not changed. */
    fun reduce(y: ByteBuffer, rowStride: Int, pixelStride: Int) {
        java.util.Arrays.fill(mx, 0)
        java.util.Arrays.fill(sum, 0)
        val need = (width - 1) * pixelStride + 1
        if (row.size < need) row = ByteArray(need)
        val src = y.duplicate()
        val lim = src.limit()
        val buf = row
        for (r in 0 until height) {
            val start = r * rowStride
            if (start >= lim) break
            val n = minOf(need, lim - start)
            src.position(start)
            src.get(buf, 0, n)
            val base = rowCell[r] * gw
            for (cx in 0 until gw) {
                val c0 = colEdge[cx]; val c1 = colEdge[cx + 1]
                var m = mx[base + cx]; var s = 0
                if (pixelStride == 1) {
                    val e = minOf(c1, n)
                    var c = c0
                    while (c < e) { val v = buf[c].toInt() and 0xff; if (v > m) m = v; s += v; c++ }
                } else {
                    var c = c0
                    while (c < c1) { val i = c * pixelStride; if (i >= n) break; val v = buf[i].toInt() and 0xff; if (v > m) m = v; s += v; c++ }
                }
                mx[base + cx] = m; sum[base + cx] += s
            }
        }
    }

    /** Width/height of the grid as shown after rotating by [rotation] degrees clockwise. */
    fun shownW(rotation: Int) = if (rotation == 90 || rotation == 270) gh else gw
    fun shownH(rotation: Int) = if (rotation == 90 || rotation == 270) gw else gh

    /**
     * Writes the max and mean grids as the user sees the frame: rotated [rotation]° clockwise
     * (0/90/180/270) and then mirrored horizontally when [mirror] (front camera). Row by row from
     * the top-left. [maxOut] and [meanOut] must hold gw·gh bytes.
     */
    fun orient(rotation: Int, mirror: Boolean, maxOut: ByteArray, meanOut: ByteArray) {
        val dw = shownW(rotation); val dh = shownH(rotation)
        var o = 0
        for (dy in 0 until dh) for (dx0 in 0 until dw) {
            val dx = if (mirror) dw - 1 - dx0 else dx0
            val sx: Int; val sy: Int
            when (rotation) {
                90 -> { sx = dy; sy = gh - 1 - dx }
                180 -> { sx = gw - 1 - dx; sy = gh - 1 - dy }
                270 -> { sx = gw - 1 - dy; sy = dx }
                else -> { sx = dx; sy = dy }
            }
            val i = sy * gw + sx
            maxOut[o] = mx[i].toByte()
            meanOut[o] = (if (count[i] > 0) sum[i] / count[i] else 0).toByte()
            o++
        }
    }

    /** Mean 0..1 over the centre-fifth cells (section-6 fallback for a core without frames). */
    fun centreLevel(): Double {
        var s = 0L; var n = 0L
        for (cy in (gh * 2 / 5) until maxOf(gh * 2 / 5 + 1, gh * 3 / 5)) for (cx in (gw * 2 / 5) until maxOf(gw * 2 / 5 + 1, gw * 3 / 5)) {
            val i = cy * gw + cx; s += sum[i]; n += count[i]
        }
        return if (n == 0L) 0.0 else s.toDouble() / n / 255.0
    }

    companion object {
        const val LONG_SIDE = 64

        /** Clockwise rotation from the sensor image to what the user sees (Camera2 convention). */
        fun rotation(sensorOrientation: Int, displayDegrees: Int, front: Boolean): Int =
            if (front) (sensorOrientation + displayDegrees) % 360 else (sensorOrientation - displayDegrees + 360) % 360
    }
}
