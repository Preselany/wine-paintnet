# Independent Direct2D measurements

Wine-only pixel tests can verify an implementation but cannot prove it matches
Microsoft's implementation. This matters for effects whose exact numerical
behavior is not specified in the public documentation.

The focused regression executable can select WARP with `D2D1_TEST_WARP=1`.
On Windows, WARP supplies Microsoft's CPU implementation of Direct3D. The
Direct2D DLL must come from the Windows installation, with no Wine DLL beside
the test executable. Normal Wine test runs leave the variable unset.

`build-reference.sh WORK_DIRECTORY` builds `reference/emboss-reference.exe`.
The probe uses only public Direct2D/Direct3D APIs. It tests seven 5x5 input
patterns, four heights, and nine directions. Each JSON line records the actual
local bounds, drawing result, and a full 7x7 floating-point target readback.
The input area is cropped to 5x5 and drawn at (1,1), leaving a visible border
for checking the written extent. The probe contains no Emboss implementation.

The same script builds `reference/effect-properties-reference.exe`, which
enumerates registered effects and reads property metadata, including nested
subproperties. Each JSON line records the effect CLSID, property path, name,
type, value size, HRESULT, and raw returned bytes. This also checks defaults and
validation metadata independently of the pixel tests. It also creates live
effect instances on WARP and records their defaults separately from factory
metadata, whose top-level values are often zero. Numeric probes check setters
and read the values back. Only public API calls are used.

Copy the executables into a directory on Windows without replacement DLLs.
For example, in PowerShell:

```powershell
$env:D2D1_TEST_WARP = '1'
.\d2d1_test.exe contrast
.\emboss-reference.exe > emboss.jsonl
.\effect-properties-reference.exe > properties.jsonl
```

The WARP driver switch leaves the normal Linux test path unchanged: all eight
focused cases pass 9,931 checks with no skipped cases after the driver switch
and test-isolation corrections.
Those counts remain Wine implementation checks, not native conformance results.

## Reference environment and first results

An isolated QEMU/KVM VM was installed from Microsoft's Windows 11
Enterprise 25H2 evaluation image. Its SHA-256 matches the published EN-US x64
value: `a61adeab895ef5a4db436e0a7011c92a2ff17bb0357f58b13bbc4062e535e7b9`.
It uses a private sparse virtual disk, two virtual CPUs, 4 GiB RAM, and a
software TPM. The original ISO remains unchanged; a small block overlay selects
the no-prompt EFI boot image already included by Microsoft for unattended boot.
The guest network is restricted to an explicitly forwarded local test service.
The service transfers this project's test programs and collects their results.

Sources: [Microsoft evaluation downloads](https://www.microsoft.com/en-us/evalcenter/download-windows-11-enterprise)
and [published hash list](https://go.microsoft.com/fwlink/?linkid=2334901).

The guest reports Windows 10.0.26200.0; its system Direct2D reports file version
10.0.26100.1. The initial Emboss run produced 252 successful pixel readbacks
with no drawing failures. The updated property probe captured 9,100 records
covering 66 effects, including live defaults and numeric setter results.

The first comparison exposed differences in the Wine implementation and in
assumptions made by its tests. Several Windows setters clamp numeric inputs
instead of rejecting them. Tests now restore valid properties before checking
pixels, so those differences do not contaminate later cases. The custom
identity test also needed the required DisplayName subproperty in its XML.

After those test corrections, the native batch ran 9,927 checks, with 283
failures against the existing expectations and no skips:

| Case | Checks | Mismatches |
| --- | ---: | ---: |
| COM identity | 28 | 0 |
| Effect context | 71 | 12 |
| Histogram | 131 | 17 |
| Opacity Metadata | 47 | 1 |
| Alpha Mask | 488 | 25 |
| Convolve Matrix | 1,498 | 161 |
| Contrast | 332 | 55 |
| Bitmap Source | 7,332 | 12 |

These are baseline discrepancies, not passing conformance claims. In particular,
Contrast needs a different curve coefficient, alpha-ignore inputs behave
differently inside effects, convolution has bounds/origin differences, and
Histogram output sizing and crop behavior need correction. Emboss measurements
show surface lighting rather than a simple linear edge convolution.

Installation files, local
credentials, proprietary binaries, VM state, and raw measurement output remain
outside the source repository in the work directory. The evaluation VM is a
measurement host; Paint.NET's Linux target continues to use Wine's builtin
Direct2D.

An earlier DLL-only reference attempt used a Microsoft symbol-server copy of
Direct2D 10.0.19041.329 in a separate Wine prefix. Factory creation succeeded,
but metadata loading failed with ERROR_RESOURCE_TYPE_NOT_FOUND for its external
SYSTEMPROPERTIES.XML resource. That attempt produced no rendering-conformance
evidence and is not used as a reference result.
