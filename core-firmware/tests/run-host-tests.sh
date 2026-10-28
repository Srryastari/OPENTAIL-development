#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -pedantic -I main main/core_logic.c main/arbitration_logic.c tests/test_core_logic.c -o /tmp/opentail-core-alpha-tests
/tmp/opentail-core-alpha-tests
cc -std=c11 -Wall -Wextra -Werror -pedantic -I main main/status_logic.c tests/test_status_logic.c -o /tmp/opentail-status-led-tests
/tmp/opentail-status-led-tests

cc -std=c11 -Wall -Wextra -Werror -Imain main/core_logic.c main/arbitration_logic.c main/show_logic.c main/wled_1d.c tests/test_show_logic.c -o /tmp/ot-show-tests && /tmp/ot-show-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/arbitration_logic.c tests/test_arbitration_logic.c -o /tmp/ot-arb-tests && /tmp/ot-arb-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/output_manager.c main/core_logic.c main/arbitration_logic.c tests/test_output_manager.c -o /tmp/ot-output-tests && /tmp/ot-output-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/trigger_manager.c main/core_logic.c main/arbitration_logic.c main/output_manager.c tests/test_trigger_manager.c -o /tmp/ot-trigger-manager-tests && /tmp/ot-trigger-manager-tests

cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/core_logic.c main/arbitration_logic.c main/show_logic.c main/wled_1d.c tests/test_wled_1d.c -o /tmp/ot-wled-1d-tests && /tmp/ot-wled-1d-tests
