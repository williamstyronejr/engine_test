# Audio error observation — 2026-11-10

The previous physical session retained terminal error `-5`, with no underrun or
suspension counted and no user-reported device change. It did not identify the
failing ALSA operation. Do not infer the cause from the error number alone.

Before modifying engine behavior, run two sequential 120-second silent sessions
against the current `default` PCM. Each uses the existing Release engine library,
with no registered sounds, no GUI and no device/configuration changes. Record the
requested device, ALSA plugin type and per-second transport counters. Retain both
attempts, including nonzero exits; a missing reproduction does not close the issue.

A diagnostic interposer delegates ALSA calls unchanged. It reports open results,
negative terminal/recovery returns and invalid write counts, including the operation,
PCM state and monotonic timestamp immediately after the return. Routine EAGAIN and
EINTR are excluded. State is a subsequent observation, not an atomic snapshot of
the failure. Symbol lookup and failure logging affect this diagnostic workload;
there is no performance or worker-allocation claim.

The child timeout is 140 seconds. Exit 2 indicates a measured device failure;
other nonzero results are collection failures. Record source, binary hashes,
commands, timestamps, environment and outputs. Do not retry or suppress EIO.

ALSA's [PCM implementation](https://github.com/alsa-project/alsa-lib/blob/master/src/pcm/pcm.c)
has several EIO return paths, including readiness-wait errors. The
[API reference](https://www.alsa-project.org/alsa-doc/alsa-lib/group___p_c_m.html)
documents the state and wait APIs. These sources motivate capturing the operation
and state; they do not establish which path caused the earlier failure.
