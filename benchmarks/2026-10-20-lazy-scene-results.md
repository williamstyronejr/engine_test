# Lazy scene reservation results — 2026-10-20

Increment 44 defers a new scene's core vector reservations until its first entity
is created. Empty game-world placeholders and scene move destinations no longer
allocate buffers that a parsed or moved scene immediately replaces. Populated
scenes retain the existing 4,096-entity capacity and keep their buffers after
deletion. Handle identity, component ownership and iteration guards are unchanged.

The largest covered transition peak falls from **8,804,022 to 7,909,182 bytes**:
**894,840 bytes (10.2%) lower**, using the same `owned_payload_v2` coverage.
Three complete runs reproduce the new peaks. Two malformed-content attempts
preserve live state and release all tracked staging.

## Collection and validation

All five sequential attempts use clean source `61c7fa9`, GNU 16.2.1 Release
(`-O3 -DNDEBUG`), the i5-4300U / HD Graphics 4400, and XWayland.
Reported system: `Linux 7.2.8-9-omarchy x86_64`; graphics:
`4.6 (Core Profile) Mesa 26.2.5-arch1.1`.
Compilation and tests finished before collection. No attempts were retried or
filtered by timing thresholds. The
[manifest](lazy-scene/2026-10-20/manifest.json) retains commands, exits, durations,
stdout/stderr and package-temperature readings (69–72 °C at run boundaries).

All 54 runtime groups passed in Debug and Release without skips. Four CPU groups
(`scene_reload`, `components`, `stress_cpu`, `spike_profile`) passed ASan/UBSan
with leak detection. New allocation-counted coverage checks empty construction,
move construction/assignment, pending-deletion transfer, moved-from reuse,
retained capacity after deletion and a full-capacity deferred-deletion queue.
The tests return to their baseline live allocation count after teardown.

Parser regressions check empty/early-invalid input, successful parsing, late syntax
failure and transform validation after entity creation. The parser updates its
staging lease immediately after creation so later failures still include reserved
buffers. The transition regression now requires construction to stay within the
completed candidate's payload peak (plus a supplied preflight overlay).
The existing seven malformed-input cases retain live handles/hashes and successfully
retry after repair.

Each complete native attempt renders 3,600 frames at 1050×1360, with 30 warmup
frames excluded from distributions. Event frames, hashes, levels and revisions
match the independent CPU reference. Final hash is `3859428580428616961`, with
1,381 final-level ticks. All three collect 3,599 elapsed GPU queries, leave one
pending, and report no skipped or invalid samples. Device audio is disabled and
VSync is requested off.

## Covered peak comparison

The previous column comes from the
[October 19 v2 collection](2026-10-19-parser-staging-results.md), source `c50b489`.
Both collections use the same workload, payload definition, compiler and reported
kernel/driver versions. These are selected payload counts; they do not establish
an RSS or total-memory bound.

| Operation | Previous CPU peak bytes | New CPU peak bytes | Reduction bytes |
| --- | ---: | ---: | ---: |
| Frame 1382 / rejected Annex preflight | 8,795,855 | 7,898,487 | 897,368 |
| Frame 1383 / transition to Annex | 8,795,855 | 7,898,487 | 897,368 |
| Frame 2086 / transition to Facility | 8,804,022 | 7,909,182 | 894,840 |
| Malformed Annex scene / late syntax failure | 8,795,866 | 7,681,754 | 1,114,112 |
| Malformed Annex tilemap / trailing data | 12,789 | 12,789 | 0 |

These operations have zero candidate GPU texel payload, so combined peaks equal
CPU peaks. All other reload CPU/GPU/combined peaks match the previous v2 collection.
Every event leaves zero tracked CPU and GPU staging bytes.

On this build a populated scene's core vectors reserve 1,114,112 bytes. Removing
the unused world reservation saves that amount during parsing. Successful
transitions now peak later, at the completed candidate/minimap overlap, so their
peak reduction is smaller. The late scene failure stops during parsing and retains
the full reservation reduction. Scene moves also avoid throwaway reservations;
member-initialization and move intermediates remain outside exhaustive staging
coverage, so no additional peak saving is claimed for them.

## Malformed-content evidence

The [scene attempt](lazy-scene/2026-10-20/malformed-scene.json) appends an unexpected
record after Annex's `end`; it retains `annex.scene:21: Data after end`.
The [tile attempt](lazy-scene/2026-10-20/malformed-tile.json) appends a byte to
Annex's ETMP; it retains `annex: Trailing data at byte 5895`.
Both faults use private asset copies.

Both runs exit 1 at frame 1383 with `complete=false`, nine events and an unverified
final event. Their frame-1382 rejection attempts fail in parsing before preflight;
the required frame-1383 transitions then fail and stop the workload. Both failed
transition attempts preserve every before/after state and live-resource field,
with zero staging remaining. Their final
hash identifies the completed Facility checkpoint, so completion and event outcomes
must be checked alongside the hash. Shipped assets were untouched.

## Timing context and limits

Timings are retained without a speedup claim. Clocks, caches and desktop scheduling
were uncontrolled, and the before/after collections occurred in separate sessions.

| Raw complete attempt | Frame median ms | Frame p99 ms | Frame max ms |
| --- | ---: | ---: | ---: |
| [Run 1](lazy-scene/2026-10-20/run-1.json) | 1.894 | 9.163 | 29.860 |
| [Run 2](lazy-scene/2026-10-20/run-2.json) | 2.051 | 10.616 | 32.449 |
| [Run 3](lazy-scene/2026-10-20/run-3.json) | 1.935 | 9.389 | 23.533 |

[Coverage exclusions](SPIKE_PROFILING.md#staging-memory-coverage) remain unchanged:
nested string storage, validation hash nodes, allocator internals/reallocation
overlap, failed allocations and opaque shader/driver memory are excluded.
The counters cover selected checkpoints, not every construction intermediate.

Next: measure collision scratch utilization to identify the next reservation
reduction while retaining transactional updates and steady-operation allocation
guarantees. Broader physical hardware/driver checks remain outstanding; asset
tooling and background loading stay deferred.
