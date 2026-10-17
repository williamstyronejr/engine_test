# Reload/transition profiling results — 2026-10-17

Increment 41 adds [the scripted spike workload](SPIKE_PROFILING.md) through normal
Feature Lab minimap/world/UI rendering. It measures reload calls and level changes
while retaining their exact state/resource checks and nearby frame timings.
Recorded ERPL profiling retains its previous content-isolation rules.

## Collection protocol

The plan was three successful `reload_transition_v1` attempts followed by one
unexpected shader-failure example. Runs were sequential, after compilation and
runtime testing, without retries or timing acceptance thresholds. All use source
`357abea`, GCC 16.2.1 Release (`-O3 -DNDEBUG`), i5-4300U / HD Graphics 4400,
Mesa 26.2.2-arch1.1, and the existing XWayland desktop. VSync-disable acceptance is
reported; compositor/display pacing is not independently controlled. Device audio
and screenshots are disabled. Shader/OS caches were not cleared, CPU clocks and
thermal behavior were not locked, and these are not cold-loading measurements.

The workload uses seed 1, 3,600 frames and 30 warmup frames. Seven fixed reload
operations occur on frames 120–840. A deliberately rejected transition is followed
by Facility → Annex → Facility through the existing `SceneFlow` path. The tests'
independent CPU reference determines expected event frames, before/after hashes,
revisions and final state. The native run also checks resource stability every
measured frame. This covers unchanged-content reloads and staged level replacement;
it does not measure filesystem-watch delivery or real authoring edits.

[The manifest](spikes/2026-10-17/manifest.json) retains command arrays, exit codes,
stdout/stderr, wall durations and package temperatures for every attempt. The
unexpected-failure case uses a private asset copy with an invalid fragment shader
from startup; the shipped source assets are untouched.

## Outcomes

All three normal attempts exited zero, completed 3,600 frames and verified all ten
operations. Their final hash is `3859428580428616961`, with 1,381 ticks in the final
Facility. Event frames/hashes/revisions match the CPU reference. The measured
viewport stayed 1050×1360. Each run reports the same 3,434 measured simulation steps,
2,683,158 window quads / 60,285 draws, and 746,477 minimap quads / 3,570 draws.

Each run submitted 3,600 elapsed GPU queries and collected 3,599, with one pending
and zero skips. There are 3,569 GPU samples after warmup. All 50 per-event neighborhood
entries have GPU values; consecutive transition events overlap, yielding 46 distinct
retained frames per run. The shader/texture/font revisions and live inventory are
stable where required. The inventory is nine textures (30,292 bytes), one target
(103,968 bytes), and a separate 16,384-byte font atlas. These are live payloads,
not transient or driver-memory peaks. Package temperatures at run boundaries were
66–67 °C; temperatures within each run were not sampled.

| Raw attempt | Frame median ms | Frame p99 ms | Frame max ms | GPU median ms |
| --- | ---: | ---: | ---: | ---: |
| [Run 1](spikes/2026-10-17/run-1.json) | 2.631 | 12.181 | 51.112 | 2.475 |
| [Run 2](spikes/2026-10-17/run-2.json) | 2.594 | 13.500 | 32.756 | 2.428 |
| [Run 3](spikes/2026-10-17/run-3.json) | 2.549 | 13.939 | 32.036 | 2.393 |

## Event timings

Ranges below are the observed minimum–maximum across three attempts, not confidence
bounds or acceptance limits. Operation latency includes the normal call's logging
and staging. Total event-frame time also includes simulation/verification, rendering,
scheduling and presentation. These scopes overlap; do not add them.

| Frame / operation | Operation ms | Total event-frame ms |
| --- | ---: | ---: |
| 120 / `shader_reload` | 1.451–3.342 | 2.571–6.655 |
| 240 / `shader_reject` | 0.662–2.661 | 3.212–16.714 |
| 360 / `shader_recover` | 2.037–2.775 | 4.031–25.313 |
| 480 / `texture_reload` | 1.647–2.964 | 3.974–10.299 |
| 600 / `texture_reject` | 0.647–0.817 | 2.334–9.858 |
| 720 / `texture_recover` | 0.888–2.352 | 4.452–9.983 |
| 840 / `font_reload` | 0.705–1.041 | 3.168–9.478 |
| 1382 / `transition_reject` | 1.239–1.904 | 3.445–9.915 |
| 1383 / `transition` | 1.229–1.703 | 3.033–9.870 |
| 2086 / `transition` | 1.253–1.518 | 3.078–9.937 |

Run 1's shader recovery call at frame 360 took 2.036562 ms; the full frame took
25.312759 ms, including 22.047400 ms in presentation. Frame 359 already spent
17.944567 ms in presentation. The following frame 361 reached 51.111652 ms, with
45.418758 ms in presentation and a 2.854240-ms GPU render interval. The retained
neighborhood exposes that longer tail. These observations locate elapsed time in
API scopes; they do not isolate compositor, driver, scheduling or deferred work as
the cause. The other two attempts did not repeat that same neighborhood spike.

Global frame maxima of 32.036–51.112 ms do not demonstrate uninterrupted 60 FPS.
This managed viewport is also not the separate 1080p target. Comparing these numbers
to earlier replay workloads would mix command sequences, UI state and environment.
The small sample count and uncontrolled caches/clocks preclude a broad reload-latency
or performance-improvement claim.

## Retained unexpected failure

The [private-copy shader failure](spikes/2026-10-17/unexpected-shader-failure.json)
returned the expected exit code 1. Startup retained the built-in shader, then the
scheduled frame-120 reload failed again. The report is incomplete at 120 frames
(90 measured), with one `applied=false, verified=false` event and the compiler
diagnostic. Its before/after gameplay hash stays `8285736648537457467`, shader
revision stays 1, and tracked resource counts are unchanged. The stopped frame's
GPU result is null because shutdown did not wait; the neighborhood ends at frame
120. This example is excluded from the three successful timing attempts above.

## Validation and remaining work

All 54 runtime CTest groups pass in GCC Debug and Release, without hardware skips.
The spike/replay profile CPU suites pass ASan/UBSan with leak detection enabled.
CPU cases cover the fixed schedule, completion, delayed GPU-to-frame correlation,
exact large hashes and escaped diagnostics, partial reports, state/resource retention,
bounds, incompatible options and the warmup resource baseline. Native tests compare
the independent route/transition reference, retain an unexpected failure, protect
existing reports on rejected options, and check that profiling creates no user storage.

The workload measures unchanged-content manual reload paths and staged level changes.
It leaves actual editor/watch timing, cold I/O, transient owned-memory peaks, driver
allocations and broader physical audio/display validation open. The next increment
will extend owned-memory accounting for staging so live-resource stability can be
supplemented with peak-memory evidence.
