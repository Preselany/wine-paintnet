#!/usr/bin/env python3
"""Compare public-API custom draw-transform measurements from Windows and Wine."""
import argparse
import math
from pathlib import Path


KINDS = {"buffer", "constant", "constant_null_zero", "missing_shader", "shader_options",
         "input_zero", "prepare", "output", "bounds", "draw", "pixels", "FAILED"}


def records(path):
    return [line.split() for line in Path(path).read_text().splitlines()
            if line.split() and line.split()[0] in KINDS]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("windows")
    parser.add_argument("wine")
    args = parser.parse_args()
    native, wine = records(args.windows), records(args.wine)
    if any(row[0] == "FAILED" for row in native + wine):
        parser.exit(1, "A probe did not complete successfully.\n")
    if len(native) != len(wine):
        parser.exit(1, f"Record count differs: Windows {len(native)}, Wine {len(wine)}.\n")
    mismatches = []
    pixels = channels = mapping_count_differences = 0
    maximum_error = 0.0
    for i, (expected, actual) in enumerate(zip(native, wine)):
        if len(expected) != len(actual) or expected[0] != actual[0]:
            mismatches.append((i, expected[:8], actual[:8]))
            continue
        if expected[0] == "draw" and expected[6] != actual[6]:
            # Cached Windows transforms may be mapped redundantly. Require both
            # runs to map bounds, but do not make an internal repeat observable.
            if int(expected[6]) >= 1 and int(actual[6]) >= 1:
                mapping_count_differences += 1
                expected = expected.copy()
                expected[6] = actual[6]
        numeric_start = 4 if expected[0] == "pixels" else 5 if expected[0] == "bounds" else len(expected)
        if expected[:numeric_start] != actual[:numeric_start]:
            mismatches.append((i, expected[:numeric_start], actual[:numeric_start]))
        if expected[0] == "pixels":
            pixels += 1
            channels += len(expected) - 4
        for channel, (left, right) in enumerate(zip(expected[numeric_start:], actual[numeric_start:])):
            left, right = float(left), float(right)
            maximum_error = max(maximum_error, abs(left-right))
            if not math.isclose(left, right, abs_tol=2e-5, rel_tol=1e-7):
                mismatches.append((i, channel, left, right))
    if not pixels:
        parser.exit(1, "No rendered pixel cases were found.\n")
    print(f"{len(native)} records, {pixels} pixel cases, {channels} channels; "
          f"{len(mismatches)} mismatches, maximum numeric error {maximum_error:.9g}.")
    if mapping_count_differences:
        print(f"{mapping_count_differences} differences in redundant bounds-mapping counts (not pixel/API mismatches).")
    for mismatch in mismatches[:12]:
        print(mismatch)
    return bool(mismatches)


if __name__ == "__main__":
    raise SystemExit(main())
