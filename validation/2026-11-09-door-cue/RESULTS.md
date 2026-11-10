# Physical acceptance and door-cue follow-up — 2026-11-09

**The original door cue failed the laptop-speaker listening check. A revised cue is
implemented at `fd93752`; seven focused Release groups pass, and the user confirms
the revised cue is audible on laptop speakers. A later audio-device failure remains open.**
No full release signoff or new full compiler/sanitizer qualification is claimed.

## Original candidate and observations

The user exercised packaged candidate `257590d18fcab4f1f7bd650d13b25ec0ffefc9f7`,
archive SHA-256 `f13f94448658a28d20c7764b17117454bbc858e35848143e68c265c986232ce9`.
The archive checksum and extracted file manifest passed before launch. Runs used a
new private user-data directory, retained across relaunches. [Prepared identity](prepared.json)
and the per-run records preserve exact commands and timestamps.

The user reported these eight scoped checks passed: title readability/click alignment,
physical movement, held-key release through F1 settings, fullscreen round trip,
binding capture/cancel and Ctrl+J movement, F6/F5 visible shader failure/recovery,
slot-1 position/progress restoration after normal quit/relaunch, and retained Ctrl+J.
[Observations](observations.json) retain exact replies, report-receipt timestamps,
limits and the original failure. They are user reports, not automated observations.

[The silent gameplay log](relaunch-2.log) confirms Facility completion and transition
to Annex. [The audible run](annex-audio-1.log) subsequently confirms Annex completion
and transition back to Facility. These logs establish route completion; they do not
independently certify each requested visual, pause or single-step observation.
The user reported no other gameplay problem. The observed final window viewport
was 2122×1360; physical monitor resolution, scale and refresh remain unrecorded.

The [eight-phase default-device probe](audio-probe-1.log) exited zero with no device
error, underruns or dropped controls. The user confirmed both left/right tones on
**laptop speakers**. The other tone phases, hardware identity and system mixer volume
have not received a separate physical report. Accepted PCM is not proof of audibility.

## Retained failure and diagnosis

The user first reported: “I hear sounds for core, but theres no sound for the door.”
After clarifying that the cue starts when the **third core is collected**, rather
than when E advances through the exit, the user retried and still heard only pickup.
This remains a failed physical check on the original candidate.

The installed WAV matches the repository and contains nonzero samples. A bounded
[diagnostic using the production Soundtrack](door-diagnostic-source.cpp.log) runs
both complete routes against ALSA null: each starts two loops, three pickups and one
door voice, without rejected/dropped commands. [Output](door-diagnostic.log) confirms
the final pickup and door start share one tick. This verifies software events only.
The original 0.4-second door cue contains a 180→100 Hz sweep. Its overlap with the
pickup and low-frequency content are plausible audibility limitations, not proven
speaker-response measurements. The audible session also ends with output healthy,
12 voices started, eight one-shots completed and no rejected/dropped commands.

## Revision and focused validation

The generator and checked-in `assets/sounds/door.wav` now retain a quieter bass body
and add descending midrange components. The opening event, mixer gains/panning,
pickup, controls and level transitions are unchanged. Pressing E still has no separate
exit cue. The new sound has the same mono PCM16/48 kHz format, 19,200 frames, 0.4-second
duration and byte size. It retains a tail after the 0.15-second pickup ends.

Regenerating the complete asset set changes **only door.wav**. [Asset checks](asset-checks.json)
retain old/new hashes and format/sample checks; both clips remain below 0.6 full-scale
peak without clipped samples. This is not physical audibility evidence.

At `fd93752`, all seven selected Release groups pass without skips: `sound`,
`audio_transport`, `audio_reload`, `audio_watch`, `audio_ui_session`,
`asset_validation`, and `asset_validation_cli`. See [commands](commands.json),
[XML](focused-tests.xml), [full output](focused-full-output.log), and [runner](validate.py).
The existing Release preset tree is reused with two compiler jobs and private disk
scratch. These are focused checks; the earlier 207-group matrix remains scoped to
its earlier candidate.

## Listening retest and remaining gates

[The retest](retest-run.json) opens the revised Release build directly in Annex,
using explicit repository assets and the same private controls. Its binary and door
asset hashes are recorded. The user confirms **“I hear the sound now”** on the same
laptop speakers. The reported missing door cue passes this physical retest; the
original failed observation remains retained. [The completed log](door-retest-1.log)
also confirms Annex completion and transition to Facility.

That same run later logs **`Audio device failed (device_error=-5)`** and disables
output. The user reports **no suspend, lid closure or audio-device change** and says
they closed the app while the PC continued running normally. The log has no per-line
timestamps, so closure is not established as the cause. The final counters retain
8 voices started, 4 completed, no rejected/dropped commands and `enabled=0`.
The [nearby user audio-service journal](audio-service-context.log) has no entries;
that does not establish healthy hardware or explain the error. R5 remains open for
this unplanned failure and the unperformed device-lifecycle cases. No transport
recovery policy is changed to hide the failure.

Physical monitor-scale/layout/focus/minimize cases, the rest of the audible controls,
real disconnect and suspend/resume, performance/endurance/broader memory, and support
signoff remain open. Existing timing failures remain retained. The manual run's
rolling timing counters are not a new performance acceptance study.

## Candidate package

At documentation commit **`bc84b22`**, all three Release package groups pass
without skips. All 34 then-current evidence/documentation files match archived
bytes. The audit verifies 1,342 relative file links, 80 shell recipes and
eight bounded CPU entry-point commands between two successful manifest checks.
Allocation profilers and session tests remain excluded. These checks validate
packaging and command correctness; physical acceptance and timing gates remain open.

The final merge receives the same three package tests and complete archive audit
after this evidence is included. Final local records live under
`build/door-cue-refresh/`; package builds reuse the Release preset tree.

[Package status](package-feature-tests.xml), [commands](package-feature-commands.json),
[test log](package-feature-tests.log), [archive audit](package-candidate/summary.json).
