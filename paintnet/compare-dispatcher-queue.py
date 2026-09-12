#!/usr/bin/env python3
"""Compare dispatcher-queue-reference output from Windows and Wine."""
import argparse
import difflib
from pathlib import Path


def records(path):
    lines = Path(path).read_text(encoding="utf-8-sig").splitlines()
    try:
        lines = lines[lines.index("INIT 00000000"):]
    except ValueError as error:
        raise ValueError(f"{path}: no successful initialization") from error
    if "AFTER_PUMP status=1 calls=6" not in lines and "AFTER_PUMP status=1 calls=8" not in lines:
        raise ValueError(f"{path}: queue did not complete all callbacks and shutdown")
    if not lines or not lines[-1].startswith(("EVENT_REF ", "STARTING_REF ")):
        raise ValueError(f"{path}: incomplete reference log")
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("windows_log")
    parser.add_argument("wine_log")
    args = parser.parse_args()
    try:
        expected, actual = records(args.windows_log), records(args.wine_log)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")
    if expected != actual:
        print("\n".join(difflib.unified_diff(expected, actual, args.windows_log,
                                             args.wine_log, lineterm="")))
        return 1
    print(f"All {len(expected)} dispatcher queue records match Windows exactly.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
