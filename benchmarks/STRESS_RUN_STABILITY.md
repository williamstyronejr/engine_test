# Stress warmup and run-order stability

Increment 70 measures the unchanged renderer before another optimization trial.
Both rejected optimization gates and every earlier failed acceptance result stand.
Commit this protocol and collector before timed collection. Use one GNU Release
binary from that commit, with the same flags and all four existing stress traces.

## Declared sequence and interpretation

Keep the default combined workload, seed 1, 1920×1080 target, immediate request,
three cycles and 600 measured frames per cycle. Compare 60 (short) and 600 (long)
warmup steps per cycle. Run six adjacent pairs: short/long, long/short, short/long,
long/short, short/long, long/short. Both lengths occupy each position three times.
All twelve processes have external telemetry enabled. Then run four observer
controls at short warmup: telemetry off/on, on/off. Keep every attempt; no retries,
timing-based exclusions, cooldown selection or threshold changes. Run sequentially
without concurrent builds, tests, screenshots or function profilers.

Warmup advances simulation. This is a comparison of two complete protocols, not
identical simulation states or an isolated causal warmup effect. Before graphics
collection, run the same binary CPU-only once at each warmup length; retain these
deterministic state/collision references. Check graphics checksums and collision
totals against the corresponding CPU reference; check equal geometry, batch counts,
buffer payloads and resources across both protocols. Keep warmup-specific frame
tags and presentation counts (1,980 or 3,600), with 1,800 measured rows in each run.

Primary diagnostic: median per-frame sum of phase thread CPU minus nested GL
thread CPU. Report each run, each cycle, and first/last 100 measured frames of each
cycle. For each warmup length report the six-run median spread:
`100 * (max(run medians) - min(run medians)) / median(run medians)`.
Also report absolute cycle-1 to cycle-3 median change relative to cycle 1, for
every run. Predeclare a local stability screen: spread ≤5% and every cycle change
≤5%. This is not a confidence interval, optimization adoption rule or release
budget. Compare pair changes and first/second-position distributions descriptively.
The four observer controls report on/off changes without claiming an overhead
estimate immune to time/order effects. No new renderer candidate is adopted here.

Keep frame p99/max and 16.7 ms failures separate from preparation CPU. Preserve
signed wall-minus-CPU gaps, switch categories, missing GPU observations and memory
scope limitations. Existing release and physical display/audio gates remain open.

## Read-only metadata

Record CPUFreq policy identity, driver, governor, limits and related CPUs; thermal
zone types; available core/package throttle counters; parent affinity/priority;
kernel, UTC, commands, build/binary/artifact hashes and child peak RSS. Record
configuration and dynamic sensors before launch and after exit in every run.
Telemetry-on additionally reads dynamic sensors approximately every 250 ms in
the collector process, sleeping between reads; retain actual monotonic offsets,
read durations, errors and collector CPU time. No governor, affinity, priority,
desktop, fan or power settings are changed. No privileged counters are required.

Dynamic sensors are each policy's `scaling_cur_freq` and `cpuinfo_avg_freq` (kHz),
thermal-zone `temp` (millidegrees Celsius), and available throttle-count files.
Unavailable files/read errors remain explicit null/error records, never zero.
Retain discovery lists even if empty. Reads are sequential, not atomic. Sensors
cover process startup, warmup, measurement, reporting and teardown, not individual
frames; do not invent frame joins or a frequency-normalized CPU metric. No engine
thread CPU placement is inferred from collector affinity. Policies may share cores;
throttle counters are not summed across logical CPUs.

CPUFreq values can represent requested rather than exact executing frequency;
readings cannot establish a thermal or scheduling cause for a slow frame. See the
[kernel CPUFreq documentation](https://www.kernel.org/doc/html/latest/admin-guide/pm/cpufreq.html).
Temperature and counter observations likewise do not exclude power limits or
unobserved transient events. Report missing sensors and observed counter deltas.

## Reproduction and validation

```sh
python3 validation/2026-10-28-stability/collect.py build/release/feature_lab
python3 validation/2026-10-28-stability/analyze.py validation/2026-10-28-stability
```

Collection refuses existing outputs. An optional second argument selects a fresh
directory containing a copy of the collector. Build the declared revision first.
Validate all trace partitions, counters, aggregates, workload identities, sensor
shape/time ordering and hashes; exercise corrupted-evidence rejection on isolated
copies. Run existing Release stress graphics/frame-sample tests, then three package
checks on feature and merge commits and compare archived evidence bytes. The
engine and runtime sources do not change, so no new compiler/sanitizer matrix is
claimed for this documentation and external measurement increment.
