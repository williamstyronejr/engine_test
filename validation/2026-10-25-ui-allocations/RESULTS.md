# UI allocation coverage validation — 2026-10-25 UTC

Tested runtime: **`4e860f5a7136d96b7f91e43d058b37a79d65937b`**.
Increment 59 adds a development-only probe for settings/controls, diagnostics and
an enabled-audio HUD. It shares the unchanged gameplay drawing body with the
normal executable. No text optimization or engine API/content-format change is
included in this increment.

[Commands](commands.json) retains 16 final configure/build/test/measurement and
screenshot invocations, with exact arguments, environment overrides, UTC timestamps,
durations and exit codes. All succeed. Existing build directories were reused with
explicit compiler/configuration/sanitizer flags and two compiler jobs; this is not
a fresh or offline-build claim. [Environment](environment.log) records the host.
Runtime configurations identify the clean implementation commit; documentation and
evidence were added afterward.

## Runtime checks

| Configuration | Passed groups | Scope |
| --- | ---: | --- |
| GNU 16.2.1 Debug | 63/63 | All native runtime groups, no skips; 127.97 s |
| GNU 16.2.1 Release | 63/63 | All native runtime groups, no skips; 116.75 s |
| Clang 23.1.3 Debug | 7/7 | UI/gameplay allocations, render targets, UI, diagnostics, HUD text, counters |
| GNU native ASan/UBSan | 3/3 | UI/gameplay allocation workloads and render targets; driver leak detection disabled |

Four named CTest XML reports retain successful-test output. All selected tests pass
without skips. ASan/UBSan halt on error; these graphical checks use
`detect_leaks=0`, so no leak-check claim follows. No suppressions were added. These
focused checks do not replace a full native sanitizer matrix or the earlier scoped
leak/race records.

The current inventory is 63 native runtime groups: 36 CPU, 23 hardware and four
platform, plus three Release packaging checks. CPU-only discovery stays at 34.
The new `ui_allocations` group validates 18 named scenarios, actual menu/capture
states, enabled/remapped labels, 270 samples per scope, positive menu-layout
allocation accounting and equality of inclusive-frame/subscope totals. The probe
itself checks nonempty/stable rendering and unchanged frozen gameplay hashes/ticks.
Its UI transitions go through ordinary pointer/key input rather than private-state
changes. Exact allocation amounts remain measurements, not hardcoded test budgets.

## Measurements and rendering evidence

Three Release reports match in every field, including all 18 scenarios' counters,
labels, gameplay state and render counts. Settings/controls and enabled-audio HUD
redraws record zero render-thread C++ allocations in the declared fixtures.
Diagnostics records 16 calls and 537 requested bytes per measured frame, with or
without shapes. Separate menu-opening, capture and rebind counters retain one-off
costs before warmup. See [measurement results](../../benchmarks/2026-10-25-ui-allocation-results.md),
[scope contract](../../benchmarks/UI_ALLOCATIONS.md) and
[comparison](comparison.json). Raw reports are [1](ui-1.json), [2](ui-2.json),
[3](ui-3.json), alongside their execution logs.

The normal game and instrumented probe share `game_draw.hpp`. The normalized
rendering body and color constants match previous main `5609e32`;
[provenance](draw-provenance.json) retains source identities, body/binary hashes and
baseline build metadata. [Production screenshot comparisons](production-screens.json)
record eight byte-identical PPM pairs across both levels' gameplay, pause,
completion and open settings. Exact commands, dimensions and hashes plus all 16
execution logs are retained. Controls and diagnostics/shapes captures were also
visually inspected. Eighteen new fixed-state captures are also hashed in
`comparison.json`; raw PPMs stay under ignored `build/ui-validation/`.

[Symbol inspection](symbols.json) confirms that production `feature_lab` has no
allocation-profile symbols and resolves ordinary allocation externally; the new
probe defines the replacements. It is not an install target. This report measures
only render-thread allocations; the live ALSA null worker is outside its snapshots
and is joined before serialization.

No physical device/display, live frame-time distribution, changing GPU history,
whole-process memory or performance acceptance is implied. The offscreen frame
excludes event polling, swap, live simulation/audio updates, startup and reporting.
GPU diagnostics use a fixed empty/default history; combined overlays and changing
GPU labels remain additional coverage work. Requested bytes are cumulative demand,
not peak/live heap or RSS.
