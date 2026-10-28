# Main-thread function-sampling evidence — 2026-10-28 UTC

See the [protocol](../../benchmarks/STRESS_FUNCTION_SAMPLING.md) and
[findings](../../benchmarks/2026-10-28-functions-results.md). Engine code and build
flags are unchanged; these are development observations, not release acceptance.

- [capabilities.json](capabilities.json), [profiler-smokes.json](profiler-smokes.json),
  `gprofng-*-partial.log`, `smoke-collect.log`: retained SIGKILL collector failures
  and corrected perf-event capability checks. NUL preallocation padding was removed
  from partial gprofng logs; no cause is asserted.
- [local-profiler.json](local-profiler.json), [setup_local_perf.py](setup_local_perf.py):
  local package URLs, SHA-256 verification and extraction; no system installation.
  `perf-smoke*.log` retains the successful pre-protocol 30-frame capability run.
- [prepare-commands.json](prepare-commands.json), `configure.log`, `build.log`,
  `baseline-images.log`: GNU Release identity/build and existing capture CTest.
- [profile-image-command.json](profile-image-command.json), [images.json](images.json),
  `profile-images.log`: exact wrapper and nine matching normal/profiled image pairs.
- [manifest.json](manifest.json), [collect.py](collect.py), six `baseline-N` /
  `sample-N` JSON/log pairs: commands, exits, source/binary/tool hashes, explicit
  environment overrides, UTC, temperatures, kernel launch-tree RSS and all traces.
- [analyze.py](analyze.py), [analysis.json](analysis.json): existing strict workload,
  counter, timing and aggregate reconstruction for both modes; failed frame limits
  and missing GPU observations remain intact.
- [export_profiles.py](export_profiles.py), [profile-commands.json](profile-commands.json),
  `sample-N-samples.log`, `sample-N-events.log`, `sample-N-functions.log`,
  `sample-N-records.log`, `sample-N-buildids.log`: all captured samples and decoded
  event/statistics/identity views. Raw-event dumps retain their aggregate suffix.
- `sample-N-raw.json`: original binary perf data compressed with zlib and base64,
  byte length and SHA-256. [profiles.json](profiles.json) keeps all symbol/object
  counts, denominators and unresolved samples. [overhead.json](overhead.json) records
  paired observations without claiming causal instrumentation overhead.
- [verify_profiles.py](verify_profiles.py): recomputes all retained summaries and
  checks raw payload lengths and SHA-256 values without requiring perf.
- [profile_analysis.py](profile_analysis.py), [profile_controls.py](profile_controls.py),
  [profile-controls.json](profile-controls.json): accepted known/unknown-object
  profile and ten rejected malformed, mixed-thread, lost/throttled or low-coverage cases.
- [executable-symbols.log](executable-symbols.log), [perf-version.log](perf-version.log),
  [submit-annotation.log](submit-annotation.log), [annotation-command.json](annotation-command.json):
  measured binary symbols and disassembly annotated from sample 1.

Re-run pure validation without re-collecting timings:

```sh
PYTHONDONTWRITEBYTECODE=1 python validation/2026-10-28-functions/analyze.py   validation/2026-10-28-functions
PYTHONDONTWRITEBYTECODE=1 python validation/2026-10-28-functions/profile_controls.py
PYTHONDONTWRITEBYTECODE=1 python validation/2026-10-28-functions/verify_profiles.py
```

To recover a retained raw profile, decode the `data` field with Python
`base64.b64decode`, then `zlib.decompress`; verify `bytes` and `sha256` before using
perf. Matching executable/library files are needed for future re-symbolization;
those binaries are not redistributed. The decoded sample logs retain the original
symbolization even after local builds or libraries change. Exporting anew requires
the measured executable hash, not a later packaging build.

No engine compiler/sanitizer matrix is claimed for this unchanged implementation.
The final feature and merge revisions receive fresh GNU Release package checks.
Physical synchronization/audibility and broader release support remain unverified.
