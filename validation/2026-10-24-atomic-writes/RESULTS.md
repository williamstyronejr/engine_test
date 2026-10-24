# Atomic-save regression validation — 2026-10-24 UTC

Implementation under test: **`be16f3e`**, based on `1362d49`. The changes add an
isolated syscall fault suite and fix two error-reporting defects in `write_atomic`:
zero-progress writes now report `EIO`, and parent-directory close failures now
report an error after replacement. File formats and public API signatures did not
change. See the [failure contract and cases](../../docs/ATOMIC_SAVE_VALIDATION.md).

## Reproduced failures

The initial eight-case regression run against the previous implementation failed
two cases: zero-progress writes used zero/stale errno, and directory-close failure
returned success. The other six cases passed. The original CTest exit was 8;
[the failing output](before-fix-tests.log) is retained rather than overwritten.
It ran with the newly added test target before the implementation fix was committed.

After the fixes, the suite passed. Coverage was also extended to interrupted
directory sync/close and descriptor reuse after either file or directory close.
The [focused Debug result](focused-debug-tests.log) records the final eight-case
suite before committing the implementation. Subsequent XML reports below contain
each named case's output at the committed revision.

## Verification

All post-fix runs passed without skips, sanitizer findings, suppressions or timeout
changes. Configurations reuse the existing builds from the preceding full matrix;
they were reconfigured and rebuilt for this revision, not claimed as fresh builds.

| Configuration | Passed | Evidence |
| --- | --- | --- |
| GCC 16.2.1 Debug, full runtime | 57/57 groups | [log](gcc-debug-tests.log), [XML](gcc-debug-tests.xml) |
| GCC 16.2.1 Release, full runtime | 57/57 groups | [log](gcc-release-tests.log), [XML](gcc-release-tests.xml) |
| Clang 23.1.3 Debug, atomic write/persistence/content | 3/3 groups | [log](clang-debug-tests.log), [XML](clang-debug-tests.xml) |
| GCC CPU ASan/UBSan, same three groups, leak detection on | 3/3 groups | [log](cpu-sanitize-tests.log), [XML](cpu-sanitize-tests.xml) |

[Commands and exit codes](commands.json), [cached configuration settings](configurations.json)
and [environment](environment.log) retain the actual compiler/build/test inputs.
The command record identifies the committed implementation; the older failing run
is deliberately separate. The new `atomic_write` group contains eight named tests,
including loops over error codes and existing/absent destinations. The full native
suite now has 33 CPU, 21 hardware and three platform groups. Release packaging adds
three separate checks and is not counted in the 57 runtime groups.

Only `atomic_write_tests` receives linker interception flags. The tests use private
directories and actual file operations plus deterministic faults; they do not fill
a disk, modify mounts or touch user save files. The existing `persistence` tests
also retain child-process interruption checks at both commit boundaries.

This focused follow-up does not rerun or replace the preceding full native
sanitizer, TSan or offline-build evidence at `1dfba31`. Those results keep their
original revision and scope. Documentation/package checks for this increment are
recorded in [implementation status](../../docs/STATUS.md#increment-55-atomic-save-filesystem-failures).

## Remaining limits

The injected errors prove API outcomes, error precedence, descriptor handling,
cleanup and retry behavior for these cases. They do not simulate kernel crashes,
physical power loss, write-cache loss or every filesystem's durability behavior.
Durability of newly created ancestor directories still needs review. Content
fingerprints continue to reject incompatible saves; no content-remapping system
or automatic orphan cleanup was added.
