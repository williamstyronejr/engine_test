# Feature Lab stress workloads

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
- `frame`: visual loop from event polling through presentation, including preview,
  HUD and swap pacing. CPU mode measures simulation plus mixing only.
- `gpu_render`: asynchronous timestamps around the offscreen workload, excluding
  preview/HUD and presentation. Frame tags exclude warmup even when results arrive
  during the next cycle. Missing/pending/skipped/invalid query counts are explicit;
  shutdown never waits for GPU timing results.

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
