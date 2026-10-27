# Implementation status

The fifty-sixth increment prepares and synchronizes the resolved save-directory
chain before record creation, with deterministic errors and process-interruption retry tests.
See [retained results](../validation/2026-10-24-storage-directories/RESULTS.md) and
[release readiness](RELEASE_READINESS.md) for the remaining gates.
It is **not completion of all six design milestones**.
The target requirements remain in [ENGINE_PLAN.md](../ENGINE_PLAN.md).

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
| F01 | Partial | `platform_lifecycle`, graphics/input/no-display tests; visible fullscreen/focus/client unmap/restore; native gameplay/replay suspension; six-size pixel/UI/pointer matrix | WM-supported minimize/restore and physical monitor-scale matrix; see [platform validation](PLATFORM_VALIDATION.md) |
| F02 | Partial | CPU edge/focus/capture tests; native key-repeat/alias/pointer/wheel/focus tests; modal gameplay routing; ten configurable keyboard/mouse actions, conflict/capture/persistence tests; mixed-device aliases, modifier chords, keypad identities, active XKB layout groups, international symbols and mapping notification refresh | Physical multi-layout/compositor validation; dead keys, text composition and Unicode glyph rendering are outside binding v1 |
| F03 | Implemented (v1) | Parented rotating/scaled/mirrored gallery; sampled affine hierarchy; camera corner/aspect/follow/zoom checkpoints; pause/step/save/replay pose tests; GPU shear/mirror/culling/resize pixels | Broader manual camera and display matrix; transform caching is optional |
| F04 | Implemented (v1) | GPU pixel/order/color/atlas/culling/batch tests; animated/parented sprites; transactional reload; independent nearest/linear min/mag filters and clamp/repeat axes; atlas inset and sampling gallery | Optional mipmaps, anisotropy and atlas-cell repetition |
| F05 | Implemented (v1) | ETMP round trips, layer/atlas validation, chunk traversal vs brute-force reference, solid-cell union and swept query tests; large scrolling map | Optional editing/streaming tools |
| F06 | Implemented (v1) | EANI reader/writer; exact frame timing, loops, pause/restart, large advances; animated player/machine/door with exactly-once completion | Optional frame callbacks and richer animation types |
| F07 | Implemented (v1) | Handle/compaction/hierarchy tests, scene round trips, deferred deletion; title/gameplay and Facility/Annex transitions; typed runtime pools with checked access, intersecting queries, ownership/exception/move tests, allocation-free steady queries and pickup integration | Optional custom component serialization codecs, query scheduling and broader ECS tooling |
| F08 | Implemented (v1) | ETEX/ETMP/EANI/WAV/EFNT readers, cache snapshots/reload tests, path validation, fallback demo, GPU lifetime tests, transactional shader/texture/font/animation/tilemap/scene/audio reload with gameplay-content replay isolation; optional bounded inotify watching, debounce, metadata recovery and watched GPU pixel tests; CPU-only bundle and single-file validation with file/level diagnostics and versioned JSON reports | Optional background loading and broader asset tooling |
| F09 | Implemented (v1) | Swept player boxes; box/circle contacts; moving-body grid vs brute force; reciprocal filters; trigger history; overlap/segment queries; alarm/switch and restore tests | Rigid-body dynamics, rotating shapes and moving-body CCD are outside initial scope |
| F10 | Partial | PCM16 WAV assets/resampling, loops, voice controls, gain ramps, master/music/effects controls, stereo emitter, null-device soundtrack/restart tests; deterministic transport recovery faults; real ALSA file-sink failure/fallback; recovery counters and listening probe | Physical disconnect/recovery, system suspend/resume and audible-output checks; see [audio validation](AUDIO_VALIDATION.md) |
| F11 | Partial | Atlas bitmap text with authored metrics, bounded allocation-free layout, font fallback and visual gallery; clipped status panel, buttons/checkboxes/sliders, keyboard/pointer navigation, bounded layout, modal settings and title screen; `ui`, scene-flow and GPU pixel tests | Broader manual navigation checks |
| F12 | Implemented (v1) | Live minimap, clipped scrolling panel; seven GPU cases for pixels, alpha composition, clipping, resizing, resource limits and example integration | Optional multisampling, HDR and post-processing |
| F13 | Implemented (v1) | Clock/catch-up, paused stepping, ERPL recording/playback, per-command state checks, seeded random state, save migration and full-game replay tests | Cross-machine float determinism and replaying menu/audio/render commands are outside v1 |
| F14 | Implemented (v1) | Versioned scene/checkpoint/config formats; three-slot UI; CRC, bounds and semantic checks; unchanged live game on failed load; interrupted-process tests; atomic-write syscall faults, cleanup/error precedence and descriptor reuse; resolved ancestor synchronization before publication with preparation-fault and process-exit retry checks | Physical power-loss/filesystem crash behavior and content migration |
| F15 | Partial | Console errors, GL callback, F2 rolling timing/counter overlay, F3 collision/camera outlines, target accounting, CPU percentiles, shader reload/failure recovery, GPU pixel/interface/lifetime tests, bounded asynchronous GPU timing with sample age/skips; separate preview/swap timing, [controlled stress results](../benchmarks/2026-10-16-validation.md) and [bounded failure evidence](../benchmarks/2026-10-17-gpu-timing.md); [recorded gameplay profiles](../benchmarks/2026-10-17-replay-results.md) | Broader drivers and manual fault scenarios |
| F16 | Partial | Configurable stress arena, CPU/GPU repeated teardown checks, allocation-free steady CPU tests, tracked buffer/texture/target bytes, JSON benchmarks, repeated 1080p baselines and independent workload comparisons, installed CPU runner and hashed relocatable archives with CPU/OpenGL smoke tests | Full allocator/driver accounting, broader gameplay/display budgets and hardware/manual checks |

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
- `AudioOutput` owns immutable samples; replacements retire old buffers only after
  worker acknowledgement, and shutdown joins before freeing remaining storage.
  Direct CPU `Mixer` use still borrows sample views; callers must validate/retain them.
  WAV support is PCM16 mono/stereo at selected rates; playback is always 48 kHz.
- Queue overflow rejects commands; voice exhaustion drops excess play requests and
  increments diagnostics. Tickets acknowledge enqueueing, not successful playback.
  No voice-stealing, streaming, or automatic device reconnection is implemented.
- The static collision API assumes positive player extents and no initial overlap.
  It resolves X then Y; it is not a general rigid-body solver or arbitrary-shape CCD.
- Ten actions accept supported keyboard or mouse bindings; reserved controls stay fixed. Fullscreen/VSync are requests
  to the window system; compositor behavior is not guaranteed by a successful call.
- Bitmap text is an uppercase subset. Diagnostic strings and screenshot capture
  allocate; a whole-engine allocation-free guarantee has not been established.
- CPU timing excludes swap waiting; asynchronous GPU intervals cover submitted
  rendering. Controlled 1080p synthetic stress and native-viewport gameplay reports
  are available; candidate workload acceptance, broader display measurements and
  remaining allocation coverage are tracked in [release readiness](RELEASE_READINESS.md).
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

## Added in the thirteenth increment

- Validated, unique letter-key bindings for ten actions, with atomic assignment,
  held-key suppression, alias aggregation, and X11 mapping-notification refresh.
- F1 Controls page with capture, conflict feedback, cancellation, restore defaults,
  responsive layout and modal gameplay isolation. Gameplay prompts reflect current
  bindings; fixed navigation remains available.
- ECFG v2 persists bindings; v1 loads existing audio/VSync plus default bindings and
  upgrades on the next preference save. Checkpoints retain ESAV v2.
- Six CPU cases plus expanded native/GPU checks. No additional runtime libraries.

## Thirteenth-increment validation (2026-10-07)

All nineteen CTest groups pass in GCC debug/release, Clang debug and native
AddressSanitizer/UndefinedBehaviorSanitizer builds, with no hardware skips. Six
new CPU cases cover bindings and settings migration; native events cover changed
bindings, aliases, held-key suppression, invalid-table retention and synthetic
mapping notifications. Both settings pages render at 320×240, 640×480 and 1280×720.
The sanitizer audio transport group initially hit its ten-second timeout while
compilers and visible validation runs competed for resources; its isolated rerun
passed in 2.80 seconds. No sanitizer error was reported. Strict warnings, formatting
and diff whitespace checks pass.

Two installed release runs from `/tmp` each rendered 1,500 frames. Native pointer
and keyboard events opened Controls, attempted a conflicting binding, assigned Q
to Move Left, closed settings to save, and reopened the page. The second process
loaded the same 72-byte ECFG v2 file and displayed Q without another edit. Captures
of both runs were inspected, including the updated movement hint. Gameplay remained
paused while the menu was open. User settings were isolated under `build/`; no
system keyboard configuration was changed. These runs are interaction checks, not
controlled performance measurements.

## Added in the fourteenth increment

- Default title screen with New Game, Continue from a chosen save slot, Resume
  Session and Quit. F1 Title suspends the active session with its pause state intact.
- New/Continue validate and stage world replacement before changing live state;
  failure retains session and entity handles. Resume retains the same world.
- Shared graphics resources, no world/minimap drawing or simulation on the title,
  initially paused audio and no duplicate voices on resume. Existing automated/demo
  modes still start in gameplay; `--play` explicitly bypasses the title.
- Six CPU transition cases, a GPU title/layout/lifetime case and an audio pause/resume
  case. No new runtime dependencies or persistence-format changes.

## Fourteenth-increment validation (2026-10-07)

All twenty CTest groups pass in GCC debug/release, Clang debug and native
AddressSanitizer/UndefinedBehaviorSanitizer builds, without hardware skips. After
shortening a clipped slot-selector label, the scene-flow and GPU rendering groups
were checked again on the final code in all three non-sanitized configurations;
the full sanitizer run also uses the final code. Formatting, strict warnings and
diff whitespace checks pass.

An eight-frame installed run rendered the title before gameplay, with zero ticks
and no minimap draws. Its final capture was inspected. A 2,400-frame native-input
run from `/tmp` exercised New Game, save slot 1, return to title, failed Continue
from an empty slot, successful Continue from slot 1, Resume Session, another New
Game, and return to title. The saved file remained byte-identical throughout the
load/resume/new-game operations. Four gameplay entries and four title returns were
recorded; new games began at tick zero and Continue restored the saved tick six.

A separate installed scripted run bypassed the title as intended, rendered 1,600
frames and completed the game at tick 1,381 with three cores and one door completion.
Visible runs used isolated user-data directories and disabled audio; the null-device
suite separately verifies initially paused audio and resume without duplicate voices.
GPU checks cover title rendering at 320×240, 640×480, 1280×720 and 1050×1360 and
stable resource counts across repeated transitions. These are functional checks,
not controlled performance measurements or arbitrary authored-level transitions.

## Current scope

The component model remains bounded to optional sprite/collider records. Tilemaps
are immutable; atlas grids are regular and clips use tick durations. Render targets
are single-sample sRGB RGBA8 color buffers. No external libraries were added.
Transitions between different authored levels and several other planned systems remain unfinished. See [format/API contracts](FORMATS.md)
for limits, rendering conventions, and lifetimes.

## Added in the fifteenth increment

- In-house SplitMix64 state with known-sequence tests, checkpoint ESAV v3 storage,
  v2 migration, seeded machine indicators and restart behavior.
- Bounded ERPL v1 files: initial checkpoint, logical input edges and held state,
  pre-update ticks and allocation-free per-command simulation hashes.
- CLI human/scripted recording, visual playback and CPU-only verification; content,
  rule and state mismatch reporting; normal-exit atomic writes and capacity stop.
- Eight CPU replay cases covering the complete game, paused starts, restart/step,
  corruption, incompatible content/rules, limits and unchanged files on failed writes.

## Fifteenth-increment validation (2026-10-07)

All twenty-one CTest groups pass in GCC debug/release, Clang debug and native
AddressSanitizer/UndefinedBehaviorSanitizer builds, without hardware skips.
The new replay group contains eight cases. After the final playback help-text
adjustment, gameplay/no-display checks were repeated in all four builds. Both
installed replay files verify in all four builds with identical per-command and
final hashes. Strict compiler warnings, formatting and diff whitespace checks pass.

An installed release recording from `/tmp` produced 1,600 commands in a 45,010-byte
ERPL file, finished at tick 1,381 with three cores and one door completion, and
verified with DISPLAY unset. Installed visual playback produced a byte-identical
ESAV checkpoint. A 170-command native X11 input recording exercised movement,
F1 suppression, pause, F10 step, resume, restart, interact and Escape-to-save. Its
playback checkpoint was also byte-identical. Both recording and playback captures
were inspected. The replay HUD labels live gameplay input as disabled. A further native-input
playback run ignored movement, restart, pause, F1 and F10 and retained the same
final checkpoint. The final capture was inspected after updating playback hints.

A valid-CRC file with an altered state hash fails at command zero. Conflicting
replay options and invalid/overflowing seeds fail before opening a display.
Visible runs used isolated user-data paths and disabled audio; existing null-device
checks cover audio transport. These are functional checks performed alongside
build work, not controlled performance measurements. Cross-machine determinism,
audio/render-command replay and menu recording remain outside this increment.

## Added in the sixteenth increment

- Configurable, deterministic sprite/tile/entity/collision/mixer workloads with
  independent counts, capacity rejection, per-cycle warmup and repeated teardown.
- Visible `--stress` mode with a fixed offscreen resolution and a CPU-only
  `stress_bench` executable. Both install and need no authored asset files.
- JSON workload/build/hardware metadata, mean/median/p95/p99/max timings, async GPU
  timing status, workload counters, final checksum and bounded storage observations.
- Reserved CPU buffer and uploaded texture-byte accounting; stable loaded resources
  and return-to-baseline GPU teardown checks; RSS reported separately without a leak claim.
- Six CPU cases, structured report validation and two GPU cases. CPU allocation
  instrumentation verifies zero allocations during steady simulation/mixing and
  baseline live allocation blocks after eight arena lifetimes; GPU tests repeat twelve lifetimes.

## Sixteenth-increment validation (2026-10-08)

All twenty-four CTest groups pass in GCC debug/release, Clang debug and native
AddressSanitizer/UndefinedBehaviorSanitizer builds, without hardware skips. The
stress groups were repeated in debug/release after final JSON escaping and HUD
counter adjustments; Clang and sanitizer suites already include those adjustments.
Formatting, strict warnings and diff whitespace checks pass.

Installed release runs from `/tmp` exercise all maximum workload counts together:
65,536 sprites, 65,536 occupied tiles, 4,096 entities, 256 overlapping bodies and
16 mixer voices. Each measured pass reports exactly 114,688 quads, 16,384 culled
sprites, 28 batches and 32,640 contacts. Resources remain stable across two cycles.
The maximum-workload capture and a default-size smoke capture were inspected.
A native Escape event produces a nonzero exit, incomplete JSON report and clean
resource teardown. Invalid workload counts and conflicting gameplay options fail
before opening a display.

Installed `stress_bench` and `feature_lab --stress --cpu-only` run with DISPLAY
unset and produce matching checksums for identical workloads. A separate release
configuration with ENGINE_BUILD_PLATFORM=OFF builds and runs `stress_bench`
without platform targets. Visible validation does not open an audio device or
read/write game settings. Maximum-workload smoke runs are functional checks,
not performance baselines. See [benchmark contracts](../benchmarks/README.md).

Three release baselines were collected after builds/tests finished, using source
`86d5f8c`: combined 1920×1080, 256 overlapping collision bodies and 16 mixer voices.
Each completes 1,800 measured steps across three fresh cycles. The combined frame
median/p95/p99 is 3.786/8.875/17.981 ms; offscreen GPU median/p95/p99 is
1.513/1.692/1.741 ms from 1,799 samples, with one result still pending at shutdown.
The frame p99 exceeds the provisional 16.7 ms target. All runs retain stable
tracked resources; RSS is observed separately. Complete metadata and report files
are in [the benchmark reference measurements](../benchmarks/README.md#initial-reference-measurements-2026-10-08).

## Added in the seventeenth increment

- Native Release archive target containing both executables, all authored assets,
  usage/API/coverage/benchmark documentation, build provenance and direct/transitive
  build-host library requirements. System/driver libraries remain host-managed.
- Per-file SHA-256 manifest and archive checksum sidecar; package creation validates
  an extracted directory moved to a path containing spaces before publication.
- CPU smoke checks outside source/build trees: complete gameplay verification,
  standalone/game CPU benchmark equivalence, symlink launch, invalid CWD assets,
  missing-package-assets failure and explicit asset override.
- Manifest integrity cases plus Release packaging/relocation/OpenGL CTest fixtures.
  Visible smoke completes the game, saves/verifies a replay and exercises stress
  resource stability, using isolated user data and leaving the package unchanged.
- No engine runtime or persistence-format change. Relocation on compatible Linux
  hosts is distinct from bundling a cross-distribution runtime.

## Seventeenth-increment validation (2026-10-08)

GCC Release passes all 28 CTest groups, including the manifest cases and three
packaging fixtures, with no skips. A fresh out-of-source Clang Release build with
BUILD_TESTING=OFF also creates its archive and passes mandatory relocated CPU
checks. The separate packaged Clang OpenGL game/replay/stress smoke passes.
Debug packaging is rejected before staging. GNU sha256sum independently validates
the GCC archive sidecar.

Extracted packages run from paths containing spaces under /tmp, outside source
and build trees, with invalid current-directory assets and loader overrides
removed. The game collects all three cores, completes the exit, saves slot 1 and
records 1,600 replay commands; headless verification reaches tick 1,381. The
packaged stress run completes eight measured frames across two cycles with stable
resources. GCC game-completion and stress captures were visually inspected.
Payload manifests still match after execution. Corrupted archive bytes, changed
payloads and unlisted files are rejected.

These checks use the existing PC's XWayland/OpenGL driver and compatible system
libraries. They do not establish cross-distribution compatibility or audible
physical-device output. No engine runtime code changed in this increment.
See [packaging contracts](PACKAGING.md) for scratch directories and test commands.

## Added in the eighteenth increment

- Read-only local-pose sampling through a scene hierarchy, preserving full affine
  composition, shear and mirrored axes without per-sample allocation.
- Authored four-node transform station near spawn: rotating nonuniformly scaled
  arm, mirrored pulsing checker panel and inherited satellite. Motion derives from
  bounded simulation tick phase and interpolates locally before world composition.
- Pause/step/restart/save/replay integration with snapped restore/step poses and
  validation preventing gameplay/collision entities beneath animated decorations.
- Camera numeric-domain checks and safer aspect scaling; expanded camera corner,
  resize/aspect, map clamp, zoom and transform reference tests.
- GPU pixel checks for the authored hierarchy at multiple phases/zooms/aspects and
  a rotated sprite whose center is outside the view but whose geometry is visible.
- Updated asset generator and authored scene. Existing save/replay formats remain
  unchanged; old scene content is rejected by the established fingerprint check.

## Eighteenth-increment validation (2026-10-08)

GCC Debug passes all 26 CTest groups, Release all 29 (including extracted-package
CPU/OpenGL fixtures), and the CPU ASan/UBSan configuration all 16. The final
full-turn normalization change was then checked with the complete Release suite,
the Debug/Clang transform and render-target groups, and the sanitizer transform
group. There were no test skips or sanitizer findings.

The new CPU group contains seven cases covering hand-computed hierarchy points,
read-only sampling and invalid input, camera corners/aspects/clamping/zoom, inherited
orbit/scale/mirror/shear, full-turn continuity and exact restore equivalence,
pause/step/restart/large-tick restore, replay poses at three interpolation fractions,
and rejection of colliders/gameplay nodes under visual animation. Two added GPU
cases extend render_targets to 15 cases: the authored mirrored/sheared panel is
sampled at five tick checkpoints, two zoom levels and three viewport shapes; a
partially visible rotated hierarchy remains rendered while an offscreen one culls.

Native replay-driven captures at normal and close zoom were visually inspected on
the existing Intel/Mesa OpenGL 4.6 XWayland session. The station label was moved
clear of the rotating panel. The C++ asset generator reproduces the checked-in
scene exactly. Full scripted gameplay and packaged replay verification still
complete at tick 1,381. Captures/tests use isolated temporary user data and no
physical audio device. These are functional checks, not performance baselines or
a broader manual display/focus test matrix.

## Added in the nineteenth increment

- Per-upload sampling descriptors and between-pass updates/queries. Minification
  and magnification select nearest/linear independently; U/V select clamp/repeat.
  Defaults and built-in/render-target sampling remain nearest/clamp.
- Bounded negative/repeating sprite UVs, existing flips/order/batching preserved,
  and sampling state retained by atomic texture replacement and reset on slot reuse.
- Validated texel-center atlas rectangles for base-level linear filtering, including
  one-texel cells; this does not add mipmaps or atlas-cell wrapping.
- Four procedural comparison panels near spawn showing all filter/wrap combinations
  without per-frame uploads/state changes. Renderer-owned textures live for the
  session; partial initialization cleans up if capacity is exhausted.
- CPU descriptor/atlas tests and native OpenGL sampling/reload/lifetime tests.
  No asset content, simulation, save/replay or shader-interface changes.

## Nineteenth-increment validation (2026-10-11)

GCC Release passes all 30 CTest groups, including the extracted-package CPU/OpenGL
fixtures. GCC Debug passes all 27 groups. The focused tile_animation and
texture_sampling groups also pass with Clang and with native-platform ASan/UBSan;
no sanitizer findings or skipped tests were reported.

Seven new GPU cases check independent minification/magnification, sRGB interpolation
and alpha, clamp/repeat on both axes (including negative/flipped UVs and seams),
atlas neighbor bleeding and inset recovery, GL binding preservation, rejected
state changes without disturbing pending batches, painter order, descriptor/pixel
preservation through failed and resized reloads, stale/recycled handles, and
partial gallery initialization cleanup. The CPU atlas suite adds descriptor/grid/
dimension validation and exact one-texel inset bounds.

The gallery's rendered pixels distinguish the configured modes. Repeated draws
retain four textures/64 payload bytes and use exactly five batches when fully
visible (four images, then all labels). Native captures at normal and close zoom
were inspected; the layer-5 gallery leaves the player visible when positioned over
a panel. Scripted gameplay, replay verification and package relocation still pass.
No physical audio or broader hardware/display matrix was exercised, and these
functional checks do not constitute new performance baselines.

## Added in the twentieth increment

- Ten configurable actions now accept letters or left/middle/right/side mouse
  buttons. Assignments are unique and transactional, with readable menu/HUD hints.
  An action has one configurable input plus any existing fixed keyboard aliases.
- Native keyboard/mouse aggregation preserves held aliases, fast-click edges and
  first-press capture order. Rebinding and mapping changes suppress held inputs
  until release; focus loss cancels pending input and menu capture.
- F1 Controls accepts mouse capture on the frame after activation. Left-click
  remains available for UI, and gameplay stays captured while settings are open.
- ECFG v3 persists stable engine mouse symbols; v1/v2 migrate while preserving
  existing audio/VSync and supported letter assignments. Save/replay formats and
  asset content are unchanged. No new dependencies.

## Twentieth-increment validation (2026-10-11)

GCC Debug and Release each pass all 27 runtime CTest groups without skips.
Focused bindings, persistence, UI and native platform-event groups also pass with
Clang and native-platform ASan/UBSan (driver leak detection disabled).

The eight-case bindings suite covers supported mouse symbols, transactional
conflicts, raw capture ordering, v1/v2 migration, v3 round trips, malformed records,
and modal held-button suppression. Native X11 events exercise all five mouse
buttons, keyboard/mouse aliases in both release orders, repeated events, fast
clicks, held remapping, mapping notifications, focus recovery, and ignored wheel/
unsupported capture inputs.

Two native Feature Lab launches used isolated settings: the first assigned RMB to
Interact, rejected a duplicate RMB assignment, assigned LMB to Move Left, closed
settings to save, and reopened Controls. The second restored both assignments
from the same 72-byte ECFG v3 file without editing. Both captures were inspected;
labels fit the controls panel and the HUD reflects LMB/RMB. Physical side-button
hardware was not exercised; side buttons were covered with native event injection.
No new performance baseline or physical audio test was taken.

## Added in the twenty-first increment

- Configurable 0–9, nine punctuation keys, Backspace, Insert/Delete, Home/End,
  and F4/F9/F12, in addition to existing letters and five mouse buttons.
  Named ASCII labels fit the existing controls panel and gameplay hints.
- One validated symbol allowlist drives assignment and native keyboard mapping.
  Reserved controls remain available. Unshifted group-zero semantics are retained,
  including Shift+1 activating a 1 binding. No modifier chords are introduced.
- ECFG v4 retains the 72-byte format and migrates v1/v2/v3 preferences, preserving
  their validation rules. Saves, replay action bits and asset content are unchanged.
- CPU cases cover all named labels, conflicts, reserved/unsupported rejection,
  legacy migration and settings capture. Native events exercise all 27 new symbols,
  repeat suppression, fixed aliases, held rebinding and fast-tap edges.

## Twenty-first-increment validation (2026-10-11)

GCC Debug and Release each pass all 27 runtime groups without skips. Focused
bindings, persistence, UI and native platform-event groups also pass with Clang
and native-platform ASan/UBSan
(driver leak detection disabled). The bindings suite now has eleven CPU cases.
Native tests exercise all 27 added keyboard symbols using the installed keymap,
including Shift-state capture, repeat, aliases, held rebinding and fast taps.

Two isolated Feature Lab launches verified capture and persistence of Move Left=1,
Interact=Home, Music Volume=Slash and Effects Volume=F12. Reserved Tab and duplicate
Home inputs were rejected during capture. Closing settings wrote a 72-byte ECFG v4
file; relaunch restored all four bindings. Both 1050x1360 captures were inspected:
controls and HUD labels fit, and the second launch remained paused at zero ticks.
These checks use injected X11 events; other keyboard layouts, physical audio and
new performance baselines were not exercised.

## Added in the twenty-second increment

- Ctrl/Shift/Alt chords for supported keyboard and mouse inputs, with atomic
  symbol/mask capture. Modifier-only presses keep capture open; labels and the
  Controls legend explain C+/S+/A+. Button text scales to fit longer bindings.
- Transactional duplicate/ambiguity validation: shared-base modifier sets must be
  nested. Fresh triggers choose the most specific match, with plain bindings as
  fallback. Modifier changes never switch an action during a held trigger.
- Fixed-array native modifier tracking and per-key/button action selection retain
  alias aggregation, repeat suppression, focus cancellation and held-rebind gating.
  Fixed menu/diagnostic controls remain available with modifiers.
- ECFG v5 appends ten modifier masks (112 bytes total). v1–v4 load their existing
  bindings with zero masks. Save/replay formats, asset content and dependencies
  are unchanged.
- CPU chord/migration/capture cases and a dedicated six-case native chord suite.

## Twenty-second-increment validation (2026-10-11)

GCC Debug and Release each pass all 28 runtime CTest groups without skips. Focused
bindings, persistence, UI, platform_events and platform_chords groups also pass
with Clang and native-platform ASan/UBSan (driver leak detection disabled).

Fourteen CPU binding cases cover specificity, nested-mask conflict rejection,
transactional edits, paired capture/reset, v1–v4 migration and malformed v5 masks.
Six native chord cases cover all eight masks, left/right Ctrl/Shift, press-time
selection, base-first rejection, modifier release order, repeat, mouse/fixed-key
aliases, held remapping, focus recovery and mapping cancellation. Existing native
keyboard/mouse and replay tests still pass.

Two isolated Feature Lab launches captured and restored Ctrl+E, Ctrl+Shift+E,
Alt+RMB and Ctrl+Shift+Alt+Slash. An ambiguous Shift+E edit was rejected before
Ctrl+Shift+E was accepted. The saved ECFG v5 file is 112 bytes with the expected
symbols/masks. Both 1050x1360 captures were inspected: long button labels fit and
HUD hints show chords; relaunch retained all assignments without another edit.
The checks used injected X11 events, not a broader physical keyboard/layout matrix.
No new performance baseline or physical audio test was taken.

## Added in the twenty-third increment

- Sixteen canonical keypad bindings: digits 0–9, decimal/separator, add, subtract,
  multiply, divide and equal. These are distinct from main-row keys and support
  existing Ctrl/Shift/Alt chords, readable labels and conflict rules.
- Keypad navigation aliases normalize before capture and lookup. Num Lock and
  Caps Lock do not change the binding, including during a held key. Locale
  separator shares the decimal identity. Num Lock never occupies capture, and
  keypad Enter remains fixed menu activation.
- ECFG v6 retains the 112-byte schema, accepts canonical keypad symbols and loads
  v1–v5 preferences under their original validation rules. Save/replay formats,
  asset content and dependencies are unchanged.
- CPU normalization, distinct-key, label, migration and menu cases; four new native
  cases exercise keypad lock states, aliases, chords, remapping and focus recovery.

## Twenty-third-increment validation (2026-10-11)

GCC Debug and Release each pass all 28 runtime CTest groups without skips. Focused
bindings, persistence, UI, platform_events and platform_chords groups also pass
with Clang and native-platform ASan/UBSan (driver leak detection disabled).

The binding suite now has sixteen CPU cases; the native chord/keypad suite has
ten. Native cases exercise all 16 canonical keypad symbols and 11 navigation
aliases under injected Num Lock/Caps Lock/Shift state masks, including lock-state
changes during repeat/release. Separate physical modifier events test actual chord
selection. Main-row separation, fixed navigation/Enter aliases, capture, held
rebinding and focus recovery pass. CPU cases cover locale separator normalization;
no physical locale-specific keypad or global desktop lock state was changed.

Two isolated Feature Lab launches captured and restored main-row 1, KP1, Ctrl+KP1,
Ctrl+Shift+Alt+KPDOT and KPMUL. An injected Num Lock press left capture open. The
112-byte ECFG v6 file retained the expected canonical symbols and masks across
relaunch. Both 1050x1360 captures were inspected: labels fit and the HUD distinguishes
main-row/keypad input. No new performance baseline or physical audio test was taken.

## Added in the twenty-fourth increment

- Active XKB keyboard groups, selected from state notifications and each key
  event's lookup state. Startup/focus-in synchronization and map/new-keyboard
  refresh keep cached mappings current. No additional libraries or config changes.
- Four bounded level-zero caches follow per-key wrap/clamp/redirect rules, ASCII
  case normalization and keypad canonicalization. Unsupported international
  symbols remain unsupported instead of becoming group-zero actions.
- Layout changes clear queued input and cancel binding capture; held keys,
  modifiers and mouse buttons remain suppressed until release. Same-group input
  does not cancel actions. ECFG v6 and save/replay formats are unchanged.
- Four in-memory map cases and four native group cases, including Feature Lab
  Controls capture and recovery. README includes a multi-layout gameplay procedure.

## Twenty-fourth-increment validation (2026-10-12)

GCC Debug and Release each pass all 29 runtime CTest groups without skips. Six
focused groups (keyboard_maps, platform_chords, platform_events, bindings,
persistence and UI) also pass with Clang and native-platform ASan/UBSan (driver
leak detection disabled). The map suite has four cases and the expanded native
chord/keypad/group suite has fourteen.

Two isolated, visible Feature Lab launches exercise injected group changes.
The first cancels an active capture, displays BINDING CANCELED, retains A and
creates no preferences file. The second captures Q after the transition, updates
the movement hint and saves the expected 112-byte ECFG v6 file. Both 1050x1360
captures were inspected. Existing GPU/menu, replay and persistence tests pass.

Distinct layout symbols and range policies are tested in memory; native tests
inject core-event group bits and focus/mapping events into owned windows. No
global keyboard mapping or desktop group was changed. Physical layout-switch
shortcuts, asynchronous server group notifications and keyboard replacement still
need a broader manual multi-layout/compositor matrix. No new performance baseline
or physical audio test was taken.

## Added in the twenty-fifth increment

- International Unicode 16 character bindings, including exact legacy X11 aliases
  normalized to canonical Latin-1/Unicode identities. ASCII is lowercase-normalized;
  non-ASCII case is preserved. Additional level-zero punctuation supports layouts
  with punctuation on number-row keys. Fixed controls and keypad identities remain.
- Short accented-letter names and bounded U+XXXX labels use the existing bitmap
  font and work with Ctrl/Shift/Alt chords. Dead keys/composition, combining marks,
  controls, whitespace and private/unassigned characters remain outside support.
- ECFG v7 keeps the 112-byte schema and reads v1–v6 under their original validation
  rules. Save/replay formats are unchanged. Character tables are checked in;
  optional in-house regeneration uses installed X11 protocol data and Python's
  standard Unicode data. No new runtime libraries or build downloads.
- Five CPU cases, three native international cases and GPU long-label coverage.
  Native tests use a client-only map fixture with real queued window events;
  Feature Lab capture, labels, settings and held-input behavior are exercised.

## Twenty-fifth-increment validation (2026-10-12)

GCC Debug and Release each pass all 31 runtime CTest groups without skips. Eight
focused groups (international_bindings, bindings, persistence, keyboard_maps,
platform_international, platform_chords, platform_events and render_targets) also
pass with Clang and native-platform ASan/UBSan (driver leak detection disabled).
The optional generator's --check reproduces the checked-in character tables.

The five new CPU cases cover representative legacy aliases across Latin, Greek,
Cyrillic, Arabic, Hebrew, Kana and Thai, direct Unicode/CJK/supplementary characters,
labels, conflicts, punctuation, excluded character categories and strict v1–v7
migration. Three native cases inject four groups into client-owned map snapshots,
then send events to owned windows to verify capture, chords, group cancellation,
settings labels/round trips and held rebinds. They do not change the server keymap.

Two visible Feature Lab launches loaded the same isolated ECFG v7 fixture with
EACUTE, U+03B1 and U+20000 modifier chords, plus CCEDIL. Both 1050x1360 captures
were inspected: Controls labels fit and HUD hints remain legible; the preferences
file remained unchanged across launches. GPU tests cover long labels at 320×240,
640×480 and 1280×720. Physical international keyboards, layout-switch shortcuts
and compositor behavior still need a broader manual matrix. No new performance
baseline or physical audio test was taken.

## Added in the twenty-sixth increment

- Authored Annex scene/map, explicit validated player spawn and a second complete
  collect/exit route. Interact advances between completed levels; Restart stays in
  the current level, and New Game returns to the startup level.
- Staged CPU worlds, resident texture/atlas preflight, staged minimap geometry and
  a scene-first commit. Missing/malformed content, invalid spawns, preparation
  failure and active scene iteration leave the live world intact.
- Cross-level title/gameplay save loading and replay startup resolve existing
  content fingerprints. ESAV v3, ERPL v1 and ECFG v7 are unchanged. Original
  Facility scene/map/animation bytes remain identical. Recordings stay single-level.
- Held-input suppression after advancement, fresh level clocks/interpolation,
  refreshed collision/minimap state and soundtrack voices/emitter placement.
- Six CPU level cases, an application-level two-level tour and GPU resource/cache
  checks. `--level annex`, `--level annex --verify` and `--level-tour` expose the
  scenarios in the sample game.

## Twenty-sixth-increment validation (2026-10-13)

GCC Debug and Release each pass all 33 runtime CTest groups without skips. Eight
focused groups (levels, scene_flow, persistence, replay_files, render_targets,
audio_transport, transforms and collision) also pass with Clang and native-platform
ASan/UBSan (driver leak detection disabled). The sanitizer build command was
terminated after linking; a build recheck found no remaining work and the separate
test run passed. The Annex CPU verification completes with three cores in 702 ticks.

The native application tour completes Facility and Annex in 2084 rendered frames.
A visible tour saves the completed Annex to slot 2, and a fresh default-Facility
launch resolves that save back to Annex. The 1050x1360 Annex spawn and restored
completion captures were inspected, including the level label, minimap and next/
restart hint. Eight repeated GPU replacements keep texture and target counts
stable. Original Facility scene/map/animation bytes match regenerated output.
Relocated-package checks now verify both authored routes. No new performance
baseline or physical audio test was taken.

## Added in the twenty-seventh increment

- Scene-owned typed component pools with dense entries and sparse checked lookup,
  at most 64 types and 4096 entries per type. Move-only resources and over-aligned
  components are supported; nonthrowing moves/destruction make compaction safe.
- Const/mutable access and intersecting queries, shared nested iteration guards,
  deferred subtree cleanup, constructor-failure rollback and scene move/replacement
  lifecycle handling. Pool buffers join existing scene memory accounting.
- CorePickup components drive both levels' collectible detection and disappear on
  entity deletion. Restart and save/load reconstruct them from authored tags and
  collected IDs. F2 and session logs expose live pickup counts.
- Ten component cases, a seeded reference model, allocation/lifetime stress checks,
  and a seventh level case for pickup persistence and transitions. The native tour
  additionally requires an empty pickup pool at Annex completion.
- Runtime pools are intentionally absent from scene text; custom persistence codecs
  are not included. Existing authored content, save/replay/config formats and minimal
  Linux system-library requirements remain unchanged.

## Twenty-seventh-increment validation (2026-10-13)

GCC Debug and Release each pass all 34 runtime CTest groups without skips. Ten
focused groups (components, content, stress_cpu, levels, transforms, scene_flow,
persistence, replay_files, render_targets and audio_transport) also pass with Clang
and native-platform ASan/UBSan (driver leak detection disabled).

The component suite covers 2500 seeded model operations, full entity/type capacity,
owned resource teardown, over-aligned storage, nested guards and transactional
construction/moves. The stress check performs 120 query/remove/reinsert rounds over
256 entities with no C++ allocations after setup, stable tracked buffers and no
remaining allocation blocks after teardown. No new timing baseline was taken.

Both authored routes complete through the native loop in 2084 frames; the final
session reports zero pickup components. Inspected 1050x669 diagnostics captures show
three live pickups at Annex startup and zero at completion, with the added row
inside its panel. A 1500-command recording created by the previous packaged release
(0bfbc3f) verifies with the new executable at tick 1381 with the same state hash.
Annex CPU verification still completes in 702 ticks. No physical audio test was taken.

## Added in the twenty-eighth increment

- `AssetWatcher` registers up to 128 keys and 32 notification groups, reads at most
  16 KiB of inotify events per poll and debounces each group for 150 ms. A one-second
  bounded metadata scan recovers missed events and directory/root replacement.
- Explicit initial synchronization and rescan, saturation/lost-watch recovery,
  missing-directory fallback and reattachment, canonical asset-root containment,
  and descriptor cleanup. No new runtime libraries or worker threads.
- Feature Lab `--watch-assets` calls existing shader/texture transactions between
  render passes, including title/settings/pause/replay frames. Watching is off by
  default. Manual commands take priority over a ready automatic group in that frame.
- Nine filesystem cases and two GPU cases cover editor-style atomic saves, debounce,
  deletion/recreation, root replacement, symlink metadata changes, bounded event
  recovery, pixel changes, invalid-edit retention, recovery and stable resources.
- No scene/audio/animation/font reload or asynchronous compilation/loading. Existing
  content, save/replay/config formats and authored levels remain unchanged.

## Twenty-eighth-increment validation (2026-10-13)

GCC Debug and Release each pass all 36 runtime CTest groups without skips. Eight
focused groups (asset_watch, watched_reload, shaders, texture_reload, content,
levels, replay_files and render_targets) also pass with Clang and native-platform
ASan/UBSan (driver leak detection disabled). This session initially lacked DISPLAY;
the existing :0 XWayland display was verified and used for all final hardware runs
on Intel HD Graphics 4400 / OpenGL 4.6.

The nine filesystem cases use private temporary files and simulated time. They
exercise atomic rename saves, independent group deadlines, missing files/directories,
directory/root replacement, symlink metadata recovery, saturated read-budget recovery
and descriptor cleanup. GPU checks compare changed shader/texture pixels, preserve
working GPU and CPU snapshots after invalid edits, recover on subsequent edits and
retain stable texture/target counts. Manual reload priority is also covered.

A 300-frame native run against a private asset copy observed initial synchronization,
invalid shader and texture edits, and successful repair: three automatic attempts
per group, final shader revision 4 and texture revision 3. The 1050x1360 recovered
capture was inspected. The original 3000-frame capture run completed its edit checks
but exceeded the harness's 45-second exit timeout; the shortened run exited normally.
Separate title and paused-settings launches reload both groups while simulation stays
at tick zero. A 1500-command replay with watching enabled retains state hash
3859428580428616961 at tick 1381. No new timing baseline or physical audio test was taken.

## Added in the twenty-ninth increment

- EFNT v1 packs a validated bitmap atlas and ASCII glyph metrics; bounded readers,
  writers, signed offsets, variable advances, line spacing and visible fallback
  validation are implemented without new dependencies.
- Immutable `BitmapFont` snapshots, constant-time byte lookup, allocation-free
  measurement/layout and a readable embedded fallback for missing/corrupt assets.
- Renderer text uses one quad per nonempty glyph. Font selection copies/uploads
  transactionally between passes; font atlas bytes have separate accounting.
- Feature Lab loads the checked-in proportional `fonts/ui.efnt` for menus/HUD and
  offers `--font-demo` for metrics, line spacing, unknown characters and clipping.
  The existing shader/texture watcher does not yet watch font assets.
- Tests cover serialization/truncation/metrics/fallback, allocation stability, GPU
  pixels/orientation/alpha/clipping, replacement lifetime and text input limits.

## Twenty-ninth-increment validation (2026-10-14)

GCC Debug and Release each pass all 38 runtime CTest groups without skips. Four
focused groups (fonts, font_graphics, stress_cpu and render_targets) pass under
Clang and native ASan/UBSan with driver leak detection disabled. The three packaging
checks pass: build, relocation and native
OpenGL gameplay/stress smoke. Hardware checks use the existing :0 XWayland display
and Intel HD Graphics 4400 / OpenGL 4.6.

Six CPU font cases exercise bounded parsing, every truncation of a small fixture,
round trips, invalid metrics, fallback visibility, signed/unknown input bytes,
multiline spacing, ownership and filesystem fallback. Five GPU cases verify exact
colored/transparent atlas pixels, offsets, fallback and line spacing, clipping,
premultiplied offscreen composition, drawing order, one-quad-per-glyph batching,
replacement lifetime, invalid-call boundaries, the gallery at two viewport sizes
and the original embedded glyph shapes. An allocation-counted stress case performs
1000 measurement/layout iterations without heap allocations and returns to baseline.

The in-house generator reproduces the checked-in font. Release native captures of
the proportional font gallery, corrupt-font fallback gallery, title and paused
settings were inspected at 1050x1360. Missing and corrupt private font copies log
source-specific errors and keep text readable. Initial two-frame captures occurred
during compositor resizing; final visual checks used 120-frame runs. Full gameplay,
level tours and replay tests still pass. No new performance baseline or physical
audio validation is claimed. Font watching and general Unicode text remain deferred.

## Added in the thirtieth increment

- `FontAssets` uses strict bounded EFNT loading and the renderer's existing atomic
  font replacement. Revisions and diagnostics distinguish the retained embedded
  startup font from a successfully loaded file; failed edits never reset a working
  file font to the embedded fallback.
- `--watch-assets` registers `fonts/ui.efnt` as an independent third notification
  group. Existing debounce, initial synchronization and directory recovery apply.
  Metrics, atlas pixels and byte accounting change together between render passes.
- Font HUD status and session counters expose successful/failed attempts and source
  provenance. No new controls, dependencies, file formats or replay commands.
- Four GPU integration cases cover direct and watched replacement, invalid metrics,
  deletion/recreation, missing-directory recovery, symlink escape rejection,
  independent groups, manual priority and repeated resource stability.

## Thirtieth-increment validation (2026-10-14)

GCC Debug and Release each pass all 39 runtime CTest groups without skips. Six focused groups
(font_reload, watched_reload, asset_watch, fonts, font_graphics and render_targets)
pass with Clang and native ASan/UBSan (driver leak detection disabled). Three
packaging checks pass: archive build, relocation and native OpenGL smoke. Tests use
Intel HD Graphics 4400 / OpenGL 4.6 through the existing :0 XWayland display.
The initial font-reload fixture incorrectly expected a texture notification without
registering a texture; it now includes a resident texture and checks all three groups.

A 360-frame native run against a private asset copy applied changed glyph color and
advance metrics, retained revision 4 after a malformed EFNT edit, and recovered to
revision 5 after repair. Four automatic font attempts (including synchronization)
were observed; shader and texture counts stayed at one each. The recovered 1050x1360
font-gallery capture was inspected. Separate title and paused-settings runs each
reload the font while simulation remains at tick zero. A 1500-command watched replay
with the modified font still ends at tick 1381 and hash 3859428580428616961.
No new performance baseline or physical audio validation was taken.

## Added in the thirty-first increment

- Explicit fullscreen requests and observed fullscreen/focus state, cached protocol
  atoms, validated close messages, and safe presentation while unmapped/closed.
- Temporary X11 focus grabs preserve logical focus; actual focus loss/unmap clears
  held input and binding associations without generating a pointer click.
- Feature Lab now suspends ordinary gameplay as well as replay/scripted modes when
  unmapped, resets hidden elapsed time, and reports suspension/resume ticks.
- Six lifecycle integration cases and an opt-in `platform_probe` separate engine
  checks from WM acceptance and physical display-scale evidence. No new dependencies.
- [Platform validation matrix](PLATFORM_VALIDATION.md) records confirmed fullscreen,
  visible focus, client unmap/restore, and unresolved WM minimize/physical scaling.

## Thirty-first-increment validation (2026-10-14)

All 40 runtime CTest groups pass with GCC Debug and Release, without skips. Six
focused platform/graphics groups pass with Clang and native ASan/UBSan (driver leak
detection disabled). Three Release packaging checks pass: archive build, relocation
and native OpenGL smoke. The lifecycle group also passes ten consecutive Release runs.
The initial resize fixture assumed immediate agreement between X11 events and GLX
buffers and retained startup cancellation before the injected click. The corrected
fixture waits for both observed dimensions to converge, synchronizes queued events,
and consumes startup input before testing the click. No production busy-wait or
extra per-frame synchronization was introduced.

The opt-in managed-window probe confirms fullscreen entry/exit, real focus transfer,
client unmap/restore and X11/GLX pixel agreement. It exits 2 because the WM minimize
request was unconfirmed, not because an engine check failed. Fullscreen was
2160x1440; restored/tiled size was 1050x1360. VSync requests were available, with no
physical timing or tearing measurement. The final probe capture was inspected.

A native 240-frame watched play run suspended/resumed at tick 30 across a 0.5-second
unmap. A watched 1500-command replay suspended/resumed at tick 21 and retained its
reference final tick 1381 and hash 3859428580428616961. Both restored game captures
were inspected. No physical monitor-scale, audible-output or performance-baseline
claim is made. F01 remains partial for the outstanding matrix entries.

## Added in the thirty-second increment

- Worker-owned block submission preserves pending samples across short writes,
  wait interruptions, underruns and suspended-device recovery. Wait errors now
  receive the same handling as write errors. Resume retries use bounded backoff
  and preserve shutdown responsiveness; unsupported resume falls back to prepare.
- Audio statistics report accepted PCM frames, underruns, suspensions, successful
  recoveries, waits and a terminal error code. Feature Lab retains diagnostics
  after disabling failed output and continues silent gameplay.
- Seven CPU recovery cases cover exact sample delivery, retry bounds, failed
  prepare and terminal errors. Two additional real-worker tests check null-device
  progress and `/dev/full` write failure with preserved soundtrack diagnostics.
- `audio_probe` still defaults to a silent check and now requires PCM progress.
  Opt-in `--exercise` provides left/right/center, mute, group pause/resume and stop
  phases. No system volume/routing configuration or dependencies were added.
- [Audio validation](AUDIO_VALIDATION.md) separates software and PCM acceptance
  from physical audibility, suspend/resume and unplug/reconnect evidence.

The full Release suite also exposed an intermittent mapped resize fixture from
increment 31. Its exact-size checks now use an unmapped drawable, synchronize X11
before GL and retire the hidden back buffer explicitly. Ten repeated lifecycle
runs pass; mapped unmap/restore and visible-probe coverage remain separate. No
production window behavior changed.

## Thirty-second-increment validation (2026-10-15)

All 41 runtime CTest groups pass with GCC Debug and Release, without skips,
including the corrected lifecycle fixture. Four focused groups (audio_recovery,
audio_transport, sound and core) pass with Clang and native ASan/UBSan (leak
detection disabled). The recovery group also passes the CPU-only sanitizer preset
with no ALSA/platform dependency. Three Release packaging checks pass: archive
build, relocation and native OpenGL smoke. No new performance baseline was taken.

The silent default-device probe accepted 6144 frames with no device errors or
recoveries. The eight-phase exercise on ALSA null completed with one started/stopped
voice, no active voices at exit and no rejected/stale commands. These are transport
and control checks, not proof of speaker output. The real ALSA `file:/dev/full`
probe latched error -5 after 1792 accepted frames and exited 1 as expected.

A native 1500-command replay with normal default output ended at tick 1381 and hash
3859428580428616961, with eight voice starts and no rejected commands or device
errors. A separate replay used a process-local ALSA configuration routing output to
`/dev/full`: it disabled audio, kept the same final tick/hash, and retained error -5
and 1792 accepted frames in the exit summary. The completed game capture with
`AUDIO OFF` was inspected. The failure configuration was verified with the probe
first; no installed system configuration was changed. Physical listening, real
suspend and real disconnect/reconnection checks remain outstanding; F10 remains partial.

## Added in the thirty-third increment

- `AnimationPlayer::replace` stages a validated snapshot and maps normalized tick
  progress by clip name, preserving pause and completed state without emitting events.
- Feature Lab stages all four animation players together and rejects incompatible
  required clips, loop modes or resident texture/atlas changes. Bad file edits
  retain working animation; ready/failed revision status appears in the HUD/logs.
- Optional `--watch-assets` adds `facility.eani` as an independent group. Animation
  watching is omitted during recording/playback; those sessions keep fixed content.
  Title/settings/paused frames can reload; unmapped frames defer all watching.
- Save fingerprints already include EANI; reload changes content identity and does
  not migrate old saves. Game/level/save replacement resets reload status for the
  newly installed snapshot. No new formats, hotkeys or external dependencies.
- Seven CPU cases and two GPU/watch cases cover phase/completion, whole-bank
  validation, old snapshot lifetime, save compatibility, debounce, changed pixels,
  failure/repair, group isolation and stable renderer resource counts.

## Thirty-third-increment validation (2026-10-15)

All 43 runtime CTest groups pass with GCC Debug and Release, without skips.
Seven focused groups (animation_reload, animation_watch, tile_animation,
replay_files, levels, font_reload and watched_reload) pass with Clang and native
ASan/UBSan with leak detection disabled. The new animation_reload group also passes
the CPU-only sanitizer preset. Three Release packaging checks pass: archive build,
relocation and native OpenGL smoke. No new performance baseline was taken.

Native 240-frame watched runs in gameplay, title and settings each applied a valid
edit, retained the bank after malformed EANI and accepted a repaired file. Each
ended at revision 4 with four attempts (including initial synchronization); title
and settings remained at tick 0. A 1500-command replay ignored a later malformed
EANI edit and retained final tick 1381 and hash 3859428580428616961. Recording also
ignored the edit; its 250-command output verified against the original assets at
tick 250 with hash 13207249788274756958. Both fixed-content sessions reported zero
animation reload attempts. Gameplay and replay captures were inspected, including
the ready/locked animation HUD. Physical audio/display validation remains pending.

## Added in the thirty-fourth increment

- Transactional map reload stages tile chunks/solids, movement scratch, static query
  bodies and a primed collision world before publication. The minimap stages through
  the same transaction and retains its target and scroll state.
- Player and restart-spawn occupancy, grid/atlas compatibility and collision body
  capacity are checked before commit. Bad edits preserve working content. Cells,
  palette entries/solidity and layers can change; grid geometry stays fixed.
- Reload preserves entity handles and gameplay progress, clears transient old
  collision events and snaps player interpolation. Active alarm contact history is
  primed without an extra entry. Save identities reflect the edited ETMP.
- Facility/Annex maps have independent watched groups. Only active-level edits
  attempt reload; recording/playback omit both files. HUD/logs report status and
  revision; whole game/level/save replacement resets status for its new content.
- Seven CPU and three GPU/watch cases cover collision insertion/removal, map/minimap
  pixels, staging failure, player/spawn/capacity rejection, save compatibility,
  preserved progress, both levels, debounce and repeated resource stability.

## Thirty-fourth-increment validation (2026-10-15)

All 45 runtime CTest groups pass with GCC Debug and Release, without skips.
Nine focused groups (tilemap_reload, tilemap_watch, animation_reload,
animation_watch, tile_animation, collision, levels, replay_files and render_targets)
pass with Clang and native ASan/UBSan, with leak detection disabled. The new CPU
reload group also passes the CPU-only sanitizer preset. Three Release packaging
checks pass: archive build, relocation and native OpenGL smoke. No new performance
baseline was taken.

Native 240-frame watched runs in Facility, Annex, title and settings each applied a
valid tile edit, rejected an edit covering the restart spawn, retained the map after
malformed ETMP and accepted a repaired file. All four ended at revision 4 with five
attempts including initial synchronization; title/settings remained at tick 0.
A 1500-command replay ignored a later malformed map edit and retained tick 1381
and hash 3859428580428616961. A recording likewise ignored map edits; its 256-command
output verified against the original assets at tick 256 with hash 362213885974637685.
Both fixed-content sessions reported zero tilemap reload attempts. Gameplay and
replay captures were inspected, including the ready/locked tilemap HUD. Existing
physical audio/display validation gaps remain unchanged.

## Added in the thirty-fifth increment

- Scene reload stages a complete world using current map/animation content, restores
  progress by persistent ID, rebuilds pickup components and collision contacts, and
  validates resident textures/minimap walls before publication.
- Success invalidates old entity handles; failure retains them. Player/exit IDs and
  core ID set remain fixed. Player/core leaves and static-collider restrictions
  prevent collected or moving subtrees from invalidating runtime bindings.
- Position, pause, time, RNG, zoom, collection, alarm and door/animation state survive.
  Both current occupancy and authored spawn are validated; inconsistent exit/win
  edits are rejected. Plain restart uses the last accepted scene and its spawn.
- Separate Facility/Annex scene groups follow the active level and are omitted for
  recording/playback. HUD/logs expose revision, outcome and lock. Minimap target and
  scroll survive; machine sound position updates without restarting voices.
- Seven CPU and three GPU/watch cases cover handles, persistent collection,
  movement/query collision, minimap/sprite pixels, invalid edits and dependencies,
  save identity, alarm priming, door completion, both levels and resource stability.

## Thirty-fifth-increment validation (2026-10-16)

All 47 runtime CTest groups pass with GCC Debug and Release, without skips. Nine focused groups
(scene_reload, scene_watch, tilemap_reload, animation_reload, scene_flow, levels,
replay_files, render_targets and audio_transport) pass with Clang and native
ASan/UBSan (driver leak detection disabled). The new scene_reload group also passes
the CPU-only sanitizer preset with leak detection enabled. Three Release packaging
checks pass: archive build, relocation and native OpenGL smoke.

Native Debug 240-frame watched runs in Facility, Annex, title and settings each
accepted an added sprite/collider, rejected a wall over the restart spawn, retained
content after malformed scene text and recovered after repair. They ended at
revision 4 after five attempts including initial synchronization. Title/settings
stayed at tick 0. Gameplay and replay captures were inspected at 1050x1360.
A 1500-command replay ignored scene edits and retained tick 1381 and reference hash
3859428580428616961. Recording ignored the edits too; its 255-command output verified
at tick 255 with hash 16886348535325130272. Both reported zero scene reload attempts.
No new performance baseline or physical audio/display validation is claimed.

## Added in the thirty-sixth increment

- Atomic audio-bank replacement validates and stages owned samples, then enqueues one
  block-boundary transaction. Stable sound/voice handles, controls and normalized
  playback position survive. Completed/stopped voices are never resurrected.
- Producer-owned retirement batches keep samples/mapping storage alive until the
  worker acknowledges consumption. Collection and destruction stay off the audio
  worker. Current, retired and candidate payloads share the 64 MiB bank budget.
- Feature Lab watches all four WAV files as one group, including during replay.
  Invalid edits or queue rejection retain working audio; device failure retains the
  existing silent fallback. HUD/logs show accepted revision and retained bytes.
- Five CPU mixer, five output/controller and two watcher cases cover exact samples,
  queue ordering, controls/completion, stalled-worker ownership, bounded memory,
  invalid edits/repair, stable voices and recorded gameplay equality.

## Thirty-sixth-increment validation (2026-10-16)

All 50 Debug and Release runtime groups pass without skips. Eight focused groups (mixer_reload,
audio_reload, audio_watch, sound, audio_transport, audio_recovery, replay_files and
watched_reload) pass with Clang and native ASan/UBSan (driver leak detection disabled).
The CPU-only mixer_reload/sound groups and native audio_reload ownership group pass
with leak detection enabled. Three Release packaging checks pass: archive build,
relocation and native OpenGL smoke. Native Debug runs using a private
ALSA null default applied valid WAV edits, retained the bank after malformed input
and recovered after repair in Facility, Annex, title, settings, recording and a
1500-command replay. Each enabled run reported four attempts and revision 4;
resident/retained bytes both settled at 299648, with no queue rejections. Gameplay
and replay captures were inspected at 1050x1360. Title and
settings remained at tick 0. Disabled audio omitted the group entirely.

Replay retained tick 1381 and hash 3859428580428616961. The 326-command recording
verified at tick 326 with hash 10563975167933789901 against the original assets.
These runs overlapped compilation and are functional evidence, not performance
measurements (some catch-up time was dropped). Physical listening, disconnect/
reconnect and system suspend remain outstanding; no crossfade or waveform-continuity
guarantee is claimed.

## Added in the thirty-seventh increment

- Stress reports separate preview/HUD submission from window buffer swapping, with
  zero samples for both in CPU mode. Schema v1 and the generated workload remain
  compatible; the timing definitions now explicitly exclude per-frame validation.
- Committed-source 1080p measurements include three combined repeats, a 720p
  comparison, sprite/tile scaling and entity/collision/mixer CPU isolates. Exact
  commands, environment, temperature observations and all fourteen raw attempts
  are preserved in [the performance report](../benchmarks/2026-10-16-validation.md).
- Combined frame p99 was 10.912–12.413 ms, with 24.400–33.185 ms maxima. Presentation
  had the largest measured CPU subinterval p99; dense all-pairs collision was the
  largest isolated simulation cost. These are workload-specific observations,
  not an uninterrupted 60 FPS or complete memory-accounting guarantee.

## Thirty-seventh-increment validation (2026-10-16)

All 50 GCC Debug and Release runtime CTest groups pass without skips. The new CPU
report fields are checked as absent measurements, and native measurements verify
1,800 samples per active CPU scope. All benchmark attempts completed their cycles
with stable tracked resources and checksums. Thirteen runs were accepted. One
sprite-only attempt was rejected for an out-of-order GPU timestamp pair; its report
is retained, and a single repeat passed. Its cause remains unresolved. GPU queue
rejection checks remain intact; no waits or timing-based CI thresholds were added.
Physical audio/display and full allocator/driver-accounting gaps remain open.

## Added in the thirty-eighth increment

- GPU timing retains the first eight reversed pairs with exact raw timestamps,
  original frame, reusable slot and latest attempted frame at collection. Later
  failures still increment the total; first-occurrence evidence survives reuse.
- Stress JSON includes bounded records and an omitted count, using decimal strings
  for 64-bit values. Gameplay logs records at normal shutdown. Capture adds no
  allocation, extra GL reads or waits, and invalid timings remain rejected.
- Two new queue cases cover delayed availability, skips, reuse, overflow and precise
  values; a report case checks 64-bit serialization. A native test covers 96 tags
  across twelve target releases before query collection.

## Thirty-eighth-increment validation (2026-10-17)

All 50 GCC Debug and Release runtime groups pass without skips. The gpu_timer_queue,
stress_cpu and stress_report groups pass ASan/UBSan with leak detection enabled.
Four committed-source native sessions complete forty load/unload cycles with stable
resources/checksums: 26,400 pairs submitted, 26,397 valid pairs collected, three
pending, and zero invalid/skipped pairs. All raw reports and exact commands are in
[the investigation report](../benchmarks/2026-10-17-gpu-timing.md).

The prior reversal did not recur; its cause remains unresolved. No GPU timing
synchronization change or new performance guarantee is claimed. Failure capture
is verified through deterministic injected pairs, not a fabricated native failure.
Physical audio/display validation and full allocator/driver accounting remain open.

## Added in the thirty-ninth increment

- `--replay ... --profile-report ... --profile-warmup ...` profiles verified normal
  gameplay rendering with separate simulation, minimap/world/UI submission, swap,
  full-frame and asynchronous combined-render GPU distributions.
- One recorded command advances per rendered frame. Default configuration, silent
  audio, disabled deliberate pacing and rejected workload-changing options isolate
  the run from user settings. Reports retain actual viewport, workload fingerprint,
  final state hash, counters, GPU failures and incomplete/resize status.
- Checked-in Facility/Annex recordings each contain 3,000 commands, complete both
  pre- and post-restart routes and exercise pause/single-step and camera zoom.
  They ship with the documentation and have a deterministic fixture generator.
- Four CPU cases and a two-level native integration group validate boundaries,
  completion, precise failure values, replay equality, report/input preservation
  and user-storage isolation. See [the profile contract](../benchmarks/REPLAY_PROFILING.md).

## Thirty-ninth-increment validation (2026-10-17)

All 52 GCC Debug and Release runtime groups pass without skips. The extended
3,000-command native cases pass in both builds. The replay_profile and replay_files
CPU groups pass ASan/UBSan with leak detection enabled. Six sequential Release
measurements use the same committed source and assets, with three repeats per level;
all finish with matching per-level gameplay hashes, fingerprints and draw counts.

Five timing runs pass; Annex repeat 2 is retained as a failed timing run because it
captured a reversed pair at frame 1614: start 68717863232 ns, end 43536 ns, collected
after tag 1615 in slot 5. A hypothetical 2^36-ns rollover yields a plausible interval,
but the installed driver path is not yet confirmed and no correction is applied.
This is the first native raw failure evidence, not a synchronization fix.

Accepted frame p99 is 9.527–9.861 ms at the actual 1050×1360 window, with maxima
22.539–43.440 ms. These results do not establish the separate 1080p target or
uninterrupted 60 FPS. [Results and all raw attempts](../benchmarks/2026-10-17-replay-results.md)
preserve the failed run and remaining physical audio/display and accounting gaps.

## Increment 40: elapsed GPU queries

- Default `GL_TIME_ELAPSED` timing uses eight asynchronous queries and preserves
  bounded collection, full-pool skipping and delayed frame tags.
- Active-query ownership is checked before begin; a rejected overlap preserves the
  tag and scope. Teardown closes an owned unfinished scope and leaves external
  queries alone. Timestamp-pair mode retains the original failure evidence.
- Replay/stress reports identify timing method and counter width. Historical raw
  results remain unchanged; elapsed durations are not independently validated by
  the timestamp-only invalid counter.
- A separate paced native probe compares elapsed results with nested timestamp
  markers across the suspected rollover. It is diagnostic tooling, not a game-loop
  wait or a performance benchmark.
- Matching upstream Mesa 26.2.2 source confirms the 36-bit nanosecond mask and the
  distinct elapsed delta path. See [evidence and validation](../benchmarks/2026-10-17-elapsed-timing.md).

Debug and Release each pass all 52 runtime groups without skips; focused CPU suites
pass ASan/UBSan. The 75-second native probe captured one reversal in 11,166 scopes;
the enclosing elapsed result differs from the hypothetical wrapped timestamp delta
by 720 ns. Facility, Annex and ten combined stress cycles preserve hashes/counts
and resources, collecting 12,597 elapsed results with three pending and no skips.

## Increment 41: reload/transition spike profiling

- `--spike-report` runs a fixed 3,600-frame, seed-1 workload with 30 warmup frames,
  default configuration, no device audio, no deliberate pacing and no user storage.
- Seven scheduled visual reload/rejection/recovery operations precede a deliberately
  rejected level preflight and two successful transitions through both authored levels.
  The workload never edits asset files and does not weaken ERPL content isolation.
- Event records retain CPU operation time, exact before/after hashes, revisions,
  resource counts/payloads, capped diagnostics, and nearby CPU/GPU timings. Missing
  GPU results remain null. Unexpected operation failures save a partial report.
- Native validation compares event frames and hashes against an independent CPU
  route/transition reference and exercises unexpected failure in a private asset copy.
- [Workload and report contract](../benchmarks/SPIKE_PROFILING.md) distinguish CPU
  operation latency from later rendering and total frame time. Live-resource checks
  do not claim to measure transient staging peaks or driver allocations.

All 54 runtime groups pass in GCC Debug and Release without skips; spike/replay
profile CPU tests pass ASan/UBSan. Three committed-source native attempts preserve
all reference hashes, counts and event frames. Each collects 3,599 elapsed queries
with one pending and no skips. A private-copy unexpected failure retains an incomplete
frame-120 report. [Raw attempts and findings](../benchmarks/2026-10-17-spike-results.md)
include the presentation tail around shader recovery; they do not establish a
constant frame budget or transient-memory bound.

## Increment 42: owned staging payload peaks

- A synchronous RAII counter records CPU, GPU-texel and simultaneous combined
  staging peaks without allocator hooks, GPU waits or counter allocations.
- Reloads account for selected encoded/decoded texture and font buffers, the
  renderer's font copy, shader source lengths and candidate GPU texture payloads.
- Transitions account for the constructed candidate's existing scene/tile/collision
  buffer metrics, gameplay buffers, animation vectors and staged minimap walls.
  Deliberately rejected overlay staging is now destroyed before returning.
- Every native spike event retains `owned_payload_v1` coverage and peak/remaining
  bytes. Remaining tracked staging must be zero for event verification. The CPU
  route reference explicitly marks memory measurement unavailable.
- [Coverage and exclusions](../benchmarks/SPIKE_PROFILING.md#staging-memory-coverage)
  distinguish these selected payload peaks from total allocator/process/driver
  memory. Transition construction intermediates remain outside this first scope.

All 54 runtime groups pass in Debug and Release without skips, and the staging/profile
CPU suite passes ASan/UBSan with leak detection. Three clean-source native attempts
retain identical peaks, reference hashes and event frames, with zero tracked staging
remaining after every operation. The largest covered peak is 7,909,182 bytes during
the return to Facility. An unexpected shader failure retains a clean-staging partial
report. See [raw evidence and results](../benchmarks/2026-10-19-staging-results.md).

## Increment 43: transition loading and parser staging

- `owned_payload_v2` extends the existing spike report with file-read/text-copy,
  binary decoder, scene parser and selected game-construction buffer checkpoints.
  The report layout and procedural game command schedule remain unchanged.
- Level loading now has explicit scene → tilemap → animation → construction order.
  Encoded files are released after decoding; payload ownership transfers between
  scopes without counting the same decoded vectors twice.
- Scene parsing records the overlap between its separate candidate and the game's
  initially reserved world/collision buffers, plus pending-parent and token vectors.
  Binary parsers record growing decoded vectors before final validation.
- CPU regressions exercise seven malformed-input cases, preserve the live hash and
  entity handles, then repair the private files and retry. Native checks retain
  partial reports for late Annex tilemap and scene parse failures.
- Coverage remains selected payload accounting: nested string storage, validation
  hash nodes, allocator internals, failed allocations and opaque driver memory are
  excluded. Member-initialization/copy/move intermediates are not exhaustively tracked.

All 54 runtime groups pass in Debug and Release without skips; the focused CPU
suite passes ASan/UBSan with leak detection. Three clean-source native runs retain
identical v2 peaks and reference hashes. Two malformed-level attempts stop at frame
1383 with unchanged live state and zero remaining staging. The largest covered
transition peak is 8,804,022 bytes. See [measurements and retained failures](../benchmarks/2026-10-19-parser-staging-results.md).

## Increment 44: lazy scene core reservations

- New empty scenes reserve no core vectors. First entity creation reserves the
  existing 4,096-entity capacity; deletion retains it and moves transfer it.
  Game-world placeholders and move destinations avoid throwaway reservations.
- Scene parser staging updates immediately after creation, retaining
  `owned_payload_v2` coverage through later validation failures.
- Allocation-counted regressions cover empty construction, moves, pending-deletion
  transfer, moved-from reuse and a full-capacity deferred queue. Parser tests cover
  failures before and after reservation. Transition tests require construction to
  remain within the completed candidate/preflight payload peak.

All 54 runtime groups pass in Debug and Release without skips. Four focused CPU
groups pass ASan/UBSan with leak detection. Three clean-source native runs reproduce
the same peaks and CPU-reference hashes; the largest covered transition peak falls
from 8,804,022 to 7,909,182 bytes (10.2%). Two malformed-level runs preserve live
state and leave zero tracked staging. Timings carry no speedup claim. See
[measurements and retained failures](../benchmarks/2026-10-20-lazy-scene-results.md).

## Increment 45: collision buffer utilization

- `CollisionWorld::memory_usage()` reports reserved payload by buffer group and
  candidate entry high-water counts from successful updates. Initialization events
  are captured before suppression; failed updates preserve the peaks.
- Spike reports retain bounded per-level maxima across published worlds, including
  warmup. The independent CPU reference supplies matching counters. Unpublished
  candidates are excluded; malformed-transition reports retain only Facility.
- Allocation-counted coverage includes 256 overlapping sensors followed by disjoint
  identities: the existing defaults support 32,640 contacts and 65,280 events
  without allocations after setup. Engine limits and reservations remain unchanged.

All 54 runtime groups pass in Debug and Release without skips; four focused CPU
groups pass ASan/UBSan with leak detection. Three clean-source native runs match
the retained CPU reference exactly. Collision storage reserves 6,557,696 bytes per
world, of which 95.6% is capacity for contacts, active sensor history and events. Facility peaks at
219 bodies and one contact/event; Annex peaks at 146 bodies and no contacts on
the route. Gameplay states and staging peaks match the previous increment; both
malformed-content runs preserve live state and clean staging. See
[measurements and limits](../benchmarks/2026-10-20-collision-usage-results.md).

## Increment 46: per-world collision budgets

- Explicit contact/event budgets retain the engine defaults and full body/query
  capacity. Zero budgets are supported; above-maximum values are rejected before
  reservation. Moves transfer limits and storage.
- Candidate overflow preserves published spans, trigger history, statistics and
  successful usage peaks, including failed `prime` operations. Tests cover exact
  saturation, rollback/retry and allocation-free updates at small/default budgets.
- Feature Lab uses 16 contacts and 32 events across restart, reload, checkpoint and
  level transitions. Tilemap candidates inherit the current world's chosen limits.

All 54 runtime groups pass in Debug and Release without skips; six focused CPU
groups pass ASan/UBSan with leak detection. Three clean-source native runs match
the CPU reference and previous gameplay states. Collision storage falls from
6,557,696 to 293,888 bytes per world (95.5%); the largest covered transition peak
falls from 7,909,182 to 1,645,374 bytes (79.2%). Both malformed-level runs preserve
the live game and clean staging. Coverage is unchanged and timings carry no
speedup claim. See [measurements and rollback evidence](../benchmarks/2026-10-20-collision-budgets-results.md).

## Increment 47: collision grid-membership budgets

- A third optional collision budget controls total grid memberships, retaining
  the 16,384 default and 256-body query capacity. Zero supports query-only worlds;
  above-maximum budgets are rejected before reservation.
- Membership overflow preserves published spans, trigger history, statistics and
  usage peaks, including failed initialization. Total overflow now throws
  `length_error`; the existing per-body cap retains `invalid_argument`.
- Feature Lab reserves 32 memberships, four times the eight-cell bound derived
  from its two participating shapes. Moves and replacement paths retain budgets.
  Saturation, rollback/retry and allocation-counted tests cover small/default grids.

All 54 runtime groups pass in Debug and Release without skips; six focused CPU
groups pass ASan/UBSan with leak detection. Three clean-source native runs match
the CPU reference and previous gameplay states. Collision storage falls from
293,888 to 32,256 bytes (89.0%); the largest covered transition peak falls from
1,645,374 to 1,383,742 bytes (15.9%). Both malformed-level runs preserve live state
and clean staging. Coverage is unchanged and timings carry no speedup claim. See
[measurements and rollback evidence](../benchmarks/2026-10-21-collision-grid-budgets-results.md).

## Increment 48: scene and component storage utilization

- `Scene::memory_usage()` separates core slot/node/deletion-queue reservations from
  aggregate dense/sparse component storage. Successful mutation peaks survive
  deletion and flush; moves transfer history and reset source counters.
- Component demand sums independent per-pool peaks, including entry handles and
  alignment padding. Failed additions do not advance peaks or publish new pools.
  Reads and bounded per-level collection allocate nothing.
- Spike reports retain per-field maxima across published replacement worlds.
  Native checks compare the full scene object to the CPU route reference; malformed
  transition reports exclude unpublished candidates. Reservations remain unchanged.

All 54 runtime groups pass in Debug and Release without skips; six focused CPU
groups pass ASan/UBSan with leak detection. Three clean-source native runs agree:
each scene reserves 1,310,720 bytes, with 4,096-entry capacities. Facility peaks at
11 nodes, Annex at nine, and each level's component pool at three entries. All
gameplay states, collision telemetry and staging peaks match the prior increment;
both malformed-level runs preserve live state and clean staging. See
[measurements and limits](../benchmarks/2026-10-21-scene-usage-results.md).

## Increment 49: per-scene capacity budgets

- `Scene::Limits` bounds live entities, handle slots, core vectors and component
  pools, retaining the 4,096-entry engine default. Parsing accepts explicit limits;
  replacement inherits the live limit and moves transfer it to the destination.
- Feature Lab selects 256 entries for both levels and every restart, reload,
  checkpoint restore and transition. A 257th node rejects without publishing the
  candidate; valid content can be retried successfully.
- Deferred deletion prunes stale generations when its queue fills, retaining valid
  request order and avoiding allocation after destroy/recreate cycles.

All 54 runtime groups pass in Debug and Release without skips; six focused CPU
groups pass ASan/UBSan with leak detection. Three clean-source native runs and the
CPU reference agree: scene reservations fall from 1,310,720 to 81,920 bytes (93.75%),
and the largest covered transition peak falls from 1,383,742 to 154,942 bytes (88.8%).
Gameplay states, collision telemetry and mutation demand are unchanged. Malformed
scene/tilemap and over-budget scene attempts preserve live state and clean staging.
Coverage is unchanged and timings carry no speedup claim. See
[measurements and budget-failure evidence](../benchmarks/2026-10-21-scene-budgets-results.md).

## Increment 50: offline asset validation

- `validate_assets [--assets DIRECTORY]` checks the fixed Feature Lab inventory,
  textures referenced by successfully constructed levels, both level constructors
  and both directions of texture residency. It reuses bounded engine decoders and
  gameplay checks, including the 256-entry scene budget and required animation modes.
- Independent failures retain file/level context and continue to a final summary.
  Exit codes distinguish valid content (0), content/root failures (1) and argument
  errors (2). Missing/corrupt assets never use fallback artwork or sounds.
- The executable builds with `ENGINE_BUILD_PLATFORM=OFF` and has only C/C++ runtime
  dependencies. It writes no assets/settings and opens no display or audio device.
  Shader validation covers bounded, nonempty, NUL-free source reads; GLSL compilation,
  rendering, audible output and route completion still require runtime checks.
- Native and CPU-only installs include the command. The release archive validates
  its assets after extraction and relocation before publication, checks working-
  directory isolation, and exercises missing-root rejection and explicit overrides.

All 56 runtime groups pass in Debug and Release without skips. The two validator
CPU groups pass ASan/UBSan with leak detection in the platform-disabled build.
Private-copy regressions cover independent malformed files, repair/retry, 256/257
scene boundaries, missing gameplay roles, invalid spawns, required animation loops,
atlas dimensions, missing references, cross-level residency, root confinement and
shader size limits. CLI tests verify exit statuses, discovery and unchanged asset
hashes/inventory. Shipped content produces 20 checks with zero failures with display
variables unset. All three package checks pass, including relocated CPU validation
and OpenGL gameplay. See [usage and validation boundaries](ASSET_VALIDATION.md).

## Increment 51: single-file asset validation

- `validate_assets --type TYPE --file KEY [--assets DIRECTORY]` explicitly selects
  scene, tilemap, animation, texture, font or sound decoding, independently of the
  filename extension. Only the root-relative file is checked; referenced assets
  and Feature Lab gameplay/transition requirements are outside this mode.
- Generic scene validation uses the 4,096-entry engine default. `--scene-budget N`
  accepts 0..4,096 with scene mode, including zero for an empty scene. Bundle mode
  retains its fixed 256-entry budget and rejects overrides.
- Missing option pairs, duplicates, unknown types, malformed budgets and incompatible
  flags return 2 before root discovery. Content/path/budget-exhaustion failures
  return 1 with file context; scene failures retain source/line diagnostics.
- The existing bounded readers, confinement, read-only operation and CPU-only
  build/install path are retained. Reports identify the selected format and scene
  budget and state that dependencies and gameplay were not checked.

All 56 runtime groups pass in Debug and Release without skips. Both validator CPU
groups pass ASan/UBSan with leak detection. Tests cover every format in an otherwise
empty root, renamed extensions, truncated files and repair/retry, unchanged bytes,
generic scenes without gameplay roles, 256/257 and zero budgets, wrong decoders,
missing parents/files, size limits and escaping paths. CLI tests cover all six
formats and option combinations, including duplicate zero-valued budgets. All three
package checks pass; the relocated command validates a scene with an explicit
256-entry budget alongside the existing bundle checks.
See [single-file usage and boundaries](ASSET_VALIDATION.md#single-file-format-checks).

## Increment 52: versioned JSON asset-validation reports

- `validate_assets --json` emits one `engine_asset_validation_v1` object to stdout
  for bundle or single-file validation. Text remains the default. Reports include
  mode, format/key, scene budget, requested/canonical roots, completion/success,
  counts, ordered check results and diagnostics.
- Content failures return complete failing reports; fatal root/validation errors
  return incomplete reports with empty checks and retained request context. Argument
  errors remain stderr text with exit 2. JSON output failures return 1 rather than
  reporting success when stdout rejects the write.
- The profiler's existing byte-escaping function is shared without adding a JSON
  dependency or linking benchmark code into the validator. `string_encoding` names
  its reversible byte-value convention; output stays ASCII even for invalid UTF-8
  paths. Numeric serialization uses the classic locale and has no timestamps.
- Package CPU checks emit and independently parse a JSON scene report from the
  relocated executable. Existing text output, validation rules and asset formats
  are preserved. Dependent checks remain omitted when their prerequisites fail;
  the schema documents counts as performed checks and distinguishes completion
  from success.

All 56 runtime groups pass in Debug and Release without skips. Five CPU groups
(`asset_validation`, `asset_validation_cli`, `stress_cpu`, `replay_profile`,
`spike_profile`) pass ASan/UBSan with leak detection. Regressions cover all six
formats, default/zero budgets, deterministic output, content/fatal errors, null
fields, quotes/backslashes/newlines/control/non-ASCII bytes, numeric locale changes,
argument errors and rejected stdout writes. An independent JSON reader validates
CLI fields and completion/success semantics. All three package checks pass, including
relocated JSON validation and OpenGL gameplay. See
[the report schema and byte convention](ASSET_VALIDATION.md#json-reports).

## Increment 53: release-readiness audit

- Audited the plan, F01–F16 manifest, current CTest inventories, retained validation
  logs, platform/audio checklists and versioned benchmark evidence at `9b9b99f`.
- Published eight release gates with commands and evidence requirements, separating
  current passes, historical/subset evidence, unconfirmed behavior and missing checks.
- Identified the current compiler/sanitizer matrix, concurrent audio TSan validation
  and a clean network-isolated build as the next automatable gaps. The sanitizer
  inventory contains 30 groups, with some binaries absent from the existing build;
  the recent five-group pass does not certify the full suite.
- Kept physical display/input, audible output/device lifecycle, performance/memory
  acceptance and persistence fault/compatibility follow-up open. Optional extensions
  are separate; no runtime behavior or release requirement changed.
- Corrected stale overview language about delivered diagnostics and 1080p profiling.
  The audit is installed with the existing documentation in release packages.

Documentation validation resolves 88 local links and syntax-checks all four audit
shell blocks. All three Release package checks pass without skips at `6a6077d`
(26.86 s), including relocated OpenGL gameplay; the archived audit matches the
committed file. The full compiler/sanitizer and offline-build recipes were not run
in this documentation increment and remain open gates.

## Increment 54: candidate compiler, sanitizer and offline validation

- Fresh builds at implementation baseline `1dfba31`: GCC 16.2.1 Debug and Release,
  and Clang 23.1.3 Debug, each pass all 56 runtime groups without skips.
- Full CPU ASan/UBSan passes all 30 groups with leak detection enabled. Native
  ASan/UBSan passes all 56 groups with driver leak detection disabled; both native
  audio groups separately pass with leak detection enabled.
- A dedicated Clang ThreadSanitizer build passes both concurrent audio groups,
  covering exercised queue, bank-retirement and shutdown paths with null/failing
  ALSA sinks. No race reports, source fixes, suppressions or timeout changes.
- A fresh archived source outside the repository builds every target with networking
  disabled, passes all 32 CPU groups and creates/validates its release archive.
  All three package checks then pass outside isolation, including OpenGL gameplay.
  Source/archive hashes identify the build despite its embedded `unknown` revision.
- Commands, exit codes, toolchain/environment, logs, CTest XML and offline package
  inventories are retained under `validation/2026-10-23/` and installed with the
  documentation. These are correctness/build records, not performance measurements.

R1–R3 have evidence for the named baseline on this host. No engine or test code
changed, and all attempts passed. Physical display/audio, workload/memory acceptance,
persistence fault coverage and final support/coverage signoff remain open. See
[results and limitations](../validation/2026-10-23/RESULTS.md).

Documentation checks resolve 123 local links and validate all 21 command records
and eight CTest XML reports. The evidence-bearing package at `711d764` passes all
three package checks without skips; all 42 validation files match the installed
archive byte-for-byte. Raw dependency inventories retain their generated trailing
blank line unchanged.

## Increment 55: atomic-save filesystem failures

- Added the CPU-only `atomic_write` group with eight named cases and scoped Linux
  linker wrappers. It exercises real private files with short/interrupted writes,
  create/write/sync/close/rename/open errors, existing/absent destinations, cleanup
  failure, retry, and immediate descriptor reuse after close errors.
- Reproduced two failures before fixing them: zero-progress writes could report
  zero/stale errno, and directory-close failures were ignored. Zero progress now
  reports `EIO`; directory-close errors report failure after the new file is visible.
- Verified primary errors survive secondary cleanup failures, old destinations
  survive pre-rename failures, new destinations survive post-rename failures, and
  failed close calls are not retried on a reused descriptor. Failed unlink may
  leave an orphan; cleanup remains best effort with no automatic orphan deletion.
- No format/API changes or production fault hooks. Tests do not fill disks, alter
  mounts or exercise physical power loss. The new failure contract distinguishes
  operation failure, published contents and uncertain crash durability.

At `be16f3e`, all 57 Debug and Release runtime groups pass without skips. Atomic
write, persistence and content groups pass with Clang and CPU ASan/UBSan with leak
detection. The initial two failures and post-fix commands/logs/XML are retained in
`validation/2026-10-24-atomic-writes/` (UTC date). Earlier full native sanitizer,
TSan and offline-build records retain their original revision and scope. See
[fault contract](ATOMIC_SAVE_VALIDATION.md) and
[regression results](../validation/2026-10-24-atomic-writes/RESULTS.md).

All three package checks pass at `71beaa6` without skips, including relocated
OpenGL gameplay (27.42 s total). The 22 new evidence files and atomic-save contract
match the archived copies byte-for-byte. Documentation validation resolves 121
local links, parses the shell recipe and checks all 12 command records/four XML reports.

## Increment 56: save-directory chain preparation

- `UserStorage` validates the record, resolves existing symlinks, creates the
  directory chain top-down, then opens/syncs/closes every resolved ancestor from
  leaf through `/` before calling the existing atomic-file primitive.
- Existing ancestors are rechecked on every attempt: directory existence, a new
  storage object or a previous process exit cannot bypass preparation. Partial
  directories remain after failure for retry; unrelated files are never replaced.
- Preparation failures include operation/path context, preserve the old/absent
  record and create no record temporary. Sync errors survive secondary close errors;
  close is attempted once. Reads and invalid records/slots have no preparation effects.
- Added ten CPU cases for config/slot paths, first/repeated saves, creation and
  ancestor open/sync/close errors, descriptor reuse, symlinks, `EEXIST` races,
  nondirectory obstructions, invalid input, normalized read/write paths and retry
  after a child exits during sync. A late regression caught a save/read mismatch
  for `missing/../config`; reads now use the same path resolution as preparation.
- No format/API changes or production fault hooks. Every resolved ancestor must
  allow directory open/sync; there is no unsupported-filesystem fallback. Preparation
  adds synchronous I/O and does not certify physical power-loss durability.

The initial nine-case suite against the old implementation fails eight new-contract
cases; that comparison and its source hashes are retained. The subsequent tenth
case reproduced one read/write normalization failure before its fix. At `aa53ca4`, all 58
Debug and Release runtime groups pass without skips. Four relevant groups pass with
Clang and CPU ASan/UBSan with leak detection. Earlier full native sanitizer, TSan
and offline-build evidence retains its original revision/scope. See the
[preparation contract](STORAGE_DIRECTORY_VALIDATION.md) and
[commands, logs and results](../validation/2026-10-24-storage-directories/RESULTS.md).

All three package checks pass without skips at `34df29f` (23.87 s total), including
relocated OpenGL gameplay. The directory contract and 44 evidence files match the
archive byte-for-byte. Documentation checks resolve 136 local links and validate
24 command records, eight XML reports, retained failures and shell syntax.

## Increment 57: gameplay and audio allocation profiling

- Added development-only `feature_lab_allocations` and `audio_allocations`
  executables. Production targets and packages retain their normal allocators.
- Thread-local snapshots count successful C++ allocation calls/requested bytes;
  fixed atomic aggregates report samples, nonzero samples, totals and maxima.
  Tests cover all eight allocating forms, alignment, zero requests, nested scopes,
  bookkeeping and thread isolation. Counters do not measure live heap or C/GPU memory.
- Gameplay uses the existing verified Facility/Annex replay path and six frame
  boundaries, excluding the 30-frame warmup. The enclosing frame includes its
  subscopes. World submission includes replay-status argument formatting.
- Audio uses the actual soundtrack producer and worker against ALSA `null`, with
  separate producer, command/mix and transport scopes. Worker shutdown precedes
  report aggregation. Startup decoding, physical devices, reloads and recovery
  allocation coverage remain outside this workload.
- Three Release repeats per level expose recurring HUD/status-string allocation
  demand and the intentional restart sample. Three audio repeats observe zero
  C++ allocations in the measured producer and worker scopes. Null sample counts
  vary with scheduling and do not establish real-time or audibility acceptance.

See [measurement contract](../benchmarks/ALLOCATION_PROFILING.md),
[results](../benchmarks/2026-10-24-allocation-results.md) and
[validation/provenance](../validation/2026-10-24-allocations/RESULTS.md).

At `31861ad`, all 61 Debug and Release runtime groups pass without skips.
Six focused Clang, three CPU ASan/UBSan, four native audio/counter ASan/UBSan and
four shared-runtime TSan groups pass. The initial static TSan link conflict and
an expected positive-control race remain in the evidence. Sanitizer leak checks
are enabled; no suppressions were added. Documentation checks resolve 160 local
links and validate 31 command records, six XML reports and all nine measurement reports.

All three package checks pass without skips at `6ea4f73` (26.66 s), including
relocated OpenGL gameplay. The 53 evidence files and two allocation documents match
the archive byte-for-byte; profiling executables are absent from the package.

## Increment 58: reusable HUD/status text

- Added caller-owned formatting that appends integer values directly with
  `std::to_chars`. Gameplay reserves 256 and 128 characters in two persistent
  strings before its frame loop; longer future labels grow without truncation.
- Status helpers share the formatter through new caller-buffer overloads while
  preserving their owning-string getters. Collision visualization retains its
  convenience overload. No cache invalidation state or engine API/format change.
- Replaced owning map-legend rows with literal views and reused storage for world
  alarm labels, HUD counters/status, completion hints and replay/record progress.
  Formatting stays inside the original measured scopes.
- Four CPU cases cover integer limits, shorter/empty output, lossless growth/reuse,
  zero-allocation repeated formatting and ready/failure/locked/replaced labels.
  Audio tests cover changed settings and a rebound international chord.
- Three Release repeats per level record zero C++ allocations in world/HUD
  submission. Simulation/restart counts, gameplay hashes, viewports and render
  totals match the earlier baseline. Seventeen before/after screenshot pairs
  match byte-for-byte across gameplay, pause, step, restart, completion, recording
  and reload-failure states.

At `a1fe4f3`, all 62 Debug and Release runtime groups pass without skips. Focused
Clang (seven), CPU ASan/UBSan (five), native ASan/UBSan (three) and shared-runtime
TSan (three) groups pass. The graphical ASan/UBSan replay also passes with driver
leak detection disabled; CPU/audio sanitizer checks retain leak detection.
The initial build caught a missing convenience overload;
its failure and correction are retained separately from final validation.
See [storage/lifetime contract](../benchmarks/HUD_TEXT.md),
[before/after results](../benchmarks/2026-10-24-hud-text-results.md) and
[validation](../validation/2026-10-24-hud-text/RESULTS.md).

At documentation commit `1204ae3`, all three Release packaging checks pass
(27.59 s); all 91 evidence files and both new benchmark documents match the archive.
Profiling executables remain excluded from installation.

## Increment 59: UI allocation coverage

- Added a development-only `ui_allocations` executable with nine states per level:
  audio off/on, remapped audio/control labels, settings, controls, binding capture
  and update, diagnostics, and debug shapes. Ordinary gameplay and the probe now
  share the unchanged drawing body through `game_draw.hpp`.
- Each frozen state renders 300 offscreen frames at 1280×800, with 30 warmup and
  270 measured frames. Render-thread scopes separate settings update, minimap,
  world, HUD, diagnostics, settings drawing and renderer end; the enclosing frame
  equals their sum. Opening/rebuilding/rebinding costs have separate counters.
- Three Release runs match in all report fields. Settings, controls and enabled-
  audio HUD redraws record zero C++ allocations, including international/mouse
  chords. Diagnostics records 16 calls / 537 requested bytes per measured frame
  in both levels, with or without shapes. No performance or whole-engine bound.
- The checker verifies all 18 states, real menu transitions, changed labels,
  sample counts, nonempty stable renders and inclusive allocation accounting.
  Eight before/after ordinary-game screenshot pairs match byte-for-byte.

At `4e860f5`, all 63 Debug and Release runtime groups pass without skips. Seven
focused Clang groups and three graphical ASan/UBSan groups also pass. Driver leak
detection is disabled for the graphical checks; no leak/race acceptance is implied.
Production has no allocation-profile symbols; the probe remains a development tool.

See [workload contract](../benchmarks/UI_ALLOCATIONS.md),
[results](../benchmarks/2026-10-25-ui-allocation-results.md) and
[validation](../validation/2026-10-25-ui-allocations/RESULTS.md).

At documentation commit `56492b9`, all three Release packaging checks pass
(48.85 s). All 46 evidence files and both new benchmark documents match the archive;
all profiling executables remain excluded from installation.

## Increment 60: reusable diagnostics text

- Diagnostic rows now reuse the existing 256-character HUD buffer. `Fixed2`
  appends two-decimal values directly with `std::to_chars`, preserving negative
  zero, rounding, nonfinite spellings and the old 64-character `OVERFLOW` behavior.
  The convenience drawing overload remains available and may allocate.
- Live GPU collection delegates to a shared ingestion method. A development-only
  `diagnostics_allocations` target exercises unsupported/waiting/completed and
  changing synthetic GPU labels, alone and behind settings/remapped controls.
  Its 12 cases have a separate workload identity from the original 18 UI cases.
- Three before/after Release repeats preserve state, rendering totals/fingerprints
  and transition costs. All measured render-thread scopes now record zero C++
  allocations in both workloads; the original diagnostics cases drop from 16
  calls / 537 bytes per frame to zero. Synthetic history setup stays excluded.
- Forty-eight PPM pairs are byte-identical. CPU cases cover decimal edge values,
  shorter replacements and delayed GPU ingestion without allocation. The shared
  checker guards zero recurring diagnostic calls after warmup in these fixtures.

At `eabf433`, all 64 Debug and Release runtime groups pass without skips. Eight
focused Clang, four CPU ASan/UBSan and four graphical ASan/UBSan groups also pass.
CPU leak detection is enabled; driver leak detection is disabled for graphics.

See [storage and workload contract](../benchmarks/DIAGNOSTIC_TEXT.md),
[measurements](../benchmarks/2026-10-26-diagnostic-text-results.md) and
[validation](../validation/2026-10-26-diagnostics-text/RESULTS.md).

At documentation commit `016842c`, all three Release packaging checks pass
(27.20 s). All 49 evidence files and both new benchmark documents match the archive;
all four profiling executables remain excluded from installation.

## Increment 61: declared workload and memory acceptance

- Committed timing and memory budgets before collection: six workloads, three
  repeats, explicit state/resource checks and separate p99/worst-frame decisions.
- Added optional standard-library Python collection/evaluation utilities. Per-child
  Linux peak RSS includes startup/teardown/reporting; payload ceilings and allocation
  histories retain their separate coverage. Audited unitemized storage and limits.
- At `d4dbe22`, all 18 ordinary Release runs complete with valid evidence. Every
  memory budget passes (maximum RSS 53.25 MiB against 256 MiB). Seventeen p99 checks
  pass, but only four worst-frame checks pass. Combined repeat 2 reaches p99
  18.155 ms; the largest frame is 44.148 ms. Timing acceptance remains failed.
- Ten focused Release groups pass without skips; 22 synthetic evaluator controls
  distinguish invalid evidence, measured budget failures and inclusive limits.
  Runtime code/assets are unchanged; this does not refresh the full sanitizer matrix.

See [declaration and memory audit](../benchmarks/WORKLOAD_ACCEPTANCE.md),
[results](../benchmarks/2026-10-26-workload-acceptance-results.md) and
[retained evidence](../validation/2026-10-26-workload-acceptance/RESULTS.md).

At documentation commit `9458a10`, all three Release package checks pass (27.26 s).
All 53 evidence files and both benchmark documents match their archived copies;
optional Python evidence tools are included, and allocation profilers remain excluded.

## Increment 62: paired stress-frame timings

- Added opt-in `--stress --frame-samples` records. CPU intervals reuse existing
  clock boundaries; delayed GPU results join by original frame tag across warmup
  and cycle boundaries. Missing results remain null and no GPU waits are added.
- Preallocated storage is bounded to 60,000 rows, with actual capacity bytes
  reported separately. Recording and pairing allocate nothing after reserve.
  Default reports omit the new fields and reserve no row payload.
- At `271053f`, three traced and three plain combined-workload attempts complete.
  All trace aggregates reconstruct from rows; workload/state/render totals match.
  Among 5,400 traced frames, 17 exceed 16.7 ms: presentation is the largest CPU
  interval in 15, submission in two. Paired GPU intervals are 0.872–2.369 ms.
- Eight focused groups pass in GNU Debug/Release and Clang; five CPU and three
  native ASan/UBSan groups pass, without skips. Four on/off image pairs match.
  These are focused checks, not a new full validation matrix.

See [trace contract](../benchmarks/STRESS_FRAME_SAMPLES.md),
[findings](../benchmarks/2026-10-27-frame-samples-results.md) and
[evidence](../validation/2026-10-26-frame-samples/RESULTS.md). All six maxima still
exceed the frame budget; neither prior failures nor physical acceptance are waived.

At documentation commit `328c41f`, all three Release package checks pass (27.29 s).
All 40 evidence files and both new benchmark documents match their archived copies;
the allocation profilers remain excluded from installation.

## Increment 63: explicit stress presentation requests

- Added `--presentation immediate|vsync` for stress mode using the existing GLX
  swap-interval API. Immediate remains default; gameplay settings are unchanged.
  Reports distinguish request issuance from physical confirmation, count all
  presentation calls, and preserve truthful legacy VSync flags.
- Conflicting/duplicate/unknown arguments and presentation options in CPU-only
  runs are rejected before replacing reports. The legacy `--no-vsync` alias stays
  compatible with immediate and CPU modes.
- At `5a6f1b2`, all six alternating traced runs complete with identical workloads,
  state/render counts and tracked payloads. Immediate median frames are 3.18–3.50 ms;
  requested VSync medians are 16.66–16.67 ms. All six maxima still exceed 16.7 ms.
  Requests are issued, but physical synchronization is not verified.
- Eight focused groups pass in GNU Debug/Release and Clang; five CPU and three
  native sanitizer groups pass, without skips. Eight default/explicit-mode image
  pairs match. These are focused checks, not a refreshed full validation matrix.

See [mode contract](../benchmarks/STRESS_PRESENTATION.md),
[comparison](../benchmarks/2026-10-27-presentation-results.md) and
[evidence](../validation/2026-10-27-presentation/RESULTS.md).

At documentation commit `3fd78d9`, all three Release package checks pass (25.50 s).
All 40 evidence files and both new benchmark documents match their archived copies;
the allocation profilers remain excluded from installation.

## Increment 64: opt-in submission timing split

- Added `--submission-samples`, requiring graphics and `--frame-samples`. The
  offscreen pass records GL setup and nonempty batch upload/draw CPU intervals;
  preparation/bookkeeping is the enclosing render interval minus those measurements.
  Existing asynchronous GPU, preview and presentation timings remain separate.
- Disabled runs add no timing reads or row payload. Enabled storage is bounded to
  60,000 rows and records without allocation after reserve. Reports expose actual
  storage bytes and per-frame batch/upload counts; invalid CPU/unpaired requests
  preserve prior reports.
- All six declared on/off attempts complete with identical workload/state/render
  counts and validated nested partitions. No buffer-streaming strategy changes.
- Nine focused groups pass in GNU Debug/Release and Clang; five CPU and four native
  sanitizer groups pass, with no skips. Sixteen image comparisons match.

See [contract](../benchmarks/STRESS_SUBMISSION.md),
[results](../benchmarks/2026-10-27-submission-results.md) and
[evidence](../validation/2026-10-27-submission/RESULTS.md).

At documentation commit `1b21565`, all three Release package checks pass (33.55 s).
All 42 evidence files and both new benchmark documents match their archived copies;
the allocation profilers remain excluded from installation.

## Increment 65: preparation phases and calling-thread CPU time

- Added opt-in `--preparation-samples`, requiring frame and submission samples.
  Four coarse phases record wall and calling-thread CPU time, with nested GL
  intervals. Signed wall-minus-CPU differences remain visible; no GPU waits or
  per-sprite timers are added. Existing report rows and payloads are unchanged.
- Separate storage bounds 60,000 rows and records without further allocation.
  Clock failures report errors; nominal clock resolution does not claim accuracy.
  Off-phase wall time remains explicit rather than inventing a CPU measurement.
- All six declared on/off runs complete with matching workload/state/render counts
  and validated phase/GL partitions. Nine focused groups pass in GNU Debug/Release
  and Clang; five CPU and four native sanitizer groups pass, without skips.
  Twenty-four final-image comparisons match byte-for-byte.

See [contract](../benchmarks/STRESS_PREPARATION.md),
[findings](../benchmarks/2026-10-27-preparation-results.md) and
[evidence](../validation/2026-10-27-preparation/RESULTS.md).

At documentation commit `82b6b2f`, all three Release package checks pass (33.45 s).
All 42 evidence files and both new benchmark documents match their archived copies;
the allocation profilers remain excluded from installation.

## Next increment

Correlate the paired gaps with render-thread voluntary/involuntary context-switch
counters before choosing a tail-latency fix. The wall-minus-CPU gap exceeds CPU
time in 23 of 30 largest profiled submissions; three p99 values and all six maxima
still fail 16.7 ms. Counter deltas do not identify wait durations or blocking causes.
Keep steady CPU optimization separate, with the scalar path as a reference.
Keep presentation timing, workload identities, declared limits and failed attempts.
Physical display/audio, broader memory-path coverage and the final support record
remain open; background loading remains deferred.
