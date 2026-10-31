package com.opentail.android;

/** Scoped persistence key for the shared WLED+ temperature UI control.
 * The control is relative-only; it must not change CORE or BLE output.
 */
final class EditorTemperatureScope {
    private EditorTemperatureScope() {}

    static String preferenceKey(String scope, int effectId, int colorRole) {
        if (scope == null || scope.isEmpty()) throw new IllegalArgumentException("scope");
        if (effectId < 0 || colorRole < 0 || colorRole > 2)
            throw new IllegalArgumentException("effect/role");
        // Preserve existing Show Modes saved temperature values exactly.
        if ("show-modes".equals(scope))
            return "wled." + effectId + ".temp" + colorRole;
        // Length-prefix prevents collisions even for arbitrary trigger names.
        return "wled-editor." + scope.length() + ":" + scope +
            ".wled." + effectId + ".temp" + colorRole;
    }
}
