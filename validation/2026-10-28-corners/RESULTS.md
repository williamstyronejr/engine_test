# Quad-corner reuse evidence — 2026-10-28 UTC

The candidate passes rendering checks but fails the predeclared CPU adoption gate.
The final renderer is restored to the reference. See the [declaration](../../benchmarks/QUAD_CORNER_REUSE.md)
and [findings](../../benchmarks/2026-10-28-corners-results.md).

- [reference-build.json](reference-build.json), [reference-commands.json](reference-commands.json)
  and `reference-*.log/xml`: original source/binary identity, seven checks and image hashes.
- [commands.json](commands.json), [environment.json](environment.json), compiler
  `*-configure.log`, `*-build.log`, `*-tests.log/xml`: prototype matrix at `2f0e9de`.
  [checks.json](checks.json) verifies 35 group executions without failures/skips;
  [images.json](images.json) retains 36 comparisons to the reference.
- [collection-build.json](collection-build.json) and `collection-*.log`: rebuild
  for corrected collector commit `2fcb6dc`; runtime sources match `2f0e9de`.
- [launch-failure/manifest.json](launch-failure/manifest.json), its original
  collector and six logs: unsupported-argument failures before any rendering.
- [manifest.json](manifest.json), `reference-N.json/log`, `candidate-N.json/log`:
  six complete timed attempts, exact commands, hashes, UTC, temperatures and RSS.
- [collect.py](collect.py), [analyze.py](analyze.py), [analysis.json](analysis.json):
  retained collection and strict trace/aggregate validation with paired CPU decision.
- [analyzer_controls.py](analyzer_controls.py), [analyzer-controls.json](analyzer-controls.json):
  two accepted controls and twenty rejected corruptions, tested in isolated copies.
- [decision.json](decision.json): failed gate, non-adoption, and identical reference/restored renderer blobs.

Both native variants use the same seven groups: transforms, font graphics, texture
sampling, stress frame samples, stress graphics, render targets and graphics.
CPU sanitizers exercise transforms with `detect_leaks=1:halt_on_error=1`; native
sanitizers exercise the six hardware groups with `detect_leaks=0:halt_on_error=1`.
Both use `UBSAN_OPTIONS=halt_on_error=1`; no suppressions. These are focused checks,
not a new full-suite claim. GNU/Clang builds use two compiler jobs sequentially.
The final restored renderer receives a Release recheck and package validation.

```sh
python validation/2026-10-28-corners/analyze.py validation/2026-10-28-corners
python validation/2026-10-28-corners/analyzer_controls.py
```

Analyzer success establishes internal consistency, not release acceptance or a
passed optimization gate. GPU missing observations and all timing failures remain.
