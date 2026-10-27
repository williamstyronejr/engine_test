# Stress preparation profiling results — 2026-10-27 UTC

Measured source: **`25ed4c843a2be3037d1f1121741d31fd2e88a8be`**. The [protocol](STRESS_PREPARATION.md) was committed before collection. All six fresh-process attempts completed and all nested intervals and existing distributions validate. Every attempt is retained in the [validation record](../validation/2026-10-27-preparation/RESULTS.md).

Collection: 2026-10-27T22:10:49.771276+00:00 through 2026-10-27T22:12:17.867244+00:00. GNU Release, sanitizers off; i5-4300U / Intel HD 4400, with full machine/driver/build identity in each report. Offscreen resolution is 1920×1080; the actual managed preview was 1050×1360. All runs issued an immediate-presentation request; physical synchronization was not verified.

Baseline runs enable frame and submission samples. Profile runs additionally enable preparation samples. Each uses the same default counts/seed and three cycles of 60 warmup plus 600 measured frames. CPU frequency, thermal state, compositor and OS scheduling are uncontrolled. Do not interpret on/off differences as a proven instrumentation cost or speedup.

## Full frame observations

Values are milliseconds; the 16.7 ms p99 and worst-frame references remain unchanged.

| Run | Median | p95 | p99 | Maximum | Frames >16.7 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| baseline 1 | 6.631 | 10.311 | 15.691 | 35.020 | 15 |
| profile 1 | 6.425 | 9.679 | 16.768 | 51.748 | 19 |
| profile 2 | 6.511 | 10.228 | 14.534 | 23.722 | 12 |
| baseline 2 | 6.532 | 9.960 | 17.203 | 38.034 | 21 |
| baseline 3 | 7.262 | 10.853 | 17.626 | 34.268 | 23 |
| profile 3 | 7.305 | 10.757 | 15.554 | 31.916 | 13 |

3/6 p99 values and 0/6 maxima meet the reference. Earlier failed acceptance attempts are preserved; this diagnostic run does not replace the six-workload acceptance declaration.

## Interpretation

The three largest per-process submissions have only 21.1%, 44.8% and 26.6% as
much measured preparation CPU time as preparation wall time. Across the ten
largest submissions in each profiled run, **23 of 30** have a wall-minus-CPU gap
larger than the preparation CPU duration itself. The worst example has 24.129 ms
preparation wall time versus 5.103 ms thread CPU time: a 19.026 ms gap.

This is evidence that the largest elapsed preparation spikes are not simply the
thread executing more geometry work. Waiting/descheduling and measurement-boundary
effects contribute to the gap; this collection cannot identify a particular
scheduler event or blocking call. A geometry optimization alone should not be
assumed to remove these tails. Total phase preparation CPU medians are 3.38–3.72 ms,
with tile and sprite phases both material; they remain candidates for reducing
steady CPU cost through a separately measured experiment.

The unmeasured wall remainder is usually a few microseconds, but its maximum is
1.721 ms in profile 3. That remainder is retained explicitly and has no CPU timing;
the phase totals must not be described as complete render-thread CPU coverage.

## Preparation execution by phase

The following calling-thread CPU values exclude nested GL blocks. Tile and sprite phases include traversal/iteration, transforms, validation, culling, vertex construction and bookkeeping. They identify coarse phases, not individual hot functions.

| Profile run | Phase | CPU median | CPU p99 | CPU maximum |
| --- | --- | ---: | ---: | ---: |
| 1 | begin | 0.009 | 0.022 | 0.177 |
| 1 | tiles | 1.617 | 2.525 | 2.923 |
| 1 | sprites | 1.678 | 2.788 | 3.376 |
| 1 | end | 0.007 | 0.016 | 0.088 |
| 2 | begin | 0.009 | 0.025 | 0.060 |
| 2 | tiles | 1.612 | 2.571 | 2.997 |
| 2 | sprites | 1.654 | 2.720 | 2.960 |
| 2 | end | 0.007 | 0.016 | 0.038 |
| 3 | begin | 0.010 | 0.026 | 0.098 |
| 3 | tiles | 1.740 | 2.603 | 3.113 |
| 3 | sprites | 1.844 | 2.775 | 3.464 |
| 3 | end | 0.008 | 0.021 | 0.043 |

## Paired CPU and wall time

For each process, this table uses its largest **outer submission** interval. Preparation values sum the four measured phases after subtracting nested GL work. Wall minus CPU is signed and includes time not running plus clock/boundary differences; it cannot distinguish blocking from scheduler delay. These values belong to the same frame and must not be added to overlapping GPU intervals.

| Profile / frame | Submission | Preparation wall | Preparation CPU | Wall − CPU | CPU / wall | Unmeasured wall | GPU |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 / 314 | 44.057 | 24.129 | 5.103 | 19.026 | 21.1% | 0.004 | 1.068 |
| 2 / 175 | 17.248 | 11.063 | 4.960 | 6.103 | 44.8% | 0.003 | 1.561 |
| 3 / 115 | 21.055 | 19.897 | 5.302 | 14.596 | 26.6% | 0.003 | 1.613 |

The unmeasured wall remainder covers camera/extent setup and inter-phase diagnostic bookkeeping inside the enclosing render call. No CPU duration is invented for it. Thread CPU includes user/system time and diagnostic overhead on the calling thread; driver worker threads are excluded. Nominal clock resolution is reported separately from accuracy. Raw thermal snapshots are retained without treating implausible sensor values as evidence of throttling.

Across 5,400 profiled frames, 2 summed preparation wall-minus-CPU differences are negative and retained unchanged. Full distributions, per-phase signed gaps, all submission-dominant frames above 16.7 ms and each run's ten largest submissions are in [analysis.json](../validation/2026-10-27-preparation/analysis.json).

## Correctness, resources and coverage

Source, machine, workload, checksum `3573445382734126645`, viewport, render counts and non-diagnostic payloads match. Every measured frame submits 10,240 visible quads in three batches and uploads 1,966,080 vertex bytes; 2,048 sprites are culled. Each process has 1,980 presentation calls including warmup and 1,800 measured rows. All phase GL wall intervals reconstruct the older GL setup/upload-draw split.

Preparation rows add 244,800 bytes for 1,800 records. Frame rows remain 158,400 bytes, submission rows 100,800 bytes, timing vectors 100,824 bytes, and tracked workload buffers 8,072,232 bytes. Uploaded texture and target payloads remain 16 and 8,294,400 bytes, with stable teardown accounting. Kernel per-child peak RSS is 54.96–55.74 MiB; it includes launch, driver state, reporting and teardown, not just engine live heap.

Nine focused groups pass in GNU Debug, GNU Release and Clang Debug. Five CPU and four native ASan/UBSan groups pass, without skips. Twenty-four image comparisons match byte-for-byte. Tests cover signed clock differences, same-clock nesting, bounded allocation-free recording, phase tags and JSON, empty passes, rollover/reset, flag dependencies and old-report preservation. This does not refresh the full compiler/audio/race matrix or physical acceptance.

## Next step

Correlate the paired gaps with render-thread voluntary and involuntary context-switch counters before choosing a tail-latency fix. Such counters show switches, not their duration or blocking cause. Keep steady CPU optimization as a separate experiment with the scalar rendering path as its correctness reference. Keep workload identities, earlier failures and limits; physical display/audio, broader memory-path coverage and the final support record remain open. Background loading remains deferred.
