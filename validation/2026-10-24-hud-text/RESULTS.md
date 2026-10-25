# HUD text reuse validation — started 2026-10-24 UTC

Tested runtime: **`a1fe4f3b7d50abbf9aa52f969d9d1f0424190209`**.
Increment 58 reuses caller-owned strings for world/HUD labels and retains existing
owning-string and drawing convenience APIs. The formatter preserves text, allows
lossless growth and returns views consumed synchronously by the renderer.

[Commands](commands.json) records 32 final configure/build/test/measurement and
screenshot invocations with argument arrays, environment overrides, timestamps,
durations and exit codes. All succeed. Existing build directories were reused with
explicit compiler/configuration/sanitizer flags and two build jobs; this is not a
fresh or offline build. Documentation was added after runtime configuration.
[Environment](environment.log) and raw gameplay reports retain host/toolchain/GL
details. The additional graphical sanitizer check completed on 2026-10-25 UTC.

## Runtime checks

| Configuration | Passed groups | Scope |
| --- | ---: | --- |
| GNU 16.2.1 Debug | 62/62 | All native runtime groups, no skips; 105.42 s |
| GNU 16.2.1 Release | 62/62 | All native runtime groups, no skips; 138.82 s |
| Clang 23.1.3 Debug | 7/7 | HUD text, gameplay allocations, font/audio/scene/tilemap/animation checks |
| GNU CPU ASan/UBSan | 5/5 | HUD text, scene/tilemap/animation reload, stress; leak detection enabled |
| GNU native ASan/UBSan | 3/3 | HUD text, audio allocations and transport; leak detection enabled |
| GNU graphical ASan/UBSan | 1/1 | Gameplay allocation replay; driver leak detection disabled |
| Clang ThreadSanitizer, shared runtime | 3/3 | HUD text, audio allocations and transport |

Seven named CTest XML reports retain successful-test output. No final test failed
or skipped. Sanitizers halt on error; no suppressions were added. The graphical
check exercises borrowed text views through rendering with `detect_leaks=0`;
CPU/audio checks keep `detect_leaks=1`. TSan uses the same shared-runtime linkage
validated by the [prior race positive control](../2026-10-24-allocations/RESULTS.md#threadsanitizer-linking-and-positive-control).
These focused checks do not replace a full native sanitizer matrix or cover all
thread schedules.

The native inventory is now 62 runtime groups: 36 CPU, 22 hardware and four
platform, plus three Release packaging checks. Platform-disabled builds discover
34 groups. The new `hud_text` group contains four cases for integer limits,
empty/shorter replacements, long-string growth and subsequent reuse, reload and
lock states, and repeated allocation-free formatting within reserved capacity.
Native audio tests additionally check changed gain/mute settings and an
international key chord through the caller-buffer overload.

## Before/after measurements and images

The baseline runtime is `31861ad`, identical in runtime source through previous
main `28b74a5`. [Baseline provenance](baseline-provenance.json) records its copied
executable hash, generated build metadata and the empty runtime-source diff.

Three Release runs for each level complete with identical allocation objects,
unchanged gameplay hashes/ticks, identical render totals, stable 1050×1360
viewports and zero invalid GPU queries. World/HUD submission records zero C++
allocation calls and requested bytes across each 2,970-frame measured run.
Simulation remains unchanged at 162 calls for Facility and 132 for Annex, in one
allocating sample consistent with the intentional restart. Audio's three repeats
retain zero allocations in their nonempty measured scopes.
[Allocation comparison](allocation-comparison.json) retains old/new counters;
the nine raw JSON reports and corresponding logs retain each measurement.
See [results and limitations](../../benchmarks/2026-10-24-hud-text-results.md) and
the [storage contract](../../benchmarks/HUD_TEXT.md).

[Screenshot comparison](screenshot-comparison.json) records 17 byte-identical
old/new PPM pairs, including commands, image sizes, headers, hashes and equality.
For both levels, replay frames 90/121/151/1501/3000 cover gameplay, pause, step,
restart and completion; scripted play, completion and recording add six pairs.
One shader/texture failure pair checks retained-resource labels. All 34 game logs
are retained here; full PPMs remain in ignored `build/hud-validation/`. A rendered
Facility frame was also visually inspected. This establishes equivalence for the
recorded states and viewport, not physical-monitor/DPI acceptance.

## Retained failure and scope limits

The initial full Debug build at `078f185` caught a source-compatibility regression:
`render_target_tests` still used the three-argument collision drawing convenience
overload. Commit `a1fe4f3` restores it and keeps the reusable-buffer overload for
gameplay. The original two command records and unmodified diagnostics remain in
[initial commands](initial/commands.json) and [initial build log](initial/gcc-debug-build.log).
They are separate from the final passing matrix. Ninja's trailing whitespace in
the failure log is preserved verbatim.

No speedup, lower RSS, peak-live-heap bound, physical audio/display acceptance or
whole-engine allocation-free behavior is claimed. Initial reservations, longer
labels, open settings/diagnostics, enabled-audio HUD, direct C allocation and
GPU/driver storage retain their stated exclusions. Next coverage should include
settings/diagnostics and enabled-audio HUD with remapped controls.

## Package and documentation follow-up

At documentation/evidence commit `1204ae3`, all three Release packaging checks
pass without skips (27.59 s), including relocation and OpenGL gameplay. All 91
files in this evidence directory and both new benchmark documents match the
archived copies byte-for-byte. Profiling executables are not installed. Build
metadata records GNU 16.2.1, Release, sanitizers off. Ignored logs are retained
under `build/hud-validation/package-feature-*.log`.

Documentation checks resolve 172 local links, parse the shell recipe, inspect
all 32 final command records and seven XML reports, and validate nine measurements
and 17 screenshot pairs. The recipe now explicitly builds its audio-test and
replay-fixture dependencies before invoking CTest. The initial raw build failure
retains its trailing whitespace; no repository whitespace setting was changed.
