# Terminal audio diagnostics and investigation — 2026-11-10

**The earlier gameplay error is not reproduced and its cause remains unresolved.**
Two predeclared 120-second silent default-device runs complete without errors.
Revision `3892d14` adds diagnostic operation/state fields; all 20 focused group
executions pass without skips, including ASan/UBSan and ThreadSanitizer checks.
Recovery and reconnection behavior are unchanged.

## Bounded observation

The [protocol](PROTOCOL.md) is committed before collection. [The collector](collect.py)
links a silent driver to the existing Release engine libraries and a diagnostic
[ALSA interposer](trace.cpp); [the driver](silent.cpp) registers no sounds. The
observed engine code is unchanged from `453e7e9`; collection source is `ce4c423`.
The already-built observation binaries and libraries are hashed before execution;
later diagnostic source edits do not change those running binaries. No engine
library is rebuilt until both attempts finish.

[Commands](collection-commands.json), [hashes](collection-hashes.json) and
[environment](environment.log) retain exact inputs. Both attempts use the default
ALSA route, which is configured for PipeWire; the open trace identifies `IOPLUG`.
They complete at 120,008 ms with 5,759,232 and 5,762,048 accepted frames respectively.
Both have zero device errors, underruns, suspensions and recoveries. See
[first attempt](silent-1.log) and [second attempt](silent-2.log). No attempt is
restarted or discarded. Neither run emits an audible sound or changes device settings.

These are silent transport checks, not the original rendered gameplay workload,
a physical device-lifecycle test or an endurance qualification. Interposition and
logging change the diagnostic environment. The original [physical failure](../2026-11-09-door-cue/RESULTS.md)
remains retained; successful observations do not establish that it is fixed.

## Diagnostic behavior

`AudioStats` now retains `device_error_operation` and `device_error_state`. The
operation distinguishes a failed write, readiness wait, prepare, or an invalid
backend write count. The raw PCM state and its name are sampled after the terminal
transport step; they are not an atomic snapshot of the call or proof of its cause.
Healthy snapshots report `none` and `-1`/`unknown`.

The worker writes the error details once before release-publishing the error code.
A stats acquire load observes that code before reading its immutable details.
Other counters remain eventual snapshots. The worker stores fixed diagnostic fields
and does not format or log errors. Feature Lab's main-thread error/exit logs and `audio_probe` expose the
fields; Soundtrack retains them after failed output is released. The existing
terminal policy still rejects commands and continues silent gameplay; no EIO retry,
automatic reopen or host configuration change is introduced.

Regression cases inject the same EIO at distinct operations after a partial write,
check error attribution and preserved accepted samples, and verify that no device
calls continue after failure. They cover prepare fallback, impossible write counts,
healthy defaults, direct concurrent stats observation and retained failure metadata.
The archive now includes diagnostic `.cpp` sources alongside the retained evidence.

## Validation

| Configuration | Passed groups | Skipped |
| --- | ---: | ---: |
| debug | 6 | 0 |
| release | 6 | 0 |
| clang | 3 | 0 |
| sanitize-platform | 3 | 0 |
| readiness-tsan | 2 | 0 |

GNU Debug and Release cover recovery, transport, reload, watch, audio allocation
and audio/UI sessions. Clang and native ASan/UBSan cover recovery, transport and
reload; the sanitizer run enables leak detection and uses null/file sinks without
a GL context. TSan covers transport and reload, including worker publication and
retirement. All preset/build directories are reused, with two compiler jobs.
This is a focused matrix, not a fresh full suite or offline build.

[Checks](checks.json), [commands](validation-commands.json) and [runner](validate.py)
retain each configuration's source, flags and exact group list. Matching XML and
full CTest logs are retained for each configuration. No test retry, suppression or
timeout increase is required.

[Probe controls](probe-controls.json) additionally check CLI output and the interposer.
[Null output](probe-null.log) reports healthy defaults. The deliberate
[`file:/dev/full` failure](probe-file-failure.log) exits **1 as expected**, with both
the trace and engine identifying `write`, error `-5` and state `PREPARED` (2).
That validates reporting for a known fault; it does not identify the earlier
PipeWire failure as a file-write error.

## Next evidence

Reproduce the original interactive workload with these diagnostics and retain its
first failure's operation/state, timestamps and contemporaneous device-service logs.
If needed, use the interposer for call-level timestamps. Preserve the current
failure policy until evidence supports an appropriate recovery. R5 device lifecycle,
other unrecorded physical cases, failed timing/endurance/broader-memory requirements
and final support/signoff remain open. No physical speaker result is added here.

## Candidate package

At documentation commit **`2e6fee4`**, all three Release package groups pass
without skips. All 50 then-current evidence/documentation files match archived
bytes. The audit verifies 1,368 relative file links, 80 shell recipes and
eight bounded CPU entry-point commands between two successful manifest checks.
Allocation profilers and session tests remain excluded. These checks validate
packaging and command correctness; physical acceptance and timing gates remain open.

The final merge receives the same three package tests and complete archive audit
after this evidence is included. Final local records live under
`build/audio-error-refresh/`; package builds reuse the Release preset tree.

[Package status](package-feature-tests.xml), [commands](package-feature-commands.json),
[test log](package-feature-tests.log), [archive audit](package-candidate/summary.json).
