# Reusing HUD text storage

Increment 58 removes recurring world/HUD string allocations from the two declared
replay workloads using caller-owned strings and direct integer formatting. It does
not change the renderer, glyph layout, gameplay, content formats or allocation
measurement boundaries. [Results](2026-10-24-hud-text-results.md) compare the same
workloads against the [allocation baseline](2026-10-24-allocation-results.md).

## Storage and lifetime

The gameplay loop owns two strings, reserving 256 characters for drawing labels
and 128 for replay/record/status text before entering the frame loop. Reservations
are minimum capacities, not exact heap accounting or hard limits. Formatting clears
and reuses the string's capacity, appends text directly and uses `std::to_chars`
with stack storage sized for the integer type. The helper never truncates: longer
labels can grow the buffer, which then retains its larger capacity. This trades a
small amount of persistent storage for removal of repeated allocation demand; it
is not evidence of lower process RSS or a peak-live-heap bound.

`format_text` returns a `std::string_view` into its caller's string. Inputs must not
alias that string. The view is valid only until the string is modified or destroyed.
The game consumes each view immediately in `Renderer::text`, which generates glyph
quads synchronously and retains no text pointer. Separate status and drawing
buffers keep the argument passed to `draw` alive while the drawing buffer changes.
All formatting stays in its original frame scope; only initial reservations happen
before measurement. No cached label or invalidation state was added.

Scene, tilemap, animation, font, texture, shader and audio status helpers gain a
caller-buffer overload. Existing owning-string getters remain available and share
the formatting implementation. They can still allocate. Collision visualization
also retains its original convenience drawing overload; gameplay supplies reusable
storage explicitly. These are Feature Lab helpers, not new public engine APIs.

The map legend uses views of complete string literals instead of rebuilding nine
owning strings. Dynamic counters, revision/failure state, replay progress, alarm
labels, completion text and key-binding labels keep their existing contents.
Every label is recomputed from current values; reloads, replacements, rebinding and
audio-setting changes therefore need no cache invalidation.

## Coverage and reproduction

Use the [same allocation executables and scope contract](ALLOCATION_PROFILING.md):

```sh
cmake --preset release
cmake --build --preset release --target feature_lab feature_lab_allocations \
  audio_allocations hud_text_tests -j2
ctest --preset release -R '^(hud_text|gameplay_allocations|audio_transport)$' --output-on-failure
./build/release/feature_lab_allocations --replay benchmarks/replays/facility.erpl \
  --profile-report build/facility-hud.json --profile-warmup 30
./build/release/feature_lab_allocations --replay benchmarks/replays/annex.erpl \
  --profile-report build/annex-hud.json --profile-warmup 30
```

The four CPU `hud_text` cases check signed/unsigned integer extremes, empty/shorter
replacement text, lossless growth past the reservation, reuse after growth, and
repeated formatting without observed C++ allocations. Reload cases check ready,
success, failure, locked and replaced labels. Native audio tests check the caller-
buffer path with changed gain/mute settings and a rebound international key chord.
Existing renderer/reload tests continue to exercise the owning/convenience paths.

The validation record also compares 17 pairs of PPM screenshots from the old and
new Release game: both levels during recorded gameplay, pause, single-step, restart,
completion, ordinary scripted gameplay and recording, plus shader/texture failure
labels. Commands, binary provenance, hashes and equality results are retained;
full images remain in the ignored validation build directory. These comparisons
cover the recorded viewport and states, not every layout or physical display.

No whole-engine allocation-free claim follows. The replay workload disables device
audio and does not open settings/diagnostics, title menus, saves or asset watching.
Those paths, abnormal errors and longer future content can allocate. Restart/load,
report serialization, window-title updates, direct C-library allocations, GPU/driver
storage and allocator metadata retain their documented exclusions. Instrumented
wall times are not used to claim a speedup. Physical display/audio and declared
release workload/memory acceptance remain open.
