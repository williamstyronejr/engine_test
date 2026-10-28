# Stress function sampling

Use the existing GNU Release executable without changing renderer code, compiler
flags, public APIs or report schemas. Kernel user-only per-thread performance
events are available on this host. The `perf` CLI was absent; a project-local
copy of the distribution's perf package and its two missing libraries is extracted
under ignored `build/function-validation/perf/`, checked against repository SHA-256
values. Nothing is installed system-wide or shipped as an engine dependency.

The installed gprofng 2.47 collector exited via SIGKILL before producing samples
for Feature Lab, `/bin/true` and `/bin/sleep`. Cause is unconfirmed; retained logs
do not show an engine crash. A first hand-written perf capability probe used an
undersized 56-byte attribute and returned E2BIG; the corrected 128-byte request
succeeded. A 30-frame perf smoke run confirms collection and application symbols.
It is a capability test, not part of the performance comparison.

## Declared collection

Commit this protocol and scripts before collecting six fresh processes, ordered
baseline/sample, sample/baseline, baseline/sample. Use one GNU Release binary:
default combined workload, seed 1, 1920×1080 offscreen, immediate presentation,
three cycles of 60 warmup plus 600 measured frames, and all four existing frame,
submission, preparation and context traces. No screenshots or simultaneous builds,
tests or benchmark jobs. Retain every attempt and its exit status, commands, UTC,
temperatures, binary/source/hash identity and kernel wait4 peak RSS.

Sample with the following options; software cpu-clock period is one million ns
(1 ms), user execution only, with inheritance disabled. No call chains or stack
copies, scheduling changes, kernel samples, affinity changes or frame waits.

```sh
export LD_LIBRARY_PATH="$PWD/build/function-validation/perf/usr/lib"
build/function-validation/perf/usr/bin/perf record --no-inherit \
  -e cpu-clock:u -c 1000000 -m 128 --no-buildid-cache \
  -o build/function-validation/example.data -- \
  build/release/feature_lab --stress --width 1920 --height 1080 \
  --frames 600 --warmup 60 --cycles 3 --presentation immediate \
  --frame-samples --submission-samples --preparation-samples --context-samples \
  --report build/function-validation/example.json
```

Baseline uses the same environment and binary without the perf wrapper. Profiling
runs' wait4 RSS describes the wrapper/child process tree maximum, not a directly
comparable engine-only heap or RSS measurement. Keep report cycle-boundary RSS
separate. Report overhead observations as paired changes in whole-frame median/p99
and non-GL preparation CPU median; these sequential pairs do not isolate causal
profiler overhead from frequency, thermal state or other session activity.

## Scope, validation and interpretation

The profile covers the **main thread's entire process lifetime**: startup, warmup,
all cycles, preview, report serialization and teardown. It does not align samples
to measured frames or isolate non-GL preparation. Driver worker threads, blocked
wall time and kernel time are excluded. A sample names the currently executing
instruction's symbol, not its caller's inclusive duration. Optimized/inlined code
is charged to its enclosing emitted symbol; keep compiler clones separate.
Sampling and interrupt skid do not identify an expensive source statement.

Export every sample with PID/TID, IP, symbol and mapped object. Validate one
PID=TID, aggregate counts by symbol and object, and preserve unknown symbols.
Retain event attributes, build IDs, complete sample logs, raw-event statistics and
raw perf data as zlib+base64 JSON with SHA-256, so the installed evidence keeps the
original binary data. Export the executable symbol table and identity; external
libraries and the executable itself are not redistributed. Future re-symbolization
needs matching binaries. Lost or throttled samples invalidate a firm ranking.
Require ≥500 application-image samples per timed profile and <5% unknown application
symbols before using the ranking to select follow-up investigation. This is an
exploratory evidence-quality rule, not a release or optimization adoption budget.

Apply the existing strict stress trace, checksum, resource, workload and aggregate
checks to all six reports. Preserve missing GPU results and all original 16.7 ms
p99/max failures. Report per-run exclusive application-function shares relative to
both all captured samples and application-image samples. Keep external libraries
and unresolved samples in the denominator; no percentiles are added together.

Run all nine existing small stress capture modes both normally and under perf,
checking their existing trace/count/argument/presentation assertions and matching
PPM hashes. These exercise the observer against the same runtime, not new engine
features. No new compiler/sanitizer matrix is claimed for unchanged engine code.
Analyzer controls must reject mixed threads, malformed sample lines, count mismatch,
lost/throttled samples and insufficient/unknown application symbols. Keep the
capability and wrapper failures visible. Only choose the next investigation after
collection; do not modify the renderer in this increment.

The Linux kernel project's [perf record documentation](https://github.com/torvalds/linux/blob/master/tools/perf/Documentation/perf-record.txt)
defines sampling options; the [gprofng manual](https://sourceware.org/binutils/docs/gprofng.html)
describes that collector. Local tool versions, help and event attributes are retained.
This work does not verify physical presentation/audio, broader memory coverage or
final support gates. Background loading remains deferred.
