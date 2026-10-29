# Renderer memory accounting validation — 2026-10-29

All 40 focused compiler/sanitizer group executions pass without skips. All 36
stress images match the pre-change renderer, and the corresponding reports retain
workload, checksum, render counts and all existing non-RSS memory payload fields.
The new API reports explicitly owned storage; it does not close total-memory or
timing acceptance. See the [contract](../../docs/RENDERER_MEMORY.md).

## Implementation and scope

Runtime/test source is `3f60321d431289cfe8b5d5be9048872498038f33`. The initial implementation is
`9c5198d`; its first Debug build fails on an ambiguous `Window` name after including
GLX/X11. Qualifying the test's `engine::Window` fixes compilation; the original
commands, compiler diagnostic and successful reference capture remain under
[initial-build](initial-build/commands.json). No failing runtime test was discarded.
The initial Ninja log also reports recovery from an existing truncated log.

`Renderer::memory()` reads vector size/capacity and committed texture inventories.
The two VBO allocation calls and reported logical size share a constant. Native
checks query `GL_BUFFER_SIZE`, confirm no observational flush, and check deleted
VBO names after four renderer lifetimes. The batch edge test covers 0, 1, 4,095,
4,096, 4,097 and 8,193 quads, rejected/culled geometry, tail pixels and empty reset.
Texture replacement, target resize/release and font swaps check committed payloads
and rollback. GPU figures exclude driver bookkeeping and orphaned storage retained
for in-flight draws; CPU figures exclude font snapshots and allocator overhead.

The graphics stress baseline is sampled before workload assets. Completed-frame
and unload stability checks now include this inventory. CPU-only reports explicitly
mark renderer observations absent. Existing memory fields keep their earlier scope;
no content/save/replay schema changes and no performance optimization are adopted.

## Observed graphics baseline

| Field | Bytes |
| --- | ---: |
| `cpu_vertex_reserved_bytes` | 786,432 |
| `cpu_vertex_used_bytes` | 0 |
| `gpu_vertex_store_bytes` | 786,432 |
| `gpu_builtin_texture_bytes` | 32 |
| `gpu_uploaded_texture_bytes` | 0 |
| `gpu_font_texture_bytes` | 16,384 |
| `gpu_target_texture_bytes` | 0 |

Used CPU bytes are contained within reserved bytes. Zero uploaded/target bytes
here describe the unloaded baseline, not a zero workload peak. Workload textures
and targets still appear in the existing peak fields. None of these logical
payloads is a measurement of physical GPU residency or process live heap.

## Checks and evidence

| Build | Focused groups | Result |
| --- | ---: | --- |
| debug | 10 | Pass, no skips |
| release | 10 | Pass, no skips |
| clang | 10 | Pass, no skips |
| sanitize | 2 | Pass, no skips |
| sanitize-platform | 8 | Pass, no skips |

The ten groups are stress CPU/report plus renderer memory, stress graphics/frame
samples, fonts, texture reload, targets, sampling and general graphics. CPU-only
ASan/UBSan runs the two CPU groups with leak detection enabled. Native ASan/UBSan
runs eight hardware groups with driver leak detection disabled. Both halt on
sanitizer findings; no suppressions are used. This is a focused matrix, not a new
full-suite claim. The native suite now has 66 runtime plus three package groups.

- [commands.json](commands.json), configure/build/test logs and test XML: exact
  source, arguments, environment overrides, UTC, durations, exits and test statuses.
- [checks.json](checks.json): compiler/sanitizer group coverage without failures/skips.
- [reference.json](reference.json), reference log/reports: previous main executable
  and original native comparison script hashes. Reference source is `07f6727`.
- [images.json](images.json): 36 exact PPM comparisons across nine modes and four
  native configurations. Captures remain under ignored build directories.
- [report-comparisons.json](report-comparisons.json), reference/build mode reports:
  independent workload/state/render and existing memory inventory comparison.
- [cpu-reports.json](cpu-reports.json) and copied CPU reports: explicit unobserved
  renderer baseline in GNU Debug/Release, Clang and CPU sanitizer builds.
- [run_validation.py](run_validation.py): reproducible sequential matrix; refuses
  existing command output. The initial failed attempt is retained separately.

Compiler temporary files and package extracts use fresh disk-backed scratch
storage outside the source/build trees, following the prior `/tmp` quota failure.
No system settings are changed. Package checks are recorded after committing this
validation. Earlier timing failures, rejected candidates and the failed warmup
stability screens remain intact. Physical display/audio and broader live-memory
coverage remain open.

At documentation commit `c7db7d8`, all three Release package groups pass
without skips (46.38 s). All 83 evidence files and the renderer memory contract
match their archived copies; allocation profilers remain excluded. See
[package-feature.json](package-feature.json) and matching configure/build/test logs.
