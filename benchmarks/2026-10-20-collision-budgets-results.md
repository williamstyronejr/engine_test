# Per-world collision budgets — 2026-10-20

Increment 46 adds explicit contact/event budgets while retaining the engine's
default limits. Feature Lab reserves 16 contacts and 32 events, reducing covered
collision storage from **6,557,696 to 293,888 bytes per world (95.5%)**. All three
complete native runs reproduce the same reduction. The largest covered transition
peak falls from **7,909,182 to 1,645,374 bytes (79.2%)**.

Body/query capacity remains 256. Gameplay states, event frames, resource revisions
and observed collision demand match the previous increment. Both malformed-level
attempts retain the live game and release all tracked staging ownership.

## Budget contract and validation

`CollisionWorld::Limits{contacts, events}` chooses fixed budgets at construction.
Active sensor history shares the contact budget. Defaults remain 32,640 contacts
and 65,280 events; each budget independently permits zero. Above-maximum values
are rejected before reservation. Moves transfer budgets and buffers.

Contact/event overflow throws before publication. Published spans, body/contact/
event contents, trigger history, statistics and successful high-water counters
remain unchanged. This includes `prime`: generated events must fit before they
are suppressed. Failed scratch work is cleared on retry. Successful updates remain
allocation-free; exception diagnostics are outside that guarantee.

Feature Lab's player and alarm are the only pair participating in reciprocal
detection. The 16/32 budgets leave headroom above that demand while retaining all
query-only walls, the switch and door. Restart, reload, checkpoint and transition
tests retain these sample budgets. Tilemap replacement inherits the current
world's budgets, including a separately configured smaller world.

All 54 runtime groups passed in Debug and Release without skips. Six focused
groups (`collision`, `stress_cpu`, `spike_profile`, `levels`, `tilemap_reload`,
`scene_reload`) passed ASan/UBSan with leak detection. Regressions cover exact
contact/event saturation, separate overflow paths, failed initialization, retry,
moves, zero budgets, full query capacity and constructor bounds.

Allocation-counted tests exercise 120 updates with 256 bodies, three active
contacts and six exit/enter events, without allocating after setup. Existing
default-world coverage still handles 256 overlapping sensors followed by disjoint
identities: 32,640 contacts and 65,280 events without allocations after setup.

## Collection protocol

The [manifest](collision-budgets/2026-10-20/manifest.json) retains three complete
native attempts, two private-copy failures and the CPU reference invocation.
Source is clean implementation commit `b97f25cad1f307d6b9ddad937738326f1c73465a`.
Collection started at 22:19:08 UTC after compilation and tests finished. No attempts
were retried or selected by timing thresholds. Package temperatures at run
boundaries were 67–72 °C.

Build: GNU 16.2.1 Release, `-O3 -DNDEBUG`. Machine: i5-4300U / HD Graphics 4400,
Linux 7.2.8-9-omarchy x86_64, OpenGL 4.6 Core / Mesa 26.2.5-arch1.1, XWayland.
Each complete run rendered 3,600 frames at 1050×1360, with 30 warmup frames excluded
from timing distributions, device audio disabled and VSync requested off.

All complete runs match the [CPU reference](collision-budgets/2026-10-20/reference.json)
for collision telemetry and event states. Final hash is `3859428580428616961`, with
1,381 final-level ticks. Each completes 3,599 elapsed GPU queries, leaves one pending
and reports zero skipped or invalid samples. Before/after event states also match
the [previous collection](2026-10-20-collision-usage-results.md).

## Capacity and transition measurements

Both levels reserve the same buffers. All three native runs and the CPU reference
agree on the following values; byte sizes describe this ABI.

| Group | Buffers | Capacity per buffer | Reserved bytes | Facility peak entries | Annex peak entries |
| --- | ---: | ---: | ---: | ---: | ---: |
| Bodies | 2 | 256 | 20,480 | 219 | 146 |
| Grid cells | 1 | 16,384 | 262,144 | 8 | 8 |
| Contacts | 2 | 16 | 768 | 1 | 0 |
| Active sensor pairs | 2 | 16 | 768 | 1 | 0 |
| Trigger events | 2 | 32 | 1,536 | 1 | 0 |

The fixed seen-pair bitset adds 8,192 bytes. Body, cell and fixed storage are
unchanged; contact/history/event reservations save exactly 6,263,808 bytes per
world. Peak counts describe single successful candidate vectors, including events
before initialization suppression. Annex's zero contacts describes the route,
not every possible play path. Coverage remains `successful_snapshots_v1`.

| Operation | Previous CPU staging peak | New CPU staging peak |
| --- | ---: | ---: |
| Rejected Annex transition, frame 1382 | 7,898,487 | 1,634,679 |
| Committed Annex transition, frame 1383 | 7,898,487 | 1,634,679 |
| Committed Facility transition, frame 2086 | 7,909,182 | 1,645,374 |
| Malformed Annex scene, frames 1382/1383 | 7,681,754 | 1,417,946 |
| Malformed Annex tilemap, frames 1382/1383 | 12,789 | 12,789 |

The first four rows fall by exactly one world's reservation savings. The tilemap
failure precedes game construction. Transition GPU staging is zero, and all events
leave zero tracked CPU/GPU staging. Shader, texture and font reload peaks are
unchanged. `owned_payload_v2` coverage is unchanged: these are selected payload
peaks, excluding nested string storage, validation hash nodes, allocator internals,
existing live resources and opaque driver memory. They are not process/RSS bounds.

## Retained failures and timing context

The [malformed scene](collision-budgets/2026-10-20/malformed-scene.json) reports
`annex.scene:21: Data after end`; the
[malformed tilemap](collision-budgets/2026-10-20/malformed-tile.json) reports
`annex: Trailing data at byte 5895`. Both exit 1 at frame 1383 with nine events,
an unverified final event, identical before/after live states and only Facility
collision telemetry. Shipped assets are untouched. Their final hash also occurs
in complete runs, so completion and event verification must be checked separately.

| Raw complete attempt | Frame median ms | Frame p99 ms | Frame max ms |
| --- | ---: | ---: | ---: |
| [Run 1](collision-budgets/2026-10-20/run-1.json) | 1.899 | 11.233 | 89.921 |
| [Run 2](collision-budgets/2026-10-20/run-2.json) | 2.120 | 9.898 | 26.506 |
| [Run 3](collision-budgets/2026-10-20/run-3.json) | 2.102 | 9.823 | 31.491 |

These timings do not establish a speedup: clocks, caches, thermals and desktop
scheduling were uncontrolled. The larger maximum in run 1 is retained.

Next: add explicit per-world grid-membership budgets with the existing default.
Grid storage now accounts for 262,144 of 293,888 reserved collision bytes, while
this route peaks at eight memberships. Choose sample headroom from collider bounds,
retain the body/query limit, and validate saturation, rollback/retry and allocation
behavior before remeasurement. Broader physical hardware/driver checks remain
outstanding; asset tooling and background loading stay deferred.
