package com.opentail.android

import kotlin.math.sin

/**
 * Offline-only *illustrative* 30-capsule preview of five simple patterns.
 * It never reads or purports to represent CORE telemetry, never enters BLE,
 * and does not assert 1:1 WLED render parity. Unported effects return null.
 */
internal object OfflineFxSketch {
    fun supports(id: Int): Boolean = id == 0 || id == 1 || id == 3 ||
        id == 9 || id == 12

    fun colorAt(id: Int, pixel: Int, pixels: Int, tick: Long,
                fg: Int, bg: Int): Int? {
        if (!supports(id) || pixels <= 0 || pixel !in 0 until pixels) return null
        val front=0xff000000.toInt() or (fg and 0xffffff)
        val back=0xff000000.toInt() or (bg and 0xffffff)
        fun blend(a:Int,b:Int,alpha:Float):Int {
            val t=alpha.coerceIn(0f,1f)
            fun channel(shift:Int):Int {
                val av=(a ushr shift) and 255
                val bv=(b ushr shift) and 255
                return (av+(bv-av)*t).toInt().coerceIn(0,255)
            }
            return 0xff000000.toInt() or
                (channel(16) shl 16) or (channel(8) shl 8) or channel(0)
        }
        return when(id) {
            0 -> front
            1 -> if ((tick / 16L) % 2L == 0L) front else back
            3 -> if(pixel <= ((tick / 2L) % (pixels+1))) front else back
            9 -> android.graphics.Color.HSVToColor(floatArrayOf(
                ((pixel * 360f / pixels) + tick * 4f) % 360f,1f,1f))
            12 -> blend(back,front, ((sin(tick * .095)+1.0)/2.0).toFloat())
            else -> null
        }
    }
}
