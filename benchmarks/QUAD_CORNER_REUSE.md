# Quad clip-space corner reuse experiment

The current renderer emits triangles with corner indices `0,1,2,0,2,3`, performing
camera subtraction and clip scaling for each emitted vertex. The candidate computes
four clip-space positions after culling/batch selection, then emits the same six
vertices. Preserve `(world - camera) * 2 / extent` operation order, UVs, colors,
bounds, batching and upload sizes. Do not replace division with reciprocals.
The earlier phase profile does not establish these calculations as a hot function.

## Reference and correctness

Reference source: `547695cd43ba707b88013e641357d7fc639e40f9`. Preserve its GNU Release
binary and hash before editing. Commit the candidate, this declaration and collection
scripts before timing. Build the candidate with the same GNU Release flags and
record both source revisions and binary hashes. Separate binaries require no
runtime branch, public renderer API, CLI option or new report schema.

Run transforms, render targets, texture sampling, basic graphics, font graphics,
stress graphics and stress frame-sample tests on the reference. Repeat these on
GNU Debug/Release and Clang Debug candidate builds, plus CPU transforms under
ASan/UBSan with leaks enabled and the six native groups under ASan/UBSan with driver
leak checks disabled. Require no failures/skips. Existing target checks cover
mirroring, inherited shear, rotation, camera translation, zoom, viewport resizing,
partially visible culling, clipping, alpha and target composition. Compare all nine
stress-mode PPM hashes to the reference across each native configuration. These
checks are representative correctness evidence, not exhaustive floating-point proof.

## Predeclared performance comparison

Collect six fresh processes sequentially in order reference/candidate,
candidate/reference, reference/candidate. Each uses the existing default combined
workload, seed 1, 1920×1080 offscreen, immediate presentation, three cycles of 60
warmup plus 600 measured frames. Enable frame, submission, preparation and context
samples equally for both binaries. No screenshots or concurrent builds/tests while
collecting. Keep every attempt; never retry a valid attempt because of its timings.
Record machine/build identities, actual viewport, thermal readings, per-child peak
RSS, commands, exits, UTC times and artifact hashes.

Validate workload/checksum/render totals, presentation requests/counts, all trace
shapes/tags/aggregates, phase/GL partitions, counter strings/monotonicity and memory
payloads using the previous context analyzer's checks. Permit only source revision
to differ in build identity. Preserve missing GPU observations and signed gaps.

Primary measure: each run's median of **per-frame summed non-GL preparation thread
CPU time** (sum of phase CPU minus nested GL CPU). Compute each adjacent pair's
percentage reduction, `100 * (reference - candidate) / reference`. Adopt only if
correctness passes and **all three pairs show at least 5% reduction**. This is a
conservative local engineering gate, not a statistical confidence interval or a
new release budget. Otherwise restore the existing renderer and retain the failed
experiment and source patch in Git history. Do not tune/retest the candidate in
this increment after seeing the results.

Also report per-run preparation CPU p95/p99, tile/sprite CPU medians, whole-frame
p99/max, counts above 16.7 ms, memory, and switch-associated preparation gaps >1 ms.
Context spans include GL and clocks and do not measure switch durations or causes.
Uncontrolled frequency/temperature/session activity still limits causal claims.
This experiment does not claim to repair preemption-associated tails or presentation
waits. Keep all earlier failed attempts and original p99/max/memory acceptance limits.
Physical display/audio, wider memory coverage and final support gates remain open.

## Reproduction

Build the reference revision and candidate revision in sequence with GNU Release,
copying the reference executable before building the candidate. Pass both binary
paths to the retained collector from the repository root:

```sh
python validation/2026-10-28-corners/collect.py \
  build/corner-validation/reference-feature-lab build/release/feature_lab
python validation/2026-10-28-corners/analyze.py validation/2026-10-28-corners
```

Collection refuses existing output files. Use a fresh evidence directory (the optional
third argument) for a new experiment, retaining the declaration, collector and
reference-build identity there. Do not overwrite this experiment's evidence.

## Collector correction before timing

The initial six launch attempts at `2f0e9de` incorrectly included `--assets`, which
stress mode rejects. Every process exited 1 before rendering, without a timing
report. Their original collector, manifest and logs remain in the evidence's
`launch-failure/` directory. Remove only this unsupported argument and commit the
correction before the six timed attempts; rebuild the candidate to stamp that
source identity. Stress renders procedural assets. No timing-based retry or
adoption-rule change is involved. The renderer remains identical to `2f0e9de`.
