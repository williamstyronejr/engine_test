# Allocation profiling validation — 2026-10-24 UTC

Tested implementation: **`31861ad5e2d14387ca16f91f91b8c7f77e8eb6ec`**.
The new development-only executables share production gameplay/audio sources but
link scoped C++ allocation counters. This increment measures demand; it does not
optimize rendering/audio or change public APIs and content formats.

[Commands](commands.json) records all 31 configure/build/test/measurement invocations,
exit codes, environment overrides, UTC timestamps and durations. Existing build
directories under `build/release-validation/` were reused, with explicit compiler,
configuration and sanitizer flags and two compiler jobs. The Debug configuration
was corrected explicitly after a preliminary compiler-cache reset; final metadata
identifies Debug/Release as specified. No fresh/offline-build claim is made.
The command source revision is the clean implementation commit; documentation and
evidence were added afterward. [Environment](environment.log) and the raw gameplay
reports preserve host/display/GL context.

## Runtime checks

| Configuration | Passed groups | Scope |
| --- | ---: | --- |
| GNU 16.2.1 Debug | 61/61 | Every native runtime group, no skips; 156.05 s |
| GNU 16.2.1 Release | 61/61 | Every native runtime group, no skips; 120.62 s |
| Clang 23.1.3 Debug | 6/6 | Allocation counters, gameplay/audio allocation workloads, replay profile, audio transport/reload |
| GNU CPU ASan/UBSan | 3/3 | Allocation counters, replay profile, CPU stress; leak detection enabled |
| GNU native ASan/UBSan | 4/4 | Allocation counters, audio allocations, audio transport/reload; leak detection enabled |
| Clang ThreadSanitizer, shared runtime | 4/4 | Allocation counters, audio allocations, audio transport/reload |

The final test groups have no failures or skips. Sanitizers halt on error; no suppressions
were added. Focused sanitizer/race checks do not replace a complete current native
sanitizer matrix or prove all thread schedules. Earlier full native/offline evidence
retains its original revision and scope. Each configuration has a named build log,
test log and CTest XML, including successful-test output.

The current native inventory is 61 runtime groups (35 CPU, 22 hardware, four platform),
with three additional packaging checks in Release. CPU-only builds discover 33 groups.
The three new groups are `allocation_counters`, `gameplay_allocations` and
`audio_allocations`. Counter cases exercise all eight allocation forms, alignment,
zero-size requests, inclusive nesting, bookkeeping and multi-thread isolation.
Gameplay checks require 2,970 samples per scope, complete verified routes, sensible
counter relationships, frame totals enclosing their subscopes, and no device-audio
samples. The audio executable requires route completion, expected voice state,
nonempty worker samples and zero observed worker C++ allocations.

[Symbol inspection](symbol-check.json) retains filtered `nm -C` output: production
`feature_lab` has no profiler implementation symbols and resolves ordinary global
allocation to the standard library; the dedicated executables define the replacements.
The profiling executables are not installation targets.

## ThreadSanitizer linking and positive control

The first Clang TSan build failed to link: its static `tsan_cxx` archive and the
profiler both define global `new`/`delete`. The failed command/diagnostics remain in
`commands.json` and [tsan-build.log](tsan-build.log). No source workaround or
multiple-definition linker override was introduced.

The follow-up uses Clang's documented
[`-shared-libsan`](https://clang.llvm.org/docs/ClangCommandLineReference.html#cmdoption-clang-shared-libsan)
to link the available shared TSan runtime, with its runtime directory recorded in
RPATH. Compiler race instrumentation stays enabled. All four selected groups pass
under `TSAN_OPTIONS=halt_on_error=1`; the successful XML is
[tsan-shared-tests.xml](tsan-shared-tests.xml).
A deliberate heap race compiled with the same profiler replacements/shared runtime
must report `ThreadSanitizer: data race` and exit 66. Its
[source](tsan-positive-control.json), build/run commands and
[expected diagnostic](tsan-positive-control-run.log) are retained to verify the
chosen runtime still detects races. This expected failure is not an engine defect.

## Measurements and retained preliminary failure

All three Release repeats per level complete with identical allocation objects,
matching route hashes/ticks, stable 1050×1360 viewports and zero invalid GPU queries.
All three audio repeats complete with zero measured producer/worker C++ allocations.
Worker sample counts vary because ALSA null has no hardware clock. See the
[measurement analysis](../../benchmarks/2026-10-24-allocation-results.md) and
[scope contract](../../benchmarks/ALLOCATION_PROFILING.md); the nine JSON reports and
corresponding logs are retained here alongside their exact commands.

The first development attempt passed the counter and gameplay groups but timed out
in the audio probe: its completion predicate incorrectly expected eight starts,
copied from a different workload that restarted its two loops. This route starts
six (two loops, three pickups and a door sound). The corrected predicate derives
that count from the level, and the probe now locates the emitter as ordinary gameplay
does. [Initial output](preliminary-tests.log) and
[focused correction check](preliminary-audio-fix.log) are preserved. Those runs used
a dirty preliminary source and an unset build type after a compiler-cache reset;
they are development diagnostics, not the final Debug/Release evidence. The final
matrix and retained measurements above all identify `31861ad` with explicit configs.

No physical audible-device, display, power-loss or whole-process memory acceptance
is implied. Direct C allocation, driver/GPU storage, allocator metadata and live
heap peaks remain outside the counter contract. Timing values in instrumented
reports are contextual and are not evidence of a speedup.

## Package and documentation follow-up

At documentation/evidence commit `6ea4f73`, all three Release package checks pass
without skips (26.66 s), including relocation and OpenGL gameplay. All 53 files in
this evidence directory and both allocation documents match the archived copies
byte-for-byte. Profiling executables are not included. The package records GNU
16.2.1, Release and sanitizers off. Local ignored logs are under
`build/allocation-validation/package-*.log`.

Documentation checks resolve 160 local links, parse the shell recipe, inspect all
31 command records and six XML reports, and validate all nine raw measurements.
The failed static TSan linker output retains Ninja's trailing space verbatim;
no repository whitespace configuration was changed.
