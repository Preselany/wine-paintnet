# Paint.NET Classic on Wine

Run the original Windows desktop release of Paint.NET on Ubuntu by improving
Wine's Windows API implementations, particularly Direct2D.

This is a Wine source fork. Paint.NET remains proprietary and is downloaded
separately from its official release repository. Its executable and DLLs are
not modified or redistributed here.

## Target

* Paint.NET **5.1.12**, the stable desktop release as of September 11, 2026.
* Official x64 portable ZIP. This is the normal application packaged without
  an installer, not the experimental Wine-specific application.
* Wine **11.17**, source commit `36b6a2cf679fb395f668a917b76537190e212d9c`.
* Ubuntu 24.04 x86-64, X11/XWayland; DXVK 3.1 supplies Direct3D 11.

Rendering path: **original Paint.NET → Wine Direct2D → DXVK Direct3D → Vulkan**.
The launcher does not enable Paint.NET's managed Direct2D replacement or
disable composition/animation to claim compatibility.

## Success criteria

1. The verified official binaries start through this Wine fork.
2. Images and the canvas render correctly through Wine's `d2d1.dll`.
3. Drawing, text, selections, layers, undo/redo, and representative effects work.
4. PNG/JPEG and layered PDN files survive save/reopen validation.
5. Fixes have focused tests and documented limitations, with patches suitable
   for independent review and eventual upstream submission.

Startup, editing, effects, file I/O, and desktop integration are separate
milestones. None should be inferred from another. See [STATUS.md](STATUS.md).

## Native Linux file dialogs

The launcher enables an opt-in Wine `IFileDialog` bridge to a GTK 3 chooser.
Paint.NET itself continues to select the codec and read/write the files.
The helper needs `/usr/bin/python3`, PyGObject and GTK 3 (Ubuntu packages
`python3-gi` and `gir1.2-gtk-3.0`) on the host. It uses X11/XWayland and local
filesystem paths. An X window manager is required for normal focus/activation
behavior, including on a private Xvfb test display.

For an existing work directory, close the development editor and update both
halves of the Wine module:

```sh
./paintnet/build.sh /absolute/path/to/work dlls/comdlg32/all dlls/comdlg32/tests/all
./paintnet/install-modules.sh /absolute/path/to/work comdlg32
DISPLAY=:93 ./paintnet/test-native-dialog.sh /absolute/path/to/work
```

`WINE_NATIVE_FILE_DIALOG=` before `run.sh` selects Wine's existing dialog UI.
An absolute helper path overrides the included helper. Other Wine applications
are unchanged unless this variable is enabled for their process. Custom-control,
non-storage, create-prompt and share-aware dialogs currently use Wine's existing
UI. Legacy `GetOpenFileName`/`GetSaveFileName` dialogs are not bridged.

This is an initial integration: folder/selection and changed-type notifications
are delivered on acceptance, rather than continuously during GTK browsing.
Application vetoes reopen the chooser with the preserved selection. The native
confirmation precedes `OnOverwrite`; full live callback ordering, custom controls,
Wayland portals and other desktop chooser backends remain integration work.
The deterministic bridge tests exercise the transport and COM lifecycle; they
are not native Windows conformance tests or substitutes for testing the GTK UI.

## Development

Scripts use a separate work directory supplied as their first argument.
Build dependencies are provided by the Dockerfile. Runtime downloads are
pinned by SHA-256; the application manifest is checked before each launch.

```sh
./paintnet/setup.sh /absolute/path/to/work
./paintnet/build.sh /absolute/path/to/work
./paintnet/install-modules.sh /absolute/path/to/work d2d1 uianimation wined3d coremessaging dcomp user32 comdlg32
DISPLAY=:93 ./paintnet/test.sh /absolute/path/to/work
DISPLAY=:93 ./paintnet/test-upload.sh /absolute/path/to/work
DISPLAY=:93 ./paintnet/run.sh /absolute/path/to/work /absolute/path/to/image.png
```

Start an isolated X server/window manager before automated UI tests. For
headless Xvfb tests with NVIDIA hardware, `DXVK_FILTER_DEVICE_NAME=llvmpipe`
can isolate Wine API failures from known virtual-display presentation issues.
Real GPU validation remains a separate required milestone.

Setup needs `curl`, `python3`, `dpkg-deb`, `tar`, `sha256sum`, a display,
and installed Vulkan drivers. Building needs Docker. The downloaded runtime
is WineHQ's Ubuntu 24.04 package; other distributions are not yet validated.
`PDN_CACHE_DIR` selects a reusable archive cache, and `BUILD_JOBS` defaults to 4.
`install-modules.sh` waits for the development prefix to exit before changing
its DLLs; close the development application normally before using it.

The default build compiles `d2d1.dll`, `uianimation.dll`, `wined3d.dll`,
`coremessaging.dll`, `dcomp.dll`, `user32.dll`, and their focused tests. The shader-reflection change lives in Wine's shared `wined3d.dll`;
DXVK still provides the application's Direct3D rendering. Additional Wine make targets
can be supplied after the work directory as development reaches other APIs.

`test.sh` runs the COM, effect-context, Histogram, Opacity Metadata, Alpha Mask,
Convolve Matrix, Contrast, Bitmap Source, Emboss, Opacity, custom source draw shaders, command-list recording, color-profile resources, UIAnimation, and shader-requirement
regressions by default. A test name can be supplied
after the work directory. `test-upload.sh` checks actual lookup-table texels by
reading the fork's Direct3D texture back. It uses Wine's private structure
layout, verifies that the installed module matches the build, and must only
run against this checkout's module. It is not a native Windows conformance
test or a test of the separate LookupTable3D image effect.

For an independent Windows reference, the focused test executable accepts
`D2D1_TEST_WARP=1` to use Microsoft's software renderer. Leave it unset for the
normal Wine/DXVK tests. `build-reference.sh` builds standalone Emboss, property, animation, shader-reflection, custom draw-transform, command-list, and color-profile
probes under the work directory's `reference/` folder. Run them on Windows
with its system Direct2D; they write JSON or labeled text records containing actual
pixels, bounds, properties, and HRESULTs. See [REFERENCE.md](REFERENCE.md) for the measurement
scope and current reference-host status.

Official application: https://github.com/paintdotnet/release/releases/tag/v5.1.12

Wine upstream: https://github.com/wine-mirror/wine

Fork: https://github.com/Preselany/wine-paintnet/tree/paintnet-classic
