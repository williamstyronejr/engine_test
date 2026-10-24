# Atomic-save failure validation

`AssetRoot::write_atomic` creates a sibling temporary file, writes and syncs its
contents, closes it, renames it onto the destination, then opens/syncs/closes the
parent directory. `UserStorage` uses this operation for configuration and save
records. Record versions, data encoding and the public API are unchanged.

## Failure contract

- Short writes advance by the number of bytes actually written. `EINTR` from
  `write` retries the unwritten remainder. A zero-byte write for a nonempty request
  fails with `EIO`, regardless of whether `errno` is zero or contains an older error.
- Other write errors and failed creation, file sync, file close or rename report
  `std::system_error` with the operation and original error code. Before rename,
  the previous destination remains unchanged, or absent if it did not exist.
- A successful rename publishes the complete new bytes. Subsequent directory
  open, sync or close errors report failure **after replacement**; there is no
  rollback. If directory open/sync fails, crash durability is unconfirmed. A
  directory-close error is reported even if the preceding sync succeeded.
- File and directory close calls are not retried after an error: this Linux-only
  implementation must not close a descriptor that has since been reused. See
  the Linux [close(2) error-return guidance](https://man7.org/linux/man-pages/man2/close.2.html#CAVEATS).
- Cleanup is best effort. A secondary close/unlink failure does not replace the
  primary exception. Failed unlink can leave an orphan `.engine-save-*` file;
  normal reads ignore it and later saves use another unique temporary. Automatic
  orphan deletion could race a live writer and is not performed.
- Sync/open/rename errors, including interrupted sync, are reported rather than
  retried by this API. The caller can retry the whole save. Concurrent writers still
  use last-rename-wins; there is no multi-file transaction or conflict resolution.

The parent directory must already exist for `AssetRoot`; `UserStorage` now
[prepares and synchronizes its resolved ancestor chain](STORAGE_DIRECTORY_VALIDATION.md)
before calling this primitive. Real power loss and device/filesystem behavior
after an actual crash remain outside the simulated evidence.

## Isolated syscall tests

The CPU-only `atomic_write` CTest group links `atomic_write_tests` against the real
engine implementation. Linux linker `--wrap` redirects only this test executable's
`mkstemp`, `write`, `fsync`, `close`, `rename`, `open` and `unlink` calls. Normal engine
and game executables have no interception or fault-injection flags.

Each case owns a private temporary directory. Injection is single-threaded and
enabled only around one write, with descriptor/path tracking so unrelated calls
pass through. Real files and syscalls are used for unaffected operations; no disk
is filled, mount altered or physical device disconnected. Fixed event storage
records operation ordering without allocating in the wrappers.

| Scenario | Required observation |
| --- | --- |
| Short writes with interrupts before and between progress | Exact complete payload, file sync/close before rename, directory sync/close after rename, ordered observers and private file permissions |
| Empty replacement | No write syscall; file sync and successful replacement with zero bytes |
| Creation denied/read-only; partial write out of space/quota; file sync/close failure; rename failure | Original or absent destination preserved, descriptors closed, temporary removed when cleanup succeeds, subsequent save succeeds |
| Zero progress with zero/stale errno | Exactly one failed write attempt, `EIO`, old destination retained and temporary removed |
| Directory open/sync/close failure, including interrupted sync/close | Error reported, complete new destination remains visible, descriptors released and subsequent save succeeds |
| Directory sync plus close/unlink failures | Original sync error survives secondary errors; complete new file remains |
| Partial write plus cleanup close/unlink failures | Original write error survives; old destination remains, partial orphan remains, later save succeeds without deleting the orphan |
| Failed file/directory close with immediate descriptor reuse | Replacement descriptor remains open; no second close, correct pre/post-rename destination state |

Pre/post-rename fault cases cover both replacing an existing file and creating a
new one. Close-error injection actually releases the descriptor before reporting
failure. Reuse tests immediately open `/dev/null` into that slot to detect an
unsafe second close. Fixtures release retained descriptors/files after assertions.

The existing `persistence` group separately covers child-process interruption after
file sync and after rename, malformed/versioned records and save/menu semantics.
Those process exits and the new syscall failures complement each other; neither
simulates storage losing power or proves all filesystem durability guarantees.

## Run and inspect

```sh
cmake --preset debug
cmake --build --preset debug --target atomic_write_tests persistence_tests content_tests -j2
ctest --preset debug -R '^(atomic_write|persistence|content)$' --output-on-failure

cmake --preset sanitize
cmake --build --preset sanitize --target atomic_write_tests persistence_tests content_tests -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --preset sanitize -R '^(atomic_write|persistence|content)$' --output-on-failure
```

The new group contains eight named cases and needs no display or audio device.
Full native runtime inventory is now 58 groups; CPU-only inventory is 32. Release
packaging adds three separate checks. See the [retained regression results](../validation/2026-10-24-atomic-writes/RESULTS.md)
for the two reproduced failures, fixes, commands and validation scope.
