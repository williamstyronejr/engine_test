# Diagnostics text reuse validation — 2026-10-26 UTC

Optimized runtime: **`eabf4332e1cb1cfc4c3a50c7123357f4754eb679`**.
Extended-fixture baseline: **`e6533f591ada0ef5e8b36a15ef1911f895af6962`**. The baseline commit
adds synthetic GPU presentation cases before changing the diagnostics formatter.
The original 18-state UI baseline remains the [prior record](../2026-10-25-ui-allocations/RESULTS.md)
at `4e860f5`; runtime source was unchanged through previous main `3a27aff`.

[Baseline commands](baseline-commands.json) retains six clean-source configure,
build, test and repeated-measurement invocations. [Final commands](commands.json)
retains 21 configure/build/test/measurement invocations. All succeed. Both include
argument arrays, explicit environment overrides, UTC timestamps, durations and exit
codes. Existing build directories were reused with explicit compiler/configuration/
sanitizer flags and two build jobs; there is no fresh/offline-build claim.
[Environment](environment.log) retains host information. Runtime source identity
was configured at clean implementation commits before documentation changes.

## Runtime checks

| Configuration | Passed groups | Scope |
| --- | ---: | --- |
| Baseline GNU Release | 1/1 | Extended diagnostic fixture, before optimization |
| GNU 16.2.1 Debug | 64/64 | Every native runtime group, no skips; 136.78 s |
| GNU 16.2.1 Release | 64/64 | Every native runtime group, no skips; 161.35 s |
| Clang 23.1.3 Debug | 8/8 | Diagnostic/UI/gameplay allocation probes, render targets, UI, diagnostics, HUD text, counters |
| GNU CPU ASan/UBSan | 4/4 | HUD text, diagnostics, UI, allocation counters; leak detection enabled |
| GNU graphical ASan/UBSan | 4/4 | Diagnostic/UI/gameplay allocation probes and render targets; driver leak detection disabled |

Six CTest XML reports retain successful-test output. No selected test failed or
skipped. Sanitizers halt on error, with no added suppressions. Graphical checks
use `detect_leaks=0`; CPU checks retain `detect_leaks=1`. These focused checks do
not replace a full native sanitizer/race matrix or physical release acceptance.

The native inventory is now 64 runtime groups: 36 CPU, 24 hardware and four platform,
plus three Release packaging checks. CPU-only discovery stays at 34. The new
`diagnostics_allocations` group exercises 12 states with synthetic timing histories.
The shared checker verifies state, label, phase-count and allocation accounting,
including zero recurring diagnostic allocation calls after warmup. The probe also
checks gameplay preservation and nonempty healthy renders; fixed cases require
stable per-frame geometry, while changing cases retain a render-count fingerprint.

`hud_text` now contains six CPU cases. New coverage checks direct two-decimal
formatting, rounding, negative zero, denormals, infinity/NaN, overflow, replacement
with shorter text and repeated allocation-free reuse. GPU ingestion checks delayed
sample identifiers and preservation across empty polls without allocating. Existing
render-target tests retain coverage of the original drawing convenience overload.
The real timer path still runs in the full runtime and gameplay profile tests.

## Before/after measurements and rendering

All three repeats match within each report set. The original 18-state UI workload
and extended 12-state diagnostic workload preserve their own state, viewport,
gameplay hashes/ticks, rendering data and separate transition costs. Every measured
render-thread scope now records zero C++ allocations after warmup. Original
visible-diagnostics cases drop from 16 calls / 537 requested bytes per frame to
zero; the extended fixtures formerly observed up to 19 calls per frame.
See [measurement results](../../benchmarks/2026-10-26-diagnostic-text-results.md) and
[scope/lifetime contract](../../benchmarks/DIAGNOSTIC_TEXT.md). Nine raw JSON reports
(three extended before, three extended after, three original-workload after) and
their logs are retained here; original-workload before reports remain in their
prior dated directory.

[Comparison evidence](comparison.json) retains old/new diagnostic counters and
matching state/render/transition checks. All 48 PPM pairs are byte-identical:
18 original UI states plus 30 extended captures, including the unsupported,
waiting, completed and overflow branches and combined overlays. Completed GPU
labels and combined controls/overflow captures were also visually inspected.
Image paths, hashes and sizes are retained; raw PPMs remain under ignored build
directories. Exact image-producing commands are in the baseline/final manifests
and the previous UI record. [Provenance](baseline-provenance.json) hashes the copied
baseline executables.

[Symbols](symbols.json) confirms that normal `feature_lab` has no allocation-profile
symbols, while the diagnostic probe defines the scoped global allocation functions.
The probe is a development target and is not installed.

## Limits

Numeric precision and `OVERFLOW` behavior are preserved; whole-label storage can
still grow for longer future content. The convenience overload may allocate.
The new path reuses the existing HUD buffer and adds no persistent text capacity.
No live/peak-heap, RSS, frame-time or speedup conclusion follows from cumulative
requested-byte counters.

Synthetic histories and query counters are presentation fixtures, not measurements
of GPU completion, failures or real-time behavior. History setup, startup, readback,
reports, live updates, event polling, swap and background audio are outside these
offscreen frame scopes. Direct C allocation and GPU/driver storage remain excluded.
Menu-opening/rebinding costs are retained separately and unchanged. Workload/memory
budgets, physical display/audio and the final support/coverage record remain open.
