package com.opentail.android

import android.os.Handler
import android.os.Looper
import android.os.SystemClock

/**
 * Continuous CORE-owned playback using the advertised volatile opcode.
 *
 * Every Android authoring control is locally live by default. A controller
 * must be explicitly authorized/armed once before edit gestures may also send
 * continuous FX commands; no edits call the persistent BLE output-config opcode.
 *
 * This supports rendering changes to already-saved CORE zones only. It CANNOT
 * preview new zone boundaries, pixel indices, color order or strip length on
 * hardware: the current protocol has no volatile configuration transaction.
 * Never claim those geometry changes are live on hardware or write NVS on drag.
 */
internal class LiveBlePreview(
    private val ble: BleDiscovery,
    private val supportedIds: () -> Set<Int>,
    private val report: (String) -> Unit,
) {
    data class Frame(
        val effectId: Int,
        val chainMask: Int,
        val zoneMask: Int,
        val foreground: Int,
        val background: Int,
        val brightness: Int,
        val speed: Int,
        val intensity: Int,
        val paletteId: Int,
    )

    private val handler = Handler(Looper.getMainLooper())
    // Low-latency, last-frame-wins BLE preview; GATT remains serial and bounded.
    private val intervalMs = 60L
    private var lastSentAt = -intervalMs
    private var scheduled = false
    private var latest: Frame? = null
    private var armed = false

    private val drain = Runnable {
        scheduled = false
        val frame = latest ?: return@Runnable
        latest = null
        if (!armed || !ble.canControl() || frame.effectId !in supportedIds()) {
            report("Preview paused: BLE authorization or CORE effect unavailable")
            return@Runnable
        }
        lastSentAt = SystemClock.uptimeMillis()
        // One accepted edit sends one command. CORE owns lifetime and arbitration.
        // No keepalive, timeout or NVS write.
        ble.showContinuousFx(
            frame.effectId, frame.chainMask, frame.zoneMask,
            (frame.foreground shr 16) and 255,
            (frame.foreground shr 8) and 255,
            frame.foreground and 255,
            (frame.background shr 16) and 255,
            (frame.background shr 8) and 255,
            frame.background and 255,
            frame.brightness.coerceIn(0, 24),
            frame.speed.coerceIn(0, 255),
            frame.intensity.coerceIn(0, 255),
            frame.paletteId
        )
    }

    fun isArmed(): Boolean = armed

    fun arm(): Boolean {
        if (!ble.canContinuousFx() || supportedIds().isEmpty()) {
            report("Authorize CORE and wait for continuous capability; update firmware if unavailable")
            return false
        }
        armed = true
        report("Continuous CORE playback enabled · Stop on Home")
        return true
    }

    fun update(frame: Frame) {
        if (!armed) return
        if (frame.effectId !in supportedIds()) {
            report("Effect is not supported by the connected CORE firmware")
            return
        }
        if (!NativePaletteEngine.coreSupports(frame.effectId, frame.paletteId)) {
            report("Palette unavailable for this CORE effect; selection stays local")
            return
        }
        if (frame.chainMask !in 1..3 || frame.zoneMask !in 1..255) return
        latest = frame
        if (!scheduled) {
            scheduled = true
            val remaining = (lastSentAt + intervalMs - SystemClock.uptimeMillis()).coerceAtLeast(0L)
            handler.postDelayed(drain, remaining)
        }
    }

    // A lifecycle event is not an owner STOP. Drop only unsent gestures.
    fun detach() {
        handler.removeCallbacks(drain)
        scheduled = false
        latest = null
        armed = false
    }
    fun stop() {
        detach()
        if (ble.canControl()) ble.stopShow()
    }
}
