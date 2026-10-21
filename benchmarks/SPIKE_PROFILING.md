# Reload and transition profiling

Run the declared workload through the normal Feature Lab renderer:

```sh
./build/release/feature_lab --spike-report build/spikes.json
```

`--spike-report FILE.json` runs `reload_transition_v1`: 3,600 rendered frames, one
scripted fixed simulation step per frame (except transition attempts), seed 1,
30 warmup frames, and Facility → Annex → Facility. Completion-idle frames remain
in the distributions. The only additional options accepted are `--assets`,
`--no-audio`, and `--no-vsync`. Device audio and VSync are disabled; default config
is used without opening user storage. Live input is ignored except Escape/close.
The mode omits the normal no-VSync sleep. The actual managed viewport is reported;
a resize after warmup makes the run incomplete.

This is a separate procedural workload, not an ERPL replay. Recorded replay mode
continues to lock out content reloads and transitions. The operation sequence is:

| Frame / condition | Operation | Expected result |
| --- | --- | --- |
| 120 | Reload shipped shaders from disk | New program revision |
| 240 | Compile deliberate invalid fragment source | Retain program and gameplay |
| 360 | Reload shipped shaders | Recovery succeeds |
| 480 | Reload all resident textures from disk | Atomic CPU/GPU replacement |
| 600 | Stage textures with a deliberate invalid final payload | Retain all textures and gameplay |
| 720 | Reload all resident textures | Recovery succeeds |
| 840 | Reload file-backed UI font | New font revision |
| First completed Facility frame, after fixed reload sequence | Load/stage next level, deliberately throw at preflight | Retain Facility |
| Next frame | Advance to Annex through `SceneFlow` | Publish staged level and reset route |
| Completed Annex, next frame | Advance to Facility through `SceneFlow` | Publish staged level and reset route |

With the shipped assets, transition attempts occur on frames **1382, 1383 and 2086**.
The final Facility completes too; the final hash is `3859428580428616961`, with
1,381 ticks in the final level. The CPU reference independently runs the scripted
route and transitions and checks native event frames, hashes, levels and revisions.
Reload failure injection uses existing diagnostic paths; transition failure is
injected after texture compatibility checks and overlay staging. No asset files
are changed by this mode. This is successful reload of unchanged content, not a
measurement of authoring edits, filesystem watching/debounce or cold disk access.

## Timings and report fields

Schema `feature_lab_spike_profile_v1` shares the existing replay profiler's build,
machine, run, totals, GPU-query metadata and aggregate `milliseconds` fields.
Its workload identifies `reload_transition_v1`, seed and frame limit, rather than
claiming a replay fingerprint. `workload.level` names the final level.

CPU scopes retain their existing definitions, with `simulation` now including
scheduled reload/transition calls and verification work before rendering. Frame
time begins before event polling and ends after swap; it excludes subsequent
sample insertion, resource accounting, GL-health checks and title updates.
`gpu_render` measures the later minimap and window world/UI submissions. It does
**not** measure GPU upload/compilation work before that query; deferred costs can
still appear during the first draw. GPU queries never wait for pending results.

`spikes.events` retains ten event records, each with:

- Render-frame tag, action, CPU wall `operation_ms`, applied status and verification.
- Before/after gameplay hashes as exact decimal strings, level (0 Facility / 1 Annex),
  shader/texture/font revisions, texture/target counts and payload bytes, and font bytes.
- Failure diagnostic, bounded to 4,096 bytes, with an explicit truncation flag.
- `staging`: explicit owned-payload CPU, GPU and simultaneous combined peaks, plus
  remaining staging bytes at return; see the coverage contract below.
- `nearby_frames`: up to five frames, from two before through two after the event,
  each with CPU timings and a delayed GPU sample, or JSON `null` if uncollected.
  `cpu_scope_order` gives the order of the CPU array. Adjacent events can share
  neighborhood frames; do not count those duplicates as independent measurements.

`operation_ms` surrounds the operation call and diagnostic capture, including normal
reload logging, parsing, staging and publication/cleanup. It excludes before/after
hashing, invariant checks and rendering. These are CPU elapsed timings, not isolated
CPU execution or GPU completion times. An event frame's total can be dominated by
presentation or scheduling; an association alone does not establish the cause.

Visual reloads and expected failures must preserve gameplay hashes and tracked
resources. Successful operations advance only their relevant revision; transitions
must change level/hash without changing the resident resource inventory. The
post-warmup resource baseline is checked every frame. These counts describe live
engine payloads. The separate staging counters cover selected temporary payloads;
neither metric measures process RSS or driver memory.
Storage is bounded: 3,600 frame records and ten events are reserved before rendering.
Only event neighborhoods are serialized; global distributions still cover all
3,570 measured frames. Startup loading and final report writing are outside them.

## Staging memory coverage

Each event adds `staging.coverage = "owned_payload_v2"`, `measured`,
`cpu_peak_bytes`, `gpu_peak_bytes`, `combined_peak_bytes`, `cpu_remaining_bytes`
and `gpu_remaining_bytes`. These are byte counts, not timings. Combined peak is
the maximum simultaneous CPU + GPU staging payload, **not** the sum of separate
category peaks. Both remaining counts must be zero for event verification.
The independent CPU route reference leaves `measured=false`; native runs require
measurement on every event. Old reports without this additive object have no
staging evidence. Historical `owned_payload_v1` reports start transition accounting
after construction; v2 extends the coverage without changing the JSON field layout.

One synchronous, thread-local measurement scope surrounds each operation. Explicit
RAII leases track the lifetime of these selected payloads until disposal or
publication; shared texture/animation data is counted once. No allocator hooks,
sampling thread, GPU waits or allocations are introduced by the counters.

| Operation | Covered CPU payload | Covered GPU payload |
| --- | --- | --- |
| Shader reload/recovery | Loaded vertex/fragment source lengths and file-read vectors during text conversion | None: compiler/program storage is opaque |
| Shader rejection demo | None: both source strings are static | None |
| Texture reload/rejection/recovery | Encoded ETEX vectors while decoding; decoded RGBA vector capacities; staged-entry and replacement-vector capacities | Successfully allocated candidate RGBA texels, until publication/cleanup |
| Font reload | Encoded EFNT vector; decoded atlas/glyph vector capacities; renderer's separate candidate atlas/glyph copy | Candidate font RGBA texels |
| Level transition/rejection | Source/read buffers, decoded tile/animation vectors, tilemap construction buffers, scene parser candidate/parent/token vectors, game-construction buffers, completed candidate and minimap walls | None: levels reuse the resident texture bank |

Loading now follows an explicit order: scene text, tilemap decode, animation decode,
then game construction and preflight. Encoded binary files are released after each
decode. Counters transfer between decoder, loader, constructor and completed
candidate without counting shared payloads twice. Tile/animation parsers update
their leases as vectors grow, before trailing-data and semantic validation. Scene
parsing counts its candidate's core buffers immediately after the first entity
is created, plus pending-parent and token-vector slots, including failures before
a complete scene exists. New empty scenes reserve no core vectors; first creation
reserves their full capacity, retained through destruction and reused by later
entities. Game construction counts its collision buffers alongside the parser's
scene, then refreshes gameplay/component buffers as they are built. The empty world
placeholder and scene move destinations no longer reserve redundant core vectors.
These changes retain the `owned_payload_v2` coverage definition.

Accounting retains existing `Scene`, `TileMap` and `CollisionWorld::buffer_bytes()`
conventions: collision includes its fixed seen-bitset; gameplay includes reserved
scratch vectors and packed collection bits. Completed candidate/overlay overlap
and immediate disposal of rejected overlay staging remain covered.

This coverage still excludes nested string character storage (including token
characters), validation hash nodes, file-stream buffers, allocator/control-block
overhead, diagnostics, profiler storage, shader compiler/program memory, driver
upload copies/alignment and deferred GPU retirement. Source-string lengths remain
logical bytes. Explicit checkpoints do not capture allocator-internal reallocation
overlap, failed allocations, or every intermediate during member initialization
and optional copy/move construction. Existing live resources are outside staging; these are
**covered-payload peaks, not total operation/process memory bounds**. GPU bytes
describe requested texel payload, without claiming physical residency. Texture
rejection validates all candidates before GPU allocation, so its GPU peak is zero.

## Collision buffer utilization

`spikes.collision_memory` adds `successful_snapshots_v1` coverage without changing
the workload or `owned_payload_v2` staging counters. A bounded two-level collector
observes the published game after each rendered frame, including warmup, and
retains maxima across replacement worlds. Each world tracks successful updates
from construction, so initial `prime` batches are included even though their
trigger events are suppressed before gameplay.

Each observed level reports total `reserved_bytes`, `fixed_bytes` for the seen-pair
bitset, and five named `groups`: `bodies`, `cells`, `contacts`, `active_sensors`,
`events`. Each group contains:

- `reserved_bytes`: capacity bytes across its published/scratch vectors.
- `entry_bytes`: size of one vector element on the measured build.
- `buffers`: vector count (one for cells, two for the other groups).
- `peak_entries`: largest single candidate-vector size in a successful update.

These counts measure workload demand, not simultaneous occupied bytes or RSS.
Peaks need not occur together. Failed update attempts leave peaks unchanged, and
unpublished transition candidates are outside this collector. A partial report
lists only levels actually observed. Collection and world counters allocate no
heap storage; JSON serialization happens afterward. Native tests compare the entire
object with the independent CPU route reference.

The default worst-case capacities remain unchanged. Smaller observed peaks are
not sufficient to lower general engine limits: disjoint batches of 256 overlapping
sensors can require 32,640 contacts and 65,280 exit/enter events in one update.
Explicit per-world contact/event/membership budgets allow applications to reserve
less while retaining body/query capacity. Feature Lab chooses 16 contacts, 32 events
and 32 memberships (four times its two participating shapes' combined cell bound);
reload candidates preserve those budgets. Overflow rejects the candidate before
publication and leaves successful utilization peaks unchanged. These reservations
change reported bytes without changing either coverage definition.

## Scene storage utilization

`spikes.scene_memory` adds `successful_mutations_v1` coverage. Like collision
collection, it observes only published worlds after each rendered frame, includes
warmup and retains independent per-field maxima across replacement worlds for
each level. World counters include successful mutations before first observation,
so initialization, deletions and deferred queues flushed within a tick are visible.
Unpublished transition candidates are excluded; partial reports list only observed
levels. Native tests compare the entire object against the CPU route reference.

Each level contains `reserved_bytes`, core groups `slots`, `nodes`, `pending`, and
`components`. Core groups report `reserved_bytes`, `entry_bytes`, `peak_entries`.
Components report `pools`, `dense_reserved_bytes`, `sparse_reserved_bytes`,
`dense_capacity_entries`, `sparse_capacity_entries`, `peak_entries_sum`, and
`peak_dense_bytes_sum`. Component peaks sum per-pool maxima; sparse capacities
include missing-entry sentinels. Pool types are deliberately aggregated without
implementation-specific type names or unordered registry ordering.

Fields can peak in different worlds or at different times. The reported total
sums independently retained reservation maxima, not a simultaneous process-memory
peak. Entry demand and component peak-byte sums are likewise not simultaneous used
bytes. Reads and bounded two-level aggregation allocate nothing. JSON serialization
happens afterward. Per-scene budgets scale core and component pool reservations
together; Feature Lab uses 256 entries while the engine default remains 4,096.
Both utilization and staging coverage stay unchanged. Strings, registry nodes,
allocator overhead and user-owned component
allocations are outside these measurements. See [scene API details](../docs/FORMATS.md#runtime-component-pools).

## Completion and failures

Success requires the frame limit, all ten verified events, a stable viewport/resource
inventory, and no invalid GPU pairs. Expected rejected operations are successful
**tests of retention**, and remain visible with `applied=false, verified=true`.
Unexpected operation outcomes stop after rendering that frame, save an incomplete
report with `verified=false`, and return nonzero. Escape/close also saves a partial
report. Incompatible CLI options fail before writing; startup errors, fatal GL errors,
process termination or output-write failures can prevent a new report. An older
file is not evidence of a completed new run: inspect its build/workload/completion.

Native CTest runs compare the full workload to the independent CPU reference and
exercise unexpected shader, Annex tilemap, malformed Annex scene and 257-entity
Annex scene failures using private asset copies. The latter exceeds Feature Lab's
256-entry budget. The failing Annex attempts fail during the expected rejection at
frame 1382 and again on the required transition at frame 1383; the latter is
unverified and stops the run. All retain state and clean staging counters. CPU
tests additionally cover truncated files, quoted tokens, missing parents and
missing gameplay roles, followed by repair/retry. Package testing
runs the workload from a relocated archive with a deliberately misleading working
directory. Real hardware behavior, driver caches, compositor scheduling, thermal
state, audio-device recovery and watching real editor writes require separate checks.
