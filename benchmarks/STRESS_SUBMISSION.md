# Opt-in stress submission timing

Use `--stress --frame-samples --submission-samples` to split the offscreen render
call into CPU preparation/bookkeeping, GL pass setup, and GL batch upload/draw.
The extra option requires graphics and paired frame samples; CPU-only or unpaired
requests fail before replacing an existing report. Immediate remains the default
presentation mode, and explicit VSync remains available.

```sh
./build/release/feature_lab --stress --width 1920 --height 1080 \
  --frames 600 --warmup 60 --cycles 3 --presentation immediate \
  --frame-samples --submission-samples --report build/submission.json
```

## Measurement boundaries

`Renderer::set_cpu_timing(bool)` is allowed only between passes. Each successful
`begin` resets its `RenderCpuTiming` record. Timing is disabled by default, with
no clock reads on that path. The stress runner enables it only for the offscreen
pass, snapshots it before the preview, then disables it. Empty flushes do not
produce measurements. There are two clock reads per nonempty batch, two for pass
setup and two around the enclosing render call; there is no per-sprite timer.

- `gl_setup`: framebuffer/viewport/state setup through the clear call in `begin`.
- `gl_upload_draw`: sum of nonempty batch blocks from program binding through
  `DrawArrays`, including uniforms, blend state, texture/buffer binding, orphaning
  (`BufferData`), upload (`BufferSubData`) and draw submission.
- `prepare_other`: enclosing render time minus those two measured GL intervals.
  It includes tile traversal, transforms, validation, culling, vertex generation,
  bookkeeping, timer overhead and any scheduling delay outside the GL intervals.
  It is a residual, **not a pure geometry execution measurement**.

All three are CPU wall times. GL-call intervals can include driver work, waits and
OS preemption; they do not establish the cause of a hitch or measure GPU execution.
The existing asynchronous GPU query remains independent, with original-frame
pairing and explicit missing results. No waits, extra queries, buffer strategy,
workload, visual content or presentation behavior are introduced.

`render_ms` encloses only `StressGraphics::render`. The existing `render_submit`
interval is wider: it also includes query begin/end and diagnostic enable/snapshot/
disable work. Compare samples with the same frame tag; do not sum percentiles.
Preview/HUD and presentation retain their existing separate intervals.

## Additive report and storage

Default reports and existing frame rows retain their schemas and payload sizes.
When enabled, `feature_lab_stress_v1` adds `submission_samples` with schema
`stress_submission_samples_v1`, `cpu_scope_order` equal to
`["prepare_other","gl_setup","gl_upload_draw"]`, and `rows`. Each measured row has
its original decimal-string `frame`, three `cpu_ms` values, `render_ms`, `batches`
and `upload_bytes`. Bytes count vertices passed to `BufferSubData`, not orphaned
capacity or driver-owned storage. Warmup is timed but not retained. Partial runs
retain only recorded rows, and sequential tags preserve gaps between cycles.

Storage is separately preallocated for at most 60,000 measured rows. Recording
allocates nothing after reserve. `memory.submission_sample_buffer_bytes` reports
actual capacity, separately from existing timing/frame buffers. On the tested
64-bit ABI this is 56 bytes per row: 100,800 bytes for 1,800 rows, up to 3,360,000.
Disabled runs allocate no submission row payload and omit these optional fields.
Diagnostic bytes are not silently charged to the old timing-vector budget.

## Declared collection

Commit this contract and runtime before collection. Run three fresh processes
with paired frames only (baseline), and three with the additional split enabled,
all requesting immediate presentation. Order: baseline/split, split/baseline,
baseline/split. Use the existing default counts and seed, 1920×1080 offscreen,
three cycles of 60 warmup plus 600 measured frames. Keep all attempts; do not retry
on timing. No screenshots or concurrent benchmark jobs during collection.

Require identical source, build, machine, managed viewport, workload, checksum,
render counts and non-diagnostic payloads. Verify 1,980 presentation calls,
1,800 measured rows, original frame tags, nonnegative nested partitions, batch
counts and upload bytes. Reconstruct existing CPU/GPU distributions from frame
rows. Report each attempt's frame distribution, the split distributions, and
paired details for submission-dominant slow frames plus the ten largest submission
intervals. Retain request outcomes and missing GPU rows. Compare on/off results as
observations with uncontrolled clocks, thermal state and scheduling, not a proven
causal overhead or speedup estimate.

The existing 16.7 ms p99 and maximum reference limits and memory budgets remain
unchanged. These diagnostics do not waive earlier failed attempts or physical
1080p/display/audio and broader memory-path acceptance. Correctness tests also
compare images with explicit VSync, without asserting physical synchronization.
