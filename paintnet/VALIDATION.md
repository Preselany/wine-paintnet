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
