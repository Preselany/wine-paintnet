#!/usr/bin/env bash
# Build public-API measurement tools for an independent Windows reference host.
set -euo pipefail
here="$(cd -- "$(dirname -- "$0")" && pwd)"
source_root="$(realpath "$here/..")"
work_root="$(realpath -e -- "${1:-$here/work}")"
test -s "$work_root/build/include/d2d1_1.h"
mkdir -p "$work_root/reference"
for probe in emboss-reference emboss-grid8-reference effect-properties-reference; do
    probe_source="$probe"
    probe_flags=()
    if [[ "$probe" == emboss-grid8-reference ]]; then
        probe_source=emboss-reference
        probe_flags=(-DPROBE_SIDE=8)
    fi
    docker run --rm --user "$(id -u):$(id -g)" \
        -v "$source_root:$source_root" -v "$work_root:$work_root" -w "$work_root/build" \
        paintnet-classic-builder:ubuntu24.04 x86_64-w64-mingw32-gcc \
        -Wall -Wextra -Werror -D__WINESRC__ -Iinclude -I"$source_root/include" \
        "${probe_flags[@]}" "$here/tests/$probe_source.c" -o "$work_root/reference/$probe.exe" \
        -ld2d1 -ld3d11 -ldxguid -luuid -lole32
    printf 'Reference probe: %s\n' "$work_root/reference/$probe.exe"
done
