# Gameplay and audio C++ allocation profiling

`feature_lab_allocations` and `audio_allocations` are development-only executables
built when `BUILD_TESTING=ON`. They replace the executable's global C++ allocation
functions and run the same gameplay renderer or audio worker source used by the game.
Normal `feature_lab`, the engine libraries and installed packages do not include the
replacement functions or collect allocation counters. Increment 57 introduced
measurement; the later [HUD text follow-up](HUD_TEXT.md) optimizes the measured labels.
[Recorded results](2026-10-24-allocation-results.md) identify the
measured implementation, environment and repeatability limits.

```sh
cmake -S . -B build/allocations -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build/allocations --target feature_lab_allocations audio_allocations \
  allocation_profile_tests make_profile_replay -j2
ctest --test-dir build/allocations --output-on-failure \
  -R '^(allocation_counters|gameplay_allocations|audio_allocations)$'
./build/allocations/feature_lab_allocations --replay benchmarks/replays/facility.erpl \
  --profile-report build/facility-allocations.json --profile-warmup 30
./build/allocations/feature_lab_allocations --replay benchmarks/replays/annex.erpl \
  --profile-report build/annex-allocations.json --profile-warmup 30
./build/allocations/audio_allocations build/audio-allocations.json
```

Run from the checkout and use new report filenames for repeats. Gameplay needs the
same Linux/X11/OpenGL session as the normal replay profiler. Audio uses ALSA `null`,
requires the checkout's assets, produces no audible output, and does not edit ALSA
configuration. These tools are not installed. Missing devices/content, rejected
commands or incomplete workloads fail; an unavailable display may skip only the
CTest gameplay wrapper. A skip is not measurement evidence.

## Counter contract

The allocation object has schema `cpp_new_scopes_v1`. Every scope reports `samples`,
`nonzero_samples`, successful `calls`, cumulative `requested_bytes`, `max_calls` and
`max_requested_bytes` per sample. Zero samples means unmeasured. Maxima need not come
from the same sample. Counts include ordinary/array, aligned and nothrow forms, each
once. A zero-byte request counts as a call with zero requested bytes, although the
allocator receives at least one byte. Allocation failure is not a successful call;
the replacement honors `new_handler`. Deallocation is passed to `free` and does not
subtract from cumulative demand. These are not live-byte, peak-live-byte or leak
measurements; bytes allocated and freed repeatedly are counted repeatedly.

Snapshots are thread-local. Workers cannot contaminate a producer's delta. Scope
aggregation uses fixed atomic counters; snapshots and aggregation allocate nothing.
Read reports only after the measured thread has stopped (the audio worker is joined
first). Aggregates are process-lifetime and have no concurrent reset operation.
The counter tests cover all eight allocating forms, alignment/zero requests,
inclusive nesting, bookkeeping, thread isolation and aggregation after join.

Only calls reaching these replaceable C++ functions are visible. Direct `malloc`,
`calloc`, `realloc`, `mmap`, allocator caches/metadata, stack storage, GPU objects,
driver-private memory and total process RSS are outside this measurement. A C++
call from a shared library that resolves to the executable's replacement can be
counted; the result is not proof of engine-only attribution. Compiler allocation
elision, private library allocators and internal bindings can reduce visibility.
Instrumentation changes execution cost and scheduling: its timings are retained
for context, not a speed comparison with uninstrumented historical measurements.

## Gameplay scopes

The usual `feature_lab_replay_profile_v1` report gains an `allocations` member;
the existing timing schema, all command/hash checks, viewport checks, GPU query
validation and warmup rules remain in force. The six scopes use snapshots adjacent
to the existing [replay timing boundaries](REPLAY_PROFILING.md):

- `simulation`: after polling through command decoding, tick/hash verification,
  gameplay update and disabled soundtrack/reload bookkeeping.
- `minimap_submit`: GPU begin marker, camera computation and minimap pass.
- `world_submit`: window-pass begin, draw-argument construction and world geometry,
  text and camera flush. **The replay-status string is constructed in this scope.**
  It is not a pure renderer allocation count.
- `ui_submit`: screen-space HUD/status strings, text layout/submission, minimap
  composition, closed settings/diagnostics calls and end-of-pass markers.
- `present`: window buffer swap.
- `frame`: before event polling through presentation, including gaps between the
  smaller scopes. This is an enclosing total: do not add it to the subscopes.

The warmup prefix is excluded from every allocation scope, but remains executed and
verified. Profile aggregation/serialization, window-title updates and GL health
checks after presentation are excluded. Startup loading, audio, menus/settings
interaction, saves, watching and transitions are not covered by these two replays.
The same instrumented executable can produce a spike report, but this increment's
retained allocation results cover only the two declared replay workloads.

Each replay contains 3,000 frames, of which 2,970 are measured at warmup 30. Pauses,
completion screens and a restart at zero-based index 1500 are intentional. Restart
reconstructs scene/collision state and is an intentional allocation exception, not
steady simulation. The original HUD/status baseline allocates; the
[buffer-reuse follow-up](2026-10-24-hud-text-results.md) records its reduction with
the same scopes. Neither result chooses a whole-engine allocation or timing budget.

The separate [UI allocation workload](UI_ALLOCATIONS.md) extends rendering coverage
to settings, controls, diagnostics and enabled-audio labels. Its frozen offscreen
frame and one-off transitions have their own report schema; they are not added to
these replay or audio-worker totals.

## Audio scopes

The `feature_lab_audio_allocations_v1` report records build identity, the
`facility_soundtrack_null_v1` workload, 3,000 iterations, zero warmup, final game tick
and pre-join transport progress. It loads the Facility, locates the emitter, starts
the normal loops, then uses the automatic collection/exit route and the real
`Soundtrack::update` and `collect` methods. The route must finish, all expected
voices must start, and only the music loop may remain active. Queue rejection,
dropped play, stale control, disabled audio or timeout fails the run.

- `audio_producer`: each update/collection call after startup, including spatial
  gain/pan changes and pickup/door/stop commands. Game simulation is outside it.
- `audio_commands_mix`: the worker's bounded command drain, mixer call, atomic
  progress publication and block-begin bookkeeping, including initial empty blocks.
- `audio_transport`: each actual `BlockWriter::step` call through the ALSA adapter.
  The adapter's direct C-library allocations are outside the C++ counter contract.

The worker begins before clips are registered, so its samples include startup
silence. Producer initialization, decoding, bank registration, thread creation,
destruction/join and report writing are excluded. There is no worker warmup;
per-sample maxima and nonzero counts cover every instrumented worker block/step.
A passing audio probe additionally requires zero observed worker C++ allocations.
Producer counts are reported without imposing a new budget.

ALSA `null` consumes without hardware pacing. The producer sleeps 1 ms every ten
iterations to let commands drain, but scheduling still changes the number of worker
blocks and accepted frames. Worker totals include work between the pre-join stats
snapshot and shutdown, so do not equate those two boundaries. Null progress is not
physical audio timing, audibility or a real-time deadline guarantee. This workload
does not exercise reloads, device errors, suspend/recovery or all sixteen voices;
existing functional/recovery tests retain their separate coverage. Passing it does
not establish allocation freedom for every supported audio path or ALSA plugin.

## Sanitizer compatibility on the recorded host

GNU ASan/UBSan works with these replacements in the covered counter/audio checks.
Clang 23's default static TSan C++ runtime conflicts with the global replacements
at link time. The validated follow-up uses the documented shared sanitizer runtime,
not a multiple-definition override. See [validation and exact flags](../validation/2026-10-24-allocations/RESULTS.md#threadsanitizer-linking-and-positive-control).
The ordinary production audio tests also run in that follow-up.
