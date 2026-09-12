# Status

## Scope

This project targets the unmodified stable Windows application through Wine's
Direct2D implementation. The earlier success using Paint.NET's experimental
managed renderer does not count toward this project's compatibility status.

## Current state — September 12, 2026

The original application reaches the editor in the private Wine test display,
but the editor is not yet usable. Startup now passes the initial effect metadata,
COM interfaces, custom image shaders, geometry operations, WIC target creation,
and overlapping animation scheduling. Two committed memory fixes preserve
recorded command payloads during buffer growth and keep the correct references
during concurrent font-collection initialization.

The local Gaussian requested-region change removes the Layers-panel infinite
allocation failure. It remains a prototype with measured blur-edge differences.
The committed WIC conversion work now passes both directions of the brush's
floating-point bitmap conversion. The latest brush test instead fails while
releasing a DirectWrite text-layout object (pdncrash.68.log); its underlying
memory error remains under investigation. No brush stroke has been successfully
verified, and saving/reopening is unverified.

Toolbar labels still render incorrectly. Gaussian Blur, glyph replay, layer
mask edges, and other local rendering prototypes have measured Windows
differences. Compositor visual presentation remains unimplemented.

Command-list DrawImage replay and source-copy compositing are local prototypes.
Their 96-DPI image cases largely match Windows, but higher-DPI and fractional
sampling differences remain; they are not presented as finished rendering work.

These runs include command-list rasterization, Color Management, and gradient
rendering work. ICC conversion and primitive antialiasing/strokes still have
measured differences. Gradient gamma-1 quantization and radial-clamp sampling
also need refinement. Editing, save/reopen, native Linux dialogs, and hardware
performance remain unverified.

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

## Fifth Wine change: Convolve Matrix and image coordinates

Convolve Matrix now has its own properties and a compute shader that applies
an arbitrary matrix, divisor, and bias. It supports transparent padding or
mirrored borders, fractional kernel offsets, alpha preservation, and clamping
before premultiplication. Intermediate coordinates can extend left/up of zero,
and survive nested convolution, Alpha Mask, crop/offset drawing, and Histogram.
Alpha Mask and convolution share resource creation and compute dispatch code.

The convolution case passes 1,496 checks and all six focused cases total 2,258
passing checks. The lookup diagnostic still verifies 120 rows. A regression
caught by the existing drawing suite was fixed: inverted DrawImage rectangles
again follow its established native behavior. The final full suite matches the
stock-Wine baseline. These tests do not establish native Windows conformance.

Current convolution rendering requires one input pixel per kernel unit after
context-DPI conversion. Other spacings return E_NOTIMPL because their resampling
passes are still missing. The effect also currently requires feature level 11.0;
precision selection and caching remain incomplete. These limits are not full
Convolve Matrix compatibility.

The unchanged application passes the Convolve Matrix metadata lookup. Its next
missing registration is Contrast (`b648a78a-0ed5-4f80-a94a-8e825aca6b77`). It
still crashes during effect-category initialization; no editor has opened.

## Sixth Wine change: Contrast

Contrast now has its own properties and a compute shader for the documented
piecewise-quadratic color adjustment, with optional input clamping and alpha
preservation. It accepts the supported input graphs and preserves their image
origin. Seven focused cases pass 2,590 checks, including 332 for Contrast; the
full Direct2D suite still matches the recorded stock-Wine baseline.

The curve coefficients and interpolation over the Contrast parameter were
inferred from Microsoft's graph and description. Native Windows numeric output
has not been captured, so exact transfer-function conformance is still open.
The implemented path requires feature level 11.0 and uses float intermediates.

All 312 application binaries remain unchanged. Paint.NET now passes Contrast
metadata and next fails on Bitmap Source
(`5fb6c24d-c6dd-4231-9404-50f4d5c3252d`). The editor still has not opened.

## Seventh Wine change: Bitmap Source

Bitmap Source now retains an IWICBitmapSource property and turns its decoded
pixels into a real image in the supported effect graph. The implementation
preserves premultiplied alpha and HDR float values, handles common 8-bit and
16-bit source formats, performs nearest/linear CPU scaling, and applies the
eight documented rotations/flips. DPI correction and crop/offset drawing have
focused checks. Repeated draws reuse the decoded/uploaded bitmap until a
property or effective context DPI changes.

The first test also exposed an XML parser bug with self-closing Inputs elements
containing attributes followed by another property. Restoring the reader to the
element before checking whether it is empty fixes three stock-Wine failures.
The source case passes 7,337 checks; all eight focused cases pass 9,927 checks.
The full Direct2D suite still matches the stock baseline.

Straight-alpha output, cubic/Fant/mipmap interpolation, and other WIC source
formats remain unsupported. Exact native bounds rounding, combined orientation
ordering, source invalidation, and DPI semantics need Windows comparison. The
pixel and cache tests exercise this implementation and do not establish native
conformance or full application performance.

The unchanged application now passes Bitmap Source metadata. Its next missing
effect is Emboss (`b1c5eb2b-0348-43f0-8107-4957cacba2ae`); startup still fails
in EffectCategories. All 312 original binaries remain intact.

## Independent Windows comparison

A separate Windows 11 evaluation VM now runs the focused tests against the
system Direct2D DLL and Microsoft WARP. This identified real discrepancies in
several earlier Wine-only expectations; passing those checks did not establish
native compatibility. The corrected COM identity regression passes all 28
checks on Windows. Pixel, property-clamping, bounds, and cache differences in
other cases remain under correction. See REFERENCE.md for the recorded baseline.
The original Paint.NET application still fails before opening the editor.

## Corrections from the Windows reference

Contrast now uses the measured 0.75 curve coefficient and clamps its amount
property. Alpha Mask and Contrast consume stored input alpha even for bitmaps
created with IGNORE alpha mode. Opacity Metadata defaults to actual infinities.
Invalid graph inputs now return the measured graph-configuration error.

Convolve Matrix now reverses kernel tap coordinates, trims unused rows/columns
when computing bounds, and filters premultiplied RGBA when PreserveAlpha is
false. Its PreserveAlpha path filters straight RGB and premultiplies afterward.
DrawImage preserves negative image origins unless an explicit crop supplies a
new origin. One-hot kernel pixel tests verify these distinctions on Windows.

Those four effects now have matching passing focused cases on Wine and Windows.
The full Wine drawing suite retains its recorded baseline, and the lookup
upload diagnostic still passes. Histogram, Bitmap Source, and effect-context
conformance differences remain; see REFERENCE.md. Emboss is still unimplemented.

## Eighth Wine effect: Emboss

Emboss now applies the measured grayscale surface-lighting filter on the GPU,
including image-boundary stencils and the DPI resampling pass. Its 23 native
pixel fixtures pass 4,919 checks on both Wine and Windows. A wider comparison
matches 2,970 full readbacks and a separate 256-case 8x8 basis sweep. See
REFERENCE.md for the numerical results and remaining DPI/caching limitations.
All nine Wine cases total 15,662 passing checks; the existing drawing suite
retains its stock baseline.

Paint.NET was launched again with all 312 original binaries verified. It gets
past Emboss and now fails on Opacity metadata
(`811d79a4-de28-4454-8094-c64685f8bd4c`). The editor still has not opened.

## Ninth Wine effect: Opacity

Opacity now renders a clamped alpha multiplier through the GPU compute path.
The same 470 checks pass on Wine and native Windows. All ten focused Wine
cases pass 16,132 checks, and the existing drawing suite retains its baseline.

Paint.NET now clears EffectCategories and reaches main-window construction.
Its first exception is UIAnimationTransitionLibrary.CreateSmoothStopTransition,
called while initializing the Colors panel. All 312 original binaries were
verified unchanged; this is progress through startup, not a working editor.

## UIAnimation timelines

Smooth-stop, linear, and instantaneous transitions now run through a real
time-driven storyboard engine. It updates values, preserves initial velocity,
tracks states and previous/final values, and dispatches callbacks in measured
Windows order. Timer GetTime uses the performance counter. The focused native
and Wine cases each pass 2,114 checks; 757 reference records match, including
callback order and shutdown behavior.

This is a first animation implementation. Keyframes, conflict arbitration,
custom interpolators, other transition types, and timer-driven callbacks still
need work. Those paths are not claimed as complete.

The original application gets past smooth-stop initialization. Its next crash
is in ComputeSharp.D2D1ReflectionServices.GetShaderInfo while creating the
checkerboard effect. Wine's shader-reflection GetMinFeatureLevel remains a
stub; GetRequiresFlags also returns a placeholder. The editor is not usable.

## Shader reflection requirements

Wine's shared shader reflection now reports minimum feature levels for shader
models 4.0, 4.1 and 5.0, including the legacy 9.1/9.3 profiles. Required feature
bits come from both shader declarations and SFI0 metadata, including doubles,
early depth/stencil, minimum precision and optional shader extensions.

Eighteen Windows-compiled shaders pass all 72 checks on Wine and Windows. The
existing reflection suite passes 1,348 checks on both stock and modified Wine.
All thirteen focused project cases execute 18,332 checks without failure or
skips (one pre-existing custom-animation-factory todo remains).

Paint.NET now passes shader inspection. Its next failure is
ID2D1RenderInfo.SetOutputBuffer while configuring its checkerboard draw transform.
Custom transform rendering must be implemented alongside that state API; simply
accepting the call would not produce a correct image. No editor is usable yet.

## Custom source-effect shaders

Zero-input `ID2D1DrawTransform` nodes now execute their loaded pixel shaders on
Direct3D. Draw info owns the shader and constant data, validates output settings,
and preserves settings after rejected shader selections. The generated vertex
shader supplies the measured `SCENE_POSITION` coordinates. Bounds and dirty
properties/DPI drive the effect callbacks; infinite source images are restricted
to the requested visible region before allocation.

Windows and Wine pass the same 75,824-check regression: source shaders and a
source feeding Opacity, normal and cached output, all six precision settings,
all three valid channel depths, negative bounds, crops, 96/192 DPI, constant
ownership, HDR values, and stored alpha. Three standalone comparisons cover
960 records and 55,296 rendered channels with no pixel/API-result mismatch at
2e-5 tolerance. Redundant bounds-mapping counts differ for cached output.

This implementation is limited to zero-input draw nodes. Input sampling,
resource textures, general graphs, custom vertex processing and compute nodes
are still unsupported. Arbitrary affine transforms and interpolation need
native measurement beyond the tested identity transform and integer offsets.
Cached output has measured channel behavior, but intermediate reuse and GPU
performance still need optimization. Precision tests currently use a float32
final target; other target formats need independent conformance coverage.

The application advances from SetOutputBuffer to command-list closing.
No Paint.NET editing session is yet verified.

## Command-list recording and lifecycle

Changing a device context's target during an active drawing session now starts
command-list recording with the current drawing state. Rebinding captures state
changes; repeated sessions preserve earlier commands. Closing accepts empty
lists, detaches bound targets, and preserves errors for later use. EndDraw now
handles command-list targets and error tags. Lists retain their device, reject a
different resource domain, and handle competing writers without changing the
first writer's successful EndDraw result.

The same 8,028-check test passes on Windows and Wine. It compares 437 operation
records plus 27 small images obtained by replaying the recorded commands through
a public command sink. It covers target switching, state changes, copied brush
values, empty/repeated closing, invalid ordering, two contexts, device domains,
and context destruction. Automatic DrawImage(commandList) and command-list
inputs to effect graphs remain separate, unimplemented rendering paths.

Paint.NET passes this failure and next stops at sRGB color-context creation.
The editor has still not opened.

## Color-profile resources

Direct2D now creates ICC, DXGI, and simple color contexts with factory ownership,
profile-byte copying, buffer-size handling, and bitmap reference retention.
WIC and filename loading recognize the measured sRGB model and scRGB profile ID;
the memory API preserves CUSTOM. EXIF sRGB and Adobe RGB contexts are supported.
Built-in profiles use Wine's bundled Little CMS and published color definitions,
with cached ICC serialization; they do not copy Windows' profile data.

The resource test passes 4,435 checks on Windows and 3,511 on Wine, with different
counts because the buffer tests visit every profile byte. There are no failures,
todos, or skips. A separate comparison transforms 3,216 RGB samples through
exported ICC profiles into XYZ (9,648 channel comparisons). Unit-range differences
are below 0.00032; extended-range differences are below 0.00064. These checks cover
profile semantics, not the still-missing Direct2D Color Management renderer.
Serialization sizes, timestamps, and metadata differ from Windows. The generated
scRGB profile has its own ICC representation and is not recognized by Windows'
reserved scRGB profile ID when reloaded through WIC; it remains a custom linear
RGB ICC profile. Generic ICC validation uses Little CMS and is not full WCS conformance.

Paint.NET passes ColorProfiles initialization, then stops in its Color Management
wrapper because the underlying Direct2D effect is not registered.

## Effects inside transform graphs

CreateTransformNodeFromEffect now retains the underlying effect and exposes a
transform node whose input count tracks it. Each creation has a distinct COM
identity. Evaluation resolves graph connections without changing the wrapped
effect's public image inputs. Nested wrappers, chains of supported built-in
effects, property updates, passthrough graphs, missing inputs, and cycles have
native comparisons. Deleting nodes removes remaining graph references, including
cycles.

The focused regression passes 222 checks on both Windows and Wine. The standalone
probe's API results and seven rendered images (112 RGBA channels) match exactly.
General custom transforms with image inputs, unsupported node types, and graph
fan-out remain open; this is not complete custom-graph support.

With the separate, uncommitted Color Management prototype, Paint.NET passes this
API and stops at CLSID_D2D1UnPremultiply during ConvertAlphaEffect initialization.

## Alpha conversion

Premultiply and UnPremultiply render through Shader Model 4 pixel shaders, so
the implementation also works at Direct3D feature level 10. RGB is multiplied
or divided by stored alpha; zero alpha produces zero RGB. Negative values,
HDR values, small alpha, and alpha itself remain unclamped. Shader objects are
created on first use and retained by the effect; pixel data stays on the GPU.

The native and Wine regression tests each pass 1,198 checks. The standalone
probe matches all 512 RGBA values at default feature level and at feature level
10, including chained conversions and premultiplied/ignore bitmap metadata.
Straight-alpha float bitmap creation is rejected by native Direct2D.

With the uncommitted Color Management prototype, Paint.NET passes alpha-effect
creation and reaches a recorded command list inside the color-icon effect graph.
Command-list rasterization is the next rendering blocker; the editor is unopened.

## Dispatcher queues

Windows.System.DispatcherQueue now activates and returns the queue associated
with the calling thread. Controllers create actual current-thread or dedicated
message loops. Callbacks execute serially at their selected priorities; the
first dedicated-thread callback retains its documented ordering. Thread-access
queries, shutdown events, and deferrals retain their objects and drain pending
work before completing the asynchronous shutdown action. ShutdownStarting is
queued at high priority, matching the independently measured Windows order.

The focused coremessaging tests pass 226 checks on both Windows and Wine. The
standalone comparisons match 42 normal-shutdown records and 52 deferral records
exactly, including a callback enqueued from a second thread during shutdown.
Timer creation remains E_NOTIMPL, and ASTA-specific apartment behavior is not
implemented separately from STA. A repeated shutdown request returns the native
E_UNEXPECTED; Windows additionally leaves its first action pending in the tested
repeated-call case, a quirk this implementation does not reproduce.

With the uncommitted rendering prototypes, the unchanged application now passes
display-aware window creation and fails at the spinner animation keyframe API.
This is progress through startup, not a successful editor session.

## Animation keyframes and loops

UIAnimation now resolves keyframes at offsets and after transitions, places
transitions at/between those keyframes, and evaluates a finite or indefinite
loop without allocating repeated transitions. Conclude ends the current loop
iteration; elapsed time tracks the full timeline while sampled values wrap.
Delayed transitions remain Scheduled until their first transition begins.
Keyframe handles are measured numeric indices, including a valid zero handle.

A native reference covers nine timelines and 225 frames, including zero/one/two
iterations, indefinite loops, delayed starts, partial-transition loops, a tail
after a loop, and stretched transitions. All 358 reference records match,
including validation errors. The focused test passes 2,182 checks on both Windows and Wine.
The complete twenty-case suite passes 109,524 checks with one existing todo. Multiple loops in one
storyboard, general overlapping transitions, and remaining UIAnimation methods
are still outside this implementation's verified coverage.

The unchanged application passes busy-spinner setup and now fails while the
zoom slider requests ID2D1GradientStopCollection1. Rendering prototypes remain
uncommitted, and there is still no successful editing session.

## Observed remaining failures

* The Color Management effect (`CLSID_D2D1ColorManagement`) is not implemented.
* General Direct2D transform graphs and remaining animation features remain open.
* Retest Paint.NET's device feature probe after its effect-category initializer
  can finish. The EffectContext1 regression passes independently.
* Missing `dcomp.dll!CreatePresentationFactory`, observed during diagnostics.
* ID2D1GradientStopCollection1 is unavailable on legacy gradient collections;
  painting the zoom slider raises an interface error.
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

## White-level adjustment

The builtin WhiteLevelAdjustment effect now preserves both FLOAT properties,
including negative, infinite, and NaN values, with the native default of 80 nits.
Rendering multiplies RGB by InputWhiteLevel / OutputWhiteLevel and preserves the
stored alpha. The pixel-shader path supports feature level 10 and avoids CPU
readback. The Windows reference probe and focused regression cover metadata,
zero and negative levels, HDR values, and premultiplied/ignored-alpha inputs.

## Legacy gradient collections

Legacy-created collections now expose ID2D1GradientStopCollection1 with the
same COM identity, retained gamma/extend settings, and native stop conversion
and color-space metadata. Linear and radial brushes now use premultiplied,
8-bit color ramps and honor gamma and clamp/wrap/mirror modes. The standard
linear probe matches Windows for gamma 2.2. Gamma-1 quantization and radial
clamp differences remain measured limitations; see VALIDATION.md.

## Shared transform-graph inputs and outputs

Graph connections now belong to destination input ports. Connecting one effect
input or one transform output to a second destination preserves the first
connection. Replacing a port's source switches cleanly between an effect input
and a transform output. Native Premultiply/UnPremultiply branch tests verify
both outputs, reconnection, graph reset, and removal of an unrelated branch.

A separate native behavior remains unresolved: removing an already realized
upstream node leaves its connection usable on Windows. Wine drops that
connection. The regression records this explicitly with two todo checks.

## Custom draw transforms with image inputs

Custom pixel shaders now receive image textures, per-input samplers, and
Direct2D TEXCOORD semantics, including scene-offset sampling. DrawInfo stores
validated input descriptions. The graph evaluator resolves custom transform
inputs and wrapped effect nodes without modifying the wrapped effect's inputs.
Rendering stays on Direct3D and restores the caller's graphics state.

Native coverage currently measures one image input, point and linear sampling,
crop/offset, 96/192 DPI, output precision, and channel-depth combinations.
The renderer has eight input slots, but general multi-input shaders still need
separate native validation. More than one requested mip level remains
unimplemented; MapOutputRectToInputRects requests do not yet drive upstream
realization. Unbounded or expanded input regions and sampler caching remain
work to do. The source-only shader regression still passes.

## Contained rectangle geometry combinations

Rectangle CombineWithGeometry now handles an input whose simplified bounds are
contained by the rectangle. Union returns the rectangle; intersection returns
the contained geometry. Exclusion and XOR add the contained alternate-filled
contours as holes. The implementation emits real geometry into the caller's
sink and preserves the requested input transform and flattening tolerance.

Cases requiring edge intersections and winding-filled subtraction remain
unimplemented. Curve flattening still differs from Windows in three measured
area cases, although the sampled aliased pixels match. Paint.NET advances
from CombineWithGeometry to its next missing Widen call.

## Default-stroke geometry widening

Rectangle widening emits the expanded outer and inset inner contours, including
solid coverage when the stroke consumes the interior. Closed path widening
constructs opposite miter-offset contours and applies the supplied transform
after expansion, matching Windows stroke scaling. Unsupported open contours,
custom stroke styles, collapsed offsets, and complex self-intersections still
return E_NOTIMPL. Ellipse widening is not implemented.

The focused regression compares native bounds, areas, and aliased pixels for
24 rectangle, concave-path, and actual Paint.NET color-control cases. Five known
Wine failures remain: two wide concave strokes and three cases with 1–3 sloping
edge pixels differing from Windows. No application files were modified.

## Path geometry combinations

Path CombineWithGeometry now normalizes each operand's filled triangle mesh
into oriented boundary contours, triangulates the combined boundaries, selects
faces using the requested UNION/INTERSECT/XOR/EXCLUDE operation, and emits the
result boundary. Both operands retain their fill rule; input transforms are
applied before intersection. This handles the union of widened paths used by
Paint.NET's color control. Rectangle geometry retains its separate contained
combination implementation; other geometry overloads are not all implemented.

Native comparisons cover 48 combinations including intersecting rectangles,
concave polygons, ellipses, and a primary path containing a hole. All sampled
pixels match. Ellipse flattening still produces bounds and area differences,
tracked by 20 focused TODO assertions. Arbitrary complex topology and exact
curve conformance remain work in progress.

## Composition controller lifecycle

Wine can now activate CompositorController on a thread with a DispatcherQueue,
return its associated compositor, retain CommitNeeded subscriptions, coalesce
dirty changes, commit empty state, and complete a queued asynchronous commit
barrier. Closing the controller releases subscriptions and rejects later calls
with RO_E_CLOSED. Color brushes store and return their color and mark the state
dirty. Rejected queue callbacks complete pending actions with a closed error.

This is lifecycle infrastructure. Sprite visuals, targets, surface brushes, and
actual swap-chain presentation still return E_NOTIMPL or lack their interfaces.
GetIids advertises only implemented interfaces; native Windows has many more.
The application advances to canvas creation without having presented its canvas.

## Window feedback configuration

user32 exports SetWindowFeedbackSetting and GetWindowFeedbackSetting. Settings
are stored per HWND, support parent-chain lookup and reset, normalize BOOL
values, validate buffers and flags, and disappear with the window. Native tests
cover unset values, size-only queries, invalid windows, all 13 accepted types,
and the FEEDBACK_MAX sentinel, which Windows accepts without changing settings.
This implements configuration storage; it does not add Wine touch-feedback
visual effects. Paint.NET uses it to disable feedback on its canvas.

## Effect feature-level queries

GetMaximumSupportedFeatureLevel queries adapter context-state capabilities once
per Direct2D device, capped at Direct2D 11.1. It preserves native list-order
selection and failure outputs even when the original Direct3D device was created
at a lower feature level. Native tests cover 47 input arrays at levels 10.0 and
11.0, with and without single-threaded creation, including repeated queries.

DXVK 3.1 has a measured backend difference: a query-only CreateDeviceContextState
promotes the device's reported feature level. Native Windows leaves it unchanged.
Four TODO assertions record that behavior separately from Direct2D query results.
Adapters below 11.1 and concurrent cache initialization remain unverified.

## Ellipse and transformed geometry strokes

Ellipse widening now emits a filled stroke outline. Narrow strokes use adaptive
curve samples and tangent normals; wide strokes combine segment rectangles and
rounded joins into a filled boundary. A transformed ellipse applies its stored
transform to the center curve before widening, then applies the caller's world
transform to the result. Reflection, solid stroke styles, and collapsed inner
strokes have native reference coverage. No extra rendering pass is involved.

The sampled circle cases match Windows. Nonuniform ellipses still have measured
flattening and edge-coverage differences, recorded by 51 focused TODO assertions.
Dashed and degenerate ellipses, nested transforms with custom styles, arbitrary
open paths, and full geometry-stroke conformance remain incomplete.

## Offset transforms — September 12

Offset nodes now translate effect coordinates while retaining their input bitmap.
Requested output regions are mapped back into input space before custom shaders
run, including infinite sources and nested offsets. Native comparisons cover
96/144/192 DPI, both unit modes, changing offsets, chained offsets, and Opacity
composition. The focused test passes all 1,153 checks on Wine and Windows.

The application now passes the offset graph and reaches command-list DrawBitmap,
which is the next unsupported replay command. The editor is still unusable.

## Invert effect — September 12

Invert is registered and rendered by the pointwise pixel-shader path. It preserves
alpha, inverts premultiplied color without clipping HDR values, and handles zero
alpha as measured on Windows. Single/double inversion, both supported source
alpha modes, offsets, three display scales, and feature levels 10/11 match the
native probes. The focused regression passes 630 checks on both platforms.

Startup now fails while measuring a brush image containing Gaussian Blur. A
separate main-thread path also exposes missing command-list glyph replay. Bitmap
replay remains a local prototype: bounds match the non-perspective reference
cases, but antialiased edge and filtering differences remain.

## Crop effect — September 12

Crop now renders through cached vertex/pixel shaders, including on feature level
10. It handles normalized rectangles, soft/hard borders, empty intersections,
shifted effect inputs, and native signed-fraction behavior at negative edges.
The Windows-measured default rectangle uses infinities. Invalid border enum
values are rejected without changing the property. Source alpha values remain
unchanged except for soft-edge multiplication, including ignore-alpha inputs.

Windows and Wine each pass 25,812 focused checks. The complete local suite passes
37 cases / 219,195 checks with 96 existing TODO failures and zero ordinary
failures. Native probes using fractional destination offsets also expose an
existing DrawImage placement/sampling difference at 144 DPI; that remains open.

All 312 original application binaries remain unchanged. The latest run is
`paintnet-20260912-081547-491678.log` / `pdncrash.53.log`: command-list geometry
strokes still fail while drawing the color wheel. Rounded-rectangle bounds also
fail in another startup path. The editor is still unusable.

Local layer rendering supports group opacity, nesting, geometric/opacity masks,
background initialization, and COPY blending. Ten focused assertions retain
measured ellipse-edge differences. Explicit layer resources, IGNORE_ALPHA,
and legacy ClearType layer initialization remain unsupported. The newly exported
DCompositionBoostCompositorClock explicitly returns E_NOTIMPL; Paint.NET handles
that failure, but dynamic refresh-rate boosting is not implemented.

## Curved geometry bounds — September 12

Ellipse and rounded-rectangle GetBounds now measure the extrema of transformed
cubic segments. Negative radii use their magnitude; rounded radii are limited
by the normalized rectangle's size. Zero dimensions and singular transforms
are covered. The focused regression passes 2,914 checks on Windows and Wine,
including queries through transformed-geometry wrappers.

With local geometry-stroke replay, Paint.NET reaches a missing Composite effect
in the Layers panel (`paintnet-20260912-082911-499220.log`, `pdncrash.54.log`).
An earlier Flood draw is also unimplemented. Command stroke replay remains a
prototype: 126 of 128 direct/replay pixel pairs match in Wine, but two transformed
ellipse cases differ by one pixel and native stroke-edge differences remain.
The editor is still unusable. The full local suite passes 38 cases / 222,109
checks with 96 existing TODO failures and no ordinary failures.

## Drawing data and font lifetimes — September 12

Growing a command-list buffer now relocates its embedded glyph, image, bitmap,
and opacity-mask payload pointers. Concurrent system-font cache initialization
now releases each call's own collection reference. The focused regressions pass
2,067 and 304 checks respectively on Windows and Wine. The previous font-cache
implementation failed every one of eight concurrent initialization rounds.

WIC now registers eight-bit alpha format metadata, with a native-verified
25-check regression. This is separate from the uncommitted A8 and floating-point
WIC render-target implementation. The complete focused suite passes 43 cases /
247,843 checks with 96 existing TODO failures and no ordinary failures.

The private X display stopped during testing; its window-creation failures were
rechecked after restarting it. The restored-display application run still fails
in LayersStrip with EXCEEDS_MAX_BITMAP_SIZE while trying to allocate an infinite
custom-effect output (pdncrash.62.log). The editor remains unusable.

## WIC render targets — September 12

A8, premultiplied half-float and full-float WIC targets now render through the
real Direct2D path. Format/alpha validation rejects incompatible targets before
allocation. Existing bitmap contents and external edits between drawing batches
are preserved. Native and Wine each pass 2,534 focused checks; the full local
suite passes 44 cases / 250,377 checks with 96 existing TODOs and no ordinary
failures. The implementation currently copies the full bitmap at BeginDraw and
EndDraw, so this path still needs performance work.

The local Gaussian region prototype now lets the editor remain open and removes
the Layers-panel allocation failure. Native probe 142 matches all 80 graph
statuses/bounds and 66 pixel cases; 14 blur-edge cases still differ. This remains
uncommitted rendering work. A brush stroke passes A8 target creation but crashes
when committing the stroke, in overlapping animation scheduling
(pdncrash.63.log). Drawing and saving are still not verified.

## Overlapping animations — September 12

Default animation arbitration now schedules a new track after existing tracks
when the permitted delay allows it, or trims their shared variable at takeover.
Unrelated variables continue animating. Initial value and velocity are sampled
at the actual start, and current/final values follow queued storyboard handoffs.

Native and Wine comparisons match 144 two-storyboard cases and 144 three-storyboard
cases. The focused regressions pass 19,393 and 23,569 checks on both platforms.
The complete local suite passes 46 cases / 293,339 checks with 96 existing TODOs
and no ordinary failures. Custom priority comparison handlers remain unsupported;
callback-order conformance and complex overlapping keyframes need further tests.

Brush interaction now passes the previous animation failure. The next failure
is conversion of floating-point brush sprites to 32-bit pixels: a WIC format
converter reaches an unimplemented path during CopyPixels (pdncrash.65/66.log).
The editor can open, but drawing still crashes and saving remains unverified.

## Floating-point brush bitmap conversion — September 12

WIC now converts straight and premultiplied RGBA float images to BGRA/PBGRA and
RGBA/PRGBA display pixels, converts the four byte formats back to float, and
converts between straight and premultiplied float formats. Conversion preserves
alpha semantics, channel order, cropped rows, and padding. The converter checks
source rectangles and output buffer bounds before writing.

Windows and Wine each pass 209,611 checks in the new regression, including a
524,290-pixel gamma ramp and every byte-channel/alpha combination in both
premultiplied byte layouts. The complete local suite passes 47 cases / 502,950
checks with 96 existing TODOs and no ordinary failures. The broader upstream WIC
converter suite retains its existing 12 format-coverage failures and nine skips.

The real brush test now passes the previously missing conversions, but crashes
in DirectWrite text-layout disposal. This remains an unusable-editor result;
no working draw/save/reopen workflow is claimed.
