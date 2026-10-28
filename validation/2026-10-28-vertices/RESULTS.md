# Fixed vertex-buffer experiment evidence — 2026-10-28 UTC

The CPU adoption gate fails; the original renderer is restored and the new boundary
regression test is retained. See [protocol](../../benchmarks/QUAD_FIXED_BUFFER.md)
and [findings](../../benchmarks/2026-10-28-vertices-results.md).

- [reference-build.json](reference-build.json), [reference-commands.json](reference-commands.json)
  and `reference-*.log/xml`: original runtime source/binary/hash and seven focused groups.
- [boundary-reference.json](boundary-reference.json), `boundary-reference-build.log`,
  `boundary-reference-tests.log/xml`: added test validated against the original
  renderer before runtime edits; test source is preserved in the candidate commit.
- [commands.json](commands.json), [environment.json](environment.json), compiler
  `*-configure.log`, `*-build.log`, `*-tests.log/xml`: candidate matrix commands,
  source, environment overrides, UTC, durations and exits.
- [checks.json](checks.json), [images.json](images.json): 36 focused group executions
  without failures/skips and 36 exact reference/candidate PPM comparisons.
- [manifest.json](manifest.json), `reference-N.json/log`, `candidate-N.json/log`:
  all six complete attempts, commands, source/build/binary hashes, UTC, temperatures,
  peak RSS, original traces, workload identities and retained frame-time failures.
- [collect.py](collect.py), [analyze.py](analyze.py), [analysis.json](analysis.json):
  declared collection, strict aggregate/trace reconstruction and paired CPU decision.
- [analyzer_controls.py](analyzer_controls.py), [analyzer-controls.json](analyzer-controls.json):
  two accepted controls, twenty rejected corruptions with updated hashes in isolated copies.
- [decision.json](decision.json): failed gate, exact renderer restoration and retained test.

The seven selected groups are transforms, render targets,
texture sampling, graphics, font graphics, stress graphics and stress frame samples.
Three candidate compiler configurations exercise those groups.
CPU sanitizers cover transforms with leak detection enabled; native sanitizers cover
the six hardware groups with driver leak detection disabled. Both halt on ASan/UBSan
errors; no suppressions. Builds use two compiler jobs sequentially. No new full-suite
claim is made. The final restored Release tree gets the stress-graphics group and
three package checks; the merge revision gets fresh package checks.

```sh
python validation/2026-10-28-vertices/analyze.py validation/2026-10-28-vertices
python validation/2026-10-28-vertices/analyzer_controls.py
```

Analyzer success establishes internal consistency, not a passed optimization gate
or release acceptance. Missing GPU observations and earlier timing failures remain.
