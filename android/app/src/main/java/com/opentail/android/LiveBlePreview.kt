package com.opentail.android

import android.os.Handler
import android.os.Looper
import android.os.SystemClock

/**
 * Transient, bounded preview for the PREEXISTING authorized CORE BLE FX opcode.
 *
 * Every Android authoring control is locally live by default. A controller
 * must be explicitly authorized/armed once before edit gestures may also send
 * temporary FX frames; no edits call the persistent BLE output-config opcode.
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
    private val intervalMs = 120L
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
        // Temporary show: existing configuration only; physical trigger zones
        // continue to override as handled by CORE. TTL automatically expires
        // even if Android dies or disconnects before STOP is acknowledged.
        ble.showFx(
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
            frame.paletteId, 5
        )
    }

    fun isArmed(): Boolean = armed

    fun arm(): Boolean {
        if (!ble.canControl() || supportedIds().isEmpty()) {
            report("Authorize CORE and read supported effects first")
            return false
        }
        armed = true
        report("Live hardware preview enabled: temporary FX, maximum 24/255 brightness")
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

    fun stop() {
        handler.removeCallbacks(drain)
        scheduled = false
        latest = null
        if (armed && ble.canControl()) ble.stopShow()
        armed = false
    }
}
