# Stress submission timing results — 2026-10-27 UTC

Measured source: **`e59ec90b15eb0d2c85adb1158ccfe9be5b1b7120`**. The [contract](STRESS_SUBMISSION.md) was committed before collection. All six declared fresh-process attempts completed; all CPU/GPU aggregates and nested render partitions validate. The [retained record](../validation/2026-10-27-submission/RESULTS.md) includes every attempt and the analysis utility.

Collection: 2026-10-27T16:54:56.829254+00:00 through 2026-10-27T16:56:12.917777+00:00. Build: GNU Release, sanitizers off. Machine and driver details are retained in each report; the managed viewport was 1050×1360, with a fixed 1920×1080 offscreen target. All runs requested immediate presentation and issued the request; physical synchronization was not verified.

Three baseline processes use paired frame samples; three additionally enable submission samples. Each has three cycles of 60 warmup and 600 measured frames. Source, machine, workload, viewport, checksum, render totals and existing payloads match. No attempt was discarded or retried for timing. CPU frequency, thermal state, compositor and OS scheduling were not controlled. These observations do not prove instrumentation overhead or a speedup.

## Complete frame observations

All values below are milliseconds. The 16.7 ms reference is unchanged.

| Run | Median | p95 | p99 | Maximum | Frames >16.7 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| baseline 1 | 3.183 | 7.504 | 9.798 | 53.599 | 2 |
| split 1 | 5.402 | 8.974 | 12.457 | 17.993 | 2 |
| split 2 | 6.448 | 10.397 | 13.516 | 19.289 | 1 |
| baseline 2 | 6.343 | 10.308 | 14.687 | 26.471 | 7 |
| baseline 3 | 6.662 | 10.506 | 14.933 | 21.966 | 8 |
| split 3 | 6.698 | 9.629 | 13.440 | 23.107 | 5 |

Across six runs, 6/6 p99 values and 0/6 maxima meet the reference. Earlier acceptance failures remain recorded; this investigation does not replace the declared six-workload acceptance run.

## Submission components

These are CPU wall intervals, not GPU execution times. Preparation/other is a residual including tile traversal, culling, vertex generation, bookkeeping, timer overhead and scheduling outside measured GL blocks. GL upload/draw combines state binding, orphaning, vertex upload and draw submission; a long interval does not by itself identify a driver stall.

| Split run | Component | Median | p95 | p99 | Maximum |
| --- | --- | ---: | ---: | ---: | ---: |
| 1 | prepare_other | 2.637 | 4.628 | 5.327 | 8.450 |
| 1 | gl_setup | 0.027 | 0.049 | 0.077 | 0.665 |
| 1 | gl_upload_draw | 0.698 | 1.125 | 1.677 | 5.535 |
| 2 | prepare_other | 3.534 | 5.360 | 6.792 | 10.042 |
| 2 | gl_setup | 0.032 | 0.059 | 0.081 | 0.806 |
| 2 | gl_upload_draw | 0.831 | 1.339 | 1.783 | 4.228 |
| 3 | prepare_other | 3.706 | 5.210 | 5.689 | 7.487 |
| 3 | gl_setup | 0.033 | 0.062 | 0.092 | 0.245 |
| 3 | gl_upload_draw | 0.929 | 1.380 | 1.668 | 2.941 |

The next table shows the largest submission interval in each split process. Every value in a row belongs to the same original frame; independent percentile values above must not be added. Query/other is the outer submission interval minus the enclosed render call.

| Split run / frame | Whole frame | Submission | Preparation/other | GL setup | GL upload/draw | Query/other | Paired GPU |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 / 1056 | 15.356 | 12.561 | 7.319 | 0.053 | 5.165 | 0.025 | 1.596 |
| 2 / 1045 | 13.867 | 10.876 | 7.973 | 0.043 | 2.838 | 0.022 | 1.082 |
| 3 / 116 | 11.474 | 9.068 | 6.961 | 0.051 | 2.036 | 0.019 | 1.170 |

One split-mode frame exceeds 16.7 ms with submission as its largest outer CPU interval. Their full paired records, plus each run's ten largest submissions, are retained in [analysis.json](../validation/2026-10-27-submission/analysis.json). Missing GPU results remain null; CPU and GPU intervals overlap and are not additive.

Preparation/other is the largest render component in **29 of the 30 retained
largest submissions**; GL upload/draw is largest in one. This points the next
investigation toward CPU preparation and scheduling, without proving where CPU
execution time is spent. Split run 2, frame 1034 takes 19.289 ms overall: submission
9.360 ms includes preparation/other 5.176 ms, GL setup 0.029 ms and GL upload/draw
4.142 ms. Its paired GPU interval is 1.186 ms and presentation 2.208 ms.

The baseline median also drifts from 3.183 to 6.343 and 6.662 ms across repeats;
there is no stable baseline for attributing the split runs' higher medians to
instrumentation. Raw thermal-zone readings are retained, including one implausible
negative-valued sensor; these boundary snapshots do not establish throttling.

## Correctness, resources and limits

Each measured offscreen frame uploads 1,966,080 vertex bytes across three nonempty batches: 10,240 visible quads, with 2,048 sprites culled. This validates accounting; upload bytes are neither orphaned capacity nor GPU residency. The original geometry workload and checksum `3573445382734126645` are unchanged. Per process, 1,800 measured frames produce 5,400 draws and 1,980 presentation calls including warmup.

Split storage adds 100,800 bytes for 1,800 rows. Existing frame storage remains 158,400 bytes, timing vectors 100,824 bytes and tracked workload buffers 8,072,232 bytes. Texture/target payloads remain 16 and 8,294,400 bytes, with stable load/unload accounting. Kernel per-child peak RSS ranges from 53.79 to 55.19 MiB. This includes launch, report formatting, driver state and teardown; it is not a live-heap measurement or coverage of all gameplay/audio/error paths.

Nine focused groups pass in GNU Debug, GNU Release and Clang Debug; five CPU and four native ASan/UBSan groups pass. There are no skips. Sixteen final-image pairs match byte-for-byte across default, paired, split, immediate and requested-VSync paths. Tests cover empty passes, batch rollover, timing enable/reset, invalid options, bounded allocation-free recording and report preservation. This is a focused matrix, not renewed full release acceptance.

## Next step

Profile CPU preparation (tile traversal, culling and vertex generation) and distinguish execution from scheduling delay before selecting an optimization. The retained split favors this investigation over an immediate buffer-streaming change. Preserve presentation timing, original workloads, earlier failed attempts and declared limits. Physical display/audio acceptance, broader memory-path coverage and the final support record remain open; background loading remains deferred.
