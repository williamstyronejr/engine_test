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

- [2026-10-28 fixed vertex-buffer trial](2026-10-28-vertices/RESULTS.md):
  capacity-boundary regressions, 36 matching captures, six paired CPU attempts and
  a failed adoption gate with exact renderer restoration.
- [2026-10-28 function sampling](2026-10-28-functions/RESULTS.md):
  profiler capability failures, six normal/profiled runs, raw main-thread samples,
  symbol attribution, observer effects and nine matching image pairs.
- [2026-10-28 quad-corner experiment](2026-10-28-corners/RESULTS.md):
  reference/prototype correctness, 36 identical captures, six paired CPU attempts,
  retained launch failures and a failed adoption gate with restored renderer.
- [2026-10-28 UTC context-switch correlation](2026-10-28-context/RESULTS.md):
  six retained attempts, exact per-thread snapshots, phase/frame gap groups,
  focused checks and thirty-two identical image pairs.
- [2026-10-27 preparation profiling](2026-10-27-preparation/RESULTS.md):
  six retained on/off runs, coarse phase wall/thread-CPU measurements, nested GL
  reconstruction, signed gaps, focused checks and twenty-four identical image pairs.
- [2026-10-27 submission timing](2026-10-27-submission/RESULTS.md):
  six retained on/off attempts, reconstructed nested CPU partitions, batch/upload
  accounting, focused compiler/sanitizer checks and sixteen identical image pairs.
- [2026-10-27 presentation requests](2026-10-27-presentation/RESULTS.md):
  immediate/VSync metadata and argument checks, eight matching image pairs, focused
  compiler/sanitizer validation and six alternating traced workload attempts.

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

[Warmup/order stability evidence](2026-10-28-stability/RESULTS.md) retains sixteen
graphics attempts, two deterministic CPU references, host sensor observations,
strict analyzer controls and unchanged-runtime Release/package verification.

[Renderer memory validation](2026-10-29-renderer-memory/RESULTS.md) covers logical
buffer/texel accounting, 40 focused compiler/sanitizer groups and 36 reference image
and workload/report comparisons, with initial compiler diagnostics retained.

[CPU font memory validation](2026-10-29-font-memory/RESULTS.md) covers spare
capacity, copy independence, committed/staged ownership and failure retention,
with 32 focused compiler/sanitizer groups and 36 image/report comparisons.

[Diagnostics memory validation](2026-10-29-diagnostics-memory/RESULTS.md) covers
F2 field labels, panel bounds/clip restoration, warmed allocation scopes and
captures across compiler builds and viewport sizes. It preserves the earlier
memory-coverage and performance limits.

[Integrated readiness refresh](2026-10-29-readiness/RESULTS.md) records full
GNU/Clang suites, complete CPU and focused native sanitizers, leak-enabled audio,
a fresh TSan audio build and packaged documentation/CPU entry-point checks. Its
incremental build scope and remaining offline/physical/timing limits are explicit.
