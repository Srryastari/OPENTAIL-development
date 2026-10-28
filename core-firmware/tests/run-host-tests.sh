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

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/ble_fx_command.c main/effects_1d.c main/palette_1d.c main/wled_fx_compat.c main/particle_system_1d.c main/wled_1d.c tests/test_ble_fx_command.c -o /tmp/ot-ble-fx-tests && /tmp/ot-ble-fx-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/particle_system_1d.c tests/test_particle_system_1d.c -o /tmp/ot-particle-1d-tests && /tmp/ot-particle-1d-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/effects_1d.c main/palette_1d.c main/wled_fx_compat.c main/particle_system_1d.c main/wled_1d.c tests/test_effects_1d.c -o /tmp/ot-effects-unified-tests && /tmp/ot-effects-unified-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/fx_preset_record.c main/fx_zone_bank.c main/effects_1d.c main/palette_1d.c main/wled_fx_compat.c main/particle_system_1d.c main/wled_1d.c tests/test_fx_preset_record.c -o /tmp/ot-fx-presets-test && /tmp/ot-fx-presets-test

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/palette_1d.c main/effects_1d.c main/wled_fx_compat.c main/particle_system_1d.c main/wled_1d.c tests/test_palette_1d.c -o /tmp/ot-palettes-test && /tmp/ot-palettes-test

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/fx_zone_bank.c tests/test_fx_zone_bank.c -o /tmp/ot-fx-bank-tests && /tmp/ot-fx-bank-tests

python3 tests/test_wled_catalog.py

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/arbitration_logic.c tests/test_arbitration_logic.c -o /tmp/ot-arb-tests && /tmp/ot-arb-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/output_manager.c main/core_logic.c main/arbitration_logic.c tests/test_output_manager.c -o /tmp/ot-output-tests && /tmp/ot-output-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/trigger_manager.c main/core_logic.c main/arbitration_logic.c main/output_manager.c tests/test_trigger_manager.c -o /tmp/ot-trigger-manager-tests && /tmp/ot-trigger-manager-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain tests/test_ot_transition.c -o /tmp/ot-transition-tests && /tmp/ot-transition-tests
