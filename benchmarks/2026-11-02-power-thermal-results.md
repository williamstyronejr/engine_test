# Power and thermal observations — 2026-11-02

**The sustained low-frequency state did not reproduce in this bounded sequence.**
Its cause remains unresolved. Sampled controls stayed unchanged, throttle counters
increased, and the kernel reports invalid ACPI thermal data. These observations do
not establish that the ACPI warning, a power cap or a service caused the earlier
slow state. No system setting or engine runtime code is changed.

## Collection and validity

The [protocol](POWER_THERMAL_OBSERVATION.md), collector and initial analyzer were
committed at **`fea2f57`** before collection. The runtime is unchanged from merge
`3adc6d2`. GNU Release uses the reused preset tree, reconfigured for the measured
revision. Three existing stress correctness groups pass without skips before timing.
This is not a fresh compiler/sanitizer qualification.

Collection spans **2026-11-02T10:52:55.917179+00:00 to 2026-11-02T10:56:02.665447+00:00**. It includes a fixed
20-second observation, one CPU state reference, six graphics workloads and a fixed
60-second observation afterward. The collector takes **369 sweeps over 131 paths**
at approximately 0.5-second spacing, retaining actual start/read durations and
errors. Every workload exits zero and completes its declared counts. No attempt is
retried, excluded or extended to obtain a desired state. Sensors are read
sequentially and cover startup, warmup, measurement, reporting and teardown.
The largest sweep lasts 0.339 seconds; this is not instantaneous or frame-aligned
telemetry, and brief events can be missed. Collector CPU time is retained.

The initial analyzer rejects the established report mode because it expects
`graphics` instead of `opengl`. The original failure and source remain available;
a one-line correction then validates **the same captured data**, without rerunning
collection. The corrected analyzer accepts two valid controls and rejects 17
corrupted cases, including updated-hash mutations. See the
[correction record](../validation/2026-11-02-power-thermal/analysis-correction.json).

## Observed workload behavior

All six graphics runs use 600 warmup plus 600 measured frames per cycle, three
cycles, the default combined workload and a 1920×1080 target. Each retains 1,800
measured frames and matches the CPU reference's checksum and collision totals.
Frame/preparation partitions, graphics totals, payload inventories and the GPU
query accounting validate. The analyzer's scope is explicit in its source; this
is not a rerun of the previous balanced warmup stability screen.

The frequency bin below is descriptive: policy 0 ≤900,000 kHz, including process
boundaries. The single low observation in graphics run 6 does not establish a
sustained low-frequency workload. Throttle deltas are CPU 0's package counter;
shared package counters are never summed across logical CPUs.

| Phase | Policy 0 median GHz | Low samples / valid | Coretemp package °C | Package throttle delta | Preparation CPU median ms | Frame max ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| graphics-1 | 2.112 | 0/34 | 55–80 | 0 | 1.699 | 22.122 |
| graphics-2 | 1.896 | 0/34 | 80–86 | 0 | 1.663 | 8.474 |
| graphics-3 | 1.906 | 0/33 | 85–90 | 91 | 1.644 | 19.974 |
| graphics-4 | 2.349 | 0/34 | 87–90 | 830 | 1.539 | 36.841 |
| graphics-5 | 2.498 | 0/36 | 88–90 | 1120 | 1.338 | 35.399 |
| graphics-6 | 2.494 | 1/37 | 88–90 | 1714 | 1.456 | 33.452 |

Only **one of 208** graphics-phase frequency samples is in the low bin. The before
and after windows contain two of 40 and seven of 117 low observations respectively;
low frequency without an engine workload is not evidence of a fault. CPU preparation
medians range from 1.338 to 1.699 ms. All six frame p99 values are below 16.7 ms,
but five maximum-frame values exceed it. This diagnostic workload does not replace
the [failed acceptance matrix or stability screens](2026-10-31-timing-results.md),
and the later session is not a historical speedup or recovery experiment.

## Controls, sensors and unresolved explanations

- Mains `online` remains 1 and BAT0 reports Full. The power-profile reports show
  performance at both boundaries; this does not observe every transient profile.
- All sampled CPUFreq policies retain `intel_cpufreq`, `schedutil` and 800,000–
  2,900,000 kHz limits. `intel_pstate` remains passive, `no_turbo=0`, minimum 27%
  and maximum 100%. Passive mode delegates selection to a generic governor; the
  profile name alone is not the kernel governor. See
  [kernel driver documentation](https://www.kernel.org/doc/html/latest/admin-guide/pm/intel_pstate.html).
- Sampled package RAPL constraints remain 19 W long-term and 25 W short-term.
  All four energy counters return permission errors. Constraints describe configured
  limits; they do not measure consumption or prove a cap was reached. No package
  wattage is inferred. See [powercap interfaces](https://www.kernel.org/doc/html/latest/power/powercap/powercap.html).
- The four Processor cooling devices and `intel_powerclamp` stay at sampled state
  zero. This does not exclude unobserved transitions or independent hardware limits.
  Cooling states are exposed separately from thermal-zone temperatures in the
  [thermal sysfs interface](https://www.kernel.org/doc/html/latest/driver-api/thermal/sysfs-api.html).
- Coretemp package readings reach 90°C; the separately read package thermal zone
  reaches 92°C. Their sweeps are not atomic. Coretemp critical alarms remain zero;
  that specific alarm is not a general proof that throttling never occurred.
  See [coretemp semantics](https://www.kernel.org/doc/html/latest/hwmon/coretemp.html).
- Package throttle counters increase during later graphics runs even though the
  sustained low-frequency state does not appear. This demonstrates that a counter
  increase alone does not identify the earlier state. Neither event duration nor
  a frame-specific cause is inferred from these counters.
- The kernel boot journal explicitly reports an ACPI thermal firmware bug, invalid
  trip points and an invalid critical threshold. `acpitz` reports implausible
  negative temperatures. These values and trip data are retained without being
  interpreted as physical temperatures or proof of the CPU-frequency cause.
- `thermald` and `power-profiles-daemon` are active; TLP and tuned are inactive.
  The filtered before/after kernel journal captures are identical. Service presence
  and boot warnings do not prove a service intervention during a slow frame.

Discovery retains sensor names, so zone/hwmon numbering is not treated as a stable
identity. The recorded zone identities match the earlier capture. Unavailable
`bios_limit`, BAT0 `power_now`, denied energy reads and unsupported subordinate RAPL
maximum-power reads stay explicit errors; none is replaced by zero. No privileged
reads, module loading, tool installation, governor/power/fan changes or user-process
termination is performed. Whole-host CPU/load readings are context only.

## Next work and remaining release gates

The available evidence does not justify changing a power or thermal setting, or
adopting a renderer optimization. Stop repeating this host study without new
information that can distinguish causes. A future reproduction with permission to
read relevant hardware reason/energy counters, or an independent host, may supply
missing evidence; neither is part of this increment.

Return to engine release qualification: refresh the full current 69-group runtime
suite and prepare a concise acceptance handoff, keeping failed timing gates and
physical display/input/audio, endurance, broader memory and support signoff open.
The current investigation is complete within its declared scope, not a resolution
of those release requirements. [Raw evidence](../validation/2026-11-02-power-thermal/RESULTS.md)
retains the original analyzer failure, corrected result and every observation.

## Candidate package

At documentation commit **`2666cd2`**, all three Release package groups pass
without skips. All 58 then-current evidence/documentation files match archived
bytes. The audit verifies 1,277 relative file links, 78 shell recipes and
eight bounded CPU entry-point commands between two successful manifest checks.
Allocation profilers and session tests remain excluded. These checks validate
packaging/command correctness and do not change the failed timing decisions.

The final merge receives the same three package tests and complete archive audit
after this evidence is included. Final local records live under
`build/power-thermal-refresh/`; package builds reuse the Release preset tree.

See the [package evidence](../validation/2026-11-02-power-thermal/package-candidate/summary.json).
