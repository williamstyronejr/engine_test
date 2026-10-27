# Stress preparation and calling-thread CPU profiling

`--stress --frame-samples --submission-samples --preparation-samples` adds bounded,
paired wall-clock and calling-thread CPU measurements to the offscreen render pass.
The new option requires the existing two tracing flags and graphics. Invalid
requests are rejected before replacing an existing report. Gameplay is unchanged.

```sh
./build/release/feature_lab --stress --width 1920 --height 1080 \
  --frames 600 --warmup 60 --cycles 3 --presentation immediate \
  --frame-samples --submission-samples --preparation-samples \
  --report build/preparation.json
```

## Meaning of the measurements

Four coarse phases wrap `begin`, visible tile traversal/submission, the sprite
loop, and `end`. Tile and sprite phases include transforms, validation, culling,
vertex generation and any batch flushes they trigger. They do not isolate those
individual functions or establish a hot instruction. There are no per-sprite timers.

Each phase samples steady-clock wall time outside a nested pair of Linux
`CLOCK_THREAD_CPUTIME_ID` reads. The thread clock counts user and system CPU time
of the calling render thread, excluding other threads and periods when this thread
is not executing. Clock errors fail explicitly; no zero samples are substituted.
Reported `clock_getres` resolution is nominal, not a guarantee of measurement
accuracy. No new library, GPU query, synchronization wait or buffer strategy is added.

Renderer diagnostics optionally sample the same thread clock inside the existing
GL setup and nonempty upload/draw blocks. Per-phase GL values are differences of
these cumulative counters; the first phase starts from zero because `begin`
resets them. Summing phase GL wall times reconstructs the existing submission
split's setup plus upload/draw intervals. Empty flushes record no GL samples.
Wall-only submission diagnostics retain their existing mode. Disabled profiling
performs no new clock reads and allocates no preparation-row payload.

For each phase, subtract nested GL wall time from phase wall time, and nested GL
thread CPU time from phase thread CPU time. These residuals describe preparation
and bookkeeping outside the measured GL blocks. The difference between residual
wall and CPU is **time the thread was not running, plus measurement differences**, not an exact
scheduler-latency measurement. It can include blocking, descheduling, interrupts,
clock/accounting granularity and different sampling boundaries. Keep signed
wall-minus-CPU differences; do not clamp negative values or assume CPU <= wall.
Thread CPU includes diagnostic overhead and excludes driver worker threads.

Camera/target extent calculation and inter-phase measurement bookkeeping are
outside the four phase intervals but inside the wider render call. Report this
unmeasured wall remainder separately; do not invent a CPU duration for it. The
existing frame/submission/GPU/presentation rows remain independent. CPU and GPU
intervals overlap and are not additive. Do not add independent percentiles.

The profiler performs eight thread-clock reads per render for the four phases,
plus two per nonempty GL batch and two for GL setup, alongside phase wall reads.
For the declared three-batch workload this is 16 thread-clock reads per frame.
Instrumentation changes measured execution and potentially GPU scheduling gaps;
the on/off comparison below does not assume negligible overhead.

## Additive report contract

Existing report/frame/submission schemas and row sizes remain unchanged. When
enabled, `preparation_samples` adds schema `stress_preparation_samples_v1`, clock
name `CLOCK_THREAD_CPUTIME_ID`, `resolution_ns`, phase order
`["begin","tiles","sprites","end"]`, and metric order
`["wall_ms","thread_cpu_ms","gl_wall_ms","gl_thread_cpu_ms"]`.
Each row contains the original decimal-string `frame` tag and four `phases` arrays
in those orders. Warmup is profiled but not retained; partial reports retain only
recorded rows, with the same cycle/warmup tag gaps as the older traces.

Storage reserves once, bounds records to 60,000 rows and records without further
allocation. `memory.preparation_sample_buffer_bytes` reports its actual capacity,
separately from older buffers. On the tested 64-bit ABI each row is 136 bytes:
244,800 bytes for 1,800 rows, up to 8,160,000 bytes. These are opt-in diagnostic
payloads, not changes to the original timing-vector or workload budgets.

## Predeclared comparison

Commit this protocol and runtime before collecting six fresh-process attempts.
Baseline uses frame and submission samples; profile additionally enables preparation
samples. All use immediate presentation, default workload counts/seed, a fixed
1920×1080 offscreen target and three cycles of 60 warmup + 600 measured frames.
Order: baseline/profile, profile/baseline, baseline/profile. Retain every attempt
without timing-based retries. Run sequentially, without screenshots or concurrent
benchmark jobs. Record the managed viewport and request issuance separately from
physical display confirmation.

Require the same source, build, machine, geometry workload, checksum, render totals
and non-diagnostic payloads. Validate original tags, 1,980 presentations, 1,800
measured rows, same-clock nesting, phase sums within the render interval, GL wall
reconstruction, batch/upload counts and actual payload sizes. Reconstruct older
CPU/GPU distributions from raw rows. For profiled runs report phase preparation
CPU/wall distributions, signed gaps, the unmeasured wall remainder, and paired
phase values for the ten largest submissions and submission-dominant slow frames.
Preserve missing GPU results. Compare on/off results without asserting causality:
clocks, thermals, compositor and OS scheduling are uncontrolled.

The original 16.7 ms p99 and maximum reference, memory limits and earlier failed
attempts remain in force. This diagnostic collection does not replace the full
six-workload acceptance run or physical display/audio and broader memory coverage.
Native correctness checks include immediate and requested-VSync image comparisons;
physical synchronization is not established by request issuance.
