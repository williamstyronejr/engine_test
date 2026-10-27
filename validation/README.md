# Release validation evidence

Each dated directory records a source revision, toolchain/environment, commands,
exit codes, build/test logs and CTest XML results. These are correctness and build
reproducibility records, not frame-time benchmark results. Performance workloads
and raw measurements normally live in [benchmarks](../benchmarks/README.md).
The allocation follow-up retains its raw reports beside the command/test evidence,
with its measurement interpretation linked from benchmarks.

- [2026-10-23 candidate validation](2026-10-23/RESULTS.md): compiler/sanitizer matrix,
  concurrent audio and a clean build with networking disabled.
- [2026-10-24 UTC atomic-save regression](2026-10-24-atomic-writes/RESULTS.md):
  reproduced syscall-failure bugs, fixes, full GCC and focused Clang/CPU sanitizer checks.
- [2026-10-24 UTC directory preparation](2026-10-24-storage-directories/RESULTS.md):
  ancestor synchronization, creation/failure/retry cases and scoped validation.

- [2026-10-24 allocation profiling](2026-10-24-allocations/RESULTS.md):
  counter tests, gameplay/audio measurements and focused sanitizer/race checks.

- [2026-10-24 HUD text reuse](2026-10-24-hud-text/RESULTS.md):
  formatter/reload regressions, repeated allocation comparison and 17 rendered-image pairs.

- [2026-10-25 UI allocation coverage](2026-10-25-ui-allocations/RESULTS.md):
  settings/controls, diagnostics, enabled-audio labels, transition costs and shared-draw validation.

- [2026-10-26 diagnostics text reuse](2026-10-26-diagnostics-text/RESULTS.md):
  decimal formatting, synthetic GPU presentation, repeated allocation reduction and 48 image pairs.

- [2026-10-26 workload acceptance](2026-10-26-workload-acceptance/RESULTS.md):
  predeclared timing/memory budgets, 18 runs with kernel peak RSS, retained timing
  failures and evaluator controls. This record uses `manifest.json` for measurement
  commands and separate preparation/native command records.

- [2026-10-27 UTC paired stress frames](2026-10-26-frame-samples/RESULTS.md):
  opt-in bounded timing rows, delayed GPU pairing, compiler/sanitizer checks,
  unchanged pixels and six retained plain/traced runs. The directory uses the
  October 26 local session date.

`commands.json` retains commands as argument arrays, explicit environment overrides,
working directory, UTC start, duration and exit status. Other environment facts are
in `environment.log`; the full-matrix run also records its inherited display there.
For the offline check, the source archive hash and temporary build path are in
that run's `offline-source.json`. Regression comparisons retain their own baseline
method and expected failures in the dated results.
Absolute temporary paths describe the original run and may no longer exist; logs
and reports in this directory are the retained evidence.
CTest XML timestamps use the host's local time (America/New_York on this development
host); command timestamps explicitly use UTC. The runner prepended the repository's
installed `.tools/bin` directory to PATH and used two compiler jobs sequentially.

XML reports include output from successful tests as well as failures. Deliberate
fault scenarios can print errors while passing their expected-outcome assertions.
Use the recorded test status and exit code to interpret them. Hardware skips and
failed attempts must remain visible. A passing subset does not stand in for the
full suite, and race instrumentation covers exercised schedules only.

The [release-readiness audit](../docs/RELEASE_READINESS.md) defines the gates and
their limits. Results do not prove physical audibility, every display configuration,
power-loss durability or broad Linux compatibility.
