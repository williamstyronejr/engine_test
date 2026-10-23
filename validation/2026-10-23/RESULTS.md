# Candidate validation — 2026-10-23

Tested implementation: **`1dfba31fd210d3237bd3d61abc6f6a497d207baa`**.
All compiler and sanitizer results below come from fresh build directories, using
the committed implementation with no tracked edits. This increment adds evidence,
documentation and installation of those records; it changes no engine/test code.
The records certify the named baseline on this host, not arbitrary later revisions.

## Compiler and sanitizer results

Every row passed without skips, sanitizer reports, retries or source fixes.
Counts are CTest groups, not individual assertions. Release packaging is selected
separately from the 56 runtime groups.

| Configuration | Passed | Evidence |
| --- | --- | --- |
| GCC 16.2.1 Debug | 56/56 | [log](gcc-debug-tests.log), [XML with case output](gcc-debug-tests.xml) |
| GCC 16.2.1 Release | 56/56 | [log](gcc-release-tests.log), [XML](gcc-release-tests.xml) |
| Clang 23.1.3 Debug | 56/56 | [log](clang-debug-tests.log), [XML](clang-debug-tests.xml) |
| GCC Debug, CPU-only ASan/UBSan, leak detection on | 30/30 | [log](cpu-sanitize-tests.log), [XML](cpu-sanitize-tests.xml) |
| GCC Debug, native ASan/UBSan, leak detection off | 56/56 | [log](native-sanitize-tests.log), [XML](native-sanitize-tests.xml) |
| Native ASan/UBSan audio, leak detection on | 2/2 | [log](native-audio-leaks.log), [XML](native-audio-leaks.xml) |
| Clang Debug, ThreadSanitizer audio | 2/2 | [log](tsan-tests.log), [XML](tsan-tests.xml) |

Each full native suite includes 32 CPU, 21 hardware and three platform groups.
The CPU-only build intentionally excludes native platform code; its complete
inventory is 30 groups. No existing build directory or compiler object cache was
reused. Configure/build logs and exact commands are retained alongside the test
reports; see [commands.json](commands.json). The presets' build settings were
reproduced with explicit CMake arguments in new `build/release-validation/` paths.

ASan/UBSan ran with `halt_on_error=1`. The native full suite disables leak detection
because of the existing driver-allocation policy; it is not evidence of a leak-free
graphics driver. The complete CPU-only suite and native audio subset retain leak
detection. No suppressions or timeout increases were added. The longer native
replay/spike checks completed within their existing limits.

The dedicated TSan build instruments project code with `-fsanitize=thread` and
`-fno-omit-frame-pointer`, separately from ASan. `audio_reload` and `audio_transport`
exercise actual worker threads with null/failing ALSA sinks, command ordering,
stalled-consumer bank retention, queue rejection, buffer collection and shutdown.
Both passed with `TSAN_OPTIONS=halt_on_error=1`. This covers the exercised schedules;
it does not instrument installed ALSA internals or prove physical device behavior.

## Clean offline build

A `git archive` of the baseline was extracted into a fresh source directory under
`/var/tmp`, outside the working repository. Its source revision and SHA-256 are
recorded in [offline-source.json](offline-source.json). A new user/network namespace
had only loopback and no routes; [the full log](offline-build.log) records the
namespace and interfaces before configuration. The following completed inside it:

- Fresh GCC Release configure and build of every target with installed tools.
- All **32 CPU groups**, with no failures or skips.
- `release_package`, including archive creation, dependency inventory, checksums
  and mandatory extracted/relocated CPU smoke validation.

After leaving the namespace, all **three package checks** passed, including
relocated OpenGL gameplay, replay and stress scenarios in the real display session:
[log](offline-package-tests.log), [XML](offline-package-tests.xml). Those graphics
checks ran outside network isolation; the build and initial CPU/package validation
ran inside it. No tools or dependencies were downloaded or installed for the run.

The [isolated archive identity](offline-isolated-package.json) and
[subsequently tested archive identity](offline-tested-package.json) retain the
compressed SHA-256 and size before/after CTest regenerates the archive. The
[build identity](offline-tested-BUILD_INFO.txt.log),
[runtime dependency inventory](offline-tested-RUNTIME_DEPENDENCIES.txt.log) and
[payload hashes](offline-tested-FILES.sha256.log) are retained too. The initial
isolated versions have the corresponding `offline-isolated-` filenames.

Because an archived source tree lacks Git metadata, the embedded/package revision
is `unknown`. The retained source revision and source/archive hashes provide its
provenance; this is not an unidentified source build. Archive timestamps may change
compressed hashes between invocations. No bit-identical archive or cross-distribution
runtime claim is made. The later package containing these validation records is
checked separately after committing its documentation/install changes.

## Environment and reproducibility

[Environment record](environment.log): Omarchy 4.0.6, Linux 7.2.8-9-omarchy x86-64,
GCC 16.2.1, Clang 23.1.3, CMake 4.4.4, Ninja 1.13.2 with the recorded Kitware build
suffix, binutils 2.47, Xlib 1.8.14 and ALSA 1.2.16.1. Native graphics logs identify
Intel HD Graphics 4400, OpenGL 4.6 Core and Mesa 26.2.5 through Xlib/GLX on the
existing XWayland session (`DISPLAY=:0`). These observations do not establish native
Wayland or another GPU/distribution's compatibility.

Commands ran sequentially with `-j2` compilation using preinstalled tools, including
the repository-local `.tools/bin` on PATH. The matrix used the same source baseline
throughout. The later offline build uses an archived copy of that baseline, so
documentation/install edits made while it runs cannot enter its source tree.

To reproduce, check out the named source revision with the documented dependencies,
follow the [audit recipes](../../docs/RELEASE_READINESS.md), and use empty build and
evidence directories. Substitute your paths for the original absolute paths in
the command record. Preserve each exit code and any failure logs. Hardware tests
require a working display; a skip cannot reproduce a passing hardware result.
Sanitizer timings are test durations, not performance acceptance measurements.

## Remaining release work

Current compiler/sanitizer coverage, concurrent audio instrumentation and clean
offline build/relocation now have retained evidence for this baseline. Physical monitor/WM/layout checks, audible
output and physical device lifecycle, candidate workload/memory acceptance,
persistence filesystem-failure coverage and final support/coverage signoff remain
open. Content-remapping and automatic audio-device reconnection are not introduced.
See [release readiness](../../docs/RELEASE_READINESS.md) for gate definitions.
