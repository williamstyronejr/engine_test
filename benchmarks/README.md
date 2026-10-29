# Feature Lab stress workloads

For scripted reload and level-transition spikes through normal gameplay rendering,
see [SPIKE_PROFILING.md](SPIKE_PROFILING.md) and the
[measured results](2026-10-17-spike-results.md). Recorded ERPL profiling remains separate.

The [October 19 staging results](2026-10-19-staging-results.md) add covered CPU/GPU
payload peaks and cleanup evidence for the same operations.

The [parser staging follow-up](2026-10-19-parser-staging-results.md) extends that
coverage through loading/construction and retains malformed-level failure reports.

The [October 20 scene reservation results](2026-10-20-lazy-scene-results.md)
reduce the largest covered transition peak by 10.2% under the same v2 coverage,
with repeated normal and malformed-content evidence.

The [collision utilization follow-up](2026-10-20-collision-usage-results.md)
breaks down reserved buffers and per-level demand, identifying contact/event
budgets as the next reduction target.

Follow-up: [elapsed timing and rollover evidence](2026-10-17-elapsed-timing.md)
records the next increment's query-method change; earlier raw results are preserved.

The [2026-10-16 validation report](2026-10-16-validation.md) records repeated 1080p
results, workload scaling, presentation timings and the remaining F15/F16 gaps.
The [2026-10-17 GPU timing investigation](2026-10-17-gpu-timing.md) adds bounded
failure evidence and records native query-reuse/teardown validation.
Separate [recorded gameplay profiling](REPLAY_PROFILING.md) measures the real
minimap/world/HUD path; its [same-day results](2026-10-17-replay-results.md) include
a recurrence with raw timestamp evidence.

Run the optimized executable after a release build. All workloads are generated
in-house and require no asset files. The visible mode uses OpenGL 4.6; the CPU
executable has no X11/OpenGL/ALSA dependency. Both install into `bin/`.

```sh
cmake --preset release
cmake --build --preset release -j2
./build/release/feature_lab --stress --width 1920 --height 1080 \
  --frames 600 --warmup 60 --cycles 3 --report build/stress.json
./build/release/stress_bench --frames 600 --warmup 60 --cycles 3 \
  --report build/stress-cpu.json
```

`feature_lab --stress --cpu-only` runs the same CPU workload without opening a
window. `--stress --help` lists the options. Normal gameplay, replay, save slots and
settings are separate modes; their options are rejected in a stress run.
Escape or closing the window aborts with a nonzero exit and an incomplete report.
The report destination's parent must exist. A report is written atomically at the
end and printed to stdout; visual-mode startup diagnostics also appear on stdout,
so use the JSON file for automated consumers. A crash does not replace a prior
report. No user preferences are loaded or changed.

## Workload v1

| Option | Default | Inclusive range | Work measured |
| --- | ---: | ---: | --- |
| `--sprites` | 8192 | 0..65536 | Moving atlas quads; indices divisible by four are deliberately offscreen |
| `--tiles` | 4096 | 0..65536 | Exact occupied-cell count in one 256×256 map, distributed by a fixed permutation |
| `--entities` | 1024 | 0..4096 | Scene nodes whose transforms change every step; independent of sprite instances |
| `--bodies` | 128 | 0..256 | Moving circle sensors in the real collision grid |
| `--dense` | off | flag | All bodies overlap; otherwise a sparse grid with no contacts |
| `--voices` | 8 | 0..16 | Looping mono voices through the real mixer, 800 stereo output frames per step |
| `--seed` | 1 | uint64 decimal | SplitMix64 sprite positions and tile permutation offset |
| `--frames` | 600 | 1..10000 | Measured steps per cycle |
| `--warmup` | 60 | 0..600 | Unmeasured steps before each cycle's samples |
| `--cycles` | 3 | 1..50 | Fresh load, warmup, measurement, teardown repetitions |
| `--width`, `--height` | 1280, 720 | 64..4096 | Fixed offscreen workload resolution |

The product of measured frames and cycles may not exceed 60,000. Camera height is
36 world units; viewport aspect ratio controls horizontal culling. At 16:9 the
complete tilemap is visible and ceil(sprites / 4) sprites are culled. Tile
and sprite draws use a shared 2×2 RGBA8 atlas. Default visible workload: 10,240
quads, 2,048 culled sprites and three batches per measured pass. Entities and
bodies are CPU workloads; they are not extra render instances. Dense mode tests
up to 32,640 contacts. This is collision detection, not rigid-body simulation.

Each rendered frame advances exactly one simulation tick. There is no wall-clock
catch-up in a benchmark. VSync disable is requested and its acceptance is recorded;
compositor/driver pacing may still apply. Mixer work is synchronous and offline,
with no sound device or audio thread. `--no-audio` and `--no-vsync` are accepted for
convenience. CPU mode omits rendering and tile traversal; it retains map creation
and the same simulation/mixer workload. Counts can be set to zero independently,
so isolated measurements should explicitly zero the other workloads.

Every cycle uses the same configuration and seed. Final checksums must match
between complete cycles. The checksum covers tick count, seed, sprite positions
and cells, entity IDs/positions, collision body IDs/positions and contacts, tile
cell IDs, the last mixed PCM block, and active voices. Transient handles, timings
and memory addresses are excluded. Exact checksums are a local regression check,
not a cross-architecture floating-point guarantee or the gameplay replay format.

## Report schema and timing scopes

`schema` is `feature_lab_stress_v1`, with `workload_version: 1`. Reports include
build revision/configuration/compiler/flags, CPU/kernel/architecture, GL driver and
device, requested offscreen and actual window dimensions, workload counts, seed,
completion/resource checks, counters, memory and timing distributions. Seeds and
checksums are decimal strings to preserve all 64 bits in JSON consumers.

Each timing object reports samples, mean, median, p95, p99 and maximum in
milliseconds. Percentiles use sorted index floor((n−1) × percentile / 100), so the
median is the lower middle sample for an even count. Empty series have zero
samples and zero values; they must not be interpreted as a zero-cost measurement.
Samples cover all measured cycles, with warmup excluded separately in each cycle.

- `load`: arena construction plus atlas/target creation in visual mode. Initial
  window/context, shader and renderer startup are outside this scope.
- `simulation`: sprite/entity motion and collision-grid update.
- `mixer`: production mixer processing of one 60 Hz block; no device I/O.
- `render_submit`: CPU time for timer commands, offscreen tile/sprite traversal,
  batching and GL submission. Driver back-pressure can be included; presentation
  and the preview/HUD are outside this interval.
- `preview_submit`: CPU time after offscreen submission through preview/HUD
  submission, including HUD formatting and driver back-pressure.
- `present`: CPU time in the window buffer swap. This can include driver/compositor
  waiting even when VSync disable was accepted; it is not a pure VSync measurement.
  These two fields are additive schema-v1 extensions; older reports omit them,
  and CPU-only runs have zero samples for both.
- `frame`: visual loop from event polling through presentation, including preview,
  HUD and swap pacing. Per-frame health/resource checks and report bookkeeping,
  cycle checksums, load and teardown are outside this interval. CPU mode measures
  simulation plus mixing only. This is not total run wall time or display latency.
- `gpu_render`: asynchronous elapsed queries around the offscreen workload, excluding
  preview/HUD and presentation. Frame tags exclude warmup even when results arrive
  during the next cycle. Missing/pending/skipped/invalid query counts are explicit;
  shutdown never waits for GPU timing results.

CPU scopes measure elapsed time, including scheduling delays, rather than CPU
execution time. GPU work overlaps CPU work; do not add CPU and GPU distributions.
Percentiles of separate scopes also cannot be added to reconstruct frame percentiles.

The additive `gpu_queries.failure_capacity`, `failures` and `failures_omitted`
fields retain the first eight reversed pairs in timestamp mode, including warmup failures,
across the entire timer lifetime. Each record has decimal-string `frame`, `start_ns`,
`end_ns` and `last_attempted_frame` fields, preserving exact 64-bit values, plus a
numeric zero-based `slot`. The last attempted tag includes skipped begins; the
slot is a reusable query-pair index, not a GL object name. `invalid` still counts
all rejected pairs, and `failures_omitted = invalid - failures.length`. CPU-only
runs and elapsed-query runs have an empty array and zero omitted failures. Older reports lack these fields.

Stress frame tags start at one and include warmup. For a retained tag `f`, cycle
is `floor((f-1)/(warmup+frames))+1`; its zero-based step within that cycle is
`(f-1) % (warmup+frames)`, with steps below `warmup` excluded from timing summaries.
A reversed pair still makes the stress command fail even if it belongs to warmup
or exceeds the retained-record limit. Diagnostics neither wait for pending pairs
at shutdown nor infer the reason for a reversal. The fixed diagnostic arrays are
outside `memory.report_buffer_bytes`, which counts reserved timing vectors only.

`totals` counts only measured offscreen passes and simulation steps. HUD quads
are excluded. `--screenshot FILE.ppm` captures the final preview for visual checks;
readback time is subtracted from that frame's CPU interval, but capture can still
change GPU scheduling. Omit screenshots for performance baselines.

## Resource checks and their limits

Every frame compares arena object counts and tracked buffer capacities against
its cycle baseline and prior cycles. Texture/target counts and RGBA8 payload bytes
must remain constant while loaded and return to the renderer's original baseline
on each teardown. GPU timer objects, the built-in atlas, VBO and shader are owned
once per renderer and excluded from the uploaded texture/target counts.

`tracked_cpu_buffer_bytes` includes arena arrays/vectors, scene slot/node/deferred
vectors, tile palette/layer/cell/occupancy/edge buffers, collision vectors/query
bits, PCM output and mixer storage. It excludes allocator overhead, Scene hash
nodes and string storage, rendering buffers, and driver memory. `report_buffer_bytes`
separately accounts for reserved timing arrays; summaries allocate temporary sorted
copies only when writing the report. These are reserved payload sizes, not process
memory usage or bytes necessarily resident in RAM.

Linux `/proc/self/statm` RSS values are observations at report startup, cycle
boundaries while loaded, and after arena teardown; zero means unavailable. Driver
and allocator caching can retain RSS after unload. RSS changes do not decide
`resources_stable`. CPU tests separately instrument C++ new/delete: steady
simulation/mixing performs no C++ heap allocations, and eight construction/destruction
cycles return live allocation blocks to baseline, including Scene hash nodes.
This does not intercept arbitrary C malloc calls or establish a whole-process leak proof.
GPU tests check twelve create/draw/release cycles, batch rollover, culling, target
pixels, failed target creation and transactional texture byte accounting.

## Comparing runs

Run on the same machine with the same workload, resolution, compiler options and
pacing; close unrelated heavy jobs first. Preserve the complete JSON. Compare
counts and checksum before comparing timing distributions. Keep CPU-only and
visual frame timings separate. A passed correctness/resource check does not imply
a 60 FPS performance pass. The engine plan's 1920×1080/16.7 ms goal remains a
measured workload-specific target, not a hardware guarantee.

Source identity is captured when CMake configures: short Git HEAD plus `-dirty`
for tracked modifications, or `unknown` without Git. Reconfigure after changing
revisions. Untracked files are not included in that marker; version the workload
before publishing a baseline. `--label TEXT` adds a bounded, JSON-escaped run label.

## Initial reference measurements: 2026-10-08

These baselines were collected from committed source `86d5f8c`, GCC 16.2.1 `-O3
-DNDEBUG`, on the testing Intel i5-4300U / HD Graphics 4400 PC, Mesa OpenGL 4.6
26.2.2. Compilation and automated tests had finished before these runs. Each uses
three cycles, 60 warmup steps per cycle and 600 measured steps per cycle (1,800
samples), without screenshots. The desktop compositor remained active.

| Report | Interval | Median ms | p95 ms | p99 ms |
| --- | --- | ---: | ---: | ---: |
| [Combined 1080p](baselines/2026-10-08-combined.json) | Frame including preview/HUD/present | 3.786 | 8.875 | 17.981 |
| Same report | CPU offscreen render submission | 1.954 | 3.082 | 4.588 |
| Same report | GPU offscreen rendering | 1.513 | 1.692 | 1.741 |
| [256 dense bodies](baselines/2026-10-08-collision.json) | Collision simulation | 2.394 | 6.186 | 8.159 |
| [16 mixer voices](baselines/2026-10-08-mixer.json) | Offline audio block | 0.172 | 0.265 | 0.291 |

The combined workload uses the default counts and a 1920×1080 offscreen target;
the window manager gave the preview a 1050×1360 window. VSync disable was accepted.
One final GPU measurement remained pending, so GPU percentiles contain 1,799
samples rather than 1,800. The frame p99 exceeds the provisional 16.7 ms target;
these results do not establish consistently paced 60 FPS. The report separates
GPU workload time from complete preview/presentation frame time.

All three runs completed every cycle with stable resources and matching per-cycle
checksums. The combined run tracked 8,072,232 CPU buffer bytes, 72,024 report-buffer
bytes, one 16-byte uploaded texture and one 8,294,400-byte target. Its observed RSS
rose from 51,994,624 to 55,193,600 bytes despite logical resource teardown returning
to baseline; driver/allocator retention is outside the tracked-payload assertion.

Reproduce the CPU isolates by setting unrelated counts to zero:

```sh
./build/release/stress_bench --sprites 0 --tiles 0 --entities 0 --bodies 256 \
  --dense --voices 0 --frames 600 --warmup 60 --cycles 3 --report build/dense.json
./build/release/stress_bench --sprites 0 --tiles 0 --entities 0 --bodies 0 \
  --voices 16 --frames 600 --warmup 60 --cycles 3 --report build/mixer.json
```

These are initial comparison baselines, not CI timing thresholds or a promise for
other hardware. Repeat under comparable conditions before attributing a timing
change to engine code; structural counts, capacity failures and resource checks
remain deterministic test gates.

GPU timing defaults to `GL_TIME_ELAPSED` from increment 40. Reports include
`gpu_queries.method` and `counter_bits`. Older reports without these fields used
`GL_TIMESTAMP` pairs. Elapsed mode trusts the driver duration; its zero `invalid`
count does not independently validate the clock. Timestamp diagnostics remain
available through the C++ timer API; see [the timing contract](../docs/FORMATS.md#asynchronous-gpu-timing).

For C++ allocation demand in actual gameplay rendering and the real audio worker,
see [allocation profiling](ALLOCATION_PROFILING.md) and the
[2026-10-24 results](2026-10-24-allocation-results.md). These scopes do not measure
live heap size, C-library allocations or GPU/driver memory.

The [HUD text follow-up](HUD_TEXT.md) replaces recurring label concatenation with
reusable buffers; [measurements](2026-10-24-hud-text-results.md) retain the same
replay scopes and compare rendered output.

The [UI allocation probe](UI_ALLOCATIONS.md) measures settings, controls, diagnostics
and enabled-audio HUD drawing in 18 frozen states, with separate transition costs.
[Results](2026-10-25-ui-allocation-results.md) identify remaining diagnostics allocations.

The [diagnostics text follow-up](DIAGNOSTIC_TEXT.md) removes those recurring
allocations and adds deterministic GPU-label/combined-overlay fixtures.
[Before/after results](2026-10-26-diagnostic-text-results.md) retain matching pixels
and separate menu-transition costs.

The [candidate workload declaration](WORKLOAD_ACCEPTANCE.md) fixes timing and memory
budgets before collection and audits storage exclusions. [Eighteen repeated runs](2026-10-26-workload-acceptance-results.md)
pass memory checks but fail timing acceptance; peak RSS is kept separate from
tracked payloads and allocation demand.

Optional [paired frame samples](STRESS_FRAME_SAMPLES.md) retain CPU intervals and
GPU results by original tag. The [first investigation](2026-10-27-frame-samples-results.md)
locates 15 of 17 traced outliers primarily in presentation and two in submission;
it does not identify the underlying driver/scheduler cause or close acceptance.

Stress mode now supports explicit [immediate/VSync presentation requests](STRESS_PRESENTATION.md).
The [alternating comparison](2026-10-27-presentation-results.md) retains matching
workloads and paired rows, reports request issuance, and keeps physical synchronization
and release acceptance unverified.

Opt-in [submission samples](STRESS_SUBMISSION.md) separate CPU preparation and
bookkeeping from GL setup and batch upload/draw intervals. The
[2026-10-27 on/off collection](2026-10-27-submission-results.md) retains paired
outliers, workload identity, image checks and existing acceptance limits.

[Preparation profiling](STRESS_PREPARATION.md) adds coarse phase wall/thread-CPU
measurements and nested GL accounting. The [on/off collection](2026-10-27-preparation-results.md)
retains signed wall-minus-CPU differences and unmeasured wall time without treating
them as exact scheduler latency or isolated GPU work.

[Context-switch traces](STRESS_CONTEXT_SWITCHES.md) retain calling-thread snapshots
around preparation phases. The [2026-10-28 correlation](2026-10-28-context-results.md)
groups gaps by observed voluntary/involuntary switches without claiming durations
or causes for individual switches.

The [quad-corner experiment](QUAD_CORNER_REUSE.md) compares separate reference and
candidate binaries with a predeclared CPU adoption rule. [Results](2026-10-28-corners-results.md)
reject adoption despite matching pixels; the renderer is restored and all attempts remain.

[Main-thread function sampling](STRESS_FUNCTION_SAMPLING.md) uses an external,
project-local development profiler against the unchanged Release binary.
[Results](2026-10-28-functions-results.md) retain full sample coverage, unknown
symbols, observer effects and frame-limit failures before selecting further work.

The [fixed vertex-buffer experiment](QUAD_FIXED_BUFFER.md) tests direct writes into
bounded storage. [Results](2026-10-28-vertices-results.md) fail the CPU adoption rule;
new boundary tests remain, and the renderer is restored. Further work first examines
run-to-run measurement stability.

The [warmup/order protocol](STRESS_RUN_STABILITY.md) balances short/long runs and
records external read-only host observations. [Results](2026-10-28-stability-results.md)
retain the late frequency/CPU-time transition, observer controls and failed
stability screens. The engine remains unchanged; no system settings are altered.

[Renderer memory accounting](../docs/RENDERER_MEMORY.md) adds an unloaded baseline
snapshot to stress JSON while preserving existing workload/peak fields. Its
[validation](../validation/2026-10-29-renderer-memory/RESULTS.md) covers buffer
lifecycle and image/report identity; it does not revise earlier timing decisions.

[CPU font memory coverage](../validation/2026-10-29-font-memory/RESULTS.md) extends
renderer baselines to coverage v2 with owned atlas/glyph reservations and used
bytes. Committed snapshots and replacement staging remain separate; old v1 reports
and earlier timing/adoption decisions are preserved.
