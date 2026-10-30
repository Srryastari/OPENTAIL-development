#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
node --test "${ROOT}/tests/native-bridge.test.mjs"
for source in test_native_overlay test_native_effects test_render_coordinator; do
  binary="$(mktemp)"
  trap 'rm -f "${binary}"' EXIT
  c++ -std=c++17 -Wall -Wextra -Werror -pedantic "${ROOT}/tests/${source}.cpp" -o "${binary}"
  "${binary}"
  rm -f "${binary}"
  trap - EXIT
done
echo 'WLED native bridge, overlay, native-effects and coordinator host tests passed.'
