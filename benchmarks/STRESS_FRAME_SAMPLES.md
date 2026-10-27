# Paired stress frame samples

`feature_lab --stress --frame-samples` adds opt-in per-frame records to the existing
stress report. This is a diagnostic mode for the combined workload that missed
its [declared timing budget](2026-10-26-workload-acceptance-results.md). The same
flag works in CPU stress. Gameplay replay/spike profiles do not yet retain this
full series; their existing reports are unchanged.

```sh
./build/release/feature_lab --stress --width 1920 --height 1080 \
  --frames 600 --warmup 60 --cycles 3 --frame-samples --report build/frames.json
```

Without the flag, there is no frame-sample vector reservation and neither new JSON
field is emitted. Existing workload identity, timing distributions, resource checks
and rendering remain unchanged. New fields are additive under
`feature_lab_stress_v1`; the optional object identifies `stress_frame_samples_v1`.

## Tags, intervals and missing values

`frame_samples.cpu_scope_order` names seven columns in each row's `cpu_ms` array:
`poll_collect`, `simulation`, `mixer`, `render_submit`, `preview_submit`, `present`,
and `frame`. The first six partition the seventh, within serialization precision.
`poll_collect` covers event handling and asynchronous result collection before
simulation; the other boundaries follow the [stress timing contract](README.md#report-schema-and-timing-scopes).
CPU mode has zero poll/render/preview/present intervals and a frame covering
simulation plus mixing. These zeros mean an absent operation in that mode.

Each row contains:

- `frame`: decimal-string absolute rendered-frame tag, including warmup in its
  numbering. Only measured frames have rows. With 60 warmup and 600 measured
  frames, retained tags are 61–660, 721–1320 and 1381–1980.
- `cpu_ms`: the intervals from that frame's existing clock readings. No extra clock
  reads, presentation calls or GPU synchronization are added. Screenshot readback
  remains excluded; do not capture screenshots during timing collection.
- `gpu_status`: `completed`, `uncollected`, or `not_submitted`.
- `gpu_ms`: the GPU offscreen interval for **this tag**, or JSON `null`.
- `gpu_collected_after`: decimal-string last rendered tag at the poll that delivered
  the result, or `null`. Subtract the row tag to get collection age in rendered
  frames. A final shutdown poll can have age zero. This is not GPU queue latency.

`not_submitted` means begin returned false (unsupported timing or a full query
pool); the existing aggregate query diagnostics distinguish those cases.
`uncollected` means a submitted query has no valid result in this report. It may
still be pending or have been invalidated; do not call it zero-duration work.
A completed zero is preserved as zero. GPU results join by original frame tag,
even across cycle warmup boundaries. Warmup GPU results are excluded from rows.
Duplicate/unrecorded GPU results, invalid durations and nonsequential CPU tags are
rejected. Partial reports retain only recorded rows, without fabricating the rest.

GPU and CPU overlap. The GPU interval excludes preview, HUD and presentation, and
can include scheduling/submission gaps. Do not add it to CPU time or infer that the
largest CPU interval is the underlying cause of a stall. CPU intervals include
OS descheduling and driver back-pressure. Pairing locates observed elapsed time;
it does not measure GPU utilization, physical refresh, input latency or compositor
internals. The strict frame budget concerns the measured frame interval only.

## Storage and observation cost

Storage reserves once before measured frames, with a maximum of 60,000 rows under
the existing CLI bounds. Recording and GPU pairing allocate nothing after reserve;
serialization and report sorting/output happen afterward. On the validated x86-64
ABI each row is 88 bytes: 1,800 rows reserve **158,400 bytes**, and 60,000 reserve
**5,280,000 bytes**. `memory.frame_sample_buffer_bytes` reports actual vector capacity
bytes separately from existing timing-vector `report_buffer_bytes`.

Trace storage is intentionally additional diagnostic memory. It is not silently
folded into the untraced declaration's 128 KiB timing-buffer budget, nor used to
waive that budget. Whole-process peak RSS also includes JSON formatting/output.
The disabled mode still has a small empty collector object but no sample payload.
Appending rows is outside frame timing; pairing delayed results occurs inside
`poll_collect`. These writes and the extra storage can affect scheduling/caches.
Alternating runs can expose variation but cannot prove zero observer effect.

## Validation and collection plan

CPU cases cover zero warmup, cycle boundaries, delayed results, zero duration,
missing/skipped queries, invalid/duplicate tags, partial reports, disabled storage,
60,000 rows, and no allocations during recording/pairing. Report tests cover CLI
opt-in and null values. A native test compares on/off checksums, counters and final
pixels and checks tag/result alignment and aggregate sample counts/maxima.

For the retained investigation, finish compiler/sanitizer checks first. Use the
same combined 1920×1080 workload as increment 61, with three fresh processes each
for plain and traced modes, ordered plain/traced, traced/plain, plain/traced.
Retain every attempt and its exit, exact command, binary/source identity and kernel
peak RSS. No screenshots, retries selected by timing, or concurrent benchmark jobs.

Reconstruct every measured CPU/GPU summary from the raw rows, verify six CPU
intervals sum to each frame, count frames above the existing 16.7 ms threshold,
and retain the worst ten with their paired GPU values and collection ages. Compare
workload/checksum/draw counts and actual viewports before discussing timing.
No previous acceptance failure is erased by this diagnostic collection. Physical
1080p presentation, audio, unmeasured memory paths and R8 remain open.
