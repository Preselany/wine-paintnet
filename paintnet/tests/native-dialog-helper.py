#!/usr/bin/python3
"""Deterministic transport peer for the Wine native_dialog regression, not a UI."""
import os
import sys
import time

fields = sys.stdin.buffer.read().decode('utf-8').rstrip('\0').split('\0')
assert fields[0] == 'PDNFD1'
mode, folder, name = fields[4], fields[6], fields[7]
one = os.path.join(folder, 'native α one.png')
two = os.path.join(folder, 'native β two.png')
new = os.path.join(folder, 'native γ saved.png')
if mode == 'wait':
    time.sleep(30)
    result = ['CANCEL']
elif mode == 'cancel':
    result = ['CANCEL']
elif mode == 'bad_cancel':
    result = ['CANCEL', 'extra']
elif mode == 'bad_index':
    result = ['OK', '-1', one]
elif mode == 'bad_path':
    result = ['OK', '0', one, 'relative.png']
elif mode == 'multi':
    result = ['OK', '1', one, two]
elif mode == 'save':
    result = ['OK', '1', new]
elif mode == 'veto':
    result = ['OK', '1', two if name == 'retry' else one]
elif mode == 'overwrite':
    result = ['OK', '1', new if name == 'retry' else one]
else:
    result = ['OK', '1', one]
sys.stdout.buffer.write(('\0'.join(result) + '\0').encode('utf-8'))
