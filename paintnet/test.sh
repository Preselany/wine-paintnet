#!/usr/bin/env bash
# Focused COM, resource, and effect-rendering regressions.
set -euo pipefail
here="$(cd -- "$(dirname -- "$0")" && pwd)"
work_root="$(realpath -e -- "${1:-$here/work}")"
if [[ $# -gt 0 ]]; then shift; fi
if [[ $# == 0 ]]; then set -- effect_identity effect_context histogram opacity_metadata alpha_mask convolve_matrix contrast bitmap_source; fi
export WINEPREFIX="$work_root/prefix" WINEDEBUG="${WINEDEBUG:--all}"
export DXVK_LOG_LEVEL="${DXVK_LOG_LEVEL:-error}"
export WINEDLLOVERRIDES='d2d1=b;d3d11=n;d3d10core=n;dxgi=n;mshtml='
result=0
for test_name in "$@"; do
    if ! "$work_root/wine/opt/wine-devel/bin/wine" \
        "$work_root/build/dlls/d2d1/tests/x86_64-windows/d2d1_test.exe" "$test_name"; then
        result=1
    fi
done
exit "$result"
