# GPU timer rollover probe

`gpu_timer_probe OUTPUT.json SECONDS` is a diagnostic executable built when native
platform targets and `BUILD_TESTING` are enabled. Duration is 1–180 seconds. It is
not installed in the game package or included in routine CTest runs.

```sh
cmake --build --preset release --target gpu_timer_probe
./build/release/gpu_timer_probe build/timer-rollover.json 75
```

The probe nests two timestamp markers inside one elapsed query, draws a 64×64
quad, and reads all three results only after availability. It deliberately flushes,
sleeps inside the scope, and drains with a three-second readiness deadline. Those
operations are confined to this diagnostic; game-loop timers never flush or wait.
Default pacing is 5 ms. Near the suspected 2^36-ns absolute timestamp boundary, it
extends a scope to 250 ms to capture a reversal inside the pair. This tests clock
behavior; **these durations are not rendering performance measurements**.

The JSON records device/driver, source revision, both reported counter widths,
all within-pair reversals with unmodified decimal-string values, total samples,
between-scope reversals, maximum elapsed duration, wall duration and completion.
A failure can leave an incomplete JSON file; only `complete=true` is a finished
experiment. Exit 77 means no display or unsupported counters. Exit zero confirms
completion/GL health, not that a reversal happened or that elapsed timing passed a
specific comparison. Examine the observations before drawing conclusions. On other
drivers a 75-second run may observe no rollover.

For this specific 36-bit hypothesis, compare each reversed pair's hypothetical
`(2^36 - start_ns) + end_ns` with the enclosing raw elapsed result. That calculation
is an analysis of the recorded experiment, never a repair of production samples.
Elapsed queries enclose the timestamp markers, so small overhead differences are
expected. Preserve the original observations and distinguish this upstream source
hypothesis from the behavior actually measured on the installed driver.
