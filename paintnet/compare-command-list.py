#!/usr/bin/env python3
"""Compare public command-list recording, ownership, and replay measurements."""
import argparse
import math
from pathlib import Path

KINDS = {"case", "cmd", "target", "end", "end_first", "end_second", "close",
         "stream", "stream_begin", "stream_end", "pixels", "FAILED"}


def records(path):
    return [line.split() for line in Path(path).read_text().splitlines()
            if line.split() and line.split()[0] in KINDS]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("windows")
    parser.add_argument("wine")
    args = parser.parse_args()
    native, wine = records(args.windows), records(args.wine)
    if not native or not wine or any(row[0] == "FAILED" for row in native + wine):
        parser.exit(1, "Missing or incomplete probe output.\n")
    if len(native) != len(wine):
        parser.exit(1, f"Record counts differ: Windows {len(native)}, Wine {len(wine)}.\n")
    mismatches = []
    images = channels = 0
    maximum_error = 0.0
    for index, (expected, actual) in enumerate(zip(native, wine)):
        if expected[0] != "pixels":
            if expected != actual:
                mismatches.append((index, expected, actual))
            continue
        images += 1
        if expected[:3] != actual[:3] or len(expected) != len(actual):
            mismatches.append((index, expected[:3], actual[:3]))
            continue
        for channel, (left, right) in enumerate(zip(expected[3:], actual[3:])):
            channels += 1
            left, right = float(left), float(right)
            maximum_error = max(maximum_error, abs(left-right))
            if not math.isclose(left, right, abs_tol=1e-7, rel_tol=0):
                mismatches.append((index, channel, left, right))
    print(f"{len(native)} records, {images} images, {channels} RGBA channels; "
          f"{len(mismatches)} mismatches, maximum pixel error {maximum_error:.9g}.")
    for mismatch in mismatches[:12]:
        print(mismatch)
    return bool(mismatches)


if __name__ == "__main__":
    raise SystemExit(main())
