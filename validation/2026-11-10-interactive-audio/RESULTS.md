# Traced interactive audio session — 2026-11-10

**The user confirms door and pickup sounds work. The session exits normally with
audio enabled and no device error; the earlier EIO does not reproduce. Its cause
remains unresolved.** This increment retains evidence and updates documentation;
engine, assets, tests and build rules are unchanged from `67d7a48`.

## Candidate and capture

The tested archive is `feature-lab-0.1.0-Linux-x86_64-67d7a48.tar.gz`, SHA-256
`78d16faa9449ac39fc3df528f02482bd4288d1531a210cc0d4ac29752c97a94e`.
Its checksum and extracted manifest were verified before launch; the archive and
manifest still match afterward. [Integrity record](integrity.json) and
[build identity](BUILD_INFO.txt.log) retain that provenance.

The game starts directly in Annex with audio, using a fresh extraction and a copy
of the earlier private controls/saves. [Prepared metadata](prepared.json) records
the exact command, copied-file hashes, display environment and interposer hash.
The interposer is the previously validated [ALSA tracer](../2026-11-10-audio-error/trace.cpp).
`stdbuf` makes lines available promptly; the [monitor](monitor.py) records raw output
and UTC line-receipt timestamps, then captures the scoped audio-service journal
when the game exits. These timestamps are not atomic event timestamps. Interposition
and buffering change the diagnostic environment; this is not a performance run.

## Observed result

- [Completion](completion.json): normal exit 0 after **1,005.646 seconds**
  (about 16 minutes 46 seconds), with 60,111 rendered frames.
- [Game output](game.log): Annex completes and the game enters Facility once.
  The final level has 102 simulation ticks. Only **1.697 seconds** separate the
  transition line from the final session line; process exit follows the transition
  by **1.953 seconds**. This run provides little time in the destination level.
- Final audio remains enabled: 8 voices started, 4 completed, 48,236,032 frames
  accepted, and zero device errors, underruns, suspensions, recoveries, dropped
  plays or rejected commands. Operation/state diagnostics retain their healthy
  defaults (`none`, `-1`, `unknown`).
- The trace records successful opening of an `IOPLUG` PCM and no failing calls.
  The selected user audio-service journal query succeeds with no entries.
- [The user reports](user-observations.json): **“Sounds for door and pickups working.”**
  This is a scoped audible-cue pass, not a report that every audio control or every
  moment of the whole session was physically checked.

[Timestamped output](timed.log), [journal command](journal-command.json),
[service output](audio-services.log), [summary](summary.json) and
[consistency analyzer](analyze.py) retain the source data and derived intervals.
The analyzer checks matching raw/timestamped lines, ordered timestamps, exit and
level state, audio counters and archive-integrity records. No engine test matrix
is rerun for this documentation/evidence increment; package checks are separate.

## Limits and next evidence

The suggested play interval was two to three minutes; the longer actual duration
is observed rather than a predeclared endurance workload. It does not establish
continuous audibility, long-session memory bounds or a timing acceptance pass.
No hardware disconnect, system suspend, device change or audio-service reset was
performed by the agent. No new listener route/volume configuration is inferred.

The [original EIO failure](../2026-11-09-door-cue/RESULTS.md) and
[two successful silent observations](../2026-11-10-audio-error/RESULTS.md) remain
separate records. One error-free interactive run does not identify or fix its cause.
A further targeted attempt should spend meaningful time **after** Annex→Facility,
retaining the first error's operation/state and contemporaneous service evidence.
R5 device lifecycle, unrecorded physical cases, R6 timing/endurance/broader memory
and R8 support/signoff remain open.
