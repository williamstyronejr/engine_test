# Release candidate acceptance handoff

**Candidate status: partial physical acceptance recorded; release signoff open.**
The [full-suite baseline evidence](../validation/2026-11-09-runtime/RESULTS.md) runs every one of
69 runtime groups in GNU Debug, GNU Release and Clang Debug. The three session
fixtures are included in each full suite. Package checks are recorded separately.
This refresh is not a new sanitizer, race-detector or offline-build qualification;
those records retain their own source revisions in [release readiness](RELEASE_READINESS.md).

The [physical follow-up](../validation/2026-11-09-door-cue/RESULTS.md) records eight
user-confirmed UI/input/save cases, both level routes, laptop-speaker observations
and a revised door cue with a successful listening retest. That asset revision has
seven focused Release checks; the earlier full-suite evidence keeps its original
revision. A later unplanned audio-device error `-5` remains unresolved.

The [audio-error investigation](../validation/2026-11-10-audio-error/RESULTS.md) adds
operation and PCM-state diagnostics for the next failure. Two bounded silent runs
do not reproduce it; 20 focused compiler/sanitizer groups pass. The cause and R5
lifecycle acceptance remain open.

The [traced interactive follow-up](../validation/2026-11-10-interactive-audio/RESULTS.md)
confirms audible door/pickup cues and normal exit without EIO. The 16-minute-46-second
session includes only about two seconds after Annex→Facility; the cause and broader
lifecycle/endurance requirements remain open.

The [longer post-transition check](../validation/2026-11-11-audio-pause/RESULTS.md)
meets its residence target and confirms cues, but reveals a brief pause pop and
five seconds of pickup static without a reported device failure. A short mixer
pause/resume fade is ready for physical retest. Static and the original EIO remain
unresolved; do not infer a clean listening pass from the healthy counters.

The tested host is Linux x86-64, Intel HD 4400, X11/GLX through XWayland, OpenGL 4.6
Core and ALSA. Kernel, compiler and driver versions are in the current evidence.
This does not certify native Wayland, another GPU or an older distribution. Shipped
system-library requirements are recorded in each archive's `RUNTIME_DEPENDENCIES.txt`;
those libraries and graphics/audio drivers are not bundled.

## Identify and launch the candidate

From the repository root, select the final merge artifact identified by the
completion record, check its SHA-256 sidecar, and extract into a new private directory:

```sh
candidate_archive="$(cat build/release/packaging/latest.txt)"
(cd "$(dirname "$candidate_archive")" && sha256sum -c "$(basename "$candidate_archive").sha256")
mkdir -p "${XDG_CACHE_HOME:-$HOME/.cache}"
acceptance_root="$(mktemp -d "${XDG_CACHE_HOME:-$HOME/.cache}/feature-lab-acceptance-XXXXXX")"
tar -xzf "$candidate_archive" -C "$acceptance_root"
candidate_dir="$acceptance_root/$(basename "$candidate_archive" .tar.gz)"
(cd "$candidate_dir" && sha256sum -c FILES.sha256)
cat "$candidate_dir/BUILD_INFO.txt"
"$candidate_dir/bin/feature_lab" --user-data "$acceptance_root/user" --no-audio
```

Record the artifact filename/hash and embedded revision before testing. `latest.txt`
can change after another package build; use the chosen artifact consistently for
the whole record. Match each evidence record to its stated revision. The current candidate includes
the revised door asset; its focused checks and listening result are separate from
the earlier full-suite baseline. Keep the private directory for logs and save/relaunch
checks. The physical follow-up used this extraction and launch approach; its record
identifies the exact earlier artifact and subsequent development-build retest.

## Outstanding observations

The table below defines the complete matrix. Consult the linked physical record
for completed subsets; unrecorded cases remain **NOT RUN**.
Use PASS, FAIL or UNAVAILABLE with a reason; absence of a failure report is not a pass.
The detailed [platform](PLATFORM_VALIDATION.md) and [audio](AUDIO_VALIDATION.md)
contracts remain authoritative for expected behavior.

| Case | Actions and expected observations | Gate |
| --- | --- | --- |
| Display and window manager | Inspect the intended resolution/scale/refresh and every advertised monitor configuration. Exercise resize, fullscreen, focus loss, actual WM minimize/restore. Check viewport/cursor mapping, clipping and readable UI. A request sent or client unmap is not proof of WM acceptance. | R4 |
| Physical input and modal UI | Use keyboard and mouse through title, New Game, settings and controls. Hold movement while opening/closing settings and changing focus; confirm no stuck movement. Capture, cancel and rebind a key/modifier combination. Switch the supported physical keyboard layout and check labels/actions. | R4 |
| Complete gameplay | Collect all three cores, finish the door and exit on Facility and Annex. Inspect animations, collisions, alarm/switch, camera/zoom, minimap and HUD. Pause/step/resume and restart. Confirm diagnostics and clipping at the chosen display scale. | R4 |
| Private persistence and shaders | Save in the private user directory, quit and relaunch with that same directory, and continue the saved state. Check settings/bindings persistence. Use F6 then F5 to observe failed shader retention and successful recovery. Record the visible result. | R4/R7 |
| Audible controls and gameplay | In an arranged listening session, verify left/right/center, mute, group pause/resume and stop, then music/effects/pickup/door cues. Record the selected ALSA route and audible artifacts; accepted frames alone are insufficient. | R5 |
| Device lifecycle | In an arranged session, exercise removable-device disconnect and system suspend/resume. Confirm supported recovery or useful terminal failure, continued silent gameplay and disabled audio controls. Preferences survive; reconnection requires a new output/session. Record actual device/error behavior. | R5 |
| Performance and endurance | The latest declared acceptance matrix retains 13 maximum-frame failures; both prior stability screens fail. Define any additional real-time endurance/interactive/audio/error-memory workload before collecting. Record physical 1080p/refresh evidence where required. Do not replace failed ceilings with p99 passes or accelerated simulation. | R6 |
| Support and signoff | Review unresolved cases and the F01–F16 coverage manifest, identify the supported configuration and candidate hash, and explicitly decide release status. Do not mark R8 complete while applicable gates remain open. | R8 |

Development probes are not installed in the archive. Use a matching source checkout
and build when their diagnostics are needed:

```sh
./build/release/platform_probe --exercise
./build/release/audio_probe --help
```

The platform probe returns 2 for unconfirmed WM requests. Retain that result.
When ready to listen, `./build/release/audio_probe --exercise` emits the documented
eight-second tone sequence through the default device; select `--device YOUR_PCM`
when testing a named route. Run the game without `--no-audio` for audible gameplay
checks. Physical disconnect and suspend/resume require coordination with the person
using the PC; the automated suite has not performed them.

## Record each result

Copy this template for every case; replace all placeholders and retain evidence
files alongside the record rather than only machine-local paths:

```text
Case ID / gate:
Status: NOT RUN | PASS | FAIL | UNAVAILABLE
Candidate source revision:
Artifact filename and SHA-256:
Tester and UTC timestamp:
Kernel, GPU/driver, display server and WM:
Monitor resolution / scale / refresh:
Keyboard layout or ALSA device / route / volume:
Exact command and private user-data directory:
Steps and expected outcome:
Observed outcome (including errors, glitches or audible artifacts):
Logs / captures / saved-state evidence:
Remaining limitation and follow-up:
```

Keep timing failures, skipped/unavailable physical checks and the existing
filesystem durability boundary visible. The [syscall failure contract](ATOMIC_SAVE_VALIDATION.md)
does not establish physical power-loss guarantees. The read-only host investigation
[did not establish a CPU-frequency cause](../benchmarks/2026-11-02-power-thermal-results.md)
and does not justify changing host settings as a release fix.
