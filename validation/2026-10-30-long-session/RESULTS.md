# Bounded repeated-session validation — 2026-10-30

Fixture implementation: **`74ddf5a`**. Evidence collection was corrected at
`451a109` and `39df835`; the fixture, engine/example code and CMake target are
unchanged across those two corrections. Exact configure/build/test commit
identities are retained in [commands](commands.json).

## Corrected compiler/sanitizer matrix

All **28 selected CTest group executions** pass with no skips. Each configuration
runs `long_session`, `levels`, `persistence`, `scene_reload`, `texture_reload`,
`audio_reload` and `renderer_memory`. Each build directory is new, with only these
selected targets compiled using two jobs. This is not a full current suite or
fresh offline qualification. [Environment](environment.log) records installed
tools and the Intel HD 4400/OpenGL 4.6/Mesa GLX stack on this host.

- debug: [test status](debug-evidence-tests.xml), [complete output](debug-full-output.log), [build](debug-build.log).
- release: [test status](release-tests.xml), [complete output](release-full-output.log), [build](release-build.log).
- clang: [test status](clang-tests.xml), [complete output](clang-full-output.log), [build](clang-build.log).
- native-sanitize: [test status](native-sanitize-tests.xml), [complete output](native-sanitize-full-output.log), [build](native-sanitize-build.log).

[Checked group names](checks.json) and [per-cycle session records](sessions.json)
are retained. Every configuration completes the same 64 alternating routes,
66,720 committed gameplay ticks, 640 sprite passes and eight real audio-worker
lifetimes. Per-level completed scene bytes and sound-bank bytes match across
all cycles and builds. Renderer payloads return to the pre-load baseline after
explicit release. Read the [fixture contract](../../docs/SESSION_VALIDATION.md)
for every assertion, fixed bound and resource-accounting exclusion.

Native ASan/UBSan runs with both `halt_on_error=1` and graphics leak detection off
under the existing driver policy. No sanitizer suppressions or timeout increases
were added. This is not leak-enabled whole-process or driver coverage. Related
CPU leak and audio race evidence remains separately scoped in the
[preceding readiness record](../2026-10-29-readiness/RESULTS.md).

The 66,720 ticks correspond to **1,112 simulated seconds** (18 minutes 32 seconds).
Elapsed execution time is recorded separately in `sessions.json`; this is an
accelerated regression, not an 18-minute wall-time soak or performance acceptance.

## Preserved development and evidence failures

The [first fixture](initial-attempt/fixture.cpp.log) attempted to write a corrupt
save through `UserStorage`, which correctly rejects invalid checksums at write
time. Its [failed test](initial-attempt/tests.log) and
[context](initial-attempt/context.json) are retained. The corrected fixture injects
corrupt bytes through `AssetRoot` within its private user-data directory, then
checks load rejection and unchanged game state. The corrected
[preflight run](preflight-output.log) passes. No engine bug or runtime fix is claimed.
The reused preflight Debug tree logged Ninja's premature-end-of-file recovery
warning and rebuilt; its cause is unestablished. The formal matrix uses new trees.

The first formal [Debug test run](debug-tests.xml) passed all seven groups. Its
successful XML output was truncated to CTest's default 1 KiB, causing the subsequent
cycle-evidence assertion to fail. A follow-up inventory query replaced temporary
`LastTest.log`, so an initial attempt to recover that output also failed. Neither
was a C++ test failure. The [capture correction](output-check-correction.json)
retains this history. Debug tests were repeated once as `debug-evidence-tests`;
the corrected collector copies full `LastTest.log` immediately after execution,
before any further CTest query. Original passing XML/logs remain separate.
The four-row matrix above counts the complete corrected records, excluding the
first Debug run and both preflights.

## Reproduction and remaining gates

Run [run_matrix.py](run_matrix.py) from the repository root with installed GCC,
Clang, CMake/Ninja, a working DISPLAY and ALSA null PCM. Create a new disk-backed
scratch directory with `compiler-tmp`, write its absolute path to
`build/long-session-refresh/scratch.txt`, and use an empty evidence directory.
The runner refuses existing command records and existing selected build trees.
`--resume-after-output-check` is narrowly reserved for the recorded initial
Debug capture failure; normal repeats should use fresh paths. Source identities,
commands, environment overrides and exit codes are explicit in the command record.

The [complete discovered inventory](release-complete-inventory.json) now has **67 runtime groups** (36 CPU, 27 hardware, four
platform), plus three Release packaging groups. Platform-disabled builds still
have 34 groups. This increment does not refresh the full 67-group matrix or
relabel the previous [66-group/offline result](../2026-10-29-offline/RESULTS.md).

Only private copied assets and private save data are edited. Audio uses a null
sink, rendering uses a hidden 64×64 GLX context and sprite target, and each cycle
has bounded route/worker waits. Full HUD/settings/tile-layer rendering, physical
input/display, audible sound/device lifecycle, wall-time endurance, timing
stability, broader memory accounting and support/signoff remain open. Background
loading stays deferred; milestone 6 is not complete.
