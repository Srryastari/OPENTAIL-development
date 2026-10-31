#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -pedantic -I main main/core_logic.c main/arbitration_logic.c tests/test_core_logic.c -o /tmp/opentail-core-alpha-tests
/tmp/opentail-core-alpha-tests
cc -std=c11 -Wall -Wextra -Werror -pedantic -I main main/status_logic.c tests/test_status_logic.c -o /tmp/opentail-status-led-tests
/tmp/opentail-status-led-tests
python3 tests/test_status_led_first_frame_contract.py
python3 tests/test_main_stack_boot_contract.py
# Pure failover model only: no backup GPIO, supervisor or auto-takeover.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain \
  main/ot_failover_essential.c main/output_manager.c main/trigger_manager.c \
  main/core_logic.c main/arbitration_logic.c tests/test_failover_essential.c \
  -o /tmp/ot-failover-essential-tests && /tmp/ot-failover-essential-tests
python3 tests/test_failover_runtime_isolation.py
python3 tests/test_failover_release_evidence.py
python3 tests/test_ble_gap_recovery_contract.py
# Durable active/staged NVS records: simulate interrupted commits and all 256
# physical-input masks for both universal zone and trigger profiles.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain \
  main/config_journal.c main/trigger_manager.c main/output_manager.c \
  main/core_logic.c main/arbitration_logic.c tests/test_config_journal.c \
  -o /tmp/ot-config-journal-tests && /tmp/ot-config-journal-tests

cc -std=c11 -Wall -Wextra -Werror -Imain main/core_logic.c main/arbitration_logic.c main/show_logic.c tests/test_show_logic.c -o /tmp/ot-show-tests && /tmp/ot-show-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/wled_fx_compat.c tests/test_wled_fx_compat.c -o /tmp/ot-wled-fx-tests -lm && /tmp/ot-wled-fx-tests

# Compare actual pixel frames against verbatim pinned WLED FX.cpp functions,
# with independent upstream math/color helpers and a narrow palette=0 shim.
# Certifies only the enumerated RGB/gamma-off profiles (not full catalog parity).
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain -c main/wled_fx_compat.c -o /tmp/ot-wled-fx-parity.o
c++ -std=c++17 -Wall -Wextra -Werror -Wno-parentheses -Imain tests/test_wled_source_parity.cpp /tmp/ot-wled-fx-parity.o -o /tmp/ot-wled-source-parity -lm
/tmp/ot-wled-source-parity

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/wled_1d.c tests/test_wled_1d.c -o /tmp/ot-wled-1d-tests && /tmp/ot-wled-1d-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/ble_command.c tests/test_ble_command.c -o /tmp/ot-ble-show-tests && /tmp/ot-ble-show-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/ble_fx_command.c main/effects_1d.c main/ot_auto_effects.c main/palette_1d.c main/wled_fx_compat.c main/particle_system_1d.c main/wled_1d.c tests/test_ble_fx_command.c -o /tmp/ot-ble-fx-tests -lm && /tmp/ot-ble-fx-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/ot_auto_effects.c main/effects_1d.c main/wled_fx_compat.c main/palette_1d.c main/particle_system_1d.c main/wled_1d.c tests/test_auto_effects.c -o /tmp/ot-auto-effects-tests -lm && /tmp/ot-auto-effects-tests
# All named OpenTail originals, including the Event Horizon special and
# Singularity Shutdown, must compile, be registered and respect true black.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/ot_auto_effects.c main/effects_1d.c main/wled_fx_compat.c main/palette_1d.c main/particle_system_1d.c main/wled_1d.c tests/test_proprietary_effects.c -o /tmp/ot-proprietary-fx-tests -lm && /tmp/ot-proprietary-fx-tests

# Continuous proprietary Show Mode loops have separate deterministic golden
# frames, phase-preserving Speed/Intensity changes and seven fixed one-shots.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain \
  main/ot_auto_effects.c main/effects_1d.c main/wled_fx_compat.c \
  main/palette_1d.c main/particle_system_1d.c main/wled_1d.c \
  tests/test_proprietary_loop_continuity.c -o /tmp/ot-proprietary-continuity -lm
/tmp/ot-proprietary-continuity

# All decorative fade maths and strobe edge timings must survive every change.
# This proves only host-side rendered frames; physical FPS/response is separate.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/ot_auto_effects.c main/effects_1d.c main/wled_fx_compat.c main/palette_1d.c main/particle_system_1d.c main/wled_1d.c tests/test_render_quality.c -o /tmp/ot-render-quality-tests -lm && /tmp/ot-render-quality-tests


cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/particle_system_1d.c tests/test_particle_system_1d.c -o /tmp/ot-particle-1d-tests && /tmp/ot-particle-1d-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/effects_1d.c main/ot_auto_effects.c main/palette_1d.c main/wled_fx_compat.c main/particle_system_1d.c main/wled_1d.c tests/test_effects_1d.c -o /tmp/ot-effects-unified-tests -lm && /tmp/ot-effects-unified-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/fx_preset_record.c main/fx_zone_bank.c main/fx_overlay_engine.c main/effects_1d.c main/ot_auto_effects.c main/palette_1d.c main/wled_fx_compat.c main/particle_system_1d.c main/wled_1d.c tests/test_fx_preset_record.c -o /tmp/ot-fx-presets-test -lm && /tmp/ot-fx-presets-test

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/palette_1d.c main/effects_1d.c main/ot_auto_effects.c main/wled_fx_compat.c main/particle_system_1d.c main/wled_1d.c tests/test_palette_1d.c -o /tmp/ot-palettes-test -lm && /tmp/ot-palettes-test

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/fx_zone_bank.c main/fx_overlay_engine.c tests/test_fx_zone_bank.c -o /tmp/ot-fx-bank-tests && /tmp/ot-fx-bank-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/fx_overlay_engine.c tests/test_fx_overlay_engine.c -o /tmp/ot-fx-overlay && /tmp/ot-fx-overlay
# Effects-owned OEM matrix on corrected firmware; no production protocol
# or trigger implementation changes. Physically verified output stays gated.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain \
  main/output_manager.c main/core_logic.c main/arbitration_logic.c \
  main/fx_overlay_engine.c tests/test_effects_oem_safety_matrix.c \
  -o /tmp/ot-effects-oem-safety && /tmp/ot-effects-oem-safety
python3 tests/test_effects_render_safety_contract.py
# The same corrected NVS recovery journal must preserve independently
# assigned FX zones and never auto-play a saved show after reboot.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain \
  main/config_journal.c main/trigger_manager.c main/output_manager.c \
  main/core_logic.c main/arbitration_logic.c main/fx_zone_bank.c \
  main/fx_overlay_engine.c tests/test_effects_nvs_recovery_isolation.c \
  -o /tmp/ot-effects-nvs-isolation && /tmp/ot-effects-nvs-isolation
python3 tests/test_effect_capabilities_manifest.py
python3 tests/test_nimble_long_read_contract.py
# Firmware-owned v3 palette codec + authenticated BLE transaction regression,
# including simulated durable write failure and session reset.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain \
  main/palette_profile.c main/palette_transfer.c main/palette_ble_v3.c \
  main/wled_fx_compat.c tests/test_palette_protocol.c \
  -o /tmp/ot-palette-v3-tests -lm && /tmp/ot-palette-v3-tests
python3 tests/test_palette_integration_contract.py

python3 tests/test_wled_catalog.py
# Inventory audit is separate from release certification (which remains blocked
# until all source-derived parity and physical evidence gates pass).
python3 tests/check_effects_release.py --audit
python3 tests/test_version_identity.py
python3 tests/test_http_stop_contract.py
python3 tests/test_dev_console_contract.py
python3 tests/test_nvs_finalization_contract.py

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/arbitration_logic.c tests/test_arbitration_logic.c -o /tmp/ot-arb-tests && /tmp/ot-arb-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/output_manager.c main/core_logic.c main/arbitration_logic.c tests/test_output_manager.c -o /tmp/ot-output-tests && /tmp/ot-output-tests
# Validate two independent board-memory budgets. Neither is a vehicle layout.
for lane_budget in 1024 3072; do
  cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain -DCONFIG_OPENTAIL_BUFFER_PIXELS_PER_OUTPUT="$lane_budget" \
     main/output_manager.c main/core_logic.c main/arbitration_logic.c tests/test_output_manager.c \
     -o /tmp/ot-output-budget-tests
  /tmp/ot-output-budget-tests
done
python3 tests/test_universal_output_contract.py
python3 tests/test_ota_boot_confirmation_contract.py
# All physical lanes must submit before CORE reports a completed frame or Stop.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain tests/test_ot_frame_contract.c -o /tmp/ot-frame-contract
/tmp/ot-frame-contract
# Frozen logical RGB888 capture must reject incomplete, stale, or disconnected frames.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/ot_frame_snapshot.c tests/test_ot_frame_snapshot.c -o /tmp/ot-frame-snapshot
/tmp/ot-frame-snapshot
python3 tests/test_frame_snapshot_contract.py
# Adaptive frame pacing is a pure C hardware-load policy; optical FPS remains a bench gate.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/ot_fps_pacer.c tests/test_ot_fps_pacer.c -o /tmp/ot-fps-pacer-tests
/tmp/ot-fps-pacer-tests

# Verify the unchanged v1 BLE wire ABI round-trips arbitrary user-created zones.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/ble_config.c main/output_manager.c main/core_logic.c main/arbitration_logic.c tests/test_ble_config.c -o /tmp/ot-ble-config-tests && /tmp/ot-ble-config-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/trigger_manager.c main/core_logic.c main/arbitration_logic.c main/output_manager.c main/show_logic.c tests/test_trigger_manager.c -o /tmp/ot-trigger-manager-tests && /tmp/ot-trigger-manager-tests
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/trigger_manager.c main/core_logic.c main/arbitration_logic.c main/output_manager.c tests/test_prior_trigger_profiles.c -o /tmp/ot-prior-trigger-profiles && /tmp/ot-prior-trigger-profiles

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain tests/test_ot_transition.c -o /tmp/ot-transition-tests && /tmp/ot-transition-tests

# Preserve the verified physical bench contract as a host regression baseline.
# This DOES NOT replace repeating the checks on GPIO16/17 and IN1..IN8.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/output_manager.c main/core_logic.c main/arbitration_logic.c main/trigger_manager.c main/ble_config.c tests/test_verified_bench_contract.c -o /tmp/ot-verified-bench-contract && /tmp/ot-verified-bench-contract

# BLE output map serialization must roundtrip arbitrary user-owned zones,
# without either Studio or the firmware presuming a vehicle's geometry.
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/output_manager.c main/core_logic.c main/arbitration_logic.c main/ble_config.c tests/test_ble_config.c -o /tmp/ot-ble-config-tests && /tmp/ot-ble-config-tests
