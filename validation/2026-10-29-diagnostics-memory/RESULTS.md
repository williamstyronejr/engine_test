# Diagnostics memory overlay validation — 2026-10-29

All 28 focused compiler/sanitizer group executions pass without skips. The F2
panel exposes the existing eleven-field inventory with explicit CPU reserved/used
and logical GPU labels. All measured render-thread C++ allocation scopes remain
zero after the 30-frame warmup in the exercised UI fixtures. No timing improvement,
total-memory bound or physical display acceptance is claimed.

Runtime and focused tests are committed at `f5d5311`; the reproducible matrix
source is `1d6ef45`. [Commands](commands.json), [group results](checks.json), CTest
XML/logs and [image hashes](images.json) retain the actual build/run identities.
GNU Debug/Release and Clang run seven groups each; CPU ASan/UBSan runs two, with
leak detection enabled; native ASan/UBSan runs five with driver leak detection
disabled. Both sanitizer configurations halt on detected errors. All builds use
two jobs, and compiler/package temporary storage is on disk outside `/tmp`.
The retained `initial-*` logs are the earlier pre-commit Release smoke check
(five groups), separate from the 28-group committed matrix.

## Semantics and native checks

The [contract](../../docs/RENDERER_MEMORY.md#f2-memory-panel-increment-73) defines
sampling before both F2 panels, KiB rounding, partial pending batches and coverage
exclusions. The CPU label test uses distinct values in every field to check the
mapping, 32-byte and sub-KiB rounding, zero replacements and maximum `size_t`.
Repeated formatting reuses the existing 256-character string without allocations.

The native render-target suite checks disabled diagnostics as a no-op, both panel
bounds and their gap, restored clipping, unchanged committed memory after drawing,
and resource release at 320×240, 640×480, 1280×720 and 1050×1360. Existing renderer
memory tests independently check inventory semantics and lifetimes. The remaining
groups cover rolling diagnostics, both UI allocation workloads and stress-frame
rendering. This is a focused suite, not a fresh full-engine certification.

## Allocation and presentation evidence

Each UI workload runs 300 frames with 270 measured after warmup. All scopes report
zero calls, requested bytes and nonzero samples in every checked scenario. The
original 18-state workload and the 12-state synthetic GPU/combined-menu workload
retain their phase boundaries; opening/rebinding transitions remain separately
measured and are unchanged against the retained original UI reference. Reports
now identify `diagnostics_layout:"memory_v1"`.

The UI reference executable reports revision `d4dbe22`; its executable hash is in
[reference metadata](reference.json). This is the retained pre-panel UI binary,
not a claim of a fresh build at the preceding merge. All 18 gameplay hashes/ticks,
UI state/labels and transition counts match. All 14 images without diagnostics and
their render counts match byte-for-byte. The four original diagnostic images are
intentionally changed and are not counted as matches.

The new synthetic diagnostic fixtures produce 30 images per native build. All
90 Debug/Clang/native-sanitizer comparisons match GNU Release byte-for-byte.
Another 36 stress images match the preceding Release renderer at `1b808aa` across
nine modes in all four native builds. Four further Release viewport runs at
960×540, 640×360, 320×240 and 1050×1360 retain 120 additional captures/hashes and
zero warmed allocations. These checks do not measure physical display behavior.

Selected PNGs are lossless conversions of the original PPM captures. The default
panel shows all fields and scope labels; combined controls correctly draws above
it. Tiny-view geometry stays within the target, while bitmap text becomes difficult
to read. Other existing HUD elements retain their prior scaling. Timing numbers in
these fixtures are synthetic, including deliberate `OVERFLOW` cases; memory rows
come from the actual renderer and include the fixture's offscreen target.

- [Standard memory panel](captures/FACILITY-gpu_completed.png)
- [Combined controls](captures/ANNEX-combined_controls.png)
- [Changing/overflow labels](captures/FACILITY-gpu_changing.png)
- [960×540](captures/960x540.png), [640×360](captures/640x360.png),
  [320×240](captures/320x240.png), [1050×1360](captures/1050x1360.png)

## Reproduction and limits

[run_validation.py](run_validation.py) preserves logs, rejects an existing
`commands.json`, runs builds sequentially and checks every reported skip/failure.
Before running in a fresh evidence directory, preserve pre-change Release binaries
at `build/diagnostics-memory-validation/reference-ui` and `reference-feature-lab`.
Create a private disk-backed scratch directory under `/home/zero/.cache` containing
`compiler-tmp`, and write its `packages` path to
`build/diagnostics-memory-validation/package-root.txt`. The script prepends
`.tools/bin` to PATH. `DISPLAY` must identify a working GLX context; ALSA `null`
is the audio probe device. Local PPMs remain under the scratch directory; the
tracked hash manifest and selected PNGs retain presentation evidence.

Render-thread counters observe C++ allocation requests in the named scopes,
excluding initialization/growth, direct C allocation, driver work, audio workers,
input/events, swaps, screenshot readback and reporting. They are not whole-engine
or total-live-heap coverage. Prior failed timing/stability and optimization trials
remain unchanged. R4/R5 physical checks, R6 timing/coverage and the R8 support
record remain open; background loading is deferred.

## Packaging

Package verification is recorded below after the committed documentation build.
