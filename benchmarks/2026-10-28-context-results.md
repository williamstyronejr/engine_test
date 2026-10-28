# Stress context-switch results — 2026-10-28 UTC

Measured source: **`3526150ce9426013ce6f4aac08f4aedab53f01f3`**. The [protocol](STRESS_CONTEXT_SWITCHES.md) was committed before collection. All six attempts completed, with validated original timing rows, phase partitions and cumulative context-switch snapshots. The [validation record](../validation/2026-10-28-context/RESULTS.md) retains every attempt.

Collection: 2026-10-28T03:17:14.143670+00:00 through 2026-10-28T03:18:10.922257+00:00. GNU Release, sanitizers off; full build/machine/driver identity is in each report. The target is fixed at 1920×1080 and the managed preview was 1050×1360. All runs issued immediate-presentation requests; physical synchronization was not verified.

Baseline enables frame, submission and preparation traces; context adds per-thread counter snapshots. Each process runs three cycles of 60 warmup plus 600 measured frames. There are no screenshots or concurrent benchmark jobs during collection. CPU frequency, thermals and other session activity are uncontrolled; the on/off observations do not establish instrumentation overhead or a performance improvement.

## Paired correlation

Across 5,400 context-traced frames, **78** have a measured non-GL preparation wall-minus-CPU gap above the predeclared 1 ms diagnostic threshold. Their observed switch categories are: 0 none, 0 voluntary only, 76 involuntary only, and 2 both.

Involuntary switches were observed in 1,015 frames overall; 937 of those had
preparation gaps at or below 1 ms. Thus the association supports treating thread
execution interruptions as relevant evidence, without treating a switch count as
a duration estimate or assuming every switch produces a large gap.

These counters span the whole phase, including GL and clock reads, and slightly wider boundaries than the timed phase. A switch beside a gap is an association; it does not identify a blocked resource, runnable-wait duration, or the cause of the whole gap. Counters cannot assign a switch to GL versus non-GL work. No observed switch does not prove uninterrupted execution. The original signed timing gaps are preserved.

The table groups measured frames by any switches observed across their four recorded spans. Counts omit events outside those spans. Timing columns describe summed non-GL preparation wall-minus-CPU gaps in milliseconds. Empty groups have no timing distribution.

| Context run | Observed category | Frames | Voluntary switches | Involuntary switches | Gap >1 ms | Gap median | Gap p99 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | none | 1413 | 0 | 0 | 0 | 0.039 | 0.112 |
| 1 | voluntary_only | 5 | 5 | 0 | 0 | 0.071 | 0.085 |
| 1 | involuntary_only | 376 | 0 | 578 | 8 | 0.078 | 1.468 |
| 1 | both | 6 | 6 | 16 | 2 | 0.675 | 1.146 |
| 2 | none | 1410 | 0 | 0 | 0 | 0.041 | 0.123 |
| 2 | voluntary_only | 0 | 0 | 0 | 0 | — | — |
| 2 | involuntary_only | 390 | 0 | 681 | 29 | 0.088 | 6.712 |
| 2 | both | 0 | 0 | 0 | 0 | — | — |
| 3 | none | 1557 | 0 | 0 | 0 | 0.035 | 0.088 |
| 3 | voluntary_only | 0 | 0 | 0 | 0 | — | — |
| 3 | involuntary_only | 243 | 0 | 498 | 39 | 0.151 | 4.642 |
| 3 | both | 0 | 0 | 0 | 0 | — | — |

Per-phase category distributions, whole-phase gaps including GL, the ten largest submissions per run, and submission-dominant slow frames are retained in [analysis.json](../validation/2026-10-28-context/analysis.json). The next table uses the single largest submission in each context run; all values within a row belong to the same frame.

| Context / frame | Whole frame | Submission | Preparation wall | Preparation CPU | Gap | Voluntary | Involuntary |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 / 254 | 16.638 | 11.931 | 8.687 | 3.323 | 5.364 | 0 | 4 |
| 2 / 345 | 25.397 | 19.324 | 18.635 | 4.051 | 14.584 | 0 | 3 |
| 3 / 1279 | 19.569 | 14.119 | 12.813 | 3.394 | 9.419 | 0 | 5 |

Counter system calls lie outside phase timing, inside the enclosing render call. Their overhead and inter-phase work remain in the explicit unmeasured wall remainder; no CPU duration is invented for it. Calling-thread CPU excludes driver worker threads. CPU/GPU timings overlap and are not additive.

## Full frame observations

All values are milliseconds. The 16.7 ms p99 and maximum reference limits are unchanged. The 1 ms gap grouping above is diagnostic only.

| Run | Median | p95 | p99 | Maximum | Frames >16.7 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| baseline 1 | 3.778 | 7.751 | 10.247 | 30.131 | 2 |
| context 1 | 3.844 | 7.468 | 9.858 | 33.700 | 5 |
| context 2 | 4.080 | 7.716 | 11.165 | 30.223 | 8 |
| baseline 2 | 3.872 | 7.599 | 10.357 | 29.417 | 1 |
| baseline 3 | 3.812 | 8.636 | 12.421 | 26.616 | 9 |
| context 3 | 3.475 | 8.806 | 11.647 | 40.148 | 7 |

6/6 p99 values and 0/6 maxima meet the reference. Earlier failures remain retained; these diagnostics do not replace the six-workload acceptance declaration.

## Correctness, resources and validation

All runs match source/build/machine, workload, viewport, checksum `3573445382734126645`, render totals and non-diagnostic payloads. Each process has 1,980 presentation calls and 1,800 measured rows. Each measured frame submits 10,240 visible quads across three batches and uploads 1,966,080 vertex bytes, with 2,048 sprites culled. Original CPU/GPU distributions reconstruct, GL intervals reconcile and counter strings preserve full unsigned 64-bit values.

Context rows add 244,800 bytes for 1,800 samples. Existing frame, submission and preparation rows retain 158,400, 100,800 and 244,800 bytes respectively. Timing vectors remain 100,824 bytes; workload buffers remain 8,072,232 bytes. Texture/target payloads remain 16 / 8,294,400 bytes and teardown accounting is stable. Per-child kernel peak RSS ranges from 54.97 to 55.78 MiB, including launch, driver state, report formatting and teardown; it is not an engine live-heap measurement.

Nine focused groups pass in GNU Debug, GNU Release and Clang. Five CPU and four native ASan/UBSan groups pass without skips. Thirty-two image comparisons match byte-for-byte, including immediate and requested-VSync context traces. CPU cases cover bounds, monotonic snapshots across phases/cycles, exact integer strings, disabled/partial reports, flag dependencies and allocation-free recording after reserve. This is a focused matrix, not full release revalidation.

## Next step

Prototype reuse of each quad's four clip-space corners before emitting its six
vertices. This is a code-inspection candidate in
[the renderer](https://github.com/williamstyronejr/engine_test/blob/3526150ce9426013ce6f4aac08f4aedab53f01f3/engine/src/renderer.cpp), which currently repeats corner-to-clip
calculations for repeated triangle vertices; the phase measurements do not prove
that calculation is the hot function. Compare the candidate with the current
path using declared CPU measurements and pixel/transform/culling checks before
adopting it. Evaluate steady CPU cost while continuing to retain the
context-switch-associated tails separately. These observations do not justify a
scheduler setting change or a different buffer-streaming strategy. Preserve prior failures, workload identities and limits. Physical display/audio, broader memory coverage and the final support record remain open; background loading remains deferred.
