# GPU elapsed timing and timestamp rollover — 2026-10-17

Increment 40 switches normal gameplay, replay profiles and stress rendering to
nonblocking `GL_TIME_ELAPSED` queries. Timestamp pairs remain available through
`GpuTimer(GpuTimingMethod::timestamp_pair)` for diagnosis. No historical result is
rewritten and no driver-specific wrap correction runs in the engine.

## Source evidence

The installed package is Mesa `1:26.2.2-1`; OpenGL identifies it as
`26.2.2-arch1.1` on Intel HD Graphics 4400 (Haswell). The corresponding upstream
[Mesa 26.2.2 Crocus query source](https://gitlab.freedesktop.org/mesa/mesa/-/blob/mesa-26.2.2/src/gallium/drivers/crocus/crocus_query.c)
computes absolute timestamps by scaling the hardware snapshot and then masking to
`TIMESTAMP_BITS`. Its elapsed CPU-result path first calculates the raw snapshot
delta (including hardware-counter rollover), then scales and masks the duration.
The [same tag's definitions](https://gitlab.freedesktop.org/mesa/mesa/-/blob/mesa-26.2.2/src/gallium/drivers/crocus/crocus_defines.h)
set `TIMESTAMP_BITS` to 36. Thus a short interval can cross the absolute timestamp's
68,719,476,736-ns boundary while its elapsed result remains small.

The files were retrieved from the upstream GitLab raw endpoints. Their SHA-256s:

- `crocus_query.c`: `8f71809452dd1b789481ed080823e08af63c2ed02ad1b66b363d70baa27be23e`
- `crocus_defines.h`: `f178b18215d49085595da1d5ef5891f5187a33e3ce2b7584728a1520bbdc95b1`

This verifies the matching upstream release's source, not a byte-for-byte rebuild
of Arch's installed binary. The native experiment below tests the installed behavior.
Both query targets report 64 counter bits here; the engine does not infer effective
width from that value or silently apply a 36-bit mask.

The historical [Annex repeat 2](gameplay/2026-10-17/annex-2.json) remains rejected.
Its raw pair was 68,717,863,232 → 43,536 ns. A hypothetical 2^36 wrap gives
1,657,040 ns, which explains the reversal plausibly but is not a replacement sample.
The older October 16 failure lacks raw values and cannot be retrospectively assigned
the same cause with certainty.

## Implementation and limits

Elapsed mode uses eight query objects instead of sixteen, with one availability
check and one result read per collected slot. The fixed pool still skips full slots,
keeps increasing frame tags, and reports pending results at shutdown without waiting.
It checks active elapsed-query ownership before begin, rejects overlaps transactionally,
and ends its own interrupted scope during destruction. Timestamp mode retains the
original bounded failure records and unsigned subtraction checks.

[Khronos specifies](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_timer_query.txt)
that elapsed queries return nanoseconds after preceding work completes, permit
nested timestamp markers, and expose asynchronous availability. Overflow of the
actual elapsed counter is undefined. The engine requires a reported 64-bit counter
but cannot validate internal driver precision or overflow. In elapsed mode zero
`invalid` means there were no timestamp pairs to reject; it is not a clock-health
certificate. Scheduling and CPU submission gaps can remain inside the interval.

Reports add `gpu_queries.method` and `counter_bits`; historical v1 reports without
these fields used timestamp pairs. Treat the method change as a measurement change,
not a demonstrated rendering speedup. See the [contract](../docs/FORMATS.md#asynchronous-gpu-timing)
and [probe instructions](GPU_TIMER_PROBE.md).

## Installed-driver experiment

The [raw probe](elapsed/2026-10-17/rollover-probe.json), built from `1374186` in GCC
Release, completed 11,166 scopes in 75.0036 seconds. Its retained loader output in
the [manifest](elapsed/2026-10-17/manifest.json) identifies `driver crocus` and DRI3.
Both targets reported 64 counter bits. There was one within-scope reversal and no
between-scope reversals:

| Observation | Nanoseconds |
| --- | ---: |
| Start timestamp, sample 6084 | 68,522,334,256 |
| End timestamp | 53,618,640 |
| Hypothetical 2^36 wrapped timestamp delta | 250,761,120 |
| Enclosing `GL_TIME_ELAPSED` result | 250,761,840 |
| Elapsed minus hypothetical delta | 720 |

The 250-ms CPU sleep was intentional probe pacing near the boundary. The enclosing
elapsed query covers the two markers and their small overhead, so its slightly
larger result is consistent with the expected scope. This reproduces the absolute
clock discontinuity on the installed driver and corroborates the release-source
explanation. Production elapsed queries avoid subtracting those wrapped absolute
values. It does not validate every driver or recover the historical failed sample.
Crocus also masks elapsed durations to 36 bits in this path: scopes of 68.72 seconds
or longer remain ambiguous despite the reported 64-bit width. This experiment
validates short intervals crossing an absolute boundary, not very long stalls.

## Gameplay and stress checks

All experiments ran sequentially after builds and runtime tests, from committed
source `1374186`, GCC 16.2.1 Release (`-O3 -DNDEBUG`), on the existing XWayland
desktop. No device audio or screenshot was enabled. There was one planned attempt
per workload, with no retries or timing acceptance threshold. Raw command arrays,
stdout/stderr, exit codes and wall durations are retained in the manifest. Dynamic
clocking, temperature and compositor behavior were uncontrolled; these are
correctness/measurement-path checks, not a before/after speed comparison.

[Facility](elapsed/2026-10-17/facility.json) and [Annex](elapsed/2026-10-17/annex.json)
each completed 3,000 replay commands with 30 warmup frames. Both retained their
expected final hashes, simulation steps and exact draw/quad totals from increment
39. Each has 2,969 measured GPU samples, 2,999 total collected queries, one pending,
zero skips, and method `time_elapsed` with 64 reported bits. The measured viewport
was 1050×1360 throughout. GPU medians were 1.703120 and 1.710640 ms; frame p99 was
9.704782 and 9.962088 ms respectively. These single runs do not establish a 1080p
budget or uninterrupted 60 FPS.

The [combined stress run](elapsed/2026-10-17/combined.json) completed ten load/unload
cycles at 1920×1080 offscreen with a 1050×1360 preview. Its 6,600 submitted queries
produced 6,599 collected results (5,999 after warmup), one pending and zero skips.
All resource checks passed and checksum `3573445382734126645` matches the earlier
combined workload. Across the two replays and stress run, 12,597 of 12,600 submitted
elapsed queries were collected; three remained pending by design. There are no
reversed-pair diagnostics in elapsed mode. The probe's separate drained queries
are not included in those production-timer totals.

## Automated validation

All 52 runtime CTest groups passed in GCC Debug and Release, with no hardware skips.
The deterministic queue, replay-profile and stress CPU suites passed ASan/UBSan;
the stress JSON check passed after rebuilding its initially stale runner binary.
The elapsed queue tests exercise delayed readiness, all eight occupied slots,
skipped tags, 100 cycles of reuse, transactional failed begins, zero and large
unsigned durations, and unsupported counter widths. Existing timestamp tests retain
raw reversal, precision, first-eight retention and failure-overflow coverage.
Native tests additionally verify conflicts between timers and external queries,
query-object deletion with pending work, and interrupted elapsed-scope cleanup.
