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
| R1: candidate compiler and sanitizer matrix | Evidence available at `1dfba31` | Fresh GCC Debug/Release and Clang each passed 56 groups; CPU ASan/UBSan passed 30, native ASan/UBSan passed 56 and leak-enabled native audio passed two. No skips. Preserve the explicit leak policy and refresh evidence when implementation/toolchain changes. | Completed on the recorded host; [raw results](../validation/2026-10-23/RESULTS.md). |
| R2: concurrent audio validation | Evidence available at `1dfba31` | Clang TSan passed both concurrent audio groups without reports, using null/failing sinks. This covers exercised queue, buffer-retirement and shutdown schedules, not all interleavings or uninstrumented ALSA internals. | Completed without physical sound output; [raw results](../validation/2026-10-23/tsan-tests.xml). |
| R3: clean offline build and candidate package | Evidence available at `1dfba31` | A fresh archived source built every target with networking disabled; 32 CPU groups and mandatory package smoke checks passed inside isolation. All three package checks passed afterward in the display session. Source/archive hashes, build identity and dependency/payload inventories are retained; the archive's embedded revision is `unknown` because Git metadata was absent. | Completed on the recorded host; [offline results and provenance](../validation/2026-10-23/RESULTS.md#clean-offline-build). |
| R4: display, input and interactive game acceptance | Open; WM minimize was unconfirmed (probe exit 2) | Finish physical scale/monitor, supported WM minimize/restore, focus, layout switching and title/settings/gameplay checks. Verify camera, clipping, rebinding, save/relaunch, level transitions and visible shader failure/recovery. Record each case, configuration and result. | Human observation and a suitable WM/monitor configuration. |
| R5: physical sound and device lifecycle | Open | Listen to channel/gain/mute/pause/stop and gameplay cues. Exercise physical disconnect and system suspend/resume in an arranged session. Confirm continued play and useful disabled-audio diagnostics on terminal failure. Record device/backend and outcome; automatic reconnection is not implemented or required by the current contract. | Human observation and physical device access. |
| R6: declared performance and memory acceptance | Open; bounded historical workloads available | Declare the release workload and budget, repeat it on the candidate, retain actual viewport/target sizes, timing scopes, warmup, queries, resource peaks and limits. Finish render/audio hot-path allocation coverage and document intentional exceptions. Audit remaining untracked storage before claiming whole-engine bounds. | Measurements can be automated; physical presentation/refresh checks require observation. |
| R7: persistence failure and compatibility boundary | Deterministic syscall coverage available at `be16f3e`; remaining durability review open | Eight atomic-write cases cover short/interrupted/zero writes, create/sync/close/rename/open failures, cleanup error precedence, descriptor reuse and retry, alongside process-interruption tests. Review newly created ancestor-directory durability and retain the existing content-rejection boundary. Physical power-loss testing requires an isolated disposable fixture if that guarantee is claimed. | [Fault contract and results](ATOMIC_SAVE_VALIDATION.md); no power-loss guarantee established. |
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

### R1: build every target before testing

```sh
cmake --preset debug
cmake --build --preset debug -j2
ctest --preset debug -LE packaging --output-on-failure
cmake --preset release
cmake --build --preset release -j2
ctest --preset release -LE packaging --output-on-failure
cmake --preset clang
cmake --build --preset clang -j2
ctest --preset clang --output-on-failure
cmake --preset sanitize
cmake --build --preset sanitize -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --preset sanitize --output-on-failure

cmake -S . -B build/sanitize-platform -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DENGINE_SANITIZERS=ON
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
After adding `atomic_write`, the current inventory is 57 Debug/Release/Clang runtime
groups, 31 CPU-only sanitizer groups, 57 native sanitizer groups and two selected
native audio groups. The full-matrix record at `1dfba31` predates this addition;
the later atomic-save validation records its own focused scope. Update counts from
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
audit_root=$(mktemp -d /tmp/feature-lab-offline.XXXXXX)
audit_revision=$(git rev-parse HEAD)
mkdir "$audit_root/source"
git archive "$audit_revision" > "$audit_root/source.tar"
tar -xf "$audit_root/source.tar" -C "$audit_root/source"
printf '%s\n' "$audit_revision" > "$audit_root/SOURCE_REVISION.txt"
unshare --user --map-root-user --net env PATH="$audit_tools:$PATH" \
  AUDIT_SOURCE="$audit_root/source" AUDIT_BUILD="$audit_root/build" sh -eu -c '
    cmake -S "$AUDIT_SOURCE" -B "$AUDIT_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release
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
1080p managed gameplay viewport or add missing allocator/syscall instrumentation;
those remain R6/R7 work. See [stress](../benchmarks/README.md),
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

R1–R3 retain evidence for their named baseline; the atomic-save follow-up supplies
newer scoped evidence for R7. Next: review durable creation of config/save directory
chains and add deterministic failure coverage for that boundary. R6 workload/memory
acceptance and physical R4/R5 cases remain
open; R8 requires their coverage/support record. Refresh automated evidence when
the implementation or toolchain changes. These results do not complete milestone 6.
