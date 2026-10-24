# Gameplay/audio allocation measurements — 2026-10-24 UTC

Increment 57 measures successful C++ allocation calls and requested bytes in the
actual gameplay renderer and audio worker. It changes no rendering or audio
algorithm. The largest recurring measured source is the HUD/status scope. The next
increment should reduce its repeated string construction and remeasure the same
workloads, rather than claim a speedup from these instrumented runs.

Implementation: `31861ad` (GNU 16.2.1 Release, `-O3 -DNDEBUG`, sanitizers off).
Host: i5-4300U, Linux 7.2.8-9-omarchy x86_64, Intel HD Graphics 4400,
OpenGL 4.6 / Mesa 26.2.5-arch1.1. All six gameplay runs used a stable **1050×1360**
managed viewport, accepted the VSync-off request, verified all 3,000 commands and
completed with zero invalid GPU queries. The first 30 frames are excluded from
allocation totals. This is not a 1920×1080 or physical scanout measurement.
See [scope/reproduction contract](ALLOCATION_PROFILING.md) and
[commands, tests and environment](../validation/2026-10-24-allocations/RESULTS.md).

## Gameplay demand

The complete allocation objects match exactly across all three repeats per level.
Each row below is one 2,970-frame measured run; requested bytes are cumulative
allocation demand, **not live memory or a leak**.

| Scope | Facility calls | Facility requested bytes | Annex calls | Annex requested bytes |
| --- | ---: | ---: | ---: | ---: |
| Simulation/replay | 162 | 105,756 | 132 | 101,497 |
| Minimap submission | 0 | 0 | 0 | 0 |
| World submission, including replay-status construction | 5,940 | 184,140 | 5,940 | 184,140 |
| HUD/UI submission | 62,278 | 2,192,022 | 63,912 | 2,229,530 |
| Presentation | 0 | 0 | 0 | 0 |
| Enclosing frame | 68,380 | 2,481,918 | 69,984 | 2,515,167 |

The frame row includes the other rows; do not sum it with them. On these runs the
subscope totals equal the frame totals, but polling/query/diagnostic gaps are still
part of the enclosing scope and could allocate on other workloads/drivers.

HUD/UI allocates on every measured frame: a maximum of 23 calls / 794 requested
bytes in Facility, and 22 / 763 in Annex. World submission records exactly two calls
and 62 bytes per frame; it includes constructing the replay-status argument before
`draw`, so those counts must not be attributed solely to GPU submission internals.

Simulation has exactly one allocating sample per replay. The fixture includes a
restart at command index 1500, and the implementation reconstructs scene/collision
state on that command; this is the expected allocation exception. The aggregate
report does not retain sample indices or allocation call stacks, so the correspondence
is an inference from the fixture and code, not independent per-allocation attribution.
Loading/restart allocations remain separate from the recurring HUD optimization.

The final hashes/ticks are unchanged across repeats:
Facility `3859428580428616961` / 1,381;
Annex `16330525715750492017` / 702.
Raw reports: Facility
[1](../validation/2026-10-24-allocations/facility-1.json),
[2](../validation/2026-10-24-allocations/facility-2.json),
[3](../validation/2026-10-24-allocations/facility-3.json);
Annex [1](../validation/2026-10-24-allocations/annex-1.json),
[2](../validation/2026-10-24-allocations/annex-2.json),
[3](../validation/2026-10-24-allocations/annex-3.json).

## Audio demand

Each run executes 3,000 producer update/collection samples, completes the Facility
at tick 1,381, starts six voices, completes four one-shots, stops the machine loop
and leaves only the music loop active. Every measured producer, command/mix and
transport sample records **zero observed C++ allocations and requested bytes**.

| Run | Producer samples | Worker command/mix samples | Transport step samples |
| --- | ---: | ---: | ---: |
| [1](../validation/2026-10-24-allocations/audio-1.json) | 3,000 | 25,218 | 25,218 |
| [2](../validation/2026-10-24-allocations/audio-2.json) | 3,000 | 19,935 | 19,935 |
| [3](../validation/2026-10-24-allocations/audio-3.json) | 3,000 | 25,061 | 25,061 |

These are real worker calls through the ALSA adapter using the silent `null` device.
The different block counts are expected: null consumes without hardware pacing,
and thread scheduling determines how many blocks run before shutdown. The workload
sleeps on the producer to drain commands; it is not an audio timing benchmark.
Startup worker silence is included, while producer startup decoding/registration
is outside its measured scope. No physical device or recovery allocation claim follows.

## Interpretation and remaining scope

Global C++ replacements do not count direct C allocation routines, GPU objects,
allocator overhead or driver-private storage. Counts describe allocations reaching
the replacement in the calling thread; library calls can contribute. There is no
whole-engine allocation-free, peak-live-memory, leak, real-time or speedup claim.
Instrumentation perturbs timings and scheduling; no timing comparison against
uninstrumented historical results is made. Timing/query metadata is retained so
workload and viewport changes remain visible.

Next: cache or otherwise avoid recurring HUD/status formatting allocations, preserve
visible text and gameplay behavior, and repeat these exact scopes/workloads. Menu,
reload, error/recovery, physical device and full-process memory coverage remain
separate work, along with the declared release workload/budget and manual acceptance.
