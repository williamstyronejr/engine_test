# Read-only power/thermal evidence — 2026-11-02

Measured source: **`fea2f57`**. Runtime code is unchanged from merge `3adc6d2`.
The bounded sequence does not reproduce sustained low frequency; causal attribution
remains unresolved. See the [protocol](../../benchmarks/POWER_THERMAL_OBSERVATION.md)
and [findings](../../benchmarks/2026-11-02-power-thermal-results.md).

- [Manifest](manifest.json): source/binary/protocol/collector hashes, exact phase
  order, commands, exits, process resource observations and artifact hashes.
- Matching `*-observations.json` files: every sweep, read duration, integer/text
  values and explicit errors across 131 paths, including the idle observation windows.
- `cpu-reference` and six `graphics-N` reports/logs: all completed workloads,
  retained frame/preparation/submission/context traces and resource inventories.
- [Commands](commands.json), [environment](environment.log), [tests](checks.xml),
  [complete output](tests-full-output.log): reused Release preparation and three
  passing stress groups without skips. The original analysis exits 1; this record
  is retained and does not imply a workload failure or passing analyzer result.
- [Initial analysis failure](analysis.log), [correction](analysis-correction.json),
  [corrected output](corrected-analysis.log), [analysis](analysis.json): one wrong
  schema label corrected from `graphics` to `opengl`, then the same captures validated.
  Original analyzer source is retained in commit `fea2f57`; no collection retry.
- [Controls](controls.json): two valid inputs accepted and 17 corrupt inputs rejected
  on isolated copies with updated hashes. This checks analysis, not causal attribution.
- [Before profile](before-profile.log), [after profile](after-profile.log),
  [kernel journal](before-kernel-journal.log), [after kernel journal](after-kernel-journal.log),
  [thermal service journal](before-thermal-journal.log): filtered read-only context.
  No network/device serial logs or physical device tests are collected.
- [Collector](collect.py), [analyzer](analyze.py), [control harness](controls.py),
  [runner](run.py): installed Python/toolchain only, disk-backed private scratch.

The original runner stops on its analyzer failure. Reproduction from the final
corrected source runs through analysis and controls directly; the historical
correction commands retain why that run took two stages. Preserve old output and
use an empty evidence directory plus a new scratch path for another collection.
The schema checks do not establish validity of implausible firmware temperatures.

No engine/runtime/test/build rule, system setting, benchmark limit or acceptance
threshold changes. This is not a fresh compiler/sanitizer matrix or the full
69-group suite. Existing timing and physical/memory/endurance/support gates remain
open. Candidate and final-merge packages are verified separately below.

## Candidate package

At documentation commit **`2666cd2`**, all three Release package groups pass
without skips. All 58 then-current evidence/documentation files match archived
bytes. The audit verifies 1,277 relative file links, 78 shell recipes and
eight bounded CPU entry-point commands between two successful manifest checks.
Allocation profilers and session tests remain excluded. These checks validate
packaging/command correctness and do not change the failed timing decisions.

The final merge receives the same three package tests and complete archive audit
after this evidence is included. Final local records live under
`build/power-thermal-refresh/`; package builds reuse the Release preset tree.

[Package status](package-feature-tests.xml), [commands](package-feature-commands.json),
[test log](package-feature-tests.log), [archive audit](package-candidate/summary.json).
