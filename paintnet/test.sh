#!/usr/bin/env bash
# Focused custom-effect COM interface regression.
set -euo pipefail
here="$(cd -- "$(dirname -- "$0")" && pwd)"
work_root="$(realpath -e -- "${1:-$here/work}")"
export WINEPREFIX="$work_root/prefix" WINEDEBUG="${WINEDEBUG:--all}"
export DXVK_LOG_LEVEL="${DXVK_LOG_LEVEL:-error}"
export WINEDLLOVERRIDES='d2d1=b;d3d11=n;d3d10core=n;dxgi=n;mshtml='
exec "$work_root/wine/opt/wine-devel/bin/wine" \
    "$work_root/build/dlls/d2d1/tests/x86_64-windows/d2d1_test.exe" effect_identity
