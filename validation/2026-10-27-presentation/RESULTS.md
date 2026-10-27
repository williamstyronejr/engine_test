# Presentation-mode validation — 2026-10-27 UTC

Increment 63 adds explicit immediate/VSync requests to stress mode, with separate
request-issuance metadata and presentation-call counts. Measured source:
**`5a6f1b2caa36b0fb4aea2d76acf556f10490d86a`**. The
[protocol](../../benchmarks/STRESS_PRESENTATION.md) was committed before collection;
[results](../../benchmarks/2026-10-27-presentation-results.md) describe the observed
pacing differences and remaining frame-budget failures.

## Compiler and sanitizer evidence

| Configuration | Focused groups | Result |
| --- | ---: | --- |
| GNU Debug | 8 | All pass, no skips |
| GNU Release | 8 | All pass, no skips |
| Clang Debug | 8 | All pass, no skips |
| CPU ASan/UBSan, leak detection enabled | 5 | All pass, no skips |
| Native ASan/UBSan, driver leak detection disabled | 3 | All pass, no skips |

The eight-group subset is `stress_cpu`, `stress_report`, `stress_frame_samples`,
`stress_graphics`, `gpu_timer_queue`, `gpu_timers`, `replay_profile`, `spike_profile`.
CPU sanitizer checks run the five CPU groups; native sanitizer checks run the three
graphics groups. Sanitizers halt on error, without suppressions. This does not
refresh the full compiler/sanitizer or concurrent-audio matrix. The inventory stays
65 native runtime groups plus three packaging checks; no new test group is added.

[commands.json](commands.json) retains all 15 successful configure/build/test commands,
UTC timestamps, durations, exit codes and sanitizer overrides. Logs/XML:

- GNU Debug: [configure](gcc-debug-configure.log), [build](gcc-debug-build.log),
  [tests](gcc-debug-tests.log), [XML](gcc-debug-tests.xml).
- GNU Release: [configure](gcc-release-configure.log), [build](gcc-release-build.log),
  [tests](gcc-release-tests.log), [XML](gcc-release-tests.xml).
- Clang Debug: [configure](clang-debug-configure.log), [build](clang-debug-build.log),
  [tests](clang-debug-tests.log), [XML](clang-debug-tests.xml).
- CPU sanitizers: [configure](cpu-sanitize-configure.log), [build](cpu-sanitize-build.log),
  [tests](cpu-sanitize-tests.log), [XML](cpu-sanitize-tests.xml).
- Native sanitizers: [configure](native-sanitize-configure.log), [build](native-sanitize-build.log),
  [tests](native-sanitize-tests.log), [XML](native-sanitize-tests.xml).

[checks.json](checks.json) records all counts and eight byte-identical PPM comparisons:
default versus explicit immediate and versus requested VSync, in each native build.
Hashes, sizes and original ignored paths are retained. The native test checks mode,
interval, request/legacy flags, warmup-inclusive call counts, state/render identity
and paired rows. CPU tests cover defaults, argument order, duplicate/unknown/missing
options, CPU-only rejection, unavailable-request metadata and old-report preservation.

## Retained comparison

[manifest.json](manifest.json) records six attempts, commands, process exits,
UTC timestamps, kernel per-child peak RSS, boundary temperatures and SHA-256 values
for reports/logs, the executable, collector and declaration. [Environment](environment.json)
records the source/session/working directory. Runtime: GNU 16.2.1 Release,
sanitizers off, i5-4300U / Intel HD 4400, Mesa 26.2.5-arch1.1, XWayland,
Linux 7.2.8-9-omarchy x86_64. CTest XML timestamps use America/New_York local time.

| Mode / repeat | Report | Log |
| --- | --- | --- |
| Immediate 1 | [JSON](immediate-1.json) | [log](immediate-1.log) |
| VSync 1 | [JSON](vsync-1.json) | [log](vsync-1.log) |
| VSync 2 | [JSON](vsync-2.json) | [log](vsync-2.log) |
| Immediate 2 | [JSON](immediate-2.json) | [log](immediate-2.log) |
| Immediate 3 | [JSON](immediate-3.json) | [log](immediate-3.log) |
| VSync 3 | [JSON](vsync-3.json) | [log](vsync-3.log) |

All attempts complete, issue their requests and retain matching source, machine,
workload, checksum, viewport, render counts and payloads. Every run has 1,980
presentation calls, 1,800 measured frames and 1,800 measured GPU results, with no
pending/skipped/invalid queries. All CPU frame/GPU summaries reconstruct from rows;
CPU subintervals sum to their frame. Each mode has 5,400 measured rows across repeats.

The retained standard-library `collect.py` uses these fixed original record paths
and refuses to overwrite attempts; choose new paths for another collection. It
keeps all attempts without timing-based retries. `wait4` peak RSS covers child
spawn/exec, startup, teardown and report formatting, including possible inherited
launcher pages. It does not attribute engine heap or GPU residency.

`analyze.py` verifies report/log hashes, request metadata, workload/state/render
identity, query accounting, per-frame tags/partitions and reconstructed distributions.
[analysis.json](analysis.json) retains counts, paired outliers and explicit
`physical_synchronization_verified=false`, `release_accepted=false` flags.
Its zero exit means valid comparison evidence, not acceptance success. Reproduce
analysis from the repository root:

```sh
python3 validation/2026-10-27-presentation/analyze.py \
  validation/2026-10-27-presentation
```

Immediate mode records nine frames above 16.7 ms; requested VSync records 2,268.
Every run's maximum exceeds that reference interval. The VSync workload intentionally
allows presentation waiting; its results do not replace the declared immediate
release workload or prove physical refresh. Earlier failures, physical acceptance,
broader memory-path coverage and the final support record remain open.

## Feature package verification

At documentation commit `3fd78d9`, all three Release package checks pass without
skips (25.50 s), including relocated OpenGL gameplay. All 40 evidence files and
both new benchmark documents match the archive byte-for-byte. Optional Python
evidence utilities are included; the four allocation profilers remain excluded.
Build identity is GNU 16.2.1, Release, sanitizers off.

[Identity and commands](package-feature.json), [configure](package-feature-configure.log),
[build](package-feature-build.log), [tests](package-feature-tests.log). This follow-up
records that prior archive; its added logs and text are included in later packaging.
