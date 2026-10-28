# Fixed-capacity vertex buffer results — 2026-10-28 UTC

**Do not adopt the candidate.** Correctness passes, but the
[predeclared CPU rule](QUAD_FIXED_BUFFER.md) fails. The original renderer is restored
byte for byte. The new capacity-boundary regression test remains, along with the
[prototype](https://github.com/williamstyronejr/engine_test/commit/d0f1f521ae853266be6cf76e7c401de7acb4183e) and
all measurements. There is no new runtime flag, API or report schema.

## Candidate and provenance

The candidate replaced a reserved vector with one owned array of 24,576 vertices,
a separate live count and direct prefix writes, publishing the count once per quad.
It preserved the existing per-quad capacity/state flush guard, arithmetic order,
validation, culling, indices, UVs, colors and GL upload/draw sizes. The 786,432-byte
vertex payload was initialized once during renderer setup, outside measured frames.
It did not reuse clip-space corners or replace division with reciprocal scaling.

[Prior function samples](2026-10-28-functions-results.md) localized the submission
function and emission loop; they did not establish an isolated instruction cost.
Reference source: `9003db72f02034c6c42028272e65171c783931a0`. Candidate/tests/protocol/collector:
`d0f1f521ae853266be6cf76e7c401de7acb4183e`, committed before timing. Both binaries use GNU 16.2.1 Release,
`-O3 -DNDEBUG`, sanitizers off. The [manifest](../validation/2026-10-28-vertices/manifest.json)
retains exact commands, hashes, UTC, thermal readings and kernel child peak RSS.

Collection: **20:12:07–20:13:03 UTC**, six fresh processes in alternating pair order,
with no concurrent builds/tests, screenshots or perf sampling. Host: i5-4300U,
Intel HD 4400, Mesa 26.2.5-arch1.1, Linux 7.2.8-9-omarchy x86_64, XWayland.
1920×1080 offscreen, actual managed preview 1050×1360. Immediate swap requests were
issued; physical synchronization is not established. Both binaries enable the same
frame/submission/preparation/context traces.

All six attempts complete with the default combined workload and seed 1: 8,192
sprites, 4,096 tiles, 1,024 entities, 128 sparse bodies and 8 offline voices.
Each process renders 3 cycles × (60 warmup + 600 measured frames): 1,980 presentations,
1,800 measured rows. All checksums are `3573445382734126645`. Each measured frame
submits 10,240 quads, culls 2,048, draws 3 batches and uploads 1,966,080 vertex bytes.
Workload/render identities, raw trace tags/partitions/aggregates and resource stability
validate. No attempts are discarded or retried based on timing.

## Adoption decision

Primary metric: median of per-frame sums of non-GL preparation thread CPU time.
Positive reduction means less CPU time. Adoption required every pair to improve
by at least 5%, together with correctness.

| Pair | Reference CPU median (ms) | Candidate CPU median (ms) | Reduction | Meets ≥5% |
| --- | ---: | ---: | ---: | --- |
| 1 | 1.473794 | 1.472038 | +0.12% | No |
| 2 | 1.337445 | 1.483100 | -10.89% | No |
| 3 | 1.325535 | 1.061636 | +19.91% | Yes |

Only the third pair meets the rule. Reference medians also drift from 1.474 to
1.326 ms. Frequency, thermal state and session activity remain uncontrolled; these
results do not establish a repeatable gain or a general regression. No tuning or
further timing runs followed the result. [decision.json](../validation/2026-10-28-vertices/decision.json)
records the rejection and matching reference/restored renderer blobs.

All duration columns are milliseconds. Phase medians do not sum to the median of
each frame's total.

| Run | CPU median | CPU p95 | CPU p99 | Tile CPU median | Sprite CPU median | Frame p99 | Frame max | Frames >16.7 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| reference 1 | 1.474 | 2.149 | 2.407 | 0.730 | 0.701 | 10.924 | 39.618 | 5 |
| candidate 1 | 1.472 | 2.070 | 2.630 | 0.763 | 0.664 | 12.081 | 19.450 | 3 |
| candidate 2 | 1.483 | 2.218 | 2.998 | 0.783 | 0.665 | 11.009 | 29.010 | 5 |
| reference 2 | 1.337 | 2.020 | 2.391 | 0.633 | 0.651 | 11.173 | 46.780 | 9 |
| reference 3 | 1.326 | 2.049 | 2.505 | 0.627 | 0.644 | 12.808 | 30.100 | 5 |
| candidate 3 | 1.062 | 1.624 | 1.959 | 0.540 | 0.502 | 11.448 | 52.610 | 8 |

All six frame p99 values pass 16.7 ms; **all six maxima fail**. Presentation is the
largest outer CPU interval in 34 of 35 slow frames; submission dominates one frame
in candidate 1. Candidate 3's 52.610 ms maximum remains, despite its lower preparation
CPU median. This trial does not repair presentation or preemption-related tails.

All 122 preparation wall-minus-CPU gaps above 1 ms accompany involuntary-only switch
spans. Another 2,045 frames have involuntary switches and smaller gaps. Counter spans
include whole phases and cannot assign durations or causes. Reference 1, candidate 2
and candidate 3 each retain one uncollected GPU row; other runs retain all measured
GPU results. No waits or synthetic values were added. Signed gaps and paired worst
frames remain in [analysis.json](../validation/2026-10-28-vertices/analysis.json).

## Correctness and memory

The new native test covers 0, 1, 4,095, 4,096, 4,097 and 8,193 quads. Final-quad
pixels check that the tail batch reaches the target; exact draw/upload counts and
empty-pass pixels check prefix length and reset. Culled and nonfinite draws at a
full buffer must not trigger extra submissions. It passes on the original renderer
before the prototype and stays in the final suite.

Reference: seven focused groups, then the group with the added boundary test.
Candidate: seven groups in GNU Debug/Release and Clang Debug, one CPU ASan/UBSan
group with leaks enabled, six native ASan/UBSan groups with driver leak checks
disabled. **36 group executions pass without failures or skips**; **36 PPM pairs**
match the reference across four native configurations. Existing tests also cover
mirrored/sheared hierarchy pixels, camera changes, zoom/resize, partial rotated
culling, sampling, fonts, alpha, clipping and texture/target transitions. These are
representative checks, not exhaustive equivalence or a new full-suite claim.

The analyzer accepts valid/full-width uint64 controls and rejects twenty corrupted
records in isolated copies. See the [evidence index](../validation/2026-10-28-vertices/RESULTS.md)
for exact commands and retained compiler/test output.

Kernel per-child peak RSS ranges **54.76–55.13 MiB**, below the original 256 MiB
ceiling. It includes initialization/reporting/teardown and is not live heap size.
Tracked workload buffers remain 8,072,232 bytes; timing vectors 100,824; frame,
submission, preparation and context payloads 158,400 / 100,800 / 244,800 / 244,800.
These workload counters do not include the separately described renderer batch.
GPU atlas remains 16 bytes and render target 8,294,400, with stable resource counts.
This does not broaden the existing memory-path coverage.

## Follow-up

Investigate run-to-run variation before a third optimization trial. Declare
observational CPU-frequency/thermal metadata and a warmup/order stability experiment
without changing governor, affinity, priority or desktop settings. This is a new
measurement-quality investigation, not a retroactive change to either rejected
candidate's gate. Keep the existing renderer and all failed attempts. Physical
display/audio, broader memory coverage and final support records remain open;
background loading stays deferred.
