# Candidate workload acceptance evidence — 2026-10-26 UTC

All **18 attempts** complete with valid evidence. All declared memory budgets pass;
**timing acceptance fails**. The evaluator correctly returns 1: seventeen frame-p99
checks pass, four sampled-frame maximum checks pass, and all 30 operation-time checks
pass. These are measured budget failures, not crashes or test failures.
See [interpretation and tables](../../benchmarks/2026-10-26-workload-acceptance-results.md)
and the [prior declaration](../../benchmarks/WORKLOAD_ACCEPTANCE.md).

## Identity and collection

Declaration and configured measurement revision:
`d4dbe22635013647b99118392f103cab03af738e`. Runtime/asset content is unchanged from
`c7b2573`. GNU 16.2.1 Release, sanitizers off, i5-4300U / Intel HD 4400,
Mesa 26.2.5-arch1.1, XWayland, Linux 7.2.8-9-omarchy x86_64.

- [policy.json](policy.json) fixes six workloads, three repeats, fingerprints/states,
  memory ceilings and timing budgets before any new measurement.
- [manifest.json](manifest.json) records all 18 commands, UTC start/finish, process
  exits, kernel per-child RSS peaks, CPU/wall time, thermal readings, input and
  executable SHA-256 values, and each report/log hash. No attempts are retried.
- [environment.json](environment.json) records source status, Python/session details
  and byte equality between checked-in and staged assets. The two root untracked
  user files are excluded from the tracked-source marker and remain untouched.
- [evaluation.json](evaluation.json) retains every budget observation, threshold
  and decision, including all 15 failed checks (one p99, fourteen maxima).

Collection uses `os.wait4` for each child, not cumulative `RUSAGE_CHILDREN` or sparse
RSS sampling. Its peak includes child launch/exec, startup and final report writing.
This metric can include inherited launcher pages and shared/driver mappings; it
is not an engine-heap attribution or GPU-residency measurement.

## Raw reports

| Workload | Repeat 1 | Repeat 2 | Repeat 3 |
| --- | --- | --- | --- |
| Combined 1080p stress | [report](combined-1.json) / [log](combined-1.log) | [report](combined-2.json) / [log](combined-2.log) | [report](combined-3.json) / [log](combined-3.log) |
| Dense collision isolate | [report](dense-1.json) / [log](dense-1.log) | [report](dense-2.json) / [log](dense-2.log) | [report](dense-3.json) / [log](dense-3.log) |
| Mixer isolate | [report](mixer-1.json) / [log](mixer-1.log) | [report](mixer-2.json) / [log](mixer-2.log) | [report](mixer-3.json) / [log](mixer-3.log) |
| Facility | [report](facility-1.json) / [log](facility-1.log) | [report](facility-2.json) / [log](facility-2.log) | [report](facility-3.json) / [log](facility-3.log) |
| Annex | [report](annex-1.json) / [log](annex-1.log) | [report](annex-2.json) / [log](annex-2.log) | [report](annex-3.json) / [log](annex-3.log) |
| Reload/transition | [report](spikes-1.json) / [log](spikes-1.log) | [report](spikes-2.json) / [log](spikes-2.log) | [report](spikes-3.json) / [log](spikes-3.log) |

All visual viewports are 1050×1360; only combined stress renders its workload to a
1920×1080 offscreen surface. Combined repeats 2 and 3 collect all queries; the other ten visual attempts
leave one pending. All have zero skipped/invalid measurements. Structural comparison checks every workload,
state/hash, timing sample count, render total, GPU accounting and spike operation.
Scene/collision/staging repeat results agree. No screenshot reads perturb collection.

## Validation

[Preparation commands](preparation.json) retain successful Release configure/build
and six focused groups: stress CPU, replay reports, spike reports, diagnostics
allocations, UI allocations and gameplay allocations. [XML](tests.xml),
[test log](tests.log), [configure log](configure.log), [build log](build.log).

[Native commands](native-command.json) retain four further passing groups:
stress reports, stress graphics, native replay profiles and native spike profiles.
[XML](native-tests.xml), [log](native-tests.log). Both test sets have zero failures
and skips. No new compiler/sanitizer matrix is claimed: runtime sources are unchanged,
and [increment 60](../2026-10-26-diagnostics-text/RESULTS.md) retains that matrix.
The native test inventory remains 64 runtime groups plus three packaging checks.

The evaluator has **22 passing synthetic controls**, including an all-pass fixture,
inclusive budget boundaries, missing/duplicate reports, failed exits, stale builds,
nonfinite values, wrong workloads/gameplay, resizing, missing/skipped GPU results,
unverified events, retained staging, and individual memory/timing overruns.
[evaluator-controls.json](evaluator-controls.json) records outcomes. Synthetic data
is created in disposable directories and never presented as benchmark measurements.

The Python tools use only the standard library and are evidence utilities, not
engine dependencies. Run from a source checkout:

```sh
python3 validation/2026-10-26-workload-acceptance/evaluate.py \
  validation/2026-10-26-workload-acceptance
# Expected exit: 1 (retained timing-budget failures).
PYTHONDONTWRITEBYTECODE=1 python3 \
  validation/2026-10-26-workload-acceptance/check_evaluator.py \
  validation/2026-10-26-workload-acceptance
# Expected exit: 0 (22 synthetic controls pass).
```

The collector is intentionally separate from evaluation: all planned attempts are
retained even when an early run fails. Existing output files are refused. Neither
passing tests nor a low RSS peak overrides failed performance criteria. Physical
acceptance, long-session coverage and the final support record remain outstanding.
