# Function sampling results — 2026-10-28 UTC

`Renderer::Impl::submit` is the largest emitted application function in each of
three profiles, with 53.62–55.01% of application-image samples and 33.64–37.50% of
all captured samples. This supports investigating its vertex-emission path next;
it does not establish a particular instruction as the cause of frame-time spikes.
Engine/runtime sources remain unchanged. The earlier rejected corner-reuse
experiment remains rejected.

## Collection and access

The [declaration](STRESS_FUNCTION_SAMPLING.md) and collector were committed at
`9b4dd215fe5094086143547e6e9e9229229c6ec4` before the six timed processes. All use one GNU 16.2.1 Release binary
(`-O3 -DNDEBUG`, sanitizers off), identical environment and the default combined
workload: 8,192 sprites, 4,096 tiles, 1,024 entities, 128 sparse bodies, 8 offline
voices, seed 1. Target 1920×1080; actual managed preview 1050×1360; immediate
presentation requests issued. Three cycles of 60 warmup plus 600 measured frames:
1,980 presentations and 1,800 measured rows per process. Collection ran
**14:51:34–14:52:30 UTC**, without concurrent builds/tests or captures.

Host: i5-4300U / Intel HD 4400, Mesa 26.2.5-arch1.1, Linux 7.2.8-9-omarchy x86_64,
XWayland. The installed gprofng 2.47 collector was killed with SIGKILL on the engine
and two trivial programs, before sampling; its cause remains unconfirmed. These
failures and the corrected perf capability probe are retained in the
[evidence](../validation/2026-10-28-functions/RESULTS.md). No permissions or system
settings were changed. A project-local perf 7.2.11-1 and two libraries were extracted
from repository packages with matching SHA-256 values, without a system install.

Perf samples the launched main thread only (`--no-inherit`), using user-space
`cpu-clock:u`, period 1,000,000 ns and a 128-page buffer. Baseline runs omit the
perf wrapper. No frame waits, call-chain capture, code instrumentation or compiler
flag changes. Profiled scope is the **whole process lifetime**, including startup,
warmup, preview, serialization and teardown. It is not aligned with measured
frames or the non-GL preparation residual. Kernel work, blocked time and driver
worker threads are excluded.

## Exclusive function attribution

All three exports have a single PID equal to TID, no lost/throttled records, over
500 application samples, and zero unresolved application symbols. Each passes the
predeclared quality rules. Full samples, counts, event attributes, object/build IDs,
raw perf data, binary hash and executable symbol table are retained.

| Profile | All samples | Application samples | Submit samples | Submit / all | Submit / application | Unresolved outside application |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 6,782 | 4,580 | 2,484 | 36.63% | 54.24% | 2,015 |
| 2 | 7,090 | 4,834 | 2,659 | 37.50% | 55.01% | 2,072 |
| 3 | 6,056 | 3,799 | 2,037 | 33.64% | 53.62% | 2,099 |

The next application symbols in all three runs are `run_stress_mode` (18.85–19.59%
of application samples) and the tile-render lambda inside `StressGraphics::render`
(16.09–17.08%). Optimized/inlined operations are charged to the emitted symbol;
these are not inclusive caller times. Compiler clones are kept separate. Unknown
library samples stay in the total denominator, so these data cannot rank individual
unresolved driver functions. See [profiles.json](../validation/2026-10-28-functions/profiles.json)
for every symbol/object count.

The [first profile's submit disassembly](../validation/2026-10-28-functions/submit-annotation.log)
retains all 2,484 samples attributed to that symbol. Samples cluster in the emission
loop around scalar division, stores and loop/vertex bookkeeping. Interrupt skid,
register scheduling and compiler transformations prevent treating a sampled
instruction as an isolated cost. The evidence justifies inspecting this loop;
it does not validate replacing division, changing floating-point order or removing
input checks.

## Observer effects and retained timing failures

All duration columns are milliseconds. Preparation CPU is the median of per-frame
sums of phase CPU minus nested GL CPU, not a sum of percentiles.

| Run | Frame median | Frame p99 | Frame max | Frames >16.7 | Preparation CPU median | Missing GPU rows |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| baseline 1 | 3.627 | 11.176 | 33.884 | 5 | 1.456 | 1 |
| sample 1 | 3.781 | 11.168 | 24.306 | 5 | 1.653 | 1 |
| sample 2 | 3.956 | 11.109 | 25.403 | 2 | 1.746 | 0 |
| baseline 2 | 3.790 | 10.106 | 37.775 | 3 | 1.659 | 0 |
| baseline 3 | 3.930 | 10.091 | 25.115 | 2 | 1.718 | 0 |
| sample 3 | 3.648 | 10.736 | 31.913 | 6 | 1.397 | 0 |

Paired changes are `100 × (sampled / baseline − 1)`; positive means a higher observed
duration. Order alternates baseline/sample, sample/baseline, baseline/sample.

| Pair | Frame median change | Frame p99 change | Preparation CPU median change |
| --- | ---: | ---: | ---: |
| 1 | +4.25% | -0.08% | +13.48% |
| 2 | +4.38% | +9.92% | +5.27% |
| 3 | -7.19% | +6.39% | -18.72% |

The third pair reverses the direction of both median measures. Uncontrolled CPU
frequency, thermal state and session activity prevent a precise causal overhead
estimate. Do not subtract these percentages from profiles or claim a speedup.
Raw functions and the timing traces are different measurement scopes.

Every frame p99 passes the existing 16.7 ms reference; **all six maxima fail**.
Of 23 frames above 16.7 ms, presentation is the largest outer CPU interval in 20
and render submission in three (sample 1). All 145 preparation gaps >1 ms accompany
involuntary-only phase switch spans; this remains an association without a duration
or cause assigned to any switch. Baseline 1 and sample 1 each retain one uncollected
GPU row; no missing value is synthesized or waited for. Earlier failed attempts,
limits and release gates remain unchanged.

## Correctness and memory

All six workloads/checksums/render totals/trace tags/partitions/resources validate;
checksum `3573445382734126645`. Per measured frame: 10,240 submitted quads, 2,048
culled, 3 batches, 1,966,080 uploaded vertex bytes. All nine existing small stress
capture modes pass their trace/identity/presentation assertions both normally and
under perf; **nine PPM pairs are byte-identical**. Ten malformed or inadequate
profile controls are rejected; a valid profile with unresolved external symbols
is accepted without dropping those samples.

Tracked CPU workload buffers remain 8,072,232 bytes, timing vectors 100,824, and
frame/submission/preparation/context payloads 158,400 / 100,800 / 244,800 / 244,800.
GPU atlas 16 bytes and target 8,294,400 bytes, with stable resource counts. Baseline
kernel peak RSS is 54.77–54.88 MiB. Profiled launch-tree peak is 54.84–55.02 MiB,
including perf and its child; it is not an engine-only heap or RSS delta. These
observations do not extend the older memory-path coverage. Raw engine cycle-boundary
RSS remains separately available in reports.

At documentation commit `6b47848`, all three Release package groups pass without
skips (36.99 s). All 65 evidence files then present and both benchmark documents
match archived copies; development profilers remain excluded.

## Next investigation

Select and predeclare one vertex-emission experiment inside `Renderer::Impl::submit`
using the retained emitted code. Preserve validation, culling, batching and
floating-point operation order; require image/transform checks and a repeatable
CPU benefit before adoption. Keep presentation/scheduling tails distinct from
steady CPU cost. No renderer optimization is adopted in this increment.
Physical display/audio, wider memory coverage and final support records remain
open; background loading remains deferred.
