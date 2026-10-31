# Current timing baseline — 2026-10-31

**Timing acceptance and both stability screens still fail.** All 18 acceptance
runs meet the 16.7 ms frame-p99 target, but 13 exceed the independent maximum-frame
ceiling. All 30 synchronous operations and all 318 declared memory checks pass.
The balanced warmup study fails the unchanged 5% preparation spread/drift screen.
No engine optimization or host setting change is adopted.

## Provenance and method

The [refresh declaration](TIMING_BASELINE_REFRESH.md), runner and copied stability
tools were committed before collection at **`49280e0`**. Engine, examples, tests
and build rules remain byte-identical to merge `485de58`. GNU 16.2.1 Release uses
`-O3 -DNDEBUG`, sanitizers off, on the i5-4300U / Intel HD 4400 / Mesa 26.2.5 /
Linux 7.2.8-9-omarchy host. The Release preset directory is reused, reconfigured
for this source identity and built before timing. No fresh compiler matrix is claimed.

The [acceptance collection](../validation/2026-10-31-timing/acceptance/manifest.json)
reuses the existing six-workload policy, collector and evaluator unchanged. All
processes exit zero, reports are complete, workload identities and hashes validate,
and evaluator exit **1** records budget failure rather than invalid evidence.
Thresholds, memory ceilings and structural checks are not relaxed. The old
[acceptance failures](2026-10-26-workload-acceptance-results.md) remain valid for
that earlier collection; this is not a controlled historical speedup comparison.

The [stability collection](../validation/2026-10-31-timing/stability/manifest.json)
then runs two CPU references, twelve balanced warmup attempts and four observer
controls. Its collector, analyzer and telemetry validator are exact copies of the
previous study. Only the control harness's evidence/scratch paths change. Runs are
sequential; no concurrent builds/tests/screenshots/profilers are started. Existing
user applications remain running, with process/load snapshots before and after.
Those snapshots do not measure per-frame interference. No retry, cooldown selection,
outlier exclusion or frequency normalization is applied.

## Acceptance workload results

Ranges below are the minimum and maximum of three per-run statistics, **not pooled
percentiles**. Every p99 passes; the last column counts maximum-frame passes.

| Workload | Frame p99 range, ms | Frame maximum range, ms | Max ≤16.7 ms |
| --- | ---: | ---: | ---: |
| Combined stress | 11.483–12.523 | 22.503–43.677 | 0/3 |
| Dense collision | 6.819–9.104 | 13.122–20.771 | 2/3 |
| Mixer capacity | 0.264–0.278 | 0.307–0.433 | 3/3 |
| Facility replay | 9.006–9.209 | 27.826–45.302 | 0/3 |
| Annex replay | 8.939–11.258 | 21.299–44.197 | 0/3 |
| Reload/transition | 9.144–9.748 | 19.441–34.811 | 0/3 |

The worst frame is 45.302 ms in Facility repeat 3. The slowest of 30 verified
reload/transition operations is **3.484 ms**. All 318 memory checks pass, including
per-process peak RSS, the named payload inventories and covered operation staging.
These checks retain the [original coverage limits](WORKLOAD_ACCEPTANCE.md#memory-budgets);
they do not establish whole-engine peak live heap, GPU residency or long-session
leak freedom. The rendering target is 1920×1080 for stress; actual managed gameplay
presentation is not a physical 1080p acceptance test. The evaluator checks viewport
consistency within each workload across repeats.

## Stability and attribution limits

Both protocols fail the predeclared requirement that six-run spread and every
absolute cycle-1-to-3 change be ≤5%. The metric is summed non-GL preparation thread
CPU time, kept separate from wall-clock frame timing.

| Warmup | Six-run spread | Largest absolute cycle change | Screen |
| --- | ---: | ---: | --- |
| short | 68.82% | 17.77% | Fail |
| long | 87.27% | 119.46% | Fail |

| Run order | Preparation CPU median, ms | Cycle 1 / 2 / 3 medians, ms | Policy 0 median GHz | Frames >16.7 ms |
| --- | ---: | --- | ---: | ---: |
| w60-1 | 1.3098 | 1.4812 / 1.2924 / 1.2182 | 2.592 | 6 |
| w600-1 | 1.3526 | 1.3900 / 1.2662 / 1.4040 | 2.494 | 4 |
| w600-2 | 1.5473 | 1.4146 / 1.5390 / 3.1045 | 2.294 | 5 |
| w60-2 | 3.6975 | 3.4816 / 3.8058 / 3.8316 | 0.798 | 3 |
| w60-3 | 3.7751 | 3.6525 / 3.8751 / 3.8389 | 0.798 | 23 |
| w600-3 | 3.8452 | 3.8535 / 3.7906 / 3.8632 | 0.798 | 12 |
| w600-4 | 3.8368 | 3.8072 / 4.2299 / 3.5643 | 0.798 | 8 |
| w60-4 | 3.5551 | 3.6337 / 3.4663 / 3.5826 | 0.798 | 19 |
| w60-5 | 3.5824 | 3.6586 / 3.6725 / 3.4421 | 0.798 | 12 |
| w600-5 | 4.3131 | 3.9460 / 4.3496 / 4.6423 | 0.798 | 7 |
| w600-6 | 3.3924 | 3.3399 / 3.4924 / 3.3568 | 0.798 | 0 |
| w60-6 | 3.6699 | 4.3695 / 3.4030 / 3.5929 | 0.798 | 42 |
| observer-off-1 | 3.8716 | 3.7727 / 3.9958 / 3.8617 | 0.798 | 25 |
| observer-on-1 | 4.1228 | 4.1642 / 4.0005 / 4.2560 | 0.798 | 52 |
| observer-on-2 | 3.2712 | 3.8242 / 3.1798 / 3.1431 | 0.798 | 35 |
| observer-off-2 | 3.2827 | 3.7268 / 3.1755 / 3.2279 | 0.798 | 42 |

Policy 0's sampled median changes from about 2.59 GHz in the first short run to
about 0.798 GHz in later runs, while preparation CPU increases. This is a repeated
host-condition confound, not proof of a thermal/power/scheduler cause. Sensors
cover startup, warmup, measured work and teardown; they are not joined to individual
frames. CPUFreq readings need not represent exact instantaneous execution clocks;
see the qualifications in the [existing protocol](STRESS_RUN_STABILITY.md#read-only-metadata).

Per-run configuration boundaries show no changes. Policy 0 records `intel_cpufreq`,
`schedutil` and 800,000–2,900,000 kHz configured limits. The package temperature zone
spans 64–91°C. CPU 0's package throttle counter increases by 4,487 across recorded
within-run intervals; shared package counts are not summed over logical CPUs.
The counter and temperature observations do not explain the sustained low-frequency
state. The implausible ACPI −266.8 to −264.2°C values remain uninterpreted in raw
evidence. There are no sensor read errors; the validator also handles unavailable
sensors explicitly rather than substituting zeros.

Across the 16 traced graphics runs, seven p99 values and fifteen maxima exceed
16.7 ms. Of **295 slow frames**, the largest measured interval is render submission
in 197, presentation in 95, simulation in two and mixer in one. This describes
intervals, not root causes: submission includes both CPU preparation and GL work,
and wall-time spans may include descheduling. All six missing GPU observations
remain missing; none is replaced with zero or waited on. Trace partitions,
aggregates, counters, deterministic checksums, geometry and resource inventories
pass their existing validator. Peak process RSS spans 54.63–55.04 MiB in this study.

The telemetry-on minus off preparation changes are +6.49% and −0.35% in the two
observer pairs. They do not isolate sensor overhead from time/order effects.
Stability traces and acceptance workloads have different instrumentation and order;
their distributions are not pooled or presented as equivalent measurements.

## Verification and next work

Six focused Release groups pass without skips: stress CPU/report, replay/spike
profile, stress graphics and frame samples. Existing frame-sample checks include
eight trace/presentation-mode image comparisons. The unchanged analyzer accepts
three valid controls and rejects 32 deliberately corrupted evidence copies after
their hashes are updated. All files, source/binary hashes, commands, durations,
exits and read-only observations are retained in the [evidence index](../validation/2026-10-31-timing/RESULTS.md).
No compiler/sanitizer matrix, physical acceptance or endurance claim is added.

Do not use either warmup protocol to adopt an optimization on this host session.
The next bounded investigation is the recurring low-frequency state: collect
read-only power/thermal evidence that can distinguish candidate causes before
another renderer trial. Do not change governor, power limits, fan settings or
acceptance thresholds without a separate decision. Physical display/input/audio,
broader memory, real-time endurance and final support/signoff remain open.

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

See the [package evidence](../validation/2026-10-31-timing/package-candidate/summary.json).
