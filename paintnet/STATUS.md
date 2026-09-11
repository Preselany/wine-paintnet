# Status

## Scope

This project targets the unmodified stable Windows application through Wine's
Direct2D implementation. The earlier success using Paint.NET's experimental
managed renderer does not count toward this project's compatibility status.

## Verified baseline — September 11, 2026

The official Paint.NET 5.1.12 x64 portable archive passes its published SHA-256:
`d5ae7043f2fb9d365b48dfe243a2aca1c74924de99b04b6445916c95354aefa3`.
All 312 application/runtime EXE and DLL files are checked against that archive
before launch. Testing uses an isolated WineHQ 11.17 prefix and Xvfb display
with DXVK 3.1 on llvmpipe. The trace confirms Wine's builtin `d2d1.dll` loads.

The original application **does not yet start successfully**. Its first effect
metadata lookup fails for `CLSID_D2D1Histogram`
(`881db7d0-f7ee-4d4d-a6d2-4697acc66ee8`), causing the `EffectCategories` type
initializer to fail. A separate failure was observed during custom-effect disposal
in that initial baseline.

## First Wine fix: custom effect COM interfaces

Wine previously cast the `IUnknown*` returned by an effect factory directly to
`ID2D1EffectImpl*`. A legal COM object can expose those interfaces at different
addresses. Calling through the wrong vtable can skip initialization or corrupt
the object's state.

The fix queries for `ID2D1EffectImpl`, retains the original factory pointer for
property binding callbacks, and releases both acquired references. A missing
interface or failed initializer is propagated with proper object cleanup.

The focused regression constructs an effect with distinct interface pointers.
It checks initialization, property get/set/size callbacks, interface rejection,
failure propagation, destruction, and the existing builtin effect path.
Before the fix, the regression exposes six failures in stock Wine. Test results
are recorded in `VALIDATION.md`.

The unchanged application was retested with the rebuilt Wine module. The custom
effect now reaches a real `ID2D1EffectContext1` query and fails cleanly with
`E_NOINTERFACE`; the prior disposal access violation was not observed in that
run. This is a partial API correction, not evidence of successful rendering.

## Second Wine change: effect-context interface and lookup-table resources

Wine now exposes `ID2D1EffectContext1` with the same COM identity as its base
interface. Its inherited calls use the existing context implementation. The
new `CreateLookupTable3D` method and the device-context entry point share an
implementation that uploads the supplied RGBA data into an immutable Direct3D
3D texture and creates a shader resource view. It supports the five explicit
buffer precisions and validates row/plane strides and data bounds before upload.

The focused test passes 71 checks; stock Wine and the previous fork revision
each fail seven of the 19 checks they can reach. The original identity test
still passes 26 checks. A separate internal diagnostic reads the actual texture
back and verifies 120 rows across five formats with tight and padded layouts,
after overwriting the caller's source buffer.

This implements resource creation, not the separate `CLSID_D2D1LookupTable3D`
image effect. No 2D-texture fallback for lower feature-level devices is provided.
Existing unimplemented methods inherited from the base effect context remain
unimplemented. Native Windows conformance, including exact invalid-argument
results and dimension limits, still needs validation.

The application was retested with all 312 binaries verified unchanged. This
run failed on Histogram metadata before reaching the custom feature effect's
initialization; the error window also encountered the same DispatcherQueue
activation failure. The new context path is verified by the focused test,
not by a successful Paint.NET startup.

## Third Wine change: Histogram and Opacity Metadata

Histogram is now a registered analysis effect with a real Direct3D 11 compute
shader. Drawing it counts clamped, unpremultiplied channel values and exposes
the normalized bins through the output property. It accepts bitmap inputs and
bitmap inputs wrapped in Opacity Metadata. The implementation currently requires
feature level 11.0; compute-capable feature-level 10 devices and general input
effect graphs remain unsupported and are not reported as successful analyses.

Opacity Metadata stores and returns its opaque-region hint while preserving
input pixels. Nested metadata effects resolve to the original bitmap for
rendering and local bounds, with cycle detection. The renderer does not yet use
the hint to optimize blending. Focused tests verify source-pixel preservation,
alpha values, crop/offset handling, and recovery after breaking a cycle.

The normal application now gets past both metadata lookups. The next missing
registration is Alpha Mask (`c80ecff0-3fd5-4f05-8328-c5d1724b4f0a`). Its crash
log also exposes a missing `CreatePresentationFactory` export from `dcomp.dll`.
All application binaries still pass the original-archive integrity check.

## Fourth Wine change: Alpha Mask and branched graphs

Alpha Mask now computes destination RGBA multiplied by mask alpha in a real
Direct3D compute shader. Its intermediate image can feed another Alpha Mask,
Opacity Metadata, or Histogram. The evaluator walks both input branches with
cycle detection and answers local-bounds queries without rendering. Source
bitmaps remain unchanged; floating-point intermediate textures retain HDR color
values. It currently requires feature level 11.0. Precision selection, caching,
lower-feature-level support, and general custom transform graphs remain open.

The focused cases now pass 762 checks: 488 for Alpha Mask and its input graphs,
130 for Histogram, and 144 for earlier COM/context/metadata tests. Pixel tests
cover transparency, nested masks, crops, target offsets, changing mask contents,
96/192 DPI drawing and pixel units, and reading Histogram bins from masked
output. The lookup diagnostic still verifies 120 rows. The existing Direct2D
suite matches the previously recorded stock-Wine baseline. Native Windows
conformance has not yet been tested.

Effect output coordinates now use context DPI. This also corrected the older
Histogram crop test's source-DPI assumption; its new checks explicitly vary
context DPI. Full details and the limitations are in VALIDATION.md.

The unchanged application passes the Alpha Mask category lookup. Its next
failure is Convolve Matrix (`407f8c08-5533-4331-a341-23cc3877843e`). The new
crash log still reports the absent presentation-factory export during diagnostic
collection. No editor, editing operation, or file round trip has been verified.

## Observed remaining failures

* Missing Convolve Matrix effect registration/implementation, now the first
  failing category lookup. Further builtin effects and general effect-graph evaluation
  still need implementation.
* Retest Paint.NET's device feature probe after its effect-category initializer
  can finish. The EffectContext1 regression passes independently.
* Missing `dcomp.dll!CreatePresentationFactory`, observed during diagnostics.
* `Windows.System.DispatcherQueue` activation fails with `REGDB_E_CLASSNOTREG`
  when Paint.NET creates display-aware windows, including error-reporting UI.
* The broader Direct2D drawing/effect pipeline, animation, and composition
  requirements still need investigation and rendering tests.

No editing, image correctness, or full application compatibility is claimed.

## Work sequence

1. Capture the first failure in the unchanged application.
2. Reduce missing API behavior to focused Wine regression tests.
3. Implement the required behavior and advance to the next observed failure.
4. Validate rendering with images, not just successful HRESULTs.
5. Run the editing and file round trips listed in README.md.

Native GTK file-dialog work from the earlier experiment is preserved separately
and has not been deployed to this baseline.
