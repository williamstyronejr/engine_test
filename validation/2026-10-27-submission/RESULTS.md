# Submission timing validation — 2026-10-27 UTC

Increment 64 runtime and precollection contract: **`e59ec90b15eb0d2c85adb1158ccfe9be5b1b7120`**. See the [contract](../../benchmarks/STRESS_SUBMISSION.md) and [results](../../benchmarks/2026-10-27-submission-results.md).

## Compiler and sanitizer checks

| Configuration | Focused groups | Result |
| --- | ---: | --- |
| GNU Debug | 9 | Pass, no skips |
| GNU Release | 9 | Pass, no skips |
| Clang Debug | 9 | Pass, no skips |
| CPU ASan/UBSan | 5 | Pass, no skips |
| Native ASan/UBSan | 4 | Pass, no skips |

The native subset is `stress_cpu`, `stress_report`, `stress_frame_samples`, `stress_graphics`, `gpu_timer_queue`, `gpu_timers`, `render_targets`, `replay_profile`, `spike_profile`. CPU sanitizers run the five CPU groups with leak detection enabled; native sanitizers run four graphics groups with driver leak detection disabled. Both halt on sanitizer errors, without suppressions. The inventory remains 65 native runtime groups plus three packaging checks; these focused checks do not refresh the whole suite or concurrent-audio coverage.

[commands.json](commands.json) records all 15 configure/build/test commands, timestamps, durations, exits and sanitizer overrides.
- gcc-debug: [configure](gcc-debug-configure.log), [build](gcc-debug-build.log), [tests](gcc-debug-tests.log), [XML](gcc-debug-tests.xml).
- gcc-release: [configure](gcc-release-configure.log), [build](gcc-release-build.log), [tests](gcc-release-tests.log), [XML](gcc-release-tests.xml).
- clang-debug: [configure](clang-debug-configure.log), [build](clang-debug-build.log), [tests](clang-debug-tests.log), [XML](clang-debug-tests.xml).
- cpu-sanitize: [configure](cpu-sanitize-configure.log), [build](cpu-sanitize-build.log), [tests](cpu-sanitize-tests.log), [XML](cpu-sanitize-tests.xml).
- native-sanitize: [configure](native-sanitize-configure.log), [build](native-sanitize-build.log), [tests](native-sanitize-tests.log), [XML](native-sanitize-tests.xml).

[checks.json](checks.json) retains 36 group executions and 16 byte-identical final PPM comparisons, with original ignored paths, hashes and sizes. [environment.json](environment.json) records the source, working directory and session. Tests verify all presentation modes, identical workloads and pixels, nested interval bounds, byte/batch accounting, option rejection and preservation of existing reports. CPU tests also fill 60,000 records without further allocations and exercise invalid durations, partitions, tags, capacity and opt-in JSON.

[Analyzer controls](analyzer-controls.json) accept a valid collection and reject ten
independent mutations: wrong tags, negative/invalid partitions, outer interval
violations, batch/upload counts, row counts, payload bytes, checksum and aggregates.
The [control utility](analyzer_controls.py) runs against isolated copies and updates
report hashes so these exercise semantic validation rather than hash mismatch.

## Retained collection

[manifest.json](manifest.json) retains every attempt, original command, UTC times, exit code, kernel peak RSS, boundary temperatures and SHA-256 values for reports, logs, executable, collector and declaration. Collection is sequential and refuses to overwrite existing attempt paths; choose new paths for another run. All six attempts completed without timing-based retries.

| Mode / repeat | Report | Log |
| --- | --- | --- |
| baseline 1 | [JSON](baseline-1.json) | [log](baseline-1.log) |
| split 1 | [JSON](split-1.json) | [log](split-1.log) |
| split 2 | [JSON](split-2.json) | [log](split-2.log) |
| baseline 2 | [JSON](baseline-2.json) | [log](baseline-2.log) |
| baseline 3 | [JSON](baseline-3.json) | [log](baseline-3.log) |
| split 3 | [JSON](split-3.json) | [log](split-3.log) |

The standard-library [collector](collect.py) uses per-child `wait4` peak RSS, including launch/exec, startup, report writing and teardown. [analyze.py](analyze.py) validates identity, hashes, requests, counts, tags and nested CPU partitions; it reconstructs CPU/GPU distributions and retains paired submission outliers in [analysis.json](analysis.json). A zero analysis exit means internally valid evidence, not release acceptance. Physical synchronization and release acceptance remain explicitly false.

```sh
python3 validation/2026-10-27-submission/analyze.py \
  validation/2026-10-27-submission
```

All existing workload, frame, presentation and GPU timing semantics are preserved. Optional split storage is separate from timing vectors and old frame records. Earlier timing failures and physical/coverage gates remain open.
