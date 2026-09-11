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
initializer to fail. A separate failure occurs during custom-effect disposal.

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

## Observed remaining failures

* Missing Histogram effect registration/implementation. Metadata alone must
  not be treated as a functioning histogram renderer.
* Missing `ID2D1EffectContext1`, queried by Paint.NET's device feature probe.
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
