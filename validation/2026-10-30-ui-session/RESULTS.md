# Full gameplay/UI session validation — 2026-10-30

Tested implementation: **`eaeca04`**. The increment adds `ui_session_tests.cpp` and
its native CTest registration. Engine/example code and the original 64-cycle
`long_session` fixture are unchanged from merge `4185fb5`.

## Selected compiler/sanitizer matrix

All **24 selected CTest group executions** pass without skips: `ui_session`, `ui`,
`diagnostics`, `font_reload`, `font_graphics` and `renderer_memory` in each of
GNU Debug, GNU Release, Clang Debug and GNU native ASan/UBSan. Each configuration
uses a new build directory and compiles only those selected targets with two
jobs. These are targeted checks, not a full 68-group or fresh offline qualification.

- debug: [status](debug-tests.xml), [complete output](debug-full-output.log), [build](debug-build.log).
- release: [status](release-tests.xml), [complete output](release-full-output.log), [build](release-build.log).
- clang: [status](clang-tests.xml), [complete output](clang-full-output.log), [build](clang-build.log).
- native-sanitize: [status](native-sanitize-tests.xml), [complete output](native-sanitize-full-output.log), [build](native-sanitize-build.log).

[Commands](commands.json), [environment](environment.log), [group checks](checks.json)
and [session/fingerprint records](sessions.json) are retained. Successful CTest
XML may truncate output; the runner copies full `LastTest.log` immediately after
each run and checks every state record. No later inventory query can replace the
retained copy. The initial uncommitted [preflight](preflight-full-output.log) also
passed; it is separate from the four-row matrix. No fixture corrections, retries,
sanitizer suppressions or timeout increases were required in this increment.

Native ASan/UBSan uses `halt_on_error=1` for both sanitizers and disables graphics
leak detection under the existing policy. This does not establish leak-free
GL drivers or total process memory. Installed tools and the recorded Intel HD
4400/OpenGL 4.6/Mesa GLX stack identify this host; no other GPU/distribution or
native Wayland compatibility is inferred.

## Workload and frame checks

Every configuration completes **16 routes**, alternating Facility and Annex,
with all three cores, one door completion and a win per route. It commits
**16,664 gameplay ticks**, renders **328 full passes**, and captures **240 full
frame readbacks** (15 UI states per cycle). Workload counts and all 240
fingerprints match across the four builds. Fingerprints are deterministic 64-bit
FNV-style checks over complete PPM readbacks; temporary image files are reused
and removed, not retained as screenshot artifacts or reviewed for readability.

The exact states, modal/config assertions and renderer resource bounds are in
[the contract](../../docs/SESSION_VALIDATION.md#full-gameplayui-companion-increment-77).
It exercises production gameplay/HUD, tile layers, minimap, collision/sampling
galleries, diagnostics and settings rendering. It checks blocked modal inputs,
mute propagation, capture/cancel/rebind, config round trips, target resizing and
font rejection/repair. Logical input frames and direct production API calls do
not certify physical key routing or all main-loop controller behavior.

All eleven owned renderer memory fields and live counts are checked after every
pass; resize checks compute the exact output/minimap target payload. Final release
of test-owned game textures/output target and restoration of the embedded font
must recover the initial presentation-helper inventory. Five helper textures and
one minimap target intentionally remain until renderer destruction. This is not
a zero-resource, whole-heap, RSS, driver-memory or post-destruction memory claim.

16,664 ticks represent **277.733 simulated seconds**. Actual elapsed execution
seconds are recorded separately; the fixture is accelerated and has cooperative
audio-worker yields. Fixed diagnostic CPU history is synthetic display data,
with no GPU timing queries. None of these timings is performance acceptance.

## Reproduction and remaining scope

Run [run_matrix.py](run_matrix.py) from the repository root with installed GCC,
Clang, CMake/Ninja, DISPLAY and ALSA null PCM. Use a new disk-backed scratch path
with a `compiler-tmp` directory, record it in `build/ui-session-refresh/scratch.txt`,
and substitute an empty evidence directory for a repeat. Existing command records
and build directories are rejected. No tools/dependencies are downloaded or
installed by the runner. No tracked source edits are allowed during its matrix.

The [complete discovered inventory](release-complete-inventory.json) has **68 native
runtime groups** (36 CPU, 28 hardware, four platform), plus three Release packaging
groups. Platform-disabled builds remain at 34. Discovery is not execution. The
older [full native/offline record](../2026-10-29-offline/RESULTS.md) remains scoped
to its own baseline; the [64-cycle companion](../2026-10-30-long-session/RESULTS.md)
retains its separate evidence.

Private copied assets/config data, a hidden 64×64 window, 640×480/320×240 offscreen
targets and an ALSA null sink keep this workload bounded. Physical display/input,
audible device lifecycle, terminal audio-error presentation, real-time endurance,
host timing stability, broader memory acceptance and final support/signoff remain
open. Background loading is deferred and milestone 6 remains incomplete.

## Candidate package

At documentation commit **`c96ddde`**, all three Release package groups pass
without skips in 54.53 seconds: [log](package-feature-tests.log),
[XML](package-feature-tests.xml), [commands](package-feature-commands.json).
All 37 then-current evidence files and five changed documentation files match
archived bytes. The allocation profilers and both session test executables remain
excluded from installation.

The [candidate audit](package-candidate/summary.json) verifies 1,167 relative
file links, 77 shell recipes and eight bounded CPU commands between two successful
manifest checks, with the archive SHA-256 retained. The merge receives all three
package checks and the same complete content/documentation audit after this record
is included; final merge artifacts remain under `build/ui-session-refresh/` locally.
Packaging reuses the Release preset tree and is separate from the fresh targeted
compiler/sanitizer builds above.
