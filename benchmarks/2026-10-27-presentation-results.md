# Stress presentation comparison — 2026-10-27 UTC

Requesting swap interval 1 changes the observed median frame interval from
**3.18–3.50 ms** in immediate mode to **16.66–16.67 ms**, while presentation median
increases from about **0.28 ms** to **11.89–12.18 ms**. Both request modes still
produce frames above 16.7 ms. The experiment demonstrates different elapsed timing
under the two requests on this host; it does not verify physical synchronization,
identify a driver/scheduler cause, or establish a rendering speedup.

Source **`5a6f1b2caa36b0fb4aea2d76acf556f10490d86a`**, GNU 16.2.1 Release,
`-O3 -DNDEBUG`, sanitizers off. The [protocol and report contract](STRESS_PRESENTATION.md)
were committed before collection. [Validation](../validation/2026-10-27-presentation/RESULTS.md)
retains every attempt, compiler/sanitizer check and image comparison.

## Workload and observed timing

All six runs use the same 8,192 sprites, 4,096 tiles, 1,024 entities, 128 sparse
bodies and eight offline voices, seed 1. The offscreen target is 1920×1080;
the actual preview window is 1050×1360. Each process performs three cycles of
60 warmup plus 600 measured frames, with paired CPU/GPU records enabled.
Order is immediate/VSync, VSync/immediate, immediate/VSync. Builds and tests finish
before collection; no screenshots, concurrent benchmark jobs or retries are added.

Every run issues its requested interval, makes 1,980 presentation calls, completes
1,800 measured frames and returns zero. Checksum, workload, draw/culling counts and
tracked payloads match. All queries complete: 1,980 submitted/completed, zero
pending/skipped/invalid, 1,800 retained measured GPU durations per run. No explicit
GPU wait is introduced. The six reported CPU frame distributions and GPU distribution
reconstruct from their rows; six CPU intervals sum to each frame.

| Attempt | Frame median ms | p95 ms | p99 ms | Maximum ms | Present median ms | Present p99 ms | Frames >16.7 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| immediate 1 | 3.184 | 7.332 | 11.182 | 23.754 | 0.285 | 5.897 | 2 |
| vsync 1 | 16.672 | 17.716 | 18.183 | 48.967 | 11.894 | 16.278 | 839 |
| vsync 2 | 16.663 | 17.708 | 18.004 | 33.858 | 11.905 | 16.272 | 824 |
| immediate 2 | 3.278 | 7.446 | 10.313 | 33.971 | 0.281 | 6.736 | 2 |
| immediate 3 | 3.502 | 7.702 | 10.539 | 23.407 | 0.286 | 6.244 | 5 |
| vsync 3 | 16.671 | 17.394 | 17.511 | 33.327 | 12.177 | 16.100 | 605 |

Immediate mode has **9/5,400 frames (0.167%)** above 16.7 ms; presentation is the
largest CPU interval in seven and render submission in two. Requested VSync has
**2,268/5,400 (42.0%)** above the reference threshold; presentation is largest in
2,266 and submission in two. These are counts from retained rows, not estimates
from p99. The VSync request deliberately introduces potential waiting close to the
reference interval; its threshold misses are not evidence that the renderer needs
42% more compute capacity. Physical refresh and scanout are not measured.

All immediate p99 values are below 16.7 ms, but every run's maximum exceeds it.
The largest immediate frame is **33.970793 ms**; requested VSync reaches
**48.967465 ms**. The earlier [acceptance failures](2026-10-26-workload-acceptance-results.md)
and [paired-frame observations](2026-10-27-frame-samples-results.md) remain retained.
No default is changed to conceal a failure, and requested VSync is a diagnostic
comparison rather than a replacement for the declared immediate workload.

## Remaining submission outliers

| Mode / repeat / frame | Frame ms | Render-submit ms | Present ms | Paired offscreen GPU ms |
| --- | ---: | ---: | ---: | ---: |
| Immediate / 3 / 1767 | 18.127 | 13.738 | 0.513 | 1.153 |
| Immediate / 3 / 1829 | 18.000 | 8.897 | 8.054 | 1.217 |
| VSync / 1 / 1594 | 17.903 | 9.186 | 7.702 | 0.998 |
| VSync / 2 / 1456 | 16.880 | 8.618 | 7.867 | 1.654 |

The [analysis](../validation/2026-10-27-presentation/analysis.json) retains missed-frame
counts, interval rankings and ten worst paired rows per attempt. Full reports retain
all rows, including these submission outliers when they are outside the top ten.
Submission includes CPU traversal, batching and GL calls plus any scheduling or
driver back-pressure. Its aggregate interval cannot distinguish those contributors.
GPU elapsed time excludes preview/presentation and can include scheduling gaps;
do not add it to CPU time or treat it as utilization. Clocks, thermals and caching
are uncontrolled; GPU distributions also differ between modes without changed geometry.

## Resource and validation boundaries

Each run retains 8,072,232 tracked arena bytes, 100,824 timing-vector bytes and
158,400 paired-row bytes. Graphics inventory remains one 16-byte atlas plus one
8,294,400-byte target, with teardown restoration. Kernel lifetime peak RSS ranges
from **53.77 to 54.51 MiB**, below the prior 256 MiB envelope; this six-run comparison
does not rerun every acceptance workload. RSS includes child launch, report output,
shared mappings and driver/allocator caches, and is not engine-owned live heap.

Collection ran **11:32:45–11:34:52 UTC** on an i5-4300U / Intel HD Graphics 4400,
Linux 7.2.8-9-omarchy x86_64, Mesa 26.2.5-arch1.1, XWayland. The active desktop
configuration remained unchanged. Request issuance is recorded independently of
physical behavior. All eight default-versus-explicit-mode image pairs match across
GNU Debug, GNU Release, Clang Debug and native sanitizer configurations.

Eight focused groups pass in each compiler configuration, plus five CPU and three
native ASan/UBSan groups, with no skips. CPU leak detection is enabled; driver leak
detection is disabled for native graphics. These are focused checks, not a refreshed
full validation matrix. Parsing/report tests cover conflicts, unavailable requests,
CPU-only misuse and preserving old reports on rejection.

Next: separate CPU geometry preparation from GL upload/draw time within the
submission interval, using opt-in measurements, before choosing a streaming change.
Retain presentation timing and the same workload identities. The immediate default,
16.7 ms acceptance limits, physical R4/R5 cases, broader memory coverage and R8 support
record remain unchanged; background loading stays deferred.
