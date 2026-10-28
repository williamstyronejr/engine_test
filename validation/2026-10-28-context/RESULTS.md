# Context-switch validation — 2026-10-28 UTC

Increment 66 runtime and precollection contract: **`3526150ce9426013ce6f4aac08f4aedab53f01f3`**. See [contract](../../benchmarks/STRESS_CONTEXT_SWITCHES.md) and [findings](../../benchmarks/2026-10-28-context-results.md).

## Compiler and sanitizer evidence

| Configuration | Focused groups | Result |
| --- | ---: | --- |
| GNU Debug | 9 | Pass, no skips |
| GNU Release | 9 | Pass, no skips |
| Clang Debug | 9 | Pass, no skips |
| CPU ASan/UBSan | 5 | Pass, no skips |
| Native ASan/UBSan | 4 | Pass, no skips |

The native subset is `stress_cpu`, `stress_report`, `stress_frame_samples`, `stress_graphics`, `gpu_timer_queue`, `gpu_timers`, `render_targets`, `replay_profile`, `spike_profile`. CPU sanitizers run the five CPU groups with leak detection enabled. Native sanitizers run four graphics groups with driver leak detection disabled. Both halt on errors without suppressions. No new CTest group is added: inventory remains 65 native runtime groups plus three packaging checks. This does not refresh full-suite, audio-race or physical coverage.

[commands.json](commands.json) retains all 15 configure/build/test commands, timestamps, exits, durations and sanitizer overrides.

- gcc-debug: [configure](gcc-debug-configure.log), [build](gcc-debug-build.log), [tests](gcc-debug-tests.log), [XML](gcc-debug-tests.xml).
- gcc-release: [configure](gcc-release-configure.log), [build](gcc-release-build.log), [tests](gcc-release-tests.log), [XML](gcc-release-tests.xml).
- clang-debug: [configure](clang-debug-configure.log), [build](clang-debug-build.log), [tests](clang-debug-tests.log), [XML](clang-debug-tests.xml).
- cpu-sanitize: [configure](cpu-sanitize-configure.log), [build](cpu-sanitize-build.log), [tests](cpu-sanitize-tests.log), [XML](cpu-sanitize-tests.xml).
- native-sanitize: [configure](native-sanitize-configure.log), [build](native-sanitize-build.log), [tests](native-sanitize-tests.log), [XML](native-sanitize-tests.xml).

[checks.json](checks.json) retains 36 group executions and 32 identical final-image pairs, including original ignored paths, sizes and hashes. [Environment](environment.json) records source and session. UTC collection dates can differ from CTest XML dates in America/New_York. Tests cover real thread-counter reads, empty/nonempty passes, nesting, monotonic snapshots, exact uint64 JSON, disabled/partial/bounded traces and preservation of old reports on invalid options.

## Retained comparison

[manifest.json](manifest.json) retains every attempt, command, UTC time, exit, per-child kernel peak RSS, raw thermal snapshots and hashes of reports/logs, executable, collector and declaration. The [collector](collect.py) refuses to overwrite original attempt paths; use new paths for another collection. All six attempts completed without timing-based retries. `wait4` peak RSS includes spawn/exec, startup, driver state, reporting and teardown.

| Mode / repeat | Report | Log |
| --- | --- | --- |
| baseline 1 | [JSON](baseline-1.json) | [log](baseline-1.log) |
| context 1 | [JSON](context-1.json) | [log](context-1.log) |
| context 2 | [JSON](context-2.json) | [log](context-2.log) |
| baseline 2 | [JSON](baseline-2.json) | [log](baseline-2.log) |
| baseline 3 | [JSON](baseline-3.json) | [log](baseline-3.log) |
| context 3 | [JSON](context-3.json) | [log](context-3.log) |

[analyze.py](analyze.py) validates identity, hashes, original timing rows/partitions and counter strings/ordering before deriving deltas and category distributions. [analysis.json](analysis.json) retains frame/phase groups and paired outliers. Its zero exit means internally valid evidence; physical synchronization and release acceptance remain explicitly false.

[Control results](analyzer-controls.json) and their [utility](analyzer_controls.py) accept valid and full-width zero-delta snapshots, and reject fourteen independently corrupted records. Controls use isolated copies with updated hashes, exercising source scope, tags, sizes, integer representation/range, monotonicity, counter order and payload bytes rather than only hash mismatch.

```sh
python3 validation/2026-10-28-context/analyze.py \
  validation/2026-10-28-context
```

Counter spans contain GL work and slightly wider boundaries than the timed phases. Deltas neither provide switch durations nor identify blocking causes. Original timing/memory limits and earlier failed attempts remain unchanged; physical and broader memory-path gates remain open.
