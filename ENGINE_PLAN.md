# Linux OpenGL 2D Game Engine Plan

Status: implementation started. This document describes the target design, not a
claim that every feature is complete. See [implementation status](docs/STATUS.md)
for delivered capabilities, test evidence, and remaining work. The eighth delivered
increment adds box/circle collision detection, spatial queries, filters, and triggers.
Diagnostics and simulation controls remain planned.

## 1. Goal and confirmed requirements

Build a reusable, high-quality, performance-focused 2D game engine for Linux PCs.
Write the engine code in-house and prove its capabilities with automated tests and
a playable example game that exercises every supported engine feature.

1. **Linux only.** No Windows, macOS, mobile, or browser support is required.
2. **OpenGL rendering.** The testing PC reportedly exposes an OpenGL version string
   of **4.6**. Verify the actual context and capabilities during startup.
3. **2D only.** No 3D renderer, perspective cameras, meshes, 3D physics, or 3D asset
   pipeline is needed.
4. **No external libraries for engine functionality.** Write all required engine
   systems, utilities, asset readers, tools, and test harnesses in-house. Do not
   vendor or copy third-party implementations as a workaround. Minimal installed
   Linux system libraries for window creation and audio are permitted alongside
   the C++ runtime and OpenGL driver.
5. **Follow game-engine engineering practices.** Use clear subsystem boundaries,
   explicit ownership, predictable updates, robust errors, and resource limits.
6. **Prioritize quality and performance.** Design for maintainability, correctness,
   efficient data access, and stable frame times. Measure optimizations.
7. **Write tests.** Automated tests are part of every implementation milestone.
8. **Create a comprehensive test game.** Every supported engine feature must have a
   documented game scenario and corresponding validation.

This revision replaces the previous 3D design and third-party dependency stack.
The initial documentation phase is complete; implementation now follows the
milestones below.

## 2. Technology choices and dependency boundary

### Proposed foundation

| Area | Choice | Purpose |
| --- | --- | --- |
| Language | C++20 (selected) | Engine, example game, asset tools, and tests |
| Graphics | OpenGL 4.6 Core Profile; GLSL 4.60 | Custom 2D renderer and shaders |
| Initial CPU target | Linux x86-64 | Focus initial validation on the testing PC |
| Toolchain | GCC and Clang | Compile and cross-check project code |
| Build tools | CMake, Ninja, CTest | Build targets and invoke in-house test executables |
| Debugging tools | GDB, compiler sanitizers, Linux profiling tools | Investigate failures and measure performance |
| Runtime architecture | Engine library plus separate game executable | Reuse systems without coupling them to one game |

Build and debugging programs are development tools, not libraries incorporated
into the engine. No build step may download third-party engine or test code.

OpenGL 4.6 is the proposed baseline because the testing PC reports support for it.
That string does not establish the context profile, GPU speed, or memory capacity.
Request a Core context and validate its capabilities. Use graphics features where
they serve the 2D design; a complex renderer is not a goal. API definitions are in
the [Khronos OpenGL registry](https://registry.khronos.org/OpenGL/index_gl.php) and
[OpenGL 4.6 reference pages](https://registry.khronos.org/OpenGL-Refpages/gl4/html/start.html).

### What must be written in-house

- Application lifecycle, logging, assertions, configuration, and timing.
- Window/input integration and OpenGL entry-point loading.
- 2D mathematics, cameras, rendering, shaders, batching, and text drawing.
- Entity/component storage, hierarchy, serialization, and asset management.
- Supported image, sound, font, tilemap, and animation readers/writers.
- Collision detection, movement resolution, queries, and triggers.
- Audio mixing, playback control, and platform audio integration.
- UI widgets, debug overlay, profiling instrumentation, and developer commands.
- Test harness, assertions, fixtures, replay runner, and reports.
- The complete example game and its test content.

Do not use SDL, GLFW, GLAD, GLEW, GLM, EnTT, stb, physics/audio frameworks,
serialization libraries, Dear ImGui, or external testing frameworks. Loading a
prohibited library dynamically does not satisfy the restriction.

### Operating-system and driver boundary

The Linux kernel, display server/compositor, and installed OpenGL driver are
platform prerequisites. The engine is not a replacement operating system or GPU
driver. Access to the installed OpenGL implementation is inherent in the OpenGL
requirement. The C++ runtime and standard library are permitted.

**Confirmed exception:** minimal installed Linux system libraries may provide
window/display integration and audio device or server access. Depending on the
selected backend, this can include Xlib or XCB, Wayland client libraries, and ALSA
or PipeWire client libraries. Use only the libraries needed for the chosen backend;
this does not authorize general-purpose engine or multimedia frameworks.

Keep these calls behind an engine-owned platform layer. System libraries may handle
native window creation, platform event delivery, graphics-context integration, and
audio output transport. The engine must still implement input actions, OpenGL
entry-point loading, rendering, assets, scenes, collision, PCM decoding, mixing,
playback logic, UI, diagnostics, and tests in-house. There is no requirement to
reimplement display-server protocols or kernel audio interfaces merely to avoid
the approved system libraries.

Choose one initial display and audio backend that works on the testing PC. Record
the selected system libraries and their purpose during platform implementation.
The initial implementation selects Xlib/GLX and ALSA; details and tested versions
are recorded in [implementation status](docs/STATUS.md).
Native Wayland and X11 are separate targets; XWayland does not prove native Wayland
support. Record direct/transitive runtime dependencies and distinguish engine
choices from installed driver internals. Do not promise broad Linux compatibility
before checking the declared configurations.

## 3. Architecture and engineering standards

| Module | Responsibilities |
| --- | --- |
| Core | Lifecycle, ownership, timing, errors, logging, configuration |
| Platform | Native window/context, events, files, device integration |
| Input | Actions, edge events, pointer coordinates, focus, UI/game routing |
| Math | Vectors, affine transforms, rectangles, circles, interpolation |
| Renderer2D | Sprites, batching, cameras, tiles, text, render targets, debug shapes |
| Assets | Validated loading, caching, stable IDs, resource lifetime, reload |
| Scene | Entity handles, component storage, hierarchy, load/save |
| Collision2D | Broad phase, shape tests, movement resolution, triggers, queries |
| Audio | PCM decoding, voice mixing, loops, gain, panning, output buffering |
| UI | Layout primitives, labels, buttons, sliders, focus, event consumption |
| Diagnostics | Counters, timing, debug drawing, logs, test reporting |
| Game | Example scenarios and game rules using public engine APIs |
| Tests | In-house runner, fixtures, regression checks, scripted scenarios |

### Code quality rules

- Separate simulation from platform I/O and OpenGL. CPU tests must run without a
  display or audio device. The engine must not depend on example-game code.
- Make dependencies explicit. Avoid hidden global mutable state and circular
  dependencies; prefer small functions and narrow public interfaces.
- Use RAII and clear ownership. Release GPU resources before their context. Define
  initialization rollback and shutdown order for partial failures.
- Use generation-checked handles for runtime entities/resources and persistent IDs
  in saved files. Never serialize pointers or OpenGL object names.
- Use compact component arrays and predictable iteration. Defer structural mutations
  during iteration to defined synchronization points.
- Document API contracts, ownership, coordinate spaces, thread affinity, and error
  behavior. Validate lengths, indices, dimensions, counts, and arithmetic before use.
- Report malformed content with file/location context. Failed loads must not
  partially replace a valid scene or live asset.
- Enable strict project warnings, consistent formatting, assertions, and sanitizers.
  Release builds must still reject invalid external data.
- Introduce custom allocators, complex abstractions, and worker threads only when
  justified. Standard language facilities are appropriate when they meet the budget.
- Every change includes relevant tests and consideration of resource lifetime,
  error paths, and performance. Document intentional limits.

### Update and threading model

1. Poll events and translate them into input actions.
2. Accumulate monotonic elapsed time; clamp unusually long pauses.
3. Run fixed simulation steps at 60 Hz: gameplay, movement/collision, triggers,
   animation, then deferred scene mutations.
4. Cap catch-up work and record dropped simulation time.
5. Interpolate presentation transforms, build the visible draw list, render world
   layers and UI, and present.
6. Record CPU/GPU timings without forcing GPU completion.

Queue input edges until a step consumes them. A press must neither disappear on a
frame with zero steps nor replay on every catch-up step. Clear held state on focus
loss. Define pause and single-step behavior explicitly.

Keep events and OpenGL on the owning main thread. Use an audio thread as required
by the output backend, communicating through a bounded queue. The audio processing
path must not allocate, perform file I/O, log synchronously, or wait on the game
thread. Add other worker jobs only when profiling identifies useful work.

## 4. Required 2D features

Feature IDs are the coverage contract. Every new supported feature needs an ID,
automated checks, and a game scenario before it is complete.

| ID | Feature | Required behavior and initial limits |
| --- | --- | --- |
| F01 | Platform lifecycle | Window/context creation, resize, drawable scaling, close, focus, minimize/restore, fullscreen, configurable VSync, capability errors |
| F02 | Input | Rebindable keyboard/mouse actions, pressed/held/released states, pointer-to-world conversion, focus handling, UI/game routing |
| F03 | Math and camera | Translation, rotation, scale, parent transforms, orthographic projection, camera follow/zoom, screen/world conversion |
| F04 | Sprites | Textured quads, tint, rotation, flipping, atlas regions, transparency, stable layers/order, batching, camera culling |
| F05 | Tilemaps | In-house format, tile layers, atlas lookup, visible-chunk traversal, collision metadata |
| F06 | Animation | Named frame sequences, frame durations, loops/one-shot playback, pause, completion events |
| F07 | Scenes/entities | Create/destroy, component queries, stale-handle rejection, cycle-safe hierarchy, versioned load/save, persistent IDs, transitions |
| F08 | Assets | In-house readers, shared cache, fallback assets, bounded allocations, lifetime management, safe reload |
| F09 | Collision | Axis-aligned boxes/circles, static/kinematic objects, broad/narrow phases, layers, triggers, overlap/raycast queries, swept player movement against walls |
| F10 | Audio | PCM effects/music, simultaneous voices, loops, master/music/effects gains, stereo panning, mute, output failure handling |
| F11 | Text/UI | Bitmap-font labels, buttons, volume slider, pointer/keyboard navigation, input consumption, HUD/pause menu |
| F12 | Render targets/clipping | Offscreen minimap, UI scissor rectangles, resize-safe framebuffer resources |
| F13 | Timing/replay | Fixed steps, interpolation, pause/single-step, bounded catch-up, seeded random state, tick-indexed input playback |
| F14 | Persistence | Versioned scene/save/config formats, validated round trips, interrupted-write protection, incompatible-version errors |
| F15 | Diagnostics | Logs, in-house overlay, CPU/GPU counters, collision/camera debug shapes, shader diagnostics, graceful asset errors |
| F16 | Delivery/resource stability | Standalone launch, asset-root resolution, controlled shutdown, repeated load/unload, bounded steady-state memory |

### Rendering conventions

Use a 2D world with +X right and +Y up, documented game units, and radians. UI
coordinates use pixels with a top-left origin; conversions go through explicit
camera/viewport functions. Render order uses a layer and stable sequence key, not
a 3D position. Rendering rotations do not imply rotated collider support.

Own the lifetimes of OpenGL buffers, vertex arrays, shaders, textures, samplers,
and framebuffers. Batch adjacent compatible sprites using atlas pages and shared
buffers. Never reorder overlapping translucent sprites merely to reduce texture
switches. Define one alpha convention and matching blend state; handle sRGB color
and linear-space blending consistently. Support pixel-aligned sampling for pixel
art and configurable filtering for other artwork.

Write a minimal loader for the exact OpenGL entry points used and validate them
with the context current. Include source paths in shader errors. Failed shader
reloads must retain the previous working program.

### Deliberately small asset formats

- Define a versioned texture format with dimensions and uncompressed RGBA8 data,
  plus an in-house writer. Generate the example artwork in-house.
- Use a bitmap-font atlas and glyph metrics with a documented ASCII subset and
  fallback glyph. General font shaping is outside scope.
- Support a documented WAV subset, initially mono/stereo 16-bit PCM. Parse chunk
  boundaries/padding correctly; reject unsupported encodings explicitly.
- Define small versioned scene, atlas, animation, and tilemap formats. Implement
  their parsers/serializers and document grammar, escaping, and limits.
- Resolve files relative to an asset root, independent of the working directory.
  Cache shared assets and preserve stable file references.
- Start with synchronous loading outside active gameplay. Provide engine-owned
  fallback texture/font data so missing files still yield useful diagnostics.

PNG/JPEG decoders, compressed audio, TrueType fonts, JSON compatibility, and complex
external formats are not first-release requirements. Any later format needs an
in-house implementation and tests.

### Collision and audio scope

Use a spatial grid for the collision broad phase with candidate deduplication and
brute-force reference tests. Define contact tolerances and stable trigger
enter/stay/exit semantics. Swept player movement must cover the supported player
shape against static walls/corners with explicit iteration limits. General dynamic
rigid bodies, joints, rotating polygon colliders, and arbitrary continuous moving
body collisions are outside scope.

Use an in-house PCM mixer with a bounded voice pool, defined clipping/gain behavior,
and conversion to the selected output format. Implement/test resampling if needed.
Validate output on the testing PC; mixer tests alone do not prove audible playback.
Missing/disconnected audio must allow continued muted play with a useful message.

## 5. Performance requirements

- Profile optimized builds on the testing PC. Record CPU, GPU, driver, resolution,
  VSync state, workload, revision, and compiler settings.
- Provisional goal: 60 FPS at 1920x1080, a 16.7 ms frame interval, in a declared
  representative scene. Establish workload budgets during implementation; OpenGL
  4.6 support is not a performance guarantee.
- Measure simulation, CPU render preparation/submission, GPU time, draw calls,
  visible sprites/tiles, collision candidates, audio voices, memory, and load time.
  Separate VSync waiting from active processing.
- Report median, p95, and p99 frame times after warm-up; store regression baselines.
- Reuse frame buffers and reserve storage. Avoid heap allocations in established
  simulation/render/audio hot paths after warm-up; track intentional exceptions.
- Cull invisible content, batch compatible draws, and update necessary data only.
  Avoid synchronous GPU readback/completion during gameplay; dedicated graphics
  tests may read back results.
- Start with straightforward buffer streaming. Add persistent mapping or more
  complex ring buffers only with measured benefit and synchronization tests.
- Stress sprites, tiles, entity counts, collision density, and audio voices
  independently. Define capacities and predictable overflow behavior.
- Repeated transitions must return live resource counts to baseline. Optimizations
  require passing correctness tests and reproducible before/after measurements.

## 6. Required tests

Write a small in-house runner with assertions, named cases, fixtures, temporary
files, fixed seeds, failure diagnostics, timeouts, and nonzero failure exit codes.
Invoke it with CTest; do not use an external test framework.

| Layer | Required coverage |
| --- | --- |
| Unit | Math/cameras, handles/storage, input edges, timing, animation, collision primitives, parsers, serialization, audio samples |
| Property/reference | Transform round trips, broad-phase versus brute-force candidates, malformed/truncated files, cache lifetime invariants |
| Integration | Scene/asset references, hierarchy, movement/triggers, save/reload, audio command ordering, startup rollback/shutdown |
| Graphics | Real OpenGL context, sprite order, alpha/color, atlas edges, clipping, camera transforms, framebuffer resize, shader failure/reload |
| Game | Scripted input, checkpoints, feature assertions, seeded replay, scene transitions/restart |
| Stress/regression | Resource bounds, repeated loads, high sprite/entity counts, dense collisions, voice limits, frame-time baselines |
| Platform/manual | Window-manager interactions, scaling, fullscreen, focus, minimize, audible output, device failure, packaged launch |

Use expected images or selected-pixel comparisons with documented tolerances and
visual diffs. Do not require bit-identical raster output across drivers. CPU tests
use simulated time without GPU/audio hardware.

Run debug, optimized, AddressSanitizer, and UndefinedBehaviorSanitizer builds; use
ThreadSanitizer where appropriate for concurrent engine code. Validate with GCC
and Clang. Hardware-dependent tests must distinguish skipped/unavailable from
passed and cannot satisfy release gates until actually run.

## 7. Required example game: Feature Lab

Build **Feature Lab**, a small top-down 2D game. The player explores a tile-based
facility, collects keys, operates switches, avoids moving hazards, and reaches an
exit. Menus, HUD, sound, transitions, saving, and diagnostics use public engine APIs.
Keep game rules in the example project.

Provide interactive play and scripted verification. The latter uses fixed seeds,
tick-indexed input, checkpoints, timeouts, and machine-readable pass/fail reports.
Compare defined simulation state rather than wall-clock time. Replay repeatability
is initially scoped to the same build/platform, not arbitrary floating-point
behavior across machines or compilers.

### Feature-to-scenario coverage

| Feature | Game scenario | Required verification |
| --- | --- | --- |
| F01 | Window exercise on title screen and during play | Startup/shutdown tests; manual resize/scaling/focus/minimize/fullscreen checks |
| F02 | Rebind movement/interact; operate menus during play | Edge/routing assertions; focus loss leaves no stuck input |
| F03 | Camera follow/zoom room with parented rotating/scaled decorations | Transform/conversion tests and camera checkpoints |
| F04 | Sprite gallery: tint, flip, atlas borders, translucent overlap, layers, offscreen objects | Image/order checks and batching/culling counters |
| F05 | Multi-layer rooms and large scrolling map | Tile location, visible-chunk and collision checks |
| F06 | Animated player, looping machinery, one-shot door | Frame timing and exactly-once completion events |
| F07 | Spawn/despawn hazards, attach carried keys, change rooms | Handle/cycle tests, entity counts, identity and transition checkpoints |
| F08 | Shared assets and deliberate missing/corrupt asset station | Cache/lifetime assertions, errors, fallback rendering, atomic reload checks |
| F09 | Walls/corners, circles, fast movement, hazards, triggers, ray-operated switches | Shape/reference tests, supported sweep prevents tunneling, layer/trigger/query assertions |
| F10 | Music, simultaneous pickups, stereo emitter, mute station | Mixer samples, voice limits, gain/loop tests, audible-output checklist |
| F11 | Title/HUD/pause menu, keyboard navigation, volume slider | Widget interaction, text/image checks, gameplay input suppression |
| F12 | Live minimap and clipped scrolling status panel | Framebuffer pixels, clipping and resize/resource checks |
| F13 | Pause, single-step, replay, induced slow frames | Tick counts, interpolation bounds, catch-up cap, repeated state checksums |
| F14 | Save checkpoint, relaunch/restore, malformed-save exercise | State round trips, interrupted-write recovery, version errors |
| F15 | Toggle diagnostics and deliberately fail shader reload | Counters/debug shapes visible, log assertions, old shader retained |
| F16 | Repeated restart and configurable stress arena | Standalone launch, resource baselines, bounded memory, capacity errors, benchmark report |

Every supported subfeature needs an explicit check under its ID. A room rendering
successfully is not enough. Maintain a coverage manifest mapping requirements to
test cases and game scenarios, including manual cases. Distinguish unimplemented,
failed, and untested items.

Exercise failures and limits as well as normal behavior. Keep fault injection in
test/development modes so ordinary play remains usable. The game complements unit
and integration tests; visual inspection alone cannot verify the entire engine.

## 8. Milestones and completion gates

| Milestone | Deliverable | Gate |
| --- | --- | --- |
| 1. Platform/test foundation | Select minimal approved system libraries, document PC, build/test harness, native window, OpenGL 4.6 Core context, audio feasibility | Dependency audit, CPU/startup/context/error/cleanup tests; viable window/audio integration |
| 2. Rendering/input | Math, camera, sprite batches, atlas/text assets, input, resize, diagnostics | Unit/graphics tests and first Feature Lab gallery |
| 3. World/content | Entities, hierarchy, tilemaps, animation, asset cache, scenes | Parser/storage/scene tests and traversable rooms |
| 4. Complete gameplay | Collision, audio, UI, minimap, persistence | Playable game and checks for F01-F16; no required stubs |
| 5. Verification/performance | Scripted runner, coverage manifest, fault scenarios, stress arena, benchmarks | Passing suites, sanitizer review, regression/coverage reports |
| 6. Release | Standalone package, API/format/build/test docs, support record | Clean offline build with installed tools; package runs outside source tree; hardware/manual and coverage gates pass |

Tests and game scenarios accompany each subsystem; milestone 5 consolidates them.
Do not expand scope while required behavior remains unstable or untested.

## 9. Proposed repository layout

Target layout; directories are added as their corresponding systems are implemented.

```text
engine/                 In-house runtime modules and public interfaces
examples/feature_lab/   Example game, scenarios, coverage manifest
assets/                 Engine-owned textures, fonts, maps, sounds, shaders
tools/                  In-house asset writers and validators
tests/                  Harness, fixtures, unit/integration/graphics tests
benchmarks/             Reproducible workloads and result schema
cmake/                  Build/platform checks; no dependency downloads
docs/                   APIs, formats, requirements, test procedures
CMakeLists.txt          Build definitions
CMakePresets.json       Debug, release, validation configurations
ENGINE_PLAN.md          This plan
```

## 10. Scope limits and remaining decisions

No 3D features will be added. Other graphics APIs, multiplayer, scripting, a plugin
system, advanced rigid-body simulation, and a full visual editor are outside the
initial scope. Gamepads, Unicode shaping, and additional asset formats can follow
with their own tests and Feature Lab scenarios. Basic UI/diagnostics are required;
a visual editor is optional and must also be written in-house if added.

The initial choices are C++20, Xlib/GLX, and ALSA. Remaining decisions include future
native Wayland support, additional hardware targets, and measured workload budgets.
Permission to use minimal installed Linux system libraries is settled.
Linux-only operation, OpenGL, 2D-only rendering, in-house engine code,
quality/performance, tests, and the comprehensive example game are confirmed.

Release requires a reproducible build, complete playable Feature Lab, coverage of
every supported feature, passing applicable tests, completed hardware checks,
documented dependencies, and performance evidence for the declared workload.
