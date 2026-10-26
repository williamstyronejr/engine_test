# Diagnostics text reuse — 2026-10-26 UTC

Increment 60 reuses the existing HUD string for diagnostic labels and formats
fixed-two-decimal values directly into it. All measured render-thread scopes now
record zero C++ allocations across the 18 original UI states and 12 additional
GPU-label/combined-overlay states. Menu transition costs remain unchanged.
This is a scoped allocation result, not a whole-engine or performance claim.

Optimized runtime: **`eabf433`**. Original UI baseline: `4e860f5`, unchanged in runtime
through previous main `3a27aff`. Extended-fixture baseline: `e6533f5`, which adds
GPU presentation fixtures before changing label formatting. All use GNU 16.2.1
Release, `-O3 -DNDEBUG`, sanitizers off, Intel HD Graphics 4400 / Mesa 26.2.5-arch1.1.

The [storage/workload contract](DIAGNOSTIC_TEXT.md) explains buffer lifetimes,
precision/overflow behavior and the synthetic history exclusions.
[Validation](../validation/2026-10-26-diagnostics-text/RESULTS.md) retains the compiler,
sanitizer and packaging checks. The two workloads have separate identities and are
compared only against their corresponding baselines.

## Same workloads, removed recurring demand

Each row describes one 270-frame measured scenario after 30 warmup frames. Facility
and Annex agree, as do all three repeats. Bytes are cumulative successful C++
requests, not live or peak heap usage. All values below are for `diagnostics_submit`.

| Workload / case | Before calls | Before requested bytes | After calls / bytes |
| --- | ---: | ---: | ---: |
| Original UI: diagnostics | 4,320 | 144,990 | 0 / 0 |
| Original UI: diagnostics + shapes | 4,320 | 144,990 | 0 / 0 |
| Extended: unsupported GPU timing | 4,590 | 161,460 | 0 / 0 |
| Extended: waiting for GPU samples | 4,590 | 159,840 | 0 / 0 |
| Extended: completed GPU samples | 4,860 | 175,770 | 0 / 0 |
| Extended: changing GPU/CPU labels | 4,743 | 169,533 | 0 / 0 |
| Extended: changing labels + settings | 4,743 | 169,533 | 0 / 0 |
| Extended: changing labels + controls | 4,743 | 169,533 | 0 / 0 |

The original diagnostic cases drop from 16 calls / 537 bytes per measured frame to
zero. Extended cases formerly allocate up to 19 times per frame depending on displayed
values; their wider numeric labels are a different fixture, not an increase in the
original workload. Every other measured scope retains its previous zero result,
and the enclosing frame equals the sum of its subscopes. Frame totals must not be
added to their included diagnostic totals.

The original UI workload retains all settings/control/audio states and labels.
Opening settings still requests 5 allocations / 983 bytes; entering remapped
controls requests 16 / 1,884; capture requests 1 / 33; accepting the binding requests
16 / 1,880. Extended combined cases retain their matching open/controls costs.
Those transitions occur before warmup and remain explicitly reported.

Three repeated reports agree in each set. Before/after comparisons preserve
scenario state, gameplay hashes/ticks, final render counts, transitions and the
extended workload's complete render-count fingerprint. Raw reports:
[original UI baseline](2026-10-25-ui-allocation-results.md);
UI after [1](../validation/2026-10-26-diagnostics-text/ui-1.json),
[2](../validation/2026-10-26-diagnostics-text/ui-2.json),
[3](../validation/2026-10-26-diagnostics-text/ui-3.json);
extended before [1](../validation/2026-10-26-diagnostics-text/before-1.json),
[2](../validation/2026-10-26-diagnostics-text/before-2.json),
[3](../validation/2026-10-26-diagnostics-text/before-3.json);
extended after [1](../validation/2026-10-26-diagnostics-text/after-1.json),
[2](../validation/2026-10-26-diagnostics-text/after-2.json),
[3](../validation/2026-10-26-diagnostics-text/after-3.json).

## Preserved rendered output

All 48 before/after PPM pairs are byte-identical at 1280×800: 18 original UI states
and 30 extended-fixture captures. The latter cover static GPU branches, changing
branches and maximum-double `OVERFLOW` output behind settings/controls as well as
standalone diagnostics. The changed drawing path shares the original formatter's
precision, rounding and conversion limit. The CPU cases also test negative zero,
denormals, infinity/NaN and transitions back to shorter strings without allocating.

[Comparison evidence](../validation/2026-10-26-diagnostics-text/comparison.json)
retains old/new counters, matching state/rendering flags and image hashes/sizes.
[Baseline provenance](../validation/2026-10-26-diagnostics-text/baseline-provenance.json)
identifies copied executables. Raw PPMs remain in ignored build directories; the
original UI baseline image hashes are also in its earlier validation record.

## Limits and next work

No timing improvement or memory-footprint reduction is claimed. These reports count
successful C++ allocation requests on the render thread, with startup, synthetic
history setup, readback, report output and background audio outside their scopes.
They do not measure allocator metadata, C allocation, GPU/driver storage or peak
live memory. The convenience draw overload can still allocate; longer future
labels can grow caller storage. Physical display/audio and broad Linux support are
not established by deterministic snapshots or null-device audio.

Next: complete the declared workload/memory acceptance record using these measured
baselines, keeping timing budgets and peak-memory limits separate from allocation
counts. Physical release checks and the final support/coverage record remain open.
