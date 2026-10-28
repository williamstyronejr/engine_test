# Fixed-capacity quad vertex buffer experiment

The retained function profiles identify `Renderer::Impl::submit` as the largest
application symbol. Its emitted loop includes vertex stores and append bookkeeping.
This locates a candidate area; samples do not prove an isolated instruction cost.

Replace the reserved `std::vector<Vertex>` with one owned heap array of exactly
`batch_capacity * 6` vertices and a separate live count. Keep the existing per-quad
capacity/state flush guard. Emit six vertices into the bounded prefix with a local
cursor, then publish the count once. Flush/upload/draw/reset use only that prefix.
Allocate and initialize the backing array once during renderer initialization.
The vertex payload remains 786,432 bytes on the tested ABI; construction now
initializes that payload, outside the measured stress frames. Keep ownership and
cleanup automatic. Do not write beyond object lifetime or vector size.

Preserve validation, world transforms, bounds/culling, six corner indices, UVs,
colors, blending, GL calls and `(world - camera) * 2 / extent` operation order.
No corner reuse, reciprocal scaling, GPU strategy change, new runtime flag or API.

## Correctness and reference

Reference source: `9003db72f02034c6c42028272e65171c783931a0`. Preserve its GNU Release
binary/hash and existing seven focused test groups before editing runtime code.
Add a native boundary test for zero, one, capacity−1, capacity, capacity+1 and
2×capacity+1 quads, final-quad pixels, culling/rejected inputs at the boundary,
exact draw/upload counts and empty-pass reset. Run it against the original path
before applying the candidate. Existing transform, sampling, font, clipping,
texture/target transitions and repeated rollover cases remain relevant.

Run seven groups (transforms, render targets, texture sampling, graphics, font
graphics, stress graphics, stress frame samples) on GNU Debug/Release and Clang
Debug. Run CPU transforms under ASan/UBSan with leaks enabled, and the six native
groups under ASan/UBSan with driver leak checks disabled. Require no failures or
skips. Compare all nine stress-mode PPMs in four native configurations against the
reference (36 pairs). These are representative geometry and storage checks, not
exhaustive floating-point equivalence proof or a new full-suite claim.

## Predeclared performance comparison

Commit candidate, tests, protocol and collector before six fresh-process runs:
reference/candidate, candidate/reference, reference/candidate. Use the same GNU
Release flags, default combined workload and seed, 1920×1080 offscreen, immediate
presentation, three cycles of 60 warmup plus 600 measured frames. Both binaries
enable existing frame/submission/preparation/context traces. Run sequentially,
without screenshots, perf sampling, concurrent builds or tests. Keep every attempt
without timing-based retries. Record exact commands, source/build/binary identities,
UTC, viewport, boundary temperatures, kernel child peak RSS and artifact hashes.

Require unchanged workload/checksum/render totals, per-frame batches and upload
sizes, non-diagnostic buffer payloads and stable resources. Apply the existing
strict trace shape/tag/counter/partition/aggregate checks; retain missing GPU rows,
signed gaps and all earlier failed attempts. Kernel peak RSS includes startup and
teardown and is not a live-heap measurement. The renderer batch payload is distinct
from the existing reported workload buffers.

Primary metric: each run's median of the per-frame sum of non-GL preparation thread
CPU durations. For each adjacent pair, reduction is `100*(reference−candidate)/reference`.
Adopt only if correctness passes and **each of all three pairs reduces that median
by at least 5%**. Otherwise restore the reference storage, preserving the candidate
in Git history and its measurements. Do not tune or retry based on these results.
This local decision gate is not a statistical confidence interval or new release
budget. Report preparation CPU p95/p99, frame p99/max, slow-frame counts and
context-switch-associated gaps separately; an arithmetic/storage optimization does
not establish a fix for preemption or presentation tails. Frequency, thermal state
and session activity remain uncontrolled.

The original 16.7 ms p99/max limits and memory limits remain. Physical display/audio,
wider memory-path coverage and final support gates stay open; background loading
remains deferred.

```sh
python validation/2026-10-28-vertices/collect.py \
  build/vertex-validation/reference-feature-lab build/vertex-validation/candidate-feature-lab
python validation/2026-10-28-vertices/analyze.py validation/2026-10-28-vertices
```

Build the named reference/candidate revisions with identical Release flags and
preserve the reference executable before the candidate build. Collection refuses
existing reports/logs. Use the optional third argument for a fresh evidence directory
with a matching reference-build identity and copied collector; never overwrite this
record. The final findings identify the measured candidate revision.
