# HUD text reuse — 2026-10-24 UTC

Increment 58 removes the recurring C++ allocation demand observed in world/HUD
submission for the two declared replay workloads. Three new Release runs per level
have identical allocation counters, unchanged gameplay hashes/ticks, identical
render totals and the same 1050×1360 viewport as the earlier baseline. This is an
allocation result, not a measured speedup or a whole-engine allocation-free claim.

Baseline runtime: `31861ad` (unchanged through prior main `28b74a5`).
New runtime: **`a1fe4f3`**. Both use GNU 16.2.1 Release, `-O3 -DNDEBUG`, sanitizers off,
Intel HD Graphics 4400 / Mesa 26.2.5-arch1.1, Linux 7.2.8-9-omarchy x86_64.
All reports complete, verify 3,000 replay commands, exclude the same 30-frame warmup,
retain a stable viewport and record zero invalid GPU queries.

The [implementation/lifetime contract](HUD_TEXT.md) explains buffer ownership,
growth, immediate view consumption and preserved convenience APIs. Measurement
boundaries and exclusions remain those in [allocation profiling](ALLOCATION_PROFILING.md).
Exact commands, environment and compiler/sanitizer checks are in
[validation](../validation/2026-10-24-hud-text/RESULTS.md).

## Same scopes, reduced demand

Each value describes one 2,970-frame measured run. All three repeats per level
agree exactly. Bytes are cumulative successful requests, not live heap size.

| Scope / metric | Facility before | Facility after | Annex before | Annex after |
| --- | ---: | ---: | ---: | ---: |
| World submission calls | 5,940 | 0 | 5,940 | 0 |
| HUD/UI submission calls | 62,278 | 0 | 63,912 | 0 |
| World + HUD requested bytes | 2,376,162 | 0 | 2,413,670 | 0 |
| Simulation calls | 162 | 162 | 132 | 132 |
| Simulation requested bytes | 105,756 | 105,756 | 101,497 | 101,497 |
| Enclosing frame calls | 68,380 | 162 | 69,984 | 132 |
| Enclosing frame requested bytes | 2,481,918 | 105,756 | 2,515,167 | 101,497 |

Minimap submission and presentation continue to record zero observed C++ allocations.
The frame includes its subscopes, so its row must not be added to the others. World
submission includes constructing replay progress and the alarm-switch label; both
now reuse storage. HUD formatting and the map legend account for the other recurring
reduction. Existing simulation counters match in every field, including the single
allocating sample consistent with the fixture's intentional restart. Aggregate
reports do not retain allocation call stacks or individual sample indices.

Two strings reserve 256 and 128 characters before the frame loop; they persist
through scene changes/restarts and can grow without truncation. Initial reservations
are outside the measured scopes. This is a small increase in retained string capacity
in exchange for avoiding repeated transient allocation, not a claim of lower RSS.
Window-title/report writing and open diagnostics/settings retain their exclusions.

Facility still ends at hash `3859428580428616961`, tick 1,381; Annex at
`16330525715750492017`, tick 702. Window/minimap quad counts, draw counts and simulation
step totals match the baseline in every repeat.
[Machine-readable comparison](../validation/2026-10-24-hud-text/allocation-comparison.json)
retains both sets of counters.

Raw after reports: Facility
[1](../validation/2026-10-24-hud-text/facility-1.json),
[2](../validation/2026-10-24-hud-text/facility-2.json),
[3](../validation/2026-10-24-hud-text/facility-3.json);
Annex [1](../validation/2026-10-24-hud-text/annex-1.json),
[2](../validation/2026-10-24-hud-text/annex-2.json),
[3](../validation/2026-10-24-hud-text/annex-3.json).
Before reports remain in the [original record](2026-10-24-allocation-results.md).

## Rendered output and audio checks

Seventeen old/new PPM pairs are byte-identical at the recorded viewport. For each
level, replay frames 90, 121, 151, 1501 and 3000 cover gameplay, pause, single-step,
restart and completion. Scripted gameplay at frame 90, scripted completion at 1450
and recording at 90 add six pairs with ordinary control/status hints. A Facility
frame with deliberate shader/texture failures checks retained-resource labels.
[Comparison evidence](../validation/2026-10-24-hud-text/screenshot-comparison.json)
records commands, sizes, dimensions, hashes and equality; full PPMs remain under
ignored `build/hud-validation/`. This is scoped visual equivalence, not a physical
monitor/DPI acceptance test.

The audio workload is unchanged. Three repeats still record zero C++ allocations
in its producer, command/mix and transport scopes. Each producer has 3,000 samples;
worker samples vary with scheduling (22,163 / 30,255 / 28,940), as expected for
unpaced ALSA null. The native audio tests separately check changed mute/gain and
international binding labels through the new caller-buffer overload.
Raw audio reports: [1](../validation/2026-10-24-hud-text/audio-1.json),
[2](../validation/2026-10-24-hud-text/audio-2.json),
[3](../validation/2026-10-24-hud-text/audio-3.json).

## Limits and next increment

No timing improvement is claimed: scheduling, thermals and desktop activity were
uncontrolled, and instrumentation perturbs execution. No physical audio, C-library
allocator, GPU/driver memory, peak-live-heap or total-process bound follows from
these counters. Owning-string/convenience APIs can still allocate, and the long-
label test explicitly permits growth before subsequent reuse.

Next: extend allocation coverage to open settings/diagnostics and the enabled-audio
HUD, including remapped controls. Retain startup, restart, error and reporting
exceptions and finish the declared workload/memory and physical acceptance records
before broad release claims.
