# Paint.NET Classic Wine project

The user's objective is to run the normal, unmodified Windows release of
Paint.NET on Ubuntu by implementing compatibility in Wine itself.

* Target the official stable desktop release, initially Paint.NET 5.1.12 x64.
* Keep the downloaded Paint.NET executable and DLLs byte-for-byte unchanged.
* Use Wine's builtin `d2d1.dll`. Do not substitute Paint.NET's managed Direct2D
  renderer, use its experimental Wine build, or launch with `/wine` or
  `/useManagedD2D` as a way of claiming success.
* DXVK may implement Direct3D underneath Wine's Direct2D implementation.
* A success-returning stub is not an implemented feature. Validate observable
  behavior, including rendered pixels, where a graphics API is implemented.
* Keep reproducible before/after traces and focused regression tests. Record
  remaining failures honestly; opening a window is not full compatibility.
* Use an isolated Wine prefix and test display. Do not automate the user's
  desktop or interrupt their existing Paint.NET installation.
* Put project scripts, status, and documentation in `paintnet/`. Keep upstream
  Wine code in its original layout and preserve upstream licensing.
* Generated binaries, downloads, prefixes, logs, and screenshots belong in a
  separate work directory and must not be committed.

The earlier Paint.NET experimental integration is a separate project. It may
provide useful Wine codec fixes, but is not evidence of this target working.
