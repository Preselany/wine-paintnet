#!/usr/bin/env python3
"""Compare complete public-API Emboss readbacks, including missing/failed cases."""
import argparse
import json
import math
from pathlib import Path


def load(path):
    cases = {}
    for number, line in enumerate(path.read_text().splitlines(), 1):
        row = json.loads(line)
        key = (row.get('side', 5), row['dpi'], row['unit_mode'], row['precision'],
               row['pattern'], row['height'], row['direction'])
        if key in cases:
            raise ValueError(f'{path}:{number}: duplicate case {key}')
        if row['draw_hr'] or row['bounds_hr']:
            raise ValueError(f'{path}:{number}: failed API call in {key}')
        if len(row['bounds']) != 4 or len(row['pixels']) != (key[0] + 2) ** 2:
            raise ValueError(f'{path}:{number}: incomplete bounds or readback')
        values = list(row['bounds'])
        for pixel in row['pixels']:
            if len(pixel) != 4:
                raise ValueError(f'{path}:{number}: incomplete pixel')
            values.extend(pixel)
        if not all(math.isfinite(value) for value in values):
            raise ValueError(f'{path}:{number}: nonfinite output')
        cases[key] = row
    if not cases:
        raise ValueError(f'{path}: empty measurement file')
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('windows', type=Path)
    parser.add_argument('wine', type=Path)
    parser.add_argument('--tolerance', type=float, default=0.00002)
    args = parser.parse_args()
    if not math.isfinite(args.tolerance) or args.tolerance < 0:
        parser.error('tolerance must be finite and nonnegative')
    native, wine = load(args.windows), load(args.wine)
    if native.keys() != wine.keys():
        raise ValueError(f'Case sets differ: missing {len(native.keys()-wine.keys())}, extra {len(wine.keys()-native.keys())}')
    bad_bounds = bad_channels = channels = 0
    largest = (0.0, None, None, None)
    for key, reference in native.items():
        actual = wine[key]
        bad_bounds += any(abs(a-b) > 0.000001 for a,b in zip(reference['bounds'], actual['bounds']))
        for index, (expected, pixel) in enumerate(zip(reference['pixels'], actual['pixels'])):
            for channel, (a,b) in enumerate(zip(expected, pixel)):
                error = abs(a-b)
                channels += 1
                bad_channels += error > args.tolerance
                if error > largest[0]:
                    largest = (error, key, index, channel)
    print(f'{len(native)} cases, {channels} channels; {bad_bounds} bounds mismatches, {bad_channels} channel mismatches.')
    print(f'Maximum absolute error: {largest[0]:.9g}; case/pixel/channel: {largest[1:]}')
    return bool(bad_bounds or bad_channels)


if __name__ == '__main__':
    raise SystemExit(main())
