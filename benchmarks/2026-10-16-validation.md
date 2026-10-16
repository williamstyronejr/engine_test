# Performance validation — 2026-10-16

## Method and scope

The suite uses committed source `f6e8ce4`, GCC 16.2.1 Release (`-O3 -DNDEBUG`),
workload version 1, seed 1, three load/unload cycles, 60 warmup steps and 600
measured steps per cycle. Each CPU distribution has 1,800 samples. Raw reports
and an ordered command/environment manifest are in [baselines/2026-10-16](baselines/2026-10-16).
The manifest contains exact argument arrays, process exit codes, wall times and
package-temperature observations; it is sufficient to repeat the same workloads
using the two Release binaries. It does not capture every background process.

The testing PC is an Intel i5-4300U with HD Graphics 4400, using Mesa 26.2.2-arch1.1,
OpenGL 4.6 Core and XWayland in the existing Wayland desktop session. AC power was
connected. The `intel_cpufreq` driver used `schedutil`, an 800–2,900 MHz allowed
range, with turbo permitted. Frequency, fan speed and compositor scheduling were
not locked. Compilation and automated tests finished before measurements; runs
were sequential, without screenshots. The ordinary desktop and this assistant
session remained active. These are controlled workload comparisons on a live
desktop, not isolated laboratory measurements.

All visual runs request VSync off. The 1920×1080 (or comparison 1280×720) workload
is rendered into a fixed RGBA8 target, then fitted into the window-manager-sized
preview with a HUD. Actual window dimensions and driver acceptance of the swap
interval request are recorded per run. No physical audio device is opened: mixer
measurements process 800 stereo samples per simulation step synchronously.

Three combined runs bracket the isolates to expose variation during the suite.
The 720p comparison preserves the 16:9 aspect ratio, object counts and simulation
checksum. Sprite and tile pairs change only their named counts, with all other
workload counts zero. The CPU cases omit rendering entirely. These empty-count
isolates still include fixed arena/mixer overhead; they do not subtract an empty
baseline. Compare CPU-only frame times only with CPU-only results.

See [timing scopes](README.md#report-schema-and-timing-scopes) for exact boundaries.
In particular, frame time ends after swap and excludes subsequent validation and
report bookkeeping. GPU timestamps cover the offscreen pass only, while CPU
submission can include driver waits. Presentation timing cannot distinguish GPU
completion, compositor scheduling and swap-interval waits. Separate percentiles
cannot be added or subtracted to reconstruct another scope's percentile.

## Results and bottlenecks

All three combined 1080p runs drew 10,240 visible quads (4,096 tiles), culled 2,048
sprites and issued three workload draw calls per measured frame. Each simulation
step had 776 collision candidates, no contacts and eight active mixer voices.
Checksums matched across the three repeats, the 720p comparison and the CPU-only
combined run (`3573445382734126645`), also matching the October 8 reference.

| Report | Interval | Median ms | p95 ms | p99 ms | Max ms |
| --- | --- | ---: | ---: | ---: | ---: |
| [1080p repeat 1](baselines/2026-10-16/combined-1.json) | frame | 3.534 | 8.753 | 12.413 | 28.133 |
| [1080p repeat 2](baselines/2026-10-16/combined-2.json) | frame | 3.319 | 9.147 | 11.312 | 24.400 |
| [1080p repeat 3](baselines/2026-10-16/combined-3.json) | frame | 3.390 | 9.245 | 10.912 | 33.185 |
| [720p comparison](baselines/2026-10-16/combined-720p.json) | frame | 2.813 | 7.464 | 10.738 | 23.561 |

Frame p99 was below the provisional 16.7 ms budget in each combined run. Maxima of
24.400–33.185 ms in the 1080p repeats show that individual frames still missed that
budget. No deadline-miss percentage can be recovered from the stored distributions.
These short unpaced runs are not a sustained 60 FPS guarantee. The October 8 p99
was 17.981 ms, but this is not evidence of an optimization: revisions, observation
dates and operating conditions differ, and no controlled before/after change was
measured. Combined mean frame time remained about 4.96–4.99 ms in this suite.

Across the three 1080p repeats, CPU offscreen-submission medians were
1.545–1.963 ms and GPU offscreen medians were 1.532–1.571 ms. Preview/HUD CPU medians
were 0.139–0.208 ms. Buffer-swap p99 was 7.361–8.383 ms, larger than any other
instrumented CPU subinterval's p99 (render submission: 3.339–5.730 ms). This makes
presentation/driver pacing the first candidate for investigation of combined-frame
tails; it does not identify the cause of a particular slow frame, because paired
per-frame samples are not stored. Lowering resolution to 720p reduced GPU median
to 0.928 ms while CPU render submission remained 1.874 ms: pixel workload matters,
but lowering it does not remove CPU traversal/submission or presentation costs.

The isolated workloads identify capacity pressure independently of the default mix:

| Report | Interval | Median ms | p95 ms | p99 ms | Max ms |
| --- | --- | ---: | ---: | ---: | ---: |
| [8,192 sprites, accepted repeat](baselines/2026-10-16/sprites-8192-repeat.json) | render_submit | 1.519 | 2.432 | 2.743 | 5.812 |
| [32,768 sprites](baselines/2026-10-16/sprites-32768.json) | render_submit | 3.220 | 5.387 | 7.912 | 21.144 |
| [8,192 sprites, accepted repeat](baselines/2026-10-16/sprites-8192-repeat.json) | gpu_render | 1.381 | 1.562 | 1.651 | 1.776 |
| [32,768 sprites](baselines/2026-10-16/sprites-32768.json) | gpu_render | 3.530 | 3.784 | 3.835 | 4.093 |
| [4,096 tiles](baselines/2026-10-16/tiles-4096.json) | render_submit | 1.224 | 2.081 | 3.872 | 9.747 |
| [16,384 tiles](baselines/2026-10-16/tiles-16384.json) | render_submit | 2.920 | 4.253 | 5.227 | 24.504 |
| [4,096 tiles](baselines/2026-10-16/tiles-4096.json) | gpu_render | 0.775 | 0.866 | 0.896 | 1.003 |
| [16,384 tiles](baselines/2026-10-16/tiles-16384.json) | gpu_render | 1.102 | 1.250 | 1.309 | 1.720 |
| [4,096 entities, CPU](baselines/2026-10-16/entities-4096.json) | simulation | 0.122 | 0.191 | 0.272 | 0.410 |
| [256 sparse bodies, CPU](baselines/2026-10-16/collision-sparse.json) | simulation | 0.040 | 0.054 | 0.069 | 0.086 |
| [256 overlapping bodies, CPU](baselines/2026-10-16/collision-dense.json) | simulation | 2.097 | 8.720 | 9.883 | 23.083 |
| [16 voices, CPU](baselines/2026-10-16/mixer-16.json) | mixer | 0.169 | 0.230 | 0.259 | 0.484 |
| [Default counts, CPU only](baselines/2026-10-16/combined-cpu.json) | frame | 0.202 | 0.318 | 0.385 | 0.493 |

Four times as many sprites increased visible quads from 6,144 to 24,576 and batches
from two to six; both CPU submission and GPU costs grew materially. Four times as
many tiles increased batches from one to four, with stronger growth in CPU
submission than GPU time. These measurements support investigating traversal,
batching and submission separately before changing buffer-streaming strategy.
They do not distinguish traversal instructions from driver back-pressure.

Dense collision is the principal simulation stressor in this set: 256 overlapping
bodies generate all 32,640 pairs/contacts per step, versus 1,672 candidates and no
contacts for the sparse case. The grid cannot eliminate overlapping pairs. Its
9.883 ms simulation p99 consumes much of a 60 Hz budget by itself. Keep overlap
density bounded in authored scenes and profile real contact workloads before
attempting an optimization. The entity and offline mixer isolates were much
smaller costs; mixing 16 voices had a 0.259 ms p99. CPU-only and rendered simulation
costs differ because cache, scheduling and frequency conditions differ.

## Run integrity and resource observations

Fourteen runs were retained: thirteen accepted runs and one rejected attempt.
All completed three cycles and passed resource/checksum checks. Every visual run
used a 1050×1360 preview and reported VSync-disable acceptance. Accepted visual
runs had zero invalid/skipped queries, 1,799 measured GPU samples with one pending
query, except 720p which collected all 1,800 measured samples with none pending.
Query totals include the 180 warmup frames; timing distributions exclude them.

The first [8,192-sprite attempt](baselines/2026-10-16/sprites-8192.json) returned
exit code 1 with one invalid timestamp pair (end value less than start). Its raw
report is preserved and excluded from the comparison tables. The suite paused
for inspection, then continued; the case was repeated once at the end and passed.
The cause is unresolved. Existing queue tests cover out-of-order timestamp
rejection, delayed availability, pool rollover and lifecycle misuse, but they do
not establish the cause of this native observation. No timestamp was clamped,
no GPU wait was added, and the nonzero failure behavior remains intact.

Combined tracked CPU buffer payload was 8,072,232 bytes; reserved report buffers
were 100,824 bytes (28,800 more than the old six-series report). Uploaded atlas
payload was 16 bytes and the 1080p target was 8,294,400 bytes; 720p used 3,686,400.
Live counts and tracked capacities stayed stable across cycles and GPU resources
returned to their unloaded baseline. Combined 1080p RSS rose by roughly 3.1 MiB
between startup and final teardown; this observation includes retained caches,
not just live arena resources. Package-temperature snapshots ranged from 56°C to
89°C. Dynamic frequency and this temperature spread limit precise comparisons;
no thermal-throttling cause is claimed from these snapshots alone.

## Verification

All 50 runtime CTest groups passed in GCC Debug and Release without skips,
including CPU benchmark/report checks, native stress rendering and GPU timing.
The collection checked completion, 1,800 CPU samples per active scope, zero
CPU-mode rendering samples, workload/checksum agreement, exact target payloads,
query validity and process exit codes. Exact commands and the rejected attempt
are retained in the manifest; performance thresholds are not correctness tests.

## Remaining F15/F16 limits

- This synthetic workload does not establish a budget for normal Feature Lab
  gameplay, minimap/UI composition, reload spikes, recording, saves or transitions.
  Those paths need a separately declared representative workload and timing scopes.
- GPU coverage is the offscreen pass, not the preview/compositor or scanout. There
  is no measured input-to-display latency, physical fullscreen/refresh/scale matrix,
  direct Wayland path or second GPU/driver result.
- Asynchronous timing intentionally leaves outstanding samples pending. Neither
  frame p99 nor a successful resource check is a worst-case 60 FPS guarantee.
- CPU buffer and texture/target payload accounting excludes allocator metadata,
  some scene storage, renderer/driver allocations and system caches. RSS snapshots
  and repeated object-count restoration are not a whole-process leak proof.
- Allocation-free CPU stress checks cover established simulation/mixing paths.
  Preview HUD string formatting, report generation and load/reload are intentional
  allocation paths; complete render hot-path allocation profiling remains open.
- Offline mixing does not measure worker scheduling, device latency, underruns,
  disconnect/reconnect, suspend/resume or audible playback. The existing
  [audio](../docs/AUDIO_VALIDATION.md) and [platform](../docs/PLATFORM_VALIDATION.md)
  manual validation gaps remain open. Broader manual fault scenarios remain open.

F15 and F16 remain partial. No timing threshold is added to CI, and these results
alone do not justify a renderer synchronization change or persistent mapping.
