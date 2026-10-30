# Linux 2D Engine / Feature Lab

A C++20/OpenGL 4.6 engine built in-house for Linux. It includes native windowing,
batched textured quads, input, simulation, collision, audio, and a playable example.
Feature Lab includes transactional asset reload, paused stepping, collisions,
settings, saves, replay profiling and a relocatable release package. A CPU-only
asset validator supports text and versioned JSON reports.

This is an early engine foundation. The full plan is in [ENGINE_PLAN.md](ENGINE_PLAN.md);
[implementation status and feature coverage](docs/STATUS.md) distinguish working
features from planned work. The [release-readiness audit](docs/RELEASE_READINESS.md)
records remaining performance, persistence and physical display/audio gates with
commands and expected evidence. The [declared workload acceptance](benchmarks/2026-10-26-workload-acceptance-results.md)
records passing memory budgets and remaining frame-time failures across 18 runs. [Candidate validation](validation/2026-10-23/RESULTS.md)
retains full compiler/sanitizer results, audio ThreadSanitizer checks and a clean
offline build with relocated package checks. The [integrated readiness refresh](validation/2026-10-29-readiness/RESULTS.md)
adds current full GNU/Clang suites, CPU and focused native sanitizers, audio race
checks and a packaged documentation/entry-point audit. The [fresh native/offline follow-up](validation/2026-10-29-offline/RESULTS.md)
adds all 66 native sanitizer groups and a full Release build with networking
disabled, followed by relocated package checks. Release acceptance remains open. A [quad-corner reuse experiment](benchmarks/2026-10-28-corners-results.md)
passed rendering checks but failed its CPU adoption gate; the existing renderer
is retained, with the prototype and all measurements preserved.
[Function sampling](benchmarks/2026-10-28-functions-results.md) now identifies quad
submission as the leading application CPU symbol. The subsequent
[fixed-buffer trial](benchmarks/2026-10-28-vertices-results.md) also failed its adoption
gate; it retains new batch-capacity regression coverage and the original renderer.
The [bounded session regression](docs/SESSION_VALIDATION.md) repeatedly completes
both levels with reload rollback, private saves, audio-bank retirement and resource
restoration checks. Its accelerated simulation does not establish real-time endurance.
The [warmup/order study](benchmarks/2026-10-28-stability-results.md) retains changing
host frequency/thermal observations; neither warmup protocol establishes stability.
[Renderer memory snapshots](docs/RENDERER_MEMORY.md) now report CPU vertex storage,
logical VBO size, texture payloads and owned CPU font buffers with explicit
coverage limits. Committed font storage is separate from replacement staging.

Atomic saves also have [isolated filesystem-failure tests](docs/ATOMIC_SAVE_VALIDATION.md)
covering short/interrupted writes, cleanup and failures before/after replacement.
Zero-progress writes and directory-close errors now report failure reliably.
User storage also [prepares the full resolved directory chain](docs/STORAGE_DIRECTORY_VALIDATION.md)
before creating a save temporary, including on retries. Every ancestor must support
directory synchronization; preparation failures leave the old record unchanged.

## Build

Requirements: Linux, a C++20 compiler, CMake 3.21+, Ninja, X11 development headers,
OpenGL/GLX development headers, ALSA development headers, and a driver supporting
OpenGL 4.6 Core. Running graphics requires an X11 or XWayland session with `DISPLAY`
set. A native Wayland backend is not implemented.

CMake and Ninja were installed in the ignored project-local `.tools` environment
on the development machine. If using that environment, run this from the repo first:

```sh
export PATH="$PWD/.tools/bin:$PATH"
```

Then:

```sh
cmake --preset debug
cmake --build --preset debug -j 2
ctest --preset debug
./build/debug/feature_lab
```

The build never downloads code. On a clean machine, install the listed development
tools and system headers first. `.tools` is optional, machine-local, and not part
of the engine or a runtime dependency.

Other presets are `release`, `clang`, and `sanitize`. The `sanitize` preset builds
CPU tests only and needs neither a display nor platform development libraries:

```sh
cmake --preset sanitize
cmake --build --preset sanitize
ctest --preset sanitize
```

To validate native platform code under AddressSanitizer/UndefinedBehaviorSanitizer:

```sh
cmake -S . -B build/sanitize-platform -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DENGINE_SANITIZERS=ON
cmake --build build/sanitize-platform -j 2
ctest --test-dir build/sanitize-platform --output-on-failure
```

## Play

Normal launch opens the title screen. Use **Tab / Enter** or click:

- **New Game** starts a fresh facility without overwriting saves.
- **Continue Saved Game** loads the selected slot (1–3). Empty or invalid saves
  show an error and preserve the current session.
- **Resume Session** returns to the game retained in memory; it becomes available
  after starting or loading a game.

Use **F1 → Title** during gameplay to return here. The session and audio pause until
resumed; previously paused games stay paused. Gameplay settings remain available
through F1 in the game. Escape quits from the title screen.

`--play` skips the title. Scripted runs, explicit load/save-slot commands and the
existing feature-demo flags also start directly in gameplay for reproducible tests.


Move the cyan robot through the facility, collect three gold cores, then reach the
exit after its door opens. Machinery and pickups loop through atlas animations;
the door plays once and removes its collision barrier on completion. The minimap
shows remaining objectives and the camera view; scroll the status panel for its legend.

| Control | Action |
| --- | --- |
| WASD / arrow keys | Move |
| + / - | Zoom in / out |
| Space / P | Pause or resume |
| R | Restart |
| E | Toggle the nearby alarm switch (within 3 units, clear line of sight) |
| Page Up / Page Down | Scroll the status panel (also while paused) |
| M | Toggle master mute |
| N | Cycle music volume: 30%, 15%, off |
| B | Cycle effects volume: 50%, 25%, off |
| F11 | Request fullscreen through the window manager |
| F1 | Open / close settings (pauses gameplay) |
| F2 | Toggle diagnostics overlay |
| F3 | Toggle collision and camera outlines |
| F10 | Advance exactly one tick while paused |
| F5 | Reload the sprite shaders from the active asset root |
| F6 | Deliberately fail a shader compile; retain the working program |
| F7 | Reload all registered texture files together, preserving handles |
| F8 | Deliberately reject a texture replacement; retain working images |
| Escape | Close settings; quit when settings are closed |

Near spawn, the red circular alarm patrols a vertical track. Touching it increments
the alarm-entry counter once per entry and highlights the player; it does not block
movement or reset progress. Press E near the square switch to disable/enable it.
The alarm turns green when disabled. Movement uses swept boxes against walls;
alarm contacts are discrete, filtered trigger tests. Pause freezes the patrol.

In settings, click buttons/checkboxes or drag volume sliders. Tab / Shift+Tab and
Up/Down move focus, Enter/Space activate, and Left/Right adjust a focused slider
by 5%. The wheel adjusts the slider under the pointer. WASD retain their arrow
aliases. Held controls are blocked from gameplay until released after closing.
Closing restores the previous pause state; Restart begins an unpaused new game.
Audio controls are disabled if sound is unavailable. Values apply immediately;
changed audio/VSync settings are saved when closing the panel or exiting normally.
F11 fullscreen remains a session-only window-manager request.

Select a slot with **SLOT 1 / 3 - CHANGE**, then **SAVE** or **LOAD**. Saving replaces
that slot's checkpoint. Loading keeps the panel open and restores the checkpoint's
pause state when you resume. Empty, corrupt, incompatible, and inaccessible saves
report an error in the panel and console without replacing the live game. Checkpoints
preserve player position, collected cores, zoom, clock, and exact animation/door
progress. Audio loops restart after loading; past one-shot sounds are not replayed.

Four generated WAV assets provide stereo music, pickups, a door cue, and a looping
machine emitter. The emitter's gain/pan follow the player's position. Pause freezes
both audio groups; restart stops old voices and restarts the loops. Mute keeps their
playback positions advancing. Audio/file/queue failures leave the game playable
with an `AUDIO OFF` indicator. Use `--no-audio` to skip sound loading and device access.

```sh
# Deterministic simulation scenario; no display or audio device required.
./build/debug/feature_lab --verify

# Visible scripted run; writes a framebuffer capture before presenting its last frame.
./build/debug/feature_lab --scripted --frames 1600 --no-audio \
  --screenshot build/feature-lab.ppm

# Start with settings open for visual inspection.
./build/debug/feature_lab --settings-demo

# Start with diagnostics and collision/camera outlines visible.
./build/debug/feature_lab --diagnostics-demo

# Check real audio-device initialization and PCM progress with silence.
./build/debug/audio_probe

# Opt in to an eight-second stereo/mute/pause/stop listening sequence.
./build/debug/audio_probe --exercise
```

Audio transport now retries partial writes, handles underruns and suspended PCMs,
and records accepted frames, recovery counters and terminal errors. Failed audio
is disabled while gameplay continues; the final report retains the last diagnostics.
See [audio validation](docs/AUDIO_VALIDATION.md) for automated fault tests and the
remaining physical listening/device checks. Removed devices are not automatically
reopened.

`--scripted` advances one simulation tick per rendered frame for reproducible visual
inspection. Pause, single-step, restart, and debug toggles also work in this mode.
Normal play uses elapsed time and a 60 Hz fixed simulation step.
`--frames N` exits after N rendered frames; `--no-vsync` requests an unpaced swap and
adds a small CPU yield. A screenshot without a frame limit defaults to 120 frames.
Run `--help` for options. The screenshot format is binary PPM.

After 30 warm-up frames, the game reports CPU update/submission median, p95, and p99
for up to 8,192 frames. These exclude presentation waiting and are **not GPU timings
or full frame-time benchmarks**. Screenshot readback is excluded from CPU samples.
The window title shows FPS and the main window pass's submitted quads/draw calls.
The exit report includes separate window/minimap draw counts and live target bytes.

F2 displays the last 240 rendered frames: wall mean, CPU mean/p95, simulation ticks,
steps per frame, discarded clock time, per-pass quads/draws, culling, GPU texture and
render-target counts, collision bodies/contacts and broad/narrow-phase counts.
Frame statistics describe completed frames, so they appear one frame later. CPU
samples include diagnostic drawing overhead. Wall intervals include pacing; scripted
mode does not count ignored wall-clock time as dropped simulation time.

The adjacent memory panel shows CPU vertex/font reservations and used bytes, plus
logical GPU vertex-store and texture payloads in KiB (1,024 bytes). It samples the
renderer once before either F2 panel draws; pending vertex bytes can therefore
describe a partial batch. Used CPU bytes are part of the reservation. RSS, staging
peaks, other heap storage and driver/residency overhead are separate. See the
[snapshot and overlay contract](docs/RENDERER_MEMORY.md#f2-memory-panel-increment-73)
and [validation record](validation/2026-10-29-diagnostics-memory/RESULTS.md).

The GPU rows show mean/p95 over the last 240 completed measurements, sample age in
rendered frames, pending slots, skipped measurements, and invalid timestamp pairs
(diagnostic mode). Timing covers the minimap and window passes, including UI/debug
drawing, inside one GPU elapsed query. It excludes simulation, shader compilation, screenshot readback, and
presentation. It can include GPU scheduling delays and gaps while the CPU submits
commands, so it is a render interval rather than a pure GPU utilization measure.
CPU/GPU values come from separate histories and need not describe the same frame.

Eight slots (eight elapsed query objects) are reused only after their results are available.
A full pool skips measurement instead of waiting or overwriting pending results.
No completed samples shows `GPU WAITING`; drivers without 64-bit elapsed counters show
`GPU TIMING UNAVAILABLE`. Rendering continues in either case. The GPU exit report
includes collected/pending/skipped/invalid counts and does not wait for the last
frame to finish. F2 hides the overlay while timing continues; there is no new key.

F3 outlines actual simulation colliders: blue static, cyan moving, gold sensors,
and red bodies in contact. The cyan viewport outline and minimap show camera bounds.
F10 steps movement, animation, collisions, triggers and gameplay together while
remaining paused. A held key does not repeat. Pause and restart take precedence
when pressed with step; completed games cannot advance. Audio stays paused and
new stepped gameplay cues wait for resume. Settings capture all debug/step controls
until release. Diagnostics are session-only and do not change checkpoint formats.

## Transform gallery and camera

Near the spawn point, at world (-27, -4), a cyan arm rotates once every 240
simulation ticks. Its checker panel inherits the rotation, uses a mirrored X axis,
and pulses with nonuniform scale; a small orange satellite inherits both parents.
The hierarchy preserves shear rather than decomposing world matrices back to
rotation and scale. Walk upward from spawn to inspect it, use +/- to zoom, and F3
to see the camera bounds. The camera follows the interpolated player and clamps to
the map; a viewport wider than the map centers that axis.

Space freezes the gallery, F10 advances one tick while paused, and R resets it.
Motion derives from ticks and authored local transforms, with fractional-tick
render sampling, so saving/loading and replay reconstruct the same poses without
extra save fields. Load and paused stepping snap to the current pose. Rendering
does not modify scene transforms or consume random state. Decoration subtrees
cannot contain gameplay entities or colliders.

The updated scene changes the content fingerprint: saves/replays from older scene
content are rejected by the existing content check. ESAV/ERPL formats are unchanged.
`transforms` covers numeric hierarchy/camera references, wraparound, pause/step,
restore/restart, replay and invalid content. `render_targets` checks mirrored and
sheared texture pixels through rotation, zoom and viewport changes, plus partially
visible rotated-sprite culling.

## Texture sampling gallery

Walk right and upward from spawn to the four panels around (-21, -4). Columns
compare **nearest** and **linear** filtering; rows compare **clamp** and **repeat**.
All four use the same tiny cyan/orange image and UV range -0.5..1.5 on each axis.
Use +/- zoom to inspect the difference. The panels draw behind the player.
These are procedural visual fixtures (separate from F7 file reloads); they do not
change gameplay, save content or random state.

Engine callers select `TextureSampling` at upload or with `set_sampling` between
passes. Minification/magnification and U/V wrapping are independent. Defaults stay
nearest/clamp, and image reloads preserve the descriptor. `atlas_uv_inset` provides
texel-center bounds for linear filtering within an atlas cell. Repeat applies to
the full image, and mipmaps are not implemented. See [sampling contracts](docs/FORMATS.md#texture-sampling)
for limits, shared handles and atlas tradeoffs.

`texture_sampling` checks real GPU pixels for filters, wrapping/seams, flips,
atlas edges, reload preservation, invalid changes and gallery resource stability.
CPU cases in `tile_animation` validate descriptors and inset bounds. The scene and
save/replay formats are unchanged in this increment.

## Shader development

Feature Lab loads `shaders/sprite.vert` and `shaders/sprite.frag` from its active
asset root at startup. Edit these files and press **F5** to reload both stages.
Use the source asset directory when editing, since normal builds replace staged
assets with the checked-in copies:

```sh
./build/debug/feature_lab --assets "$PWD/assets" --diagnostics-demo

# Exercise a compile failure without modifying any files.
./build/debug/feature_lab --shader-error-demo
```

**F6** triggers the same failure demo during play. An orange HUD status identifies
the retained program revision, and the console shows stage/compiler/linker details.
F5 recovers after fixing the files. Failed startup loads retain the embedded shader;
failed later loads retain the last successfully installed shader, including custom
changes. Successful reloads increment the revision and show a ready status. Settings
capture these controls; held keys do not recompile every frame.

Compilation is synchronous, on demand, between render passes; it can cause a frame
hitch. Sources are bounded to 64 KiB per stage. Candidates must match the renderer's
active input/uniform/output interface. Optimized-out required resources are rejected.
This validates compatibility, not the visual behavior of custom GLSL. Keep the
premultiplied-alpha handling when editing. See [shader API contracts](docs/FORMATS.md#shader-program-reload).
Optional file watching is described below. Background compilation and a general
material system are not included.

## Texture development

Press **F7** to reload all registered ETEX files from the active asset root. Tiles,
animated sprites and other users of a shared texture keep their existing handles.
The entire set changes together after decoding, atlas validation and GPU upload
succeed. An error retains every previous GPU image and CPU snapshot; the console
explains the failure and the HUD shows the retained revision.

```sh
./build/debug/feature_lab --assets "$PWD/assets"

# Exercise rejection without changing any files.
./build/debug/feature_lab --texture-error-demo
```

**F8** triggers the same rejection during play. Fix the file and press F7 to recover.
F7 can also recover the tile texture in `--missing-texture-demo`. Files must remain
ETEX v1 and fit all registered atlas grids; this command does not reload scene or
animation definitions. Settings capture these controls and held keys do not repeat reloads.

Reload runs synchronously between passes and may hitch. There are at most 64
uploaded texture handles, 16 MiB decoded per image, and 64 MiB of candidate pixel
payload per transaction. Candidates temporarily coexist with the old resources.
See [texture reload contracts](docs/FORMATS.md#texture-reload) for memory and lifetime
details. Background loading remains unimplemented.

## Automatic asset reload

Launch with an explicit editable asset root and **`--watch-assets`**:

```sh
./build/debug/feature_lab --play --watch-assets --assets "$PWD/assets" --no-audio
```

Edits to `shaders/sprite.vert`, `shaders/sprite.frag`, a resident ETEX texture,
`fonts/ui.efnt`, `facility.eani`, the active level's ETMP/scene file, or the four
WAV files (with audio enabled) automatically use
transactional reload paths. Each
affected group waits for 150 ms without another observed change; shader stages reload together,
textures reload as one bank, WAV sounds as another, and font, animation, tilemap
and scene files use independent groups.
Invalid or missing files retain the last working resources and show the existing failure status. Fix the file to retry. An unchanged
failed file is not retried every frame. F5–F8 remain available and take precedence
over a ready automatic reload of their group in the same frame.

Watching is off by default and has no persisted preference. Enabling it performs
one initial debounced refresh to synchronize the loaded resources with disk. It
operates while the title, settings or paused game is visible; minimized rendering
defers checks until the window becomes drawable. Shader/texture/font edits affect
presentation during replay without changing the recorded simulation state. Audio
watching is also available during recording/playback.

The in-house watcher uses nonblocking Linux inotify and checks bounded file metadata
once per second to recover missed notifications, symlink-target edits and replaced
directories. Atomic editor saves, deletion/recreation and notification saturation
are covered by tests. If directory notifications are unavailable, metadata checks
continue and reattachment is retried. Only the two shader files, resident texture
keys, `fonts/ui.efnt` and (outside recording/playback) `facility.eani`,
`facility.etmp`, `annex.etmp`, `facility.scene` and `annex.scene` are watched.
Inactive-level map/scene edits are ignored until that level is loaded. With audio
enabled, `sounds/pickup.wav`, `sounds/music.wav`, `sounds/machine.wav` and
`sounds/door.wav` are watched as one additional group.

Animation reload retains each active clip's name, pause state and normalized tick
progress. All four players swap together. Finished doors remain finished; a timing
edit cannot manufacture a completion event. The five required gameplay clips and
the existing texture key/atlas grid must remain compatible. The HUD and console
show the retained revision after bad edits. Animation timing affects gameplay, so
animation watching is disabled for recording/playback even with `--watch-assets`.
Shader, texture and font watching remain available in those modes.

Saves keep their existing content checks: editing EANI changes animation identity,
so old checkpoints require matching content. A successful new game, level change or
save replacement resets animation reload status to revision 1 for that snapshot.
There is no new hotkey or file format. See [animation reload semantics](docs/FORMATS.md#animation-snapshot-replacement).

Tilemap reload stages tile chunks, solid cells, collision queries and minimap walls
before publication. It preserves entities, pickups, animation, pause and game time;
player interpolation snaps to the current position. Edits that cover the current
player or restart spawn, exceed the collision body limit, or change grid
geometry/texture/atlas are rejected. Cells, palette entries, solidity and layers
may change. This validates placement, not level solvability. Collision contacts are
primed without synthetic alarm entries. Minimap scroll and GPU targets are retained.

Map edits affect gameplay, so both tilemap files are omitted from recording/playback
watching. ETMP content fingerprints change after a successful edit; old saves still
require matching content. New-game/level/save replacement resets the map's reload
status; plain restart uses the current edited map. See [tilemap reload semantics](docs/FORMATS.md#tilemap-snapshot-replacement).

Scene reload replaces the world and invalidates all old entity handles on success.
Player/exit IDs and the set of core IDs must stay fixed; pickups are rebound by ID,
so collected cores stay collected even if records are reordered. Position, pause,
clock, RNG, camera zoom, alarm state and animation/door progress are preserved.
Sprite appearance, decoration hierarchy, static colliders, core/exit positions and
restart spawn may change if the candidate remains compatible with live state.
Player/core nodes must be leaves, and player/core/exit nodes cannot own static colliders.

Reload validates both the new restart spawn and current player against collision,
checks door/win consistency, rebuilds runtime components and collision contacts, and
stages minimap walls and resident texture validation before commit. New texture keys
are rejected; use the already loaded bank. Failed edits preserve handles and all
working content. Minimap scroll and targets survive; the machine sound emitter is
relocated without restarting audio. Edits do not prove route solvability.

Scene watching is disabled during recording/playback. Successful edits update the
scene content fingerprint, so old saves (and the title's original new-game checkpoint)
still require matching authored content. Plain restart uses the current scene and
its edited spawn. Scene status/revision appears in the HUD and session log; game,
level and save replacement reset it. See [scene reload semantics](docs/FORMATS.md#scene-snapshot-replacement).

Audio reload validates all four WAV files before enqueueing one sound-bank swap.
Active voices keep their handles, loop/group/pause state, pan and gain ramps. Their
frame positions map to the same normalized position in the new clips, including
mono/stereo or length changes. Completed/stopped voices are not restarted. The new
bank is used for later play commands; existing music and machine loops update in
place. No gameplay state or replay content identity changes.

Malformed, missing, escaping or oversized files and queue rejection keep the current
bank and working output. The HUD reports the accepted revision as `QUEUED`; it does
not claim the device has played the new samples. Old buffers are reclaimed on the
main thread after worker acknowledgement. Current, retired and staged replacement
samples must fit the output's 64 MiB payload budget. A real device failure retains
the existing silent-fallback behavior. `--no-audio` omits WAV watches; reload does
not reconnect a failed output. No new hotkey or file format is added.

This is a block-boundary sample swap, without a crossfade or waveform matching;
edits can cause an audible discontinuity. Physical listening remains pending.
See [audio replacement contracts](docs/FORMATS.md#audio-bank-replacement).

Loading/compilation still runs synchronously between render passes and can hitch a
frame. Debounce does not make separate filesystem writes an atomic project edit;
use atomic saves for complete files. See [watcher bounds and recovery](docs/FORMATS.md#asset-file-watching)
for limits and metadata-detection caveats. Console `[watch]` messages report mode,
and session logs include automatic reload attempt counts.

## Authored levels

Feature Lab now includes **Facility** and **Annex**, each with its own scene, tilemap,
player spawn, core positions and exit. Collect the cores and reach the open exit,
then press your **Interact** binding (E by default) to enter the other level.
R restarts the current level. New Game returns to the level selected at startup;
Resume retains the current level and progress. A transition resets level progress,
clock, camera interpolation and audio voices, while retaining controls, audio
preferences and the session's random seed.

```sh
./build/debug/feature_lab --level annex --no-audio
./build/debug/feature_lab --level annex --verify
./build/debug/feature_lab --level-tour --no-audio --no-vsync --screenshot tour.ppm
```

`--level-tour` traverses Facility, advances into Annex through the real game loop,
and exits after completing Annex. It fails if a frame limit or early exit prevents
completion. `--scripted --level annex` tests Annex alone. Replay recordings remain
single-level sessions; recording/playback disables level advancement, and a replay's
initial content identity selects its installed level. Save slots can likewise load
either level from gameplay, the title screen or `--load-slot` without a schema change.
Existing Facility content files remain unchanged.

Loading stages the destination and validates its player spawn, resident texture
requirements and minimap data before replacing the live world. Failure retains the
current level and displays **LEVEL LOAD FAILED - SESSION RETAINED**. Release and
press Interact again to retry. Held gameplay keys/buttons are suppressed until
release after a successful transition. These two authored levels share a resident
texture bank; arbitrary new texture sets and background loading are not included.

The player entity's authored world position is the entry spawn. Its parent must
have an identity transform; the player box must fit inside the map and avoid walls
and the closed exit. The Annex moves the alarm station to its own play area.

## Runtime components

`Scene` owns typed component pools alongside its authored nodes. Add your own
C++ types without editing the engine; entity destruction, subtree deletion and
world replacement release their components automatically.

```cpp
struct Health { int points; };
struct Regeneration { int per_tick; };
engine::Scene scene;
const auto actor = scene.create(1, "actor");
scene.add_component<Health>(actor, 80);
scene.add_component<Regeneration>(actor, 1);
scene.each_component<Health, Regeneration>(
    [](engine::Entity, Health& health, const Regeneration& rate) {
        health.points += rate.per_tick;
    });
```

Queries visit the intersection of requested types. Const scenes expose const
payloads. `try_component<T>` returns null for absent components or invalid handles;
`get_component<T>` rejects them. Add/remove and other structural changes must occur
outside iteration; queue entity deletion with `defer_destroy`, then call `flush`.
Use entity handles to reacquire payloads after structural changes.

Pools reserve bounded dense storage when first used (4096 entries per type, at most
64 types per scene). Queries and removal/reinsertion do not allocate engine storage
after setup. Components must have nonthrowing moves and destruction; user constructors
can fail without publishing a partial entry. See [component contracts](docs/FORMATS.md#runtime-component-pools)
for memory, iteration order and lifetime details.

These are runtime components: the existing scene format still stores authored
transforms, sprites and colliders. Custom serialization codecs are not included.
Feature Lab rebuilds `CorePickup` components from core tags and restores collection
state from the existing save format. Both levels exercise pickup, deferred deletion,
restart, save/load and world replacement. F2 shows live entity and pickup counts.

## Keyboard and mouse controls

Open **F1 → Controls**, select an action, then press a supported key or mouse
button, optionally holding Ctrl, Shift and/or Alt first. Keyboard choices include
A–Z, 0–9, common punctuation, Backspace, Insert/Delete, Home/End, F4/F9/F12,
keypad digits, decimal and arithmetic keys, and international character keys.
Ten actions can be rebound: movement in four directions, pause, restart,
interact, mute, music volume and effects volume. Supported buttons are left (LMB),
middle (MMB), right (RMB), and two side buttons (SIDE1/SIDE2, X11 buttons 8/9).
Wheel input remains scrolling. Each action has one configurable input; assigning
a new key, mouse button or chord replaces the previous input. Identical bindings
cannot be shared. Conflicts and unsupported keys leave capture open; Escape cancels it.
Restore Defaults restores A/D/W/S/P/R/E/M/N/B. Hints follow the selected input.
Changes apply immediately and save when settings close or the game exits normally.

Arrow keys, Space, Tab, Enter, Escape, Shift, zoom/page keys and F1/F2/F3/F5/F6/F7/
F8/F10/F11 stay fixed so menus and diagnostics remain accessible. Left-click still
operates menus even when bound to gameplay. Held keys/buttons are suppressed until released after a binding
change, and modal menus capture gameplay. Focus loss cancels capture. Keyboard
bindings use the active XKB layout group at level zero: Shift+1 uses the base
symbol 1 on layouts where that key has 1 at level zero.
Supported punctuation: grave, brackets, backslash, semicolon, apostrophe, comma,
period and slash, plus exclamation, quotation mark, hash, dollar, percent,
ampersand, parentheses, star, colon, comparison signs, question mark, at, caret,
braces, pipe and tilde. Main-row plus/minus/equal/underscore remain zoom controls.
Bindings follow symbols in the active layout rather than physical key positions.

International keys accept Unicode 16 letters, numbers, punctuation and symbols.
Exact legacy X11 character aliases and their Unicode equivalents have one identity:
for example, legacy Greek alpha and U+03B1 cannot become separate bindings.
ASCII letters normalize to lowercase; non-ASCII case is preserved. Common accented
letters have short labels such as **EACUTE**, **AUML** and **CCEDIL**. Other characters
use **U+XXXX** (up to six hex digits), so all labels work in the existing bitmap
font. Ctrl/Shift/Alt chords work with these keys. Settings save as ECFG v7 and load
v1–v6 without changing their existing assignments.

Dead keys, composed text, IME input, combining marks, whitespace, controls and
private/unassigned code points are outside binding support. This does not add
Unicode text rendering. See [character data and regeneration](docs/KEYBOARD_DATA.md).
To test in Feature Lab, bind Interact to an accented or non-Latin key in your
active layout, operate the alarm switch, then close and reopen the game to verify
the binding and label. Try a Ctrl chord and a conflicting assignment too.

Keypad digits use **KP0–KP9**, separate from the number row. Decimal/separator uses
**KPDOT**; arithmetic keys use **KP+**, **KP-**, **KPMUL**, **KP/** and **KPEQ**.
Num Lock on/off selects the same binding. Keypad navigation aliases (such as End
on keypad 1) become keypad digits, without affecting dedicated navigation keys.
Num Lock does not consume capture; keypad Enter remains fixed menu activation.
To test this, bind Interact to KP1, toggle Num Lock, and use keypad 1 at the alarm
switch in either state. Ctrl/Shift/Alt chords work with keypad keys too.

Keyboard group changes cancel pending input and Controls capture, and suppress
held keys, modifiers and mouse buttons until released. New presses use the new
layout; switching back restores the original symbol mapping. Keys with fewer
groups follow their XKB wrap/clamp/redirect rule. Settings keep symbol bindings,
so changing layouts does not rewrite preferences.

To exercise this in Feature Lab with layouts already configured on your desktop:

1. Hold a movement key, switch layouts, then release and press again. Movement
   must stop at the switch and resume according to the new layout's symbol.
2. Open **F1 → Controls**, select an action, and switch layouts. Capture must end
   with **BINDING CANCELED**. Select it again to bind a supported key in that layout.
3. Repeat with a held Ctrl chord or mouse button; neither should remain active.
   Verify arrows, menu keys and keypad input still work, then switch back.

Chord labels use **C+** for Ctrl, **S+** for Shift and **A+** for Alt. Modifier keys
alone keep capture open. For bindings sharing a base key/button, the required
modifier sets must be nested: E, Ctrl+E and Ctrl+Shift+E may coexist, but Ctrl+E and
Shift+E conflict because Ctrl+Shift+E would be ambiguous. The most specific
matching chord wins; extra modifiers still allow a plain binding as a fallback.
The action is selected on the fresh key/button press and stays selected until that
key/button is released. Pressing/releasing modifiers during a hold does not switch
actions. After rebinding, release held modifiers and press them again.

To exercise chords, keep Interact=E and bind Move Left=Ctrl+E and Move Right=
Ctrl+Shift+E. E interacts, Ctrl+E moves left, and Ctrl+Shift+E moves right. Releasing
Ctrl/Shift while holding E keeps the selected movement until E is released. Try
Alt+RMB for another action and restore defaults when finished.

Try binding **Interact** to Home or F12 and **Move Left** to 1, then close settings.
Use the selected interact key at the alarm switch. Hold 1 and Left Arrow together;
movement continues until both are released. Controls and HUD show the selected
names. Reopen Controls and choose Restore Defaults to reset all ten actions.

To exercise mouse input in Feature Lab, bind **Interact** to RMB, close settings,
and right-click at the alarm switch. Bind a movement direction to a mouse button
to test holding it alongside the matching arrow key; releasing either input keeps
movement active until both are released. Reopen Controls to restore defaults.

## Saved data

Settings use ECFG v6. ECFG v1 loads defaults, v2 preserves letters, v3 preserves
letter/mouse bindings, v4 preserves extended keys, and v5 preserves chords.
v1–v4 load with no required modifiers. All older versions upgrade on the next
preference save. Saves and replays are unchanged.

Settings: `$XDG_CONFIG_HOME/feature_lab/settings.ecfg`, falling back to
`$HOME/.config/feature_lab/settings.ecfg`. Slots: `$XDG_STATE_HOME/feature_lab/slot-N.esav`,
falling back to `$HOME/.local/state/feature_lab/slot-N.esav`. Relative XDG values are
ignored. Reads do not create directories; writes create the required directories.
Use `--user-data DIR` to isolate both under `DIR/config` and `DIR/state`.
Shipped assets are never used as user-save storage.

```sh
# Save a scripted checkpoint into an isolated directory on exit.
./build/debug/feature_lab --user-data build/my-saves --scripted --frames 500 --save-slot 1

# Relaunch at that checkpoint with normal player control.
./build/debug/feature_lab --user-data build/my-saves --load-slot 1
```

Slots are 1..3. `--load-slot` restores before rendering and disables scripted driving;
its failure exits with diagnostics. `--save-slot` writes on normal exit. `--no-vsync`
is a session override; it does not overwrite saved VSync unless changed in the menu.
Bad settings use defaults and remain untouched unless you change preferences.
Save compatibility requires the same authored scene, map, and animation data.
Current checkpoints use ESAV v3 with alarm and random state. ESAV v2 migrates with
the default seed; ESAV v1 remains unsupported. Checksums detect accidental corruption.

## Replay files and random state

Record a complete example run, verify it without a display or audio device, then
watch the recorded inputs:

```sh
./build/debug/feature_lab --scripted --seed 42 --frames 1600 --no-audio \
  --user-data build/replay-user --record-replay build/example.erpl
./build/debug/feature_lab --verify-replay build/example.erpl
./build/debug/feature_lab --replay build/example.erpl --no-audio \
  --user-data build/replay-user
```

Omit `--scripted` and `--frames` to record human play; Escape ends and saves the
recording. `--load-slot` can supply its starting checkpoint. `--seed` accepts an
unsigned 64-bit decimal integer (default 1) and cannot accompany a loaded save or
replay. Machines change indicator brightness once per simulated second using the
saved sequence. Pause freezes it, single-step advances it, and restart resets it.

Record/playback starts directly in gameplay. F1 and in-session slot operations are
unavailable in these modes. Playback ignores live gameplay keys; Escape exits and
F11 still toggles fullscreen. Each file command advances one rendered frame during
playback; VSync controls viewing speed. Playback exits at EOF, or earlier with
`--frames`/Escape. The verifier checks every command without rendering. A mismatch
reports the zero-based command index and exits unsuccessfully.

Files contain an initial checkpoint, tick-indexed logical inputs and per-step state
hashes. They are bounded to 36,000 commands (ten minutes at 60 fixed steps/second,
including paused steps) and 1 MiB. Reaching the limit ends the session and saves it.
Writes are atomic on clean exit; a crash loses the current in-memory recording.
The destination parent must exist. Matching content and simulation rules are
required; determinism across arbitrary architectures/compiler floating-point modes
is not promised. Audio, rendering and menu actions are outside replay state.
See [ERPL contracts](docs/FORMATS.md#replay-files-erpl-v1).

## Stress arena and benchmark reports

```sh
./build/release/feature_lab --stress --width 1920 --height 1080 \
  --frames 600 --warmup 60 --cycles 3 --report build/stress.json
./build/release/stress_bench --frames 600 --warmup 60 --cycles 3 \
  --report build/stress-cpu.json
```

The arena independently scales sprites, tiles, scene nodes, collision density and
offline mixer voices. It repeats complete load/measure/unload cycles and checks
tracked resources for growth. Reports include warmup-excluded median/p95/p99 CPU
and GPU timings, hardware/build metadata, workload counts, checksum and memory
observations. `--stress --help` lists options; Escape aborts. This mode does not
modify saves or settings. See [workloads, report fields and measurement limits](benchmarks/README.md) and the
[controlled 1080p validation results](benchmarks/2026-10-16-validation.md).
[GPU timestamp diagnostics](benchmarks/2026-10-17-gpu-timing.md) retain raw evidence
for rejected timing pairs without waiting for the GPU.

Recorded gameplay uses the normal minimap/world/HUD rendering path:

```sh
./build/release/feature_lab --replay benchmarks/replays/facility.erpl \
  --profile-report build/facility-profile.json --profile-warmup 30
```

Profile mode verifies every replay state, uses default configuration, disables audio
and deliberate pacing, and reports the actual managed viewport. See the
[workloads, timing boundaries and report contract](benchmarks/REPLAY_PROFILING.md).

Reload and transition profiling uses a separate scripted workload:

```sh
./build/release/feature_lab --spike-report build/spikes.json
```

It exercises successful/rejected shader and texture reloads, font reload, and
Facility → Annex → Facility, retaining per-operation timings and nearby frames.
Each operation also reports covered CPU/GPU staging-payload peaks and verifies
that staging ownership is released on return. These counters exclude allocator
overhead, nested string storage and driver memory. Loading, parsing and selected
construction buffers are included, with malformed-transition cleanup checks.
Lazy scene reservations reduce the largest covered transition peak by 10.2%;
see [the measurements and rollback evidence](benchmarks/2026-10-20-lazy-scene-results.md).
Per-level collision counters also report reserved capacity and peak demand from
successful updates; see [the utilization results](benchmarks/2026-10-20-collision-usage-results.md).
Explicit per-world budgets now reduce Feature Lab's collision reservations by
95.5% and its largest covered transition peak by 79.2% relative to that baseline;
see [budget validation and measurements](benchmarks/2026-10-20-collision-budgets-results.md).
Grid-membership budgets further reduce collision reservations to 32,256 bytes and
the largest covered transition peak to 1,383,742 bytes;
see [grid budget evidence](benchmarks/2026-10-21-collision-grid-budgets-results.md).
Scene/core and component counters now report reserved capacity and mutation peaks;
see [scene utilization measurements](benchmarks/2026-10-21-scene-usage-results.md).
Per-scene budgets now reduce Feature Lab scene reservations to 81,920 bytes and
the largest covered transition peak to 154,942 bytes, with overflow rollback;
see [scene budget measurements](benchmarks/2026-10-21-scene-budgets-results.md).
Expected failures must preserve state/resources; unexpected outcomes save an
incomplete report and return nonzero. See [the workload and timing contract](benchmarks/SPIKE_PROFILING.md).

Dedicated development executables now measure C++ allocation calls and requested
bytes in the actual gameplay renderer and audio worker. Normal executables remain
uninstrumented. See [scope and reproduction instructions](benchmarks/ALLOCATION_PROFILING.md)
and [the three-repeat results](benchmarks/2026-10-24-allocation-results.md).

HUD/status formatting now reuses caller-owned storage. The two verified replay
workloads record zero C++ allocations in world/HUD submission with unchanged
rendered output; restart allocations remain. See [the storage contract](benchmarks/HUD_TEXT.md)
and [before/after measurements](benchmarks/2026-10-24-hud-text-results.md).

The [UI allocation probe](benchmarks/UI_ALLOCATIONS.md) extends coverage to open
settings, diagnostics and enabled-audio HUD labels, including remapped controls.
[Recorded results](benchmarks/2026-10-25-ui-allocation-results.md) separate steady
redraws from menu/rebinding transitions.

Diagnostics shares the reusable HUD buffer. Increment 60 preserved decimal labels
and rendered output; increment 73 adds the memory panel. The [diagnostics storage contract](benchmarks/DIAGNOSTIC_TEXT.md)
and [measurements](benchmarks/2026-10-26-diagnostic-text-results.md) cover changing
GPU labels and combined settings/controls overlays as well as the original UI states.

For stress-frame outliers, `--stress --frame-samples` retains paired CPU/GPU
intervals with original frame tags. See [the opt-in contract](benchmarks/STRESS_FRAME_SAMPLES.md)
and [the recorded investigation](benchmarks/2026-10-27-frame-samples-results.md). Stress mode also
accepts `--presentation immediate|vsync`; [request semantics and comparison](benchmarks/STRESS_PRESENTATION.md)
keep requested pacing separate from physical synchronization.

Stress diagnostics also support `--frame-samples --submission-samples` to split
CPU preparation/bookkeeping from GL pass setup and batch upload/draw. See the
[measurement contract](benchmarks/STRESS_SUBMISSION.md) and
[retained results](benchmarks/2026-10-27-submission-results.md). Add
`--preparation-samples` for coarse phase wall/thread-CPU measurements; see the
[clock and phase contract](benchmarks/STRESS_PREPARATION.md) and
[profile findings](benchmarks/2026-10-27-preparation-results.md). Add `--context-samples`
to correlate calling-thread switch counts with those phases; see the
[counter scope](benchmarks/STRESS_CONTEXT_SWITCHES.md) and
[retained comparison](benchmarks/2026-10-28-context-results.md).

## Window lifecycle

Fullscreen and minimize are asynchronous window-manager requests. The engine exposes
observed fullscreen/focus state after polling. Feature Lab suspends simulation,
replay consumption, rendering and asset watching while its window is unmapped,
and resumes without catching up hidden time. Focus loss cancels held input but
does not itself pause gameplay. Already-playing audio is not suspended by unmapping.

```sh
DISPLAY=:0 ./build/debug/platform_probe --exercise
```

This development probe opens its own window to exercise fullscreen, focus,
minimize/restore and drawable dimensions. Exit 2 means a desktop request was
unconfirmed; see the [platform validation matrix](docs/PLATFORM_VALIDATION.md).
The current desktop confirms fullscreen and client unmap/restore but does not
confirm the WM minimize request. Physical monitor scaling remains unverified.

## Tests

Five CPU `mixer_reload` cases verify exact output samples, normalized frame progress,
mono/stereo changes, paused controls/ramps, completion and FIFO ordering. Five
`audio_reload` cases exercise real ALSA null output with a held worker, queued
replacement lifetime, queue pressure, bounds, file failures/repair and shutdown.
Two `audio_watch` cases cover bank debounce, atomic failure/repair, stable voices,
manual priority, disabled watches and unchanged recorded gameplay bytes.

Seven CPU `scene_reload` cases cover persistent-ID rebinding, stale handles,
movement/query collision, spawn and live-state rejection, staged failure, save
compatibility, active iteration, alarm contacts, door completion, both levels and
file repair/locking. Three `scene_watch` GPU cases verify sprite/minimap pixels,
texture preflight, failure retention, debounce, level isolation and stable resources.

Seven CPU `tilemap_reload` cases cover collision insertion/removal, player/spawn
rejection, capacity/geometry limits, dependent staging failure, entity/progress
preservation, contact priming, saves, both levels and file repair. Three
`tilemap_watch` GPU cases check tile/minimap pixel agreement, retained failures,
repeated resource stability, active-level routing and fixed-content omission.

Seven CPU `animation_reload` cases cover normalized tick progress, pause/completion,
maximum durations, required clips/atlas constraints, atomic publication, save
compatibility, file failure/repair and old-snapshot release. Two `animation_watch`
GPU cases cover debounce, changed sprite pixels, retained failures, independent
watch groups, fixed-content omission and repeated resource stability.

The CPU `audio_recovery` group checks short writes, wait errors, underruns, bounded
suspend/resume retries, failed recovery and exact pending samples. `audio_transport`
also exercises a real ALSA write failure through `file:/dev/full` and verifies
Feature Lab disables audio while retaining failure diagnostics.

The `platform_lifecycle` group covers fullscreen observation/requests, focus modes,
input cancellation, repeated unmap/restore, six resize/UI/pointer sizes, and close
protocol validation. See [platform evidence and manual checks](docs/PLATFORM_VALIDATION.md).

Stress tests cover deterministic workloads, all capacity limits, dense collision
results, allocation-free steady CPU updates, repeated CPU/GPU teardown, batching,
culling, texture-byte accounting and JSON report validation.


The eight-case `replay_files` group checks seeded reference sequences, save migration,
full-game record/decode/playback, paused starts, short taps, stepping, restart,
content/state divergence, malformed files, recording limits and atomic file storage.

Seven level cases complete both routes, check reset and held-input behavior, reject
missing/corrupt/invalid-spawn content and failed resource preparation, reject
replacement during scene iteration, resolve cross-level saves and replay Annex
commands from a Facility startup. A native `level_tour` CTest completes both levels
through the application loop. GPU tests repeat eight replacements, verify minimap
wall counts and stable texture/target counts, and reject absent/incompatible
resident texture requirements.

Ten `components` cases cover checked access, compact removal, resource ownership,
const and intersecting queries, alignment, nested iteration, constructor failures,
deferred subtree deletion, scene moves/replacement, capacity limits and a 2500-step
reference-model comparison. The stress suite checks zero C++ allocations during
120 rounds of typed queries and component removal/reinsertion, then full teardown.
The level suite also checks live pickup components through saves, restart and both
levels; the native two-level tour verifies that all pickup components are removed.

The six-case scene-flow suite covers new/continue/resume, empty/corrupt/invalid
checkpoints, failed replacement during scene iteration, input capture, menu return,
and layout bounds. GPU checks render the title at four viewport sizes while
repeated transitions keep resource counts stable. A null-device audio case checks
initial pause and resume without duplicate voices.

The sixteen-case bindings suite checks keyboard/mouse conflicts, named symbols,
reserved-key rejection, v1–v6 migration and v7 round trips, malformed bindings,
ordered raw press consumption, capture/cancel/defaults, held-button modal
suppression and layout bounds. Native tests exercise
all 27 new keyboard symbols and five mouse buttons, unshifted capture, aliases,
duplicate events, fast clicks, held remapping, focus recovery, ignored wheel/
unsupported buttons and mapping notifications. GPU tests draw both settings pages at small and large viewports.

The fourteen-case native chord/keypad/group suite checks all modifier masks, press-time selection,
modifier release order, repeat suppression, left/right Ctrl/Shift, mouse chords,
fixed aliases, focus/mapping cancellation, held rebinds and atomic capture. CPU
cases additionally check nested conflict rules and settings capture/defaults.
Keypad cases cover all 16 canonical symbols plus 11 navigation aliases, lock-state
event masks, main-row separation, chords, Num Lock capture and fixed keypad Enter.
Locale separator normalization is covered in CPU tests.

Keyboard layout tests include four in-memory XKB map cases for distinct groups,
level-zero lookup, wrap/clamp/redirect, single-group keys and unsupported symbols.
Native group cases inject per-event group state into owned windows and verify
held-input cancellation, release/repress, focus resynchronization, fixed/keypad
keys and Feature Lab Controls capture. They do not change the desktop keymap;
physical layout-switch shortcuts and compositor behavior still need manual checks.

Five international binding cases check alias identity across scripts, character
categories, labels, punctuation, conflicts, canonical records and v7 migration.
Three native cases use a test-only client-side XKB map fixture and real queued
window events to exercise accented/Greek/CJK capture, chords, group transitions,
Controls labels, settings round trips and held-input suppression. The fixture
never changes the server keymap. GPU tests render long international chord labels
at 320×240, 640×480 and 1280×720.

The eight-case texture reload suite checks shared-handle pixel updates, dimensions,
alpha/filter/wrap behavior, atomic invalid-batch rejection, active-pass rejection,
full handle capacity, repeated GPU-object retirement, staging bounds, coordinated
CPU/GPU publication, retained readers, fallback recovery, shared atlas compatibility,
and modal/press-edge controls. Native event tests cover F7/F8.

The GPU timing suites include eleven deterministic CPU cases for delayed availability,
full-pool skipping, FIFO reuse, tag/scope validation, unsupported counters, timestamp
reversal, bounded failure evidence, elapsed results and rolling history; six OpenGL
cases cover real result collection, pixel stability, saturation/recovery, shader
reload/resize, external query conflicts, interrupted-scope cleanup and overlay layouts.
The eight-case GPU shader suite verifies shipped/fallback parity, changed pixels,
compile/link/interface failures, source bounds, active-pass rejection, sampler and
premultiplied uniform relocation, repeated program retirement, real GL-error detection,
file recovery and modal input capture. Native event tests cover F5/F6.

The nine-case `asset_watch` suite covers bounded registration, descriptor cleanup,
debounce, atomic saves, deletion/recreation, directory/root replacement, symlink
metadata recovery, event saturation and missing-directory reattachment. Two native
`watched_reload` cases verify shader/texture pixels, invalid-edit retention, recovery,
resource counts and manual-command priority through the same controller used by
Feature Lab. Four `font_reload` GPU cases cover atomic metrics/pixel replacement,
invalid/missing file retention, recovery from missing directories, path containment,
resource stability and independent notification groups. These filesystem tests use
private temporary assets and simulated time.

The diagnostics suite tests rolling timing statistics, overflow/invalid samples,
input edges, catch-up single-step behavior, modal capture, save/restore and complete
stepped replay equivalence. GPU checks exercise debug outline pixels, clipping,
invalid geometry, responsive diagnostics panels and resource stability; native
checks cover F2/F3/F10. An ALSA null-device case verifies stepped cues stay paused.
The in-house CPU runner covers camera/transform math, input edges, time accounting,
collision, audio mixing, and gameplay. The content suite additionally covers bounded
texture decoding, invalid scene records, entity lifetime/compaction, hierarchy
cycles, atomic file replacement, cache snapshots, and deterministic serialization.
The tile/animation suite checks bounded binary formats, visible cells against a
brute-force reference, chunk culling, collision metadata, animation boundaries,
pause/restart, large time advances, and one-shot completion. CTest also runs a deterministic game
route, missing-display handling, native key/focus/close events sent only to a hidden
test window, ALSA null-device queue/shutdown checks, and real GPU
pixel tests for color/blending, atlas sampling, order, culling, batching, and resize.
The sound suite checks PCM16 decoding/resampling, malformed chunks, stereo mixing,
loop/pause/stop, gain ramps, voice limits, and FIFO command behavior. The audio
transport suite exercises owned sample lifetimes and the game soundtrack on ALSA's
null device, including restart, settings changes, redundant-command suppression,
and failure fallback.
The render-target suite checks offscreen pixels/orientation, transparent multi-pass
composition, nested scissor state, failed/successful resizing, memory/handle limits,
and Feature Lab minimap projection and clipped scrolling at different resolutions.
The collision suite checks box/circle contacts and tangency, moving grid candidates
against brute force, duplicate removal, filtering, trigger history, query masks,
segment hit order/normals, capacity errors, switch occlusion, and checkpoint
continuation during an active alarm contact. GPU tests inspect the alarm colors.
The persistence suite checks exact checkpoint/config round trips, continuation
against uninterrupted gameplay, stale handles, semantic validation, bad versions,
checksums, bounds, missing/corrupt slots, XDG paths, and interrupted atomic writes.
Child processes exit abruptly before/after rename to verify that the committed file
remains complete; this does not simulate filesystem or power failure.
The UI suite covers transactional layouts, focus order, clipped/disabled hit targets,
press/release capture, canceled clicks, slider dragging, wheel and keyboard control,
modal pause/input isolation, and responsive settings actions. Native tests cover
pointer events and UI keys; GPU checks cover focus/value pixels and panel resizing.

Graphics tests report **skipped** if `DISPLAY` is absent. A display that cannot
provide the required context is a failure. The null audio test is not evidence of
audible playback. CTest timeouts apply per executable; individual CPU cases are
named and failures include source locations.

Format source with the checked-in `.clang-format` settings. Both supported compilers
build project code with strict warnings and warnings treated as errors.

## Dependencies and delivery

All engine/game/test implementations are original project code. Direct system
library dependencies are Xlib, GLX/OpenGL, ALSA, and the C++/C runtime. Xlib and GPU
drivers bring their own platform dependencies; no SDL/GLFW/GLAD/GLM, game framework,
asset decoder, or external test library is used.

Feature Lab loads `facility.scene`, `facility.etmp`, `facility.eani`, and ETEX textures from staged `assets/` beside
its executable, or `../share/feature_lab` in an installed package. Use `--assets DIR`
to select another root. It runs independently of the current working directory.
The in-house C++ generator produces the scene, map, textures and sounds. Shader
sources live in `assets/shaders/`, with embedded startup fallbacks; the diagnostic
font is loaded from `fonts/ui.efnt` with an embedded fallback; `sounds/*.wav` are loaded from disk. See [format/API contracts](docs/FORMATS.md).

For a visible missing-texture/fallback scenario:

```sh
./build/debug/feature_lab --missing-texture-demo --no-audio
```

Invalid startup scenes, maps, and animations are fatal with source diagnostics; missing or
broken textures produce an error and a magenta fallback. F7 provides coordinated
CPU/GPU texture reload. `--watch-assets` automates shader/texture/font/animation/tilemap/scene/audio reload; invalid
animation/map/scene/audio edits retain working content.

The install target includes both executables, assets and documentation:

```sh
cmake --install build/release --prefix "$PWD/build/package"
./build/package/bin/feature_lab
```

Build an archive that can be moved to another directory on a compatible Linux host:

```sh
cmake --preset release
cmake --build --preset release --target release_package -j2
ctest --preset release -L packaging --output-on-failure
```

The `.tar.gz` and checksum sidecar appear in `build/release/dist/`. Packaging checks
an extracted, relocated copy before publishing the archive. Release CTest also
runs a packaged OpenGL gameplay/replay/stress smoke test when a display is present.
System libraries and drivers remain host-provided; older distributions are not
automatically compatible. See [package contents, tests and compatibility](docs/PACKAGING.md).

## Offline asset validation

Check both Feature Lab levels and their assets without opening a display or audio
device:

```sh
cmake --build --preset debug --target validate_assets -j2
./build/debug/validate_assets --assets assets
./build/debug/validate_assets --assets /path/to/work --type scene --file draft.scene --scene-budget 256
```

Use `--type` and `--file` to check one scene, tilemap, animation, texture, font or
sound file without a complete game bundle. Single-file mode checks its format and
optional scene budget; bundle mode also checks references and gameplay semantics.
The release archive includes `bin/validate_assets`, which discovers its packaged
assets automatically. Missing/corrupt assets, scene-budget overflow, gameplay
construction errors and incompatible texture references return nonzero. Shader
source checks do not compile GLSL. Add `--json` for a versioned report on stdout
with mode, budgets, check results and failure diagnostics. See
[coverage, exit codes and report schema](docs/ASSET_VALIDATION.md).

## Repository workflow

Develop new features on a dedicated branch. Run the relevant tests, commit the
completed feature, and merge it into `main` after validation. Push both the feature
branch and updated `main` to the GitHub remote.


### Bitmap-font assets

Feature Lab loads `fonts/ui.efnt` for its HUD and menus. This in-house format packs
an RGBA atlas, glyph rectangles, advances, offsets, line spacing and a fallback
glyph. At startup, missing or damaged font files log a diagnostic and keep readable
embedded artwork. Invalid later edits preserve the current font. Text now submits one quad per glyph instead of one quad per lit pixel.
The supplied font supports uppercase A-Z, digits, space and `%+-/:.?`; unsupported
bytes display `?`. Unicode shaping is outside v1.

```sh
./build/release/feature_lab --play --font-demo --no-audio
```

The gallery demonstrates proportional widths, measured width, multiline layout,
fallback characters, color/alpha and clipping. `make_assets` regenerates the font
alongside the other sample content. Fonts load at startup; `--watch-assets` also
reloads `fonts/ui.efnt` after edits. Combine `--font-demo --watch-assets --assets
"$PWD/assets"` to see changes in the gallery. Missing or corrupt edits retain the
current atlas and metrics; a subsequent valid edit recovers automatically. The
startup embedded font stays active until the first valid file can be loaded.

The HUD shows font revision and whether the retained font is embedded. Console
`[font applied]`/`[font retained]` messages include diagnostics; session summaries
report `watched_fonts`, revision, last outcome, embedded state and atlas bytes.
Watching remains opt-in, and there is no new keyboard shortcut. See
[the EFNT and text API contract](docs/FORMATS.md#bitmap-fonts-efnt-v1).
