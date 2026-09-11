# Independent Direct2D measurements

Wine-only pixel tests can verify an implementation but cannot prove it matches
Microsoft's implementation. This matters for effects whose exact numerical
behavior is not specified in the public documentation.

The focused regression executable can select WARP with `D2D1_TEST_WARP=1`.
On Windows, WARP supplies Microsoft's CPU implementation of Direct3D. The
Direct2D DLL must come from the Windows installation, with no Wine DLL beside
the test executable. Normal Wine test runs leave the variable unset.

`build-reference.sh WORK_DIRECTORY` builds `reference/emboss-reference.exe`.
The probe uses only public Direct2D/Direct3D APIs. It tests eleven 5x5 input
patterns, five heights, and nine directions at 96/192 DPI, in DIP/pixel units,
with default and explicit 32-bit float precision. Each JSON line records the actual
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

## First corrections verified against Windows

The next native batch passes COM identity (28), Opacity Metadata (47), Alpha
Mask (488), Contrast (356), and Convolve Matrix (2,286), with no skips. Wine
passes the same rendering expectations; its COM case executes 26 checks because
callback counts differ. All eight Wine cases total 10,743 passing checks. The
remaining native discrepancies are Effect Context (12), Histogram (17), and
Bitmap Source (12); they are still open.

The kernel probe now checks all nine one-hot 3x3 kernels, a full kernel, and a
zero kernel, recording both local bounds and pixels through an explicit crop.
It also measures short kernel-property storage. Native results establish kernel
direction and bounds trimming rather than merely testing the Wine formula.
Short arrays can be stored; rendering arrays with a different length from the
configured dimensions is still unsupported in this fork.

The expanded Emboss run completed 2,970 cases with no drawing failures. It
confirms that DPI changes include an internal resampling step, while pixel units
restore the original sample grid. For the captured cases, requesting 32-bit
float precision did not change the output. The exact implementation is still
under development; these measurements do not establish a Wine Emboss result.

## Emboss implementation and comparison

Emboss now renders through Wine's builtin Direct2D compute path. Public-API
impulse sweeps on 5x5 and 8x8 images recovered the same nine boundary stencils.
Each coefficient is an exact multiple of 1/1024. For each input basis image,
the normal components were recovered from lighting at 0/90/180/270 degrees;
height was 0.125, light elevation 50 degrees, and grayscale channel weights
were measured as 0.299/0.587/0.114. The formula and captured coefficients predict
independent mixed-color/HDR images to within two millionths. This is behavioral
measurement, not copied Microsoft implementation code.

`emboss-reference.exe --basis` and `emboss-grid8-reference.exe --basis`
reproduce those input sweeps. The normal probe runs all 2,970 cases. Compare
complete native and Wine JSON files with:

```sh
./paintnet/compare-emboss.py windows.jsonl wine.jsonl
```

The comparison validates case identity/count, successful calls, complete finite
readbacks, bounds, and every RGBA component. At the default 0.00002 tolerance,
all 2,970 cases and 582,120 channels match. Maximum absolute error is 0.00000164.
The separate 8x8 basis run matches all 256 cases and 102,400 channels, with a
maximum difference of 0.000000119. The focused Wine/Windows test passes 4,919
checks using 23 captured pixel fixtures, property clamps, and graph recovery.

The implementation computes luminance gradients on the effect's 96-DPI grid,
then applies diffuse lighting. DPI resampling uses a second GPU pass when
needed. It preserves HDR input values and produces opaque grayscale output.
Current limitations include feature level 11.0, no output cache, and the shared
integer-pixel bounds representation. Fractional output extents or origins that
do not align with the working grid return E_NOTIMPL. Tests cover 96/192 DPI and
pixel units; this is not a claim of full arbitrary-DPI or hardware conformance.
