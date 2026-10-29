# CPU font memory accounting validation — 2026-10-29

All 32 focused compiler/sanitizer group executions pass without skips. All 36
stress images match the previous renderer; corresponding reports preserve workload,
checksum, render counts and existing non-RSS memory values. The coverage marker
advances from v1 to v2 to identify the new CPU font fields. See the
[contract](../../docs/RENDERER_MEMORY.md).

Runtime, tests, contract and validation runner were committed at
`91886bad9a19167aaf849bb55c49538c39966215` before validation. Reference source is
`b284d2e58122eb4f6266de491342b3228c281fcc`. No compiler or runtime failures occur
in this increment's matrix. All earlier failed timing/optimization/package attempts
remain in their original evidence directories and are not reclassified.

## Ownership and staging checks

`BitmapFont::memory()` reports actual atlas/glyph vector capacities and used bytes
for that object. The CPU test deliberately reserves excess capacity, moves the
buffers into a snapshot, and checks that reserved and used bytes differ. Copy and
layout checks retain serialized content and prove that reported capacities belong
to the queried copy. No assumption that copying preserves spare capacity is made.

`Renderer::memory()` now exposes the same four fields for its committed font copy.
Native tests pass an oversized caller snapshot, then replace and destroy the
caller while checking the renderer's memory and layout stay unchanged. Thirty-two
alternating small/built-in replacements check used bytes and committed reservations
against the direct candidate's staging peak, with zero staging bytes remaining.

Direct `set_font()` staging tracks only the new renderer-owned CPU copy and GPU
atlas. Its leases end at publication. Existing committed font storage, caller
buffers and retired stores are not added to these temporary counts. File reloads
can additionally stage a loader-owned candidate; tests verify overlapping candidate
coverage without equating its peak to a committed snapshot or a whole-heap peak.

Active-pass rejection preserves the snapshot with no direct setter staging.
Malformed EFNT reload preserves live memory/layout/pixels and leaves staging clean.
Repeated valid reloads still publish atlas and metrics together. Tests do not
force a real allocator or driver out-of-memory failure. BitmapFont/Impl objects,
lookup storage, allocator metadata, driver overhead and asynchronous retirement
remain outside this buffer inventory.

## Observed unloaded graphics baseline

| Added field | Bytes |
| --- | ---: |
| `cpu_font_atlas_reserved_bytes` | 16,384 |
| `cpu_font_atlas_used_bytes` | 16,384 |
| `cpu_font_glyph_reserved_bytes` | 2,048 |
| `cpu_font_glyph_used_bytes` | 1,408 |

Used bytes are subsets of reserved bytes. These fields describe the renderer's
startup built-in snapshot, not the application's caller font or replacement peak.
CPU-only reports have `observed:false` and zero placeholders for all eleven fields.
The outer report schema stays `feature_lab_stress_v1`; older coverage-v1 records
lack these fields and must not be interpreted as measured zeros. Existing workload,
vertex, texture and target fields keep their meanings and values in this comparison.

## Verification record

| Build | Focused groups | Result |
| --- | ---: | --- |
| debug | 8 | Pass, no skips |
| release | 8 | Pass, no skips |
| clang | 8 | Pass, no skips |
| sanitize | 3 | Pass, no skips |
| sanitize-platform | 5 | Pass, no skips |

The eight groups are fonts, stress CPU/report, renderer memory, stress graphics,
stress frame samples, font graphics and font reload. CPU ASan/UBSan runs the first
three CPU groups with leak checks enabled; native ASan/UBSan runs five hardware
groups with driver leak checks disabled. Both halt on sanitizer findings without
suppressions. This is a focused matrix; it does not replace the full-suite record.
There is no new CTest group: 66 native runtime plus three package groups remain.

- [commands.json](commands.json), configure/build/test logs and XML: exact source,
  commands, environment overrides, UTC, durations, exits and group statuses.
- [checks.json](checks.json): all five configurations and exercised groups.
- [reference.json](reference.json), reference log and reports: previous binary and
  original frame-test script hashes; the reference uses the retained v1 contract.
- [images.json](images.json): 36 matching PPM pairs across nine modes/four builds;
  actual images remain under ignored build directories.
- [report-comparisons.json](report-comparisons.json): preserved workload, state,
  render and existing memory fields, with explicit v1→v2 coverage handling.
- [cpu-reports.json](cpu-reports.json) and copied reports: unobserved v2 renderer
  baselines in GNU Debug/Release, Clang and CPU sanitizer builds.
- [run_validation.py](run_validation.py): sequential matrix using disk-backed
  compiler temporary files and package scratch space outside the source/build tree.

No system settings are changed and no timing gain or complete live-memory budget
is claimed. Existing timing failures, physical display/audio cases and support
coverage remain open. Package checks are recorded after this evidence is committed.

At documentation commit `279bb88`, all three Release package groups pass
without skips (46.74 s). All 78 evidence files and the renderer memory contract
match their archived copies; allocation profilers remain excluded. See
[package-feature.json](package-feature.json) and matching configure/build/test logs.
