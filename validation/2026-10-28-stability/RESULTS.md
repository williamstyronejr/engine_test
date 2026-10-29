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
