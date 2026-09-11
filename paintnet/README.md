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

## Development

Scripts use a separate work directory supplied as their first argument.
Build dependencies are provided by the Dockerfile. Runtime downloads are
pinned by SHA-256; the application manifest is checked before each launch.

```sh
./paintnet/setup.sh /absolute/path/to/work
./paintnet/build.sh /absolute/path/to/work
./paintnet/install-modules.sh /absolute/path/to/work d2d1
DISPLAY=:93 ./paintnet/test.sh /absolute/path/to/work
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

The initial build compiles `d2d1.dll` and its tests. Additional Wine make targets
can be supplied after the work directory as development reaches other APIs.

Official application: https://github.com/paintdotnet/release/releases/tag/v5.1.12

Wine upstream: https://github.com/wine-mirror/wine

Fork: https://github.com/Preselany/wine-paintnet/tree/paintnet-classic
