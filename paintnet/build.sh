#!/usr/bin/env bash
set -euo pipefail
here="$(cd -- "$(dirname -- "$0")" && pwd)"
source_root="$(realpath "$here/..")"
work_root="$(realpath -m -- "${1:-$here/work}")"
if [[ $# -gt 0 ]]; then shift; fi
mkdir -p "$work_root/build" "$work_root/logs"
docker build -t paintnet-classic-builder:ubuntu24.04 "$here"
if [[ $# == 0 ]]; then set -- dlls/d2d1/all dlls/d2d1/tests/all dlls/uianimation/all dlls/uianimation/tests/all; fi
docker run --rm --user "$(id -u):$(id -g)" \
    -v "$source_root:$source_root" -v "$work_root:$work_root" -w "$work_root/build" \
    -e SOURCE_ROOT="$source_root" -e BUILD_JOBS="${BUILD_JOBS:-4}" \
    paintnet-classic-builder:ubuntu24.04 bash -euc '
        if [[ ! -f Makefile ]]; then "$SOURCE_ROOT/configure" --enable-win64 --without-wayland; fi
        make -j"$BUILD_JOBS" "$@"
    ' build "$@"
echo "Built in $work_root/build. Use install-modules.sh to update the isolated runtime."
