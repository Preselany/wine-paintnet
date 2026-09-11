#!/usr/bin/env bash
# Internal texture readback diagnostic; only for this fork's matching module.
set -euo pipefail
here="$(cd -- "$(dirname -- "$0")" && pwd)"
source_root="$(realpath "$here/..")"
work_root="$(realpath -e -- "${1:-$here/work}")"
built_dll="$work_root/build/dlls/d2d1/x86_64-windows/d2d1.dll"
runtime_dll="$work_root/wine/opt/wine-devel/lib/wine/x86_64-windows/d2d1.dll"
if ! cmp --quiet "$built_dll" "$runtime_dll" || \
        ! cmp --quiet "$built_dll" "$work_root/prefix/drive_c/windows/system32/d2d1.dll"; then
    echo 'Install the matching rebuilt d2d1.dll before running the internal upload diagnostic.' >&2
    exit 1
fi
docker run --rm --user "$(id -u):$(id -g)" \
    -v "$source_root:$source_root" -v "$work_root:$work_root" -w "$work_root/build" \
    paintnet-classic-builder:ubuntu24.04 x86_64-w64-mingw32-gcc \
    -Wall -Wextra -Werror -D__WINESRC__ -Iinclude -I"$source_root/include" \
    -I"$source_root/dlls/d2d1" "$here/tests/lookup-upload.c" \
    -o "$work_root/lookup-upload.exe" -ld2d1 -ld3d11 -ldxguid -luuid
export WINEPREFIX="$work_root/prefix" WINEDEBUG="${WINEDEBUG:--all}"
export DXVK_LOG_LEVEL="${DXVK_LOG_LEVEL:-error}"
export WINEDLLOVERRIDES='d2d1=b;d3d11=n;d3d10core=n;dxgi=n;mshtml='
exec "$work_root/wine/opt/wine-devel/bin/wine" "$work_root/lookup-upload.exe"
