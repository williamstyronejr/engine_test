# Recorded gameplay measurements — 2026-10-17

Five of six preplanned runs passed. All six verified every replay command and
finished with matching per-level fingerprints, final hashes, viewport and draw
counts; Annex repeat 2 returned failure because of one reversed GPU timestamp pair.
That attempt is preserved and excluded from the accepted timing table below.

## Method

The [declared workloads and timing contract](REPLAY_PROFILING.md) use normal
Feature Lab minimap/world/UI rendering. Each checked-in recording has 3,000 commands
and completes its level before and after restart. The first 30 frames are warmup,
leaving 2,970 CPU samples per scope per run. Facility and Annex were alternated for
three repeats each, without retries, from committed source `9b6d52c` using GCC
16.2.1 Release (`-O3 -DNDEBUG`). No compilation or automated tests overlapped the
measurements. No screenshots were taken, and no audio device was opened.

The i5-4300U / HD Graphics 4400 used Mesa 26.2.2-arch1.1 and OpenGL 4.6 Core under
XWayland. AC power was connected and `intel_cpufreq` used `schedutil`; clocks,
compositor scheduling and cooling were not locked. Temperature snapshots ranged
from 57°C to 86°C. The ordinary desktop and assistant session remained active.
The [manifest](gameplay/2026-10-17/manifest.json) records exact commands, source,
exit codes, stderr, environment and temperature observations for every attempt.

Every measured viewport was **1050×1360**, with no resize during measurement.
VSync-disable requests were accepted. These are native-window results at the
window-manager-selected size, not evidence for the separate 1920×1080 target.
Frame timing ends at swap completion and excludes subsequent bookkeeping, health
checks and title updates. It is not total process wall time or scanout latency.

## Accepted measurements

| Report | Frame median ms | p95 ms | p99 ms | Max ms | GPU median ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| [facility-1](gameplay/2026-10-17/facility-1.json) | 1.699 | 7.012 | 9.607 | 40.864 | 1.637 |
| [annex-1](gameplay/2026-10-17/annex-1.json) | 1.754 | 7.074 | 9.527 | 22.730 | 1.684 |
| [facility-2](gameplay/2026-10-17/facility-2.json) | 1.738 | 7.036 | 9.606 | 22.539 | 1.645 |
| [facility-3](gameplay/2026-10-17/facility-3.json) | 1.763 | 7.201 | 9.596 | 37.104 | 1.684 |
| [annex-3](gameplay/2026-10-17/annex-3.json) | 1.833 | 7.355 | 9.861 | 43.440 | 1.756 |

Accepted frame p99 values were 9.527–9.861 ms, below the provisional 16.7 ms interval,
but maxima reached 22.539–43.440 ms. A missed-deadline percentage cannot be recovered
from these distributions, and this does not establish uninterrupted 60 FPS. These
runs mix active movement, pause/single-step, camera zoom, restart and completion
screens; they are not a continuous-motion worst case.

| CPU scope | Facility median range ms | Annex median range ms (accepted runs) |
| --- | ---: | ---: |
| `simulation` | 0.072–0.078 | 0.011–0.011 |
| `minimap_submit` | 0.151–0.163 | 0.136–0.148 |
| `world_submit` | 0.203–0.223 | 0.206–0.230 |
| `ui_submit` | 0.309–0.336 | 0.336–0.383 |
| `present` | 0.919–0.949 | 1.009–1.013 |

Presentation had the largest CPU subinterval p99 in every accepted run:
8.887–9.262 ms. Other CPU scope p99 values were at most 1.855 ms. This supports
investigating driver/compositor pacing before treating simulation as the principal
frame-tail bottleneck. It does not identify the cause of an individual slow frame:
paired per-frame samples are not retained, and separate percentiles cannot be added
or subtracted. GPU rendering overlaps CPU submission and presentation.

UI submission includes bitmap text, normal HUD/status panels and minimap composition;
its median was higher than the world or minimap submission median in these runs.
This is an observation for this interface and viewport, not a reason by itself to
change batching or buffer synchronization. Annex reaches its completion state sooner
and performs fewer measured simulation steps, so its smaller simulation median
must not be interpreted as an equivalent-work speedup over Facility.

Facility counted 2,734 measured frames with an increased simulation tick, 2,322,687
window quads, 50,963 window draws, 663,508 minimap quads and 2,970 minimap draws.
Annex counted 1,376 such simulation frames, 2,321,225 window quads, 49,474 window
draws, 445,162 minimap quads and 2,970 minimap draws. These totals matched across
all three repeats of each level, including the rejected Annex run. Final state
hashes were `3859428580428616961` at tick 1381 (Facility) and
`16330525715750492017` at tick 702 (Annex). All replay file fingerprints also match
the checked-in recordings.

## Reproduced GPU timestamp reversal

[Annex repeat 2](gameplay/2026-10-17/annex-2.json) retained this exact record and
returned exit code 1:

```json
{"frame":"1614","start_ns":"68717863232","end_ns":"43536",
 "last_attempted_frame":"1615","slot":5}
```

Frame 1614 is measured command index 1613, after the restart at command index 1500.
Collection occurred after the next frame tag had been attempted, in reusable pair
slot 5. No query was skipped. The run completed all gameplay checks, but its timing
result is rejected. No replacement run was substituted into the table.

The start is 1,613,504 ns below 2^36 and the end is 43,536 ns above zero. A
**hypothetical** 2^36-ns rollover would yield `(2^36 - start) + end = 1,657,040 ns`
(1.657040 ms), compatible with nearby distribution values. Historical upstream
[Crocus query code](https://cgit.freedesktop.org/mesa/mesa/commit/?id=f3630548f1da904ec6c63b43ece7e68afdb8867e)
contains a 36-bit mask on scaled timestamp results, providing a driver-specific
lead. That historical source is not an audit of the installed Mesa 26.2.2 build;
the arithmetic and source match are evidence for a candidate explanation, not
proof of this installed driver's path or a portable correction rule.

The engine accepted 64-bit timestamp support and still rejected `end < start`.
No guessed mask, clamping, invented duration, synchronization wait or automatic
retry was introduced. Verifying the installed driver's timestamp handling and a
portable elapsed-query approach is the next timing investigation.

Across all six runs, 18,000 query pairs were submitted: 17,993 valid pairs collected,
one invalid pair rejected and six left pending. Warmup is included in query totals
but excluded from GPU timing distributions. The rejected run has 2,968 measured
GPU samples; the others each have 2,969. All failure records and process outcomes
remain available in the raw reports.

## Verification and remaining coverage

All 52 runtime CTest groups passed in GCC Debug and Release without skips. The
extended 3,000-command Facility/Annex integration cases also passed in both builds,
checking CPU replay equivalence, sample counts, options rejected without replacing
reports, protection of replay inputs, and absence of user-storage directories.
Four CPU profile cases cover warmup, delayed GPU samples, bounds, resize rejection,
incomplete reports, fingerprints and exact failure values. The profile and replay
file groups passed ASan/UBSan with leak detection enabled.

Physical audio, live-input latency, different displays/drivers, reload spikes and
cross-level transitions need separate workloads. The profile records no allocator
or driver-memory accounting and establishes no allocation-free render guarantee.
F15/F16 remain partial, and no performance threshold is added to CI.
