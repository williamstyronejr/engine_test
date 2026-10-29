# Reusing diagnostics text storage

Increment 60 removes recurring diagnostic label allocations measured by the
[UI allocation workload](UI_ALLOCATIONS.md). Ordinary gameplay and both rendering
probes pass their existing 256-character HUD string into `Diagnostics::draw_hud`.
The buffer is cleared and reused for each label, with the renderer consuming each
view immediately. No additional persistent text buffer or cached label is added.

The existing drawing overload remains available and delegates through a local
string. It may allocate. The caller-buffer overload can also grow for longer future
content; reservation is a minimum capacity, not a hard truncation limit. Inputs to
`format_text` must not alias its output. Returned views last only until that output
is modified or destroyed, as in the [HUD storage contract](HUD_TEXT.md).

## Decimal formatting

`Fixed2{value}` tells `format_text` to append a double with two fractional digits.
It uses `std::to_chars` with `std::chars_format::fixed`, precision 2 and the same
64-character conversion buffer as the old diagnostics helper. Conversion failure
still displays `OVERFLOW`; there is no partial number or silent truncation. Whole
label storage can grow normally, independently of this preserved numeric limit.
Integer counters continue to use direct integer formatting.

CPU tests check rounding (`12.125` → `12.12`), negative zero, ordinary negative
values, denormals, nonfinite values, overflow, shorter replacement text and repeated
formatting without C++ allocations after reservation. Another case checks delayed
GPU sample ingestion and preservation across empty polls without allocating.
Current live collection delegates to `record_gpu`; the same method accepts the
probe's deterministic presentation data. This is a Feature Lab helper, not an
engine-wide query API change. Samples must remain valid, ordered and no newer than
the caller's current frame, as they are in the production `GpuTimer` path.

## Extended presentation workload

The development-only `diagnostics_allocations` target shares the UI probe's drawing
loop, scope boundaries, 1280×800 target, 300 frames and 30-frame warmup. Its distinct
workload name is `diagnostics_ui_states_v1`; the original `frozen_ui_states_v1`
workload still contains its original 18 states and retains its comparison baseline.

```sh
cmake --preset release
cmake --build --preset release --target hud_text_tests ui_allocations \
  diagnostics_allocations -j2
ctest --preset release -R '^(hud_text|ui_allocations|diagnostics_allocations)$' --output-on-failure
./build/release/ui_allocations build/ui-after.json build/ui-after-images
./build/release/diagnostics_allocations build/diagnostics-after.json build/diagnostics-images
```

The new executable has six cases per level:

| Case | Diagnostic presentation |
| --- | --- |
| `gpu_unsupported` | Unsupported-timing branch |
| `gpu_waiting` | Supported timing, no completed sample, eight pending slots |
| `gpu_completed` | Two completed samples and numeric summary/age |
| `gpu_changing` | Repeated unsupported/waiting/completed phases, with shapes |
| `combined_settings` | Changing diagnostics/shapes behind open settings |
| `combined_controls` | Changing diagnostics/shapes behind remapped controls |

History preparation happens before each measured iteration. Each fixture rebuilds
CPU/GPU presentation state with deterministic values; it does not issue GPU timing
queries. Changing cases repeat three ten-frame phases and cycle numeric values
through zero, 12.125, 9999.5 and maximum finite double. This exercises different
widths, rounding and overflow, then shorter values again. Age and pending/skipped/
invalid counters also vary synthetically. These counters are test inputs, not
observed query failures or evidence about the driver.

The report's `gpu_phases` counts all 300 rendered frames: 100 per phase in changing
cases (90 per phase remain after warmup), or 300 in the one static phase. A
`render_fingerprint` accumulates quad/draw/cull counts across all frames. Changing
cases allow different per-frame geometry counts; fixed cases keep the original
stable-count check. Both preserve gameplay hashes/ticks and verify nonempty healthy
rendering. Optional PPMs capture the final image and, for changing cases, frames
0/10/20: 30 images across the two levels.

Synthetic history creation, image readback, startup, reports and audio-worker work
are outside the measured render-thread scopes. The other UI probe boundaries and
separate opening/rebinding transition counts remain unchanged. These are offscreen
presentation fixtures, not real-time GPU timing or interactive frame-time tests.
The `frame` total must not be interpreted as the ordinary gameplay frame scope.

## Regression coverage and limits

The shared CTest checker now requires zero `diagnostics_submit` allocation calls
after warmup in these reserved-buffer fixtures, in addition to its state/label,
sample-count, positive layout-control and inclusive-counter checks. Other exact
allocation amounts remain reported measurements, not whole-engine budgets.

[Before/after results](2026-10-26-diagnostic-text-results.md) compare three runs of
each workload, all 48 PPM pairs, gameplay state, rendering counts/fingerprints and
unchanged menu-transition costs. Production binaries contain no allocation profiler;
the new probe is not installed. Real timer/platform behavior remains covered by
existing runtime tests; the synthetic cases do not replace physical-device testing.

No speedup, lower RSS, peak-live-heap bound or whole-engine allocation-free behavior
is claimed. Direct C allocation, driver/GPU memory, input/event processing, swap,
live updates, initial reservations/growth, loading, exceptional errors and reporting
retain their exclusions. Declared workload/memory budgets and physical release
acceptance remain separate work.

## Memory overlay follow-up (increment 73)

The F2 renderer memory panel reuses the same string and formatting path. The
[memory contract](../docs/RENDERER_MEMORY.md#f2-memory-panel-increment-73) defines
sampling, units and exclusions. Its additional labels intentionally change
diagnostic images and render counts; the increment-60 comparisons above remain
historical evidence for their original revisions.

Both UI probes now emit `diagnostics_layout:"memory_v1"`. Their workload names and
scenario/timing/allocation boundaries remain unchanged. Treat layout identity as
part of any render-count/fingerprint comparison; older reports lack the marker.
The checker requires the current marker and still enforces zero warmed diagnostic
allocation calls. The validation runner additionally verifies zero allocations
in every measured render-thread scope of the exercised fixtures.

An optional width/height pair follows the screenshot directory; each dimension
must be an integer from 240 through 2560. Defaults remain 1280×800. For example:

```sh
./build/release/diagnostics_allocations build/diagnostics-640.json \
  build/diagnostics-640-images 640 360
```

These are offscreen UI fixtures, including their own render target in the memory
inventory. They do not establish physical display legibility, input behavior or
frame-time acceptance. See the [new results](../validation/2026-10-29-diagnostics-memory/RESULTS.md).
