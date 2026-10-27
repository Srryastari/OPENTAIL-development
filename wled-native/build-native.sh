#!/usr/bin/env bash
set -euo pipefail
# Creates a separate EXPERIMENTAL WLED-native firmware. NEVER flashes/erases.
# Requires git, Python 3 and PlatformIO CLI.
WLED_SHA=20e7a717664295eefc6ad77972cee1888d2da072
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET="${1:-${ROOT}/.build/wled}"
if [[ -e "${TARGET}" ]]; then
  echo 'ERROR: target exists; choose an empty folder to avoid overwriting work' >&2
  exit 2
fi
mkdir -p "$(dirname "${TARGET}")"
git clone --filter=blob:none https://github.com/wled/WLED.git "${TARGET}"
git -C "${TARGET}" fetch origin "${WLED_SHA}"
git -C "${TARGET}" checkout --detach "${WLED_SHA}"
[[ "$(git -C "${TARGET}" rev-parse HEAD)" == "${WLED_SHA}" ]] || exit 2
mkdir -p "${TARGET}/usermods/opentail_native"
cp "${ROOT}/usermods/opentail_native/"{opentail_native.cpp,ot_native_overlay.h,library.json} "${TARGET}/usermods/opentail_native/"
cp "${ROOT}/platformio_override.ini" "${TARGET}/platformio_override.ini"
(cd "${TARGET}" && pio run -e opentail_s3_n16r8)
printf 'Native WLED build completed (NOT FLASHED). Upstream: %s\n' "${WLED_SHA}"
