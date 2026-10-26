package com.opentail.android;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.HapticFeedbackConstants;
import android.view.View;

/** User-configurable native Android feedback; never requires a network or BLE connection. */
final class HapticController {
    static final String PREFS = "opentail_android_preferences";
    static final String ENABLED = "haptics_enabled";
    static final String STRENGTH = "haptics_strength";
    static final String SLIDER = "haptics_slider";
    private final SharedPreferences prefs;
    private final Vibrator vibrator;

    HapticController(Context context) {
        prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        vibrator = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
    }
    boolean enabled() { return prefs.getBoolean(ENABLED, true); }
    int strength() { return prefs.getInt(STRENGTH, 0); } // 0 light; 1 medium; 2 strong
    boolean sliderEnabled() { return prefs.getBoolean(SLIDER, false); }
    void configure(boolean on, int level, boolean sliders) {
        prefs.edit().putBoolean(ENABLED, on).putInt(STRENGTH, Math.max(0, Math.min(2,level)))
                .putBoolean(SLIDER, sliders).apply();
    }
    void tap(View source, String kind) {
        if (!enabled() || ("slider".equals(kind) && !sliderEnabled())) return;
        if (vibrator == null || !vibrator.hasVibrator()) {
            if (source != null) source.performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP);
            return;
        }
        int amplitude = new int[] {36, 85, 150}[strength()];
        try {
            if (Build.VERSION.SDK_INT >= 26) {
                if ("saved".equals(kind)) vibrator.vibrate(VibrationEffect.createWaveform(
                        new long[]{0, 18, 80, 23}, new int[]{0, amplitude, 0, amplitude}, -1));
                else if ("error".equals(kind)) vibrator.vibrate(VibrationEffect.createWaveform(
                        new long[]{0, 38, 65, 38}, new int[]{0, amplitude, 0, amplitude}, -1));
                else vibrator.vibrate(VibrationEffect.createOneShot("toggle".equals(kind)?22:12, amplitude));
            }
        } catch (SecurityException | IllegalArgumentException ex) {
            if (source != null) source.performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP);
        }
    }
}
