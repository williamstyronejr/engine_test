# Transition parser staging results — 2026-10-19

Increment 43 extends [staging coverage](SPIKE_PROFILING.md#staging-memory-coverage)
from completed level candidates into loading, decoding, scene parsing and selected
construction steps. Three complete runs produce identical `owned_payload_v2` peaks.
Two malformed-content runs preserve the active Facility and release all tracked staging.
The largest covered transition peak is **8,804,022 bytes (8.40 MiB)**.

## Collection and validation

All five sequential attempts use clean source `c50b489`, GNU 16.2.1 Release
(`-O3 -DNDEBUG`), the i5-4300U / HD Graphics 4400, and the existing XWayland desktop.
Reported system: `Linux 7.2.8-9-omarchy x86_64`; graphics: `4.6 (Core Profile) Mesa 26.2.5-arch1.1`.
Compilation and tests finished before collection. There were no retries or timing
acceptance thresholds. CPU clocks, shader/OS caches and compositor scheduling were
uncontrolled; package temperatures at run boundaries ranged from 76 to 89 °C.
The [manifest](parser-staging/2026-10-19/manifest.json) retains command arrays,
exit codes, durations, temperatures and stdout/stderr for every attempt.

All 54 runtime groups passed in Debug and Release without skips. The focused CPU
suite passed ASan/UBSan with leak detection. Seven malformed-input cases cover
truncated/trailing tilemaps, trailing animation data, late scene syntax errors,
unterminated quoted tokens, missing gameplay roles and unknown parent IDs.
Each preserves the live hash and entity handle, then succeeds after private files
are repaired. Decoder tests check both exact covered vector payloads and cleanup
after final validation fails. Native tests retain the two parser-failure reports.

Each complete attempt renders 3,600 frames at 1050×1360, excluding 30 warmup frames
from distributions. Event frames, hashes, levels and revisions match the independent
CPU reference; final hash is `3859428580428616961`, with 1,381 final-level ticks.
Each collects 3,599 elapsed GPU queries, leaves one pending and reports no skips or
invalid samples. The workload disables device audio and requests VSync off.

## Covered peaks

The following byte counts are identical across all three complete runs. CPU and GPU
peaks can occur at different times; combined peak measures their actual overlap.
Every event has zero CPU and GPU staging bytes remaining when the operation returns.

| Frame / operation | CPU peak | GPU texel peak | Combined peak |
| --- | ---: | ---: | ---: |
| 120 / `shader_reload` | 809 | 0 | 809 |
| 240 / `shader_reject` | 0 | 0 | 0 |
| 360 / `shader_recover` | 809 | 0 | 809 |
| 480 / `texture_reload` | 49,360 | 26,132 | 52,456 |
| 600 / `texture_reject` | 49,360 | 0 | 49,360 |
| 720 / `texture_recover` | 49,360 | 26,132 | 52,456 |
| 840 / `font_reload` | 35,624 | 16,384 | 51,968 |
| 1382 / `transition_reject` | 8,795,855 | 0 | 8,795,855 |
| 1383 / `transition` | 8,795,855 | 0 | 8,795,855 |
| 2086 / `transition` | 8,804,022 | 0 | 8,804,022 |

Transition construction now captures the temporary scene parsed alongside the
game’s initially reserved world and collision buffers. These values use broader
coverage than [the v1 measurements](2026-10-19-staging-results.md); the roughly
0.85 MiB increase in reported transition peak is not a total-memory regression
comparison. Loading also now releases encoded files after each decode in an
explicit scene → tilemap → animation → construction sequence. Shader reload peaks
include the file-read/text-copy overlap that v1 omitted.

Coverage still excludes nested string character storage, validation hash nodes,
allocator internals/reallocation overlap, failed allocations, and opaque shader or
driver memory. Member-initialization and optional copy/move intermediates are not
exhaustively tracked. These remain selected logical payload peaks, not RSS or a
complete per-operation memory bound.

## Malformed-content evidence

Both private-copy faults affect Annex only, allowing Facility startup and the
route to finish. The frame-1382 rejection attempt fails before preflight; the
required frame-1383 transition fails again and stops the run. Each exits 1 with
`run.complete=false`, nine events and an unverified final event. Both attempts
preserve every before/after state and live-resource field and leave staging clean.

| Private-copy fault | Failure point / retained diagnostic | CPU peak bytes |
| --- | --- | ---: |
| [scene](parser-staging/2026-10-19/malformed-scene.json) | Late scene parse / `Data after end` | 8,795,866 |
| [tile](parser-staging/2026-10-19/malformed-tile.json) | Tilemap final validation / `Trailing data` | 12,789 |

The scene failure occurs after game member buffers have been constructed; the
binary failure occurs earlier. The scene peak is eleven bytes above the matching
valid transition because the injected `unexpected` line adds eleven source bytes.
Both failures have zero GPU staging. Their final hash matches a completed Facility
checkpoint, which also occurs at the end of a normal run: **hash alone does not
establish workload completion**. Inspect completion, frame count and event outcomes.
Shipped assets were untouched.

## Timing context

Timings are retained without an improvement claim. Coverage and loading order have
changed, the reported kernel differs from the earlier v1 run, and scheduling/caches
were uncontrolled. This is not a controlled before/after performance comparison.

| Raw complete attempt | Frame median ms | Frame p99 ms | Frame max ms |
| --- | ---: | ---: | ---: |
| [Run 1](parser-staging/2026-10-19/run-1.json) | 1.461 | 8.830 | 22.729 |
| [Run 2](parser-staging/2026-10-19/run-2.json) | 1.574 | 9.318 | 40.315 |
| [Run 3](parser-staging/2026-10-19/run-3.json) | 1.575 | 9.834 | 42.054 |

Next: reduce redundant scene-buffer reservations and repeat transition/rollback
measurements. Broader hardware/driver checks remain outstanding; asset tooling and
background loading stay deferred.
