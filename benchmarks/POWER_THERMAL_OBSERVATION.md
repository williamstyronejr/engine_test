# Read-only power/thermal observation (increment 80)

Commit this protocol and collector before collecting. Investigate the recurring
low-frequency state with unchanged engine/runtime code from merge `3adc6d2`.
Do not change power profiles, governors, limits, cooling settings, affinity, user
applications or acceptance thresholds. Do not load kernel modules, install tools,
request elevated reads or write device/sysfs files. Unavailable evidence stays
explicitly unavailable. This is a host investigation, not an optimization trial.

Use one GNU Release build with a current embedded revision. Before collection,
run the existing stress CPU, stress graphics and stress-frame-sample groups.
Then collect a fixed sequence with no concurrent builds/tests/screenshots:

1. Twenty seconds of observation without an engine workload.
2. One CPU-only state reference: default combined workload, 600 warmup and 600
   measured frames per cycle, three cycles, seed 1.
3. Six sequential graphics runs at those same counts, immediate presentation,
   1920×1080 offscreen, with all four existing frame/submission/preparation/context
   traces. No retry, exclusion, cooldown selection or change after seeing results. Each child
   has a 180-second watchdog; a timeout is retained as invalid evidence.
4. Sixty seconds of observation without an engine workload, regardless of results.

Sample discovered read-only paths approximately every 0.5 seconds during all
phases. Record the actual sweep start and duration: reads are sequential, not
atomic, and include startup/warmup/reporting/teardown. Observe all CPUFreq policies,
intel_pstate controls, thermal zone types/temperatures/trips, cooling-device states,
coretemp temperatures/critical alarms, available throttle counts, RAPL zone names/
enablement/constraints/energy access, mains state and BAT0 status/current/voltage.
Preserve negative temperatures and errors. Discover identities; never assume zone
or hwmon numbering survives a reboot. Do not collect device serials or network logs.

Record host CPU times and load averages for context, not per-engine utilization.
At the boundaries, retain the power-profile report, selected service state and
filtered kernel/thermal-service journal records. A running service or cooling-device
configuration is not proof that it acted during a slow frame. A cap value is not
measured consumption. Denied energy reads cannot be replaced by an estimated zero.
A firmware warning may explain invalid sensor data without explaining low frequency.

Validate source/artifact hashes, workload and CPU-reference identity, report/frame
counts, graphics totals, resource stability, numeric traces, and observation shape/
monotonic ordering. Keep representative corrupted-input controls. Summarize all six
runs, per-run low-frequency observations (policy 0 ≤900,000 kHz as a descriptive
bin), CPU preparation medians, temperatures, cooling-state changes and counter deltas.
Low frequency during idle is not itself a fault. Do not infer per-frame causes from
process-wide observations or normalize timing by sampled frequency.

All 16.7 ms acceptance failures and the prior 5% stability failures remain in force.
This study neither reruns that acceptance matrix nor certifies stability. If the
state is not reproduced, report that outcome and the observation limits without
extending the run. If it is reproduced without a decisive signal, report the cause
as unresolved. Package checks follow on feature and merge commits; runtime sources
stay unchanged. No compiler/sanitizer matrix or physical acceptance is claimed.
