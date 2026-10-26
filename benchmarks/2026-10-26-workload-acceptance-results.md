# Workload and memory acceptance — 2026-10-26 UTC

**Memory budgets pass; timing acceptance fails.** All 18 declared attempts complete
with valid workload/state/resource evidence. Seventeen pass the 16.7 ms frame-p99
target; only four pass the 16.7 ms sampled-frame maximum. Combined-stress repeat 2
has p99 **18.154982 ms**. The largest measured frame is **44.147747 ms** in Facility
repeat 3. No failed attempt is discarded or replaced.

The [declaration](WORKLOAD_ACCEPTANCE.md) and machine-readable policy were committed
at **`d4dbe22635013647b99118392f103cab03af738e` before collection**. That is also the
configured/measured source revision. Runtime code and assets are unchanged from
increment 60's `c7b2573`; this increment adds acceptance tools, evidence and docs.
The [validation record](../validation/2026-10-26-workload-acceptance/RESULTS.md)
retains the commands, raw reports, identities and evaluator controls.

## Timing results

Each cell below is the minimum–maximum across three **separate** runs, not a pooled
percentile. Times are milliseconds. Pass counts apply independently to each run.
Peak RSS is the largest kernel-reported child-process high-water value, in MiB.

| Workload | Frame median | Frame p95 | Frame p99 | Frame maximum | p99 passes | Maximum passes | Peak RSS MiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| combined | 3.138–6.934 | 8.982–11.538 | 12.149–18.155 | 29.497–37.019 | 2/3 | 0/3 | 52.58 |
| dense | 2.286–2.406 | 5.963–6.515 | 8.435–10.164 | 14.419–28.838 | 3/3 | 1/3 | 17.41 |
| mixer | 0.169–0.187 | 0.213–0.255 | 0.251–0.288 | 0.287–0.408 | 3/3 | 3/3 | 17.41 |
| facility | 2.096–2.511 | 7.778–9.446 | 9.367–12.318 | 21.401–44.148 | 3/3 | 0/3 | 52.93 |
| annex | 2.082–2.378 | 7.998–9.689 | 9.625–12.407 | 17.666–36.413 | 3/3 | 0/3 | 52.88 |
| spikes | 2.101–2.529 | 7.950–10.028 | 10.348–11.923 | 19.129–25.472 | 3/3 | 0/3 | 53.25 |

The 1080p combined workload renders 10,240 visible quads per measured offscreen pass,
with three draw batches. Its preview window and all gameplay runs use **1050×1360**.
CPU isolates have no window; gameplay is not a physical 1920×1080 acceptance test.
All visual runs accept the VSync-off request, use 64-bit elapsed GPU queries,
report zero skipped/invalid samples, and leave zero or one query pending. CPU
distributions contain 1,800 stress, 2,970 replay, or 3,570 spike samples per run.
Combined repeats 2 and 3 collect all 1,800 measured GPU samples; the other visual
runs leave one pending and collect one fewer measured sample. No GPU completion waits or screenshots are added.

All 30 reload/transition operation times pass their separate 16.7 ms budget;
the maximum is **4.783432 ms** (texture recovery, repeat 3). Startup and report writing
are outside frame timing. Stress load distributions remain in the raw reports as
observations; no startup-latency budget is declared here.

The combined-stress p99 failure coincides with higher aggregate CPU submission
values (render-submit p99 13.450 ms versus 4.934 ms in repeat 1), while its GPU p99
is 1.882 ms. These are separate distributions, not paired evidence identifying the
slow frame's cause. CPU elapsed scopes include scheduling and driver back-pressure.
Do not add their percentiles or infer a GPU bottleneck, regression, or speedup.

## Memory and resource results

Every process stays below the predeclared **256 MiB** RSS envelope; the largest
observed peak is **55,832,576 bytes (53.25 MiB)**. This is a new lifetime measurement,
not a comparison against older sparse `/proc/self/statm` observations. Linux `wait4`
includes the child spawn/exec phase as well as startup, teardown and report output;
inherited launcher pages can affect small CPU-process peaks. It does not isolate
engine-owned live heap or GPU residency. No values are subtracted or pooled.

| Covered payload | Observed bytes | Declared ceiling bytes |
| --- | ---: | ---: |
| Combined stress CPU buffers | 8,072,232 | 9,437,184 |
| Dense collision CPU buffers | 6,774,824 | 9,437,184 |
| Mixer isolate CPU buffers | 6,764,584 | 9,437,184 |
| Stress timing buffers, each workload | 100,824 | 131,072 |
| Published scene reservation, each level | 81,920 | 81,920 |
| Published collision reservation, each level | 32,256 | 32,768 |
| Maximum covered CPU staging | 154,942 | 163,840 |
| Maximum covered GPU staging | 26,132 | 32,768 |
| Maximum simultaneous combined staging | 154,942 | 163,840 |

The stock graphics inventory remains nine textures / 30,292 payload bytes, one
minimap target / 103,968 bytes, and a 16,384-byte font atlas throughout the spike
workload. Combined stress has one 16-byte atlas and one 8,294,400-byte target;
CPU isolates have neither. All stress teardown baselines restore, all ten spike
operations retain verified expected states/revisions, and each event returns with
zero tracked CPU/GPU staging. Both levels' scene/collision utilization and all
structural results match across repeats. Existing gameplay fingerprints/final hashes
and rendering totals match the predeclared reference.

These categories overlap or have different lifetimes: do not sum them into total
memory. The [coverage audit](WORKLOAD_ACCEPTANCE.md#storage-coverage-audit) lists
strings/hash storage, renderer internals, audio banks, platform/driver caches and
other unitemized ownership. Allocation counts remain their own historical result;
ordinary timing executables contain no allocation hooks in this collection.

## Environment and release decision

Collection ran sequentially from **19:58:06 to 20:00:57 UTC** after Release build
and focused checks completed. GNU 16.2.1, `-O3 -DNDEBUG`, sanitizers off; i5-4300U,
Intel HD Graphics 4400, Mesa 26.2.5-arch1.1, Linux 7.2.8-9-omarchy x86_64, XWayland.
The desktop compositor remained active. Thermal readings are retained at every
process boundary; clocks, caching and thermals were not controlled. The invalid
negative zone-0 sensor readings must not be interpreted as CPU temperatures.

The evaluator returns **1** for these measured budget failures, with
`evidence_valid=true`, `automated_budgets_passed=false`, and `release_accepted=false`.
Missing/invalid evidence would instead return 2. Memory passes are scoped to these
workloads and this host. R6 remains open because timing targets fail, physical 1080p
presentation is unverified, and interactive/audio/error/long-session memory paths
are not all covered. R4/R5 and the final R8 support record remain open too.

Next: retain paired frame samples for the failed workload so CPU submission,
presentation and delayed GPU durations can be correlated on the same frame tags.
Use that evidence to choose an optimization; keep the declared budgets and these
failed attempts. Background loading remains deferred.
