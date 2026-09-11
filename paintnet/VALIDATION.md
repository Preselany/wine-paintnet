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
