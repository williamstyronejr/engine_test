# Stress presentation requests

The stress benchmark accepts `--presentation immediate` (the default) or
`--presentation vsync`. Both modes render the same offscreen workload, preview and
HUD, then call `Window::present()` once per rendered frame. They request GLX swap
interval **0** or **1**, respectively, through the existing `Window::set_vsync`
API. No CPU sleep, busy wait, `glFinish`, extra GPU query or desktop configuration
change is added. Ordinary gameplay settings are unchanged.

```sh
./build/release/feature_lab --stress --width 1920 --height 1080 \
  --frames 600 --warmup 60 --cycles 3 --frame-samples \
  --presentation immediate --report build/immediate.json
./build/release/feature_lab --stress --width 1920 --height 1080 \
  --frames 600 --warmup 60 --cycles 3 --frame-samples \
  --presentation vsync --report build/vsync.json
```

A VSync request is not proof of synchronization, refresh rate, display latency or
60 FPS. The window API returns true when the extension/function is available and
the request is issued; it does not query the resulting interval or confirm physical
scanout. Unsupported requests leave rendering operational and report failure to
issue the request. Comparisons must retain that outcome. Timing differences can
include compositor/driver pacing and OS scheduling, not just offscreen GPU work.

## CLI and report contract

The default remains immediate. `--no-vsync` remains an immediate-mode alias and is
still accepted by CPU stress. It conflicts with `--presentation vsync` in either
argument order. Duplicate `--presentation` options, missing/unknown values and
explicit presentation modes in CPU-only runs are rejected before replacing a report.
Use default CPU mode or its existing `--no-vsync` alias instead.

Reports retain `feature_lab_stress_v1` and add a top-level `presentation` object:

| Field | Meaning |
| --- | --- |
| `mode` | `immediate`, `vsync`, or `none` for CPU-only runs |
| `swap_interval_requested` | 0, 1, or JSON null for CPU-only runs |
| `request_issued` | The window API returned true; false for unavailable requests and CPU mode |
| `present_calls` | Calls to `Window::present`, including warmup; zero in CPU mode |

`run.vsync_requested` is true only for the VSync mode. The legacy
`run.vsync_disable_accepted` is true only when immediate mode issues its interval-0
request; its historical field name does not establish physical synchronization.
The geometry/simulation `workload` object remains unchanged. Treat **both workload
and presentation metadata** as comparison identity; do not silently pool the modes.

The existing paired CPU/GPU rows and timing boundaries are unchanged. Presentation
request setup precedes the workload; the call counter updates after the frame's
end timestamp. `present` covers the swap call, while `gpu_render` covers only the
offscreen rendering interval, excluding preview/HUD/presentation. A full or
unsupported GPU query pool still produces explicit missing rows. No pending result
is waited on at shutdown. See [frame records](STRESS_FRAME_SAMPLES.md).

## Declared comparison

This protocol is committed before collecting new timings. After compiler/sanitizer
checks, run three fresh processes per mode on the same testing PC, with
`--frame-samples`, the default counts/seed, a fixed 1920×1080 offscreen target,
three cycles, 60 warmup and 600 measured frames per cycle. Alternate mode order:
immediate/VSync, VSync/immediate, immediate/VSync. Keep every attempt; do not retry
based on timing. No screenshots or concurrent benchmark jobs during collection.
The compositor and actual managed viewport remain recorded conditions.

Require matching source, machine, geometry workload, checksum, draw counts and
tracked payloads; confirm 1,980 presentation calls and 1,800 measured frames.
Record request issuance independently. Reconstruct CPU/GPU aggregates from rows,
verify CPU partitions and tag alignment, then report per-mode frame/presentation
median, p95, p99, maximum and counts above 16.7 ms. Preserve missing GPU samples and
avoid summing independent percentiles. Comparison validity requires both requested
modes to issue their requests; otherwise report that limitation without claiming
an effective mode comparison.

The 16.7 ms thresholds remain reference values from the existing declaration, not
new acceptance limits tuned to the result. Immediate mode is the declared release
benchmark; requested VSync is a diagnostic comparison with deliberate potential
waiting. Even favorable results do not erase earlier failures or establish physical
1080p display acceptance. Existing allocation/memory limits and unmeasured paths
retain their previous scopes. Kernel peak RSS includes launch, teardown and report
output; raw row storage remains 158,400 bytes for 1,800 samples on this ABI.

Native checks compare default, explicit immediate and requested VSync workloads,
final pixels, request metadata, presentation counts and paired rows. CPU cases cover
parsing/conflicts, CPU-only rejection, unavailable-request reporting and preservation
of an existing report on invalid input. No new platform or runtime dependency is
introduced.
