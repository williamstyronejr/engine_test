# Current timing baseline refresh (increment 79)

Commit this declaration and runner before collection. Use the unchanged runtime
from merge `485de58`, GNU Release, sanitizers off, with a freshly configured build
identity. The existing Release build directory is reused. No optimization is
introduced or adopted by this increment.

Run the six focused stress/report/replay/spike correctness groups before collecting.
Then execute exactly these studies, sequentially, with no builds, tests, screenshots
or other agent-started benchmark jobs during measurement:

1. The [version 1 acceptance declaration](WORKLOAD_ACCEPTANCE.md): all six workloads,
   three repeats, using its existing collector, policy and evaluator unchanged.
   Preserve 16.7 ms p99/max/operation limits, memory ceilings, fingerprints and
   report-validity checks. Exit 1 from evaluation is a measured failure; exit 2 is
   invalid evidence. Neither is acceptance. Retain every attempt without retry.
2. The [warmup/run-order protocol](STRESS_RUN_STABILITY.md): two CPU references,
   twelve balanced short/long runs and four observer controls. Copy its collector,
   analyzer and telemetry validator byte-for-byte into the new evidence directory.
   Keep its 5% spread and within-run drift screen unchanged. Run its positive and
   corrupt-evidence controls afterward, using isolated disk-backed scratch copies.

Record exact commands, source/binary/input hashes, host environment and read-only
process/load snapshots before and after the studies. The stability collector retains
its existing frequency/thermal observations and their limitations. Do not kill or
suspend user applications, change power/clock/affinity/desktop settings, select a
cooldown based on results, normalize timing by sampled frequency, or exclude outliers.
Background activity remains uncontrolled and snapshots do not identify interference
at individual frames. All times are local-host observations, not universal claims.

The two studies have different instrumentation and run order; do not pool their
frame distributions or treat them as a paired historical speedup experiment. The
acceptance collector has only boundary temperature reads. The stability collector
adds trace/telemetry overhead and advances different simulation states during its
two warmup lengths. Passing validity is separate from passing timing or stability.

If stability fails, retain the failed result and do not adopt an optimization on
these measurements. If it passes, use the retained interval/CPU evidence to name
the next bounded optimization experiment; passing alone does not accept a renderer
change. Physical display/input/audio, long-duration stability, unmeasured memory
and support signoff remain independent release gates.

The evidence lives in `validation/2026-10-31-timing/`. Run its `run.py` from a clean
committed tree after installing the normal dependencies. Use a new private disk
scratch path recorded in `build/timing-baseline-refresh/scratch.txt`, containing a
`compiler-tmp` subdirectory. Existing commands/measurement files are rejected.
This collection uses installed tools only. Package validation follows documentation,
on both feature and merge commits, with byte checks of archived evidence and docs.
