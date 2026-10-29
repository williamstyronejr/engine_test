# Warmup and run-order stability — 2026-10-28 local / 2026-10-29 UTC

**Neither protocol passes the predeclared stability screen.** Longer warmup does
not establish a repeatable CPU baseline in this session. No renderer optimization
is adopted, no system setting is changed, and both earlier rejected trials stand.
All twelve warmup runs and four observer controls are retained, including the
late shift to slower CPU preparation. The engine, examples, tests and build rules
remain byte-identical to the previous main revision.

## Provenance and validity

The [protocol](STRESS_RUN_STABILITY.md), collector and analyzer were committed at
`455e03b993d5c48d53f0e97d2b29ffa1bb529b7a` before timing. One GNU 16.2.1 Release binary
(`-O3 -DNDEBUG`, sanitizers off) runs throughout. Collection spans
2026-10-29T02:00:21.611741+00:00 through 2026-10-29T02:04:37.082346+00:00; this is October 28
in America/New_York. Directory names use that local date. Hardware remains the
i5-4300U / HD Graphics 4400, Mesa 26.2.5-arch1.1, Linux 7.2.8-9-omarchy, XWayland.

All sixteen graphics attempts exit zero, complete three cycles/1,800 measured
frames, and pass strict trace, aggregate, resource and workload checks. Both
CPU-only state references complete. Warmup 60 has checksum
`3573445382734126645`; warmup 600 has `18020814675797539703`. These differ because
warmup advances simulation; each graphics run matches its corresponding CPU
reference. Both retain 1,396,800 measured collision candidates and no contacts.

Every measured frame retains 10,240 submitted quads, 2,048 culled, three batches
and 1,966,080 uploaded vertex bytes. The target is 1920×1080 and actual managed
preview 1050×1360. Presentation requests are issued, not physically verified;
there are 1,980 or 3,600 presents, including warmup. No run is retried or excluded.

## Run order and CPU stability

The primary metric is the median sum of non-GL preparation thread CPU per frame.
Percentiles use the existing sorted index `floor((n-1)*percent/100)`, including the
six-run median; no rounding is used for the screen. Each row below represents the
same binary. Frequency observations cover the whole process, not measured frames.

| Order / run | CPU median ms | Cycle 1 / 2 / 3 medians ms | Policy 0 frequency median GHz | Package temperature range °C |
| --- | ---: | --- | ---: | --- |
| 1: w60-1 | 1.571400 | 1.611319 / 1.551668 / 1.571400 | 2.127 | 70–81 |
| 2: w600-1 | 1.719381 | 1.855430 / 1.631080 / 1.717480 | 1.895 | 79–88 |
| 3: w600-2 | 1.653505 | 1.880935 / 1.639585 / 1.458267 | 1.896 | 84–90 |
| 4: w60-2 | 1.423444 | 1.501027 / 1.509156 / 1.266079 | 2.594 | 87–91 |
| 5: w60-3 | 1.311218 | 1.358078 / 1.219829 / 1.362427 | 2.594 | 88–90 |
| 6: w600-3 | 1.212917 | 1.251874 / 1.230964 / 1.172035 | 2.594 | 88–91 |
| 7: w600-4 | 1.175119 | 1.158253 / 1.194497 / 1.173499 | 2.594 | 87–91 |
| 8: w60-4 | 1.215844 | 1.348368 / 1.208864 / 1.127317 | 2.594 | 89–92 |
| 9: w60-5 | 1.295836 | 1.287450 / 1.183715 / 1.423807 | 2.594 | 87–91 |
| 10: w600-5 | 3.192176 | 1.323103 / 3.721866 / 3.945548 | 0.798 | 80–91 |
| 11: w600-6 | 4.152042 | 4.251345 / 4.139925 / 4.056583 | 0.798 | 78–84 |
| 12: w60-6 | 4.139169 | 4.057034 / 4.127867 / 4.225052 | 0.798 | 76–80 |

| Warmup | Six-run spread | Largest absolute cycle-1→3 change | ≤5% for both |
| --- | ---: | ---: | --- |
| short | 222.95% | 16.39% | Fail |
| long | 180.04% | 198.20% | Fail |

The adjacent long-minus-short changes are +9.42%, +16.16%, -7.50%, -3.35%, +146.34%, +0.31%.
Order is balanced (three first and three second positions per length), but does
not remove the observed time trend. Run, cycle and first/last-100-frame
statistics are retained in the [analysis](../validation/2026-10-28-stability/analysis.json).
The long protocol changes simulation state and total work; these observations do
not isolate a causal warmup benefit, nor supply a statistical confidence interval.

## Host observations and limitations

All four CPUFreq policies report `intel_cpufreq`, `schedutil`, and 800,000–2,900,000
kHz configured limits at every recorded boundary. Collector affinity is CPUs 0–3,
priority 0; it does not set engine affinity. Boundary configuration is unchanged;
that does not prove no unobserved transient setting change occurred.

During runs 4–9, all four policies' sampled frequency medians are about 2.594 GHz.
During runs 10–12 they are about 0.798 GHz, while preparation medians increase.
This is an observed host-condition confound, not a per-frame causal attribution.
`scaling_cur_freq` can describe requested rather than exact executing frequency;
`cpuinfo_avg_freq` readings are also retained independently. See the
[kernel CPUFreq documentation](https://www.kernel.org/doc/html/latest/admin-guide/pm/cpufreq.html).
No frequency normalization or per-core placement inference is applied.

The `x86_pkg_temp` sensor spans 70–92°C across the graphics runs. CPU 0 package
throttle count rises from 12,247 to 20,346; its core count rises from 3,068 to
4,808. The increments occur before the final low-frequency runs; counters remain
flat during those later runs. These counters do not prove the cause or duration
of the sustained low-frequency state. Shared package counters are not summed
across logical CPUs. All individual paths and boundary deltas are retained.

The `acpitz` zone reports −266 to −264.2°C. These implausible values remain in raw
evidence and summaries, but are not interpreted as machine temperature. There
are no read errors in this collection; the analyzer also accepts explicit
unavailable/error observations rather than substituting zero.

Telemetry-on runs retain 38–113 snapshots. Actual spacing is retained: the largest
interval is 0.3623 s and one sensor sweep takes 0.1121 s elapsed. Reads are
sequential, not atomic, and include startup, warmup, reporting and teardown.
Collector CPU time is 0.091–0.516 s per telemetry-on graphics run, versus
0.008–0.011 s in boundary-only controls. Exit observation can lag process exit by
a polling interval; process duration is not substituted for frame timing.

## Observer controls and frame limits

| Observer run | Preparation CPU median ms | Frame p99 ms | Frame maximum ms |
| --- | ---: | ---: | ---: |
| observer-off-1 | 3.845026 | 14.816933 | 24.169875 |
| observer-on-1 | 3.937957 | 19.443167 | 46.392883 |
| observer-on-2 | 4.381965 | 21.263769 | 56.417992 |
| observer-off-2 | 4.306711 | 17.535355 | 37.232787 |

On-minus-off paired CPU changes are +2.42%, +1.75%. Both controls occur
after the frequency transition. Two pairs do not separate observer cost from
session drift. Boundary-only frequency medians use just two snapshots and must
not be compared as though they sampled the process interior.

| Run | CPU p95 / p99 ms | Frame p99 / max ms | Frames >16.7 ms | Missing GPU rows |
| --- | --- | --- | ---: | ---: |
| w60-1 | 2.146300 / 2.355166 | 10.838037 / 24.251911 | 6 | 0 |
| w600-1 | 2.413494 / 2.651719 | 7.780227 / 11.752922 | 0 | 0 |
| w600-2 | 2.466516 / 2.997032 | 8.942852 / 12.749545 | 0 | 0 |
| w60-2 | 2.109127 / 2.429975 | 11.518135 / 40.020775 | 7 | 0 |
| w60-3 | 1.963374 / 2.321739 | 12.644144 / 48.718561 | 9 | 1 |
| w600-3 | 1.799653 / 1.977269 | 9.732994 / 13.329644 | 0 | 0 |
| w600-4 | 1.708243 / 2.118432 | 10.402978 / 20.894502 | 1 | 1 |
| w60-4 | 1.965404 / 2.509046 | 11.746183 / 46.562552 | 4 | 1 |
| w60-5 | 2.034140 / 2.630568 | 11.077248 / 43.936355 | 3 | 0 |
| w600-5 | 4.986477 / 5.390252 | 12.917895 / 19.992132 | 9 | 0 |
| w600-6 | 5.295839 / 5.587907 | 11.325679 / 17.774553 | 1 | 0 |
| w60-6 | 5.203892 / 5.445497 | 16.791175 / 28.324874 | 19 | 1 |
| observer-off-1 | 4.996892 / 5.315438 | 14.816933 / 24.169875 | 7 | 1 |
| observer-on-1 | 5.113595 / 5.371506 | 19.443167 / 46.392883 | 29 | 1 |
| observer-on-2 | 5.411017 / 5.675924 | 21.263769 / 56.417992 | 29 | 0 |
| observer-off-2 | 5.310098 / 5.596796 | 17.535355 / 37.232787 | 21 | 1 |

Four of sixteen frame p99 values and thirteen maxima exceed 16.7 ms. Of 145 slow
frames, submission is the largest interval in 93, presentation in 46 and preview
in six. This differs from the earlier presentation-dominated session; it does
not erase those results. All 611 preparation wall-minus-CPU gaps above 1 ms occur
in involuntary-only switch spans. Counts are not wait durations or causes, and
signed negative gaps remain intact. Seven measured GPU observations are missing;
none is converted to zero or forced complete by waiting.

Peak child RSS is 54.66–55.30 MiB including startup/teardown. Tracked workload CPU
payload is 8,072,232 bytes; timing vectors 100,824; frame/submission/preparation/
context payloads 158,400 / 100,800 / 244,800 / 244,800 bytes. Texture texels are
16 bytes and target texels 8,294,400 bytes. These inventories exclude renderer
vertex storage and driver/object overhead; the existing broader memory-coverage
gap remains open. RSS is not a live-heap accounting total.

## Verification and next work

Two existing Release groups pass without skips: stress graphics (including the
retained batch-capacity regression) and stress frame samples. Their eight
trace/presentation-mode image comparisons match the plain reference. Three
valid analyzer controls pass (including full-width uint64 counters and entirely
unavailable dynamic sensors), while 32 corrupt report/order/sensor controls are
rejected after updating artifact hashes on isolated copies. No new runtime API,
CLI or schema is added; JSON files are external measurement evidence.

See the [evidence index](../validation/2026-10-28-stability/RESULTS.md). This is not
a fresh full compiler/sanitizer validation of the unchanged engine. Package checks
are recorded separately below after documentation is committed.

Do not use either warmup protocol to claim a stable baseline or revive rejected
optimizations. Timing acceptance remains open. The next increment should extend
renderer memory accounting to explicitly owned CPU vertex storage and known GPU
buffer allocation sizes, keeping driver overhead separate. That addresses a
concrete remaining coverage gap while host-dependent timing remains unresolved.
Physical display/audio and final support records stay open; background loading
remains deferred.

At documentation commit `4cdeb26`, all three Release package checks pass
without skips (45.73 s). All 71 evidence files and both benchmark documents
match their archived copies; allocation profilers remain excluded. See
[package record](../validation/2026-10-28-stability/package-feature.json).

The first merge package attempt at `775622e` fails before publication: CMake
extraction returns zero, but the extracted manifest is entirely zero-filled and
425 listed payloads differ from the valid archive. Independent archive decoding
matches all 1,072 staged payload hashes and the manifest. Including the manifest,
426 extracted files differ; 425 are entirely zero-filled. The exact cause is
unconfirmed; no disk-full or thermal cause is asserted. The invalid package is
rejected by the existing verifier and dependent smoke tests do not run.

A fresh extraction of that same archive to a disk-backed directory outside the
repository passes the unchanged verifier. Using the existing
`ENGINE_PACKAGE_TEST_ROOT` CMake option for that location, all three package tests
then pass at the same merge revision without skips (45.23 s). No source/build
rule, system setting, stress benchmark measurement or acceptance threshold changes.
The failed attempt, manifest reconstruction, archive/file hashes, changed test-root
configuration and successful recheck remain in [package-recheck evidence](../validation/2026-10-28-stability/package-recheck/inspection.json).

A subsequent rebuild at `40b8358` explicitly reports **Disk quota exceeded**
while the compiler writes temporary assembly under `/tmp`; compilation stops before
package tests. The failed configure/build commands and logs are retained beside
[compiler-temp.json](../validation/2026-10-28-stability/package-recheck/compiler-temp.json). Follow-up validation also sets the compiler's
`TMPDIR` to the disk-backed scratch area. This is a per-command environment
override, not a source or system-configuration change. The later quota diagnostic
is confirmed; the earlier zero-filled extraction's exact cause remains unconfirmed.
