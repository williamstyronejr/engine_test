# Staging payload results — 2026-10-19

Increment 42 adds per-operation owned-payload peaks to the existing
[reload/transition workload](SPIKE_PROFILING.md). All three native attempts
completed with identical peaks and zero tracked staging bytes remaining after
every operation. The largest covered peak was **7,909,182 bytes (7.54 MiB)**
while staging the return to Facility. These are selected payload counts, not
complete allocator, process or driver-memory measurements.

## Collection and validation

Three sequential runs and one private-copy shader failure use clean source `10d2a91`
with GCC 16.2.1 Release (`-O3 -DNDEBUG`) on the i5-4300U / HD Graphics 4400,
Mesa 26.2.5-arch1.1, and the existing XWayland desktop. Compilation and tests
finished before collection; no attempts were retried or filtered by timings.
CPU clocks, OS/shader caches and compositor scheduling were uncontrolled.
Package temperatures at run boundaries ranged from 65 to 66 °C.
The [manifest](staging/2026-10-19/manifest.json) retains commands, exits, stdout,
stderr, durations and temperature readings for all four attempts.

Each normal run completed 3,600 frames at 1050×1360, with 30 warmup frames,
1,381 final-level ticks and final hash `3859428580428616961`. All ten event
frames, before/after hashes, levels and revisions match the independent CPU
reference. Each collected 3,599 elapsed GPU queries, leaving one pending, with
no skips or invalid samples. Audio and VSync were disabled by the workload.
Both Debug and Release passed all 54 runtime test groups without skips;
staging/profile CPU tests also passed ASan/UBSan with leak detection. Tests cover
overlap versus separate peaks, ownership transfer, exceptions, counter overflow,
partial GPU staging rollback, font-copy accounting and rejected transitions.

## Identical covered peaks across three runs

All values below are bytes. `combined` is the simultaneous peak, not CPU peak
plus GPU peak. Both remaining-byte fields are zero for every event in every run.

| Frame / operation | CPU peak | GPU texel peak | Combined peak |
| --- | ---: | ---: | ---: |
| 120 / `shader_reload` | 518 | 0 | 518 |
| 240 / `shader_reject` | 0 | 0 | 0 |
| 360 / `shader_recover` | 518 | 0 | 518 |
| 480 / `texture_reload` | 49,360 | 26,132 | 52,456 |
| 600 / `texture_reject` | 49,360 | 0 | 49,360 |
| 720 / `texture_recover` | 49,360 | 26,132 | 52,456 |
| 840 / `font_reload` | 35,624 | 16,384 | 51,968 |
| 1382 / `transition_reject` | 7,898,487 | 0 | 7,898,487 |
| 1383 / `transition` | 7,898,487 | 0 | 7,898,487 |
| 2086 / `transition` | 7,909,182 | 0 | 7,909,182 |

Texture decoding reaches its CPU peak while an encoded file overlaps decoded
payloads. That file is gone before GPU candidates reach their peak, so adding
49,360 and 26,132 would overstate the measured 52,456-byte overlap. The four
file-backed textures are reloaded; the live inventory also contains gallery
resources, which are not candidates. Texture rejection happens before upload.
The font path includes both the loaded candidate and the renderer-owned copy.

Transitions include the constructed candidate and staged minimap walls. Existing
collision accounting includes preallocated body/contact/event buffers, so the
count describes reserved capacity rather than only occupied gameplay data.
The failed Annex preflight reaches the same covered peak as the successful
Annex transition, then releases its staging before the following attempt.

The zero for the shader rejection demo means that its static source strings
own no covered buffers. It does **not** mean shader compilation uses no memory.
The [coverage contract](SPIKE_PROFILING.md#staging-memory-coverage) excludes
opaque compiler/driver allocations, allocator overhead, nested strings/hash
nodes, and transition loading/parsing intermediates. Construction failures
before an accounting point are not measured by this initial coverage.

## Timing context

Timings are retained for context, without an improvement claim or a fixed frame
budget assertion. These runs include instrumentation, a newer Mesa version than
the October 17 measurements, and uncontrolled desktop scheduling; they are not
directly comparable to those earlier attempts.

| Raw attempt | Frame median ms | Frame p99 ms | Frame max ms |
| --- | ---: | ---: | ---: |
| [Run 1](staging/2026-10-19/run-1.json) | 2.679 | 12.705 | 30.289 |
| [Run 2](staging/2026-10-19/run-2.json) | 2.758 | 12.691 | 52.892 |
| [Run 3](staging/2026-10-19/run-3.json) | 2.705 | 12.248 | 56.510 |

## Retained unexpected failure

The [invalid-shader attempt](staging/2026-10-19/unexpected-shader-failure.json)
returned exit code 1 and retained an incomplete frame-120 report. The first
event is `applied=false, verified=false`, with its compiler diagnostic and
a 296-byte covered CPU peak, zero GPU peak and zero remaining staging bytes.
Its gameplay hash and live resources are preserved. It is separate from the
normal timing attempts. Source assets were untouched; only a private build
directory copy contained the injected shader error.

Next: extend accounting through transition loading/parsing and malformed-content
failures before candidate construction completes. Broader physical hardware and
driver coverage remain outstanding; asset tooling and background loading stay deferred.
