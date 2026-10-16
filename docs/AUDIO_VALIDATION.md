# Audio recovery and validation

The engine owns decoding, sample storage, mixing, voice controls and its command
queue. Installed ALSA provides the Linux PCM device interface. Output is 48 kHz,
interleaved stereo float PCM, with a nonblocking device and 256-frame mixed blocks.

## Recovery contract

The worker retains the unwritten tail of a mixed block across partial writes,
interrupted calls and recoverable errors. It does not remix that block, drain new
commands or advance voice cursors again while waiting to submit its remainder.
This keeps voice progression tied to mixed blocks rather than retry count.

| Device result | Action |
| --- | --- |
| Positive write count | Advance only by the accepted frames |
| Zero or `-EAGAIN` write | Wait for readiness with a 10 ms timeout; inspect wait errors too |
| `-EINTR` | Retry on the next worker iteration |
| `-EPIPE` from write/wait | Count an underrun and prepare the device |
| `-ESTRPIPE` from write/wait | Count a suspension and enter resume handling |
| Resume returns `-EAGAIN` or `-EINTR` | Back off 10 ms, then return to the shutdown check |
| Resume succeeds | Continue submitting the pending tail |
| Resume otherwise fails | Try prepare once |
| Prepare fails or another terminal write/wait error occurs | Latch the returned negative error, stop the worker, reject new commands |

Every recovery step is bounded at the engine level; there is no internal indefinite
resume loop. Shutdown is checked between steps. Driver calls and OS scheduling can
still affect shutdown latency; this is not a hard realtime deadline. No allocations,
file reads or decoding were added to the transport worker. The production adapter
and deterministic tests use the same internal `BlockWriter` state machine.

Preparing can discard PCM already queued inside ALSA. The engine retries only its
unwritten tail; it cannot promise gapless output or exact audible timing after an
underrun or suspend. Recovery of an open PCM is distinct from reopening a removed
device. Automatic device reconnection remains unimplemented. A full command queue
can still reject controls during prolonged suspension; Feature Lab then disables
audio according to its existing failure policy.

`AudioStats` adds accepted `written_frames`, `underruns`, `suspensions`, successful
`recoveries`, readiness `waits`, and latched `device_error`. These are eventual,
independent counters, not one atomic snapshot. Accepted PCM frames do not prove
that sound reached a speaker. The null device consumes as fast as the worker can
mix, so its counts do not represent wall-clock playback duration.

Feature Lab logs the negative device error, releases failed output, continues
playing silently, and retains its last observed audio statistics for the exit
report. Preferences remain editable after failure. Initialization errors still
report their opening/format diagnostic and may have no running-device counters.

This policy follows the [ALSA PCM error/recovery documentation](https://www.alsa-project.org/alsa-doc/alsa-lib/pcm.html)
and [PCM wait/resume API contracts](https://www.alsa-project.org/alsa-doc/alsa-lib/group___p_c_m.html).

## Repeatable checks

```sh
ctest --test-dir build/release --output-on-failure -R '^audio_(recovery|transport)$'
./build/release/audio_probe                  # Silent default-device check
./build/release/audio_probe null --exercise # Control sequence, no physical output
./build/release/audio_probe file:/dev/full  # Expected failure, exit 1
```

The CPU `audio_recovery` group injects short writes, interruptions, timeouts,
underruns, suspension retries, unsupported resume, failed prepare, lost devices and
invalid backend counts. It checks every accepted stereo sample, no duplicate prefix,
bounded retries, counters and no remixing during recovery. It requires no ALSA
headers, display or audio hardware and is included in CPU-only builds.

`audio_transport` tests the real worker and standard ALSA null/file plugins.
`file:/dev/full` deterministically fails writes; tests require failed health,
rejected commands, preserved diagnostics, and safe Feature Lab fallback. This does
not simulate physical unplug behavior. No system ALSA configuration is modified.
The default silent probe requires accepted frames, not just a successful open.

## Listening and physical-device checks

```sh
./build/release/audio_probe --exercise
# Or select a supported ALSA PCM explicitly:
./build/release/audio_probe --device YOUR_PCM --exercise
```

The opt-in sequence lasts about eight seconds: left-only, right-only, centered,
muted, restored, group-paused, group-resumed, then stopped. Tone frequency is 440 Hz
with output amplitude at most 0.06 before the system's own volume controls. The
probe changes no system volume or routing settings. Its console labels describe
expected output; a successful exit only confirms software/device acceptance.

For a physical check, record device, route, mixer volume and whether each phase
sounds as labeled. Check for channel reversal, leakage during silence, distortion,
clicks and lost/duplicated playback. Separately test system suspend/resume and
removable-device disconnect during playback. The worker should recover where the
PCM supports it, or report a terminal error and stop accepting commands. Feature
Lab should remain usable with `AUDIO OFF`. Reconnection currently requires a new
output/session. Record actual returned errors and counters; do not infer a physical
result from injected errors or a null-device run.

Physical speaker audibility, real disconnect/reconnect and real system
suspend/resume remain outstanding. F10 remains partial until that evidence exists.


## Observed results — 2026-10-15

The default PCM accepted 6144 silent frames without recovery or failure. The
null-device exercise completed all eight control phases, with exactly one voice
started/stopped and no rejected or stale commands. The real ALSA file sink to
`/dev/full` failed with returned error -5 after 1792 accepted frames, and the probe
exited 1. A native full-game replay continued with audio disabled under the same
failure and retained tick 1381 / hash 3859428580428616961, matching normal output.
See [increment validation](STATUS.md#thirty-second-increment-validation-2026-10-15)
for compiler, sanitizer and packaging results. No physical speaker or unplug result
is claimed.

## Audio reload validation — 2026-10-16

Audio-bank reload now has exact mixer sample tests and ALSA null-output tests for
paused/active voices, queue rejection, acknowledged retirement, repair and shutdown.
These do not establish audible continuity. Sample replacement preserves normalized
frame position but does not crossfade or match waveforms. For a physical listening
check, edit the four WAVs under `--watch-assets`, confirm live loops change while
volume/mute/pause controls survive, then corrupt and repair one file. Record any
clicks and verify that already completed effects do not replay. Physical listening
and the existing disconnect/suspend matrix remain outstanding.
