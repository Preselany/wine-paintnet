#!/usr/bin/env python3
"""Check the application binaries against the verified official archive."""
import hashlib
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
manifest = json.loads((root / 'app-binaries.json').read_text())
for name, expected in manifest.items():
    path = root / 'app' / name
    if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        sys.exit(f'Application binary differs from official release: {name}')
if list((root / 'app').glob('*Direct2D1.Managed*')):
    sys.exit('Unexpected managed Direct2D replacement in the classic application')
print(f'Verified {len(manifest)} original Paint.NET runtime binaries.')
