# Settings, diagnostics and audio-HUD allocation coverage

Increment 59 adds the development-only `ui_allocations` executable. It measures
production drawing and settings-update code in declared, frozen UI states. The
existing replay and audio-worker workloads keep their original scope and behavior.
The game drawing function now lives in `examples/feature_lab/game_draw.hpp`, shared
by ordinary gameplay and the probe; its rendering body is unchanged.

## Reproduce

```sh
cmake --preset release
cmake --build --preset release --target ui_allocations -j2
ctest --preset release -R '^ui_allocations$' --output-on-failure
./build/release/ui_allocations build/ui-allocations.json
# Optional PPM captures, written after measurement:
./build/release/ui_allocations build/ui-with-images.json build/ui-images
```

`BUILD_TESTING=ON` and the native platform build are required. The executable uses
an X11/OpenGL context and ALSA `null`; it returns 77 without `DISPLAY`, which CTest
reports as a skip. Failure to initialize the requested audio device or start its loops, an incomplete
fixture, changed gameplay/render totals or a failed report write returns nonzero. A complete
report is written only after all scenarios finish. No user configuration or save
storage is loaded or written. The executable is not installed in release packages.

## Workload and states

`frozen_ui_states_v1` renders nine states in each of Facility and Annex:

| State | Fixture |
| --- | --- |
| `audio_off` | Closed menus, audio disabled |
| `audio_default` | Enabled ALSA null soundtrack, default controls/gains |
| `audio_remapped` | International and mouse chords; muted, music 15%, effects 25% |
| `settings_default` | Open settings, enabled audio and default labels |
| `controls_remapped` | Open controls page with remapped labels |
| `binding_capture` | Waiting for a replacement binding |
| `binding_updated` | Accepted Ctrl+Shift+OUML replacement through real menu input |
| `diagnostics` | Visible diagnostics with fixed CPU history |
| `diagnostics_shapes` | Same diagnostics plus collision/camera shapes |

Each scenario creates its own renderer/resources and soundtrack, runs 90 scripted
gameplay commands, sets its UI state, then renders 300 frames to a 1280×800 target.
The first 30 frames warm renderer/string capacity; 270 are measured. Gameplay is
frozen after setup. The probe checks requested menu/capture states, active audio,
nonempty stable drawing and an unchanged gameplay hash/tick count. It waits for
both soundtrack loops and some null-device output before warmup. The audio worker
is joined before serialization, and its allocations are outside these render-thread
snapshots. This is not an end-to-end gameplay or audio transport benchmark.

Diagnostics uses 240 fixed `{wall_ms=16.75, cpu_ms=2.25}` samples, fixed previous-pass
counters and 125 ms dropped-clock text. Its GPU history is empty and its default
unsupported-timing label is displayed. The probe does not simulate completed GPU
queries, pending/invalid-query changes, live frame-time distributions or resize.
Those branches retain existing functional tests and need separate allocation coverage.

Remapped actions include Ctrl+Shift+Alt+EACUTE for left movement, SIDE2 for interact,
NTILDE for mute, Ctrl+Shift+U+03BB for music and Ctrl+Shift+Alt+U+1F642 for effects.
The report retains the actual audio and first binding labels. The controls/capture
fixtures use normal pointer press/release and key-symbol input through `Settings`;
they do not change private menu state directly.

## Measurement boundaries

Counters use the same successful global C++ `new`/`new[]` replacements as
[allocation profiling](ALLOCATION_PROFILING.md). All aligned/nothrow forms are
covered. Snapshots are local to the rendering thread, and each scope reports
`samples`, `nonzero_samples`, `calls`, `requested_bytes`, `max_calls` and
`max_requested_bytes`.

| Scope | Included work |
| --- | --- |
| `settings_update` | Idle `Settings::update` with empty input |
| `minimap_submit` | Minimap pass, including target-size check |
| `world_submit` | Main-target begin, world drawing and optional debug shapes, through the existing world boundary |
| `hud_submit` | Shared ordinary gameplay HUD and map/status overlays |
| `diagnostics_submit` | Diagnostics text/geometry; no-op when hidden |
| `settings_submit` | Settings/controls UI text/geometry; no-op when closed |
| `render_end` | Final renderer flush/end |
| `frame` | Inclusive sum of all preceding scopes |

The `frame` is an offscreen render/update iteration, with no window-event polling,
swap/presentation, pacing, simulation or sound-producer update. Do not compare it
as if it were the enclosing replay `frame`. The fixed level/status header is
formatted before this loop; live header updates remain covered by the replay
workload, not by this probe. Buffer reservations also precede warmup.

Separate `transitions` record one sample when opening settings, entering controls,
starting capture and accepting a rebind. They expose layout/string construction
costs instead of mixing them into steady redraws. Asset loading, resource creation,
initial scripted simulation, soundtrack setup, screenshots and report serialization
are excluded. Transition teardown/deallocation is not a requested-byte measurement.

## Report and regression checks

The `feature_lab_ui_allocations_v1` JSON report identifies source revision,
compiler/configuration/flags, GL device, viewport, warmup, thread/device contract,
all 18 named scenarios, exact labels, gameplay hashes/ticks, final per-frame render
counts, measured scopes and separate transition counters. Byte values are cumulative
successful requests, not live/peak heap usage. The checker verifies scenario/state
coverage, labels after rebinding, sample counts, nonempty rendering and equality
between inclusive-frame totals and their subscopes. Menu opening provides a positive
allocation control. The [diagnostics follow-up](DIAGNOSTIC_TEXT.md) additionally
requires zero measured `diagnostics_submit` calls after warmup in the reserved-buffer
fixtures; other exact totals remain measurements, not whole-engine budgets.

[Recorded results](2026-10-25-ui-allocation-results.md) retain three repeated Release
runs from the original measurement increment. The subsequent
[diagnostics optimization](2026-10-26-diagnostic-text-results.md) compares the same
workload; neither establishes whole-engine allocation-free behavior. Direct C allocation, driver/GPU
memory, allocator metadata, physical audio/display acceptance, live input/window
processing, simultaneous menus/diagnostics and changing GPU-timing labels remain
outside this workload. No speedup, lower RSS or frame-time budget follows.

The separate `diagnostics_allocations` executable extends presentation coverage to
synthetic GPU-timing histories and combined overlays; see its distinct
[workload and exclusions](DIAGNOSTIC_TEXT.md). It leaves this original workload intact.
