# Integrated release-readiness refresh — 2026-10-29

The corrected matrix at **`6d0da8f`** passes **253 CTest group executions** without
skips. Engine/example/profiling and build-system sources are unchanged from merge
**`855805d`**. This increment corrects one stale test expectation and refreshes
automated evidence and documentation; it does not close the remaining release gates.

The initial Debug attempt at `1fc0903` passed 65 of 66 groups and failed one
GPU-overlay pixel assertion. Its [commands](initial-attempt/commands.json),
[log](initial-attempt/debug-tests.log) and [XML](initial-attempt/debug-tests.xml)
remain intact. The failure is not relabeled as a passing attempt.

## Failure investigation and correction

Increment 73 deliberately made diagnostics scale below the general HUD's 0.35
minimum to fit tiny viewports. The GPU timer presentation test still sampled
pixels using that old minimum. At 320×240, diagnostics now uses 0.25, placing the
first panel's lower edge at y=131.5; the stale test sampled near y=182, outside the
panel. The five real-query, slot/lifetime and ownership cases in the same group
passed before this fix.

The test now uses the expected diagnostics dimensions and checks inside, beside
and below the panel across all four viewports. It still requires a real collected
GPU sample, healthy rendering and stable resources. No timer behavior, thresholds,
timeouts, skips or renderer implementation were changed. GPU timers are included
in the refreshed native sanitizer subset as well as every full native compiler run.

## Corrected matrix

| Build / instrumentation | Passed groups | Scope |
| --- | ---: | --- |
| GNU Debug | 66 | All runtime groups |
| GNU Release | 66 | All runtime groups; package fixtures selected separately |
| Clang Debug | 66 | All runtime groups |
| CPU ASan/UBSan | 34 | Complete platform-disabled suite, leaks enabled |
| Native ASan/UBSan | 17 | Selected timer, renderer, target, reload, UI and audio groups, driver leaks disabled |
| Native audio ASan/UBSan | 2 | Audio transport/reload with leaks enabled |
| Clang TSan audio | 2 | Audio transport/reload in a new dedicated build directory |

[Commands](commands.json), [selected inventories and results](checks.json),
[environment](environment.json) and each configuration's CTest XML/logs record
what actually ran. Native suites contain 36 CPU, 26 hardware and four platform
groups. The CPU-only build intentionally omits the two CPU-labeled groups that
require platform targets. Counts are CTest groups, not individual assertions.

The 17 native sanitizer groups cover GPU timers, renderer memory, render targets,
font graphics/reload, texture reload, shaders, watched reload, scene/tilemap/animation
watchers, stress graphics/frame samples, both UI allocation probes, audio transport
and audio reload. ASan/UBSan halt on errors; only native graphics suppresses leak
detection, with separate CPU/audio leak-enabled runs. TSan uses Clang with
`-fsanitize=thread -fno-omit-frame-pointer` and halts on reports. No suppressions or
timeout increases were introduced. ALSA null/failing sinks do not prove audibility.

The existing preset build trees were reused and every required target was built
before its tests. Only the TSan directory was new. Ninja reported recovery from
premature end-of-file in existing build metadata; those warnings remain in the
build logs. The resulting rebuilds completed and their binaries were tested.
The cause of the metadata warnings is not established. This is not a fresh-build,
offline-build, filesystem-durability or full current native-sanitizer qualification.
Compilation used two jobs and disk-backed compiler/package scratch outside `/tmp`.

## Packaged documentation and entry points

The preceding merge archive at `855805d` was independently audited; its
[summary and compressed hash](package-baseline/summary.json),
[documentation findings](package-baseline/documentation.json),
[commands](package-baseline/commands.json), build identity and dependency inventory
are retained. All **1,031 relative file links** resolve within the archive, and all
**74 shell recipes** pass `bash -n`. This checks syntax and file targets, not
external URLs or Markdown heading anchors.

A private, disk-backed extraction was renamed to a path containing spaces. Eight
documented CPU entry-point invocations passed from an unrelated directory with
`DISPLAY`, `LD_LIBRARY_PATH` and `LD_PRELOAD` unset: game help, stress help,
validator help, scripted simulation verification, all-asset validation, single-scene
validation, JSON validation and the 60-frame/two-cycle CPU benchmark. Two manifest
checks bracket these commands and confirm the installed payload stayed unchanged.
Reports stayed outside the package. No interactive recipe was launched against
the user's real settings or saves.

The CPU benchmark completed 120 measured frames with stable resources; this is
a bounded sample-command check, not declared timing/memory acceptance. Package
graphics fixtures separately cover relocated scripted OpenGL gameplay, save/replay,
stress and profile paths. Interactive/default-play instructions and physical
display/audio behavior remain manual checks.

The current archive is checked again after the documentation commit. Current
inventory counts and scratch-directory recipes are updated, while the earlier
audit and offline record remain explicitly historical.

## Reproduction and remaining work

Check out the named matrix revision with installed dependencies and working GLX
`DISPLAY`, then run [run_matrix.py](run_matrix.py) from the repository root. Create
`build/readiness-refresh/package-root.txt` containing an absolute package scratch
path under a fresh directory outside the checkout, and create its sibling
`compiler-tmp` directory. The runner prepends `.tools/bin` to PATH, refuses to
overwrite `commands.json`, builds sequentially and preserves failures. Use a new
checkout/evidence location for another attempt; an existing TSan directory is also
rejected. The versioned initial attempt stays separate from corrected results.

[audit_package.py](audit_package.py) takes an archive and a new output directory;
set `TMPDIR` to the private disk-backed scratch directory before invoking it.
The archive's checksum sidecar must accompany it. Its extraction is retained for
inspection. The [readiness recipes](../../docs/RELEASE_READINESS.md) describe broader
full-native and offline follow-up checks as well as the current scope limits.

R1 still needs the full current native sanitizer scope if that qualification is
claimed. R2 has refreshed evidence for exercised schedules only. R3's clean
offline result remains at `1dfba31`; incremental builds and current package checks
do not replace it. R4/R5 physical display/audio, R6 timing and unmeasured memory
coverage, and R8 support/signoff remain open. Earlier performance/stability failures
and rejected optimizations remain unchanged. Background loading remains deferred.

## Current package verification

At documentation commit `f2773cc`, all three Release package groups pass without
skips (49.27 s). The [content audit](package-feature-inventory.json) verifies all
64 then-current evidence files and five changed documentation files byte-for-byte;
the four allocation profilers remain excluded. Exact configure/build/test commands
are in [the package record](package-feature-commands.json).

The [current archive audit](package-candidate/summary.json) verifies 1,052 relative
file links, 75 shell recipes and the same eight CPU entry points bracketed by two
manifest checks. Its compressed archive hash and build/dependency records identify
the tested artifact. Original failure and baseline records remain unchanged.

A fresh Release configure/build and all three package fixtures run again at the
local merge revision before pushing `main`; the merge archive also receives the
documentation/entry-point audit.
