# Warmup/order stability evidence — October 28 local, October 29 UTC

Neither declared warmup protocol passes its stability screen. All measurements
remain; engine/runtime/test/build sources are unchanged. See the
[protocol](../../benchmarks/STRESS_RUN_STABILITY.md) and
[findings](../../benchmarks/2026-10-28-stability-results.md).

- [manifest.json](manifest.json): declared source, binary/protocol/collector hashes,
  machine, commands, UTC, order, exit status, peak RSS and artifact hashes.
- `cpu-60` and `cpu-600` JSON/logs: independent deterministic CPU-only state references.
- `w60-N`, `w600-N` and `observer-on/off-N` JSON/logs: all sixteen graphics attempts,
  including the late frequency/CPU-time transition and failed frame limits.
- Matching `*-sensors.json`: read-only before/running/after sensor observations,
  explicit read errors, actual monotonic offsets, read costs and static metadata.
- [collect.py](collect.py): predeclared sequence, no retries or system changes.
- [analyze.py](analyze.py), [telemetry.py](telemetry.py), [analysis.json](analysis.json):
  strict workload/trace/aggregate/sensor validation and descriptive comparisons.
- [analyzer_controls.py](analyzer_controls.py), [analyzer-controls.json](analyzer-controls.json):
  three accepted controls and 32 rejected corruptions with matching replacement hashes.
- [commands.json](commands.json), configure/build/tests/collection logs,
  [checks.xml](checks.xml), [checks.json](checks.json): two passing Release groups,
  no skips, eight matching images and unchanged runtime/test/build source audit.

Evidence validity is not performance acceptance. No per-frame sensor join,
frequency normalization, exact scheduler delay or causal thermal conclusion is
made. The ACPI temperature values are implausible and remain uninterpreted.
Package checks follow in the committed record; no fresh full compiler/sanitizer
matrix is claimed for unchanged runtime code.

At documentation commit `4cdeb26`, all three Release package checks pass
without skips (45.73 s). All 71 evidence files and both benchmark documents
match their archived copies; allocation profilers remain excluded. See
[package-feature.json](package-feature.json) and matching configure/build/test logs.

The first merge package attempt at `775622e` fails before publication: CMake
extraction returns zero, but the extracted manifest is entirely zero-filled and
425 listed payloads differ from the valid archive. Independent archive decoding
matches all 1,072 staged payload hashes and the manifest. Including the manifest,
426 extracted files differ; 425 are entirely zero-filled. The exact cause is
unconfirmed; no disk-full or thermal cause is asserted. The invalid package is
rejected by the existing verifier and dependent smoke tests do not run.

A fresh extraction of that same archive to a disk-backed directory outside the
repository passes the unchanged verifier. Using the existing
`ENGINE_PACKAGE_TEST_ROOT` CMake option for that location, all three package tests
then pass at the same merge revision without skips (45.23 s). No source/build
rule, system setting, stress benchmark measurement or acceptance threshold changes.
The failed attempt, manifest reconstruction, archive/file hashes, changed test-root
configuration and successful recheck remain in [package-recheck/inspection.json](package-recheck/inspection.json) and adjacent command/log files.
