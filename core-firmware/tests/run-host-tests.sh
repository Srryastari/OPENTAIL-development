#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -pedantic -I main main/core_logic.c main/arbitration_logic.c tests/test_core_logic.c -o /tmp/opentail-core-alpha-tests
/tmp/opentail-core-alpha-tests
cc -std=c11 -Wall -Wextra -Werror -pedantic -I main main/status_logic.c tests/test_status_logic.c -o /tmp/opentail-status-led-tests
/tmp/opentail-status-led-tests

cc -std=c11 -Wall -Wextra -Werror -Imain main/core_logic.c main/arbitration_logic.c main/show_logic.c tests/test_show_logic.c -o /tmp/ot-show-tests && /tmp/ot-show-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/wled_fx_compat.c tests/test_wled_fx_compat.c -o /tmp/ot-wled-fx-tests && /tmp/ot-wled-fx-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/wled_1d.c tests/test_wled_1d.c -o /tmp/ot-wled-1d-tests && /tmp/ot-wled-1d-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/ble_fx_command.c main/effects_1d.c main/ot_auto_effects.c main/palette_1d.c main/wled_fx_compat.c main/particle_system_1d.c main/wled_1d.c tests/test_ble_fx_command.c -o /tmp/ot-ble-fx-tests && /tmp/ot-ble-fx-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/ot_auto_effects.c main/effects_1d.c main/wled_fx_compat.c main/palette_1d.c main/particle_system_1d.c main/wled_1d.c tests/test_auto_effects.c -o /tmp/ot-auto-effects-tests && /tmp/ot-auto-effects-tests
# All named OpenTail originals, including the Event Horizon special and
# Singularity Shutdown, must compile, be registered and respect true black.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/ot_auto_effects.c main/effects_1d.c main/wled_fx_compat.c main/palette_1d.c main/particle_system_1d.c main/wled_1d.c tests/test_proprietary_effects.c -o /tmp/ot-proprietary-fx-tests && /tmp/ot-proprietary-fx-tests

# All decorative fade maths and strobe edge timings must survive every change.
# This proves only host-side rendered frames; physical FPS/response is separate.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/ot_auto_effects.c main/effects_1d.c main/wled_fx_compat.c main/palette_1d.c main/particle_system_1d.c main/wled_1d.c tests/test_render_quality.c -o /tmp/ot-render-quality-tests && /tmp/ot-render-quality-tests


cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/particle_system_1d.c tests/test_particle_system_1d.c -o /tmp/ot-particle-1d-tests && /tmp/ot-particle-1d-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/effects_1d.c main/ot_auto_effects.c main/palette_1d.c main/wled_fx_compat.c main/particle_system_1d.c main/wled_1d.c tests/test_effects_1d.c -o /tmp/ot-effects-unified-tests && /tmp/ot-effects-unified-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/fx_preset_record.c main/fx_zone_bank.c main/effects_1d.c main/ot_auto_effects.c main/palette_1d.c main/wled_fx_compat.c main/particle_system_1d.c main/wled_1d.c tests/test_fx_preset_record.c -o /tmp/ot-fx-presets-test && /tmp/ot-fx-presets-test

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/palette_1d.c main/effects_1d.c main/ot_auto_effects.c main/wled_fx_compat.c main/particle_system_1d.c main/wled_1d.c tests/test_palette_1d.c -o /tmp/ot-palettes-test && /tmp/ot-palettes-test

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/fx_zone_bank.c tests/test_fx_zone_bank.c -o /tmp/ot-fx-bank-tests && /tmp/ot-fx-bank-tests

python3 tests/test_wled_catalog.py
# Inventory audit is separate from release certification (which remains blocked
# until all source-derived parity and physical evidence gates pass).
python3 tests/check_effects_release.py --audit
python3 tests/test_version_identity.py
python3 tests/test_http_stop_contract.py
python3 tests/test_dev_console_contract.py

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/arbitration_logic.c tests/test_arbitration_logic.c -o /tmp/ot-arb-tests && /tmp/ot-arb-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/output_manager.c main/core_logic.c main/arbitration_logic.c tests/test_output_manager.c -o /tmp/ot-output-tests && /tmp/ot-output-tests

# Verify the unchanged v1 BLE wire ABI round-trips arbitrary user-created zones.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/ble_config.c main/output_manager.c main/core_logic.c main/arbitration_logic.c tests/test_ble_config.c -o /tmp/ot-ble-config-tests && /tmp/ot-ble-config-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/trigger_manager.c main/core_logic.c main/arbitration_logic.c main/output_manager.c tests/test_trigger_manager.c -o /tmp/ot-trigger-manager-tests && /tmp/ot-trigger-manager-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain tests/test_ot_transition.c -o /tmp/ot-transition-tests && /tmp/ot-transition-tests

# Preserve the verified physical bench contract as a host regression baseline.
# This DOES NOT replace repeating the checks on GPIO16/17 and IN1..IN8.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/output_manager.c main/core_logic.c main/arbitration_logic.c main/trigger_manager.c main/ble_config.c tests/test_verified_bench_contract.c -o /tmp/ot-verified-bench-contract && /tmp/ot-verified-bench-contract

# BLE output map serialization must roundtrip arbitrary user-owned zones,
# without either Studio or the firmware presuming a vehicle's geometry.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/output_manager.c main/core_logic.c main/arbitration_logic.c main/ble_config.c tests/test_ble_config.c -o /tmp/ot-ble-config-tests && /tmp/ot-ble-config-tests
