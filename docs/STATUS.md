# Implementation status

The twelfth increment adds transactional texture reload with shared-handle
preservation, coordinated CPU/GPU publication, and failure recovery in Feature Lab. It is **not completion of all six design milestones**. The target requirements remain in [ENGINE_PLAN.md](../ENGINE_PLAN.md).

## Delivered

- C++20 targets for the core, native platform/rendering/audio library, example game,
  CPU/GPU/audio tests, and a silent audio-device probe. No dependency downloads.
- Xlib window/events, GLX OpenGL 4.6 Core creation and validation, startup hardware
  diagnostics, resize, focus-loss handling, close, fullscreen request, and VSync request.
- In-house OpenGL function loading and shader compilation. RAII teardown releases
  graphics objects before their context and cleans up partially initialized objects.
- A preallocated ordered quad batch, generated sRGB atlas, rotation/scaling,
  transparency, camera culling, orthographic mapping, bitmap glyph drawing, draw
  counters, and framebuffer capture. Adjacent quads use one atlas and preserve order.
- Queued input edges, alias handling for WASD/arrows, fixed 60 Hz simulation,
  interpolation, bounded catch-up, and pause/restart behavior.
- Axis-separated swept player boxes against static boxes, with wall sliding.
- A bounded 16-voice mono/stereo PCM mixer and 64-slot SPSC command ring
  (63 usable slots). ALSA outputs 48 kHz float stereo on a separate thread.
- Feature Lab: collect three cores and reach an exit, plus a CPU-only verification
  route and scripted rendering mode. Two copies of the simulation receive identical
  tick inputs; the test checks matching state, wall penetration, and completion.

## Added in the second increment

- In-house ETEX reader/writer with bounded dimensions/payload, exact-length and
  version checks; asset-root resolution independent of the working directory.
- Immutable texture-cache snapshots, failed-reload preservation, cache collection,
  visible fallback images, and explicit GPU upload/release with stale-handle checks.
- Dense scene nodes with generation/world-checked entity handles, persistent IDs,
  optional sprite/collider records, parent transforms, cycle/depth validation,
  subtree deletion, and deferred destruction during iteration.
- Versioned scene parser/serializer, deterministic float round trips, staged scene
  replacement, and atomic disk writes. This is not yet a gameplay save-slot system.
- That version of Feature Lab loaded 252 entities and two shared textures from disk,
  drew in stable layer/ID order, destroyed collected entities, and recreated the
  scene on restart. Verification compares an original scene with its serialized/reloaded copy.
- Nine new content test cases, in addition to the original twelve CPU cases, and
  GPU tests for uploaded pixels, UV flips, release/reuse, and resource counts.

See [format and API contracts](FORMATS.md). Normal builds stage checked-in assets;
the in-house `make_assets` tool regenerates the sample only when explicitly run.

## Feature coverage manifest

“Partial” means only the listed subset is implemented. Tests do not satisfy an
entire plan feature ID when required subfeatures are still absent.

| ID | Status | Current evidence | Still required |
| --- | --- | --- | --- |
| F01 | Partial | `graphics`, `platform_events`, `platform_no_display`, visible startup/resize | Manual minimize/fullscreen/scaling matrix and visible-window focus checks |
| F02 | Partial | CPU edge/focus/capture tests; native key-repeat/alias/pointer/wheel/focus tests; modal gameplay routing | Rebinding, configurable mouse actions, keymap-change support |
| F03 | Partial | Camera round trips, transform hierarchy tests, clamped follow/zoom, GPU world/HUD projection checks | Parented rotating/scaled example decorations and broader manual camera validation |
| F04 | Partial | GPU pixel/order/color/atlas/culling/batch rollover tests, animated atlas sprites; shared-handle transactional texture reload tests | Richer sampling controls; live reload, assets, UV flips and scene layers now implemented |
| F05 | Implemented (v1) | ETMP round trips, layer/atlas validation, chunk traversal vs brute-force reference, solid-cell union and swept query tests; large scrolling map | Optional editing/streaming tools |
| F06 | Implemented (v1) | EANI reader/writer; exact frame timing, loops, pause/restart, large advances; animated player/machine/door with exactly-once completion | Optional frame callbacks and richer animation types |
| F07 | Partial | Handle/compaction/hierarchy tests, scene round trips, deferred core deletion and restart | Extensible component pools, multi-scene transition workflow |
| F08 | Partial | ETEX/ETMP/EANI/WAV readers, cache snapshots/reload tests, path validation, fallback demo, GPU lifetime tests, bounded shader files and transactional shader/texture reload | Further asset types and file watching |
| F09 | Implemented (v1) | Swept player boxes; box/circle contacts; moving-body grid vs brute force; reciprocal filters; trigger history; overlap/segment queries; alarm/switch and restore tests | Rigid-body dynamics, rotating shapes and moving-body CCD are outside initial scope |
| F10 | Partial | PCM16 WAV assets/resampling, loops, voice controls, gain ramps, master/music/effects controls, stereo emitter, null-device soundtrack/restart/failure tests | Physical disconnect/recovery testing and audible-output checks |
| F11 | Partial | Bitmap text, clipped status panel, buttons/checkboxes/sliders, keyboard/pointer navigation, bounded layout, modal settings; `ui` and GPU pixel tests | Dedicated title screen, broader text fixtures and manual navigation checks |
| F12 | Implemented (v1) | Live minimap, clipped scrolling panel; seven GPU cases for pixels, alpha composition, clipping, resizing, resource limits and example integration | Optional multisampling, HDR and post-processing |
| F13 | Partial | Clock conservation/catch-up tests, F10 paused stepping, full stepped replay/state equivalence, modal capture and checkpoint tests | Replay-file format and random-state serialization |
| F14 | Implemented (v1) | Versioned scene/checkpoint/config formats; three-slot UI; CRC, bounds and semantic checks; unchanged live game on failed load; interrupted-process write tests | Optional version migration and power-loss/filesystem fault testing |
| F15 | Partial | Console errors, GL callback, F2 rolling timing/counter overlay, F3 collision/camera outlines, target accounting, CPU percentiles, shader reload/failure recovery, GPU pixel/interface/lifetime tests, bounded asynchronous GPU timing with sample age/skips | Broader manual fault scenarios and profiling coverage |
| F16 | Partial | Audio/GPU lifetime tests; install includes scene, map, animations, textures and WAV sounds | Aggregate resource accounting, stress arena, portable packaging, broader hardware checks |

## Initial validation environment

- Omarchy 4.0.4, Linux x86-64; current desktop session is Wayland, with this engine
  running through XWayland (`DISPLAY=:0`). Native Wayland support is not claimed.
- Intel Core i5-4300U; Intel HD Graphics 4400.
- OpenGL 4.6 Core / GLSL 4.60, Mesa 26.2.2-arch1.1.
- GCC 16.2.1 and Clang 22.1.8.
- Xlib 1.8.13, ALSA 1.2.16.1. GLX/OpenGL use the installed graphics driver stack.
- CMake 4.4.4 and Ninja 1.13.2 in the ignored local `.tools` environment.

The binary's direct dynamic dependencies are `libX11`, `libGLX`, `libOpenGL`,
`libasound`, and language/system runtime libraries. Observed transitive dependencies
include `libxcb`, `libXau`, `libXdmcp`, and `libGLdispatch`; the graphics driver may
load further system libraries at runtime.

The initial 12-case CPU suite, real GPU tests, and gameplay verification have passed
with GCC and Clang. The scripted game collects all three cores and wins in 449 ticks.
CPU AddressSanitizer/UndefinedBehaviorSanitizer checks passed. The real ALSA default
device accepted silent 48 kHz stereo output; this does not verify physical speakers.

First-increment checks on 2026-10-04: all six CTest groups passed in GCC debug, GCC release,
Clang debug, and the native AddressSanitizer/UndefinedBehaviorSanitizer build, with
no skipped hardware cases. This includes native key/alias/focus/close events and
actual resized framebuffer dimensions. Hidden test windows bypass window-manager
resize interception; visible application windows remain normally managed.

The installed release executable also ran from `/tmp` for 600 scripted frames,
collected all three cores, and completed the game. Its framebuffer capture was
visually inspected. The final scene used one draw call. An exploratory run at the
window-manager-selected 1050x1360 viewport measured CPU update/submission times of
0.64 ms median, 2.50 ms p95, and 4.49 ms p99 over 570 post-warm-up samples. This is
not a controlled 1080p benchmark or a GPU/full-frame performance guarantee.

## Current contracts and limitations

- One window/context and one renderer at a time. Window and rendering operations,
  including destruction, stay on the main thread. Mixing is confined to the audio
  worker; `play` has a single main-thread producer.
- `AudioOutput` owns registered immutable samples until its worker joins at shutdown.
  Direct CPU `Mixer` use still borrows sample views; callers must validate/retain them.
  WAV support is PCM16 mono/stereo at selected rates; playback is always 48 kHz.
- Queue overflow rejects commands; voice exhaustion drops excess play requests and
  increments diagnostics. Tickets acknowledge enqueueing, not successful playback.
  No voice-stealing, streaming, or automatic device reconnection is implemented.
- The static collision API assumes positive player extents and no initial overlap.
  It resolves X then Y; it is not a general rigid-body solver or arbitrary-shape CCD.
- Keyboard bindings are fixed and captured at startup. Fullscreen/VSync are requests
  to the window system; compositor behavior is not guaranteed by a successful call.
- Bitmap text is an uppercase subset. Diagnostic strings and screenshot capture
  allocate; a whole-engine allocation-free guarantee has not been established.
- CPU timing excludes swap waiting; asynchronous GPU intervals cover submitted
  rendering. Controlled 1080p performance budgets remain future work.
  Tiling-window-manager sizing is honored.
- Automated CPU and GPU checks complement rather than replace the pending manual
  focus, fullscreen, scaling, minimized-state, and audible-output checks.

## Second-increment validation

All seven CTest groups pass with GCC release, Clang debug, and native
AddressSanitizer/UndefinedBehaviorSanitizer builds. There are now 21 named CPU cases
(12 foundation/game cases and 9 content cases), plus GPU, platform, audio-transport,
and game integration checks. No hardware tests were skipped on the testing PC.

The installed executable was launched from `/tmp` and resolved its installed
`share/feature_lab` assets. The original and round-tripped scenes both complete the
replay in 449 ticks; collected entity handles become stale, and restart invalidates
the old world's handles. The 600-frame visible run and missing-texture fallback
were captured and inspected. Two file textures are shared by 252 starting entities.

The visible release run used five ordered batches; unlike the initial all-built-in
atlas, file textures require texture switches. At the window-manager-selected
1050x1360 viewport, CPU update/submission measured 0.94 ms median, 5.09 ms p95, and
8.04 ms p99 over 570 samples. This remains an exploratory CPU-only measurement,
not a controlled GPU or full-frame benchmark.

## Added in the third increment

- Validated ETMP tilemap files, atlas lookup, ordered layers, immutable cell storage,
  16×16 chunk occupancy, visible-cell traversal, and deduplicated solid metadata.
- Validated EANI clip files with integer durations, looping and one-shot playback,
  pause/restart, immutable clip snapshots, and exactly-once completion reporting.
- Feature Lab loads a 64×32 three-layer map and seven scene entities. Player walk,
  pickups, and machinery animate; the exit barrier opens only after its animation
  finishes. A clamped follow camera with +/- zoom and a fixed HUD displays traversal
  counters. Same-layer tiles precede sprites; sprite ties use persistent IDs.
- Nine tile/animation test cases plus a gameplay door/pause/zoom/restart case.
  GPU tests verify atlas frame selection and world/HUD camera switching.

## Third-increment validation

GCC debug/release, Clang debug, and native AddressSanitizer/UndefinedBehaviorSanitizer
builds pass all eight CTest groups, including real OpenGL and native window events,
with no skipped hardware cases. The in-house
CPU suites now contain 31 named cases: 13 foundation/game, nine content, and nine
tile/animation cases. Decimal tile-boundary regression checks cover culling and
collision queries against the same geometry.

Original and serialized/reloaded scene, map, and animation data produce matching
replays. Both collect three cores and finish in 1,381 ticks with one door-completion
event. Pause freezes animation progress; restart resets the door and camera and
invalidates old entity handles. The door blocks movement while closed.

The installed release executable resolved its assets from `/tmp` and completed a
1,600-frame visible scripted run. Start, completion, and missing-texture captures
were inspected. The final 1050×669 view traversed six of 24 layer chunks and drew
343 tiles out of 6,144 map cells. This demonstrates culling on that view, not a
controlled performance benchmark. No new runtime libraries were introduced.

## Added in the fourth increment

- Renderer-owned, serial-checked offscreen color targets with create/resize/release
  APIs, framebuffer completeness checks, eight-target/64 MiB storage limits,
  and separate live resource accounting. Invalid resize requests preserve contents;
  successful resizes replace storage atomically and keep the handle stable.
- Transparent target composition with linear-space premultiplied storage, sRGB
  sampling/output, correct separate alpha blending, and feedback-loop rejection.
  Uploaded texture/tint inputs retain their existing straight-alpha convention.
- Nested top-left pixel scissor rectangles, parent/viewport intersection, a bounded
  16-level stack, and ordered flushes when clip state changes.
- Feature Lab renders a live minimap before its window pass. It shows walls, cores,
  exit state, player, and camera bounds, and resizes with the window. Page Up/Down
  scroll a clipped status panel even while gameplay is paused.
- Seven named GPU cases in `render_targets`, including screenshot orientation,
  offscreen alpha through multiple passes, clip errors/limits, 40 consecutive target
  resizes, failure preservation, and pixel comparisons of the actual scrolling UI.
  Native event tests also cover the new Page Up/Down bindings.

## Fourth-increment validation

All nine CTest groups pass in GCC debug/release, Clang debug, and native
AddressSanitizer/UndefinedBehaviorSanitizer builds, with no hardware skips. The
seven new render-target cases run against the real OpenGL 4.6 driver. The existing
31 named CPU cases and gameplay replay remain passing.

The installed release executable ran from `/tmp` for 1,600 scripted frames,
resolved its packaged assets, collected all three cores, and finished in 1,381 ticks
with one door-completion event. Start and completion captures were inspected at
the window-manager-selected 1050×1360 viewport. The final frame used eight window
batches and one minimap batch, with one live 228×114 target (103,968 color bytes).
This confirms the example integration; it is not a controlled performance benchmark.

## Added in the fifth increment

- In-house RIFF/WAVE PCM16 reader/writer, mono/stereo at 22.05/24/44.1/48 kHz,
  bounded chunks/duration, exact-length validation, and offline conversion to 48 kHz.
- Output-owned immutable sound storage, checked sound handles, unique voice tickets,
  looping, pause/stop, balance/pan, master/music/effects gains and group pause.
  Gain changes ramp over 64 output frames. Mixing and command application do no
  allocation, locking, or I/O; file loading/validation stays on the main thread.
- Bounded FIFO command processing and counters for completed/stopped/dropped voices,
  stale controls, queue rejection, and active voices. Shutdown joins before sample
  storage is released. The sound bank holds at most 32 sounds / 64 MiB of float PCM.
- Four WAV assets generated in-house: stereo music, pickup, machinery, and door.
  Feature Lab demonstrates simultaneous voices, loops, a positional machine emitter,
  pause/restart, M mute, and N/B music/effects volume presets. Initialization, asset,
  queue, or device errors disable sound and retain playable gameplay.
- Nine new CPU sound cases and four audio transport cases; native event tests cover
  M/N/B. Decoder tests use an independent byte fixture and malformed/truncated files;
  sample tests check resampling, mixing, exact loop boundaries, and block-invariant
  gain ramps. Null-device integration runs the complete game soundtrack scenario.

## Fifth-increment validation (2026-10-05)

All ten CTest groups pass in GCC debug/release, Clang debug, and native
AddressSanitizer/UndefinedBehaviorSanitizer builds, with no skipped hardware cases.
There are now 40 named CPU cases, four audio transport cases, and seven offscreen
GPU cases, alongside the existing graphics/platform/replay checks.

The installed release executable ran from `/tmp` with its packaged WAV files and
the real ALSA default device for 1,600 scripted frames. It won in 1,381 ticks, started
six voices (two loops and four effects), completed the four one-shot effects, and
reported zero dropped plays or queue rejections. The machinery voice stops on win;
music continues until shutdown. Startup and completion captures were inspected.
The silent default-device probe also passed. These checks confirm software output
and device acceptance; they do not establish physical speaker audibility or
successful handling of a real device disconnect.

## Added in the sixth increment

- In-house retained UI with at most 64 stable-ID buttons, checkboxes, and normalized
  sliders. Layout validation preserves the previous UI on error. Updates use fixed
  action storage, coalesce changes per widget, and do not allocate.
- Tab/Shift+Tab and directional focus, keyboard activation/adjustment, clipped hit
  testing, disabled controls, primary-button capture, dragging beyond widget bounds,
  and wheel adjustment. Layout changes and focus loss cancel active pointer captures.
- Native X11 pointer motion, primary-button edges with press/release positions,
  wheel input, and UI key mappings. Modal routing suppresses gameplay input, including
  held controls until release after the menu closes.
- Responsive F1 settings panel: resume, restart, quit, master/music/effects sliders,
  mute, VSync request, and fullscreen request. Opening pauses simulation and audio;
  closing restores prior pause state. Settings apply to the session only.
- Eleven new CPU UI cases, a fifth audio transport case for settings/command
  coalescing, expanded native event checks, and two more GPU cases for focus/value
  pixels, clip restoration, and panel rendering at multiple sizes.

## Sixth-increment validation (2026-10-05)

All eleven CTest groups pass in GCC debug/release, Clang debug, and native
AddressSanitizer/UndefinedBehaviorSanitizer builds, with no skipped hardware cases.
The suites now include 51 named CPU cases, five audio transport cases, and nine
render-target/UI GPU cases, plus graphics, native events, and replay checks.
Formatting checks pass under the repository's clang-format configuration.

The installed release executable resolved packaged assets from `/tmp` and completed
1,600 scripted frames, winning in 1,381 ticks with three cores and one door-completion
event. The real ALSA default output reported six started voices, four completed
one-shots, and no dropped plays or queue rejections. A second installed run opened
the settings panel with audio disabled and retained zero simulation ticks for ten
frames. Enabled/disabled settings and completion captures were visually inspected
at the window-manager-selected 1050×1360 viewport. GPU tests also render settings
at 320×240, 640×480, and 1280×720. No new runtime dependencies were introduced.

These checks establish the implemented UI baseline. The manual fullscreen/focus/
scaling matrix, physical speaker checks, dedicated title screen, and remaining
feature-manifest items are still outstanding.

## Added in the seventh increment

- Reusable bounded persistence records with an in-house CRC32 implementation,
  per-user config/state path discovery, three slots, and existing atomic-write
  machinery. Reads do not create directories and are bounded to 64 KiB.
- Versioned ECFG audio/VSync settings, loaded on startup and saved after changes
  when closing settings or exiting normally. Malformed settings retain defaults;
  command-line VSync overrides do not silently rewrite preferences.
- Versioned ESAV checkpoints with facility fingerprints, persistent collected-core
  IDs, player position, zoom, simulation clock, pause/win/door state, and exact
  animation positions. Restoration validates a fresh world before committing it,
  invalidates old handles, and resets interpolation history to the loaded position.
- Three selectable save slots in the F1 panel, success/failure notices, and isolated
  `--user-data`, `--load-slot`, and `--save-slot` workflows. Menu pause is not saved as
  gameplay pause. Loads restart audio loops without replaying historical effects.
- Thirteen CPU persistence cases, a twelfth UI case for slot actions, and a sixth
  audio transport case for restoration. Interrupted-write tests terminate child
  processes after file sync and after rename, checking the committed old/new file.
- Initial mixer gains apply before worker startup, with a tenth sound case proving
  exact first-sample mute/volume behavior.
- Stable slider midpoint mapping under fractional panel scales, retaining the
  existing no-change behavior when selecting an already centered thumb.

## Seventh-increment validation (2026-10-05)

All twelve CTest groups pass in GCC debug/release, Clang debug, and native
AddressSanitizer/UndefinedBehaviorSanitizer builds, with no hardware skips. There
are 66 named CPU cases, six audio transport cases, and nine render-target/UI GPU
cases, plus the graphics, native-event, and replay checks. Formatting checks pass.

The installed release executable ran from `/tmp` using an isolated user directory.
A 500-tick, one-core checkpoint was saved, loaded in a new process, and saved into
a second slot with byte-for-byte identical contents. A 1,600-frame replay completed
at tick 1,381, saved into slot three, and reloaded with all three cores, the open
door, one completion event, and the win state intact. Both restored-state captures
were visually inspected. An independent ECFG fixture restored 40% master, 20%
music, 70% effects, mute enabled, and VSync disabled in the visible panel.

Child-process tests leave the old complete file before rename and the new complete
file after rename; an orphan temporary does not prevent the next save. Malformed
records and incompatible facility data retain live state. A sample-level test
confirms initial mute produces zero samples before any gain ramp. No runtime
libraries were added. Actual power loss, version migration, physical speaker
checks, and the remaining feature coverage gaps are not claimed complete.

## Added in the eighth increment

- In-house box/box, circle/circle and circle/box detection with closed-shape contact
  semantics. A bounded 4-unit uniform grid handles moving-body snapshots with pair
  deduplication, reciprocal layer/mask filtering, stable ordering, and diagnostics.
- Transactional snapshot publication, bounded preallocated working buffers, and
  trigger enter/stay/exit events. Priming restores overlap history without emitting
  historical enter events. Query-only bodies skip grid work but remain queryable.
- Shape overlap queries and nearest segment queries with masks, sensor exclusion,
  ignored IDs, hit fractions/points/normals, and deterministic equal-hit ordering.
- Feature Lab's circular patrol alarm records entries without blocking the player.
  E operates a nearby switch only within range and with a clear segment to it.
  Disabled alarms filter out player contacts. Patrol motion derives from integer
  simulation ticks; pause freezes it and restored active contacts do not count twice.
- ESAV v2 adds alarm-entry count and disabled state. Older v1 checkpoints are
  explicitly rejected before changing live state; settings remain ECFG v1.
- Nine new CPU collision cases, expanded replay/checkpoint and native E-key checks,
  and a tenth GPU render-target/UI case for alarm colors. The collision fixture
  compares 100 moving/filtering bodies against brute force over 80 shuffled snapshots.

## Eighth-increment validation (2026-10-05)

All thirteen CTest groups pass in GCC debug/release, Clang debug, and native
AddressSanitizer/UndefinedBehaviorSanitizer builds, with no hardware skips. The
suites now include 75 CPU cases, six audio transport cases, and ten render-target/UI
GPU cases, alongside native-event, graphics, and gameplay replay checks. Formatting
checks pass under the repository configuration.

The installed release executable launched from `/tmp`, saved an active alarm contact
at tick 35, loaded it in a new process, and saved byte-identical state into a second
slot. The alarm-entry counter remained one. A full 1,600-frame run collected all
three cores and finished in 1,381 ticks with one door completion and zero audio
queue rejections/dropped plays. The active-alarm and completion captures were
visually inspected at 1050×1360. The procedural circle adds one GPU texture; the
existing scene/map assets were not regenerated or changed.

CPU checks also verify blocked switch rays, restored disabled alarms, stable contact
history, and full-capacity pair/event output. GPU pixels verify active red and
disabled green alarm rendering. No new runtime dependencies were introduced.
Discrete trigger sampling and the bounded example collision world remain explicit
limits; this increment does not add general rigid-body dynamics or moving-body CCD.

## Added in the ninth increment

- In-house, allocation-free 240-frame timing history with finite-input validation,
  rolling mean/p95/max and reset. F2 displays CPU/wall timing, tick/step counts,
  discarded time, per-pass rendering counters, target memory and collision counters.
- F3 draws actual published collision snapshots with contact/sensor/static/moving
  colors, plus camera bounds. Public line/rectangle/box/circle debug helpers use the
  existing ordered renderer, culling and clipping without new GPU resources.
- F10 advances exactly one full tick while paused, preserving pause and snapping
  interpolation to the result. Settings capture the new controls; pause/restart
  priority and terminal win state are explicit. Scripted driving honors controls.
- `--diagnostics-demo` enables both views. CPU measurement excludes screenshot
  readback; completed-frame counters lag one frame and are not GPU elapsed timings.
- Seven new CPU diagnostics cases cover bounded history, invalid/large samples,
  toggles, focus cancellation, catch-up edge consumption, menu capture, checkpoints,
  control priority and full-game stepped replay equivalence. Two additional GPU
  cases check outline pixels/validation/clipping and panels at four viewport sizes.
  Native events cover F2/F3/F10; a seventh audio transport case checks paused cues.

## Ninth-increment validation (2026-10-06)

All fourteen CTest groups pass in GCC debug/release, Clang debug, and native
AddressSanitizer/UndefinedBehaviorSanitizer builds, with no hardware skips. The
suites include 82 named CPU cases, seven audio transport cases, and twelve
render-target/UI GPU cases, plus native-event, graphics, and replay checks.
Formatting checks pass for all touched C++ sources.

The installed release app launched from `/tmp`. A native X11 event driver paused
before the first tick, sent four F10 presses and five additional repeat events,
and ran 500 rendered frames. Exactly four simulation ticks occurred; the saved
paused checkpoint reloaded and re-saved byte-for-byte unchanged. A second installed
run with diagnostics enabled completed 1,600 frames, collected three cores and
finished at tick 1,381 with one door completion. The real ALSA default output
reported six started voices, four completed one-shots and no dropped plays or
queue rejections. Paused-step and completed-game captures were visually inspected
at 1050×1360; GPU tests also cover 320×240, 640×480 and 1280×720 panels.

The full stepped CPU replay matches ordinary playback checkpoints, including exact
animation, trigger and door state. Debug drawing adds no textures or targets.
These checks do not establish GPU elapsed timings, broad performance budgets, safe
shader reload, replay-file storage or completion of the remaining feature manifest.
No runtime dependencies or persistence schema versions changed.

## Added in the tenth increment

- Transactional renderer shader reload: bounded source validation, separate candidate
  compilation/linking, strict active-interface reflection, sampler initialization,
  uniform-location rebinding and publication only after all checks pass.
- Bounded stage/link logs and renderer revisions. Failed reads/compiles/links or
  incompatible interfaces preserve the exact current program. Compiler diagnostics
  are recoverable; unrelated GL API/driver errors still fail renderer health.
- Candidate and retired-program cleanup, detached shader objects, and unchanged
  texture/target handles and completed-pass readback across reloads.
- Shipped sprite GLSL files with embedded startup fallbacks. Feature Lab loads them
  at startup; F5 reloads, F6 injects a compile failure, and `--shader-error-demo`
  starts with that failure visible. Status/revision appears in the HUD; full
  diagnostics go to the console. Commands respect modal capture and press edges.
- Eight GPU shader cases cover fallback parity, changed pixels, compile/link/interface
  rejection, source bounds, active-pass rejection, uniform/sampler relocation,
  premultiplied composition, repeated retirement, real GL errors, file recovery and
  settings capture. Native tests cover F5/F6. No new runtime dependencies.

## Tenth-increment validation (2026-10-06)

All fifteen CTest groups pass in GCC debug/release, Clang debug, and native
AddressSanitizer/UndefinedBehaviorSanitizer builds, with no hardware skips. The
existing 82 named CPU cases, seven audio transport cases and twelve render-target/UI
GPU cases remain passing; eight new GPU shader cases cover reload behavior.
Formatting checks pass for all touched C++ files.

An installed release run from `/tmp` loaded shader revision 2, deliberately failed
compilation, retained revision 2, and completed 1,600 frames at tick 1,381 with three
cores and exactly one door completion. Real ALSA output reported six started voices,
four completed one-shots, and no dropped plays or queue rejections. The failure HUD
and completed game were visually inspected at 1050×1360.

A second installed run received native F5/F6 events: recovery to revision 3, a failed
compile retaining revision 3, and recovery to revision 4. Five repeated F5 events
without release did not cause extra reloads. The run continued for 500 ticks and its
ready HUD was visually inspected. A separate malformed-file startup used embedded
revision 1 and rendered normally. Source assets were unchanged by failure scenarios.

GPU tests confirm failed reloads retain a custom color-swapping program, not merely
the default fallback, and that relocated uniforms preserve transparent render-target
composition. Twenty-four success/failure cycles verify retired program deletion and
zero attached stage objects after publication. No GPU timer queries, file watcher,
background compiler or material system are claimed by this increment.

## Added in the eleventh increment

- Public context-owned `GpuTimer` with eight paired timestamp slots, increasing frame
  tags, FIFO availability checks, and result reads only after both queries are ready.
  Saturation skips samples; collection never waits, flushes, or overwrites unread work.
- Capability reporting for 64-bit timestamps, separate pending/completed/skipped/
  invalid counters, and cleanup of pending or interrupted timestamp scopes.
- Allocation-free history of the last 240 completed GPU intervals with mean/p95/max
  and latest tag/time, independent of CPU history. Reversed timestamps are discarded.
- F2 GPU rows display timings, sample age, pending capacity, skips and invalid results.
  The measured interval includes minimap/window/UI/debug rendering; readback, present,
  simulation and shader compilation are outside it. Shutdown polls once without waiting.
- Seven deterministic CPU queue/history cases and four OpenGL cases cover delayed
  results, saturation/recovery, tag validation, narrow-counter fallback, FIFO reuse,
  clock reversal, real measurements, pixels, reload/resize, object deletion and
  responsive overlay rendering. No runtime dependency or persistence changes.

## Eleventh-increment validation (2026-10-06)

All seventeen CTest groups pass in GCC debug/release, Clang debug, and native
AddressSanitizer/UndefinedBehaviorSanitizer builds, with no hardware skips. The
suites now include 89 named CPU cases, seven audio transport cases, twelve
render-target/UI GPU cases, eight shader cases and four GPU timer cases, plus
native-event, graphics and full-game checks. Formatting checks pass.

The test PC reports 64-bit timestamp counters. Deterministic backend tests prove
unavailable results are never read, full pools do not overwrite slots, and bounded
FIFO collection resumes after delayed queries become ready. Real GPU tests verify
query deletion, unchanged pixels, pool saturation/recovery and continued timing
across shader reload and render-target resizing. Overlay layouts pass at 320×240,
640×480, 1280×720 and 1050×1360.

An installed release run from `/tmp` completed 1,600 frames with diagnostics enabled
and a deliberately failed shader reload. It finished gameplay at tick 1,381 with
three cores and one door completion. All 1,600 timing intervals were collected,
with no skips or invalid timestamps; the GPU history remained bounded at 240.
Audio reported no dropped plays or queue rejections. The visible overlay showed
sample age and pending work; active and completion captures were inspected.

A separate installed native-input run remained paused except for four F10 steps.
It rendered 500 frames and collected 500 GPU measurements, demonstrating that GPU
tags track rendered frames independently of simulation ticks. Both installed runs
had no remaining pending samples after their final nonblocking poll; screenshot
readback elsewhere in those runs can naturally make queued queries ready. These
runs verify timing collection and lifecycle, not controlled performance budgets or
pure GPU busy-time measurements. Narrow-counter fallback is covered deterministically;
it was not exercised on different physical hardware.

## Added in the twelfth increment

- All-or-nothing GPU texture replacement with unchanged handles, strict batch/payload
  validation, staged allocation, and old-object retirement. Works at full handle
  capacity; 64 MiB bounds candidate pixel payload.
- Feature Lab coordinates strict disk reads, shared atlas constraints, immutable
  CPU snapshots, a fresh cache and GPU replacement before publishing any change.
  F7 reloads, F8/`--texture-error-demo` demonstrates rejection, and the HUD/console
  expose revision and recovery status. F7 also recovers startup fallback textures.
- Eight GPU/integration cases exercise shared pixels, failure retention, resource
  lifetimes and limits, atlas/sampling/alpha behavior, snapshots and input capture.
  Native key tests include F7/F8. No runtime dependency or file-format changes.

## Twelfth-increment validation (2026-10-06)

All eighteen CTest groups pass in GCC debug/release, Clang debug, and native
AddressSanitizer/UndefinedBehaviorSanitizer builds, without hardware skips. The new suite adds eight texture GPU/integration cases to the
existing CPU, audio, native-event, graphics, shader, timer and full-game checks.
Formatting and strict compiler warnings pass.

An installed release run from `/tmp` used a temporary copy of the assets, leaving
source files untouched. Native F7/F8 events exercised a startup rejection, a
malformed tile file, successful recoloring of both resident atlases, held-key
suppression, F8 rejection of the new version, and F7 recovery. The log recorded
three retained transactions and two commits, ending at texture revision 3. The
1,600-frame game finished at tick 1,381 with all three cores and one door completion.
Audio reported six started voices, four completed one-shots, no dropped plays and
no rejected commands. All GPU timing samples completed with no skips or invalid
results. The inspected capture showed the changed tiles and the ready revision.

A second installed run started with the tile fallback, recovered the original file
through native F7, and rendered 500 frames at revision 2. Pixel tests separately
verify that handles and resource counts remain stable, failed batches change no
images, old CPU readers survive publication, and repeated full-capacity reloads
retire old OpenGL objects. Validation does not force a real driver out-of-memory
condition or establish a whole-engine memory/performance budget.

## Current scope

The component model remains bounded to optional sprite/collider records. Tilemaps
are immutable; atlas grids are regular and clips use tick durations. Render targets
are single-sample sRGB RGBA8 color buffers. No external libraries were added.
Richer scene workflows and several other planned systems remain unfinished. See [format/API contracts](FORMATS.md)
for limits, rendering conventions, and lifetimes.

## Next increment

Add configurable keyboard bindings with conflict validation, persisted settings,
and native/CPU tests plus a Feature Lab rebinding workflow (F02).
