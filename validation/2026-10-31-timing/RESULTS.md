# Timing baseline evidence — 2026-10-31

Source **`49280e0`**: all acceptance evidence is valid, but 13 maximum-frame checks
fail. Both warmup protocols fail the 5% stability screen. Engine/runtime/test/build
sources are unchanged. See the [declaration](../../benchmarks/TIMING_BASELINE_REFRESH.md)
and [findings](../../benchmarks/2026-10-31-timing-results.md).

- [commands.json](commands.json), [provenance.json](provenance.json),
  [environment](environment.log): exact source/toolchain, commands, hashes and outcomes.
- [checks.xml](checks.xml), [complete test output](tests-full-output.log): six
  focused Release groups pass without skips; build and configure logs are adjacent.
- [acceptance manifest](acceptance/manifest.json), [policy](acceptance/policy.json),
  [evaluation](acceptance/evaluation.json): all 18 attempts, raw report/log hashes,
  process peaks and unchanged timing/memory decisions. Evaluator exit 1 is expected
  for the measured failures; it is not converted to a passing budget decision.
- [stability manifest](stability/manifest.json), [analysis](stability/analysis.json):
  two CPU references and all 16 graphics attempts, including frequency changes,
  both failed screens, signed gaps, slow intervals and missing GPU samples.
- Matching `*-sensors.json` files preserve read-only boundary/dynamic observations,
  read costs and explicit unavailable/error handling. No per-frame causal joins.
- [controls](stability/analyzer-controls.json): three valid inputs accepted and 32
  corrupted inputs rejected on isolated copies with updated hashes.
- [host before](host-before.log), [host after](host-after.log): process-name/load
  snapshots, not instantaneous or per-frame interference measurements.
- [runner](run.py), [collector](stability/collect.py), [analyzer](stability/analyze.py),
  [telemetry validator](stability/telemetry.py), [control harness](stability/analyzer_controls.py).

The runtime is unchanged from `485de58`. The three copied stability tools are
byte-identical to the prior study. The control harness changes only evidence and
scratch locations. Collection uses disk-backed scratch beneath `/home/zero/.cache`,
installed tools and the reused Release preset tree. No retries, omissions, policy
changes, system-setting changes or new dependencies occur. All timing failures
remain visible. This is not a full compiler/sanitizer or release qualification.

The next investigation addresses the recurring low-frequency state using read-only
host evidence. Physical display/input/audio, broader memory, endurance and support
signoff remain open. Candidate and merge packaging are checked separately below.

## Candidate package

At documentation commit **`e5d9e62`**, all three Release package groups pass
without skips. All 124 then-current evidence/documentation files match archived
bytes. The audit verifies 1,238 relative file links, 78 shell recipes and
eight bounded CPU entry-point commands between two successful manifest checks.
Allocation profilers and session tests remain excluded. These checks validate
packaging/command correctness and do not change the failed timing decisions.

The final merge receives the same three package tests and complete archive audit
after this evidence is included. Final local records live under
`build/timing-baseline-refresh/`; package builds reuse the Release preset tree.

[Package status](package-feature-tests.xml), [commands](package-feature-commands.json),
[test log](package-feature-tests.log), [archive audit](package-candidate/summary.json).
