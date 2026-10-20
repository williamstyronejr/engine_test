# Collision buffer utilization — 2026-10-20

Increment 45 adds allocation-free collision high-water counters and per-level
reporting. Three complete native runs agree exactly with the retained CPU route
reference. Each world still reserves **6,557,696 bytes (6.25 MiB)** of covered
collision storage; contact, active sensor and trigger event buffers account for
**6,266,880 bytes (95.6%)**.

Facility reaches 219 bodies but only one contact and one event per update. Annex
reaches 146 bodies and no contacts on this route. This identifies contact/event
budgets as the next reduction target while retaining the body/query capacity.
Reservations and default engine limits are unchanged in this increment.

## Protocol and validation

The [manifest](collision-usage/2026-10-20/manifest.json) records five sequential native
attempts and a separate CPU reference invocation, using clean source `928e8bc`.
Build: GNU 16.2.1 Release, `-O3 -DNDEBUG`. Machine: i5-4300U / HD Graphics 4400,
`Linux 7.2.8-9-omarchy x86_64`, `4.6 (Core Profile) Mesa 26.2.5-arch1.1`, XWayland.
Compilation and tests finished before collection. No attempts were retried or
filtered by timing thresholds. Package temperatures at run boundaries were 67–68 °C.

All 54 runtime groups passed in Debug and Release without skips. Four CPU groups
(`collision`, `stress_cpu`, `spike_profile`, `scene_reload`) passed ASan/UBSan with
leak detection. Regressions cover capacity accounting, suppressed initialization
events, disjoint enter/exit batches, retained peaks after smaller/failed updates,
moves and replacement-world aggregation.

An allocation-counted test fills the world with 256 overlapping sensors, then
replaces every body identity. It observes 32,640 contacts and 65,280 events in the
second update, with no heap allocations after setup. Repeated telemetry collection
also allocates nothing. This demonstrates why small route measurements cannot
justify reducing the general engine defaults.

Each complete native run renders 3,600 frames at 1050×1360, with 30 warmup frames
excluded from timing distributions. Event frames, hashes, levels, revisions and
the entire collision-usage object match the
[CPU reference](collision-usage/2026-10-20/reference.json).
Final hash is `3859428580428616961`, with 1,381 final-level ticks. Each run completes
3,599 elapsed GPU queries, leaves one pending, and reports no skipped or invalid
samples. Device audio is disabled and VSync is requested off.

## Per-level demand and reserved capacity

The table is identical across all three complete native runs and the CPU reference.
Byte sizes describe this ABI. Peak entries are the largest **single candidate
vector** in a successful update, including initialization and warmup.

| Group | Buffers | Capacity per buffer | Reserved bytes across buffers | Facility peak entries | Annex peak entries |
| --- | ---: | ---: | ---: | ---: | ---: |
| Bodies | 2 | 256 | 20,480 | 219 | 146 |
| Grid cells | 1 | 16,384 | 262,144 | 8 | 8 |
| Contacts | 2 | 32,640 | 1,566,720 | 1 | 0 |
| Active sensor pairs | 2 | 32,640 | 1,566,720 | 1 | 0 |
| Trigger events | 2 | 65,280 | 3,133,440 | 1 | 0 |

The fixed seen-pair bitset contributes another 8,192 bytes. Entry sizes are 40 bytes
for bodies, 16 for grid cells, and 24 for contacts, sensor pairs and events.

Feature Lab's registration code gives walls, the switch and the door a zero
collision mask: they remain queryable but do not occupy the broadphase grid.
Only the player and enabled alarm participate in reciprocal collision detection.
This explains the large body count alongside small pair demand. Annex's zero
contacts describes this procedural route; it does not mean other play paths
cannot touch the alarm.

Coverage `successful_snapshots_v1` records successful candidate sizes before
`prime` clears its events, then merges maxima across published worlds for each
level. These maxima need not occur on the same update and must not be summed as
simultaneous used bytes. Failed update attempts and unpublished transition
candidates are excluded. This is capacity/utilization evidence, not allocator or
RSS accounting. See the [measurement contract](SPIKE_PROFILING.md#collision-buffer-utilization).

## Staging and rollback remain unchanged

All native reload/transition staging peaks and event states match the
[previous collection](2026-10-20-lazy-scene-results.md), with unchanged
`owned_payload_v2` coverage. The largest covered transition peak remains
7,909,182 bytes. Every event leaves zero tracked CPU/GPU staging.

Both private-copy malformed runs exit 1 at frame 1383, with nine events and an
unverified final event. Failed transition attempts preserve every before/after
state and live-resource field. Their collision telemetry contains only Facility;
the failed Annex candidates never become published levels.

| Retained attempt | Diagnostic | Failed-transition CPU staging peak |
| --- | --- | ---: |
| [Malformed scene](collision-usage/2026-10-20/malformed-scene.json) | `annex.scene:21: Data after end` | 7,681,754 bytes |
| [Malformed tilemap](collision-usage/2026-10-20/malformed-tile.json) | `annex: Trailing data at byte 5895` | 12,789 bytes |

Both failures retain the completed Facility hash, which also occurs at the end of
a successful full run. Completion, frame count and event verification remain
necessary to distinguish the outcomes. Shipped assets were untouched.

## Timing context and next change

These runs are not a controlled profiler-overhead or speed comparison. CPU clocks,
caches and desktop scheduling were uncontrolled.

| Raw complete attempt | Frame median ms | Frame p99 ms | Frame max ms |
| --- | ---: | ---: | ---: |
| [Run 1](collision-usage/2026-10-20/run-1.json) | 2.062 | 10.633 | 30.386 |
| [Run 2](collision-usage/2026-10-20/run-2.json) | 2.053 | 10.053 | 40.436 |
| [Run 3](collision-usage/2026-10-20/run-3.json) | 2.116 | 9.846 | 43.159 |

Next: introduce explicit per-world contact/event budgets with the current defaults,
choose conservative budgets for Feature Lab while preserving its body/query limit,
and reject budget overflow before publication. Validate saturated/default worlds,
rollback/retry and allocation-free steady updates, then repeat transition staging
measurements. Broader physical hardware/driver checks remain outstanding; asset
tooling and background loading stay deferred.
