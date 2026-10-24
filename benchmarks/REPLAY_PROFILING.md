# Recorded gameplay profiling

The [2026-10-17 results](2026-10-17-replay-results.md) preserve three runs per level,
including one rejected GPU timing attempt.

`feature_lab --replay FILE.erpl --profile-report FILE.json` profiles the real
Feature Lab rendering path: the minimap, world, text/status panels and normal HUD.
It verifies every recorded command's tick and resulting gameplay hash through the
same playback function as ordinary replay. One command advances per rendered frame,
with interpolation alpha 1. Paused commands still render but may perform no simulation
step. This is an unpaced workload, not wall-clock-driven live input or a latency test.

```sh
./build/release/feature_lab --replay benchmarks/replays/facility.erpl \
  --profile-report build/facility-profile.json --profile-warmup 30
./build/release/feature_lab --verify-replay benchmarks/replays/facility.erpl
```

Profile mode uses default configuration without opening user configuration/save
storage, disables the physical audio device, requests VSync off, and omits the
normal no-VSync 1 ms sleep. Only `--replay`, `--profile-report`, `--profile-warmup`,
`--assets`, `--no-audio` and `--no-vsync` are accepted together. Asset watching,
recording, screenshots, partial frame limits, demos and slot operations are rejected.
Escape/window close aborts; live movement, fullscreen and menu controls cannot
alter the workload. These restrictions do not change ordinary replay mode.

A nonempty replay may contain at most 36,000 commands. Warmup defaults to 30 and
must be less than the command count. It is a prefix of the replay, not a replay of
those commands before restarting. All commands, including warmup, remain verified.
Timing storage is reserved before the window opens. A completed run with zero
invalid GPU timestamps returns zero; an abort, viewport change after warmup, or
invalid GPU timestamp returns nonzero with a report. Earlier fatal errors such as
replay divergence can leave a previous report intact. Reports are written atomically
at normal shutdown. A report path referring to its input replay is rejected.

## Timing and report contract

The schema is `feature_lab_replay_profile_v1`. It records build revision/compiler/
flags, CPU/kernel and GL device/driver, canonical encoded replay FNV-1a fingerprint,
command count, level, warmup, final gameplay hash/tick, viewport, draw/quad counters,
actual simulation-step count, VSync-request acceptance and GPU query diagnostics.
Fingerprints identify workloads, not cryptographic authenticity. Scene/map/animation
content identity is checked when restoring the replay checkpoint. Visual asset edits
can change rendering without changing the gameplay hash; compare identical asset
revisions as well as report metadata.

The requested window is 1280×720; the window manager chooses its actual size.
The report gives the first measured viewport. Startup resizing within warmup is
allowed; any later viewport change makes `run.complete` false. These are native
window measurements, not fixed 1920×1080 offscreen stress measurements. GPU work
and CPU culling/UI scale depend on the actual viewport.

Each distribution reports samples, mean, lower median, p95, p99 and maximum in ms,
using sorted index `floor((n-1)*percent/100)`. Zero samples means unmeasured, not free.
Counters and CPU distributions exclude warmup. Timing scopes are:

- `simulation`: from after event polling through replay command decoding, tick/hash
  verification, gameplay update, disabled-soundtrack bookkeeping and collection.
  It includes loop/control overhead; it is not a timer around only `Game::update`.
- `minimap_submit`: GPU begin marker, camera computation and the full minimap pass,
  including target resize checks and render submission.
- `world_submit`: window-pass begin and world tiles/sprites/gallery/debug/goal text,
  through the camera change that flushes world geometry before screen-space UI.
- `ui_submit`: screen-space HUD, text, scrolling status/minimap composition and
  completion of the window pass, including the GPU end marker. Closed settings and
  hidden diagnostics calls are included; title/settings interaction is not exercised.
- `present`: the window buffer-swap call, including any driver/compositor waiting.
- `frame`: from before event polling through swap completion. Includes GPU query
  collection and existing diagnostic recording. Excludes subsequent profile sample
  storage, GL health check, window-title updates and shutdown/report writing.
- `gpu_render`: asynchronous elapsed queries around minimap plus window world/UI rendering,
  excluding event handling, simulation, presentation and compositor scanout.

CPU timings include OS scheduling delays and driver back-pressure. GPU and CPU
work overlap. Separate percentiles cannot be added to reconstruct frame percentiles.
Per-frame paired raw timings, deadline-miss percentages and input-to-display latency
are not retained. GPU samples are filtered by frame tag, so delayed warmup results
remain excluded. Pending queries are never waited on at shutdown. Query totals
include warmup, while distributions do not. The first eight reversed timestamp pairs
and omitted count follow the [GPU timing diagnostics contract](../docs/FORMATS.md#asynchronous-gpu-timing).

For reload and transition event timings, use the separate
[scripted spike workload](SPIKE_PROFILING.md).

The replay profiler does not measure device audio or offline mixing, startup loading,
filesystem writes, reload spikes, or transitions between levels. It retains normal
minimap/UI allocations and all gameplay replay checks; this is not an allocation-free
or complete resource-accounting claim. A visible driver's accepted VSync-off request
does not prove an unpaced compositor or physical display.

## Declared workloads

The checked-in Facility and Annex ERPL files each contain 3,000 commands with seed 1.
They finish the existing automatic collection/exit route before and after restart, pause at command index 120,
single-step at 150, resume at 180, zoom in during 30–39, zoom out during 210–219,
and restart at 1500 before following the route again. Indices are zero-based. Each
level is a separate run, not a level-transition benchmark. Completion screens and
paused frames contribute to the distribution and must not be described as continuous
movement. The default warmup excludes the first 30 commands only.

To regenerate from the current checked-in assets (requires `BUILD_TESTING=ON`):

```sh
cmake --build --preset release --target make_profile_replay
./build/release/make_profile_replay build/facility.erpl facility
./build/release/make_profile_replay build/annex.erpl annex
```

Use a Release build, finish compilation/testing before collecting timing baselines,
and repeat each workload on the same viewport/driver/configuration. Preserve raw
reports and process exit codes, compare replay fingerprints/final hashes and draw
counts first, and retain any failed attempt. A passing replay and p99 below 16.7 ms
alone do not establish uninterrupted 60 FPS.

GPU timing defaults to `GL_TIME_ELAPSED` from increment 40. Reports include
`gpu_queries.method` and `counter_bits`. Older reports without these fields used
`GL_TIMESTAMP` pairs. Elapsed mode trusts the driver duration; its zero `invalid`
count does not independently validate the clock. Timestamp diagnostics remain
available through the C++ timer API; see [the timing contract](../docs/FORMATS.md#asynchronous-gpu-timing).

The development-only `feature_lab_allocations` executable adds C++ allocation
counts at these same boundaries. See [the allocation contract](ALLOCATION_PROFILING.md)
for warmup, inclusive totals, scope attribution and measurement exclusions.
