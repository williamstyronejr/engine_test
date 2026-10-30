# Full native sanitizer and fresh offline validation — 2026-10-29

Tested source: **`4f603120812a8b3eca2d2ad7e014329e2e536b19`** (increment 74 merge).
This increment adds validation records and documentation; engine, examples, tests,
profiling and build rules are unchanged. Both builds use a fixed `git archive` of
that commit in new directories outside the repository. No compiler objects or
preset build directories were reused for these two builds.

## Source and environment

[Source provenance](source.json) records the full revision, source tar SHA-256,
size and disk-backed extraction path. [Extraction verification](source-extraction-check.json)
compares every regular file's bytes with the source archive, using
[verify_source.py](verify_source.py). Because this tree has
no Git metadata, its native and Release binaries correctly embed revision
`unknown`; the recorded archive ties them to the named source commit.

[Environment](environment.log) and [exact commands](commands.json) identify the
installed compiler, CMake, Ninja, system libraries and display stack. Builds run
sequentially with two compiler jobs. Compiler temporaries and package extractions
use private disk-backed directories below the user's cache, outside the repository.
No dependencies or tools were downloaded or installed during validation.

## Complete native ASan/UBSan suite

The fresh GNU Debug build compiles every target with address and undefined-behavior
sanitizers. All **66 runtime groups** pass without skips: 36 CPU, 26 hardware and
four platform groups. See the [inventory](native-sanitize-inventory.json),
[test log](native-sanitize-tests.log), [XML](native-sanitize-tests.xml),
[configure](native-configure.log) and [complete build log](native-build.log).

Both sanitizers use `halt_on_error=1`. Leak detection is disabled for this complete
native suite under the existing graphics-driver policy; it does not establish
leak-free graphics drivers or whole-process memory coverage. The separate native
[audio subset](native-audio-leaks-tests.xml) passes both groups with leak detection
enabled. These sanitizer runs needed no suppressions, timeout increases, retries
or engine/test fixes.
The prior [complete CPU leak and audio TSan evidence](../2026-10-29-readiness/RESULTS.md)
remains separately scoped to its named revision; it is not rerun or relabeled here.

## Fresh offline Release build

The first isolation check failed **before CMake configuration**: its
[driver](isolated-initial.py) enumerated `/sys/class/net`, which reflects the host's
shared sysfs mount even inside the new network namespace. It saw the host Wi-Fi
name and stopped; [failure log](offline-build.log) and
[initial observation](initial-network.json) are retained. This was a validation
harness error, not a compiler or engine failure. No offline build existed yet.

The corrected driver enumerates interfaces through `socket.if_nameindex()` and
queries routes through netlink with `ip -json`, verifies a namespace distinct from
the parent, and asserts only loopback and no non-loopback routes. It then resumes
only the offline phase; the completed sanitizer results are retained unchanged.
The command record includes the failed attempt and corrected run. No failed
build/test result is replaced or counted as a pass.

A new unprivileged user/network namespace surrounds the entire configure, full
build, CPU test and initial package sequence. [Network topology](network.json)
records only loopback and no IPv4 routes; any IPv6 routes are confined to loopback.
The corrected driver asserts this topology and unsets DISPLAY before configuration.
[Isolated driver](isolated.py) and [full output](offline-corrected-build.log) retain the exact
commands and their ordered results.

Inside network isolation, a fresh GNU Release build of every target succeeds;
all **36 CPU groups** pass with no failures or skips, followed by `release_package`
and its mandatory extracted/relocated headless smoke checks. See the
[CPU inventory](offline-cpu-inventory.json) and [CPU XML](offline-cpu-tests.xml).

After leaving isolation, all **three package groups** pass without skips, including
relocated OpenGL gameplay/replay/stress checks in the real display session:
[log](offline-package-tests.log), [XML](offline-package-tests.xml).
Graphics checks ran outside network isolation. This is evidence for the build PC's
Linux ABI and GLX/XWayland stack, not another distribution, GPU or native Wayland.

The initial [isolated package identity](offline-isolated-package.json) and
[CTest-regenerated package identity](offline-tested-package.json) retain compressed
SHA-256 and sizes before/after package tests. Their embedded build records,
dependency inventories and payload manifests are retained alongside them. Different
compressed hashes can result from archive timestamps; bit-identical reproduction
is not claimed.

The [relocated documentation/command audit](offline-audit/summary.json) checks
1,055 relative Markdown file links, 75 shell recipes and eight bounded CPU entry-point
commands, bracketed by two successful payload-manifest checks. The extraction path
contains spaces, the working directory is unrelated, and DISPLAY and library-path
overrides are unset. External URLs, heading anchors, interactive recipes and
physical device behavior are excluded. CPU stress here checks a documented command,
not performance acceptance.

## Scope and reproduction

All **107 CTest group executions** pass: 66 full native sanitizer, two leak-enabled
audio, 36 offline CPU and three relocated package groups. [Checks](checks.json)
retain their exact names; counts describe test groups, not assertions. Native
sanitizer and offline Release directories were fresh. The final Git-stamped
candidate/merge package checks are separate from these archived-source builds.

Run [run_validation.py](run_validation.py), then `python3 validation/2026-10-29-offline/verify_source.py`,
from the repository root with installed dependencies, a working display and unprivileged network namespaces. It fixes the
source revision and creates fresh cache scratch. Use a new evidence directory for
a repeat: it refuses to overwrite an existing command record. The narrowly scoped
`--resume-offline` mode is only for the retained pre-configuration harness failure;
it refuses an existing offline build directory and appends commands to the record.
The source archive and built binaries remain local; portable provenance and test logs are retained
here. A missing namespace or skipped graphics group cannot reproduce this result.

Physical display/input (R4), audible device/lifecycle behavior (R5), unresolved
frame-time and broader memory/long-session coverage (R6), and final support/signoff
(R8) remain open. No physical power-loss guarantee is added. Historical timing
failures and rejected optimization trials remain unchanged. Background loading
remains deferred; these checks do not complete milestone 6.

## Git-stamped candidate package

At documentation commit **`60c4e8e`**, all three Release package groups pass without
skips (70.73 s): [test log](package-feature-tests.log),
[XML](package-feature-tests.xml) and [commands](package-feature-commands.json).
The archive embeds the commit identity. All 53 then-current evidence files and
five changed documentation files match their archived copies byte for byte;
the four allocation profilers remain excluded from installation.

The [candidate archive audit](package-candidate/summary.json) verifies 1,090 relative
file links, 75 shell recipes and eight bounded CPU entry-point commands, with two
successful manifest checks. Its compressed SHA-256 is recorded in that summary.
The later merge receives the same three package checks and a complete content/audit
check after adding this record; those final merge artifacts remain under
`build/offline-refresh/` on the validation machine. These package checks reuse the
Release preset build directory and are distinct from the two fresh builds above.
