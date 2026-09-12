#!/usr/bin/env bash
set -euo pipefail
here="$(cd -- "$(dirname -- "$0")" && pwd)"
work_root="$(realpath -e -- "${1:-$here/work}")"
export WINEPREFIX="$work_root/prefix" WINEDEBUG="${WINEDEBUG:--all}"
export WINE_NATIVE_FILE_DIALOG="$here/tests/native-dialog-helper.py" WINE_NATIVE_DIALOG_TEST=1
exec "$work_root/wine/opt/wine-devel/bin/wine" \
    "$work_root/build/dlls/comdlg32/tests/x86_64-windows/comdlg32_test.exe" native_dialog
