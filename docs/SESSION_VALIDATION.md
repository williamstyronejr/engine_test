# Bounded repeated-session regression

`long_session` exercises repeated gameplay, transactional reload failures and
resource retirement in one process. It is a correctness fixture for R6 coverage,
not a frame-time benchmark or a real-time endurance certification.

## Fixed workload

The test completes **64 routes**, alternating Facility and Annex, with a maximum
of **3,000 route updates per cycle**. It uses the normal `Game`, `Replay`,
`SceneFlow`, `SceneAssets`, `TextureAssets`, `Soundtrack` and save/load APIs.
Each cycle must collect all three cores, finish the door exactly once, win and
advance through the production level-transition path. The same renderer, resident
texture registry, scene coordinator and save directory survive the entire run.

At route step 120 in every cycle, it:

- Saves the live checkpoint, advances the live game, restores the save and checks
  exact checkpoint bytes. Injects a bad checksum through the private asset API,
  verifies load rejection leaves state unchanged, and repairs the saved bytes.
- Replaces the private scene file with invalid text, verifies failed reload retains
  the revision, entity and checkpoint, then restores the original file and reloads.
  Successful publication must preserve checkpoint bytes and invalidate the old entity.
- Rejects an invalid texture replacement batch, verifies its revision and renderer
  inventory stay unchanged, then reloads the original resident bank. Instrumented
  texture staging must return to zero after both outcomes.
- Rejects an invalid private WAV, repairs it, publishes the original audio bank,
  and waits for retired bank bytes to return to the live bank size.
- Pauses and resumes the game and audio groups; a paused update must not advance
  the simulation tick.

A real ALSA worker uses the **null** PCM. The soundtrack is restored after save/load,
its emitter follows each level, and it restarts loops at transitions. Every eight
cycles, the output is destroyed and recreated: eight workers are opened and joined
across the run. Every completed route must settle to one active music voice with
no retired banks, queue rejections, dropped plays, stale controls or device error.
Worker waits have a three-second deadline. A one-millisecond cooperative sleep
occurs every 16 route updates; this does not synchronize simulation to real time.

A hidden 64×64 GLX window owns the context. World sprites render to a 64×64 target
every 120 route updates and at cycle completion. Each pass must submit geometry
and leave a healthy renderer. Each cycle also creates, resizes and releases a
transient target. This exercises sprite rendering and target lifetimes; it does
not exercise the full HUD, tile layers, minimap or physical presentation.

## Resource checks and limits

After each rendered pass, all eleven `Renderer::memory()` fields must equal the
resident baseline, including zero pending vertex usage. Texture/target counts
stay fixed. After explicit release of every uploaded texture and target, counts
are zero and the renderer inventory matches its pre-load snapshot.

Collision buffer bytes remain fixed. Completed-world buffer bytes match the
first completed cycle of the same level. Audio live bank bytes stay fixed and
retired bank bytes drain before the next cycle. These checks cover the named
payloads; they do not measure RSS, total live heap, driver caches, staging peaks
for every operation or post-destruction process memory. See
[renderer memory scope](RENDERER_MEMORY.md) and
[workload acceptance](../benchmarks/WORKLOAD_ACCEPTANCE.md).

The fixture prints every cycle and a final record containing cycles, maximum
steps, committed gameplay ticks, render passes, output lifetimes, elapsed seconds
and resource restoration. Simulated duration is ticks divided by 60. A restored
speculative update is not counted in the committed tick total. Compiler/sanitizer
runs must agree on the workload counts; elapsed time is diagnostic only.

The initial validated workload commits **66,720 ticks** (1,112 simulated seconds,
18 minutes 32 seconds), renders **640 passes**, and completes all 64 cycles.
This accelerated workload does not prove stability during 18 minutes of wall time.
It uses private copied assets and private `UserStorage` beneath the test scratch
root. Original assets and the user's settings/saves are not edited.

## Run and interpret

With the project's installed dependencies and a working display:

```sh
cmake --preset debug
cmake --build --preset debug --target long_session_tests -j2
ctest --preset debug -R '^long_session$' --output-on-failure
```

The test is native-only, labeled `hardware`, runs serially and has a 180-second
CTest timeout. Without DISPLAY it returns skip code 77; a skip is not a pass.
Use the [disk-backed scratch recipe](RELEASE_READINESS.md#reproduce-the-automated-checks)
for compiler and private test files. No benchmark ceilings, timeout adjustments
or sanitizer suppressions are introduced by this fixture.

[Validation evidence](../validation/2026-10-30-long-session/RESULTS.md) records the
source commit, exact commands, fresh targeted builds, per-cycle output and the
initial save-corruption fixture error. Related unit groups continue to cover
individual behaviors; this fixture adds their repeated composition.

For this original fixture, settings/HUD coverage is outside its scope; the
companion below adds that coverage separately. Physical input/display (R4), audible
sound/device disconnect/suspend (R5), wall-time endurance, host timing instability,
broader memory coverage (R6), and support/signoff (R8) remain open. It does not
exercise physical audio, driver leak detection, artificial device failure or
background loading. Passing this test does not complete milestone 6.

## Full gameplay/UI companion (increment 77)

`ui_session` adds **16 completed routes** (eight per level) through the production
`draw_game`, `Overlays::render_minimap`, `Diagnostics::draw_hud` and `Settings::draw`
paths. It keeps one renderer, texture bank, minimap, collision/sampling galleries,
settings model and null-PCM soundtrack alive across all cycles. The original
64-cycle `long_session` fixture remains unchanged and separately scoped.

At route update 120, each cycle renders and reads back these 15 states in order:

1. Gameplay with its HUD, tiles, world sprites, sampling gallery and minimap.
2. F2 diagnostics, then F2 with F3 collision shapes.
3. Settings over diagnostics, then the changed mute setting.
4. Controls, binding capture, canceled capture and accepted modified-key binding.
5. Return to settings and resume gameplay with diagnostics/shapes closed.
6. A 320×240 render target, then restoration to 640×480.
7. Rejected font reload retaining the live font, then repaired font reload.

The grouped list above describes 15 distinct captures, not 15 separate CTest tests.
Each capture hashes the complete PPM readback with a deterministic 64-bit FNV-style
fingerprint, including image dimensions. The temporary image is reused inside the
private copied-asset directory and removed with the fixture. Fingerprints and
state labels remain in the validation record; screenshots are not retained or
presented as a human visual review. All 240 fingerprints must match across the
four tested compiler/sanitizer configurations on this host. Matching fingerprints
are a regression check, not evidence of readability or compatibility on another GPU.

Menu opening must pause gameplay and block a held movement input. A diagnostics
key routed through an open menu must leave the underlying overlay unchanged.
Escape cancels capture without closing controls; accepting a binding checks its
symbol and modifiers. Mute changes must reach the live soundtrack. Closing the
menu restores the exact pre-menu checkpoint, and a private config write/read
round trip must retain audio settings, vsync preference and bindings. These checks
call production UI and storage APIs with logical `InputFrame` values; they do not
exercise physical keys, native keymap routing or all application-controller glue.

Each level must collect its three cores, finish its door exactly once and win
within 3,000 route updates. The production transition stages new minimap walls and
publishes them with the new game. Every cycle settles to one active music voice
without rejected commands, dropped plays, stale controls or device errors.
The fixture performs **328 full passes and 240 readbacks**, committing **16,664
gameplay ticks** (277.733 simulated seconds). This accelerated workload is not a
wall-time soak. Diagnostic CPU history is fixed synthetic display data; no GPU
timing queries or performance-acceptance measurements are made.

All eleven renderer memory fields and live texture/target counts are checked
after every pass. At 320×240, only the output/minimap RGBA8 target payload may
change, and its exact expected byte count is calculated from both dimensions.
Returning to 640×480 must restore the resident baseline. Invalid font input must
retain its revision and inventory; a repaired reload advances the revision with
unchanged owned font storage. The minimap must submit geometry, and the full
pass must submit more than 100 quads with a healthy renderer.

At final teardown, the test releases its output target and resident game textures
and restores an explicitly installed embedded font. The inventory must match the
baseline captured after creating/warming the presentation helpers. Those helpers
retain five textures and one minimap target until the renderer is destroyed;
**the final assertion does not claim zero GPU resources or zero process memory**.
The embedded font is installed before the baseline so copied vector capacity is
compared consistently. Driver memory, whole heap/RSS, uninstrumented staging and
post-destruction process memory remain outside these checks.

```sh
cmake --preset debug
cmake --build --preset debug --target ui_session_tests -j2
ctest --preset debug -R '^ui_session$' --output-on-failure
```

This native-only group is serial, labeled `hardware`, has a 180-second timeout,
and skips with code 77 without DISPLAY. The GLX window stays hidden at 64×64;
640×480 and 320×240 are offscreen targets, not physical window/display acceptance.
The [full UI validation record](../validation/2026-10-30-ui-session/RESULTS.md)
retains commands, complete output and frame fingerprints. No real user settings,
installed assets, physical sound devices or display configuration are changed.

This extends full rendering and modal UI composition coverage. Physical R4/R5
checks, terminal audio-device failure presentation, real-time endurance, timing
stability, broader memory acceptance and final support/signoff remain open.

## Audio failure/UI companion (increment 78)

`audio_ui_session` adds 16 complete routes with the full gameplay, minimap,
settings and diagnostic draw paths. Each of four cases runs twice on each level:
terminal device failure during a held mute-checkbox press; terminal failure during
binding capture; audio explicitly disabled at construction; and a private invalid
WAV causing startup disablement. The last case repairs the WAV immediately after
construction. Each route collects all three cores, completes its door once and wins.

An executable-local linker wrapper returns `-ENODEV` on one armed ALSA write.
Eight injected failures pass through the real worker's transport/error path and
`Soundtrack::update`, which destroys/joins the failed output. All other writes use
the actual ALSA null PCM. This exercises a controlled terminal error, not physical
unplugging, audible playback or suspend/resume. Constructor-disabled audio must
make no PCM writes. Invalid-WAV startup may briefly run a null worker before it is
joined; the test does not interpret that as a device failure.

At route update 120, the fixture verifies that availability changes rebuild the
open settings panel: a still-held checkbox capture is cleared, its stale release
cannot change settings, and all four audio widgets are disabled and ignore clicks.
Binding capture survives loss of audio, remains cancelable and preserves bindings.
Audio preferences survive unavailability and a private configuration round trip.
Closing settings restores exact checkpoint bytes and gameplay completes normally.

The disabled object reports `AUDIO OFF` and `AUDIO BANK OFF`, with zero live/retained
sound-bank bytes. Reload after repairing the asset, settings configuration,
checkpoint restoration and an audio restart input must not create more PCM writes
or reconnect that object. At the end of each route the fixture explicitly constructs
a new `Soundtrack`, checks the completed game's single music voice, and verifies
that audio widgets work again and mute reaches the new output. This models the
existing manual session-recreation contract; it adds no automatic reconnection or
new recovery button to Feature Lab.

Each cycle captures `before`, `unavailable`, `resumed_disabled`,
`completed_disabled` and `manual_restart`; four binding cases also capture
`capture_disabled`. Complete PPM fingerprints cover all **84 readbacks** across
four compiler/sanitizer configurations on this host. They compare equivalent runs,
not enabled/disabled pictures against each other or against a human-approved image.
Temporary images are removed with the private fixture. Diagnostic CPU history is
synthetic; no GPU timer or performance measurement is enabled.

The workload commits **16,664 ticks** (277.733 simulated seconds), with **156 full
passes**, each preceded by the production minimap pass, plus one minimap warmup.
All eleven renderer inventory fields and live texture/target counts remain at the
resident baseline after every pass. Releasing the output/game textures and restoring
the embedded font returns to the presentation-helper baseline: five helper textures
and one minimap target remain until renderer destruction. Audio-bank accounting is
scoped to the soundtrack API. Neither check covers whole heap/RSS, driver residency
or post-destruction process memory. The original two session fixtures are unchanged.

```sh
cmake --preset debug
cmake --build --preset debug --target audio_ui_session_tests -j2
ctest --preset debug -R '^audio_ui_session$' --output-on-failure
```

The group is native-only, serial, labeled `hardware`, times out at 180 seconds and
skips with code 77 without DISPLAY. Worker conditions have three-second deadlines;
route loops have 3,000-update limits and cooperative one-millisecond yields every
16 updates. A hidden 64×64 GLX window supplies a 640×480 offscreen target. Logical
input and direct production calls do not cover physical input or all main-loop glue.
See [validation evidence](../validation/2026-10-30-audio-ui-session/RESULTS.md).
Physical R4/R5 acceptance, real-time endurance, timing stability, broader memory
acceptance and support/signoff remain open.
