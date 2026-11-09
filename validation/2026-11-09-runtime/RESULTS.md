# Complete current runtime qualification — 2026-11-09

**All 207 runtime group executions pass without skips** at **`4afa230`**. GNU
Debug, GNU Release and Clang Debug each execute the same complete 69-group native
inventory: 36 CPU, 29 hardware and four platform groups. All three repeated-session
fixtures (`long_session`, `ui_session`, `audio_ui_session`) are included in each
suite. These are CTest groups, not a count of individual assertions.

Runtime, example, test and build-system sources remain unchanged from merge
`54a61ba`. This increment adds the complete-suite record and a
[release acceptance handoff](../../docs/RELEASE_HANDOFF.md). It also corrects stale
audio documentation to match the already-tested behavior: preferences are retained,
while unavailable audio controls are disabled until a new output/session exists.

## Complete suite results

| Configuration | Passed / discovered groups | Skipped | Test command elapsed |
| --- | ---: | ---: | ---: |
| debug | 69 / 69 | 0 | 250.14 s |
| release | 69 / 69 | 0 | 216.74 s |
| clang | 69 / 69 | 0 | 304.53 s |

- GNU Debug: [inventory](debug-inventory.json), [XML](debug-tests.xml),
  [complete output](debug-full-output.log), [build](debug-build.log).
- GNU Release: [inventory](release-inventory.json), [XML](release-tests.xml),
  [complete output](release-full-output.log), [build](release-build.log).
- Clang Debug: [inventory](clang-inventory.json), [XML](clang-tests.xml),
  [complete output](clang-full-output.log), [build](clang-build.log).

[Commands](commands.json), [group checks](checks.json) and [environment](environment.log)
retain exact arguments, source revision, outcomes and durations. Each suite's XML
names must exactly match its discovered inventory; names also match across all
three configurations. CTest runs serially and successful full output is copied
immediately after execution, before another inventory query can overwrite it.
Packaging fixtures are explicitly excluded from this runtime count and checked
separately below. A skip would reject the run. No test failure, retry, suppression
or timeout increase was required.

The existing preset directories are reused, reconfigured and fully built with two
compiler jobs before their tests. Ninja reports recovery from premature end-of-file
in reused build metadata for: **debug**. The warnings and resulting
rebuilds are retained; their cause is unestablished. Successful rebuilt executables
are what the suites test. These are not fresh build directories or offline builds.
Compiler temporary files and package extraction use private disk-backed scratch
outside the repository and `/tmp`.

The host records Linux `7.2.15-1-omarchy`, GNU `16.2.1`, Clang `23.1.3`, Mesa
`26.2.5-arch1.1`, Intel HD 4400 and OpenGL 4.6 via GLX. This environment is newer
than the kernel in the earlier performance studies. Passing correctness here does
not repeat their timing measurements or establish that a host update resolved them.
Actual compilation/testing durations are diagnostic, not performance acceptance.

## Scope and handoff

No sanitizer, race-detector, new offline build or performance acceptance study is
rerun in this increment. The earlier native sanitizer/offline baseline and targeted session
sanitizer records remain scoped to their own revisions/environments; this full
runtime refresh does not relabel them as a fresh 69-group sanitizer matrix.

The handoff identifies the candidate artifact and build identity, verifies checksums,
uses private user data, and gives expected physical display/input/audio outcomes
plus a case-level evidence template. Its manual cases are **NOT RUN** until a
person records observations. Automated OpenGL screenshots, synthetic input,
null-device audio and accelerated session loops do not substitute for those cases.

The latest retained acceptance matrix still has 13 maximum-frame failures; both
balanced CPU-stability screens fail. The later host observation does not establish
a cause or recovery. Physical 1080p/display/input/audio, real-time endurance,
broader memory coverage and final support/signoff remain open. See
[release readiness](../../docs/RELEASE_READINESS.md) and the
[handoff](../../docs/RELEASE_HANDOFF.md) for the next actions.

## Reproduction

Use [run.py](run.py) from a clean committed repository with installed dependencies,
DISPLAY and ALSA null/file plugins. Record a new disk-backed scratch directory with
a `compiler-tmp` child in `build/runtime-qualification-refresh/scratch.txt`. For a
repeat, copy the runner to a new evidence directory, update its `root` path and
commit that declaration before running; preserve this existing record. The runner refuses existing command records
and tracked source changes between configurations. It uses the existing debug,
release and clang presets and installs no tools or dependencies.
