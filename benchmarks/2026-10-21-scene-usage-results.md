# Scene and component storage utilization — 2026-10-21

Increment 48 adds allocation-free scene/core and component utilization reporting.
Three complete native runs agree exactly with the retained CPU route reference.
Each published scene reserves **1,310,720 bytes (1.25 MiB)** of covered vector
payload, with capacity for 4,096 entries in each core vector and component pool.
Facility peaks at **11 nodes**, Annex at **nine**, and each level's single runtime
component pool peaks at **three entries**.

This identifies per-scene capacity budgets as the next reduction target. Engine
limits and reservations are unchanged here. Gameplay states, collision telemetry
and all covered staging peaks match the previous increment.

## Collection and validation

The [manifest](scene-usage/2026-10-21/manifest.json) records three complete native
attempts, two private-copy failures and a separate CPU reference invocation.
Clean source: `9bfd3687843c00ee9c6e0cead4327fc0f3c2c7d2`. Collection began at
16:36:13 UTC after compilation and testing. No attempts were retried or selected
by timing thresholds. Package temperatures at run boundaries were 69–76 °C.

Build: GNU 16.2.1 Release, `-O3 -DNDEBUG`. Machine: i5-4300U / HD Graphics 4400,
Linux 7.2.8-9-omarchy x86_64, OpenGL 4.6 Core / Mesa 26.2.5-arch1.1, XWayland.
Each complete run renders 3,600 frames at 1050×1360, with 30 warmup frames excluded
from timing distributions, device audio disabled and VSync requested off.
Each completes 3,599 elapsed GPU queries, leaves one pending and reports zero
skipped or invalid samples.

All 54 runtime groups passed in Debug and Release without skips. Six focused CPU
groups (`content`, `components`, `stress_cpu`, `spike_profile`, `scene_reload`,
`levels`) passed ASan/UBSan with leak detection. Regressions cover aligned component
entry sizes, independent pool peaks after removal, failed additions to new/existing
pools, duplicate deletion requests, failed replacement, move transfer and moved-from
reuse. Full-capacity coverage checks 4,096 slots, nodes and queued deletions.

Allocation-counted tests repeatedly add/remove components, read usage and collect
both levels without heap allocations after setup. Counters retain demand that
occurs before first observation, including deferred queues flushed within a tick.
Collection retains maxima when a level replaces its world with an empty scene;
invalid level indices leave the report unchanged.

Every complete run matches the [CPU reference](scene-usage/2026-10-21/reference.json)
for the entire scene and collision usage objects and verified event states.
Final hash is `3859428580428616961`, with 1,381 final-level ticks. Event frames,
before/after states and resource revisions also match the
[previous collection](2026-10-21-collision-grid-budgets-results.md).

## Scene reservations and demand

The following measurements agree across all three native runs and the CPU
reference. Byte sizes describe this ABI. Core peaks are successful mutation
high-water counts, retained through deletion and flush.

| Core vector | Entry bytes | Capacity | Reserved bytes | Facility peak entries | Annex peak entries |
| --- | ---: | ---: | ---: | ---: | ---: |
| Handle slots | 16 | 4,096 | 65,536 | 11 | 9 |
| Scene nodes | 232 | 4,096 | 950,272 | 11 | 9 |
| Deferred deletion queue | 24 | 4,096 | 98,304 | 1 | 1 |

Core vectors reserve **1,114,112 bytes** in total. Slot demand includes retained
handle slots after deletion; node demand counts live dense entries. Pending demand
counts unique queued handles, including handles that become stale before flush.

Both levels have the same component measurements:

| Component metric | Value |
| --- | ---: |
| Retained pools | 1 |
| Dense capacity entries | 4,096 |
| Dense reserved bytes | 163,840 |
| Sparse capacity entries | 4,096 |
| Sparse reserved bytes | 32,768 |
| Sum of per-pool peak entries | 3 |
| Sum of per-pool peak dense bytes | 120 |

The sample's `CorePickup` pool uses 40-byte dense entries, including the entity
handle and padding. Sparse entries use eight bytes each, including missing-entry
sentinels. Together these vectors reserve **196,608 bytes**. Component values'
separate allocations are outside this accounting.

Coverage `successful_mutations_v1` observes published worlds after each rendered
frame, includes warmup and merges each field's maximum across replacements.
Component peak sums combine each pool's independent high-water mark; they are not
simultaneous occupied totals. Core fields and reservations can also peak in
different worlds. The report's total sums retained reservation maxima, not a
process-memory peak. Pool types are aggregated without implementation-specific
type names or unordered iteration order. Failed/unpublished candidates are excluded.

Moves transfer history and reset source counters. Failed component construction
does not advance peaks or publish a new pool. Failed mutations do not advance peaks,
though a failed allocation during initial core reservation may leave some capacity
reserved. Registry/hash overhead, object metadata, nested strings and allocations
owned by component values are excluded. See the
[measurement contract](SPIKE_PROFILING.md#scene-storage-utilization).

## Staging and retained failures

Existing `owned_payload_v2` staging coverage and peaks are unchanged. The largest
covered transition peak remains **1,383,742 bytes**; collision storage remains
32,256 bytes per world. Every event leaves zero tracked CPU/GPU staging.

Both malformed-level attempts exit 1 at frame 1383 with nine events and an
unverified final event. Failed attempts preserve before/after live states. Both
scene and collision telemetry contain only Facility, excluding unpublished Annex
candidates. Shipped assets are untouched.

| Retained attempt | Diagnostic | Failed-transition CPU staging peak |
| --- | --- | ---: |
| [Malformed scene](scene-usage/2026-10-21/malformed-scene.json) | `annex.scene:21: Data after end` | 1,156,314 bytes |
| [Malformed tilemap](scene-usage/2026-10-21/malformed-tile.json) | `annex: Trailing data at byte 5895` | 12,789 bytes |

Their final hash also occurs in complete runs. Frame counts, completion and event
verification distinguish successful completion from preserved state after failure.

## Timing context and next change

| Raw complete attempt | Frame median ms | Frame p99 ms | Frame max ms |
| --- | ---: | ---: | ---: |
| [Run 1](scene-usage/2026-10-21/run-1.json) | 1.808 | 9.210 | 31.507 |
| [Run 2](scene-usage/2026-10-21/run-2.json) | 1.940 | 9.393 | 24.068 |
| [Run 3](scene-usage/2026-10-21/run-3.json) | 2.005 | 9.696 | 30.095 |

These timings establish neither a speedup nor a profiler-overhead bound: clocks,
caches, thermals and desktop scheduling were uncontrolled. Counter reads scan at
most 64 pools and allocate nothing; JSON serialization happens after collection.

Next: introduce explicit per-scene entity/slot budgets with the current 4,096-entry
default, propagate them through parsing and component pools, and choose conservative
Feature Lab capacity. Validate saturation, handle identity, deferred deletion,
rollback/retry and allocation behavior before repeating transition measurements.
Broader physical hardware/driver checks remain outstanding; asset tooling and
background loading stay deferred.
