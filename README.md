# Linux 2D Engine / Feature Lab

A C++20/OpenGL 4.6 engine built in-house for Linux. It includes native windowing,
batched textured quads, input, simulation, collision, audio, and a playable example.
The fourteenth increment adds a title screen, New Game, Continue from a save slot,
and Resume Session, with validated world replacement and retained state on failure.
Feature Lab includes shader reload, paused stepping, collisions, settings, and saves.

This is an early engine foundation. The full plan is in [ENGINE_PLAN.md](ENGINE_PLAN.md);
[implementation status and feature coverage](docs/STATUS.md) distinguish working
features from planned work.

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

# Check real audio-device initialization with silence, not an audible test.
./build/debug/audio_probe
```

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
render-target counts/bytes, collision bodies/contacts and broad/narrow-phase counts.
Frame statistics describe completed frames, so they appear one frame later. CPU
samples include diagnostic drawing overhead. Wall intervals include pacing; scripted
mode does not count ignored wall-clock time as dropped simulation time.

The GPU rows show mean/p95 over the last 240 completed measurements, sample age in
rendered frames, pending slots, skipped measurements, and invalid timestamps. Timing
covers the minimap and window passes, including UI/debug drawing, between two GPU
timestamps. It excludes simulation, shader compilation, screenshot readback, and
presentation. It can include GPU scheduling delays and gaps while the CPU submits
commands, so it is a render interval rather than a pure GPU utilization measure.
CPU/GPU values come from separate histories and need not describe the same frame.

Eight slots (16 query objects) are reused only after their results are available.
A full pool skips measurement instead of waiting or overwriting pending results.
No completed samples shows `GPU WAITING`; drivers without 64-bit timestamps show
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
There is no file watcher, background compilation, or general material system.

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
ETEX v1 and fit all registered atlas grids; scene and animation definitions are not
reloaded. Settings capture these controls and held keys do not repeat reloads.

Reload runs synchronously between passes and may hitch. There are at most 64
uploaded texture handles, 16 MiB decoded per image, and 64 MiB of candidate pixel
payload per transaction. Candidates temporarily coexist with the old resources.
See [texture reload contracts](docs/FORMATS.md#texture-reload) for memory and lifetime
details. File watching and background loading remain unimplemented.

## Keyboard controls

Open **F1 → Controls**, select an action, then press a letter **A–Z**. Ten actions
can be rebound: movement in four directions, pause, restart, interact, mute, music
volume and effects volume. A letter can belong to only one action. Conflicts and
non-letter keys leave the current binding unchanged; choose another letter or
press Escape to cancel. Restore Defaults restores the original letter bindings.
Changes apply immediately; on-screen hints follow the selected keys. Preferences
save when settings close or the game exits normally.

Arrow keys, Space, Tab, Enter, Escape, Shift, zoom/page keys and function keys stay
fixed so menus remain accessible. Held keys are suppressed until released after a
binding change. Focus loss cancels an active capture. These are unshifted letter
symbols in X11 keyboard group zero, not physical scan codes or modifier chords.
Mouse actions and arbitrary-key rebinding remain future work.

## Saved data

Settings use ECFG v2; ECFG v1 files load with default letter bindings and are
upgraded on the next preference save.

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
Save compatibility requires the same authored scene, map, and animation data;
Current checkpoints use ESAV v2 to include alarm state. ESAV v1 checkpoints are
rejected without changing the current game; version migration is not implemented. Checksums detect accidental corruption.

## Tests

The six-case scene-flow suite covers new/continue/resume, empty/corrupt/invalid
checkpoints, failed replacement during scene iteration, input capture, menu return,
and layout bounds. GPU checks render the title at four viewport sizes while
repeated transitions keep resource counts stable. A null-device audio case checks
initial pause and resume without duplicate voices.

The six-case bindings suite checks transactional conflicts, raw press consumption,
v1 migration/v2 round trips, malformed bindings, capture/cancel/defaults and menu
layout bounds. Native tests exercise rebound aliases, held-key suppression,
invalid-table retention and mapping notifications; GPU tests draw both settings
pages at small and large viewport sizes.

The eight-case texture reload suite checks shared-handle pixel updates, dimensions,
alpha/filter/wrap behavior, atomic invalid-batch rejection, active-pass rejection,
full handle capacity, repeated GPU-object retirement, staging bounds, coordinated
CPU/GPU publication, retained readers, fallback recovery, shared atlas compatibility,
and modal/press-edge controls. Native event tests cover F7/F8.

The GPU timing suites include seven deterministic CPU cases for delayed availability,
full-pool skipping, FIFO reuse, tag/scope validation, unsupported counters, timestamp
reversal and rolling history; four OpenGL cases cover real result collection, pixel
stability, saturation/recovery, shader reload/resize, cleanup and overlay layouts.
The eight-case GPU shader suite verifies shipped/fallback parity, changed pixels,
compile/link/interface failures, source bounds, active-pass rejection, sampler and
premultiplied uniform relocation, repeated program retirement, real GL-error detection,
file recovery and modal input capture. Native event tests cover F5/F6.
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
font remains embedded; `sounds/*.wav` are loaded from disk. See [format/API contracts](docs/FORMATS.md).

For a visible missing-texture/fallback scenario:

```sh
./build/debug/feature_lab --missing-texture-demo --no-audio
```

Invalid scenes, maps, and animations are fatal with source diagnostics; missing or
broken textures produce an error and a magenta fallback. F7 provides coordinated
CPU/GPU texture reload. Interactive scene reload and a file watcher are not implemented.

The install target includes the scene, map, animations, textures, and WAV sounds:


```sh
cmake --install build/release --prefix "$PWD/build/package"
./build/package/bin/feature_lab
```

This uses the host system libraries and driver. It is not a portable Linux package
or a promise of compatibility with distributions older than the build host.

## Repository workflow

Develop new features on a dedicated branch. Run the relevant tests, commit the
completed feature, and merge it into `main` after validation. Push both the feature
branch and updated `main` to the GitHub remote.
