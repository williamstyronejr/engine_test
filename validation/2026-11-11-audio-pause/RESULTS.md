# Post-transition audio artifacts and pause fade — 2026-11-11

**The user heard all pickup and door cues, but reports a brief pause pop and about
five seconds of static after the final Facility pickup while moving.** The static
cleared on its own. Transport diagnostics report no error; this is not a clean
audible-output pass. The pause implementation now fades active voices, with a
physical listening retest still required. The static and original EIO remain open.

## Retained interactive observation

The [predeclared protocol](session/PROTOCOL.md) targets at least 300 seconds in
Facility after Annex→Facility. The unchanged `7066514` release archive has SHA-256
`62add19764b21dd0dafe597e9026401da7e2a34febfd8e62814dc86f000b0cdb`.
[Preparation](session/prepared.json) retains the private extraction, copied controls
and saves, exact command, source and tracer hash. [Build identity](session/BUILD_INFO.txt.log),
[completion](session/completion.json), [integrity](session/integrity.json) and the
before/after manifest logs retain the tested artifact's provenance.

The run exits normally after **448.872 seconds**, including **400.424 seconds**
between the first Facility transition receipt and exit. It reaches all three
Facility pickups and completes the door animation. It renders 21,542 frames and
ends with audio enabled: 12 voices started, 8 completed, 21,499,904 accepted frames,
zero rejected/dropped plays, underruns, suspensions, recoveries and device errors.
Only a successful IOPLUG open appears in the ALSA trace. The scoped service journal
queries succeed with no entries, and before/after service-state queries succeed.

[Raw output](session/game.log), [timestamped output](session/timed.log),
[transition receipts](session/transitions.json),
[before-service/device capture](session/before-launch/capture.json),
[after-service/device capture](session/after-exit/capture.json),
[the exact user reports](user-observations.json), [summary](session-summary.json)
and [consistency analyzer](analyze.py) preserve these separate observations.
The selected pause key was not specified. No speaker output or per-pickup event
timestamps were captured; the approximate static interval cannot be aligned to
an exact device call. The first-error capture never triggers because no error is
reported, which does not contradict the user's audible artifact.

The residence target is met. This elapsed interval does not prove uninterrupted
active play or a performance/endurance pass. The final session also reports
77.3743 dropped simulation seconds; there is no time-resolved trace tying those
to the static, so no causal claim follows. Receipt timestamps are observations,
not atomic event times. The tracer and line buffering affect this diagnostic run.

## Pause correction

There is no authored pause cue: `Soundtrack` pauses both audio groups. At the
tested revision, `Mixer` immediately excludes paused voices, allowing a nonzero
sample to jump to silence. This is a concrete source of a discontinuity consistent
with the reported click; it does not establish the cause of the longer static.

Implementation **`fa6827f`** adds a separate 64-frame voice envelope, about 1.33 ms
at 48 kHz. Active voices fade to silence before freezing their sample cursor and
fade back in on resume. Reversal starts from the current envelope; repeated commands
do not extend it, and overlapping voice/group pauses preserve their combined state.
Voices paused before their first mix remain silent at frame zero. Reload preserves
the fade state. One-shots may finish during a fade and are not resurrected.
Stop and pan remain immediate; assets, transport recovery and device settings are
unchanged. The mixer adds no allocation, locking or I/O.

Three new sample-level regression cases fail on the previous mixer and pass with
the change. [Baseline output](baseline-sound-tests.log) retains the expected failures.
Coverage includes stereo boundary continuity, frozen phase after the fade, silent
starts in paused groups, resume, overlapping/rapid/repeated controls and block-size
independence. A separate reload regression preserves an in-progress fade.
These are software checks; the revised pause has not yet passed a physical retest.

## Static remains unassigned

[Offline asset inspection](asset-inspection.json) finds a 0.15-second pickup and
0.4-second door clip. The initial/final stored gains match; one pickup, one door
and both loops at those gains have a conservative per-channel absolute sum below
the mixer's clamp. This excludes neither additional overlapping effects nor
transient controls, downstream distortion or transport artifacts. It is not a
recording of what the user heard, and unchanged settings files do not reconstruct
all live settings during the run. No asset or device change is justified yet.

The next listening check should verify pause/resume separately, then attempt the
Facility pickup event with time-correlated event and audio capture if the static
recurs. Keep the earlier EIO, this static report and the pause retest distinct.
The wider physical device lifecycle, timing/endurance and support gates remain open.

## Focused validation

At `fa6827f`, **20 group executions pass without skips**: GNU Debug (5), GNU
Release (7), Clang Debug (4) and native ASan/UBSan (4, leak detection enabled).
All four configurations exercise `sound`, `mixer_reload`, `audio_transport` and
`audio_reload`. Debug/Release add `audio_watch`; Release adds `audio_allocations`
and `audio_ui_session`. Device tests use null/file sinks; the UI session uses real
OpenGL and controlled audio failure. This is not another full-suite, TSan, offline
build, timing or physical-listening qualification.

[Commands](validation-commands.json), [summary](validation-summary.json),
[Debug](debug-tests.xml), [Release](release-tests.xml), [Clang](clang-tests.xml)
and [sanitizer](sanitize-platform-tests.xml) logs retain exact source, environment,
builds and statuses. Existing build trees are reused. The Debug build reports
`ninja: warning: premature end of file; recovering`, then completes its rebuild
and tests. This metadata warning is retained; its cause is not established.

The candidate and final merged release also receive package build, relocation and
OpenGL checks, plus the archive documentation/CPU entry-point audit. These do not
substitute for the pending pause listening retest or resolve the pickup static.
