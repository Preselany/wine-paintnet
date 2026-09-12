# Validation — September 11, 2026

## Reproduction

* Wine source: `36b6a2cf679fb395f668a917b76537190e212d9c` (11.17).
* Target: official Paint.NET 5.1.12 x64 portable release.
* Host: Ubuntu 24.04.4 x86-64.
* Test display: isolated Xvfb, with DXVK 3.1 selecting llvmpipe.
* Only the rebuilt Wine `d2d1.dll` was installed for the first compatibility fix.
* All 312 application EXE/DLL files matched the verified official archive.

The setup script completed against a second clean work directory using the
pinned download cache. Building the Direct2D module and test executable,
installing the module, launching the application, and invoking the focused
test script were exercised. Shell syntax checks and Python compilation passed.
An intentionally altered executable in the disposable reproduction runtime was
correctly rejected by the integrity checker and then restored and reverified.

## Direct2D custom-effect identity regression

The same final test executable was run against two independent prefixes:

| Runtime | Checks executed | Failures | Skipped |
| --- | ---: | ---: | ---: |
| Stock WineHQ 11.17 | 17 | 6 | 0 |
| Wine with the interface fix | 26 | 0 | 0 |

The patched run reaches additional property round-trip checks because effect
creation succeeds. The stock run fails effect creation, skips those dependent
checks, and also fails initialization, interface-rejection, and failure-code
checks. Both runs exercise the builtin Flood property path.

These are COM initialization, property, and lifetime tests, not pixel-rendering
tests. They have not yet been run against Microsoft's Direct2D on Windows.

## Application retest

The normal application loads Wine's builtin `d2d1.dll`. The custom device-feature
effect reaches its `ID2D1EffectContext1` query and reports `E_NOINTERFACE`, rather
than invoking the wrong implementation vtable. The earlier disposal access
violation was not observed in this run.

Startup still fails. The trace records the missing Histogram effect and missing
effect-context interface; display-aware window creation also encounters
`REGDB_E_CLASSNOTREG` for `Windows.System.DispatcherQueue`. See STATUS.md.

The entire Wine test suite and the real GPU rendering path have not been
validated for this fork. The later Direct2D suite comparison is recorded below. There is no claim yet of usable application editing.

## EffectContext1 and lookup-table resource validation

The rebuilt Direct2D module and test executable compile successfully. The same
`effect_context` test executable was run against stock Wine, the first-fix
module, and the new module:

| Runtime | Checks executed | Failures | Skipped |
| --- | ---: | ---: | ---: |
| Stock WineHQ 11.17 | 19 | 7 | 0 |
| First-fix fork module | 19 | 7 | 0 |
| EffectContext1/resource module | 71 | 0 | 0 |

The seven baseline failures are the versioned-interface query, five resource
formats, and a padded-data resource. The new module also reaches the dependent
COM identity, inherited DPI/feature query, factory ownership, and interface
checks. Invalid sizes/strides/extents are rejected. The unchanged
`effect_identity` regression remains at 26 checks, zero failures, zero skips.

`paintnet/test-upload.sh` compiles and runs a separate internal diagnostic
against the installed module, after checking that it matches the build. It
reads the actual Direct3D texture back through a staging resource and compares
120 rows (240 RGBA texels), covering all five buffer precisions with both tight
and padded layouts. The input buffer is overwritten after creation. All rows
match, demonstrating that the module copies the supplied data with the correct
strides. This uses the fork's private structure layout and is deliberately not
a Windows conformance test. It does not exercise the LookupTable3D image effect
or validate the rendering of a Paint.NET image.

API references: Microsoft's [EffectContext1 interface](https://learn.microsoft.com/en-us/windows/win32/api/d2d1effectauthor_1/nn-d2d1effectauthor_1-id2d1effectcontext1)
and [lookup-table resource documentation](https://learn.microsoft.com/en-us/windows/win32/direct2d/3d-lookup-table-effect).
The five precision/format mappings also appear in Microsoft's
[Win2D implementation](https://github.com/microsoft/Win2D/blob/winappsdk/main/winrt/lib/effects/EffectTransferTable3D.cpp).
Exact Windows argument-validation behavior and low-feature-level fallback
remain unverified.

## Application retest after the resource change

Launch integrity checks passed for all 312 binaries, and the trace confirms
Wine's builtin Direct2D module. The first missing effect remains Histogram.
This run did not reach the custom feature-effect initializer. The expanded
.NET error dialog still identifies `Windows.System.DispatcherQueue` activation
with `0x80040154`. No editor opened. The test process was closed in the isolated
prefix after recording the failure.

Local evidence lives outside this source repository under the work directory:
`logs/effect-context-stock.log`, `logs/effect-context-before.log`,
`logs/effect-context-after.log`, `logs/focused-script-context1.log`,
`logs/upload-script-context1.log`,
`logs/paintnet-20260911-203506-127782.log`, and
`logs/context1-startup-details.txt`.

## Existing Direct2D suite comparison

The unmodified upstream `d2d1` test case was run with `--single` on the stock
runtime and on the rebuilt fork, using the same executable and llvmpipe/DXVK
configuration. Both completed with exactly the same summary: 17,151 checks,
243 marked todo, two reported failures, and one skip. The two reported failures
are identical unexpected successes in existing todo blocks for bitmap mapping
at `d2d1.c:15662`; neither is a new failing assertion introduced by this change.
The skip is the unavailable Direct3D reference device. The suite includes
existing bitmap/effect rendering comparisons. This is a baseline comparison,
not an assertion that all of Direct2D is correct.

Evidence: `logs/d2d1-suite-stock.log` and `logs/d2d1-suite-context1.log` in the
work directory. Both focused-test scripts and their shell syntax checks also
passed. The internal diagnostic compiles with `-Wall -Wextra -Werror`.

## Histogram and Opacity Metadata

The final focused test script now includes four test cases. Histogram checks
normalized bin values for all four channels, unpremultiplication, transparent
pixels, clamping, repeat draws without accumulation, crops, changing bin counts,
replacing the source, BGRA input with ignored alpha, and input wrapped in
Opacity Metadata. It also verifies all eight target pixels remain unchanged by
analysis. The earlier version without the metadata-input case passed 115 checks;
the final version also covers DIP/pixel crops on a 192 DPI bitmap and passes
123.

Opacity Metadata compares direct drawing with a two-effect chain and compares
the resulting pixels with the original premultiplied pixels. A second draw uses
an image crop and target offset. Tests also cover property defaults/round trips,
local bounds, cyclic input rejection, and successful drawing after breaking the
cycle. It passes 47 checks.

| Test | Stock WineHQ 11.17 | Rebuilt fork |
| --- | --- | --- |
| Histogram | Registration missing; 2 checks, 1 failure | 123 checks, 0 failures |
| Opacity Metadata | Effect missing; 5 checks, 1 failure | 47 checks, 0 failures |
| Earlier custom COM identity | Previously recorded 6 failures | 26 checks, 0 failures |
| EffectContext1/resources | Previously recorded 7 failures | 71 checks, 0 failures |

These shader tests used DXVK with llvmpipe on the isolated X server. They have
not yet been compared against native Windows Direct2D. The Histogram bin range
follows the 2–1024 range in Microsoft's
[Win2D histogram implementation](https://github.com/microsoft/Win2D/blob/winappsdk/main/winrt/lib/images/CanvasImage.cpp).
The normalized sum is described and used by Microsoft's
[HDR sample](https://github.com/microsoft/Windows-universal-samples/blob/main/Samples/D2DAdvancedColorImages/cpp/D2DAdvancedColorImages/D2DAdvancedColorImagesRenderer.cpp).
The channel and alpha semantics follow the
[Histogram documentation](https://learn.microsoft.com/en-us/windows/win32/direct2d/histogram).
Opacity Metadata's property default follows the
[SDK property documentation](https://learn.microsoft.com/en-us/windows/win32/api/d2d1effects/ne-d2d1effects-d2d1_opacitymetadata_prop);
its non-destructive nature is also documented by
[Paint.NET](https://paintdotnet.github.io/apidocs/api/PaintDotNet.Direct2D1.Effects.OpacityMetadataEffect.html).

The application trace first advanced from Histogram to Opacity Metadata, then
from Opacity Metadata to Alpha Mask. It still does not open an editor. The
latest crash diagnostics also report the absent `CreatePresentationFactory`
export from `dcomp.dll`. No application switches or binaries were changed.

Evidence outside the repository: `logs/histogram-stock.log`,
`logs/histogram-after.log`, `logs/opacity-stock.log`,
`logs/analysis-effects-final.log`, `logs/analysis-effects-upload.log`,
`logs/paintnet-20260911-210325-143270.log`, and
`logs/paintnet-20260911-210928-145714.log`.

The pixel-unit regression was also run before installing the final unit-mode
fix. That version failed two assertions because the cropped histogram was empty.
After the fix, both the DIP and pixel-coordinate crops produce the expected
bins. The final focused run totals 267 checks, zero failures, zero skips; the
lookup texture readback still matches all 120 rows.

The existing Direct2D suite was rerun after the new effects and local-bounds
support: 17,151 checks, 243 todos, the same two unexpected todo successes, and
one reference-device skip as stock Wine. The final unit-mode correction is
covered by the focused before/after test. Evidence:
`logs/d2d1-suite-analysis-final.log` and
`logs/histogram-pixel-units-before.log`.


## Alpha Mask and branched input graphs

Alpha Mask is registered under its own SDK CLSID with two inputs and no custom
properties. A compute shader multiplies premultiplied destination RGBA by mask
alpha. Intermediate pixels use RGBA32 float, retaining HDR values. The graph
walker evaluates nested Alpha Mask and Opacity Metadata inputs without C-stack
recursion and detects cycles through either input. Bounds queries walk the same
supported graph without allocating or rendering intermediate textures.

The new Alpha Mask case passes **488 checks**, including real texture readback
for fractional alpha, transparent pixels, mask RGB independence, HDR channels,
nested/shared graph branches, repeated rendering, crop/offset, differently sized
inputs, ignored alpha on either input, in-place mask updates, 96/192 DPI drawing,
pixel units, and unchanged source pixels. Error cases cover missing inputs,
cycles, nondrawable bitmaps, a target used as an input, and a separate bitmap
object sharing the target resource. Breaking a cycle restores successful draws.
A Histogram consuming Alpha Mask output returns the expected normalized bins.
Stock Wine fails at Alpha Mask metadata: three checks, one failure.

The initial Alpha Mask pixel-unit test exposed 26 wrong channels when its
192-DPI output was interpreted as DIPs. Effect intermediates now use the
context's effective DPI; pixel-unit drawing also honors the unit mode in the
geometry and axis-aligned-clip transforms. The first combined run exposed an
older Histogram test's incorrect assumption that an effect crop followed the
source bitmap DPI. Crop conversion now uses context DPI, and the regression
explicitly sets the context DPI and checks both 96/192-DPI crops. The older
Histogram DPI results above describe the earlier revision and are superseded
by this correction.

Final focused results: COM identity 26, EffectContext1 71, Histogram 130,
Opacity Metadata 47, Alpha Mask 488: **762 checks, zero failures, zero skips**.
The internal lookup diagnostic still verifies 120 texture rows with no failures.

These are implementation regressions on Wine/DXVK/llvmpipe, not native Windows
conformance results. In particular, effect bounds/DPI behavior and exact error
codes still need native comparison. The effect uses SM5 compute and currently
requires feature level 11.0; a lower-feature-level pixel-shader path is not
implemented. Intermediate-precision selection and render-cache optimizations
remain incomplete. General custom transform graphs and other builtin effect
renderers are not supplied by this change.

References: Microsoft's [Alpha Mask documentation](https://learn.microsoft.com/en-us/windows/win32/direct2d/alpha-mask-effect),
[unit modes](https://learn.microsoft.com/en-us/windows/win32/api/d2d1_1/ne-d2d1_1-d2d1_unit_mode),
and [Win2D DPI compensation behavior](https://microsoft.github.io/Win2D/WinUI3/html/T_Microsoft_Graphics_Canvas_Effects_DpiCompensationEffect.htm).
The last source describes Win2D inserting explicit compensation effects; the
Wine implementation follows pixel-based transforms and context-based output
coordinates rather than automatically resampling Alpha Mask's input bitmaps.

Evidence outside the repository: `logs/alpha-mask-stock.log`,
`logs/alpha-mask-first.log`, `logs/alpha-mask-focused.log`,
`logs/alpha-mask-final.log`, `logs/alpha-mask-upload.log`, and
`logs/build-alpha-mask-final.log`.


The full upstream test case was rerun after the final Alpha Mask, context-DPI,
and pixel-unit changes: **17,151 checks, 243 todos, the same two unexpected
todo successes and one unavailable reference-device skip**, matching the
recorded stock baseline. No new failing assertion appeared. Evidence:
`logs/d2d1-suite-alpha-mask.log`.

The application launch verified all 312 original binaries. The trace passes
Histogram, Opacity Metadata, and Alpha Mask, then reports missing Convolve
Matrix (`407f8c08-5533-4331-a341-23cc3877843e`). The matching crash file identifies
`EffectCategories` initialization and again reports the missing
`dcomp!CreatePresentationFactory` export during diagnostic collection. The
failed test application was closed only in the isolated development prefix.
Evidence: `logs/paintnet-20260911-213601-166029.log` and
`app/Paint.NET App Files/CrashLogs/pdncrash.7.log` in the work directory.


## Convolve Matrix and intermediate coordinates

The new regression passes **1,496 checks**, including rendered-pixel comparisons
for the default identity, one- and two-dimensional averaging, an asymmetric
edge kernel, soft padding, mirrored borders, fractional offsets, divisor/bias,
HDR output, alpha preservation, clamping before premultiplication, zero divisor,
caller-owned kernel storage, source-content changes, nested convolution, and
convolution feeding Alpha Mask or Histogram. Crop/offset tests include negative
image origins. DPI tests use a half-DIP kernel unit at 192 DPI and pixel units
with the context still set to 192 DPI. Stock Wine stops at missing registration:
two checks, one failure.

The image evaluator now carries each intermediate's integer pixel rectangle.
Bounds-only evaluation computes those rectangles without rendering. Alpha Mask
intersects and aligns its two input rectangles; Histogram translates its crop
into the intermediate's texture coordinates. The common compute dispatcher
retains the previous device-context state handling and resource-domain checks.

An initial full-suite run found four surface mismatches because the revised
DrawImage path treated inverted rectangles as empty. The existing native-derived
suite expects them to be ignored. After preserving that behavior, the final
suite again matches stock: 17,151 checks, 243 todos, two identical unexpected
todo successes, and one reference-device skip. The focused case also checks
inverted rectangles on convolution output.

Final focused results: identity 26, context/resources 71, Histogram 130,
Opacity Metadata 47, Alpha Mask 488, Convolve Matrix 1,496: **2,258 checks,
zero failures and zero skips**. The lookup upload diagnostic compares 120 rows
with no failures. Build output has no compiler warnings or errors.

Limitations: native Windows results have not been recorded. Exact convolution
bounds, kernel-offset conventions, numerical edge cases, and property validation
still need that comparison. The implemented path requires kernel-unit spacing
of exactly one input pixel after DPI conversion. Other spacings return
E_NOTIMPL; pre/post-resampling for the ScaleMode choices is not implemented.
The compute path requires feature level 11.0. Precision selection, caches, and
unrelated custom transform renderers remain incomplete.

References: Microsoft's [Convolve Matrix description](https://learn.microsoft.com/en-us/windows/win32/direct2d/convolve-matrix),
[SDK property declarations](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/d2d1effects.h),
and [Win2D property setup and validation](https://github.com/microsoft/Win2D/blob/winappsdk/main/winrt/lib/effects/generated/ConvolveMatrixEffect.cpp).
The SDK and Win2D both declare KernelUnitLength as VECTOR2, despite the overview
page's FLOAT description; the implementation follows those declarations.

Application retest: all 312 binaries passed integrity checks. The trace passes
Convolve Matrix and next fails on Contrast
(`b648a78a-0ed5-4f80-a94a-8e825aca6b77`). The crash remains in EffectCategories
initialization, and diagnostic collection still reports the missing presentation
factory export. The failed application was stopped only in the isolated prefix.

Evidence outside the repository: `logs/convolve-stock.log`,
`logs/convolve-first.log`, `logs/convolve-final.log`, `logs/convolve-upload.log`,
`logs/d2d1-suite-convolve.log`, `logs/d2d1-suite-convolve-final.log`,
`logs/build-convolve-final.log`, `logs/paintnet-20260911-215307-179341.log`,
and `app/Paint.NET App Files/CrashLogs/pdncrash.8.log`.


## Contrast

Contrast's focused test passes **332 checks** for positive/negative/zero amount,
input clamping, HDR values, premultiplied transparency, ignored alpha, source
preservation, nested convolution with a negative image origin, context DPI,
and pixel units. The eight amount/clamp cases compare actual floating-point
texture readback against a small transfer-value table. Stock Wine stops at the
missing effect metadata: two checks, one failure.

The transfer uses two quadratic pieces with matching midpoint slope. The graph
in Microsoft's documentation was used to infer the maximum-positive coefficients;
linear interpolation from identity across the amount parameter and extrapolation
outside [0,1] are implementation assumptions. These pixel tests validate that
implementation, not native Windows equivalence. Native comparison is required
before claiming exact Contrast conformance. Feature-level 10 support, intermediate
precision selection, and caching are not implemented.

Reference: Microsoft's [Contrast description and graph](https://learn.microsoft.com/en-us/windows/win32/direct2d/contrast-effect)
and [Win2D property defaults and validation](https://github.com/microsoft/Win2D/blob/winappsdk/main/winrt/lib/effects/generated/ContrastEffect.cpp).

All seven focused cases pass **2,590 checks, zero failures, zero skips**. The
full suite reports 17,151 checks, 243 todos, two previously recorded unexpected
todo successes, and one reference-device skip, matching stock. The new module
build has no compiler warnings or errors. The application launch verifies 312
unchanged binaries and advances from Contrast to missing Bitmap Source
(`5fb6c24d-c6dd-4231-9404-50f4d5c3252d`); its crash is still in EffectCategories.
The missing presentation-factory export remains in diagnostic collection.

Evidence outside the repository: `logs/build-contrast-tests.log`,
`logs/contrast-stock.log`, `logs/contrast-first.log`, `logs/contrast-final.log`,
`logs/d2d1-suite-contrast.log`, `logs/paintnet-20260911-220706-187549.log`, and
`app/Paint.NET App Files/CrashLogs/pdncrash.9.log`.


## Bitmap Source and empty input-list XML

The new Bitmap Source case passes **7,337 checks**, including readback of complete
8x8 float targets for the source's own extent and the untouched pixels outside it.
Cases cover premultiplied HDR input; straight BGRA converted to premultiplied
output; 24-bit WIC conversion; 16-bit RGBA precision; all eight orientations;
nearest enlargement; linear enlargement/reduction; crop/offset; a source feeding
Contrast; 192-DPI correction and pixel units; zero-width output; missing/invalid
sources; property defaults/validation; and COM ownership. A counting WIC source
checks that bounds queries do not copy pixels and repeated draws use the cache.
Reassigning the source or changing properties invalidates that cache.

The initial new-effect registration failed because parse_effect_inputs tested
IsEmptyElement while still positioned on an attribute. It now returns to the
Inputs element first. Five focused XML cases include self-closing input lists
with neither, either, or both bounds attributes, and an explicitly closed input
list, each followed by another property. Stock Wine fails the three self-closing
attribute cases and then stops at missing Bitmap Source: 12 checks, four failures.

All eight focused cases pass **9,927 checks, zero failures, zero skips**. The
full suite again reports 17,151 checks, 243 todos, the same two unexpected todo
successes, and one unavailable reference-device skip. The final build contains
no compiler warnings or errors. The first build's missing test-header include
was corrected before runtime validation.

Limitations: only nearest/linear scaling and premultiplied output are rendered.
Cubic, Fant, mipmap interpolation, and straight-alpha output return E_NOTIMPL.
Unsupported WIC formats report an error instead of silently losing precision.
The implementation currently decodes the full image and stores an RGBA32-float
GPU bitmap, so memory use can be high for large inputs. Native Windows numeric
and behavioral comparisons remain necessary, especially for fractional output
bounds, DPI/pixel-unit interaction, combined rotate/flip order, and cache
invalidation semantics. These are functional implementation regressions, not
native conformance results or application-level performance benchmarks.

References: Microsoft's [Bitmap Source description](https://learn.microsoft.com/en-us/windows/win32/direct2d/bitmap-source)
and [SDK property/enumeration declarations](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/d2d1effects.h).

Application retest verified all 312 original binaries and advanced past Bitmap
Source to missing Emboss (`b1c5eb2b-0348-43f0-8107-4957cacba2ae`). EffectCategories
still fails, and diagnostic generation still lacks CreatePresentationFactory.
No successful editor startup or editing/file round trip has been observed.

Evidence outside the repository: `logs/build-bitmap-source-final.log`,
`logs/bitmap-source-registration.log`, `logs/bitmap-source-stock-final.log`,
`logs/bitmap-source-final.log`, `logs/d2d1-suite-bitmap-source.log`,
`logs/paintnet-20260911-222112-196386.log`, and
`app/Paint.NET App Files/CrashLogs/pdncrash.10.log`.

## Independent Windows baseline

A Windows 11 evaluation VM now runs the same public-API tests against its
system Direct2D and Microsoft WARP. The corrected native batch records 9,927
checks, 283 expectation mismatches, and no skips. COM identity passes all 28
checks. The mismatch breakdown and separate Emboss/property measurements are
in REFERENCE.md. These findings supersede any interpretation of the earlier
Wine-only pixel tests as native conformance.

Restoring valid state after numeric validation and adding required custom
property DisplayName metadata prevents cascading native test failures. The
updated tests retain 9,931 passing checks on Wine with no skips. The reference
tools build with -Wall -Wextra -Werror. No implementation changed in this step.

## Native color, alpha, and convolution corrections

Windows/WARP now passes Contrast 356, Alpha Mask 488, Opacity Metadata 47,
and Convolve Matrix 2,286 checks, including the new nine one-hot kernel cases.
The same rendering cases pass on Wine. All eight Wine cases total 10,743 checks,
with zero failures/skips. Logs: native-color-convolution-wine.log and the native
009-native-color-convolution result batch in the separate work directory.

The full existing Direct2D suite executes 17,151 checks, 243 todos, the same two
unexpected todo successes at d2d1.c:15662, and one reference-device skip as the
stock baseline. The texture-upload diagnostic compares 120 rows with no errors.
No new upstream-suite failure was observed. Logs:
d2d1-suite-native-color-convolution.log and upload-native-color-convolution.log.

The initial build needed the math header for the infinity test; the final build
completed without warnings. No application binary was modified.

## Emboss

Stock Wine reaches five checks and fails effect creation because Emboss is not
registered. The implementation passes 4,919 checks on Wine and Windows/WARP.
The focused fixtures cover flat/impulse/colored/transparent/HDR/mixed inputs,
height and direction changes, 96/192 DPI, pixel units, boundaries, bounds, and
cycle recovery. Logs: emboss-stock.log, emboss-final.log; native batch
012-emboss-fixtures.

Full native comparison: 2,970 cases, 582,120 channels, zero mismatches at 0.00002
tolerance, maximum error 0.00000164. Independent 8x8 basis: 256 cases, 102,400
channels, zero mismatches, maximum error 0.000000119. Comparison logs:
emboss-native-comparison.log and emboss-grid8-native-comparison.log.

All nine focused cases pass 15,662 checks with zero skips. The existing suite
remains at 17,151 checks, 243 todos, the same two unexpected todo successes at
d2d1.c:15662, and one skip. No new failure was introduced. Final build succeeds;
intermediate dimensions are checked before conversion/allocation. Logs:
emboss-all-focused.log, d2d1-suite-emboss.log, build-emboss-final.log.

The application run verifies all 312 binaries and advances to missing Opacity
metadata. Log paintnet-20260911-233216-227449.log; crash pdncrash.12.log. It still
fails in EffectCategories, and CreatePresentationFactory remains absent during
diagnostic collection. This is not successful startup or application QA.

## Opacity

Stock Wine fails the metadata lookup with 0x80070490 after two checks. The
implemented effect passes 470 checks on both Wine and Windows/WARP, with no
skips. The initial Wine run caught an uninitialized default amount; setting
the factory default to 1 corrected the failure. All ten focused Wine cases
pass 16,132 checks. Logs: opacity-stock.log, opacity-first.log,
opacity-all-focused.log; native batch 013-opacity.

The broader drawing suite retains 17,151 checks, 243 todos, the same two
unexpected todo successes, and one skip. Log: d2d1-suite-opacity.log.

The application passes all category lookups and reaches MainForm construction.
It then fails on CreateSmoothStopTransition while creating UserColorsControl.
Log: paintnet-20260911-234242-236228.log; crash: pdncrash.13.log. The missing
presentation-factory export still affects diagnostic collection.

## UIAnimation

Stock Wine fails all eleven timeline scenarios at transition creation (56
checks, 11 failures). The implemented timeline case passes 2,114 checks on
Wine and Windows; the existing animation suite passes 14 checks with one
remaining todo for the unimplemented custom-transition factory. No skips.
Logs: animation-timeline-stock.log, animation-suite-final.log; native batch
016-animation-timeline.

All 757 native reference records match, including event ordering and timer
invariants. Logs: animation-reference-final.log, animation-native-comparison.log.
The tests caught a float-boundary completion delay, callback ordering, and
zero-duration notification differences. Build completed after correcting a
missing stdarg include and duplicate test GUID definitions.

The application run verifies all 312 original binaries and passes the animation
constructor failure. Next failure: shader reflection in the checkerboard effect.
Log: paintnet-20260912-000139-244700.log; crash: pdncrash.15.log. No editor or
image round trip is verified.

## Shader reflection requirements

Stock Wine fails GetMinFeatureLevel for all eighteen Windows-compiled reference
shaders. The implementation passes 72 fixture checks on Wine and Windows, with
no skips. The existing reflection suite passes 1,348 checks on both stock and
modified Wine. Logs: shader-reflection-stock.log, shader-requirements-first.log,
reflection-suite-stock.log, reflection-suite-fork.log; native batch
018-shader-requirements.

The complete thirteen-case project set executes 18,332 checks with zero failures
and skips; one expected todo remains in the old custom-animation-factory test.
Log: shader-requirements-all-focused.log. The changed reflection implementation
is linked into wined3d.dll, so that private module is rebuilt and installed.
DXVK continues to provide Direct3D rendering.

The application verifies all 312 original binaries and advances to
SetOutputBuffer during the checkerboard draw transform's initialization.
Log: paintnet-20260912-001321-252991.log; crash: pdncrash.17.log. The earlier
reflection diagnostic is preserved compressed as
paintnet-20260912-000503-246950.log.gz. No application binary was changed.

## Custom source draw transforms

The new draw_transform case passes 75,824 checks on both Wine and native Windows
with no todos, failures, or skips. It exercises 288 rendered configurations,
including a nested Opacity effect and caller-owned constant data overwritten
after upload. Native batch: 026-draw-tests-final. Wine log:
draw-transform-tests-final.log. An earlier native assertion required exactly
two bounds mappings; Windows may remap a cached node, so the regression now
checks that bounds were mapped for both requests without requiring an internal
call count. The pixel expectations did not change for that correction.

Standalone direct, cached, and nested runs each compare 320 records
and 18,432 color channels without pixel or API-result mismatch. Cached Windows
runs map bounds three times versus Wine's two; the comparison reports these
72 redundant-call differences separately. Maximum absolute pixel errors are
below 1e-7. Reference batches: 020-draw-transform, 021-draw-cached,
025-draw-nested-clean. Wine logs: draw-transform-final.log,
draw-transform-cached-final.log, draw-transform-nested-final.log. The comparison
script checks API results, bounds, callbacks, and every pixel. Batch 022 also
measures the full scene-position vector (z=0, w=1). Batch 024 inherited a probe
mode in the reference harness and is superseded by explicitly cleared modes in
025; it is not used as conformance evidence.

Stock Wine stops at the constant-buffer stub and cannot render the probe.
Log: draw-transform-stock.log. The unchanged thirteen earlier focused cases
pass 18,332 checks with one expected animation todo and no failures/skips.
Log: custom-all-focused.log. The full Direct2D suite executes 17,157 checks,
243 todos and one skip, with only the two existing unexpected todo successes
at d2d1.c:15662. Log: d2d1-suite-custom-final.log. Unsupported input-bearing
custom transforms retain the existing fallback; they are not claimed to render.

All 312 original application binaries were verified before the next startup
attempt. It passes checkerboard shader setup and fails in CommandList.Close
while recording the color-button icon. Log:
paintnet-20260912-003142-258762.log; crash: pdncrash.18.log. The shader path is
validated independently; this application run has not yet displayed the editor.

## Command-list recording and lifecycle

The command_list_state test passes 8,028 checks on both Wine and Windows,
without todos, failures, or skips. Native batch: 033-command-tests-final;
Wine log: command-list-tests-final.log. The stock runtime executes 7,794 checks
with 500 failures against the same regression; log:
command-list-tests-stock-final.log. This includes expected cascades in the
ordered record comparison, not 500 distinct bugs.

Standalone measurements match 376 records, including 27 replayed images and
6,912 RGBA channels, exactly. Seven ownership/domain scenarios add 88 matching
records. Native batches: 029-command-list-pixels and
032-command-context-expanded. Wine logs: command-list-pixels-final.log and
command-context-final.log. compare-command-list.py compares these results.
The sink uses public APIs to replay state and drawing to a bitmap; this is not
a claim that Wine's automatic DrawImage(commandList) path works.

All fifteen focused project cases pass 102,184 checks with no failures or skips
and one existing custom-animation-factory todo. Log: command-all-focused.log.
The broad Direct2D suite executes 17,157 checks with 237 todos and one skip;
its only two failures remain the known unexpected todo successes (now at
line 15659 after removing three newly passing todo markers). Log:
d2d1-suite-command-display.log. Two earlier attempts could not create windows
because the private Xvfb display had stopped; those were environment failures.
The display was restored before the successful baseline comparison.

The official application still verifies all 312 binaries unchanged. It now
passes command-list closing and reaches CreateColorContext(SRGB,NULL,0), which
returns E_NOTIMPL. Latest log: paintnet-20260912-010831-279604.log;
crash: pdncrash.20.log. The first run showing that advance was
paintnet-20260912-005413-271576.log / pdncrash.19.log. No working editor or file
round trip is claimed.

## Color-profile resources

The color_context regression passes 4,435 checks on Windows (batch
040-color-tests-expanded) and 3,511 on Wine (color-context-tests-final.log),
with no failures, todos, or skips. Counts differ because complete buffers are
checked byte by byte and the generated ICC profiles have different sizes.
Stock Wine reaches 87 checks and fails 71: color-context-tests-stock-final.log.
Coverage includes ICC copying and zeroed buffer tails, COM identity/factory
ownership, bitmap retention, DXGI enum validation, simple profiles, WIC EXIF,
invalid profiles, memory-versus-WIC/file classification, and trailing file data.

Reference batches 035 through 038 progressively measure these interfaces.
Batch 034 terminates on a NULL custom ICC input on Windows; that invalid call
was excluded from later probes. Batch 039 exposed native unsupported-QI output
preservation; the final implementation and batch 040 include that correction.
The original application's standard calls do not use those invalid inputs.

compare-color-profiles.py reads three exported profiles from each standalone
ICC probe, then uses the same host Little CMS engine for an independent
RGB-to-XYZ comparison. Reference: 038-color-context-classification; Wine:
color-context-icc-final.log. It checks 729 unit-range and 343 extended-range
colors per profile, including negative values and values above one. All 9,648
XYZ channels pass. Maximum absolute errors are 0.000198365 for sRGB,
0.000639797 for scRGB, and 0.000070096 for Adobe RGB. Tolerances are 0.0005 in
the unit range and 0.001 for extended values. Windows and Wine profile bytes,
sizes, dates, and metadata are not identical. This does not test Direct2D's
Color Management image effect or GPU rendering.

The earlier fifteen focused cases still pass 102,184 checks with one existing
animation todo: color-all-focused.log. Together with color_context, the sixteen
cases pass 105,695 Wine checks without failures or skips. The broader Direct2D
suite keeps its prior result: 17,157 checks, 237 todos, one skip, and only the two
known unexpected todo successes at d2d1.c:15659. Log:
d2d1-suite-color-final.log. An earlier wrapper run was superseded after the
script was edited while Bash was reading it; the final run uses a stable script.

The unchanged app verifies all 312 runtime files, passes color-profile creation,
and stops at the missing Color Management effect registration. Log:
paintnet-20260912-012518-285482.log; crash: pdncrash.21.log. The later traceback
also still encounters the missing dcomp presentation-factory export. The editor
has not opened; no editing, save/reopen, native chooser, or hardware optimization
milestone is claimed.

## Effect nodes

The effect_node regression passes 222 checks on Windows and Wine, with no
failures, todos, or skips. Windows batch: 044-node-alpha; Wine log:
effect-node-tests.log. Reference probes 042-effect-node and 043-effect-node-chain
measure node identity, retained references, unsupported-QI output preservation,
dynamic input counts, another context on the same device, unchanged public
inputs, live property changes, nested wrappers, chained effects, missing inputs,
cycles, and graph passthrough. All 54 records in the final standalone probe
match exactly, including seven images and 112 RGBA channels:
effect-node-first.log. Existing sixteen focused cases remain at 105,695 passing
checks with one animation todo: effect-node-focused.log. Together the seventeen
cases have 105,917 checks.

A separate local Color Management prototype is present during the latest app
run. Its 108 standard-color cases preserve earlier pixels after an alpha-metadata
fix, but custom ICC color differences remain unresolved. No full Color Management
support is claimed. App log paintnet-20260912-021705-307058.log and pdncrash.24.log
verify 312 unchanged binaries and show the next missing effect is UnPremultiply.
The editor still has not opened.

The broad Direct2D suite preserves its prior result: 17,157 checks, 237 todos,
one skip, and only the two known unexpected todo successes at d2d1.c:15659.
Log: d2d1-suite-effect-node.log. No new broad-suite failures were observed.

## Alpha conversion

alpha_conversion passes 1,198 checks with no failures, todos, or skips on both
Windows (046-alpha-tests) and Wine (alpha-conversion-tests.log). It tests both
default Direct3D feature level and explicitly requested level 10. The standalone
probe's 512 RGBA values match Windows exactly at both levels:
044-node-alpha--alpha-conversion.log, 045-alpha-fl10--alpha-conversion.log,
alpha-conversion-first-render.log, and alpha-conversion-fl10.log. Coverage
includes zero/negative/tiny/greater-than-one alpha, extended RGB, premultiply,
unpremultiply, both chain orders, and premultiplied/ignore source metadata.

The preceding seventeen focused cases pass 105,917 checks with one existing
animation todo: alpha-conversion-focused.log. Including the new case gives
107,115 checks. The first prototype registration failed because its XML omitted
the declaration required by Wine's parser; it was corrected before pixel tests.

The unchanged app run paintnet-20260912-022725-314447.log verifies all 312 runtime
binaries and now reaches DrawingContext.EndDraw in ColorsForm.SetColorAddIcon.
Crash pdncrash.25.log reports E_NOTIMPL. The trace follows the Color Management
wrapper's passthrough graph to an ID2D1CommandList input, whose rasterization is
not implemented. The Color Management prototype remains uncommitted and has
known custom-ICC differences. No editor or application round trip is claimed.

The broad Direct2D suite remains at 17,157 checks, 237 todos, one skip, and the
two known unexpected todo successes at d2d1.c:15659; no new failures. Log:
d2d1-suite-alpha-conversion.log.

## Dispatcher queue execution and shutdown

The original coremessaging suite passes 226 checks against native Windows
(051-dispatcher-queue--coremessaging_test.log). The implementation initially
exposed a test race: the ShutdownCompleted handler may signal an event while
the returned async action is still Started. The test now waits for that action
before checking completion and closing it. This revised suite passes all 226
checks on Windows and Wine, without todos or skips:
053-dispatcher-deferral--queue-tests.log and queue-priority-tests.log.

The standalone queue probe compares activation, per-thread identity, duplicate
creation, valid/invalid priorities, callback order, retained references, shutdown
status, and queue removal. All 42 normal records match exactly:
052-dispatcher-normal--queue-normal.log and queue-native-comparison.log.
The expanded deferral probe exposed ShutdownStarting's high-priority ordering;
after correction all 52 records match, including deferred shutdown and work
submitted from another thread: 053-dispatcher-deferral--queue-deferral.log and
queue-deferral-priority.log. The comparison rejects incomplete logs and requires
completed callbacks and a completed async action. These probes do not cover
queue timers or ASTA-specific apartment behavior.

The actual application run paintnet-20260912-030821-332210.log verifies all 312
original binaries, creates the main thread's queue, calls GetForCurrentThread,
and enqueues a low-priority callback. It reaches MainForm.OnShown and initial
blank-document creation. Crash pdncrash.27.log reports E_NOTIMPL from
UIAnimationStoryboard.AddKeyFrameAfterTransition in the busy-spinner animation.
This run includes uncommitted command-list and Color Management prototypes.
No successful editing, file round trip, or hardware performance is claimed.

The complete nineteen-case focused suite passes 107,341 checks, zero failures,
and one existing animation todo (queue-focused.log). The module installation
script refreshes the new WinRT class in the isolated prefix; shell syntax and
reference-log comparisons pass.

## Animation keyframes and repeating timelines

Native logs 054-animation-keyframes--keyframes.log and
055-animation-keyframe-errors--keyframes.log measure nine timelines, 225 frames,
and keyframe validation. The final Wine prototype matches all 358 labeled
records exactly (animation-keyframes-validation.log), including variable values,
final values, elapsed time, effective durations, status, change flags, Conclude,
and post-scheduling sealing. Zero iterations skip the loop; count one plays it
once; count two wraps once. Numeric keyframe zero is valid after allocation.

The keyframes regression converts these measurements to 2,182 assertions. The
Wine run passes without failures, todos, or skips (keyframes-tests-first.log).
The existing timeline suite still passes 2,114 checks, and the older 14-check
UIAnimation case retains its one pre-existing todo. Multiple simultaneous loops
and general overlapping transitions remain unverified/unsupported.

The application run paintnet-20260912-032523-338752.log again verifies all 312
original binaries and passes busy-spinner setup. Crash pdncrash.28.log reports
an unsupported gradient-stop interface while painting ZoomSliderControl.
The trace identifies IID_ID2D1GradientStopCollection1
(ae1572f4-5dd0-4777-998b-9279472ae63b) on a legacy-created collection.
Command rendering and Color Management prototypes are still in the working
runtime; no editing, saved-image correctness, or GPU performance is claimed.

The identical keyframes test passes all 2,182 assertions on native Windows
(056-keyframes-tests--keyframes-tests.log). A combined run exposed a dispatcher
reference-count race: retaining the queue itself during callback execution
made its observable count one higher. Dispatch now retains the controller,
which owns the queue, and the regression waits until the callback is running
before checking its count. That strengthened test passes 227 checks on Windows
and Wine (057-queue-dispatch-lifetime--queue-tests.log). The final twenty-case
suite passes 109,524 checks, zero failures, with one existing animation todo
(keyframes-queue-focused.log).

## WhiteLevelAdjustment — September 12, 2026

- Native job 061 and Wine `white-level-metadata.log`: all 170 API and pixel
  records match exactly, including 20 rendered cases / 1,280 RGBA components.
- Native job 063 and Wine `white-gradient-focused.log`: `white_level` executes
  3,118 checks with zero failures at default and forced feature level 10.
- Existing 20-case focused suite: 109,524 checks, zero failures, one existing
  UIAnimation todo (`white-gradient-full-focused.log`).
- The unchanged app passes the WhiteLevelAdjustment initialization and reaches
  a one-input custom shader. Run `paintnet-20260912-040320-355817.log` and crash
  report 30 identify `ID2D1DrawInfo::SetInputDescription` as the next failure.
- White-level default subproperties are populated explicitly because the
  existing effect XML parser still ignores nested property metadata. The
  missing-property getter now clears the caller buffer, as the native probe
  demonstrates for absent minimum/maximum metadata.
- Reference API: https://learn.microsoft.com/en-us/windows/win32/api/d2d1effects_2/ne-d2d1effects_2-d2d1_whiteleveladjustment_prop

## Versioned legacy gradient collections and ramps — September 12, 2026

- Native job 058 / Wine `gradient-stops-clamp.log`: all 108 metadata/API
  records match. Gamma 2.2 pixels in all six color/extend combinations match
  within 1.2e-7. Gamma 1.0 has residual quantization differences up to one
  8-bit step; the focused test retains the Windows values and permits that
  explicitly documented bound for gamma 1.0 only.
- Native job 062 / Wine `gradient-stops-tests.log`: 1,303 checks, zero failures.
  Tests cover COM identity, stored gamma/extend, versioned color spaces,
  converted stop values, untouched trailing elements, and float target pixels.
- Native jobs 059/060 vary the gradient length, world scale, and DPI. Legacy
  ramps use power-of-two tables with two border texels in clamp mode; scale
  and DPI affect the selected resolution. The renderer caches levels 4–1024
  in an 8,176-byte GPU buffer and reads two entries per pixel.
- Native job 064 / Wine `gradient-stops-radial.log`: the tested radial wrap
  and mirror cases match within 6.1e-8 at gamma 2.2; radial clamp still differs
  by up to 0.005576 (and up to 0.009437 at gamma 1.0). Radial ramp selection
  and gamma quantization remain open, as do broader transformed/radial cases.
- Broad Direct2D test: 17,157 checks, 237 todos, one skip, and the same two
  previously recorded unexpected todo successes at d2d1.c:15659. No new
  ordinary failures (`d2d1-suite-white-gradient-verified.log`). A first wrapper
  run was invalidated by editing the script while Bash was reading it; the
  reported repeat invokes the test executable directly after all script edits.
- App run `paintnet-20260912-034647-347428.log` passes the zoom-slider interface
  request and reaches the formerly missing WhiteLevelAdjustment effect.

## Transform-graph fanout — September 12

Native probes 066 and 067 connect one graph input to Premultiply and
UnPremultiply and one node output to two downstream nodes. Wine previously
lost the first edge; six of twelve rendering cases failed. The fix matches
Windows bounds, HRESULTs, and all RGBA values in eleven cases. The remaining
case removes a connected upstream node after realization: Windows retains
the realized connection, whereas Wine reports INVALID_GRAPH_CONFIGURATION.
Re-invalidating the effect input on Windows preserves this observation.

The focused graph_fanout regression passes 298 checks on native Windows/WARP
and 279 on Wine, with two explicit todo failures and no ordinary failures.
The existing effect_node (222 checks) and draw_transform (75,824 checks)
regressions still pass. Paint.NET now gets through DocumentStrip bounds
validation; pdncrash.32.log instead records a DrawImage command replay failure
at EndDraw. All 312 original Paint.NET runtime binaries remain unchanged.

## Custom image-input shader validation — September 12

The native draw-input reference (job 065) and Wine agree on all 95 API records,
72 bounds/draw cases, and 18,432 RGBA values to within 1.5e-8. Coverage spans
six output precisions, three channel depths, full and cropped images, offset
sampling, 192 DPI, and point versus linear filters. Matching native sampling
required clamped texture addressing rather than transparent border addressing.

The independent focused draw_input test passes 18,971 checks on both native
Windows/WARP (job 068) and Wine with no failures or skipped cases. The native
expected pixel arrays are retained in the test; tolerance is 1e-6. Source-only
draw_transform (75,824 checks) and wrapped effect_node (222 checks) also pass.
This does not validate arbitrary multi-input shaders, mipmap realization,
expanded-region propagation, or completed application rendering.

## Contained geometry combination validation — September 12

Native reference 072 covers rectangles, a concave polygon, and an ellipse with
and without an input transform, across all four combination modes. All 24
cases match HRESULTs, bounds, and 13,824 aliased RGBA samples exactly. Twenty-one
areas match; the untransformed ellipse intersection differs by 0.7012768 in
area, with the same difference in exclusion and XOR. These inherited flattening
differences are explicit todos, not relaxed tolerances.

The focused contained_combine regression executes 14,126 checks on both Wine
and native Windows/WARP (job 073). Wine has three todo failures, no ordinary
failures, and no skipped cases; native has no failures. The actual Paint.NET
retest, pdncrash.34.log, passes the ColorRectangleControl subtraction and fails
on Widen(strokeWidth=2, strokeStyle=NULL). All original runtime binaries pass
the integrity check.

The complete focused suite then passed 25 test cases and 147,321 checks, with six recorded todo failures and no ordinary failures (contained-full-focused.log).

## Default-stroke widening — September 12, 2026

Native Windows/WARP job 076 runs the focused `widen` regression: 340 checks,
zero failures. The Wine/DXVK llvmpipe run executes 322 checks with five expected
TODO failures and zero ordinary failures. Raw probes 074 and 075 measure
rectangles, concave contours, ellipses, and the actual seven-point Paint.NET
color-control contour at four widths and two transforms. All eight rectangle
cases match native bounds, area, and pixels. The eight actual contour cases
match bounds, with at most one ULP of area accumulation difference; three
cases differ at 1–3 aliased edge pixels. Ellipse Widen remains unimplemented.

The unchanged application advances past Widen and, with the local path-combination
prototype, past its geometry union. Run `paintnet-20260912-051834-391715.log`
fails at CompositorController activation; `pdncrash.38.log` records
CLASS_E_CLASSNOTAVAILABLE. This is not successful editor startup.

## Path combination and geometry integration — September 12, 2026

Native jobs 077/078 measure 48 combinations at 12×12 RGBA32_FLOAT, covering
three input shapes, four boolean modes, identity/sheared translated inputs,
and a primary path with and without a hole. All 27,648 sampled Wine RGBA
values equal Windows/WARP. All rectangle/polygon bounds and areas match;
ellipse bounds and area differences remain documented by the regression.

Focused `path_combine`: 28,260 checks on both Wine and native job 079. Windows
has zero failures; Wine has 20 expected TODO failures and zero ordinary failures.
The complete focused suite after both geometry changes runs 27 cases and
175,903 checks, with 31 TODO failures and zero ordinary failures
(`geometry-full-focused.log`). The unchanged Paint.NET startup advances to
CompositorController activation. Editor interactions and final presentation
remain unverified.

## Composition controller lifecycle — September 12, 2026

Native probe 081 establishes dispatcher-required activation (E_ACCESSDENIED
without a queue), stable compositor identity, event retention/coalescing, empty
commit, deferred completion, and RO_E_CLOSED after Close. The Wine probe matches
all exercised HRESULTs, names, trust levels, event calls, and handler references.
Its GetIids lists are shorter because most composition interfaces remain absent.
Probe 080 had a truncated class-name string and is not valid activation evidence.

The focused `composition` regression passes 103 checks on native Windows job
082 and 66 checks on Wine, both with zero failures. The count differs because
the test queries every interface advertised by GetIids. It also verifies color
round trips and releases. No visual presentation is claimed by these tests.

Paint.NET run `paintnet-20260912-053507-400701.log` passes controller setup and
then fails during ScrollableCanvasControl creation because user32 does not
export SetWindowFeedbackSetting (`pdncrash.39.log`).

## Window feedback configuration — September 12, 2026

Native probes 083–086 measure feedback settings, inheritance, resets, normalized
nonzero BOOLs, accepted types, invalid arguments, output buffer behavior,
GetLastError preservation, destroyed HWNDs, and FEEDBACK_MAX. The standalone
stock-Wine baseline reports missing exports. The focused `feedback` regression
passes 936 checks on Wine and native Windows job 088 with zero failures.

After replacing the isolated user32 module, the full focused suite runs 29
cases and 176,905 checks with 31 TODO failures and zero ordinary failures
(`feedback-full-focused.log`). Paint.NET advances past feedback configuration
and fails at GetMaximumSupportedFeatureLevel during canvas initialization
(`paintnet-20260912-054455-414605.log`, `pdncrash.40.log`). Visual presentation
and editing remain unverified.

## Effect feature levels — September 12

Native probes 089 and 091 compare 47 requested-level arrays on devices created
at 11.0 and 10.0. All 94 Direct2D result records match the implementation. Probe
090 accidentally reused the previous executable after a compile failure and is
not used for context-state comparisons. Native focused job 093 passes 784 checks,
including single-threaded devices. Wine passes the same test with four TODOs for
DXVK's existing device-level promotion during the prerequisite capability query.

The complete focused suite passes 30 cases / 177,689 checks, with 35 TODOs and
zero ordinary failures (`feature-level-full-focused.log`). All 312 original
Paint.NET binaries pass verification. Startup passes the device feature queries
and reaches transformed ellipse widening during brush activation
(`paintnet-20260912-060257-424706.log`, `pdncrash.41.log`). No editor session is
yet usable.

The implementation uses the public context-state query without changing active
D3D context state. DXVK's reported-device-level promotion is visible in its
[CreateDeviceContextState implementation](https://github.com/doitsujin/dxvk/blob/v3.1/src/d3d11/d3d11_device.cpp#L1316).

## Ellipse stroke outlines — September 12

Native jobs 094–095 measure transformed ellipses at four stroke widths and two
world transforms, with default and explicit solid round styles. Job 097 adds
reflections. The circle samples, including strokes wider than the diameter,
match all rendered pixels. Nonuniform shapes differ by 1–9 sampled edge pixels
in affected cases, and some transformed bounds/areas differ due to flattening.
The focused regression preserves the separate reflected native fixtures.

Native focused job 098 passes 1,374 checks. Wine passes 1,374 checks with 51
explicit TODOs for the measured nonuniform-ellipse differences and zero ordinary
failures. The complete suite runs 31 cases / 179,063 checks, with 86 TODOs and
zero ordinary failures (`ellipse-widen-full-focused.log`).

The unchanged application passes brush activation and reaches canvas rendering.
Its next E_NOTIMPL is graph initialization for ID2D1OffsetTransform
(`paintnet-20260912-062129-434011.log`, `pdncrash.43.log`). The earlier ellipse
run (`paintnet-20260912-061740-431682.log`, `pdncrash.42.log`) exposed the same
canvas drawing failure. The canvas and an editing session are still unverified.

## Offset transform execution — September 12

Native job 100 measures 72 finite-bitmap cases and three extreme-coordinate
bounds queries. All bounds and sampled RGBA values match Wine at feature level
11.0. Job 101 measures 144 custom-source cases, including infinite bounds; all
match Wine. At feature level 10.0 the 48 bitmap and 96 custom-source cases using
plain/chained offsets also match the corresponding Windows records. Opacity
combinations are exercised only at 11.0 because Wine's separate Opacity renderer
currently requires that level. These tests do not establish lower-level Opacity
support. Job 099 timed out with partial output and is superseded by job 100.

Updated focused job 103 passes 1,153 checks on Windows and Wine, including source
pixel preservation. The full focused suite runs 32 cases / 180,216 checks with
86 TODO failures and zero ordinary failures
(`offset-transform-full-focused-2.log`). The earlier focused revision attempted
unsupported feature-10 Opacity setup and is superseded by this run.

All 312 original application binaries remain unchanged. The application now
passes offset evaluation and fails at command 12 (DrawBitmap) during command-list
replay (`paintnet-20260912-063228-439553.log`, `pdncrash.44.log`). Editing, final
compositor presentation, and native file dialogs remain unverified.

## Invert and bitmap-replay progress — September 12

Native job 105 measures single/double inversion on float images with positive,
zero, negative, tiny, and above-one alpha values. All eight result records across
feature levels 11 and 10 match Wine. Native job 107 adds 72 graph/DPI/unit cases
per feature level; all bounds and sampled pixels match. The focused regression
passes 630 checks on Windows job 108 and Wine, including source preservation.
The earlier job 106 used a mismatched alpha format for its source-copy check and
is superseded by the corrected test; this was a test setup error.

The full focused suite runs 33 cases / 180,846 checks, with 86 TODO failures and
zero ordinary failures (`invert-full-focused.log`).

Local bitmap-replay probe 104 has 128 cases. The 112 non-perspective bounds now
match Windows; 36 of those cases retain pixel differences in antialiasing or
filtering. Sixteen perspective cases remain unsupported. This prototype is not
yet committed as complete command-list rendering support. With it, the unchanged
application passes command 12 and fails at missing Invert registration in
`paintnet-20260912-064937-448961.log` (`pdncrash.45.log`).

After Invert implementation, all 312 application binaries still pass verification.
`paintnet-20260912-065431-452073.log` (`pdncrash.46.log`) fails during brush image
bounds evaluation. The final unsupported input is Gaussian Blur (effect
`000074CCA4F25220`, CLSID `1feb6d69-2fe6-4ac9-8c58-1d7f93e7a6a5`). The main
thread separately reports unsupported command 8 (DrawGlyphRun) in a command list.
The editor remains unusable.

## Half-float WIC format registration — September 12

Wine now registers the 16-bit gray, 48-bit RGB, 64-bit RGBA, and 64-bit
premultiplied RGBA half-float pixel formats. Native job 114 and Wine agree on
105 non-name result records covering channel masks, buffer-size behavior,
numeric representation, transparency, and bitmap storage. Friendly-name
capitalization differs. The focused `half_format` test passes 197 checks on
both Windows (job 115) and Wine, including padded multirow storage and cropped
pixel copying. This registration does not implement format conversion.

The full focused suite passes 34 cases / 181,043 checks, with 86 existing TODO
failures and zero ordinary failures (`wic-half-full-focused.log`). This run
includes the local Gaussian Blur and glyph-replay prototypes, but the suite
does not yet establish their pixel conformance. Native Gaussian probes 109,
111, and 113 and glyph probe 112 retain measured differences.

The unchanged application's History-panel drawing passes with glyph replay.
`paintnet-20260912-071132-459411.log` (`pdncrash.48.log`) then fails during
Layers-panel half-float format metadata lookup. With registration installed,
`paintnet-20260912-071508-463572.log` (`pdncrash.49.log`) passes that lookup and
fails at `CreateBitmapFromWicBitmap` for `GUID_WICPixelFormat64bppPRGBAHalf`.
Direct2D upload support is the next separate change. The editor is still unusable.

## High-precision WIC bitmap uploads — September 12

Direct2D now uploads 64-bit integer RGBA, 64-bit half-float RGBA, and 128-bit
float RGBA WIC bitmaps. It inherits premultiplication metadata and accepts
explicit premultiplied/ignore-alpha overrides for straight-alpha sources, as
measured on Windows. Incompatible DXGI views return E_INVALIDARG. Compatible
sRGB views of 8-bit sources remain supported, and RGB sources reject alpha
modes that require a stored alpha channel. The allocation check also prevents
32-bit total-byte-count overflow before CopyPixels.

Native job 116 covers 200 upload cases. All status, metadata, and raw bytes
match Wine; maximum sampled pixel difference is 1.5e-8. Expanded job 118 adds
RGB and sRGB cases (312 total). Every status, metadata, and raw-byte record
matches. Four sRGB rendering cases differ by up to 0.000727 in a float channel,
consistent with the current llvmpipe texture-decoding approximation. This
change does not claim exact sRGB rendering. The regression uses 0.001 absolute
tolerance for sRGB decoding, 0.000001 otherwise, and exact raw-byte comparisons.

The focused regression also modifies and releases each WIC source after upload
to check that the GPU bitmap owns its pixel copy. Windows job 120 and Wine each
pass 10,246 checks across feature levels 11.0 and 10.0. The earlier stricter
sRGB comparison exposed 80 Wine pixel differences; they are accounted for by
the explicit sRGB tolerance, rather than described as fixed. The full suite
passes 35 cases / 191,289 checks with 86 existing TODO failures and zero ordinary
failures (`wic-upload-full-focused.log`).

All 312 original Paint.NET binaries remain unchanged. The Layers-panel upload
now succeeds. `paintnet-20260912-072935-469719.log` (`pdncrash.50.log`) instead
fails on command 20 (PushLayer) while obtaining command-list bounds. Drawing
layers and final compositor presentation are still missing; the editor remains
unusable.

## Crop and the current startup boundary — September 12

The Crop implementation uses pixel shaders and retains the source's pixel origin.
The native measurements refine the documented [Crop behavior](https://learn.microsoft.com/en-us/windows/win32/direct2d/crop):
rectangle setters normalize inverted coordinates, the default contains infinities,
output bounds round before intersection, and negative soft borders use signed
fractional parts. No managed Paint.NET renderer or Microsoft Direct2D DLL is used.

Native job 127 measures 768 cases combining positive/negative offset transforms,
two source alpha modes, soft/hard borders, negative fractional edges, three DPIs,
and both unit modes. All bounds and pixels match Wine within 0.000001; maximum
sampled difference is 3.58e-7. Native job 128 adds 228 crop/property cases including
empty, inverted, infinite and NaN rectangles. Every record matches, with maximum
finite channel difference 1.78e-7. The source probes are `crop-negative-reference.c`
and `crop-render-reference.c`. Job 125 separately establishes enum rejection and
preservation of prior property values.

The focused `crop` regression uses the native pixel fixtures and repeats on
feature levels default and 10. Windows job 129 and Wine each pass 25,812 checks,
with no TODOs. `crop-full-focused.log` passes 37 cases / 219,195 checks, 96 existing
TODO failures, zero ordinary failures, and zero flaky failures. Initial probe 124
uses a fractional destination offset at 144 DPI: 18 cases still have DrawImage
placement/sampling differences despite matching Crop bounds. Probe 126 retains
additional fractional-offset evidence; the aligned probes above isolate Crop.

The unchanged app's `pdncrash.53.log` / `paintnet-20260912-081547-491678.log` no
longer reports the Crop bounds failure. Its remaining main-thread failure is
command 10 (DrawGeometry) during command-list evaluation. An earlier thread also
hits rounded-rectangle GetBounds. Editing and compositor presentation are still
unverified.

The full run includes uncommitted layer, glyph, bitmap replay, Gaussian Blur and
Color Management prototypes. Layer probe 121 matches 150 of 160 pixel cases and
all bounds; ten ellipse-edge cases differ. Native focused job 122 passes 2,094
checks, while Wine records ten TODO failures and no ordinary failures. The
DCompositionBoostCompositorClock export returns E_NOTIMPL, an explicit unsupported
result that the app ignores; it is not a refresh-boost implementation.

## Ellipse and rounded-rectangle bounds — September 12

Native ellipse probe 049 covers seven signed/zero radius pairs and five matrix
choices. Rounded probe 131 covers 225 cases: five normalized/inverted/degenerate
rectangles, nine radius pairs, and null/identity/shear/zero/rotation matrices.
Rounded bounds match Wine within 0.000001 (maximum measured difference 9.6e-7).
Both shapes use transformed cubic extrema rather than the bounding box's corners.
The native reference files are `ellipse-bounds-reference.c` and
`rounded-bounds-reference.c`.

The `curve_bounds` regression checks all these native bounds and repeats them
through transformed-geometry wrappers. Windows job 132 and Wine each pass 2,914
checks, without TODOs. `curve-bounds-full-focused.log` passes 38 cases / 222,109
checks with 96 existing TODO failures and zero ordinary/flaky failures.

The local command-list DrawGeometry prototype uses stroke-outline bounds and
replays the recorded operation. Native probe 130 has 256 cases; all 128 native
direct/replay pairs match. Wine matches 126 pairs, with two one-pixel differences
for a transformed nonuniform ellipse at 144 DPI. A circle-bounds correction removes
32 earlier bound differences. Sixteen nonuniform-ellipse bound cases still differ
by up to 0.008834, and four triangle cases by at most 1.9e-6. Many native pixel
comparisons also retain the underlying Wine stroke/antialiasing differences.
This prototype is not presented as full command-rendering conformance.

The unchanged app now passes the previous color-wheel failure. Its next failure
is Composite bounds inside a Layers-panel image graph. Flood is separately
unhandled in an earlier draw. This is `pdncrash.54.log` and
`paintnet-20260912-082911-499220.log`; all 312 original runtime binaries were
verified before launch.

## Flood and Composite pixel rendering — September 12

Flood now renders the requested region of its infinite solid-color output with
cached shader-model-4 shaders. Its raw color, including HDR, negative, NaN and
infinite components, is preserved. The default color is opaque black.
Composite now folds its inputs in order with all thirteen documented modes,
computing each mode's union, intersection, source or destination bounds. Input
count changes rebuild the authoring graph; the builtin evaluator consumes the
current inputs. Invalid mode values return E_INVALIDARG without changing state.

Native Windows WARP and Wine/DXVK/llvmpipe match exactly for 240 Flood cases
(direct and cropped, three DPIs, DIPs/pixels, source-over/source-copy) and 702
Composite cases (one to three inputs, three origin arrangements, thirteen modes,
three DPIs, DIPs/pixels). Composite input alpha includes values outside [0,1],
and RGB includes negative and HDR values. The original Composite logging probe
accidentally overwrote SetValue's HRESULT during GetValue; the corrected probe
136 confirms the invalid-mode HRESULT and retained previous value. Render case
results were unaffected by that probe defect.

The same focused executable passes on Windows and Wine at both the default
feature level and feature level 10: Flood 7,712 checks and Composite 15,626 checks,
no TODOs or failures. The complete local focused suite passes 40 test cases,
245,447 checks, 96 existing TODO failures, zero ordinary or flaky failures.
Evidence: native jobs 133, 134, 136, 137; flood-first-comparison.log,
composite-graph-comparison.log, flood-composite-focused.log and
flood-composite-full-focused.log in the isolated work directory.

Limits: the shared graph evaluator currently accepts at most eight inputs per
node. Infinite sources still require a requested finite region when rendering;
parent-specific region expansion and all unbounded nested-graph paths are not
implemented. Composite currently allocates intermediate surfaces for each fold;
resource pooling and extreme-coordinate coverage remain future work.

With all 312 original application binaries verified unchanged, startup now
reaches the editor (paintnet-20260912-084719-508658.log). The editor has visible
canvas and toolbar defects. A brush click crashes in animation scheduling
(pdncrash.55.log); a second traced run reaches mask rendering and fails the A8
WIC render-target format (pdncrash.56.log,
paintnet-20260912-085022-510330.log). Editing and save/reopen are not yet working
reliably. These startup results also include the older local rendering prototypes.

## Recorded payloads, concurrent font cache, and A8 metadata — September 12

Native job 141 and Wine pass the same focused executables: command_data 2,067
checks, font_cache 304 checks, alpha_format 25 checks, no TODOs or failures.
Command recording repeatedly grows its buffer while retaining all four variable
payload types, then validates them through the public command sink. The baseline
reports corrupt glyph indices, advances, offsets and description and fails to
complete. Eight isolated-font-factory rounds start twelve callers together and
retain every returned collection. The baseline underretains the common collection
(one reference for twelve callers) in all eight rounds.

The A8 test checks pixel metadata, channel masks, allocation and cropped/padded
CopyPixels data. Reference job 139 also confirms native alpha-format metadata.
The full restored-display suite passes 43 cases / 247,843 checks, with 96 existing
TODO failures, no ordinary/flaky failures, and no skips. Evidence logs are
command-data-before.log, command-data-fixed.log, font-cache-before.log,
font-cache-fixed.log, wic-alpha-focused.log, and
memory-full-focused-display-restored.log. An earlier suite ran after the private
X display had exited; its window failures are environmental, and that run is not
used as passing validation.

Application binaries remain unchanged (312 verified). Restoring the display
removes the window-handle failure in pdncrash.61.log. The subsequent launch
paintnet-20260912-092403-530078.log / pdncrash.62.log fails LayersStrip rendering:
a custom effect below Gaussian Blur tries to allocate INT_MIN..INT_MAX bounds.
Finite requested-region propagation remains under investigation. No working
editing/save workflow is claimed.

## WIC target formats and synchronization — September 12

Native probe 138 covers 576 format/alpha/type combinations across twelve WIC
formats, six DXGI choices, four alpha modes, and DEFAULT/SOFTWARE target types.
It measures creation HRESULTs, resolved formats, existing bitmap contents, Clear,
and repeated rectangle drawing. Probe 140 changes the WIC bitmap directly
between drawing batches and checks subsequent empty and drawing batches. The
Wine implementation matches every measured status and byte, including A8's
native default straight-alpha mode. Premultiplied alpha is also accepted for A8.
Unsupported WIC formats fail before explicit DXGI overrides are considered.

The wic_target test combines native format outcomes with all six bitmap phases.
Native job 143 and Wine each pass 2,534 checks without TODOs or failures. Full
wic-target-full-focused.log: 44 cases, 250,377 checks, 96 existing TODO failures,
zero ordinary/flaky failures and no skips. Evidence also includes
wic-target-first-comparison.log and wic-target-update-first.log.

The local Gaussian region change expands requested inputs by the blur footprint
instead of dropping finite region limits. Reference 142 includes infinite custom
shaders, Flood, nested blur, Crop and Composite at 96/144 DPI. All 80 statuses and
bounds match Windows; 66 pixel cases agree within 1e-6. Fourteen finite-crop blur
cases retain errors up to 0.042697, so Gaussian conformance remains incomplete.

Application launch paintnet-20260912-093450-534484.log remains open until a brush
interaction. A8 mask creation succeeds; pdncrash.63.log instead fails in
AnimatedValue.TryAnimateRawValueCore while committing the stroke. This is still
an unusable-editor result, not a successful editing workflow.

## Default animation arbitration — September 12

Native reference 144 has 144 two-storyboard cases: linear, smooth-stop and looping
old tracks; linear, smooth-stop and instantaneous new tracks; four delay policies;
optional unrelated variable tracks; immediate and future requested starts. All
measured values, previous/final values, statuses, elapsed times and current
storyboards match Wine exactly. Reference 146 repeats the matrix with a third
queued storyboard and reversed object-creation order; all 144 cases also match.
The focused conflict and queue executables pass 19,393 and 23,569 checks on both
Windows (jobs 145/147) and Wine, without TODOs. Existing timeline/keyframe tests
still pass. Full animation-conflict-full-focused.log: 46 cases / 293,339 checks,
96 existing TODO failures, no ordinary/flaky failures and no skips.

The app diagnostic paintnet-20260912-094155-538772.log explicitly confirms the
previous brush failure: Schedule rejects an already animated variable. With
arbitration installed, paintnet-20260912-094815-540814.log / pdncrash.65.log passes
scheduling but fails WICBitmapSource.CopyPixels while building a brush sprite.
The WIC trace paintnet-20260912-095146-542588.log / pdncrash.66.log identifies
copypixels_to_32bppBGRA's unsupported source conversion while targeting PBGRA.
These remain failed brush tests; no successful editing workflow is claimed.

Limits: custom priority comparison callbacks are still explicitly unsupported.
The new tests cover default arbitration, not all callback orderings, overlapping
keyframe intervals, compressed schedules, or every cancellation/lifetime edge.

## Floating-point WIC conversion — September 12

Native references 148 and 150 cover float RGBA/PRGBA to all four byte RGBA/BGRA
layouts and to both float alpha layouts, and all four byte layouts to float.
Inputs include zero/fractional alpha, RGB exceeding alpha, negative/HDR float
channels, NaN, and infinite alpha. Each conversion copies a full image, a cropped
rectangle and a one-pixel edge, with padded strides and a minimally sized buffer.
The final float-source reference matches all measured status and output bytes.
Byte-to-float output agrees within 2e-7; the floating-point transfer function can
differ by small rounding amounts. Byte outputs and float-source outputs are
checked exactly in the focused fixtures.

Reference 149 sweeps 262,145 samples over [0,1] and another 262,145 over [0,.02].
Every observed output transition matches after quantizing the linear input to
steps of 1/3354 before sRGB conversion. Reference 152 exhaustively checks all
65,536 input channel/alpha pairs in PBGRA and PRGBA: 16-bit fixed-point reciprocal
unpremultiplication reproduces the observed byte indices before linearization.
Reference 151 measures rectangle errors, short strides, undersized buffers and
unchanged output on failure across sixteen format pairs (160 cases).

The focused float_conversion regression passes **209,611 checks** with no TODOs,
failures or skips on native Windows (job 153) and Wine. It checks exact byte
pixels, float values, crop positions, every padding/guard byte, dense gamma
intervals, exhaustive unpremultiplication row hashes, and error HRESULTs.
Full wic-float-full-focused.log: **47 cases / 502,950 checks**, 96 existing TODO
failures, zero ordinary/flaky failures and no skips. The upstream WIC converter
case passes the same 20,313-check run before/after output bounds validation,
retaining twelve existing missing-format failures, 133 reported TODOs and nine
skips; this broader suite is not claimed to pass.

Application evidence: paintnet-20260912-100423-548004.log / pdncrash.67.log first
exposes the reverse PBGRA-to-float conversion after float-to-PBGRA is available.
With both implemented, paintnet-20260912-100912-550122.log / pdncrash.68.log passes
those paths but fails with AccessViolationException while disposing a
DWriteTextLayout4 in the drawing-resource cache. Drawing remains unsuccessful;
the next investigation is the text-layout/resource lifetime error.

## Native chooser integration (September 12)

- `test-native-dialog.sh`: **103 checks**, zero TODOs, failures or skips. The
  deterministic peer tests Wine transport/lifecycle behavior rather than GTK.
- Existing `comdlg32:itemdlg` with the bridge disabled: **1,315 checks**, 27 known
  TODOs, zero ordinary failures or skips.
- Official Paint.NET, all 312 binaries verified: GTK Save PNG → GTK Open PNG →
  GTK Save PDN → fresh-process Open PDN → GTK Save PNG. Both PNGs exactly match
  all 480,000 source RGBA pixels. Unicode names and format extension changes
  are verified; cancelling overwrite preserves hash and modification time.
- Logs: `work/classic/logs/native-chooser-final-transport.log`,
  `native-chooser-upstream-itemdlg.log`, and application logs
  `paintnet-20260912-124441-619006.log` / `paintnet-20260912-125344-623041.log`.
- Report: `work/classic/validation/native-chooser-roundtrip-report.json`.
- Normal activation QA now runs with a private Openbox on Xvfb `:93`. Reconnecting
  noVNC cleared stale modifier state; cancellation followed by keyboard Save As
  worked without clicking the editor. The real user's desktop remains separate.

No native Windows conformance claim is made for the GTK transport test. Remaining
callback, custom-control and desktop-backend limitations are in `README.md` and
`STATUS.md`.
