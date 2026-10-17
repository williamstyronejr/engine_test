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
engine payloads, not transient staging allocations, process RSS or driver memory.
Storage is bounded: 3,600 frame records and ten events are reserved before rendering.
Only event neighborhoods are serialized; global distributions still cover all
3,570 measured frames. Startup loading and final report writing are outside them.

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
exercise an unexpected shader failure using a private asset copy. Package testing
runs the workload from a relocated archive with a deliberately misleading working
directory. Real hardware behavior, driver caches, compositor scheduling, thermal
state, audio-device recovery and watching real editor writes require separate checks.
