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
