# Audio failure/UI session validation — 2026-10-30

Tested implementation: **`30a87f3`**. Adds `audio_ui_session_tests.cpp` and its native
CTest registration. Engine/example code and both earlier session fixtures are
unchanged from merge `7185b0a`.

## Selected validation

All **24 selected CTest group executions** pass without skips: `audio_ui_session`,
`ui_session`, `ui`, `audio_reload`, `audio_transport` and `renderer_memory` in GNU
Debug, GNU Release, Clang Debug and native GNU ASan/UBSan. Four fresh build
directories compile only the selected targets, with two jobs and sequential phases.
This is not a full runtime-suite or offline qualification.

- GNU Debug: [status](debug-tests.xml), [complete output](debug-full-output.log).
- GNU Release: [status](release-tests.xml), [complete output](release-full-output.log).
- Clang Debug: [status](clang-tests.xml), [complete output](clang-full-output.log).
- Native ASan/UBSan: [status](native-sanitize-tests.xml), [complete output](native-sanitize-full-output.log).

[Commands](commands.json), [environment](environment.log), [checks](checks.json)
and [fingerprints/workload records](sessions.json) retain the source, toolchain,
flags, durations and outcomes. Complete CTest output is copied immediately after
each test run, before another inventory command can overwrite it. The initial
uncommitted [preflight](preflight-full-output.log) passed before strengthening the
held-capture assertion and checking that the optional config read exists. The
formal matrix includes those assertions. No failing attempts, retries, sanitizer
suppressions or timeout increases were required.

Both sanitizers halt on errors; graphics leak detection remains disabled under
the existing driver policy. These runs establish no total-process/driver leak
claim. The recorded Intel HD 4400/Mesa/OpenGL 4.6 GLX environment scopes graphics
results to this host; no other GPU or distribution is qualified.

## Repeated workload

Each build completes **16 routes**, twice per level for each of four cases:
terminal failure during held checkbox input, terminal failure during binding
capture, explicitly disabled audio, and startup failure from a private damaged WAV.
Eight injected `-ENODEV` writes pass through the actual worker and disable the
soundtrack. Nonfault writes use ALSA null PCM, never a physical output device.

Every run commits **16,664 ticks**, renders **156 full passes**, and records **84
whole-frame PPM fingerprints**. Workload counts and all 84 fingerprints match
across all four configurations. The [fixture contract](../../docs/SESSION_VALIDATION.md#audio-failureui-companion-increment-78)
details disabled-widget/stale-release checks, binding cancellation, configuration
persistence, exact checkpoint restoration and explicit new-session recovery.

Disabled soundtracks have zero live/retained bank bytes and do not issue new PCM
writes after reload/configure/restore/restart requests. Fresh soundtracks restore
working audio controls and the completed game's music-only state. The fixture
preserves the current manual-recreation contract; it adds no reconnection feature.

Every pass checks all eleven owned renderer inventory fields and live counts.
Final cleanup restores a presentation-helper baseline with five textures and one
minimap target still alive until renderer destruction. These counters exclude
whole heap/RSS, driver overhead/residency and post-destruction process memory.
Full-frame fingerprints compare equivalent executions; they do not certify human
readability or cross-driver image identity. Temporary PPM files are not retained.

The 277.733 simulated seconds are accelerated; actual elapsed seconds are recorded
separately. Fixed diagnostic CPU history is synthetic. No benchmark, physical-input
or real-time endurance acceptance follows from these results.

## Reproduction and scope

Run [run_matrix.py](run_matrix.py) from the repository root with installed tools,
DISPLAY and ALSA null PCM. Set a fresh disk-backed scratch directory with a
`compiler-tmp` subdirectory in `build/audio-ui-session-refresh/scratch.txt` and use
an empty evidence directory for a repeat. The runner refuses existing command
records/build directories and tracked source changes during the matrix. It installs
no dependencies. Private copied assets and configuration files are removed by the
fixture; original assets and user data are untouched.

The [discovered Release inventory](release-complete-inventory.json) contains
**69 runtime groups** (36 CPU, 29 hardware, four platform), plus three packaging
groups. Platform-disabled inventory remains 34. Discovery is not execution; the
[full native/offline evidence](../2026-10-29-offline/RESULTS.md) remains scoped to its
older 66-group baseline. Both previous session fixtures remain unchanged.

The 64×64 GLX window stays hidden and full rendering targets 640×480 offscreen.
Logical inputs and direct production calls do not exercise physical input or all
main-loop glue. Controlled errors do not certify audible playback, physical device
disconnect/suspend/resume, or power management. Those acceptance gates, timing
stability, real-time endurance, broader memory coverage and support/signoff remain
open. The next increment returns to the declared workload's frame-time failures.
