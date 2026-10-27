# Paired stress-frame investigation — 2026-10-27 UTC

The three traced runs retain **5,400 measured frames**, including **17 above
16.7 ms**. Presentation is the largest recorded CPU interval in **15** of those
frames; render submission is largest in the other **two**. Their paired offscreen
GPU intervals are **0.872–2.369 ms**. This locates elapsed time within the CPU frame;
it does not identify whether a driver wait, OS scheduling, compositor behavior or
another mechanism caused it.

Source **`271053fd1c7c024237e3b2144321327d54d9efd3`**, GNU 16.2.1 Release,
`-O3 -DNDEBUG`, sanitizers off. The [opt-in contract](STRESS_FRAME_SAMPLES.md) was
committed before collection. All six attempts are retained in the
[validation record](../validation/2026-10-26-frame-samples/RESULTS.md).
The evidence directory uses the session's October 26 local date; collection ran
**00:14:13–00:15:08 UTC on October 27** (October 26 in America/New_York).

## Same combined workload, alternating modes

Each process runs 8,192 sprites, 4,096 tiles, 1,024 entities, 128 sparse bodies and
eight offline voices, seed 1, at a fixed 1920×1080 offscreen surface. Three cycles
have 60 warmup plus 600 measured frames each. The preview window is 1050×1360.
Order: plain/traced, traced/plain, plain/traced. Builds and tests finish first;
there are no screenshots, concurrent benchmark jobs or timing-selected retries.

All checksums, render totals, workload settings and resource payloads match.
The six reported CPU frame distributions and the GPU distribution reconstruct
from each trace's rows within serialization precision; load timing is separate. The six CPU subintervals sum to each frame.
One uncollected result remains in traced repeats 1 and 3; repeat 2 collects all
1,800 measured GPU samples. All runs have zero skipped/invalid queries. Missing
results are null, and no GPU wait is introduced to fill them.

| Attempt | Frame median ms | p95 ms | p99 ms | Maximum ms | Frames >16.7 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| plain 1 | 3.449 | 7.903 | 13.407 | 37.704 | Not retained |
| traced 1 | 3.399 | 7.437 | 11.133 | 32.828 | 2 |
| traced 2 | 3.480 | 7.381 | 11.567 | 22.734 | 4 |
| plain 2 | 3.481 | 7.721 | 9.580 | 23.456 | Not retained |
| plain 3 | 3.276 | 8.798 | 12.147 | 37.885 | Not retained |
| traced 3 | 3.270 | 9.080 | 13.291 | 31.071 | 11 |

The traced miss counts are 2/1,800 (0.111%), 4/1,800 (0.222%) and 11/1,800 (0.611%).
Counts come from retained rows; aggregate-only plain reports cannot supply them.
All six p99 values are below 16.7 ms, but **all six maxima exceed it**. Earlier
[acceptance failures](2026-10-26-workload-acceptance-results.md), including the
18.155 ms p99, remain valid observations and are not replaced by these attempts.
These short, uncontrolled runs do not establish a speedup, zero trace overhead,
consistent physical 60 FPS or a changed release decision.

## What the paired rows show

| Traced run / frame tag | Frame ms | CPU render-submit ms | CPU present ms | Paired GPU ms | Collected after tag |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 / 299 | 32.828 | 4.782 | 27.074 | 0.917 | 300 |
| 1 / 308 | 19.277 | 14.101 | 0.701 | 0.872 | 309 |
| 2 / 314 | 20.606 | 16.757 | 2.834 | 0.980 | 315 |
| 3 / 109 | 31.071 | 1.936 | 28.640 | 1.076 | 110 |

The [analysis](../validation/2026-10-26-frame-samples/analysis.json) retains the ten
worst frames per trace with every CPU interval, paired GPU duration and collection
age. Complete rows retain all outliers, including the eleventh in repeat 3.
`Window::present()` calls `glXSwapBuffers` for a drawable window; its elapsed interval
can contain waiting or descheduling. A short offscreen GPU interval does not time
the preview, presentation or display compositor, and does not rule out driver
back-pressure. Likewise, a long render-submit interval does not prove that CPU
sprite traversal consumed the whole interval. No renderer optimization is justified
solely by ranking these elapsed intervals.

## Storage, validation and next work

Tracing reserves **158,400 bytes** for 1,800 rows on this ABI, separately reported
from the unchanged 100,824-byte timing vectors and 8,072,232-byte arena buffers.
Kernel lifetime peak RSS ranges from **53.09–53.60 MiB** in plain attempts and
**54.00–54.29 MiB** in traced attempts. It includes launch, report formatting and
output, shared mappings and driver/allocator caches; the difference is not an
attribution of live engine heap. The 256 MiB declared process envelope is not
exceeded, but this diagnostic series does not rerun all six acceptance workloads.

Eight focused groups pass in GNU Debug, GNU Release and Clang Debug; five CPU
ASan/UBSan groups and three native ASan/UBSan groups pass, with no skips. CPU leak
detection is enabled; driver leak detection is disabled for native graphics.
Four before/after image pairs are byte-identical. CPU cases verify allocation-free
recording/pairing after reserve, the 60,000-row limit, delayed/warmup results,
partial runs, invalid input and missing-versus-zero GPU semantics.

Next: compare presentation pacing under explicit benchmark modes while retaining
these same frame records and workload identities. Investigate the two submission
outliers separately before changing buffer streaming. Do not change desktop/driver
settings implicitly or claim a physical-display result from an offscreen surface.
R6 timing/coverage, physical R4/R5 acceptance and the final R8 support record remain
open; background loading remains deferred.
