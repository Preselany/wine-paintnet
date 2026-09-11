#!/usr/bin/env python3
"""Compare colors represented by exported native and Wine ICC profiles using LCMS.

Input: complete color-context-icc-reference logs. This checks ICC color
semantics independently of Direct2D rendering; serialization need not match.
Requires the system liblcms2 shared library.
"""
import argparse
import ctypes as C
import ctypes.util
import itertools
import math
from pathlib import Path


def profiles(path):
    lines = Path(path).read_text().splitlines()
    if not lines or lines[-1] != "done":
        raise ValueError(f"Incomplete measurement: {path}")
    result = [bytes.fromhex(line.split()[1]) for line in lines if line.startswith("profile_data ")]
    if len(result) != 3 or any(len(data) < 128 or data[36:40] != b"acsp" for data in result):
        raise ValueError(f"Expected three valid ICC profiles: {path}")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("windows")
    parser.add_argument("wine")
    args = parser.parse_args()
    native, wine = profiles(args.windows), profiles(args.wine)
    library = ctypes.util.find_library("lcms2")
    if not library:
        parser.exit(1, "liblcms2 is required.\n")
    lib = C.CDLL(library)

    def bind(name, arguments, result):
        func = getattr(lib, name)
        func.argtypes, func.restype = arguments, result
        return func

    open_profile = bind("cmsOpenProfileFromMem", [C.c_void_p, C.c_uint32], C.c_void_p)
    create_xyz = bind("cmsCreateXYZProfile", [], C.c_void_p)
    create = bind("cmsCreateTransform", [C.c_void_p, C.c_uint32, C.c_void_p, C.c_uint32,
                                         C.c_uint32, C.c_uint32], C.c_void_p)
    run = bind("cmsDoTransform", [C.c_void_p, C.c_void_p, C.c_void_p, C.c_uint32], None)
    close = bind("cmsCloseProfile", [C.c_void_p], None)
    delete = bind("cmsDeleteTransform", [C.c_void_p], None)
    xyz = create_xyz()
    if not xyz:
        raise RuntimeError("Cannot create XYZ profile")
    failed = False
    total = 0
    try:
        for index, name in enumerate(("sRGB", "scRGB", "Adobe RGB")):
            for label, values, tolerance in (("unit", [i/8 for i in range(9)], 0.0005),
                                             ("extended", [-0.5, 0, 0.04045, 0.5, 1, 1.5, 2], 0.001)):
                samples = list(itertools.product(values, repeat=3))
                channels = 3*len(samples)
                inputs = (C.c_float*channels)(*(v for sample in samples for v in sample))
                results = []
                for data in (native[index], wine[index]):
                    profile = open_profile(data, len(data))
                    if not profile:
                        raise ValueError(f"LCMS rejected {name} profile")
                    try:
                        # TYPE_RGB_FLT -> TYPE_XYZ_FLT, relative colorimetric,
                        # no optimization: exercise each profile's actual tags.
                        transform = create(profile, (1 << 22) | (4 << 16) | (3 << 3) | 4,
                                           xyz, (1 << 22) | (9 << 16) | (3 << 3) | 4, 1, 0x100)
                        if not transform:
                            raise ValueError(f"Cannot transform {name} profile")
                        try:
                            output = (C.c_float*channels)()
                            run(transform, inputs, output, len(samples))
                            results.append(list(output))
                        finally:
                            delete(transform)
                    finally:
                        close(profile)
                errors = [abs(a-b) for a, b in zip(*results)]
                invalid = sum(not math.isfinite(error) or error > tolerance for error in errors)
                total += channels
                failed |= bool(invalid)
                print(f"{name}, {label}: {len(samples)} colors, {channels} XYZ channels, "
                      f"maximum error {max(errors):.9g}, tolerance {tolerance:g}, {invalid} failures.")
    finally:
        close(xyz)
    print(f"{total} channel comparisons; {'FAIL' if failed else 'PASS'}. "
          "ICC bytes and Direct2D rendering are separate checks.")
    return failed


if __name__ == "__main__":
    raise SystemExit(main())
