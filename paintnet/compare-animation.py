#!/usr/bin/env python3
"""Compare every deterministic UIAnimation reference record and validate timer readings."""
import argparse
import json
import math
from pathlib import Path


def read(path):
    records = [json.loads(line) for line in Path(path).read_text().splitlines() if line.startswith('{')]
    timers = [row for row in records if 'timer_time' in row]
    if len(timers) != 2 or not all(math.isfinite(row['timer_time']) for row in timers):
        raise ValueError(f'{path}: missing or invalid timer samples')
    if timers[0]['timer_time'] <= 0 or abs(timers[0]['timer_time'] - timers[0]['qpc']) > .1:
        raise ValueError(f'{path}: timer does not follow QueryPerformanceCounter')
    if timers[1]['timer_time'] - timers[0]['timer_time'] < .01:
        raise ValueError(f'{path}: timer failed to advance across Sleep(20)')
    records = [row for row in records if 'timer_time' not in row]
    if sum('case' in row for row in records) != 11:
        raise ValueError(f'{path}: expected eleven complete cases')
    return records


def equal(a, b):
    if isinstance(a, dict):
        return isinstance(b, dict) and a.keys() == b.keys() and all(equal(a[key], b[key]) for key in a)
    if isinstance(a, list):
        return isinstance(b, list) and len(a) == len(b) and all(equal(x, y) for x, y in zip(a, b))
    if isinstance(a, (float, int)) and isinstance(b, (float, int)):
        return math.isfinite(a) and math.isfinite(b) and math.isclose(a, b, rel_tol=1e-12, abs_tol=1e-12)
    return a == b


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('windows')
    parser.add_argument('wine')
    args = parser.parse_args()
    expected, actual = read(args.windows), read(args.wine)
    if len(expected) != len(actual):
        raise ValueError(f'Record counts differ: {len(expected)} versus {len(actual)}')
    errors = [(i, a, b) for i, (a, b) in enumerate(zip(expected, actual)) if not equal(a, b)]
    for i, a, b in errors[:20]:
        print(f'Record {i}: expected {a}; got {b}')
    print(f'{len(expected)} records, {len(errors)} mismatches; timer invariants passed.')
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
