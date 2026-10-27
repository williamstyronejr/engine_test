# Paired stress-frame validation — 2026-10-27 UTC

Increment 62 adds bounded, opt-in CPU/GPU frame records to stress reports. Measured
revision: **`271053fd1c7c024237e3b2144321327d54d9efd3`**. The
[contract](../../benchmarks/STRESS_FRAME_SAMPLES.md) defines tags, intervals,
missing values and observer/storage cost. [Results](../../benchmarks/2026-10-27-frame-samples-results.md)
explain the 17 traced outliers and the remaining acceptance failure.

The directory uses the October 26 local session date; collection timestamps are
October 27 UTC. CTest XML timestamps use America/New_York local time.

## Compiler and sanitizer checks

| Configuration | Focused groups | Outcome |
| --- | ---: | --- |
| GNU Debug | 8 | All pass, no skips |
| GNU Release | 8 | All pass, no skips |
| Clang Debug | 8 | All pass, no skips |
| CPU ASan/UBSan, leak detection enabled | 5 | All pass, no skips |
| Native ASan/UBSan, driver leak detection disabled | 3 | All pass, no skips |

The native eight-group subset is `stress_cpu`, `stress_report`,
`stress_frame_samples`, `stress_graphics`, `gpu_timer_queue`, `gpu_timers`,
`replay_profile`, `spike_profile`. CPU sanitizer checks omit the three graphics
groups; native sanitizer checks run those three. Both sanitizers halt on error.
These are focused checks, not a refreshed full compiler/sanitizer or race matrix.
The current inventory is 65 native runtime groups (36 CPU, 25 hardware, four
platform), plus three packaging fixtures; platform-disabled inventory remains 34.

[commands.json](commands.json) retains all 15 successful configure/build/test
commands, UTC times, durations, exits and sanitizer overrides. Logs/XML:

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

[checks.json](checks.json) records test counts and four byte-identical plain/traced
PPM pairs, including hashes, sizes and original ignored paths. Native report tests
also compare workload/checksum/render counts, per-frame tags, missing GPU values,
collection tags, sample counts and maxima. The CPU suite exercises 60,000 rows
without allocation after reserve and restores live blocks on destruction.

## Collection and analysis

[manifest.json](manifest.json) records all six attempts, exact argument arrays,
exit status, timestamps, kernel `wait4` peak RSS, thermal readings and report/log,
binary, declaration and collector SHA-256 values. [Environment](environment.json)
records the source, session and working directory. GNU 16.2.1 Release, sanitizers
off, i5-4300U / Intel HD 4400, Mesa 26.2.5-arch1.1, Linux 7.2.8-9-omarchy x86_64.

| Attempt | Report | Log |
| --- | --- | --- |
| plain 1 | [JSON](plain-1.json) | [log](plain-1.log) |
| traced 1 | [JSON](traced-1.json) | [log](traced-1.log) |
| traced 2 | [JSON](traced-2.json) | [log](traced-2.log) |
| plain 2 | [JSON](plain-2.json) | [log](plain-2.log) |
| plain 3 | [JSON](plain-3.json) | [log](plain-3.log) |
| traced 3 | [JSON](traced-3.json) | [log](traced-3.log) |

The standard-library Python collector is retained as `collect.py`. It uses the
fixed original record paths and refuses to overwrite attempts; choose new output
paths before another collection. All planned attempts run regardless of earlier
exit codes. CPU/GPU work is not synchronized for tracing. Temperature/caches/clocks
and desktop activity remain uncontrolled; no timing-based retries are selected.
Kernel peak RSS includes child spawn/exec, startup, teardown and report formatting,
and may include inherited launcher pages. It is not engine-owned heap or VRAM.

`analyze.py` verifies hashes, workload/state/render identity, query accounting,
row/tag alignment, CPU partitions and reconstructed CPU/GPU distributions. It
retains missed-frame counts, largest recorded CPU intervals and ten worst paired
rows per trace in [analysis.json](analysis.json). Its successful exit means the
records are internally consistent; **it is not an acceptance pass**.

From the source repository root:

```sh
python3 validation/2026-10-26-frame-samples/analyze.py \
  validation/2026-10-26-frame-samples
```

All six attempts complete and retain stable resources, but all six frame maxima
exceed 16.7 ms. Prior acceptance failures remain in their original record. The
17 measured traced outliers locate elapsed time, without proving a scheduler,
driver or compositor cause. Physical presentation/audio and broad memory/support
acceptance remain open.
