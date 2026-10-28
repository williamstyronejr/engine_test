# Stress context-switch correlation

Add `--context-samples` to the existing frame, submission and preparation tracing
flags to retain calling-thread voluntary and involuntary context-switch snapshots.
Graphics and all three prerequisite flags are required. Invalid requests fail
before replacing an existing report. No scheduling policy, affinity, priority,
desktop setting, render workload or buffer strategy is changed.

```sh
./build/release/feature_lab --stress --width 1920 --height 1080 \
  --frames 600 --warmup 60 --cycles 3 --presentation immediate \
  --frame-samples --submission-samples --preparation-samples --context-samples \
  --report build/context.json
```

## Counter scope and limits

Linux `getrusage(RUSAGE_THREAD)` supplies cumulative `ru_nvcsw` and `ru_nivcsw`
counts for the calling render thread. Each of the four existing preparation phases
(`begin`, `tiles`, `sprites`, `end`) gets a snapshot before its first clock read
and after its last clock read: eight extra system calls per rendered frame.
These calls are outside the old phase timings but inside the enclosing render
interval, increasing its explicit unmeasured wall remainder. Disabled collection
makes no new system calls and reserves no context-row payload.

Counter spans enclose the **whole phase, including GL calls and clock reads**.
They are slightly wider than the timed phases and do not cover all inter-phase
work. They do not isolate non-GL preparation. Delta counts can correlate with
both whole-phase wall-minus-CPU gaps and the older non-GL preparation residual;
they cannot assign an individual switch to the GL or non-GL portion.

A voluntary or involuntary switch count does not supply a duration, blocked
resource, runnable-wait duration or root cause. Even an involuntary switch next
to a large gap is association, not proof that this switch caused the entire gap.
No switch observed does not prove uninterrupted execution: sampling boundaries,
interrupts and clock/accounting differences remain relevant. Counter reads have
overhead and can themselves be preempted. Preserve signed timing gaps, including
negative values. GPU intervals remain separate and overlap CPU intervals.

System-call failures and negative kernel counters fail explicitly rather than
manufacturing zeros. The recorder rejects regressions within snapshots, between
phases and across measured rows/cycles. Counts that increase during unrecorded
warmup or inter-phase work are valid but not attributed to the recorded spans.

## Additive report contract

Existing report/frame/submission/preparation schemas and row sizes remain unchanged.
Enabled reports add `context_samples`, with schema `stress_context_samples_v1`,
source `getrusage(RUSAGE_THREAD)`, phase order `["begin","tiles","sprites","end"]`
and counter order `["voluntary","involuntary"]`. Each row has the original
string `frame` tag and four `phases` entries. Each phase contains `before` and
`after` arrays of **decimal strings**, preserving the full unsigned 64-bit values.
Subtract before from after for a span's counts. Zero deltas are valid observations.

Storage is independent, preallocated and bounded to 60,000 measured rows; recording
allocates nothing after reserve. Warmup is sampled but not retained. Partial runs
retain only recorded rows, with existing cycle/warmup tag gaps. On the tested
64-bit ABI, rows are 136 bytes: 244,800 bytes for 1,800 records, at most 8,160,000.
`memory.context_sample_buffer_bytes` reports actual capacity separately from older
diagnostic buffers. These optional bytes do not change the declared workload or
timing-vector budgets.

## Predeclared collection

Commit this protocol and runtime before collecting six fresh-process attempts.
Baseline enables frame, submission and preparation samples; context additionally
enables context samples. Order: baseline/context, context/baseline, baseline/context.
Use immediate presentation, default workload counts/seed, 1920×1080 offscreen,
and three cycles of 60 warmup plus 600 measured frames. Keep every attempt without
timing-based retries. Run sequentially, with no screenshots or concurrent benchmark
jobs. Record source/build/machine, actual managed viewport, request issuance,
boundary temperatures, kernel per-child peak RSS and hashes of retained artifacts.

Require matching workload, checksum, render counts and non-diagnostic payloads;
verify 1,980 presentations, 1,800 measured rows, original tags, nesting, GL wall
reconstruction and old CPU/GPU summaries. Validate exact counter strings and
monotonic snapshots before deriving deltas. Preserve missing GPU results.

For each context run, group frames and phases by no observed switches,
voluntary-only, involuntary-only and both. Report counts and distributions of whole
phase and non-GL preparation wall-minus-CPU gaps. A **1 ms preparation gap** is a
predeclared diagnostic grouping threshold, not a new acceptance limit. Count which
switch categories accompany gaps above it. Retain paired counters for the ten
largest submissions and submission-dominant frames above the original 16.7 ms
reference. Do not infer causation from group differences or add percentiles.

CPU frequency, thermal state and other session activity are uncontrolled; on/off
observations do not prove instrumentation overhead or a performance improvement.
The original p99/worst-frame and memory limits, earlier failed attempts and physical
display/audio/broader memory-coverage gates remain open. Native correctness checks
also compare requested-VSync images without claiming physical synchronization.
