# Per-scene budgets — 2026-10-21

Increment 49 introduces fixed per-scene entity/slot budgets. The engine default
remains 4,096 entries; Feature Lab selects 256 for both levels and every restart,
reload, restore and transition. Covered scene reservations fall from **1,310,720
to 81,920 bytes (80 KiB)** per published world, a **93.75% reduction**. The largest
covered transition staging peak falls from **1,383,742 to 154,942 bytes (88.8%)**.

Three complete native runs and the CPU reference agree on scene demand, collision
usage and gameplay states. Malformed scenes, malformed tilemaps and a new 257-entity
scene fixture preserve the live level and release all tracked staging ownership.
These are covered payload reductions, not total process-memory or speedup claims.

## Collection and validation

The [manifest](scene-budgets/2026-10-21/manifest.json) retains all six sequential
native attempts and the separate CPU reference command, including exits, logs and
boundary temperatures. Clean source: `c4d07e27f9dbc830572fa3439e49220ad9b8bdec`.
Collection began at 22:50:26 UTC after builds and tests finished. No attempts were
retried or selected by timing thresholds. Package temperatures were 65–70 °C.

Build: GNU 16.2.1 Release, `-O3 -DNDEBUG`. Machine: i5-4300U / HD Graphics 4400,
Linux 7.2.8-9-omarchy x86_64, OpenGL 4.6 Core / Mesa 26.2.5-arch1.1, XWayland.
Each complete run renders 3,600 frames at 1050×1360; 30 warmup frames are excluded
from timing distributions. Audio-device startup is disabled and VSync requested
off. Each completes 3,599 GPU queries, leaves one pending and reports no skipped
or invalid samples.

All 54 runtime groups passed in Debug (69.23 s) and Release (75.21 s), without
skips. Six focused CPU groups (`content`, `components`, `stress_cpu`, `spike_profile`,
`scene_reload`, `levels`) passed ASan/UBSan with leak detection (18.93 s).
Regression coverage includes:

- Zero and invalid limits, full-scene overflow, preservation of live handles and
  usage on rejection, successful replacement/retry and old-handle invalidation.
- Budget transfer on moves, moved-from reuse and iteration-guard rejection before
  changing the destination's limit or ownership.
- Full component pools, dense compaction, slot reuse and surviving component values.
- Repeated destroy/recreate/defer cycles without deferred-queue growth or allocation;
  warmed component add/remove and memory observation also allocate nothing.
- A 256-node Feature Lab reload that succeeds, a 257th node that rejects without
  changing live state, and repair/retry. Existing 4,096-entry default tests remain.
- Both levels retaining scene and collision budgets through restart, scene/tilemap
  reload, checkpoint restore and transition. CPU transition faults cover eight
  loading/parsing/semantic failure cases; native tests add the 257-entity fixture.

## API and storage behavior

`Scene::Limits{entities}` accepts zero through 4,096. Core vectors reserve on first
creation; component dense/sparse vectors use the same chosen budget. `parse_scene`
accepts limits and `replace_scene` inherits the live scene's limits. Moves transfer
the limit while leaving the empty source reusable with that same limit. Limits are
runtime configuration and do not change scene file formats.

Entity or slot exhaustion throws `std::length_error`, including at the default
maximum (previously `std::runtime_error`). Parser errors retain their source/line
`runtime_error` diagnostics. Larger Feature Lab content requires raising the
application's budget. The stock route peaks at 11 nodes, leaving substantial
headroom within 256. This budget does not cap nested strings, hash allocations or
all memory owned by components.

A full deferred-deletion queue now discards stale handles before appending another
unique request. Direct destruction and slot reuse can otherwise accumulate old
generations before a flush. Valid requests keep their order and cannot exceed the
live-entity limit. Peak counters retain earlier queue demand after pruning.
See the [runtime contracts](../docs/FORMATS.md#runtime-entity-contracts).

## Reservations and demand

All three native runs match the entire scene and collision objects in the
[CPU reference](scene-budgets/2026-10-21/reference.json). The following capacities
are 256 entries each. Byte sizes describe this ABI.

| Scene storage | Previous reserved bytes | Current reserved bytes | Facility peak entries | Annex peak entries |
| --- | ---: | ---: | ---: | ---: |
| Handle slots (16-byte entries) | 65,536 | 4,096 | 11 | 9 |
| Scene nodes (232-byte entries) | 950,272 | 59,392 | 11 | 9 |
| Deferred queue (24-byte handles) | 98,304 | 6,144 | 1 | 1 |
| Component dense entries (40 bytes) | 163,840 | 10,240 | 3 | 3 |
| Component sparse indices (8 bytes) | 32,768 | 2,048 | — | — |
| **Total** | **1,310,720** | **81,920** | — | — |

Core reservations total 69,632 bytes; the single retained `CorePickup` pool adds
12,288 bytes. Its peak dense payload remains 120 bytes. Sparse indices include
missing-entry sentinels. Collision reservations remain **32,256 bytes per world**,
with the same full utilization object as the
[previous collection](2026-10-21-scene-usage-results.md).

Coverage remains `successful_mutations_v1` for scenes, `successful_snapshots_v1`
for collisions and `owned_payload_v2` for staging. Per-level fields retain independent
maxima across published replacement worlds, including warmup. Component peak sums
combine independent per-pool peaks; totals are not simultaneous occupied bytes or
process-memory peaks. Failed candidates are excluded from published-world telemetry.
Registry/hash overhead, nested strings, object metadata, allocator overhead,
component-owned allocations and driver memory remain outside these counters.

## Transitions and retained failures

Every complete run ends at hash `3859428580428616961` with 1,381 final-level ticks.
All ten event frames, before/after states and resource revisions match the previous
collection. Each transition saves exactly **1,228,800 covered bytes**, equal to
the reduced scene reservations. Shader, texture and font staging peaks are unchanged.

| Transition | Previous CPU/combined peak | Current CPU/combined peak |
| --- | ---: | ---: |
| Annex preflight rejection, frame 1382 | 1,373,047 | 144,247 |
| Annex publication, frame 1383 | 1,373,047 | 144,247 |
| Facility publication, frame 2086 | 1,383,742 | 154,942 |

Transition GPU staging is zero; all events leave zero tracked CPU/GPU staging.
Private-copy failure attempts stop at frame 1383 with exit 1, nine events and an
unverified final event. Both failed transition attempts preserve before/after live
state. Published scene and collision telemetry contains only Facility. Shipped
assets remain unchanged.

| Retained attempt | Diagnostic | Failed-transition CPU/combined peak |
| --- | --- | ---: |
| [Malformed scene](scene-budgets/2026-10-21/malformed-scene.json) | `annex.scene:21: Data after end` | 111,834 bytes |
| [Malformed tilemap](scene-budgets/2026-10-21/malformed-tile.json) | `annex: Trailing data at byte 5895` | 12,789 bytes |
| [257-entity scene](scene-budgets/2026-10-21/over-budget-scene.json) | `annex.scene:267: Scene entity budget exceeded` | 130,351 bytes |

The malformed scene saves 1,044,480 bytes because parsing fails before component
pool construction; the malformed tilemap peak is unchanged. The over-budget
fixture adds harmless decor records to Annex and has larger text/parser buffers
than the stock scene. Failure final hashes equal the completed Facility state;
completion flags and event verification distinguish retention from a full run.

## Timing context and next increment

| Raw complete attempt | Frame median ms | Frame p99 ms | Frame max ms |
| --- | ---: | ---: | ---: |
| [Run 1](scene-budgets/2026-10-21/run-1.json) | 2.160 | 11.548 | 40.897 |
| [Run 2](scene-budgets/2026-10-21/run-2.json) | 2.219 | 10.701 | 32.754 |
| [Run 3](scene-budgets/2026-10-21/run-3.json) | 2.165 | 10.030 | 27.676 |

These timings do not establish a speedup: desktop scheduling, caches, clocks and
thermals were uncontrolled. The repeatable result is the covered memory reduction
with unchanged behavior and measurement coverage.

Next: add an offline asset-validation command using existing readers and Feature
Lab semantic checks, with useful file diagnostics and malformed-fixture coverage.
This lets content authors check assets and scene budgets without launching OpenGL.
Broader physical hardware/driver and manual release checks remain outstanding;
background loading remains deferred.
