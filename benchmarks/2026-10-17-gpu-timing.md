# GPU timestamp investigation — 2026-10-17

Follow-up: a [later gameplay profile](2026-10-17-replay-results.md#reproduced-gpu-timestamp-reversal)
on the same day reproduced a reversal and retained raw values. The sessions below
preceded that observation.

The [October 16 sprite-only run](baselines/2026-10-16/sprites-8192.json) reported
one reversed GPU timestamp pair and correctly exited with code 1. That report
retained only the invalid count, so its raw values and frame cannot be recovered.
This increment closes that evidence gap. The original cause remains unresolved;
the native sessions below did not reproduce it.

## Evidence retained

`GpuTimer::failures()` retains the first eight pairs for which `end_ns < start_ns`
over a timer's lifetime. Each record stores the original frame tag, both exact
64-bit timestamps, the reusable query-pair slot, and the latest attempted frame
tag at collection (including skipped begins). Slot reuse never overwrites a
retained failure. Subsequent failures increment the total and omitted count.

Capture uses a fixed array with no additional GL reads, allocation, formatting or
I/O during collection. Query availability checks, ordering, skip behavior and
nonblocking teardown remain unchanged. Invalid durations are still excluded from
samples. Stress mode still returns failure if any invalid pair occurs, including
during warmup, and writes the retained records into its report. Normal gameplay
logs retained records at shutdown while preserving its existing rendering behavior.

The additive schema-v1 fields under `gpu_queries` are `failure_capacity`, `failures`
and `failures_omitted`. Frame tags and raw timestamps are decimal strings to preserve
values beyond JSON consumers' exact floating-point integer range. A typical record
has `frame`, `start_ns`, `end_ns`, `last_attempted_frame` and numeric `slot` fields.
See the [API contract](../docs/FORMATS.md#asynchronous-gpu-timing) and
[report definitions](README.md#report-schema-and-timing-scopes) for lifetimes,
counting and warmup/cycle mapping. Missing fields in older reports do not mean an
empty failure history. Crashes or earlier fatal errors can prevent a final report.

## Tests

Two new deterministic queue cases exercise repeated reuse with valid/reversed pairs,
full-pool skips, delayed start availability, preservation of the original tag,
first-eight retention, overflow accounting, empty polls and fresh-timer isolation.
They check that collection still performs only the original two availability
checks and two result reads per ready pair. Exact timestamps near `UINT64_MAX` and
above 2^53 survive capture. The report test checks all eight serialized records
and the omitted count; the CPU command's JSON must contain an empty failure array.

The new native test fills all eight slots, releases the measured render target
before collecting, drains the pending pairs, then repeats twelve times. It checks
96 ordered tags, finite durations, zero invalid/skipped pairs, zero live targets
after each release and renderer health. Test-only flushing and bounded polling
make hidden-window completion observable; production collection adds no flush or
wait. Existing tests also cover pending/unfinished timer destruction, GL query
object deletion, shader reload and target resizing.

All 50 runtime CTest groups passed with GCC Debug and Release, without hardware
skips. The `gpu_timer_queue`, `stress_cpu` and `stress_report` groups also passed
ASan/UBSan with leak detection enabled. Deterministic failures are injected only
through the test backend; native reports below contain actual driver observations.

## Native diagnostic collection

Four sessions were planned in advance and run sequentially from committed source
`16419dd`, GCC 16.2.1 Release (`-O3 -DNDEBUG`). Each used seed 1, ten load/unload
cycles, 60 warmup and 600 measured frames per cycle: 6,600 attempted pairs and
6,000 measured CPU frames. The first three used 8,192 sprites with all other counts
zero; the fourth used default combined counts. No retries or timing thresholds
were used. Compilation and automated testing had finished before collection.

The PC was the existing i5-4300U / HD Graphics 4400 with Mesa 26.2.2-arch1.1,
OpenGL 4.6 Core under XWayland. Every session rendered a 1920×1080 offscreen target
into a 1050×1360 managed preview, reported VSync-disable acceptance and used no
physical audio device or screenshot. This is diagnostic coverage on the normal
desktop, not a new controlled performance comparison or hardware reliability bound.

| Raw report | Submitted | Collected valid | Pending | Invalid / skipped | Measured GPU samples |
| --- | ---: | ---: | ---: | --- | ---: |
| [Sprites 1](diagnostics/2026-10-17/sprites-1.json) | 6,600 | 6,599 | 1 | 0 / 0 | 5,999 |
| [Sprites 2](diagnostics/2026-10-17/sprites-2.json) | 6,600 | 6,599 | 1 | 0 / 0 | 5,999 |
| [Sprites 3](diagnostics/2026-10-17/sprites-3.json) | 6,600 | 6,599 | 1 | 0 / 0 | 5,999 |
| [Combined](diagnostics/2026-10-17/combined.json) | 6,600 | 6,600 | 0 | 0 / 0 | 6,000 |

All sessions returned zero, completed all cycles and preserved resource counts and
checksums. Sprite checksum `10141788747900155707` and combined checksum
`3573445382734126645` match the previous corresponding workloads. Reports have empty
failure arrays and zero omitted failures. Query counts include warmup; GPU timing
sample counts exclude it. Three pending pairs remain unobserved by design.
The [manifest](diagnostics/2026-10-17/manifest.json) preserves exact command arrays,
source revision, exit codes, wall times and stderr for every attempt.

No root cause or synchronization fix is claimed. The tests provide stronger
coverage for ownership/reuse and teardown, and the new records make the next
occurrence inspectable. Further investigation should retain the complete report,
identify the affected cycle/step from the frame tag, compare raw timestamps and
collection lag, and capture relevant driver/session events before assigning a
cause. Representative gameplay profiling and the existing physical audio/display
validation gaps remain open.
