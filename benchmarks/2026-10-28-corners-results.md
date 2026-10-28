# Quad-corner reuse results — 2026-10-28 UTC

**Do not adopt the candidate.** It passed representative rendering checks but failed
its [predeclared CPU gate](QUAD_CORNER_REUSE.md). The renderer is restored byte for
byte to the reference. The [prototype remains in Git history](https://github.com/williamstyronejr/engine_test/commit/2f0e9de93119629079f4bf98ff4059065edcba3b),
and all attempts and the [decision record](../validation/2026-10-28-corners/decision.json)
are retained. There is no new runtime API, option or report schema.

## Scope and provenance

Reference `547695cd43ba707b88013e641357d7fc639e40f9`; prototype/correctness
`2f0e9de93119629079f4bf98ff4059065edcba3b`; timed candidate `2fcb6dc0b5ecdd473c7c0c7fce4d0b613ce30cea`.
The latter changes the collector and evidence only, with identical runtime sources.
The [manifest](../validation/2026-10-28-corners/manifest.json) records executable
hashes, commands, temperatures, RSS and source identities. Collection ran
**09:38:34–09:39:31 UTC**, sequentially, without concurrent builds/tests or captures.

GNU 16.2.1 Release, `-O3 -DNDEBUG`, sanitizers off; i5-4300U, Intel HD 4400,
Mesa 26.2.5-arch1.1, Linux 7.2.8-9-omarchy x86_64, XWayland. Both binaries enabled
the same frame/submission/preparation/context instrumentation. Offscreen target
1920×1080; actual managed preview 1050×1360. Immediate swap requests were issued in
all six runs, which does not verify physical presentation synchronization.

All six timed attempts completed: 3 cycles × (60 warmup + 600 measured frames),
1,980 presentations and 1,800 measured rows each. Default workload: 8,192 sprites,
4,096 tiles, 1,024 entities, 128 sparse bodies, 8 offline voices, seed 1. Every
checksum is `3573445382734126645`; all workload, render totals, batch/upload sizes,
trace tags/partitions and resource stability checks match. Each measured frame
submits 10,240 quads, culls 2,048, draws 3 batches and uploads 1,966,080 vertex bytes.

Before timing, all six original launch attempts exited 1 because the collector
passed unsupported `--assets` to stress mode. No frames or reports were produced.
Their [original manifest](../validation/2026-10-28-corners/launch-failure/manifest.json),
collector and logs remain intact. The correction was committed before timing;
there were no timing-based retries, candidate revisions or adoption-rule changes.

## CPU decision

Primary measure is the median of each frame's sum of non-GL preparation thread
CPU durations, preserving subtraction and summation before taking the percentile.
Positive percentages indicate less CPU time. Adoption required every pair to show
at least 5% reduction, plus passing correctness.

| Pair | Reference CPU median (ms) | Candidate CPU median (ms) | Reduction | Meets ≥5% |
| --- | ---: | ---: | ---: | --- |
| 1 | 1.588824 | 1.580654 | +0.51% | No |
| 2 | 1.349577 | 1.643021 | -21.74% | No |
| 3 | 1.252615 | 1.169568 | +6.63% | Yes |

The gate fails. Reference CPU medians also drift from 1.589 to 1.253 ms across the
session; frequency, thermal state and other activity were uncontrolled. These
observations neither prove a general slowdown nor establish a repeatable speedup.
Do not compare these CPU values directly to earlier sessions as an optimization
result. No tuning or further timing attempts were made after seeing the result.

All duration columns below are milliseconds. Tile/sprite values are each phase's
non-GL thread CPU medians; their sum need not equal the median of per-frame totals.

| Run | CPU median | CPU p95 | CPU p99 | Tile CPU median | Sprite CPU median | Frame p99 | Frame max | Frames >16.7 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| reference 1 | 1.589 | 2.175 | 2.335 | 0.768 | 0.782 | 10.738 | 23.816 | 7 |
| candidate 1 | 1.581 | 2.266 | 2.680 | 0.816 | 0.738 | 9.913 | 26.435 | 4 |
| candidate 2 | 1.643 | 2.280 | 2.604 | 0.860 | 0.754 | 10.535 | 25.185 | 5 |
| reference 2 | 1.350 | 2.073 | 2.451 | 0.649 | 0.655 | 10.065 | 22.138 | 4 |
| reference 3 | 1.253 | 1.870 | 2.182 | 0.602 | 0.616 | 11.515 | 47.571 | 4 |
| candidate 3 | 1.170 | 1.928 | 2.386 | 0.570 | 0.567 | 12.184 | 78.446 | 10 |

All six frame p99 values pass the existing 16.7 ms reference; all six maxima fail.
Presentation is the largest outer CPU interval in all 34 frames above 16.7 ms.
The largest frame, candidate 3 at 78.446 ms, remains in the evidence; it is not
attributed to corner arithmetic. All 139 preparation wall-minus-CPU gaps above
1 ms accompany involuntary-only switch spans. Another 1,908 frames have involuntary
switches with gaps ≤1 ms. Counts include whole phases and cannot assign causes or
durations to switches. Paired worst frames and all signed gaps remain available
in [analysis.json](../validation/2026-10-28-corners/analysis.json).

Reference 2 and candidate 3 each retain one uncollected measured GPU result; the
other four runs retain all 1,800. No GPU waits or manufactured values were added.

## Memory and correctness

Kernel per-child peak RSS is **54.94–55.38 MiB**, below the original 256 MiB ceiling;
it includes launch, reporting and teardown, and is not live heap measurement.
Tracked workload buffers remain 8,072,232 bytes; timing vectors 100,824; raw frame,
submission, preparation and context payloads 158,400 / 100,800 / 244,800 / 244,800.
The atlas remains 16 GPU bytes and the target 8,294,400, with stable resource counts.
No allocation or buffer strategy changed. Broader memory-path coverage stays open.

The reference passed seven focused groups. The prototype passed the same seven in
GNU Debug/Release and Clang Debug, one CPU ASan/UBSan group with leak checking, and
six native ASan/UBSan groups with driver leak checks disabled: **35 group executions,
no failures or skips**. All **36 stress PPM comparisons** across four native
configurations match the reference. Existing transform/target tests cover mirrored
and sheared hierarchy pixels, camera motion/zoom/resizing and partial rotated
culling; other groups cover sampling, font graphics, alpha, clipping and batching.
These are representative checks, not exhaustive floating-point equivalence proof.

The analyzer accepts valid/full-width uint64 controls and rejects twenty corrupted
records after hashes are updated in isolated copies. See the
[evidence index](../validation/2026-10-28-corners/RESULTS.md) for commands, compiler
logs, test XML and final packaging checks.

At restoration/documentation commit `dd434ea`, the seven Release correctness groups
pass again together with all three package groups (10/10, no skips, 47.51 s).
All 61 evidence files then present and both benchmark documents match their archive
copies; development allocation profilers remain excluded.

## Follow-up

Obtain function-level CPU attribution before proposing another arithmetic
optimization; first check sampling-tool availability and access, then declare the
collection and its overhead checks. Phase totals do not identify a hot function.
Keep this failed optimization trial and earlier failed frame-time attempts.
Physical display/audio, broader memory coverage and the final support record remain
open. Background loading stays deferred. This experiment does not close release
acceptance or milestone 6.
