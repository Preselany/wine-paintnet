#!/usr/bin/env bash
set -euo pipefail
[[ $# -ge 2 ]] || { echo 'Usage: install-modules.sh WORK_DIRECTORY MODULE...' >&2; exit 2; }
work_root="$(realpath -e -- "$1")"
shift
export WINEPREFIX="$work_root/prefix"
wine_root="$work_root/wine/opt/wine-devel"
# Wait for the development prefix to exit normally. Never kill an editing session.
"$wine_root/bin/wineserver" -w
mkdir -p "$work_root/stock-modules"
for module in "$@"; do
    [[ "$module" =~ ^[a-z0-9_]+$ ]] || exit 2
    built="$work_root/build/dlls/$module/x86_64-windows/$module.dll"
    test -s "$built"
    if [[ ! -e "$work_root/stock-modules/$module.dll" ]]; then
        cp "$wine_root/lib/wine/x86_64-windows/$module.dll" "$work_root/stock-modules/"
    fi
    cp "$built" "$wine_root/lib/wine/x86_64-windows/$module.dll"
    cp "$built" "$WINEPREFIX/drive_c/windows/system32/$module.dll"
    if [[ "$module" == coremessaging ]]; then
        # A full Wine prefix setup imports classes.idl's registry resource.
        # A module-only update also needs the newly introduced runtime class.
        WINEDEBUG=-all "$wine_root/bin/wine" reg add \
            'HKLM\Software\Microsoft\WindowsRuntime\ActivatableClassId\Windows.System.DispatcherQueue' \
            /v DllPath /t REG_SZ /d 'C:\windows\system32\coremessaging.dll' /f
    fi
    if [[ "$module" == dcomp ]]; then
        WINEDEBUG=-all "$wine_root/bin/wine" reg add \
            'HKLM\Software\Microsoft\WindowsRuntime\ActivatableClassId\Windows.UI.Composition.Core.CompositorController' \
            /v DllPath /t REG_SZ /d 'C:\windows\system32\dcomp.dll' /f
    fi
done
