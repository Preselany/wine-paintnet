#!/usr/bin/env bash
# Rootless, isolated runtime for the unmodified stable Paint.NET application.
set -euo pipefail
here="$(cd -- "$(dirname -- "$0")" && pwd)"
work_root="$(realpath -m -- "${1:-$here/work}")"
cache="$(realpath -m -- "${PDN_CACHE_DIR:-$work_root/downloads}")"
[[ "$(uname -m)" == x86_64 ]] || { echo 'Requires x86-64' >&2; exit 1; }
[[ ! -e "$work_root/prefix" && ! -e "$work_root/app" ]] || { echo 'Refusing to replace an existing runtime' >&2; exit 1; }
mkdir -p "$cache" "$work_root/logs"
fetch() {
    local file="$1" hash="$2" url="$3"
    if [[ ! -f "$cache/$file" ]]; then
        curl --fail --location --retry 3 "$url" -o "$cache/$file.part"
        mv "$cache/$file.part" "$cache/$file"
    fi
    printf '%s  %s\n' "$hash" "$cache/$file" | sha256sum --check --status
}
archive=paint.net.5.1.12.portable.x64.zip
fetch "$archive" d5ae7043f2fb9d365b48dfe243a2aca1c74924de99b04b6445916c95354aefa3 \
    "https://github.com/paintdotnet/release/releases/download/v5.1.12/$archive"
fetch wine-devel_11.17~noble-1_amd64.deb eb42cc830c0e0582ecb1c5cf94cbfefe5d0a0235caae771035077b5e7b8ddd8f \
    'https://dl.winehq.org/wine-builds/ubuntu/pool/main/w/wine/wine-devel_11.17~noble-1_amd64.deb'
fetch wine-devel-amd64_11.17~noble-1_amd64.deb 9184a81f0848003460526f30a66bd470a997fa0bd43c0d59efcacce0a12adb63 \
    'https://dl.winehq.org/wine-builds/ubuntu/pool/main/w/wine/wine-devel-amd64_11.17~noble-1_amd64.deb'
fetch dxvk-3.1.tar.gz 30f9cc326874be344285582275446968cfa4c069db31ce56df312d6644179154 \
    https://github.com/doitsujin/dxvk/releases/download/v3.1/dxvk-3.1.tar.gz
python3 - "$cache/$archive" "$work_root" <<'PY'
import hashlib, json, sys, zipfile
from pathlib import Path
root = Path(sys.argv[2])
app = (root / 'app').resolve()
manifest = {}
with zipfile.ZipFile(sys.argv[1]) as archive:
    for entry in archive.infolist():
        name = entry.filename.replace('\\', '/')
        path = app / name
        if not path.resolve().is_relative_to(app): raise ValueError('Invalid archive path')
        if entry.is_dir():
            path.mkdir(parents=True, exist_ok=True)
        else:
            data = archive.read(entry)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            if path.suffix.lower() in ('.exe', '.dll'):
                manifest[name] = hashlib.sha256(data).hexdigest()
(root / 'app-binaries.json').write_text(json.dumps(manifest, indent=2) + '\n')
PY
for package in wine-devel wine-devel-amd64; do
    dpkg-deb -x "$cache/${package}_11.17~noble-1_amd64.deb" "$work_root/wine"
done
export WINEPREFIX="$work_root/prefix" WINEARCH=win64 WINEDEBUG=-all WINEDLLOVERRIDES='mscoree=;mshtml='
wine="$work_root/wine/opt/wine-devel/bin/wine"
"${wine%/*}/wineboot" -i > "$work_root/logs/setup.log" 2>&1
"${wine%/*}/wineserver" -w
"${wine%/*}/winecfg" -v win11 >> "$work_root/logs/setup.log" 2>&1
tar -xzf "$cache/dxvk-3.1.tar.gz" -C "$work_root" dxvk-3.1/x64/d3d11.dll dxvk-3.1/x64/dxgi.dll dxvk-3.1/x64/d3d10core.dll
for module in d3d11 dxgi d3d10core; do
    cp "$work_root/dxvk-3.1/x64/$module.dll" "$WINEPREFIX/drive_c/windows/system32/"
    "$wine" reg add 'HKCU\Software\Wine\DllOverrides' /v "$module" /t REG_SZ /d native /f >> "$work_root/logs/setup.log" 2>&1
done
"$wine" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /t REG_SZ /d x11 /f >> "$work_root/logs/setup.log" 2>&1
"${wine%/*}/wineserver" -w
python3 "$here/verify-app.py" "$work_root"
echo "Development runtime ready: $work_root"
