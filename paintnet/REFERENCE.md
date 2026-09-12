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

## Opacity

The Opacity effect multiplies premultiplied RGBA by its clamped [0,1] amount.
The independent Windows/WARP run and Wine each pass 470 checks: defaults and
clamping, stored alpha with PREMULTIPLIED/IGNORE bitmaps, HDR input, nested
opacity, cropped output, 96/192 DPI, pixel units, graph errors and recovery,
and source preservation. Native batch: 013-opacity.

The implementation uses the existing GPU compute path and preserves input
bounds even when opacity is zero. Like the other new compute effects it
currently requires feature level 11.0 and uses uncached float intermediates.
NaN-property behavior and arbitrary graph types are not established by this
comparison. API documentation: [Microsoft Opacity effect](https://learn.microsoft.com/en-us/windows/win32/direct2d/opacity-effect).

## UIAnimation

The Windows public-API probe captures eleven timelines: smooth-stop with six
initial velocities, descending and unchanged targets, linear and instantaneous
transitions. It records values, previous/final values, integer rounding, duration
queries, manager/storyboard states, current storyboard, update results, ordered
callbacks, rescheduling, shutdown, and timer/performance-counter readings.

Windows batches 014-animation and 015-animation-events establish the baseline.
The implementation matches all 757 deterministic records at 1e-12 tolerance;
timer values are checked against each host's performance counter and advancement
across Sleep(20), since absolute times differ between hosts. Reproduce with
`compare-animation.py windows.log wine.log`. The eleven captured fixtures and
callback sequences also pass 2,114 checks on both Wine and Windows (batch
016-animation-timeline).

Smooth-stop uses a parabolic stop when 2*distance/initial_velocity is positive
and within the maximum duration, or a cubic Hermite curve otherwise. A zero
distance finishes immediately. Native notifications skip PLAYING for zero
duration and deliver storyboard callbacks before variable changes. Both details
were measured independently rather than inferred from the Wine implementation.

Keyframes, conflict arbitration, custom interpolators, timer update callbacks,
other transition types, and complete pause/bounds behavior are not covered by
this reference comparison and remain incomplete.

## Shader reflection

`shader-reflection-reference.exe` compiles eighteen small HLSL shaders using
the native system compiler and captures their bytecode, shader versions,
minimum feature levels and requirement flags. Inputs include vertex, pixel and
compute profiles 4.0/4.1/5.0; vertex/pixel level_9_1 and level_9_3; early
depth/stencil, double arithmetic, 11.1 double extensions, minimum precision and
vertex UAV access. Windows batch 017-shader-reflection supplies these fixtures.

The level_9_1 and level_9_3 profiles share the primary 4.0 version but differ in
the legacy Aon9 shader's 2_0/2_1 version token. Early depth/stencil may appear
only in dcl_globalFlags, without an SFI0 chunk. Optional double and vertex-UAV
features still report feature level 11.0; their flags are independent capability
requirements. The implementation reads both declaration and SFI0 bits.

The exact captured bytecodes are regression fixtures with their own HLSL inputs
recorded in the probe source. All 72 checks pass on Wine and Windows (batch
018-shader-requirements), avoiding differences between compilers. Other shader
models, malformed-container compatibility and all optional-feature combinations
are not established by this comparison.

## Custom source draw transforms

`draw-transform-reference.exe` measures draw-info validation, constant buffers,
scene coordinates, bounds, and RGBA pixels using public Direct2D APIs and its own
HLSL shader. It uses the Microsoft WARP device when `D2D1_TEST_WARP=1` is set.
The optional modes `D2D1_PROBE_CACHED`, `D2D1_PROBE_SCENE`, and
`D2D1_PROBE_NESTED` select cached output, the raw scene-position vector, and an
outer Opacity effect respectively. Unset each unused mode before a run; an
inherited environment variable selects that mode even if its value is `0`.

The native and Wine `draw_transform` regression additionally tests direct and
nested paths in one process, with both caching settings, caller-data ownership,
all supported precision/depth enum values, invalid settings, 96/192 DPI,
negative finite bounds, and an infinite source cropped before drawing.

Compare matching standalone modes with:

```sh
python3 paintnet/compare-draw-transform.py windows.log wine.log
```

Each standalone mode has 72 pixel cases, 18,432 RGBA channels, and 320 complete
records. Redundant bounds-mapping count differences are reported separately.
The comparison uses an absolute pixel tolerance of 2e-5 and does not
accept an incomplete probe. Source transforms with image inputs, custom vertex
processing, general transform graphs, arbitrary affine transforms, non-float32
final targets, and performance require further independent measurements.

## Command-list recording and lifecycle

`command-list-reference.exe` exercises 23 call sequences involving BeginDraw,
EndDraw, target changes, state setters, drawing, closing, and streaming. A public
ID2D1CommandSink replays the recorded state and rectangles into a separate bitmap
context. The 27 resulting 8×8 images use exactly representable RGBA values.

`command-context-reference.exe` adds seven cases for shared versus different
Direct2D devices, competing writers, sequential reuse, multiple inactive
bindings, and destruction of the creating context. It uses the same sink for
record inspection without replaying across resource domains.

Both programs support `D2D1_TEST_WARP=1` for the Windows software reference.
Compare their labeled text records with:

```sh
python3 paintnet/compare-command-list.py windows.log wine.log
```

The focused command_list_state test includes the measured ordered records and
compact palette-based pixel fixtures. It checks recording and public-sink
replay; automatic Direct2D command-list rasterization still needs implementation.

## Color-profile resources

color-context-reference.exe measures ICC/DXGI/simple creation, QI and factory
ownership, profile buffers, WIC EXIF input, filenames, and bitmap references.
color-context-icc-reference.exe additionally exports three complete ICC profiles
and measures copying, WIC/file classification, model/ID mutations, and trailing
data. Both use D2D1_TEST_WARP=1 on Windows and system Direct2D. Use an isolated
working directory; they write disposable .icc files there.

The color_context regression checks portable API behavior. It deliberately
does not require a Windows-specific ICC size, serialization, or timestamp.
Unsupported QI preserves the output pointer on the native color-context object.
WIC/file classification differs from the memory-creation API; see the regression
for the independently measured model/ID precedence.

Compare the color semantics of the exported profiles with:

```sh
python3 paintnet/compare-color-profiles.py windows-icc.log wine-icc.log
```

The comparison requires host liblcms2. It uses the same engine for both profiles,
relative colorimetric RGB-to-XYZ transforms, and no optimization. It rejects
incomplete logs, rejected profiles, non-finite values, and out-of-tolerance
channels. This measures the profiles themselves, not Direct2D image rendering.
Generated profile metadata and the reserved Windows scRGB profile ID differ;
these are documented compatibility limits, not hidden by a byte-equality claim.

API documentation: [CreateColorContext](https://learn.microsoft.com/en-us/windows/win32/api/d2d1_1/nf-d2d1_1-id2d1devicecontext-createcolorcontext),
[GetProfile](https://learn.microsoft.com/en-us/windows/win32/api/d2d1_1/nf-d2d1_1-id2d1colorcontext-getprofile),
and [DXGI color contexts](https://learn.microsoft.com/en-us/windows/win32/api/d2d1_3/nf-d2d1_3-id2d1devicecontext5-createcolorcontextfromdxgicolorspace).
Native measurements establish the discrepancies from the documented memory-API
classification and unsupported-QI behavior.

## Dispatcher queues

Build dispatcher-queue-reference.exe with build-reference.sh, then run the same
executable against system CoreMessaging on Windows and the fork on Wine. Run
once normally and once with QUEUE_DEFERRAL=1. Compare corresponding logs with:

```sh
python3 paintnet/compare-dispatcher-queue.py windows.log wine.log
```

Normal mode checks priorities, queue identity, and shutdown. Deferral mode also
checks ShutdownStarting ordering, enqueuing while shutdown is deferred, and
completing that deferral from another thread. QUEUE_REPEAT_SHUTDOWN=1 is a
separate diagnostic: on the tested Windows build its second call fails and
leaves the first action pending. The normal comparator deliberately rejects
that incomplete-shutdown diagnostic.

API documentation: [GetForCurrentThread](https://learn.microsoft.com/en-us/uwp/api/windows.system.dispatcherqueue.getforcurrentthread),
[ShutdownQueueAsync](https://learn.microsoft.com/en-us/uwp/api/windows.system.dispatcherqueuecontroller.shutdownqueueasync),
and [CreateDispatcherQueueController](https://learn.microsoft.com/en-us/windows/win32/api/dispatcherqueue/nf-dispatcherqueue-createdispatcherqueuecontroller).
