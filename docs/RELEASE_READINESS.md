# Release-readiness audit

Audit date: **2026-10-23**. Implementation baseline: **`9b9b99f`** (increment 52).
The engine has a playable game, automated coverage and a relocatable package, but
**the release gates are not complete**. This audit changes no runtime behavior or
requirements. [ENGINE_PLAN.md](../ENGINE_PLAN.md) defines the requirements;
the [F01–F16 manifest](STATUS.md#feature-coverage-manifest) maps delivered behavior
to scenarios and tests.

Follow-up: [candidate validation at `1dfba31`](../validation/2026-10-23/RESULTS.md)
retains fresh compiler/sanitizer, concurrent-audio and clean offline-build results. The evidence table
below preserves the original audit snapshot; gate states reflect the follow-up.
The [atomic-save follow-up](../validation/2026-10-24-atomic-writes/RESULTS.md) adds
deterministic filesystem failures and fixes at `be16f3e`; it retains separate
full GCC and focused Clang/CPU sanitizer results without relabeling older evidence.
The [directory-preparation follow-up](../validation/2026-10-24-storage-directories/RESULTS.md)
at `aa53ca4` covers ancestor creation/synchronization, errors, process-interruption
retry and consistent read/write normalization. It documents stricter ancestor-access
requirements and synchronous I/O.

Integrated refresh: [2026-10-29 integrated evidence](../validation/2026-10-29-readiness/RESULTS.md)
at `6d0da8f`, with runtime sources unchanged from merge `855805d`, passes 253 test
group executions: full GNU Debug/Release and Clang suites, complete CPU ASan/UBSan,
17 focused native sanitizer groups, two leak-enabled audio groups and two TSan
audio groups. Preset build trees were reused; only the TSan build directory was
new. This is not a fresh offline build or a full current native sanitizer suite.
The first Debug attempt at `1fc0903` retained one failed GPU-overlay pixel test;
`6d0da8f` corrects its stale tiny-viewport scale expectation and checks the lower
panel boundary. Engine/example runtime sources remain unchanged.
The original audit evidence table remains historical; the gate table records the
new scopes without replacing earlier failures or physical acceptance requirements.

Latest follow-up: [fresh native sanitizer and offline validation](../validation/2026-10-29-offline/RESULTS.md)
at archived source `4f60312` passes the complete 66-group native ASan/UBSan suite
and two leak-enabled audio groups. A separate new Release build completes with
networking disabled, passing all 36 CPU groups and mandatory headless package
checks inside isolation; three relocated package groups pass afterward in the
real display session. Both build directories are fresh. Source/package hashes
retain provenance for archived binaries whose embedded revision is `unknown`.
This refresh changes no engine, example, test, profiling or build rules.

Increment 76 adds a [bounded repeated-session fixture](SESSION_VALIDATION.md) at
`74ddf5a`, with 64 complete routes, reload rollback, private saves and eight audio
worker lifetimes. [Twenty-eight selected groups](../validation/2026-10-30-long-session/RESULTS.md)
pass across GNU Debug/Release, Clang and native ASan/UBSan. It adds automated R6
composition coverage; accelerated ticks, null audio and hidden sprite rendering
do not establish wall-time endurance, audible output or physical acceptance.

“Evidence available” below applies to the stated revision and environment only.
“Open” means missing evidence or unfinished acceptance work, not a reproduced code
failure. Unconfirmed WM minimization and rejected historical timing runs are called
out separately. A skipped test, missing binary or sanitizer startup failure cannot
close a gate. Earlier passing subsets do not certify later changes.

## Evidence inspected in the original audit

| Evidence | Result and limits |
| --- | --- |
| Increment 52 GCC Debug / Release | 56/56 runtime groups in each, no skips; 72.19 s / 93.39 s. Logs: `build/validation-json-{debug,release}-tests.log`. These are local ignored artifacts, summarized in [status](STATUS.md#increment-52-versioned-json-asset-validation-reports), not portable evidence attachments. |
| Increment 52 CPU ASan/UBSan | Five groups passed with leak detection: asset validation, validator CLI, CPU stress, replay reports and spike reports. `build/validation-json-sanitize-tests.log`; 14.35 s. This is a focused subset. |
| Baseline merge package | Three package checks passed at `9b9b99f`, including relocated OpenGL gameplay; 23.34 s. `build/validation-json-merge-package-tests.log` and generated `package-build-info.txt` identify GNU 16.2.1, Release, sanitizers off. |
| Current test inventory | Release discovers 59 tests: 56 runtime plus three packaging fixtures. The platform-disabled sanitizer configuration discovers 30; several binaries are absent in the existing build directory. Discovery is not execution. |
| Earlier compiler / native sanitizer evidence | Increment 36 records eight focused Clang and native ASan/UBSan groups; later increments mainly record focused CPU sanitizer runs. The fresh Clang Release package from increment 17 predates recent changes. A full current matrix remains open. |
| Retained performance evidence | [1080p synthetic stress](../benchmarks/2026-10-16-validation.md), [recorded gameplay](../benchmarks/2026-10-17-replay-results.md), [elapsed-query follow-up](../benchmarks/2026-10-17-elapsed-timing.md), and [scene-budget results](../benchmarks/2026-10-21-scene-budgets-results.md) link raw reports and exact commands/revisions. These are historical measurements, not measurements of this audit revision. |
| Physical platform / audio evidence | [Platform validation](PLATFORM_VALIDATION.md) and [audio validation](AUDIO_VALIDATION.md) explicitly retain outstanding manual checks. ALSA accepted frames and client unmapping do not prove audibility or WM minimization. |

The tested development configuration is Linux x86-64 on an i5-4300U / Intel HD
4400, using Xlib/GLX through XWayland and ALSA. The October 21 reports identify Mesa
26.2.5 and a managed 1050×1360 viewport. Older reports use their own recorded driver
versions. No native Wayland, second GPU or older-distribution certification follows
from these results. Broader configurations require their own evidence before being
advertised; a second GPU is not a substitute for finishing checks on the testing PC.

## Remaining gates

| Gate | State | Required closure / evidence | Execution |
| --- | --- | --- | --- |
| R1: candidate compiler and sanitizer matrix | Full baseline suite at `4f60312`; new session fixture validated separately | Fresh GNU native ASan/UBSan passes all 66 runtime groups, plus two leak-enabled audio groups. The prior integrated refresh supplies full GNU Debug/Release and Clang (66 each), all 34 CPU sanitizer groups and two audio TSan groups. All pass without skips. Those prior compiler trees were reused; no fresh full cross-compiler matrix is claimed. | [Fresh native suite](../validation/2026-10-29-offline/RESULTS.md), [integrated matrix](../validation/2026-10-29-readiness/RESULTS.md), [session subset](../validation/2026-10-30-long-session/RESULTS.md). |
| R2: concurrent audio validation | Refreshed evidence at `6d0da8f` | A new Clang TSan build passes both audio groups without reports, using null/failing sinks. This covers exercised queue, buffer-retirement and shutdown schedules, not all interleavings or uninstrumented ALSA internals. | No physical sound claim; [race results](../validation/2026-10-29-readiness/tsan-audio-tests.xml). |
| R3: clean offline build and candidate package | Fresh archived-source evidence at `4f60312` | New full GNU Release build, 36 CPU groups and mandatory headless package validation pass inside network isolation. Three package groups, including relocated OpenGL scenarios, pass afterward in the real display session. Source/archive hashes identify the build; embedded revision is `unknown` without Git metadata. Final Git-stamped candidate packages are checked separately. | [Offline provenance and package audit](../validation/2026-10-29-offline/RESULTS.md); older `1dfba31` evidence remains historical. |
| R4: display, input and interactive game acceptance | Open; WM minimize was unconfirmed (probe exit 2) | Finish physical scale/monitor, supported WM minimize/restore, focus, layout switching and title/settings/gameplay checks. Verify camera, clipping, rebinding, save/relaunch, level transitions and visible shader failure/recovery. Record each case, configuration and result. | Human observation and a suitable WM/monitor configuration. |
| R5: physical sound and device lifecycle | Open | Listen to channel/gain/mute/pause/stop and gameplay cues. Exercise physical disconnect and system suspend/resume in an arranged session. Confirm continued play and useful disabled-audio diagnostics on terminal failure. Record device/backend and outcome; automatic reconnection is not implemented or required by the current contract. | Human observation and physical device access. |
| R6: declared performance and memory acceptance | Open; declared memory checks pass, timing fails at `d4dbe22` | [Six workloads / 18 attempts](../benchmarks/2026-10-26-workload-acceptance-results.md) retain predeclared ceilings, kernel peak RSS and a storage coverage audit. All memory checks pass; one p99 and fourteen worst-frame checks exceed 16.7 ms. Investigate paired-frame causes, then repeat without discarding failures; extend interactive/audio/error/long-session coverage and physical 1080p acceptance. | Measurements are automated; physical presentation/refresh checks require observation. |
| R7: persistence failure and compatibility boundary | Implemented syscall contract has evidence; physical durability unverified | Atomic-write failures are covered at `be16f3e`; `aa53ca4` adds ten directory cases covering full resolved ancestry before publication, faults, retry after process exit and matching read/write normalization. Existing content rejection remains unchanged. Physical power-loss/filesystem qualification requires an isolated disposable fixture if that guarantee is claimed. | [Atomic faults](ATOMIC_SAVE_VALIDATION.md) and [directory preparation](STORAGE_DIRECTORY_VALIDATION.md); no physical power-loss guarantee established. |
| R8: release coverage and support record | Open | Close applicable R1–R7 cases, map each supported subfeature to named checks, retain manual results and unresolved failures, then publish the supported configuration and candidate artifact hash. Review scenario substitutions against the plan instead of treating one passing room or test-group count as complete coverage. | Documentation after evidence collection; unresolved scope decisions remain explicit. |

R6 does not require inventing a whole-process allocation-free promise or attributing
driver caches to engine ownership. Existing tracked-payload evidence is useful but
cannot establish the unmeasured bounds. The 1080p synthetic repeats had frame p99
below 16.7 ms and maxima above it; their preview window was 1050×1360. Gameplay
profiles used the managed viewport, not a 1920×1080 gameplay surface. Neither
establishes sustained 60 FPS at a physical 1080p display. Preserve rejected timing
runs; the elapsed-query follow-up supersedes the timestamp-pair measurement method,
not the historical failures.

R7 preserves the outstanding F14 item without inventing a requirement to migrate
arbitrary edited content. Current configuration/save version handling is tested;
changed gameplay content invalidates saves/replays and there is no content-remapping
system. The current interrupted-write test exits a child after file sync and after
rename, and checks an injected pre-rename exception. It does not simulate lost
power, failed storage flushes or every filesystem syscall failure. Keep those
distinctions in release notes until the remaining policy/coverage is resolved.

## Reproduce the automated checks

Run from a clean candidate checkout with installed CMake, Ninja, GCC/Clang and the
documented X11/GLX/ALSA development libraries. If using the local tool installation,
prepend its absolute `.tools/bin` path. Preserve the session's actual `DISPLAY`;
do not assume `:0` on another PC. Save stdout/stderr and exit status for each command
in a new evidence directory. The original audit did not run these recipes. The
[follow-up](../validation/2026-10-23/RESULTS.md) records actual commands and results
in fresh build paths; use these recipes to refresh evidence for later candidates.

Use a fresh disk-backed scratch directory outside the checkout for compiler and
package temporary files. Earlier runs retained an explicit `/tmp` quota failure
and a separate unconfirmed extraction corruption; do not reuse that storage
choice. This setup changes only the current shell's compiler scratch environment:

```sh
mkdir -p "${XDG_CACHE_HOME:-$HOME/.cache}"
audit_scratch=$(mktemp -d "${XDG_CACHE_HOME:-$HOME/.cache}/engine-validation.XXXXXX")
mkdir "$audit_scratch/compiler-tmp"
export TMPDIR="$audit_scratch/compiler-tmp"
```

### R1: build every target before testing

```sh
cmake --preset debug -DENGINE_PACKAGE_TEST_ROOT="$audit_scratch/packages"
cmake --build --preset debug -j2
ctest --preset debug -LE packaging --output-on-failure
cmake --preset release -DENGINE_PACKAGE_TEST_ROOT="$audit_scratch/packages"
cmake --build --preset release -j2
ctest --preset release -LE packaging --output-on-failure
cmake --preset clang -DENGINE_PACKAGE_TEST_ROOT="$audit_scratch/packages"
cmake --build --preset clang -j2
ctest --preset clang --output-on-failure
cmake --preset sanitize -DENGINE_PACKAGE_TEST_ROOT="$audit_scratch/packages"
cmake --build --preset sanitize -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --preset sanitize --output-on-failure

cmake -S . -B build/sanitize-platform -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DENGINE_SANITIZERS=ON \
  -DENGINE_PACKAGE_TEST_ROOT="$audit_scratch/packages"
cmake --build build/sanitize-platform -j2
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize-platform --output-on-failure
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize-platform --output-on-failure \
    -R '^(audio_transport|audio_reload)$'
```

Native graphics previously disabled leak detection because of driver allocations;
that run checks address/undefined behavior only. Retain CPU and audio leak-enabled
results separately. Do not suppress an engine report to make the aggregate pass.
After the repeated-session addition, the current inventory is 67
native runtime groups (36 CPU, 27 hardware, four platform), plus three packaging
fixtures; platform-disabled builds discover 34 CPU groups. Discovery is not execution.
The [current refresh](../validation/2026-10-29-readiness/RESULTS.md) runs every GNU/Clang
runtime group and CPU sanitizer group, with a focused native sanitizer subset.
The [fresh follow-up](../validation/2026-10-29-offline/RESULTS.md) completes the
then-current 66-group native sanitizer suite from archived source `4f60312`.
The added `long_session` group receives targeted validation separately; the full
67-group suite has not been refreshed by that addition.
The full fresh-matrix record at `1dfba31` predates these additions; later storage and
[allocation validation](../validation/2026-10-24-allocations/RESULTS.md) record their
own full/focused scopes. Update counts from
the candidate's inventory if tests are added.

### R2: dedicated ThreadSanitizer build

```sh
cmake -S . -B build/tsan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ -DENGINE_SANITIZERS=OFF \
  -DCMAKE_CXX_FLAGS='-fsanitize=thread -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread
cmake --build build/tsan --target audio_tests audio_reload_tests -j2
TSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/tsan --output-on-failure \
    -R '^(audio_transport|audio_reload)$'
```

Use a separate build directory; TSan must not be combined with ASan. These tests
exercise actual worker threads with ALSA null/failing sinks, including stalled
consumer bank ownership. They cover exercised interleavings, not all possible
schedules or uninstrumented system-library internals. Record any timeout or TSan
startup failure before deciding whether the host or tests need adjustment.

### R3: fresh offline snapshot and package

One Linux option, **where unprivileged network namespaces are available**, is:

```sh
audit_tools=$(dirname "$(command -v cmake)")
audit_root=$(mktemp -d "$audit_scratch/offline.XXXXXX")
audit_revision=$(git rev-parse HEAD)
mkdir "$audit_root/source"
git archive "$audit_revision" > "$audit_root/source.tar"
tar -xf "$audit_root/source.tar" -C "$audit_root/source"
printf '%s\n' "$audit_revision" > "$audit_root/SOURCE_REVISION.txt"
unshare --user --map-root-user --net env PATH="$audit_tools:$PATH" \
  AUDIT_SOURCE="$audit_root/source" AUDIT_BUILD="$audit_root/build" \
  AUDIT_PACKAGE_ROOT="$audit_root/packages" sh -eu -c '
    cmake -S "$AUDIT_SOURCE" -B "$AUDIT_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DENGINE_PACKAGE_TEST_ROOT="$AUDIT_PACKAGE_ROOT"
    cmake --build "$AUDIT_BUILD" -j2
    ctest --test-dir "$AUDIT_BUILD" -L cpu --output-on-failure
    cmake --build "$AUDIT_BUILD" --target release_package
  '
ctest --test-dir "$audit_root/build" -L packaging --output-on-failure
```

The isolated phase needs no display. Run graphics checks after leaving the network
namespace in the real display session. If `unshare` is unavailable, use a recorded
network-disabled VM/container with dependencies preinstalled; an ordinary cached
build does not close the offline gate. No package installation belongs in the
isolated build. A `git archive` has no Git metadata, so its embedded revision is
`unknown`: preserve `SOURCE_REVISION.txt` and the source archive hash alongside its
logs. Separately build the final candidate from a clean Git checkout so its package
records the commit, following [packaging](PACKAGING.md). Archive byte identity is
not promised across timestamps/toolchains; reproducible build instructions and
traceable artifacts are the required evidence here.

### R6: baseline workloads and R7: current persistence coverage

```sh
./build/release/feature_lab --stress --width 1920 --height 1080 \
  --frames 600 --warmup 60 --cycles 3 --report build/release-stress.json
./build/release/feature_lab --replay benchmarks/replays/facility.erpl \
  --profile-report build/release-facility.json --profile-warmup 30
./build/release/feature_lab --replay benchmarks/replays/annex.erpl \
  --profile-report build/release-annex.json --profile-warmup 30
./build/release/feature_lab --spike-report build/release-spikes.json
ctest --preset release -R '^persistence$' --output-on-failure
```

Use distinct filenames for repeats; retain failures and exact commands. Require
complete reports, verified routes/checksums, bounded resource restoration and
valid query accounting before comparing timings. Existing commands do not force a
1080p managed gameplay viewport. Use the dedicated
[allocation executables](../benchmarks/ALLOCATION_PROFILING.md) for C++ allocation
counts; they exclude C-library allocators, driver/GPU storage and live-heap peaks.
The storage fault/preparation tests now cover the documented syscall contracts;
physical power-loss acceptance remains open. See [stress](../benchmarks/README.md),
[replay](../benchmarks/REPLAY_PROFILING.md) and
[spike](../benchmarks/SPIKE_PROFILING.md) contracts for measured scopes/exclusions.

## Physical and interactive acceptance record

Start R4 with `./build/release/platform_probe --exercise` and the exact cases in
[platform validation](PLATFORM_VALIDATION.md). Exit 2 means a WM operation remains
unconfirmed; client unmap/restore does not replace it. In a private user-data
directory, launch `./build/release/feature_lab --user-data /tmp/feature-lab-manual-SESSION`.
Replace `SESSION` with a unique identifier. Exercise title/settings/gameplay with
keyboard and mouse, held controls across focus/modal changes, physical layout
switching, camera/zoom, fullscreen/scale, both levels, and save/quit/relaunch.
Use F6 then F5 to check visible shader failure and recovery without editing files.

For R5 use the opt-in `./build/release/audio_probe --exercise` listening sequence
and the gameplay/device checklist in [audio validation](AUDIO_VALIDATION.md).
Arrange suspend/disconnect checks with the person using the PC; this audit does not
perform them. Record audible clicks/gaps on reload, disabled-audio behavior and
whether restart restores output, rather than expecting automatic reconnection.

For each case retain: candidate SHA and package SHA-256 if used, timestamp,
tester, kernel/toolchain, GPU/driver, display server/WM, resolution/scale/refresh,
keyboard layout or ALSA device, exact steps, expected outcome, observed outcome,
pass/fail/unavailable, and log/capture paths. A terminal error is evidence, not a
reason to omit the case. Transfer relevant ignored logs into the release evidence
bundle; paths on the development machine alone are insufficient for review.

## Deferred extensions and next increment

Background loading, native Wayland, extra formats, Unicode shaping, gamepads,
mipmaps/anisotropy, a visual editor and advanced rigid-body simulation remain
deferred or outside v1. None closes the open release checks above. Optional JSON
validation/profiling reports do not add a JSON asset-format requirement.

R1–R3 retain evidence for their named baseline; the storage follow-ups supply newer
scoped evidence for R7. Increment 57 adds
[repeatable render/audio C++ allocation measurements](../benchmarks/2026-10-24-allocation-results.md)
for R6, with warmup, restart/formatting exceptions and explicit exclusions.
Increment 58 [reuses HUD storage](../benchmarks/2026-10-24-hud-text-results.md), with
zero measured world/HUD C++ allocations in those replays and unchanged rendered
output. Increment 59 [measures UI allocation coverage](../benchmarks/UI_ALLOCATIONS.md)
for open settings/controls, diagnostics and enabled-audio labels. Increment 60
[reuses diagnostic text](../benchmarks/2026-10-26-diagnostic-text-results.md) and
compares synthetic GPU-label and combined-overlay fixtures as well as the original
UI workload. All measured render-thread scopes are zero after warmup in these
fixtures; menu-transition costs remain separate. Increment 61 adds the
[declared acceptance record](../benchmarks/2026-10-26-workload-acceptance-results.md):
all 18 runs pass memory ceilings, while one p99 and fourteen frame maxima fail
16.7 ms. The [coverage audit](../benchmarks/WORKLOAD_ACCEPTANCE.md#storage-coverage-audit)
keeps process RSS, tracked payloads and cumulative allocation counts distinct.
Increment 62 [retains paired stress frames](../benchmarks/2026-10-27-frame-samples-results.md):
15 of 17 traced outliers have presentation as their largest CPU interval and two
have submission. This localizes elapsed time without proving its underlying cause.
Increment 63 [compares explicit presentation requests](../benchmarks/2026-10-27-presentation-results.md):
immediate medians are 3.18–3.50 ms and requested VSync medians 16.66–16.67 ms,
with all maxima still above 16.7 ms. Issued requests do not confirm physical synchronization.
Increment 64 [splits submission CPU intervals](../benchmarks/2026-10-27-submission-results.md)
into preparation/bookkeeping, GL setup and batch upload/draw, with six retained
on/off attempts. These are CPU wall intervals, not isolated GPU execution costs.
Preparation/other dominates 29 of 30 retained largest submissions; all maxima still
fail the frame reference. Increment 65 [profiles preparation phases](../benchmarks/2026-10-27-preparation-results.md)
with calling-thread CPU time and signed wall-minus-CPU gaps. All six attempts and
nested phase/GL partitions are retained; these do not establish a hot instruction
or attribute a gap specifically to scheduler delay. In 23 of 30 largest profiled
submissions the preparation wall-minus-CPU gap exceeds its CPU duration. Three p99
values and all six maxima fail 16.7 ms. Increment 66 [adds thread switch counters](../benchmarks/2026-10-28-context-results.md)
with six retained attempts and phase/frame category distributions. Spans include
GL work and wider boundaries, so counts cannot isolate non-GL waits or their cause.
All 78 preparation gaps above 1 ms accompany involuntary switches, while 937 other
frames with involuntary switches have smaller gaps; no duration or root cause is
assigned to individual switches. All six maxima still fail 16.7 ms.
Increment 67 [tests quad-corner reuse](../benchmarks/2026-10-28-corners-results.md):
correctness passes, but the predeclared CPU gate fails, so the renderer is restored.
All six new frame maxima exceed 16.7 ms; presentation dominates all 34 slow frames.
Increment 68 [collects main-thread function samples](../benchmarks/2026-10-28-functions-results.md):
quad submission leads application symbols consistently, with no lost/throttled
samples and matching normal/profiled pixels. Whole-process sample scope and
unresolved driver symbols limit interpretation; all six frame maxima still fail.
Increment 69 [tests fixed-capacity vertex emission](../benchmarks/2026-10-28-vertices-results.md):
all correctness checks pass, but inconsistent paired CPU changes fail the declared
adoption gate. The renderer is restored; new capacity-boundary regression tests
remain. Every frame maximum still fails. Investigate run-to-run stability before
another optimization trial.
Increment 70 [tests warmup/order stability](../benchmarks/2026-10-28-stability-results.md):
neither warmup protocol meets its predeclared stability screen. Read-only telemetry
retains a late low-frequency/CPU-time transition, throttle counters and implausible
ACPI readings without causal attribution. Four of sixteen p99 values and thirteen
maxima fail. Runtime remains unchanged; renderer memory accounting is the next
concrete coverage task while host-dependent timing stays unresolved.
Increment 71 [adds renderer memory snapshots](RENDERER_MEMORY.md), with
[validated](../validation/2026-10-29-renderer-memory/RESULTS.md) CPU vertex capacity,
logical VBO size and built-in/font/user/target texel accounting. This narrows the
renderer inventory gap for new reports; old reports retain their original scope.
CPU font snapshots, allocator/driver overhead and retired GPU stores remain outside
coverage. No timing budget or whole-engine memory gate is closed.
Increment 72 [covers committed CPU font buffers](../validation/2026-10-29-font-memory/RESULTS.md)
in coverage-v2 renderer snapshots. Atlas/glyph reservations and used bytes remain
separate from candidate staging. Ownership/rollback tests and 32 focused groups
pass, with 36 matching images; object/lookup storage, allocator/driver overhead and
retired stores remain excluded. No whole-engine live-memory or timing gate closes.
Increment 73 [exposes the scoped renderer inventory in F2](../validation/2026-10-29-diagnostics-memory/RESULTS.md).
CPU reserved/used values and logical GPU payloads remain separate from RSS and
staging peaks. The overlay samples before its own drawing and reuses HUD storage;
this observability change does not extend inventory coverage or establish a
performance improvement. Tiny offscreen captures do not replace physical display
checks. The next evidence refresh must retain these limits.
R6 timing/coverage and physical R4/R5 cases remain open; R8 requires their
coverage/support record. Refresh automated evidence when the implementation or
toolchain changes. These results do not complete milestone 6.

Increment 74 [refreshes the integrated automated evidence](../validation/2026-10-29-readiness/RESULTS.md).
All 253 scoped compiler/sanitizer/race group executions pass without skips. The
package audit checks relative file links and shell syntax, then runs bounded CPU
entry points outside the package directory with the display unset. It does not
test external URLs, heading anchors, interactive recipes or physical devices.
Increment 75 [completes that full native and fresh offline follow-up](../validation/2026-10-29-offline/RESULTS.md)
for archived source `4f60312`, with 107 passing CTest group executions and explicit
network/source/package provenance. R4/R5, R6 timing/coverage and R8 support/signoff
stay open. The repeated-session follow-up is recorded below.

Increment 76 [composes repeated gameplay/reload/audio coverage](SESSION_VALIDATION.md).
Each of four targeted builds completes 64 routes with matching deterministic
counts and restored tracked resources. The successful Debug evidence is retained
across a capture correction; the initial save-corruption fixture failure stays
separate. This narrows repeated-lifecycle coverage gaps but leaves full HUD/settings,
physical devices, wall-time endurance, timing and unmeasured memory acceptance open.
