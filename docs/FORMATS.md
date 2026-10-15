# Asset formats and runtime contracts

These formats and all readers/writers are implemented in-house. Files use stable
relative asset paths and persistent entity IDs; runtime handles and GPU object
names are never serialized. The public interfaces are in `assets.hpp`, `scene.hpp`,
`tilemap.hpp`, `animation.hpp`, `sound.hpp`, `audio.hpp`, and `renderer.hpp` under
`engine/include/engine`.

## Asset roots and file operations

`AssetRoot` resolves relative keys below a canonical directory. Keys are printable
ASCII, at most 240 bytes, with `/` separators. Empty components, `.`, `..`, absolute
paths, backslashes, colons, control characters, and paths resolving outside the root
are rejected. The root is trusted local project storage; this is not a sandbox
against another process concurrently replacing filesystem links.

The application accepts `--assets DIRECTORY`. Otherwise it searches relative to
`/proc/self/exe`, first `assets/` beside the executable, then
`../share/feature_lab/`. It never relies on the process working directory or silently
falls back from an invalid explicit override. CMake copies source assets into the
build directory and installs them under `share/feature_lab`.

`write_atomic` creates a unique temporary file in the destination directory, writes
and syncs it, renames it onto the destination, and syncs the directory. Readers see
either the old or complete new file. Failures before rename preserve the destination
and remove the temporary. A directory-sync failure after rename reports an error
but cannot undo the replacement; crash durability is then uncertain. Parent
directories must already exist. The utility does not create directory trees.

## ETEX texture

Binary, little-endian, exactly the following layout:

| Offset | Size | Value |
| --- | --- | --- |
| 0 | 4 bytes | ASCII `ETEX` |
| 4 | uint32 | Version, exactly `1` |
| 8 | uint32 | Width |
| 12 | uint32 | Height |
| 16 | width × height × 4 bytes | RGBA8 pixels, bottom row first, left to right |

RGB bytes represent sRGB color; alpha is linear, straight alpha. Each dimension
must be 1..4096 and the decoded pixel payload must not exceed 16 MiB. Header,
version, dimensions, arithmetic, and exact file length are checked before pixel
allocation. Trailing bytes and truncated files are errors. There is no compression,
metadata, mip chain, or implicit format conversion.

`TextureCache::load` reuses a live immutable snapshot for a path. `reload` validates
a replacement before publishing it; failure preserves the previous cache entry.
Existing readers keep their previous snapshot until they release it. The cache
holds weak references, supports collecting expired entries, and limits live keys
to 64. The 16 MiB limit is per image, not an aggregate budget for snapshots retained
by callers. `load_or_fallback` returns an in-house magenta/black checker with an
error message; strict `load` and `reload` throw instead.

GPU textures have a separate lifetime. `Renderer::upload` validates data and creates
a checked handle, up to 64 live uploads. `release` invalidates that handle; stale or
foreign handles are rejected. Upload/release happen outside an active render frame.
The renderer owns and frees all remaining uploads before its context is destroyed.
It flushes on texture changes without reordering overlapping sprites. A standalone
CPU cache reload does not automatically replace an existing GPU upload; the
coordinated Feature Lab workflow below publishes both together.

`Renderer::sprite` accepts an affine transform of a centered unit quad, linear tint,
and a UV rectangle. Reversed UV endpoints flip the image. Uploaded sprites accept
finite UV components in [-16384,16384], including negative/repeated ranges. The
built-in atlas remains available for plain quads, text, and diagnostics.

## Texture sampling

`upload(data, TextureSampling)` selects independent `min`/`mag` filters (`nearest`
or `linear`) and `u`/`v` wrapping (`clamp` or `repeat`). All defaults remain nearest
and clamp-to-edge. `sampling(handle)` returns the current descriptor;
`set_sampling(handle, descriptor)` changes it only between render passes on the
owning context/thread. Changes affect all consumers sharing the handle. Invalid
enums/handles and in-pass changes throw before flushing or altering queued draws.
The setter restores the previous GL texture binding; a GL failure restores the
previous descriptor's parameters. No upload or per-draw sampler allocation occurs.

Minification and magnification use the base image only: there are no mipmaps,
anisotropic filters, border colors or mirrored-repeat modes in this API. Repeat
wraps the entire image; it does not repeat an atlas cell. Use a separate texture
for an independently repeating tile. Nearest and linear modes retain the existing
sRGB decode, straight-alpha image and linear blending contracts. Transparent
texels should carry suitable edge RGB to avoid colored fringes when filtered.

Sampling state is per uploaded handle and survives coordinated image replacement,
including dimension changes. Recycled handles start with upload's supplied/default
state. Mixed modes in one pass use distinct handles; existing texture-change
flushes preserve sprite order. Built-in glyph/checker and render-target textures
keep fixed nearest/clamp sampling, and target_sprite UVs remain within [0,1].

The mappings use standard OpenGL [texture filtering and wrapping rules](https://wikis.khronos.org/opengl/Sampler_Object).
`TextureSampling` is runtime state and adds no fields to ETEX or scene files.

## Texture reload

`Renderer::replace_textures(span<TextureReplacement>)` borrows each payload for the
call and operates on the owning context/thread between passes. Every handle must
be live, unique and owned by this renderer. The batch is bounded to 64 entries and
64 MiB of summed decoded candidate pixels, with the usual 16 MiB/image and
4096/driver dimension limits. Empty batches are no-ops outside a pass; calls during
a pass throw even if empty, without flushing pending geometry.

All handles, ETEX dimensions, payload sizes and aggregate bounds are checked before
allocation; each upload also checks the driver dimension limit. Candidate objects
use sRGB RGBA8 and retain each live handle's sampling descriptor. Old images stay
installed until every upload succeeds. Publication
swaps object IDs without changing handle slots/serials, then deletes old objects.
Failures throw and delete staged objects without publishing any replacement.
Replacement needs no spare handle slots and does not change `live_textures()`.
The next draw through any shared handle uses the replacement; completed render
targets retain their pixels until redrawn. Render targets and built-in glyph/checker
textures are not included in this API.

Feature Lab's `TextureAssets` records original file keys, immutable snapshots,
GPU handles and each consumer's atlas constraints. F7 strictly loads all registered
files into a fresh candidate cache in sorted key order, validates dimensions against
the least common multiple of the registered grids, then submits one renderer batch.
Only after GPU success does it swap CPU snapshots/cache and increment its session
revision (initially 1). Existing external readers keep old snapshots. Startup may
use a fallback, but reload never substitutes one for a failed candidate. Original
keys remain registered so fallback images can recover through F7. No scene, map,
animation definitions, procedural textures, save formats or files are modified.

F8/`--texture-error-demo` supplies an invalid payload in the last staged replacement,
exercising validation rejection without disk writes. Failure leaves the revision
unchanged and reports retained status; F7 takes precedence if both keys are pressed.
Settings capture both keys until release. Reload is synchronous, including while
paused, with optional `--watch-assets` automation but no background loading. It is
outside the GPU render timing interval.

The 64 MiB limit counts candidate RGBA bytes, not driver overhead or all engine
memory. Old GPU objects coexist with candidates during upload; the driver may retain
in-flight work after deletion. CPU snapshots can outlive publication. File decoding
also uses a bounded encoded buffer and may decode one extra image before rejecting
the aggregate candidate limit. These bounds do not impose a global memory budget.

## Scene text

A scene begins with `scene 1` and ends with `end`. Blank lines and `#` comments are
allowed. Records have exactly the indicated fields, separated by spaces/tabs.
Strings may be bare tokens or double-quoted. Only `\"` and `\\` are recognized
quoted escapes; strings cannot contain control characters or non-ASCII bytes.
Floats are finite, locale-independent decimal numbers. IDs are unsigned 64-bit
integers; zero is reserved for an absent parent.

```text
scene 1
entity 1 "Facility" "group" 0 0 0 0 1 1
entity 20 "Player" "player" 1 -11 -3.5 0 1 1
sprite 20 "textures/white.etex" 0.6 0.6 0.04 0.85 0.7 1 30
entity 10 "Wall" "wall" 1 -4.7 -2.25 0 1 1
sprite 10 "textures/checker.etex" 0.6 3.5 0.12 0.22 0.28 1 10
collider 10 0.3 1.75
end
```

| Record | Fields after record name |
| --- | --- |
| `entity` | persistent ID, name, game-defined tag, parent ID, x, y, rotation radians, scale x, scale y |
| `sprite` | entity ID, texture key, width, height, linear r, g, b, alpha, integer layer |
| `collider` | entity ID, positive half-width, positive half-height |

Entities must precede their component records. Parent IDs may refer forward. Each
entity has one transform and at most one sprite and collider. The `tag` is game
metadata; the engine does not interpret values such as `player` or `core`.

Limits and validation:

- At most 4096 entities, hierarchy depth 64, file size 4 MiB, line size 4096 bytes,
  and name/tag length 120 bytes.
- IDs are unique and nonzero. Parent references must exist; cycles are rejected.
- Local position/rotation magnitude is at most 1,000,000; absolute scale is between
  0.0001 and 10,000. Negative scales are permitted. Computed world-matrix values
  must remain finite and within magnitude 1e12.
- Sprite dimensions and collider half-extents are positive and at most 1,000,000.
  Colors/alpha are within [0,1]; layer magnitude is at most 100,000.
- Unknown records/versions, duplicate components, invalid references, extra fields,
  data after `end`, and incomplete files are errors with source/line diagnostics.

Serialization sorts by persistent ID and uses enough decimal precision to round-trip
floats. Parsing constructs a complete candidate. `replace_scene` commits only after
successful validation and invalidates all previous runtime entity handles. Saving
uses `serialize_scene` followed by `AssetRoot::write_text`. Feature Lab gameplay
checkpoints use the separate ESAV format described below.

## Runtime entity contracts

`Entity` contains a slot, generation, and world identity. Destruction invalidates
old handles; slot reuse cannot revive them, and handles from other scenes are
rejected. Moving a scene transfers its valid handles and gives the moved-from scene
a new identity. Persistent IDs remain stable across serialization/reload.

Nodes occupy dense storage, with optional sprite/collider records. Additional
runtime component types live in scene-owned pools described below. `get` returns a
borrowed reference: structural changes can invalidate references, but valid entity
handles remain usable. Reparenting preserves the local transform, not world space.
World transforms are computed from the ancestor chain; dirty-transform caching is
not implemented yet. `sampled_world_transform(entity, callback)` accepts a pure
local-pose sampler returning `LocalTransform` for each node from child through root.
It composes complete affine matrices, preserving inherited shear and negative
scale. The sampler must not mutate the scene. Sampling allocates no storage and
leaves authored local transforms unchanged; invalid handles and nonfinite or
out-of-range final matrices are rejected, as with `world_transform`.

`each` forbids structural scene mutation (component payload edits remain allowed).
Queue deletions with `defer_destroy`, then
call `flush` outside iteration. Deleting a parent removes its subtree. Repeated
requests are deduplicated; already-removed descendants are ignored during flush.
Scene storage is thread-confined; concurrent readers/writers require external
coordination.

## Runtime component pools

`Scene::add_component<T>(entity, constructor_arguments...)` creates one `T` for a
live entity. The application defines `T`; no engine registration macro or external
library is required. A scene lazily creates at most 64 distinct type pools. Each
reserves 4096 dense `{Entity, T}` entries and a 4096-entry sparse index vector on
first successful use. Empty pools retain their storage and registry slot until
scene destruction or replacement. Lookup-only APIs never create a pool.

`try_component<T>` returns a borrowed pointer, or null for absent components,
null/stale/foreign entities. `get_component<T>` returns a reference or throws for
those cases. Mutable scenes expose mutable payloads; const scenes expose only
const payloads. `remove_component<T>` rejects invalid entities and returns false
for a missing component on a valid entity. `component_count<T>` reports live entries.
Adding a duplicate component throws; replacement is an explicit payload assignment
or remove/add operation. Pool removal swaps the last dense entry into the hole and
repairs its sparse index. Full world/generation handles prevent slot reuse from
resurrecting data belonging to a deleted entity.

`each_component<First, Rest...>(callback)` scans First's dense pool and calls
`callback(Entity, First&, Rest&...)` only when all requested components exist.
Const scenes pass const references. Put a selective type first: the query costs
O(first-pool size times requested-type count), resolves type pools once, and uses
constant-time sparse lookup per candidate. Missing/empty pools yield no calls.
Iteration order is insertion order modified by swap erasure, not persistent-ID
order. Repeated types refer to the same payload. Applications needing stable
ordering must explicitly order by persistent IDs outside the query.

Queries and `each` share a nesting-aware iteration guard. Payload updates, checked
lookups, nested queries and `defer_destroy` are allowed. Immediate entity creation,
destruction, node setters, component add/remove, flush and scene move/replacement
are rejected while either involved scene is being iterated. Callback exceptions
release their guard, but do not roll back earlier payload edits or queued deletes.
`flush` outside iteration removes queued subtrees and every attached component.
No callback may retain an entry reference across structural changes. Scene moves
transfer pools with their entity domain; the moved-from scene is empty and reusable.

Component types must be unqualified non-array objects with nonthrowing move
construction, move assignment and destruction. Default construction and copying
are not required; move-only owned resources and over-aligned types are supported.
All user constructors and special members must avoid accessing the owning scene.
Structural constructor reentry is additionally guarded. A throwing constructor
does not publish an entry or a new pool; existing entries remain intact. Once a
pool exists, queries, removal and reinsertion allocate no engine storage, although
user component constructors or callbacks may allocate. Scene structural operations
remain thread-confined. References are short-lived borrows, never persistent IDs.

`Scene::buffer_bytes` includes reserved sparse/dense pool payloads, including padding
and entity handles. It excludes hash registry/allocator overhead and allocations
owned by component values. Memory is bounded by the type count and each application's
component sizes, not by a universal byte budget.

These pools contain **runtime data only**. Scene text version 1 remains unchanged;
`serialize_scene` writes authored nodes/sprites/colliders and omits runtime pools.
Parsing returns empty pools; applications must rebuild derived state or implement
explicit versioned codecs before using custom components as persistent state.
Built-in node fields remain separate from runtime pools. RTTI identifies types
within the executable; there are no serialized type names or plugin ABI promises.
Feature Lab builds `CorePickup {slot, radius}` from core tags, queries the pool for
pickup detection and defers deletion. Save loading reconstructs the pool, then
deletes already-collected IDs. ESAV v3, ERPL v1 and ECFG v7 remain unchanged.

## Tile and animation atlases

Both formats embed a texture key and a regular atlas grid. Grid dimensions are
1..256 each; the texture's pixel dimensions must be divisible by the grid. Atlas
cells are zero-based, left to right, **bottom row first**, matching ETEX pixels.
`atlas_uv` calculates a cell's normalized rectangle. Nearest-neighbor sampling
avoids interpolation across adjacent cells. For base-level linear filtering,
`atlas_uv_inset(cell, columns, rows, width, height)` maps the quad to the cell's
outer texel centers, half a texel inside every edge. It validates the grid, cell,
image divisibility and 4096-pixel image limit. A one-texel cell collapses to a
constant UV. This prevents neighbor bleeding but slightly changes edge-texel
coverage; it is not padding/extrusion or a solution for mipmapped atlases.
Recompute the inset if a reloaded atlas changes dimensions. Existing tile and
animation helpers retain full-cell UVs and nearest sampling. Atlas packing,
arbitrary rectangles and per-frame pivots are not implemented.

## ETMP tilemap

All integers are little-endian; `f32` is an IEEE-754 binary32 bit pattern. Strings
are a uint32 byte length followed by that many bytes, without a terminator.
Fields are written consecutively with no padding:

| Field | Encoding |
| --- | --- |
| Magic, version | Four bytes `ETMP`, uint32 `1` |
| Width, height, layer count | Three uint32 values |
| Origin x, origin y, tile size | Three f32 values |
| Atlas columns, rows | Two uint32 values |
| Texture key | Length-prefixed asset key |
| Palette count | uint32 |
| Each palette entry | uint32 atlas cell, uint32 solid flag (`0` or `1`) |
| Each layer | int32 draw order, then width × height uint16 palette IDs |

Cells are row-major from the bottom-left origin. Palette ID zero means empty and
must have atlas cell zero and solid=false. Other IDs reference validated entries.
Layers must have unique, increasing order in [-100000,100000]. The example draws
tiles before sprites with the same order; sprite ties use persistent entity IDs.

Limits: dimensions 1..1024, layers 1..8, at most 1,048,576 cells across all layers,
1..256 palette entries, 4 MiB file size, finite origin coordinates within ±1e6,
and tile size 0.01..1000 world units. Tile size must also be at least eight float
epsilons times the largest world-boundary magnitude (minimum magnitude 1), so
cells remain representable. Queries search precomputed, shared float boundaries
to avoid gaps or skipped cells from decimal-size rounding. Readers reject unknown versions/flags,
invalid keys/IDs/grid references, truncated records, and trailing bytes. Counts
are bounded before allocating cell storage.

`TileMap` validates and owns immutable data. It precomputes occupancy for 16×16
chunks per layer and unions solid metadata across layers. `visit_visible` visits
only intersecting chunks, skips empty chunks, and clips iteration to the view's
cell range; counters distinguish chunks examined, cells examined, and tiles drawn.
Queries use strict rectangle overlap; touching edges and empty rectangles emit no
tiles. Partial edge chunks and maps with negative origins are supported.

`append_colliders` appends each overlapping solid cell once, regardless of how many
layers mark it solid. The caller clears/reuses its output vector and can reserve
`solid_cells()` entries. Feature Lab queries a swept player box each tick before
axis-separated collision resolution. This is a static tile grid, not a general
moving-body broad phase. Runtime tile editing, streaming, and an editor are deferred.

## EANI animation set

Uses the same byte order, length-prefixed strings, atlas indexing, and exact-file
validation as ETMP:

| Field | Encoding |
| --- | --- |
| Magic, version | Four bytes `EANI`, uint32 `1` |
| Atlas columns, rows | Two uint32 values |
| Texture key | Length-prefixed asset key |
| Clip count | uint32 |
| Each clip | Name string, uint32 loop flag (`0` or `1`), uint32 frame count |
| Each frame in that clip | uint32 atlas cell, uint32 duration in simulation ticks |

Limits: 1 MiB file, 1..64 clips with unique names, names of 1..64 printable ASCII
bytes without spaces, 1..256 frames per clip, 4096 total frames, and 1..36000 ticks
per frame. Atlas references and flags are validated before playback.

`AnimationPlayer` retains a `shared_ptr<const AnimationSet>` snapshot. Callers must
not retain a mutable alias to that snapshot. `play(name)` resets to frame zero and
unpauses; `play(name, false)` preserves state when the named clip is already active.
An unknown name throws without changing playback. `pause(true)` consumes no ticks.
At an exact frame boundary the next frame becomes active. Looping playback returns
the number of loops crossed, including advances larger than the entire clip.
One-shot playback holds the last frame and returns `completed=true` **once**, when
the last frame's duration has elapsed; restarting explicitly permits a new event.

Advancing uses integer tick arithmetic and prefix frame ends, without per-tick
iteration or allocation. Large uint64 advances do not overflow playback state.
Feature Lab advances animations once per 60 Hz simulation tick and freezes them
on game pause/completion. Clip durations therefore remain independent of render FPS.
No arbitrary frame callbacks or interpolated/skeletal animation are provided.

## Camera changes within a frame

`Renderer::set_camera` flushes queued geometry before changing projection. It does
not clear the framebuffer or reset frame statistics and is valid only between
`begin` and `end`. Feature Lab uses a clamped player-following world camera, then
switches to pixel coordinates for a fixed HUD. +/- changes world-camera height.
Camera screen coordinates have a top-left origin and downward Y; world coordinates
have upward Y. Conversion uses continuous viewport coordinates (0..width, 0..height),
not pixel-center indices. Extents reject nonpositive dimensions/heights, nonfinite
centers and overflowing/underflowing horizontal extents. The aspect ratio is
computed before scaling to avoid an unnecessary intermediate overflow.

## Offscreen render targets

`create_target(width, height)` allocates a renderer-owned framebuffer and sRGB RGBA8
color texture. `RenderTargetHandle` is a separate type from uploaded texture handles;
stale, forged, and foreign handles are rejected. Targets are main-thread resources
and are freed by `release(target)` or renderer destruction before context teardown.
They have no depth/stencil, multisampling, mip chain, or HDR storage in this version.

Call `begin(target, camera, clear)`, submit ordinary draws, and `end()` to complete
a pass. Then call `begin(window_width, window_height, camera)` and draw that target
with `target_sprite(target, model, tint, uv)`. Targets can also be composed into
other targets. Sampling an unfinished target or the current render destination
throws before issuing a draw. New/resized targets cannot be sampled until a pass
completes. A begin/end pair never nests; each begin clears its destination and resets
pass counters. Copy `stats()` before beginning another pass when totals are needed.

Inputs use linear RGB and straight alpha, as before. Transparent offscreen storage
is **premultiplied in linear space**, with RGB subsequently encoded as sRGB bytes.
The clear color follows the same rule. Uploaded sprites use straight-alpha source
over blending, while target sprites use premultiplied source over blending and
multiply their RGB by tint alpha. Both calculate destination alpha separately as
`source_alpha + destination_alpha * (1 - source_alpha)`. This prevents dark edges
and repeated alpha multiplication when compositing transparent passes. Keep tint
RGB/alpha in [0,1]. UV origin remains bottom-left; reversed endpoints flip the image.

Each dimension is 1..4096 and must fit the driver's texture limit. At most eight
targets and 64 MiB of committed color payload are allowed per renderer. `target_bytes`
counts width × height × 4; it excludes driver overhead, uploaded textures, the window,
and buffers retained by the driver for in-flight work. Resize stages the replacement
before deleting the old framebuffer/texture, so temporary storage may exceed the
committed budget by the old target's size. Existing target storage is reallocated
only when the requested dimensions change.

`resize_target` is allowed only outside a pass, preserves the handle, and validates
allocation/completeness before committing. Invalid sizes or budget failures leave
the previous size/content usable. A size-preserving resize is a no-op. A successful
resize discards old contents and requires a fresh pass before sampling. Creation,
resizing, and release during a pass are rejected. Releasing a target invalidates its
handle; subsequent slot reuse does not revive it.

`pixel` and `screenshot` read the **last completed pass**, whether window or target.
Pixel coordinates are bottom-left. PPM output is top-row first and omits alpha;
transparent-target RGB bytes retain the storage premultiplication described above.
Resizing/releasing the last read destination invalidates readback until another pass
completes. Window readback must happen before presentation. Beginning another pass
selects its framebuffer and viewport explicitly, so target dimensions never leak
into the next window pass.

## Nested clipping

`push_clip(PixelRect{x,y,width,height})` uses top-left destination pixels, independent
of the current camera. The effective clip is its intersection with the viewport and
all parent clips. Push/pop flush pending geometry before changing GL scissor state,
preserving draw order. Negative origins and clips partly outside the viewport are
supported; zero-size or disjoint intersections draw nothing. Negative sizes throw.
Intersection arithmetic uses widened integers to avoid signed-overflow errors.

At most 16 clips may be active. `pop_clip` restores the parent or disables scissoring
when the stack becomes empty. Camera changes preserve clip coordinates. Clip calls
outside a pass, underflow, and overflow throw without changing existing clip state.
`end()` rejects an unbalanced stack; callers can pop remaining clips and then end.
Every begin disables scissor before clearing the entire destination. Scissoring
limits rasterization; submitted-quad counters do not count surviving pixels.

Feature Lab uses nested clips around the minimap and a separate scrolling status
viewport. The minimap uses one target, with dimensions proportional to HUD scale
and a 2:1 overview camera. Walls are cached from immutable collision data; objective,
player, exit, and camera markers update every rendered frame. Page Up/Down scroll
within bounded content; restart returns the panel to its first row. Rendering these
overlays does not advance gameplay or animation ticks.

## WAV sound assets

WAV uses RIFF chunks with little-endian lengths, a `WAVE` form identifier, and a pad
byte after odd-length payloads. PCM byte rate and block alignment follow the channel
count, sample rate, and bit depth. See Microsoft's [RIFF format](https://learn.microsoft.com/en-us/windows/win32/xaudio2/resource-interchange-file-format--riff-)
and [WAVEFORMATEX contract](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/ns-mmeapi-waveformatex).

The in-house reader supports PCM format tag 1, **signed 16-bit mono/stereo** at
22050, 24000, 44100, or 48000 Hz. It requires one `fmt ` chunk (16 bytes, or 18 with
zero extension size) and one nonempty, frame-aligned `data` chunk. Unknown chunks
are skipped with their padding. Duplicate required chunks, truncated fields/padding,
wrong byte rate/block alignment, inconsistent RIFF size, and trailing bytes fail
with a source-qualified error. Float, extensible, compressed, RF64, and big-endian
WAV files are explicitly unsupported.

Files are limited to 32 MiB and 60 seconds. The decoder checks source geometry and
duration before allocating samples. PCM16 converts to float by dividing signed
samples by 32768. Mono stays mono; stereo remains interleaved left/right. Lower
sample rates convert once during loading using linear interpolation and rational
integer positions (no cumulative phase drift), with the final sample held at the
end. Output frame count is `ceil(input_frames * 48000 / source_rate)`. This is a
basic resampler, not a band-limited high-fidelity converter; native 48 kHz assets are
preferred. No resampling, file access, or decoding occurs in the audio worker.

`SoundData` contains 1..2,880,000 frames at 48 kHz and one or two channels. Samples
must be finite and within [-1,1]. `encode_wav` emits canonical 48 kHz PCM16 RIFF/WAVE;
quantization rounds and clamps, so +1 maps to 32767. There is no dithering.

## Audio ownership, mixing, and commands

`AudioOutput::register_sound(SoundData)` takes ownership of sample storage, validates
it, and returns a checked `SoundHandle`. Callers must abandon mutable aliases after
transferring storage. The output retains sounds until shutdown and joins its worker
before freeing them. There are at most 32 registrations / 64 MiB of float payload
per output; no individual unload or streaming API exists yet. Bank operations and
all command producers run on the owning main thread. The worker accesses only
immutable sample views published through the queue, never the bank's owner slots.

`play(handle, Playback{gain, pan, loop, group})` returns a unique `VoiceHandle` ticket
or zero on invalid input, a full queue, or device failure. A nonzero ticket means
**enqueued**, not that a voice slot was available. Up to 16 voices play together;
excess starts are dropped without stealing voices. `stop`, `pause`, `set_gain`, and
`set_pan` target that exact ticket, so stale controls cannot affect a reused slot.
`stop_all` stops voices in FIFO order without resetting master/group settings.
`set_master_gain`, `set_group_gain`, and `pause_group` control music/effects groups.

The single-producer/single-consumer ring holds 63 commands. Publication uses
release/acquire atomics; the consumer processes at most 63 commands before mixing
a 256-frame output block. Commands do not allocate or retain shared ownership in
the worker. Producer calls return false when rejected; callers can retry or report
failure. Valid but stale voice controls are discarded by the consumer and counted.
`stats()` exposes eventual counters for processed commands, started/completed/stopped
voices, dropped plays, stale controls, queue rejections, and active voices. A snapshot
is not a per-voice completion notification or an atomic multi-field transaction.

Gains are finite in [0,1]. Finite pan values clamp to [-1,1], attenuating the opposite
channel; mono feeds both channels, while stereo uses balance without crossfeed.
Voice × group × master gain multiplies samples; summed output clamps to [-1,1].
Gain setters ramp linearly over 64 output frames and produce the same samples
regardless of output block partitioning. New voices start at their requested gain.
Stop, pause, and pan changes are immediate at command boundaries; seamless stop
fades and equal-power pan are not implemented.

Looping wraps to frame zero within the same output block with no silent gap.
One-shot voices retire after their last frame. Pause freezes sample cursors but
gain ramps still follow output time. Muting is master gain zero and does **not**
freeze cursors. Direct `Mixer` calls are thread-confined and borrow validated,
immutable views; callers must keep those views alive until voices stop. The mono
span convenience overload remains for simple CPU use. Production asynchronous
playback uses output-owned sound handles, replacing the earlier borrowed-span API.

ALSA output remains nonblocking 48 kHz float stereo. Short writes preserve the
unwritten tail. Write and wait errors share handling: underruns attempt prepare;
suspension first attempts resume, backs off on temporary failure, and falls back
to prepare when resume is unsupported or otherwise fails. One bounded step returns
to the shutdown check; no block is remixed during recovery. Failed prepare and
terminal write/wait errors latch the negative device error and reject commands.

`AudioStats` additionally reports accepted `written_frames`, `underruns`,
`suspensions`, successful `recoveries`, readiness `waits`, and `device_error`.
Device reconnection is not automatic. Feature Lab logs errors, joins/releases the
output, displays `AUDIO OFF`, and retains the last observed statistics after
failure. These counters do not prove speaker output. See [audio validation and
recovery limits](AUDIO_VALIDATION.md) for exact retry semantics, injected and real
ALSA file-sink checks, and the outstanding physical device/listening matrix.

## Input and UI contracts

`InputFrame` contains logical keyboard buttons, pointer position/primary-button
edges, a signed wheel delta, and a cancellation flag. Coordinates are top-left
window pixels (x right, y down), matching UI hit testing. `consume()` clears edges,
wheel, and cancellation while preserving held state. Primary press/release positions
are retained separately so a press outside followed by a release inside in the same
tick does not become a click. Multiple events between ticks are coalesced; this is
not an ordered event stream or a double-click API. Wheel accumulation saturates at
±120 steps. Focus loss/unmap clears held input and cancels gestures without creating
a primary-button release action. Key repeat is ignored; UI keyboard changes occur
once per physical press, not continuously while held.

`Ui` is main-thread state with up to 64 widgets and nonzero IDs unique per layout.
Widgets own printable ASCII labels (at most 80 bytes); rendering uses the existing
uppercase bitmap font subset. Bounds and clip rectangles must be finite, positive,
and within ±1,000,000 pixels. Layout validates and copies before replacement,
preserves eligible focus by ID, and cancels capture. `set_value` validates normalized
[0,1] slider values and exact 0/1 checkbox values. `UiColumn` places vertical rows
with a nonnegative gap and rejects overflow without advancing.

Tab/Shift+Tab or Up/Down cycle enabled widgets that intersect the clip. Enter/Space
activate a focused button/checkbox; Left/Right adjust a slider by 0.05. The pointer
wheel adjusts the hovered slider by 0.05 per step. Press captures the topmost hit
widget; buttons/checkboxes activate only on release over that same widget. Disabled
widgets block click-through. Slider capture continues outside its bounds and clamps
to [0,1]. Focus loss or layout replacement cancels capture. Keyboard navigation
cancels an existing drag. Overlapping widgets draw and hit-test in layout order,
with the last widget on top; keyboard navigation retains declared order.

`update(input, modal)` returns fixed-capacity actions and keyboard/pointer capture
flags. Actions are coalesced to the final value per widget during that update;
button actions carry their unchanged value. The event span borrows the result
object: keep that object alive while iterating. Modal UI consumes both input domains.
`InputGate::route` suppresses captured domains and keeps held buttons suppressed
until physical release, preventing a menu-close key or drag from affecting gameplay.
Use capture on the closing tick too. `draw_ui` requires an active renderer pass with
a top-left pixel camera (`{{width/2, -height/2}, height}`); it pushes two clip levels
and restores them on success. Reserve that space in the renderer's clip stack.
UI layout can allocate; UI interaction updates do not. Text drawing still uses the
renderer’s existing dynamic formatting/submission paths.

Feature Lab's F1 panel uses modal capture and remembers the prior pause state.
Escape or Resume closes it; Restart injects one restart action and resets the
scripted route. Scripted simulation does not advance through an open menu.
Display controls are requests to the platform. Failed VSync control disables that
widget and reports unavailable; the fullscreen button does not claim an observed
window-manager state. Audio controls reflect live hotkey values on opening and
are disabled when output is unavailable. Only changed gain values enqueue commands;
mute restores the chosen master gain on unmute. Configuration persists audio gains,
mute, and requested VSync; fullscreen remains session-local.
`--settings-demo` opens the same panel at startup for visual inspection.

## Keyboard and mouse bindings

`KeyBindings::symbols` and `modifier_masks` store ten bindings in stable action order:
left, right, up, down, pause, restart, interact, mute, music volume, effects volume.
`assign` and `validate` reject unsupported symbols/masks and conflicts before publication.
Defaults are A/D/W/S/P/R/E/M/N/B. Supported keyboard symbols are lowercase
ASCII `a`–`z`, digits `0`–`9`, and the following stable X11 symbols. The CPU core
does not depend on Xlib; its allowlist and labels are in `bindings.hpp`.

| Keys | Symbols | Labels |
| --- | --- | --- |
| Grave, left/right bracket, backslash | 0x60, 0x5b, 0x5d, 0x5c | GRAVE, LBRK, RBRK, BSLS |
| Semicolon, apostrophe, comma, period, slash | 0x3b, 0x27, 0x2c, 0x2e, 0x2f | SEMI, APOS, COMMA, DOT, SLASH |
| Backspace, Home, End, Insert, Delete | 0xff08, 0xff50, 0xff57, 0xff63, 0xffff | BKSP, HOME, END, INS, DEL |
| F4, F9, F12 | 0xffc1, 0xffc6, 0xffc9 | F4, F9, F12 |
| Keypad 0–9 | 0xffb0–0xffb9 | KP0–KP9 |
| Keypad decimal, multiply, add, subtract, divide, equal | 0xffae, 0xffaa, 0xffab, 0xffad, 0xffaf, 0xffbd | KPDOT, KPMUL, KP+, KP-, KP/, KPEQ |

Additional layout punctuation is supported: `!`, quotation mark, `#`, `$`, `%`,
`&`, `(`, `)`, `*`, `:`, `<`, `>`, `?`, `@`, `^`, `{`, `|`, `}`, `~`. Labels are EXCL, QUOTE, HASH, DOLLAR, PERCENT,
AMP, LPAREN, RPAREN, STAR, COLON, LESS, GREATER, QUESTION, AT, CARET, LBRACE, PIPE,
RBRACE and TILDE. Main-row plus/minus/equal/underscore remain reserved.

International canonical symbols use direct Latin-1 values 0xA1–0xFF or
0x01000000 + Unicode code point for U+0100–U+10FFFF. The vocabulary is frozen to
Unicode 16.0.0 categories L/N/P/S (letters, numbers, punctuation, symbols).
Controls, whitespace, combining marks, format characters, surrogates, private-use,
unassigned values and noncharacters are rejected. Exact legacy X11 character
mappings normalize before capture/lookup; approximate mappings are not imported.
Unicode-encoded ASCII/Latin-1 aliases normalize to their direct representation.
ASCII capitals normalize to lowercase; non-ASCII case is preserved. No composition,
normalization of character sequences, locale case folding or text input is performed.
`KeyBindings` and persisted records require canonical values, so duplicate aliases
cannot evade conflict checks. Function/keypad/mouse identities remain distinct.

Labels for common Latin-1 characters include EACUTE, EGRAVE, ECIRC, AGRAVE,
AUML, OUML, UUML, ARING, CCEDIL, NTILDE and SHARPS. Others use uppercase hexadecimal
U+XXXX with at least four digits, at most six. Prefixes still use C+/S+/A+.
This keeps labels bounded and drawable by the bitmap font. Mapping and category
lookup use sorted, read-only tables and binary searches; no allocation or locale
query occurs during map normalization or successful validation. See [data provenance and
regeneration](KEYBOARD_DATA.md).

Other keyboard symbols are rejected, including raw uppercase ASCII, noncanonical
aliases, modifiers, dead keys and media/function symbols outside the allowlist.
Fixed arrow/menu/page/zoom controls and
F1/F2/F3/F5/F6/F7/F8/F10/F11 cannot be reassigned, even with modifiers. The
unshifted active-group base symbol is used (Shift+1 uses symbol 1 when that
key has 1 at level zero).

`canonical_keypad_symbol` normalizes the native active-group keypad symbols before
capture and lookup. KP_Insert/End/Down/Next/Left/Begin/Right/Home/Up/Prior map to
KP_0/1/2/3/4/5/6/7/8/9. KP_Delete and KP_Separator map to KP_Decimal. Canonical
symbols pass through unchanged. Dedicated arrows/navigation and main-row digits
remain distinct. Assignment and persisted records accept only canonical symbols;
raw keypad navigation aliases are rejected to prevent duplicate identities.

The backend uses cached level-zero key symbols for all four XKB groups rather
than lock-dependent text translation. Num Lock (including changes during a hold) and Caps Lock do not
change keypad identity or modifier masks; Shift still participates in chords.
Decimal and locale separator share one bindable identity. KP_Enter remains fixed
menu activation, and Num_Lock presses never occupy the raw capture slot. Keypad
PF keys, keypad Space/Tab and other specialized symbols remain unsupported.

Mouse symbols are 0x20000001 left, 0x20000002 middle, 0x20000003 right,
0x20000004 side 1, 0x20000005 side 2. X11 maps native buttons 1/2/3/8/9 to these
values; wheel buttons 4/5 remain scroll and other buttons are ignored.
Labels use LMB/MMB/RMB/SIDE1/SIDE2. Each action has one configurable input;
assigning another replaces it. Fixed aliases remain available.

`Window::set_bindings` validates the complete table before replacing its cached
keycode mapping, cancels queued input, and suppresses physically held keys/buttons
until release, including primary UI clicks. Keyboard and mouse sources for an
action are aggregated; releasing one alias does not release another.
MappingKeyboard/MappingModifier notifications refresh the X11 mapping with the
same cancellation policy. XKB map/new-keyboard notifications rebuild the bounded
4 × 256 symbol cache. Effective-group notifications and per-key-event group bits
select the active group; startup, keyboard replacement and focus-in query the
server state. Group changes apply the same cancellation policy, including modifier
and mouse holds,
and cancel Feature Lab binding capture. Same-group notifications do not interrupt
input. Per-key wrap/clamp/redirect rules preserve keys with fewer groups; missing
symbols do not fall back to a different layout. Lookup uses level zero, ASCII
case normalization and keypad normalization. No server round trip or allocation
is needed for ordinary key events or group changes. Binding serialization remains
ECFG v7: OS layout state is not a preference. Physical scan codes and text
composition are not supported.

The backend follows the [Xlib XKB specification](https://www.x.org/releases/current/doc/libX11/XKB/xkblib.html)
for effective group state and per-key group-range normalization.

Modifier masks use Shift=1, Ctrl=2, Alt=4; only bits 0–2 are valid. Zero means no
required modifiers. Left/right Shift, Control and Alt keysyms contribute to the
same bit; Caps Lock, Num Lock, Super and AltGr are not chord modifiers. Only observed,
unblocked physical modifier presses participate. Focus loss clears them, and
rebinding/mapping changes suppress held modifiers until release and a fresh press.

Identical symbol/mask pairs conflict. For a shared symbol, each mask must be a
subset or superset of every other mask. Thus E/Ctrl+E/Ctrl+Shift+E is valid, while
Ctrl+E/Shift+E is rejected transactionally. A fresh trigger chooses the matching
binding with the largest number of required modifiers. Extra modifiers are
permitted; a zero-mask binding remains a fallback. Fixed keyboard aliases bypass
chord lookup and remain accessible with any modifiers.

The selected action is latched per physical key/button until its release. Modifier
changes or repeat events never activate or switch held triggers. A base pressed
before its required modifiers must be released and pressed again. Aggregation of
latched keyboard and mouse aliases prevents one release from canceling another.
This event-driven path uses fixed arrays and no allocations.

Labels prefix C+ (Ctrl), S+ (Shift), A+ (Alt) in that order; for example C+S+E or
A+RMB. Button text scales down to fit long chord labels, preserving the layout.

`InputFrame::pressed_symbol` carries the first fresh native press per consumed
frame (key or supported mouse button), or zero. Additional presses in that interval
are ignored for capture. `pressed_modifiers` snapshots the mask for that same
press. Modifier-only presses do not occupy the capture slot; repeat generates no
fresh capture. Consumption, focus loss and modal routing clear both fields. This bounded field is for binding capture, not text input.
`InputGate::route` takes logical-action capture (regardless of device) and UI-pointer
capture separately. Both are enabled for modal gameplay routing. Pointer-only
capture also clears raw mouse symbols; it does not suppress mapped logical actions.

Feature Lab exposes ten binding buttons, Restore Defaults and Back in F1 Controls.
Left-click always operates widgets, including when assigned to a gameplay action.
Activation starts capture on a subsequent input frame. Conflicts/unsupported keys
keep capture open; Escape/focus loss cancels it and F1 closes settings. Gameplay
remains paused and captured throughout. Valid edits apply immediately; preferences
persist on settings close or normal exit.

## Persistence: ECFG v7 and ESAV v3

All integers are little-endian, floats are IEEE binary32, and booleans are u32 0/1.
Files begin with four-byte magic and u32 version (ECFG 7, ESAV 3), ending with a u32 CRC32 of all
preceding bytes (reflected polynomial 0xEDB88320, initial/final XOR 0xFFFFFFFF).
The envelope is 12..65,536 bytes; parsers reject wrong type/version, excess counts,
truncation, trailing bytes, invalid booleans, and checksum mismatches. CRC detects
accidental corruption, not malicious edits. Binary reader/writer utilities are now
public in `engine/binary.hpp`; envelope and storage APIs are in `engine/persistence.hpp`.
Schemas below belong to Feature Lab; the engine storage layer is schema-agnostic.

ECFG payload after the header is three f32 gains (master, music, effects), u32 mute,
and u32 VSync, followed by ten u32 input symbols in the binding order above, then
ten u32 modifier masks in the same order. Symbols begin at byte 28, masks at byte
68, and CRC at byte 108. Gains must be finite within [0,1]; bindings must satisfy
the symbol, mask and conflict rules above. Total ECFG v7 size is 112 bytes.
Legacy ECFG v1 is 32 bytes, omits bindings and loads with defaults. ECFG v2 is 72 bytes and retains strict lowercase
letter validation. ECFG v3 has the same size and permits only letters and mouse
symbols. Legacy validation is retained before migration; newer keyboard symbols
in v2/v3 records are rejected even with a valid checksum. Existing assignments
are preserved. ECFG v4 is 72 bytes and supports extended keys. All older schemas
before v5 load with zero modifier masks and retain their unique-symbol rule.
ECFG v5 is 112 bytes and preserves existing chords. Versions 1–5 reject keypad
bindings even with valid checksums; v6 adds canonical keypad symbols. Versions
1–6 reject the new international and punctuation symbols even with valid checksums.
ECFG v7 keeps the 112-byte layout and adds the vocabulary above. Noncanonical
character aliases are rejected instead of silently repairing saved records.
The next preference save writes v7. Other versions are rejected. Checkpoint compatibility
is described below.
Startup uses defaults on malformed/inaccessible configuration, logs the reason,
and shows a notice. No automatic repair overwrites the file. Changed preferences
are written on panel close or normal application exit. Crashes before that boundary
can lose unsaved preference changes. A failed write remains eligible for retry on
next close/exit. Audio-disabled runs retain stored preferences. Fullscreen is not
persisted because the backend only requests a window-manager state transition.

ESAV payload after the header, in order:

| Field | Representation |
| --- | --- |
| Facility identity | Three u32 CRCs: original scene text, encoded ETMP, encoded EANI |
| Player position and camera height | f32 x, y, height |
| Simulation time | u64 ticks (at most INT64_MAX) |
| State flags | u32 paused, won, door_started, door_open |
| Collected cores | u32 count (0..64), then u64 persistent IDs |
| Four animation records | Player, cores, machine, door; each u32 name length + bytes (at most 80), u64 tick position, u32 paused |
| Alarm state | u64 entry count, u32 disabled; appended in ESAV v2 |
| Random state | u64 initial seed, u64 current SplitMix64 state; appended in ESAV v3 |

Animation snapshots validate clip existence and tick range before replacement.
Loop positions are less than clip duration; non-loop positions may equal duration
for completed playback. Restoring a finished clip never emits another completion
event. Entity pointers, transient handles, GL objects, audio voice tickets, and the
scripted driver's waypoint are not serialized. Audio sample cursors are not saved.

ESAV v2 loads with seed 1 and random state advanced by floor(ticks / 60). Subsequent
saves write v3. ESAV v1 remains unsupported. Alarm count is bounded
to twice the tick count, and patrol position is derived from the saved tick count.
Restoration primes contact history to prevent a duplicate trigger enter.

A checkpoint must match all three facility fingerprints. Scene text changes,
including whitespace, invalidate compatibility; textures and sounds do not affect
this gameplay identity. There is no migration or content-remapping system yet.
Restore creates a fresh scene from installed content, maps collected IDs, validates
finite/bounded player and camera state, rejects a player inside collision geometry,
and checks door/animation/win consistency before replacing the live world. Collision
validation uses a 0.0001-unit tolerance against ordinary movement rounding. This
checkpoint schema covers Feature Lab's gameplay state, not arbitrary runtime scene
edits or user-defined components. Invalid loads leave the live world and handles
unchanged. Successful loads invalidate prior handles and set previous position equal
to current position to avoid interpolating from the old location.

`UserPaths::discover` uses absolute XDG_CONFIG_HOME and XDG_STATE_HOME values, with
HOME/.config and HOME/.local/state fallbacks, appending `feature_lab`. Relative or
empty environment values are ignored. `--user-data DIR` overrides both with absolute
DIR/config and DIR/state paths. Config is `settings.ecfg`; slots are `slot-1.esav`
through `slot-3.esav`. Missing files return no value; inaccessible, nonregular, or
oversized files report errors. Storage roots are trusted local user directories,
not an adversarial shared-filesystem sandbox. Concurrent writers use last-rename-wins;
there is no multi-process conflict resolution.

Writes validate the envelope, create missing directories, write a mode-0600 sibling
temporary, sync its contents, rename over the destination, then sync the destination
directory. Pre-rename errors preserve the old file and remove the temporary. A
post-rename sync error may report failure after the complete new file is visible.
Abrupt process exit can leave an orphan `.engine-save-*` temporary; reads ignore it
and later writes use unique names. No automatic cleanup races other live writers.
Interrupted-process tests cover both commit boundaries. Actual power loss and
filesystem faults, including newly created ancestor-directory durability, remain
untested. `AtomicWriteObserver` is an optional diagnostic/test callback at the two
commit stages; ordinary engine/game writes do not install one.

Saving through the panel replaces the selected slot and stores the pause state from
before opening the panel. A successful load stays in the panel, remembers the loaded
pause state, and restarts sound loops with matching pause/win status. Historical
pickup/door effects are suppressed. Closing resumes according to the loaded pause
state. Error notices retain the existing game. CLI load failures instead exit with
an error before opening a window. Loading disables scripted driving because that
external test driver's cursor is not gameplay state. A CLI exit save is explicit;
there is no automatic gameplay autosave. These bounded file operations run
synchronously on explicit actions/startup/shutdown and can stall the main thread;
normal simulation ticks perform no filesystem I/O. Saved audio preferences initialize mixer gains before its worker starts, so an
already-muted session is silent from the first sample. Subsequent gain changes
retain the normal 64-frame ramp.

## Collision world and queries

`CollisionWorld` accepts an entire pose snapshot per update. Bodies have unique,
nonzero application IDs, an axis-aligned box or circle, nonzero category bits,
collision mask, and moving/sensor flags. The caller integrates poses. This system
reports contacts and triggers; the separate `move_box` function continues to resolve
the supported player's swept movement against static walls. No dynamic impulse
solver, rotating collider, or moving-body continuous collision detection is provided.

Shape centers and segment endpoints must be finite within ±100,000. Half-extents
and radii are positive and at most 1,024; a circle's two half-extents must match.
Box/box uses inclusive interval overlap; circle pairs use squared distance and
circle/box uses the nearest point. Exact touching counts as contact, with no added
skin tolerance. Circle distance and ray calculations use double intermediates.
Legacy `overlaps(Rect,Rect)` retains its strict overlap semantics for wall movement.

The broad phase uses 4-unit square cells, with mathematical floor for negative
coordinates and inclusive bounds at cell edges. Memberships are sorted by cell and
body ID. A fixed bitset deduplicates candidate pairs shared by multiple cells.
Both `(a.mask & b.layer)` and `(b.mask & a.layer)` must be nonzero; pairs with both
bodies marked stationary are skipped. A body with mask zero is query-only and does
not occupy the grid. Query filters operate on category bits independently of masks.

Limits: 256 bodies, 256 memberships per participating body, 16,384 memberships total,
32,640 contact pairs, and up to 65,280 trigger events per update. Constructor reserves
all buffers (approximately 6.5 MiB of capacity per world on the current ABI); ordinary
updates do not allocate. Worlds are movable but not copyable; a moved-from world
must be reassigned before updating. Invalid input or exceeded limits throw before publishing,
leaving the previous bodies, contacts, trigger events, and counters unchanged.
Sorting gives deterministic ID order independent of input insertion order.
`stats()` reports grid memberships, unique candidate pairs, and narrow-phase tests.

A pair involving at least one sensor emits enter, then stay on subsequent overlap
snapshots, then exit when separated, filtered, made non-sensor, or removed. Events
are sorted by the pair's ascending IDs; callers must consume each update's event
span once. Returned spans remain valid until the next successful update. Reusing
an ID across adjacent snapshots declares the same identity; use a new ID for a new
body, or publish its removal first. `prime` validates/publishes a snapshot and clears
its event list while retaining current trigger history, for restart/load. Contacts
are discrete: a fast sensor can cross another body between snapshots unnoticed.

`query(shape, output, filter)` clears the caller's vector and returns sorted unique
IDs. Reserve 256 entries to avoid query allocations. It scans the bounded body set,
filters categories/ignored ID/sensors, rejects disjoint AABBs, then tests exact shapes.
`segment(start,end,filter)` uses the same filters and returns the nearest box/circle
intersection. Fraction is in [0,1], with an outward entry normal. Starting inside or
on a shape returns fraction zero and zero normal; a zero-length segment is a point
query. Equal fractions prefer the lowest ID. Shapes at the segment endpoint count.
Segment queries allocate no memory; neither query depends on moving/static flags.

Feature Lab assigns independent collision IDs for its player, patrol alarm, switch,
static walls, and closed door. The example permits at most 252 static wall boxes
(including solid tile cells) so its four reserved bodies fit the 256-body world;
larger authored maps need a different example-side partitioning strategy. Static
walls/switch are query-only. The alarm is a
radius-0.7 circle at x=-24, moving between y=-12 and -8 in a 240-tick triangle wave.
The switch at (-27,-7.5) toggles its mask through E, within 3 units and unobstructed
by wall/door boxes. Overlap queries drive the player highlight; trigger enter events
increment the alarm counter. This is a nonblocking training hazard. Reaching the
exit still requires all three cores; the switch is optional. The generated disk
texture visualizes the circle without introducing an external image decoder.

## Shader program reload

`Renderer::reload_shaders(vertex, fragment)` builds an independent candidate on the
owning GL context/thread. Call it only between render passes; calls during
`begin`/`end` throw `logic_error` without flushing or changing the active program.
Sources are string views (no terminating NUL required), each 1..65,536 bytes with no
embedded NUL. Source validation, compilation, linking, and interface failures return
`ShaderReloadResult{applied=false, revision, diagnostics}`. Resource exhaustion can
throw; candidate RAII cleanup preserves the previous program. Successful publication
increments the renderer's revision, initially 1 for its embedded fallback. There
are no throwing operations between acquiring the validated candidate and publication.

The linked program must expose exactly this active interface:

| Resource | Name | Type | Location |
| --- | --- | --- | --- |
| Vertex input | `position` | `vec2` | 0 |
| Vertex input | `texcoord` | `vec2` | 1 |
| Vertex input | `tint` | `vec4` | 2 |
| Fragment output | `output_color` | `vec4` | 0, index 0 |
| Default-block uniform | `atlas` | `sampler2D` | Reflected |
| Default-block uniform | `premultiplied` | `int` | Reflected |

Array sizes must be one, input/output components zero. No additional active inputs,
outputs, uniforms, uniform/storage blocks, atomic-counter buffers, or vertex/fragment
subroutine uniforms are accepted. Resources optimized out by the compiler are not
active and therefore cannot fulfill the contract. Reflection uses the driver's
[program resource interface](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGetProgramResource.xhtml).
The candidate sampler is initialized to texture unit zero regardless of its source
binding; every batch updates the reflected premultiplied uniform location. This
allows uniform locations to change safely between revisions.

Vertex/fragment compiler and link logs are collected, including successful warnings,
with at most 4,095 bytes per log and an explicit truncation marker. Failure details
name the stage or incompatible interface. Expected shader-compiler debug messages
are handled by these logs; API/driver high-severity messages still mark renderer
health as failed. Tests explicitly distinguish a compile rejection from a real GL
API error. Detached stage objects are deleted after building; failed candidate
programs are deleted, and a successful swap unbinds/deletes the retired program.
Texture/target handles, completed-pass readback, and render counters remain intact.

`default_vertex_shader()` and `default_fragment_shader()` expose embedded source
views. The shipped `shaders/sprite.vert` and `.frag` match those fallbacks; tests
check parity. Feature Lab reads both bounded files before building at startup/F5.
Its F6/`--shader-error-demo` submits a deliberate compiler error without writing files.
The HUD shows success/failure and the active revision; full details go to the console.
F5 takes precedence over F6 if both are pressed; settings capture both keys. Reload
is explicit by default, with optional `--watch-assets` automation described below.
It is synchronous between passes and may hitch a frame. The normal bounded fixed
clock handles the delay.

GLSL semantics remain the shader author's responsibility. Inputs use clip-space
coordinates and linear tint; straight textures use straight-alpha blending, while
render targets store premultiplied color. Keep the premultiplied branch when changing
fragment behavior. All sprite/quad/text/target passes share this program. A program
that meets the interface can still produce undesired pixels; compatibility checks
are not a shader sandbox or visual correctness proof. No save-format changes occur.

## Asynchronous GPU timing

`GpuTimer` (`gpu_timer.hpp`) owns eight slots of two timestamp query objects each.
Construct, use, and destroy it on the owning OpenGL context/thread. It is neither
copyable nor movable. A 64-bit `GL_TIMESTAMP` counter is required; other counter
widths allocate no query objects and report `supported=false` without blocking
rendering. The tested Intel/Mesa driver reports 64 bits. Narrow counters are omitted
to avoid ambiguous multi-wrap intervals during slow frames.

`begin(frame)` / `end()` enclose an interval in the GPU command stream. Tags must be
nonzero and strictly increasing, including skipped attempts. Pair every begin with
end even when begin returns false (unsupported counters or a full pool). Nested
begins, unmatched ends and collect calls inside an open scope throw `logic_error`;
bad tags throw `invalid_argument` before changing state. A scope interrupted by
an exception can be destroyed safely: timestamp markers are not active BeginQuery
blocks and need no EndQuery cleanup.

Call `collect()` between scopes. It polls the oldest pending slot's end and start
availability once, stops at the first unavailable slot, and reads values only after
both are available. Work is bounded by eight slots, with no busy loop, explicit
flush, finish, fence wait, or unavailable `GL_QUERY_RESULT` read. Uncollected slots
are never overwritten, even if the driver already has their results. Query object
names are reused after collection, and all names are deleted on destruction without
waiting for pending work. This follows OpenGL's
[asynchronous timer-query contract](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_timer_query.txt).

Returned `GpuTimingSample{frame, milliseconds}` entries are ordered by submitted tag.
The returned span is valid until the next collect or destruction. Results subtract
64-bit nanoseconds before converting to floating-point milliseconds; equal timestamps
are valid zero-duration samples. An end earlier than its start is discarded as invalid
(clock reset/wrap) and its slot freed. Collection never substitutes CPU time for a
missing GPU reading. Driver errors still flow through the renderer's GL health check.

Stats report counter width/support, pending closed scopes, submitted closed measured
scopes, completed valid samples, skipped begins, and discarded invalid samples.
Outside an open scope, `submitted = completed + invalid + pending`. An open measured
scope occupies a slot but is not counted as pending/submitted until end. Results
are delayed and may be skipped under pressure; these are not per-frame guarantees.

`GpuTimingHistory` retains 240 completed measurements without allocation. Tags must
increase and milliseconds must be finite/nonnegative; bad records preserve state.
It reports sample count, latest tag/time, mean, sorted index `floor((n-1)*0.95)` p95,
and max. `clear()` resets history and accepted tags. CPU and GPU histories are
independent because GPU results can arrive several rendered frames later.

Feature Lab collects once before rendering and measures from before the minimap
pass through completion of window submission, including HUD/settings/debug draws.
Simulation, file/shader reload, screenshot readback and present are outside the
markers. GPU scheduling delays and gaps in CPU submission can affect the interval;
it is not a pure busy-time/utilization metric or a full frame-time benchmark. F2 shows
mean/p95, sample count, age in rendered frames, pending slots, skips and invalid results.
All frames are eligible, even while gameplay is paused or the overlay is hidden;
unmapped-window iterations issue no measurement. The exit summary makes one final
nonblocking poll and reports remaining pending work rather than waiting for it.

## Diagnostics and paused stepping

`FrameHistory` (`diagnostics.hpp`) retains the last 240 finite, nonnegative wall/CPU
millisecond samples in a fixed array. Invalid records preserve history. Statistics
use only occupied samples; mean, maximum, and sorted index `floor((n-1)*0.95)` p95
are defined even for one sample. Empty history returns zero values. `clear()` drops
history; sampling and summaries allocate no heap memory. This is a main-thread API.

Feature Lab samples steady-clock start-to-start wall intervals and update/render
submission CPU duration, excluding swap wait and screenshot readback. Its rolling
window includes startup and diagnostic drawing costs; the separate exit summary
still skips 30 warm-up frames and retains up to 8,192 samples. Completed window and
minimap counters are displayed one frame later and include debug/HUD geometry.
GPU texture count, render-target color payload, and asynchronous GPU render timings
are available. Driver overhead and all-engine memory accounting are not measured.
Discarded wall-clock time is counted only during normal fixed-clock playback.

`debug_line`, `debug_rect`, and `debug_shape` (`debug_draw.hpp`) submit ordered
solid quads inside the caller's active render pass. Thickness uses world units;
camera/scissor and batching work like other quads. Lines/rectangles require finite
coordinates within +/-1,000,000 and finite positive thickness <=1,000,000. Empty or
inverted rectangles are rejected. Zero-length lines are no-ops. Shapes use collision
validation limits; circles use 32 segments. Color components must be finite.
Invalid geometry is checked before submission. Outlines have butt-ended segments,
with no joins or antialiasing beyond normal rasterization.

`CollisionWorld::bodies()` exposes the published immutable shape snapshot, including
query-only bodies. Its span is invalidated by update/prime, moving the world, or
destruction. Feature Lab draws this snapshot rather than guessing from sprite
bounds; outlines represent simulation positions, whereas moving sprites normally
interpolate. Camera bounds are inset by the outline width to remain visible.

F2/F3 toggle diagnostics/shapes independently on press edges. F10 advances one tick
only when already paused, using the ordinary game update path. Pause toggles take
precedence over stepping, and restart takes precedence over both. A running game
never gains an extra tick from F10; a won game cannot advance. Every stepped system
keeps the ordinary tick ordering. Interpolation history snaps to the stepped
position, pause remains set in checkpoints, and no save-schema change is needed.
Held/repeated presses do not step again until released/repressed; settings input
capture also applies to these keys. Audio groups remain paused, with new pickup/door
cues waiting to play when resumed. The scripted route respects these controls and
retains its waypoint while paused. Diagnostics are not serialized.

## Feature Lab content

`facility.scene` contains seven entities: root, player, exit, three cores, and a
machine. `facility.etmp` stores the 64×32 facility in three layers (floor, structure,
overhead pipes), with 6144 total cells. `facility.eani` defines idle, walking,
core pulse, machinery, and one-shot door clips. `tiles.etex` and `actors.etex` are
shared atlases; the white/checker textures also supply the transform gallery.
`sounds/pickup.wav`, `music.wav`, `machine.wav`, and `door.wav` supply in-house
procedural audio, serialized as ordinary WAV files. Music is stereo; effects are mono.

The example recognizes `player`, `exit`, `core`, and `machine` gameplay tags.
`rotor` and `pulse` tags add visual-only procedural local poses: a 240-tick turn
and sinusoidal X/Y scale multipliers (1 ± 0.25 and 1 ± 0.2 respectively).
Descendants inherit these sampled poses. Animated decoration subtrees reject
colliders and gameplay tags so rendered motion cannot diverge from collision or
pickup positions. Authored transforms remain unchanged; runtime `world_transform`
still reports the authored pose. Use `Game::render_transform` for the visual pose.
The transient previous tick is reset on restore/restart/paused step; fractional
render interpolation samples the bounded phase before composing the hierarchy,
avoiding matrix interpolation shrinkage or a full-turn wrap discontinuity.
Render alpha must be finite and in [0,1]. Integer-tick wrap phases are canonicalized
so a restored pose matches the live pose exactly even at a full turn.
Pose state derives from existing simulation ticks and content, so no ESAV/ERPL
fields or replay hash fields are added. The new scene fingerprint intentionally
rejects old-content checkpoints and replays.

Gameplay sprites with atlas animation must use the animation set's texture. Static
collider records and solid tiles block the player. The exit has a game-owned barrier until its 32-tick opening
clip emits completion, after all three cores are collected. Restart reconstructs
the scene, invalidates old handles, and resets animation, camera, and door state.

It requires one player, one exit, and 1..64 cores. The player uses a fixed 0.3-unit
collision half-extent and an identity parent world transform; static colliders must
remain axis-aligned. These are example restrictions, not restrictions on the engine's
affine hierarchy. The replay targets the shipped map and is not a general pathfinder.

Regenerate the shipped sample content explicitly with the in-house C++ writer:

```sh
./build/debug/make_assets assets
cmake --build --preset debug
```

This overwrites the generated sample files. Ordinary builds only stage the existing
files; they never regenerate or overwrite authored source content.

## Title and gameplay transitions

Feature Lab's `SceneFlow` owns the title/gameplay state, an initial checkpoint,
selected continue slot and input gate. It uses the already-loaded facility scene,
map and animation data; title transitions do not read new authored content or
replace GPU textures/targets. This is a two-screen example workflow, not a generic
multi-level streaming system.

Normal startup enters the title with no resumable session. `--play`, `--scripted`,
explicit load/save-slot options and existing demo flags enter gameplay directly.
`--verify` remains CPU-only. Both screens share renderer resources. The title skips
world/minimap rendering and simulation; its layout is prepared before the first
render even if no fixed tick has elapsed. Gameplay audio groups start paused on
title startup and retain their voices across Title/Resume.

New Game restores a startup checkpoint through `Game::restore`. Continue strictly
reads/decodes the chosen ESAV slot before that same staged restore. All game-state
validation finishes before the world is replaced; successful replacements invalidate
old entity handles. Failure retains the title, previous world, handles and pause
state and reports a visible notice plus console detail. Saves are never overwritten
by these actions. Continue availability indicates usable storage, not a verified
save; empty/corrupt slots can be retried or another slot selected.

F1 Title closes settings and preserves the underlying gameplay pause state before
suspending the session. Resume preserves entity handles and restores that state.
New/Continue reset replay and panel scroll and rebuild audio playback without old
one-shot events; Resume continues existing voices. The title's Tab/Enter/pointer
input is captured, including the transition tick, and held actions remain blocked
until physical release. At most one scene command is applied per fixed tick.
The title-flow increment did not change the persistence schemas. Current schemas
are ECFG v6 and ESAV v3.


## Replay files: ERPL v1

All fields are little-endian and packed without padding. Feature Lab owns this
schema and its simulation rules; `engine::Random`, binary readers and atomic
storage are reusable engine utilities.

| Field | Encoding |
| --- | --- |
| Magic/version | Four bytes ERPL, u32 1 |
| Simulation rules | u32 1; unknown values rejected |
| Command count | u32, 0..36,000 |
| Initial checkpoint | u32 byte count, then a complete CRC-protected ESAV record (at most 64 KiB) |
| Each command | u64 pre-update game tick; u32 held, pressed, released masks; u64 post-update state hash (28 bytes total) |
| Envelope CRC | u32 CRC32 over every preceding byte |

Maximum file size is 1 MiB. The envelope APIs accept an explicit size limit for
ERPL; their default and UserStorage limits remain 64 KiB. Counts and complete
payload length are checked before command allocation. Unknown versions, rules,
mask bits, invalid tick range, checksum failures, truncation and trailing data
fail before playback. The embedded checkpoint is semantically/content-validated
by staged Game::restore before replacing the active game.

Command vector offset is the zero-based, monotonically increasing fixed-step
index. Game ticks can repeat during pause or reset on restart, so each command
also stores its pre-update game tick. Bits 0..10 map to left, right, up, down,
zoom in, zoom out, pause, space, restart, single step and interact. Native key codes
and Key enum ordinals are not serialized. Pressed/released edges may both be set
on a short tap with held=false. Menu, pointer, audio and renderer actions are not
recorded. Every Game::update call in a recording contributes a command, including
paused or already-won steps. Menus and save loads are disabled while recording or
playing back so no unrecorded state transitions occur.

ReplayRecorder reserves all command storage at startup. Its per-step path packs
input, runs the game, and hashes mutable simulation state without allocations for
recording/hashing. Game systems retain their own existing storage behavior.
FNV-1a-64 uses offset 14695981039346656037 and multiplier 1099511628211. Values are
encoded as little-endian u64 (including zero-extended binary32 position/camera
bits and booleans), in this order: x, y, camera height, ticks, paused, won,
door started/open/completion count, alarm entries/disabled/touching, seed, random
state, core count, each core's persistent ID/collected flag, then each animation's
clip byte length/raw bytes/tick position/paused flag (player, core, machine, door).
Previous interpolated position, derived collision caches, transient handles,
textures, GPU timings and audio cursors are excluded. This is a deterministic
regression checksum, not a security or collision-proof identity.

Playback checks the tick before update and hash after update. On divergence it
reports the command index and stops; post-update divergence does not roll back the
failed step. CPU verification completes all commands; interactive playback may
stop early at Escape/frame limit. Empty files restore their checkpoint and finish
without simulation. Recordings are atomically saved only on normal exit; capacity
ends the session with a valid complete recording rather than dropping inputs.

`engine::Random` implements the [SplitMix64 algorithm](https://prng.di.unimi.it/splitmix64.c)
with a single u64 state, all seeds valid. `next()` increments modulo 2^64 by
0x9e3779b97f4a7c15, then mixes with xor-shifts 30/27/31 and multipliers
0xbf58476d1ce4e5b9 and 0x94d049bb133111eb. `advance(n)` skips n draws in constant
time; `value()` mixes the current state without advancing. This is for deterministic
simulation, not cryptography. Feature Lab advances once per 60 active simulation
ticks; the top eight output bits modulate machine brightness. Restart restores the
initial seed; checkpoints retain both seed and current state, even if a consumer
has advanced the generator independently. No third-party library is linked.

The rules version must change when input semantics, simulation behavior or hash
layout becomes incompatible. Content fingerprints reject a changed facility.
Exact floating-point hashes are tested in supported local builds; compatibility
across arbitrary machines, compiler flags or future physics implementations is not
a guarantee. Renderer/audio nondeterminism has no effect on simulation playback.


## Stress workload and resource accounting

The versioned stress workload and JSON report contract are documented in
[benchmarks/README.md](../benchmarks/README.md). `Scene`, `TileMap` and
`CollisionWorld::buffer_bytes()` expose tracked reserved buffer payloads with
explicit exclusions. `Renderer::texture_bytes()` counts uploaded RGBA8 payloads,
including dimensions changed through transactional replacement. Failed replacement
leaves both images and byte counts unchanged; release returns their counts to zero.
Built-in rendering storage and target bytes are separate. `graphics_info()` reads
GL vendor/device/version from the owning current context. These methods add no
external dependency and do not change content, save or replay schemas.

## Authored level replacement

The Feature Lab catalog contains `Level::facility` (`facility.scene` and
`facility.etmp`) and `Level::annex` (`annex.scene` and `annex.etmp`). Both share
`facility.eani` and the same resident texture bank. The scene's player transform
is the explicit entry spawn; it must lie wholly inside map bounds and outside
static/closed-door collision geometry. Each level has its own deterministic
script route and alarm/switch placement. `make_assets` regenerates both scenes
and maps; this increment leaves the original Facility files byte-for-byte intact.

`Game::replace` moves the candidate scene first. Scene move assignment rejects
active iteration in either world before modifying either one. Remaining owned
state has nonthrowing moves: map, source scene text, animation data/players,
collision state, entity handles, counters and presentation state. Old entity
handles cannot alias the new world. `Game::restore` uses the same commit path
with an optional preparation callback after checkpoint validation.

`SceneFlow::advance_level` requires a completed level, loads its successor,
resets level progress with the existing random seed, invokes resource preparation,
and publishes the candidate only after success. It captures transition-frame
input through the existing InputGate so held actions stay blocked until release.
Level changes are explicit Interact presses; the native `--level-tour` scenario
advances once automatically and stops at Annex completion. Restart stays in the
current level; New Game restores the initial session checkpoint, including its
startup level and seed. Settings remain outside Game and survive replacement.

The application's preparation callback validates all map/animation/sprite textures
against the resident bank and stages minimap wall geometry. Incoming atlas grids
must divide the resident grids, preserving the existing texture reload contract.
It does not upload or replace GPU resources. The successful commit installs the
staged minimap geometry into its existing render target and restarts soundtrack
voices/emitter placement. CPU parsing, spawn/resource validation or iteration
failure retains the previous world and presentation resources. Loading is synchronous
and temporarily holds both CPU worlds. Separate graphics banks, streaming,
cross-level inventory and multi-level replay commands are outside this increment.

ESAV v3 and ERPL v1 remain unchanged. Their existing scene/map/animation content
fingerprints resolve which installed level a record belongs to. `restore_game`
first checks the current level; on mismatch it stages the other catalog level and
requires an exact content match before applying the checkpoint and committing.
No filename comes from a save/replay record. Unknown/modified content is rejected.
A recording remains a single-level session; gameplay advancement is unavailable
while recording or playing it. ECFG v7 is unchanged.

## Asset file watching

`AssetWatcher` is an in-house Linux notification coordinator in `engine_core`.
It owns one `IN_NONBLOCK | IN_CLOEXEC` inotify descriptor and watches the immediate
parent directories of 1..128 explicitly registered asset keys. A `WatchedAsset`
combines a validated relative key with a nonzero 32-bit group mask. Duplicate keys,
invalid paths, empty registration and excess registrations are rejected. The
watcher is neither copyable nor movable and is confined to its owner's thread.
Destroying it closes the descriptor and all associated kernel watches.

Directory notifications cover ordinary writes, metadata changes, atomic rename
saves and deletion/recreation without binding to a file's old inode. `poll(now)`
reads at most four 4096-byte batches per call, using bounded storage and dispatch
over the registered keys. Unrelated filenames are ignored unless their volume
exhausts that read budget. Kernel overflow, lost watches, interrupted/failed reads
or budget saturation trigger conservative resynchronization of all groups and a
new inotify instance. Rebuilding the instance discards stale watch-descriptor events.
This path favors an extra reload over silently losing a watched change.

Once per second of supplied steady-clock time, the watcher compares bounded metadata
snapshots (device, inode, size, mode, nanosecond modification/change timestamps).
It also checks parent-directory identity and reattaches after directory/root
replacement. Missing directories or notification setup failure leave metadata
polling active and cause notification setup to retry on the next scheduled scan.
Event-triggered scans update the same baseline so the periodic scan does not repeat
an already-reported edit. Canonical containment checks prevent intentionally following
symlink targets outside the asset root; normal asset reads still perform their own
validation. This is not a filesystem security boundary against concurrent renames.

Each group has an independent 150 ms quiet deadline, reset by observed changes.
`poll` returns the mask of ready groups and clears those pending bits. The first
poll schedules all groups for an initial synchronization, covering the interval
between resource loading and watcher creation. `request_rescan()` schedules another
conservative synchronization. Supplied times must be monotonic. Filesystem events
are hints: consumers must fully validate candidates and retain working resources
on failure. A failed reload alone does not schedule another attempt; another file
change, rescan or explicit manual command is needed.

Idle notification polls do not read asset contents. Metadata scans and watch rebuilds
can perform filesystem work and allocate temporary path storage. There is no worker
thread, recursive directory enumeration, background decoding or content hashing.
Continuous edits may postpone a group's reload indefinitely. A change invisible to
both inotify and the metadata fingerprint cannot be detected. Local filesystems are
the intended development target; debounce does not guarantee consistency across
separately written files or make a concurrently edited file immutable during reads.
See the [Linux inotify contract](https://man7.org/linux/man-pages/man7/inotify.7.html)
for event coalescing, directory/inode behavior and queue limitations.

Feature Lab's optional `--watch-assets` registers the two sprite shader files as one
group, all resident texture keys as another, and `fonts/ui.efnt` as a third. `AssetReload::update` polls once per
drawable frame after input/simulation, before GPU timing and any render pass. This
also works on the title, in settings, while paused and during replay; minimized
windows defer it until drawing resumes. Ready groups call the existing full shader
or texture-bank transactions, or the strict font transaction. A manual F5/F6 or F7/F8 attempt suppresses a ready
automatic transaction in the same group/frame. Groups remain independent; this is
not one atomic shader/texture/font project transaction. Logs count automatic attempts
including failures, and report notification or metadata-fallback mode changes.

Watching is disabled by default and is not persisted in configuration. It never
modifies source assets, game checkpoints or replay commands. Reload still runs on
the render thread and can hitch a frame. Watched keys remain fixed for the session's
shared resident bank and fixed font key; arbitrary new banks, interactive scene
reload, audio reload and background loading remain unimplemented. Asset/save/replay/
configuration schemas and Linux system-library requirements are unchanged.

## Bitmap fonts (EFNT v1)

`fonts/ui.efnt` contains an engine-owned bitmap atlas and glyph metrics in one file,
so the loader cannot accidentally combine mismatched metrics and image revisions.
All integers are little-endian. No struct-memory serialization is used.

| Field | Encoding |
| --- | --- |
| Magic/version | `EFNT`, u32 `1` |
| Line height | u32, 1..512 unscaled pixels |
| Fallback character | u32, must name a visible glyph |
| Glyph count | u32, 1..95 |
| Each glyph | Eight 32-bit fields: code, x, y, width, height, signed offset_x, signed offset_y, advance |
| Atlas length | u32 byte length of the following complete ETEX v1 image |
| Atlas | Embedded ETEX v1, bottom-up RGBA8, at most 1024x1024 |

Files are capped at 4 MiB + 4096 bytes. Truncated/trailing data, unknown versions,
duplicate codes, out-of-atlas rectangles, invalid metrics and invisible fallback
coverage are rejected before a `BitmapFont` can be constructed. Glyph codes are
unique printable ASCII (32..126). Width/height are at most 256 and must both be
zero or both nonzero. Advances are 1..256; offsets are -256..256. Space is required
and must be advance-only. The fallback must have at least one nonzero-alpha texel.
Atlas rectangles may overlap, allowing shared artwork.

Rectangles use bottom-left atlas pixels. Offsets are relative to a top-left text
pen, x right and y down. There is no baseline/kerning/shaping convention implicit
in the format. `BitmapFont` owns a validated snapshot and a fixed lookup table.
`TextCursor` emits positioned glyph rectangles without allocations. LF resets x
and advances by line height; all other absent characters and non-ASCII bytes emit
one fallback glyph per byte. UTF-8 decoding, CRLF normalization, tabs and Unicode
shaping are not provided. Callers can normalize their own input beforehand.

`measure(text, scale)` returns the maximum line advance (including trailing spaces)
and line-box height, not the ink bounds. Empty text measures zero; a trailing LF
adds an empty line. Both measurement and layout accept up to 4096 bytes and finite
scales in (0,1024]. Offsets and overhangs do not change logical advance measurements.

`load_font` uses the asset-root containment and size checks and reports the source
key on failure. `load_font_or_fallback` returns the embedded font and a diagnostic
if loading/validation fails. Success clears the diagnostic. The embedded font uses
uppercase A-Z, digits, space and `%+-/:.?`; other characters use `?`. The checked-in
font uses the same artwork with narrower I, 1, period and colon metrics. The in-house
`make_assets` tool reproducibly writes it; no font/image libraries are required.

Each renderer starts with an embedded atlas font. `set_font` copies a snapshot and
uploads a replacement atlas between passes, publishing only after successful upload.
It rejects calls inside a render pass. The renderer owns this single current font;
callers may destroy their source snapshot immediately. `text` submits one sprite
per nonempty glyph, preserving order, tint, clipping, culling and offscreen alpha
through the normal batch path. Space and LF do not submit sprites. Coordinates are
world units, with the given origin at the text's top-left and world y pointing up.
Text requires an active pass even for an empty string. Button fitting uses measured
advance width and UI sizing respects the selected line height.

`font_bytes()` reports the current font's RGBA atlas payload separately from public
texture handles, `live_textures()` and `texture_bytes()`. It excludes CPU snapshots
and driver overhead. The diagnostics overlay displays font KiB. `--font-demo` shows
proportional advances, measurement, multiline text, fallback, tint and clipping in
Feature Lab. All ordinary menus/HUD labels load `fonts/ui.efnt` at startup, retaining
readable embedded text when it is missing or corrupt. Multiple simultaneously
selected fonts and additional text encodings remain deferred.

### Font reload

Feature Lab's `FontAssets` tracks the current UI font's revision, last reload
outcome, embedded/file provenance and failure diagnostic. Revision 1 describes the
renderer's initial embedded font. Startup and watched loads use `load_font`, not
`load_font_or_fallback`: once a file font has been installed, a missing or invalid
edit must not replace it with the embedded font. The renderer owns the complete
live font; the controller retains no additional CPU snapshot or texture handle.

`FontAssets::reload(renderer, assets)` reads and validates the bounded EFNT candidate
before calling `Renderer::set_font`. Atlas and metrics publish together between
passes. Successful publication increments the revision and clears the diagnostic;
read/decode/upload failures preserve the last working metrics, pixels, atlas-byte
count and revision. On startup failure the renderer's embedded font remains active.
A valid reload inside a render pass remains a programming error and throws.
Allocation failure also propagates while preserving the renderer's live font.

`AssetReload` registers font mask 4 independently of shader mask 1 and texture mask 2.
Initial synchronization, 150 ms debounce, atomic saves, deletion/recreation,
metadata recovery and the absence of a retry loop for unchanged failures apply to
fonts as well. Recovery from a missing font directory automatically reattaches its
watch. Candidate reads still enforce normal asset-root containment. Font failure
does not block the shader/texture groups; a font-only edit does not normally reload
either of them. Conservative watcher resynchronization can refresh all groups.

The controller's manual mask can suppress a ready font transaction for callers that
already invoked it that frame; Feature Lab adds no keyboard binding. The existing
`--watch-assets` option enables this third group in gameplay, title, settings, pause
and replay frames. Polling occurs before all render passes and GPU timing; minimized
windows defer updates. EFNT, save/replay/config schemas and simulation behavior are
unchanged. Font upload/decoding is synchronous and can hitch. The HUD and `[font]`
logs report the last outcome and retained source; `watched_fonts` counts automatic
attempts, including failures, and excludes the startup load.


## Window lifecycle and suspension

`Window::set_fullscreen(bool)` and `toggle_fullscreen()` send and flush EWMH
requests. `fullscreen()` reflects the last `_NET_WM_STATE` observed by `poll`, not
the requested value. State parsing is bounded to 256 atoms; missing, malformed or
oversized properties report false. `minimize()` sends and flushes an ICCCM
iconification request; its boolean reports submission only. `restore()` maps and
raises the window. The WM can ignore or override these requests. Width and height
are observed X11 pixels; `resize()` validates and requests a size but a managed
window can retain a WM-selected size.

`focused()` records logical keyboard focus. Real focus loss and unmapping clear
held keys/buttons and their action associations, cancel UI interaction, and do not
synthesize pointer clicks. Temporary `NotifyGrab`/`NotifyUngrab` focus events and
focus moving to an inferior window do not count as logical focus loss. Focus gain
refreshes the keyboard group. WM close messages must target this window and have
32-bit WM_PROTOCOLS/WM_DELETE_WINDOW payloads. `drawable()` is false after close or
unmap; `present()` is then a safe no-op. The context and GPU resources survive an
unmap/restore cycle. Mapped-but-occluded windows still count as drawable.

Feature Lab polls while unmapped with a 10 ms sleep, defers all simulation, replay
consumption, asset reload and drawing, and resets its fixed-step accumulator and
wall-clock baseline. The first restored frame receives zero hidden elapsed time;
scripted/replay modes then resume their normal one-command-per-frame behavior.
Suspension does not change the user's pause preference. Focus loss alone does not
suspend simulation, and already-playing audio continues unless separately paused
or muted. Transition logs report suspension/resume ticks; the session summary
includes the final observed fullscreen/focus flags. No save or replay schema changes.

See [platform validation](PLATFORM_VALIDATION.md) for the automated and visible
checks, asynchronous GLX buffer-resize handling in tests, protocol references and
remaining physical-monitor/WM checks.
