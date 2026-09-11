#!/usr/bin/env bash
set -euo pipefail
here="$(cd -- "$(dirname -- "$0")" && pwd)"
work_root="$(realpath -e -- "${1:-$here/work}")"
if [[ $# -gt 0 ]]; then shift; fi
python3 "$here/verify-app.py" "$work_root"
export WINEPREFIX="$work_root/prefix"
export WINEDEBUG="${WINEDEBUG:--all,+timestamp,+d2d,+loaddll}"
export WINEDLLOVERRIDES='d2d1=b;uianimation=b;d3d11=n;d3d10core=n;dxgi=n;mshtml='
unset WINE_DWM_DISABLE_COMPOSITION WINE_NATIVE_FILE_DIALOG
files=()
for file in "$@"; do
    file="$(realpath -e -- "$file")"
    files+=("Z:${file//\//\\}")
done
mkdir -p "$work_root/logs"
log="$work_root/logs/paintnet-$(date +%Y%m%d-%H%M%S)-$$.log"
printf 'Log: %s\n' "$log"
cd "$work_root/app"
exec "$work_root/wine/opt/wine-devel/bin/wine" paintdotnet.exe "${files[@]}" > "$log" 2>&1
