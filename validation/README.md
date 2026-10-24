# Release validation evidence

Each dated directory records a source revision, toolchain/environment, commands,
exit codes, build/test logs and CTest XML results. These are correctness and build
reproducibility records, not frame-time benchmark results. Performance workloads
and raw measurements live in [benchmarks](../benchmarks/README.md).

- [2026-10-23 candidate validation](2026-10-23/RESULTS.md): compiler/sanitizer matrix,
  concurrent audio and a clean build with networking disabled.
- [2026-10-24 UTC atomic-save regression](2026-10-24-atomic-writes/RESULTS.md):
  reproduced syscall-failure bugs, fixes, full GCC and focused Clang/CPU sanitizer checks.

`commands.json` retains commands as argument arrays, explicit environment overrides,
working directory, UTC start, duration and exit status. Other environment facts are
in `environment.log`; the inherited display is recorded there. The source archive
hash and temporary build path for the offline check are in `offline-source.json`.
Absolute temporary paths describe the original run and may no longer exist; logs
and reports in this directory are the retained evidence.
CTest XML timestamps use the host's local time (America/New_York for the October 23
run); command timestamps explicitly use UTC. The runner prepended the repository's
installed `.tools/bin` directory to PATH and used two compiler jobs sequentially.

XML reports include output from successful tests as well as failures. Deliberate
fault scenarios can print errors while passing their expected-outcome assertions.
Use the recorded test status and exit code to interpret them. Hardware skips and
failed attempts must remain visible. A passing subset does not stand in for the
full suite, and race instrumentation covers exercised schedules only.

The [release-readiness audit](../docs/RELEASE_READINESS.md) defines the gates and
their limits. Results do not prove physical audibility, every display configuration,
power-loss durability or broad Linux compatibility.
