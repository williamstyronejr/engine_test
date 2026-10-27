# Preparation profile validation — 2026-10-27 UTC

Increment 65 measured runtime and precollection contract: **`25ed4c843a2be3037d1f1121741d31fd2e88a8be`**. See the [contract](../../benchmarks/STRESS_PREPARATION.md) and [findings](../../benchmarks/2026-10-27-preparation-results.md).

## Compiler and sanitizer evidence

| Configuration | Focused groups | Result |
| --- | ---: | --- |
| GNU Debug | 9 | Pass, no skips |
| GNU Release | 9 | Pass, no skips |
| Clang Debug | 9 | Pass, no skips |
| CPU ASan/UBSan | 5 | Pass, no skips |
| Native ASan/UBSan | 4 | Pass, no skips |

The nine-group subset is `stress_cpu`, `stress_report`, `stress_frame_samples`, `stress_graphics`, `gpu_timer_queue`, `gpu_timers`, `render_targets`, `replay_profile`, `spike_profile`. CPU sanitizers run the five CPU groups with leak detection enabled; native sanitizers run four graphics groups with driver leak detection disabled. Both halt on errors without suppressions. No new CTest group is added: inventory remains 65 native runtime groups plus three packaging checks. This is a focused matrix, not full release revalidation.

[commands.json](commands.json) retains 15 configure/build/test commands, timestamps, durations, exits and sanitizer overrides.

- gcc-debug: [configure](gcc-debug-configure.log), [build](gcc-debug-build.log), [tests](gcc-debug-tests.log), [XML](gcc-debug-tests.xml).
- gcc-release: [configure](gcc-release-configure.log), [build](gcc-release-build.log), [tests](gcc-release-tests.log), [XML](gcc-release-tests.xml).
- clang-debug: [configure](clang-debug-configure.log), [build](clang-debug-build.log), [tests](clang-debug-tests.log), [XML](clang-debug-tests.xml).
- cpu-sanitize: [configure](cpu-sanitize-configure.log), [build](cpu-sanitize-build.log), [tests](cpu-sanitize-tests.log), [XML](cpu-sanitize-tests.xml).
- native-sanitize: [configure](native-sanitize-configure.log), [build](native-sanitize-build.log), [tests](native-sanitize-tests.log), [XML](native-sanitize-tests.xml).

[checks.json](checks.json) verifies 36 group executions and 24 identical PPM comparisons, retaining hashes, sizes and original ignored paths. [environment.json](environment.json) records source, working directory and session. Tests exercise graphics, clock-mode reset, empty/nonempty batches, bounded row allocation, signed differences, same-clock nesting, flag dependencies and prior-report preservation.

## Retained collection

[manifest.json](manifest.json) retains all six attempts, commands, UTC timestamps, exits, kernel peak RSS, raw boundary temperatures and SHA-256 values for reports/logs, executable, collector and declaration. All attempts completed without timing-based retries. The [collector](collect.py) refuses to overwrite original attempt paths; choose new paths for another collection. Its `wait4` peak RSS covers launch/exec, startup, driver state, report formatting and teardown.

| Mode / repeat | Report | Log |
| --- | --- | --- |
| baseline 1 | [JSON](baseline-1.json) | [log](baseline-1.log) |
| profile 1 | [JSON](profile-1.json) | [log](profile-1.log) |
| profile 2 | [JSON](profile-2.json) | [log](profile-2.log) |
| baseline 2 | [JSON](baseline-2.json) | [log](baseline-2.log) |
| baseline 3 | [JSON](baseline-3.json) | [log](baseline-3.log) |
| profile 3 | [JSON](profile-3.json) | [log](profile-3.log) |

[analyze.py](analyze.py) validates source/build/workload identity, hashes, request metadata, raw tags and nested partitions, reconstructs old CPU/GPU distributions and GL wall intervals, and retains phase CPU/wall distributions and paired outliers in [analysis.json](analysis.json). Its zero exit means valid evidence, not release acceptance. Physical synchronization and release acceptance remain explicitly false.

[Control results](analyzer-controls.json) and their [utility](analyzer_controls.py) accept valid and signed-difference records and reject twelve independent mutations. These use isolated copies with updated hashes to test semantic checks: tags, negative/nonfinite values, phase/metric counts, GL nesting/reconstruction, outer wall interval, payload bytes, clock identity and row counts.

```sh
python3 validation/2026-10-27-preparation/analyze.py \
  validation/2026-10-27-preparation
```

Earlier acceptance failures remain recorded. Physical display/audio, broad interactive/error/long-session memory coverage and the final support record remain open.
