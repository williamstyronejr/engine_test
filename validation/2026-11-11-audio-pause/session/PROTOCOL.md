# Post-transition interactive audio observation — 2026-11-11

Use the unchanged release archive at `7066514`, with the previously validated
ALSA interposer and line-buffered logs. Start in Annex using a private copy of the
previous session's controls/saves. Complete Annex and enter Facility, then play
in Facility for at least 300 seconds before closing normally. Do not deliberately
suspend, disconnect devices or reset audio services in this observation.

Retain archive/manifest checks, source and tracer hashes, the exact command,
raw output, UTC receipt timestamps, monotonic transition receipts, normal exit
status, audio counters and the user's report. Capture scoped audio-service logs
and read-only device state at launch, the first observed failure, and exit.
Retain early exits and failures; do not silently retry. A first transition back
out of Facility ends the measured Facility residence interval.

This is a diagnostic reproduction attempt. Receipt times are not atomic event
times; instrumentation and synchronous diagnostic capture affect the workload.
Elapsed residence does not establish active gameplay or continuous audibility.
A user report is required for audible acceptance. A healthy run does not resolve
the original EIO, qualify performance/endurance or close device lifecycle cases.
No engine behavior, assets, desktop configuration or audio services are changed.
