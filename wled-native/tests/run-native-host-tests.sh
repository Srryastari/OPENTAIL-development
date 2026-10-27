#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
node --test "${ROOT}/tests/native-bridge.test.mjs"
binary="$(mktemp)"
trap 'rm -f "${binary}"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror -pedantic "${ROOT}/tests/test_native_overlay.cpp" -o "${binary}"
"${binary}"
echo 'WLED native bridge and trigger overlay host tests passed.'
