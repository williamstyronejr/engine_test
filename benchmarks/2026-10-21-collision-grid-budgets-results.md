# Collision grid budgets — 2026-10-21

Increment 47 adds per-world grid-membership budgets. Feature Lab reserves 32
memberships, reducing covered collision storage from **293,888 to 32,256 bytes
per world (89.0%)** relative to the contact/event-budget baseline. Three complete
native runs reproduce the same savings. The largest covered transition peak falls
from **1,645,374 to 1,383,742 bytes (15.9%)**.

Engine defaults and the full 256-body query capacity remain unchanged. Gameplay
states and observed collision demand match the previous increment. Both retained
malformed-level attempts preserve the live game and release tracked staging.

## Capacity choice and API contract

`CollisionWorld::Limits{contacts, events, memberships}` adds an optional third
budget. Existing two-field initializers retain 16,384 memberships. Budgets are
fixed at construction, transferred by moves and readable through `limits()`.
Zero memberships supports query-only worlds; above-maximum values are rejected
before reservation. Nonzero-mask bodies consume memberships even when stationary
or unable to form reciprocal pairs. Zero-mask bodies consume none.

The player's box is 0.6 units wide and the alarm's circle has diameter 1.4 units.
Each is narrower than a 4-unit grid cell on both axes, so each can occupy at most
four cells at any position, including boundaries. Their combined bound is eight.
The sample reserves 32, four times that bound. This choice follows collider
geometry rather than relying solely on the measured route. Tests check both
levels' participating-body count and extents; query-only walls retain full capacity.

Total membership overflow is checked before adding that body's cells and before
publishing the candidate. It throws `length_error`, preserving published spans,
trigger history, statistics and successful usage peaks, including failed `prime`
calls. The unchanged 256-membership per-body cap throws `invalid_argument`.
**Compatibility detail:** default-world total overflow also now throws
`length_error`, where it previously threw `invalid_argument`.

## Validation and protocol

All 54 runtime groups passed in Debug and Release without skips. Six focused groups
(`collision`, `stress_cpu`, `spike_profile`, `levels`, `tilemap_reload`,
`scene_reload`) passed ASan/UBSan with leak detection.

Regressions cover exact small/default membership saturation, cumulative and
single-body budget overflow, failed initialization, retry, moves, constructor
bounds and all 256 queries with zero grid capacity. Exact default saturation uses
64 bodies with 256 memberships each; adding a 65th rejects the snapshot. Small
worlds cross negative/zero cell boundaries and preserve trigger history on failure.

Allocation-counted updates alternate between three and twelve memberships with
256 bodies, three contacts and six events. A separate test repeatedly fills and
empties all 16,384 grid memberships. Neither allocates after setup. Existing
default contact/event worst-case tests remain in place. Reload, restart, restore
and transition checks retain sample budgets; tilemap replacement also retains a
custom eight-membership budget.

The [manifest](collision-grid-budgets/2026-10-21/manifest.json) records three complete
native attempts, two private-copy failures and a separate CPU reference invocation.
Clean source: `a91397f8d6bb37b6cb562a7528a5587ce6dc7ae9`. Collection began at
11:20:03 UTC after compilation and testing. No attempts were retried or filtered
by timing thresholds. Package temperatures at run boundaries were 72–83 °C.

Build: GNU 16.2.1 Release, `-O3 -DNDEBUG`. Machine: i5-4300U / HD Graphics 4400,
Linux 7.2.8-9-omarchy x86_64, OpenGL 4.6 Core / Mesa 26.2.5-arch1.1, XWayland.
Each complete run renders 3,600 frames at 1050×1360 with 30 warmup frames excluded
from timing distributions, audio disabled and VSync requested off. Each completes
3,599 GPU queries, leaves one pending and reports zero skipped or invalid samples.

All complete runs match the [CPU reference](collision-grid-budgets/2026-10-21/reference.json)
for collision telemetry and event states. Final hash is `3859428580428616961`, with
1,381 final-level ticks. Event frames, before/after gameplay states and resource
revisions match the [previous collection](2026-10-20-collision-budgets-results.md).

## Reserved payload and transition peaks

Both levels reserve the same buffers. All three complete runs and the CPU reference
agree on these values; byte sizes describe this ABI.

| Group | Buffers | Capacity per buffer | Reserved bytes | Facility peak entries | Annex peak entries |
| --- | ---: | ---: | ---: | ---: | ---: |
| Bodies | 2 | 256 | 20,480 | 219 | 146 |
| Grid cells | 1 | 32 | 512 | 8 | 8 |
| Contacts | 2 | 16 | 768 | 1 | 0 |
| Active sensor pairs | 2 | 16 | 768 | 1 | 0 |
| Trigger events | 2 | 32 | 1,536 | 1 | 0 |

The fixed deduplication bitset adds 8,192 bytes. Only grid reservations change,
falling from 262,144 to 512 bytes, saving **261,632 bytes per world**. The other
groups and all observed peak counts are unchanged. Coverage remains
`successful_snapshots_v1`: peaks describe successful candidate vectors, including
initialization events before suppression, and need not occur simultaneously.

| Operation | Previous CPU staging peak | New CPU staging peak |
| --- | ---: | ---: |
| Rejected Annex transition, frame 1382 | 1,634,679 | 1,373,047 |
| Committed Annex transition, frame 1383 | 1,634,679 | 1,373,047 |
| Committed Facility transition, frame 2086 | 1,645,374 | 1,383,742 |
| Malformed Annex scene, frames 1382/1383 | 1,417,946 | 1,156,314 |
| Malformed Annex tilemap, frames 1382/1383 | 12,789 | 12,789 |

The first four rows fall by exactly one world's grid savings. The tilemap failure
precedes game construction. Transition GPU staging remains zero; every event leaves
zero tracked CPU/GPU staging. Shader, texture and font reload peaks are unchanged.
`owned_payload_v2` coverage is unchanged. These selected payload counts exclude
nested strings, validation hash nodes, allocator internals, existing live resources
and opaque driver memory; they are not process/RSS bounds.

## Retained failures and timing context

The [malformed scene](collision-grid-budgets/2026-10-21/malformed-scene.json) reports
`annex.scene:21: Data after end`; the
[malformed tilemap](collision-grid-budgets/2026-10-21/malformed-tile.json) reports
`annex: Trailing data at byte 5895`. Both exit 1 at frame 1383 with nine events,
an unverified final event, identical before/after live states and only Facility
collision telemetry. Shipped assets are untouched. Their final hash also occurs
in complete runs, so frame counts, completion and event verification distinguish
success from retained failure.

| Raw complete attempt | Frame median ms | Frame p99 ms | Frame max ms |
| --- | ---: | ---: | ---: |
| [Run 1](collision-grid-budgets/2026-10-21/run-1.json) | 1.439 | 8.856 | 33.594 |
| [Run 2](collision-grid-budgets/2026-10-21/run-2.json) | 1.863 | 8.675 | 19.665 |
| [Run 3](collision-grid-budgets/2026-10-21/run-3.json) | 1.939 | 9.499 | 20.057 |

Timings do not establish a speedup: clocks, caches, thermals and desktop scheduling
were uncontrolled. All three attempts are retained.

Next: measure scene core and component reservations and peak demand alongside
collision utilization before changing the scene's 4,096-entity default. Collision
payload now contributes only 32,256 bytes to the covered transition peak. Broader
physical hardware/driver checks remain outstanding; asset tooling and background
loading stay deferred.
