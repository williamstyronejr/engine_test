# Candidate workload and memory acceptance, version 1

This declaration fixes the increment 61 evaluation **before collection**. It applies
only to the shipped assets, Linux x86-64 development PC (i5-4300U / Intel HD 4400),
GNU Release with sanitizers off, and the workloads below. It adds no runtime limits
or format changes. The machine-readable [policy](../validation/2026-10-26-workload-acceptance/policy.json)
fixes counts, fingerprints, budgets and three repeats per workload. Budget changes
require a new declaration; failed attempts must remain in the record.

## Workloads and timing decisions

Run sequentially after building/testing, without screenshots or other benchmark
jobs. Use seed 1, request VSync off, preserve the active compositor and record its
actual viewport. Do not change desktop settings for a measurement. Each process
starts fresh; disk/driver caches and CPU clocks are uncontrolled.

| Workload | Declared work per repeat | Timing scope |
| --- | --- | --- |
| Combined stress | 8,192 sprites, 4,096 tiles, 1,024 entities, 128 sparse bodies, eight offline voices; 1920×1080 offscreen; three cycles of 60 warmup + 600 measured frames | Frame includes preview and presentation; GPU covers offscreen workload only |
| Dense collision | CPU stress, 256 overlapping bodies, all other counts zero; same cycles/warmup | Simulation plus empty mixer/frame overhead; no renderer |
| Mixer capacity | CPU stress, 16 offline voices, all other counts zero; same cycles/warmup | Simulation plus mixer/frame overhead; no ALSA device |
| Facility replay | Checked-in 3,000-command ERPL; first 30 warmup | Actual managed-window gameplay frame including minimap/UI/present |
| Annex replay | Same length/warmup, independent checked-in route | Same scope, different level |
| Reload/transition | `reload_transition_v1`, 3,600 frames, first 30 warmup; ten verified operations | Gameplay frame, plus individual synchronous operation times |

All six workloads must complete three times with consistent structural results.
For each attempt, report **two separate timing decisions**: frame p99 ≤ **16.7 ms**
(the tail target), and frame maximum ≤ **16.7 ms** (the stricter sampled-frame
ceiling). All ten reload/transition operations must also be ≤16.7 ms. Retain median,
p95, p99 and maximum for every measured scope; never add separate percentiles.
A failed ceiling is not waived because p99 passed. No deadline-miss percentage can
be inferred from these aggregate distributions. These targets apply to the stated
intervals, not whole-loop wall time, display scanout, or startup.

The 16.7 ms target comes from [the engine plan](../ENGINE_PLAN.md#5-performance-requirements).
The stress surface is 1080p; the managed gameplay window is **not** guaranteed to
be 1080p. Passing these tests would not close physical 1080p/60 Hz acceptance.
GPU queries must use supported elapsed timing, no invalid/skipped results, consistent
submitted/completed/pending accounting, and at least measured frames minus eight
completed measured samples. Pending results are not waited on. CPU isolates have
no GPU samples. Any missing/partial report, changed workload, nonzero process exit,
nonfinite timing, or unverified operation is an evidence failure, never a pass.

## Memory budgets

These are workload acceptance ceilings, not engine API capacities. Binary KiB/MiB
mean 1,024/1,048,576 bytes. Every repeat must pass independently.

| Measurement | Ceiling | Reason and limits |
| --- | ---: | --- |
| Whole-process peak resident set | 256 MiB per process, all six workloads | A new conservative development-host envelope above historical ~53 MiB boundary observations. Measured using Linux `wait4().ru_maxrss × 1024`, including startup, execution, teardown and report writing. Not a measured heap/GPU limit or a historical peak comparison. |
| Stress tracked CPU payload | 9 MiB, each stress workload | Headroom above historical combined 8,072,232 bytes; use actual vector capacities. Includes selected arena storage only. |
| Stress timing buffers | 128 KiB | Three-cycle report reservation, distinct from arena payload and RSS. |
| Combined uploaded texture / target payload | 16 bytes / 8,294,400 bytes; one each | Exact tiny atlas and 1920×1080 RGBA8 target; baseline restoration required. CPU isolates must have zero. |
| Published scene reservation | 80 KiB per level | Existing 256-entry Feature Lab scene policy on this ABI; includes component dense/sparse buffers. |
| Published collision reservation | 32 KiB per level | Existing 32,256-byte reservation rounded up; body/contact/event/membership capacities still govern overflow. |
| Covered operation CPU / GPU / simultaneous combined staging | 160 KiB / 32 KiB / 160 KiB per event | Headroom above historical 154,942-byte combined maximum; zero remaining CPU/GPU staging required. CPU and GPU maxima need not coincide. |
| Stock live graphics inventory during spike workload | Nine textures ≤32 KiB, one minimap target ≤128 KiB, font ≤16 KiB | Snapshot inventory plus per-frame stability check; not total VRAM. |

RSS covers CPU-resident pages attributable to the process, including shared mappings
and driver/allocator caches; it does not identify ownership, capture swapped pages,
or measure GPU residency. Kernel high-water collection avoids confusing sparse
cycle-boundary RSS with a peak. RSS is not added to any payload counter. No hard
OS memory cap is installed, and the 256 MiB budget is not a portable system requirement.

Existing [allocation evidence](2026-10-26-diagnostic-text-results.md) remains a
separate scoped demand check: zero recurring measured render calls in 30 UI cases,
with startup, layout/rebinding, errors, reporting and replay restart exceptions.
The [audio allocation record](2026-10-24-allocation-results.md) covers the real
worker using null/failing sinks. None measures peak live memory. This collection
uses ordinary executables without allocation hooks and does not re-label those
older runs as new evidence.

## Storage coverage audit

| Storage | Existing protection/evidence | Remaining coverage boundary |
| --- | --- | --- |
| Scene and collision | Explicit capacities; transactional overflow tests; published reservation and utilization reports | Scene strings, registry/hash nodes, arbitrary component-owned storage, unsuccessful/unpublished demand excluded from utilization |
| Game assets and transitions | Bounded readers; selected vector/staging leases; rejected candidates preserve live state | Stream buffers, parser strings/hash nodes, allocation metadata, reallocation overlap and some constructor intermediates excluded |
| Renderer | 4,096-quad batch; 64 texture handles, eight targets; target/reload byte limits; fixed clip stack | CPU vertex buffer, built-in atlas, VBO/program/query objects, driver uploads/alignment/retirement excluded from texel inventory; handle count is not a total texture-memory cap |
| UI and diagnostics | Caller-owned reusable text; bounded diagnostic histories; measured warmup/transition allocation scopes | Future text growth and font/layout storage are not a global live-heap budget |
| Audio | 16 mixer voices, 32 sound slots, bounded queue/bank checks; worker allocation and race tests | These six runs use offline/no audio; retained/replacement banks, ALSA buffers and real device lifecycle need separate acceptance |
| Profiling, filesystem and platform | Bounded report/event counts; RSS includes resident process storage | Report sorting/serialization, settings/saves, file streams, Xlib/GL/ALSA and allocator caches are not all itemized |

Do not sum independent utilization peaks or mistake handle/element limits for a
whole-engine byte cap. The RSS envelope gives a complementary process observation
for these runs; it does not close unexercised paths or prove long-session leak freedom.
A broader claim needs expanded interactive/audio/error/long-duration workloads and
ownership instrumentation where attribution matters.

## Reproduction and acceptance record

The evidence directory retains exact commands, timestamps/exits, source and asset
hashes, binary hashes, environment, raw reports, per-child peak RSS, an evaluator
and its decisions. Use a clean committed source and reconfigure Release so report
build identity matches. Run the collector from the repository root with Python 3
(standard library only; no engine build/runtime dependency):

```sh
export PATH="$PWD/.tools/bin:$PATH"
cmake --preset release
cmake --build --preset release --target feature_lab stress_bench -j2
python3 validation/2026-10-26-workload-acceptance/collect.py NEW_EMPTY_DIRECTORY
python3 validation/2026-10-26-workload-acceptance/evaluate.py NEW_EMPTY_DIRECTORY
```

The collector refuses existing output files, records each of the 18 attempts even
if an earlier attempt fails, and hashes its declaration. The evaluator returns 0
only if all automated criteria pass, 1 for a measured budget failure, and 2 for
invalid/incomplete evidence. Changing viewport/driver between repeats invalidates
comparison. Results apply to their recorded build, not automatically to later code.
R6 remains open if any required timing/memory condition fails or the representative
physical presentation and unmeasured-path coverage remain incomplete. R4/R5 and the
final R8 support record are independent release gates.
