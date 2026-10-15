# Platform lifecycle validation

The supported backend is X11/GLX on Linux, including XWayland, with OpenGL 4.6
Core. Window-manager requests are asynchronous. A successful send is not proof
that the desktop accepted a request. This matrix separates engine behavior,
observed desktop behavior, and checks requiring physical display inspection.

## Repeatable checks

```sh
DISPLAY=:0 ctest --test-dir build/release --output-on-failure -R '^platform_lifecycle$'
DISPLAY=:0 ./build/release/platform_probe --exercise --screenshot build/platform.ppm
```

Use your session's `DISPLAY` value. `platform_probe` is a development executable,
not part of the installed game. Without `--exercise` it prints help. It opens its
own managed window and an owned focus-transfer window, and changes no desktop
settings. Exit 0 means all requested operations were observed; 2 means at least
one window-manager request was unconfirmed; 1 means an engine check failed; 77
means `DISPLAY` is absent. A configured but inaccessible display is an error.

The probe requests fullscreen entry/exit, transfers focus between its own windows,
requests minimize/restore, independently unmaps/restores its window, and compares
observed X11 dimensions with GLX drawable pixels. It reports VSync request support,
not measured presentation timing. It checks GL errors and tracked resource counts.
An unconfirmed operation is not silently replaced with a different operation.

The six-case `platform_lifecycle` group checks bounded fullscreen-property parsing,
explicit flushed request messages, real versus temporary-grab focus transitions,
input cancellation, four actual unmap/restore cycles with retained texture pixels,
resize/camera/UI/pointer agreement, and close-message validation. The resize matrix
covers 320x240, 640x480, 1280x720, 1920x1080, 900x1200 and 320x200. The exact-size
fixture stays unmapped to avoid asynchronous desktop sizing.
It orders X11 resizing before GL with `glXWaitX` and explicitly swaps its hidden
test drawable to retire the old back buffer; public `Window::present()` correctly
skips unmapped windows. The test requires observed X11/GLX dimensions to converge
within two seconds and synchronizes queued events before the controlled click.
Mapped unmap/restore remains covered independently, including the visible probe.
This is viewport-pixel testing, not a physical DPI or compositor-scale test.

## Observed desktop results — 2026-10-14

Machine: Intel HD Graphics 4400, Mesa 26.2.2, OpenGL 4.6 Core, existing XWayland
session on `:0`. No display settings or keyboard layouts were changed.

| Operation | Result | Evidence and limits |
| --- | --- | --- |
| Startup/context/capabilities/close | Passed | Existing graphics/no-display checks plus validated own-window close messages |
| Fullscreen entry/exit | Confirmed | Managed probe observed 2160x1440 fullscreen, then 1050x1360 tiled window and matching state changes |
| Visible focus transfer | Confirmed | Real X server focus loss canceled held action and pointer without a click; focus returned |
| Temporary X11 grabs | Passed in automated test | Grab/ungrab events preserve logical focus and input; not a physical desktop shortcut matrix |
| Window-manager minimize | Unconfirmed | ICCCM request submitted; no unmap observed within two seconds; probe exits 2 |
| Client unmap/restore | Confirmed | Actual unmap observed, restore drawable, context/resources retained; separate from WM minimize |
| Gameplay suspension | Confirmed | Native 240-frame play run suspended/resumed at the same tick across a 0.5-second unmap |
| Replay suspension | Confirmed | Native 1500-command replay retained its suspension tick and final reference state |
| Resize and pointer alignment | Passed | Six-size automated matrix; managed resize respected observed WM-selected dimensions |
| VSync control | Requests available | Both interval requests supported; timing/tearing not physically measured |
| Physical display scaling | Outstanding | No monitor-scale changes, fractional-scale or cross-monitor transfer checks performed |

## Remaining manual matrix

On a desktop that supports minimizing X11 clients, run the probe and require
`wm_minimize=confirmed` and `wm_restore=confirmed`. Also exercise title, settings,
and live gameplay with the desktop minimize/restore controls. Confirm held movement
and pointer drags do not survive restoration and no hidden simulation advances.

At each supported monitor scale (including fractional scale if offered), inspect
text, sprite edges and UI targets in title/settings/gameplay; verify clicks align
at corners and center, fullscreen round trips restore the expected layout, and
moving between differently scaled monitors preserves alignment. Record compositor,
monitor mode, configured scale, observed window pixel dimensions and capture.
The engine currently uses X11 pixel coordinates for both pointer and drawable;
there is no physical-DPI policy or native Wayland backend.

These outstanding checks keep F01 partial. The probe's unmap result must not be
reported as successful WM minimization, and viewport resizing must not be reported
as physical scaling validation.

## Protocol references

Fullscreen requests and observed state follow the
[EWMH window-manager specification](https://specifications.freedesktop.org/wm/latest-single/).
Map/unmap, focus-grab modes and iconification use the installed Xlib interfaces
specified in the [Xlib programming manual](https://xorg.freedesktop.org/releases/current/doc/libX11/libX11/libX11.html).


The thirty-second increment isolates the size fixture after a full-suite run
observed a mapped override-redirect window revert from a requested 640x480 to
320x240. It tests the same size, pixel and pointer assertions without relying on
the desktop retaining that exact mapped size. Native window behavior is unchanged.
