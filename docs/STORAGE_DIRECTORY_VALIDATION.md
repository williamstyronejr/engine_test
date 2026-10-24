# Preparing save-directory chains

`UserStorage::write_config` and `write_slot` now prepare their directory chain
before handing the record to `AssetRoot::write_atomic`. Record formats and public
API signatures are unchanged. Reads and rejected envelopes/slot numbers create
no directories and perform no directory synchronization.
Reads resolve directory paths with the same normalization as writes, so an absolute
path containing a missing component followed by `..` still reads the saved record.

## Preparation contract

1. Validate the complete record envelope before filesystem mutation.
2. Resolve existing symlink components with `weakly_canonical`. Build the resolved
   path's ancestor list, including the filesystem namespace root.
3. Create directories from the root toward the leaf with mode `0777`, subject to
   the existing umask/default ACL rules. Existing directories keep their modes.
   `EEXIST` is accepted provisionally; later `O_DIRECTORY` opens verify directory
   type. An unrelated file at a directory path is never replaced.
4. Starting at the leaf and walking to `/`, open each directory with
   `O_RDONLY | O_DIRECTORY | O_CLOEXEC`, `fsync` it, then close it once. All these
   operations must succeed before creating a record temporary.
5. Use the resolved leaf for the existing atomic file transaction: write/sync/close
   the sibling temporary, rename it, then sync/close the leaf again to publish the
   file entry. See [atomic-save error semantics](ATOMIC_SAVE_VALIDATION.md).

This ordering synchronizes child directory metadata and then the containing
entries, before publishing a record. A file sync alone does not establish that
its directory entry reached storage; directory synchronization is required too.
See the Linux [fsync(2) contract](https://man7.org/linux/man-pages/man2/fsync.2.html#DESCRIPTION).
Directory creation retains Linux [mkdir(2) mode/umask semantics](https://man7.org/linux/man-pages/man2/mkdir.2.html#DESCRIPTION).

Every write rechecks/synchronizes the full resolved chain, including already
existing ancestors. A previous attempt or process may have created directories
and failed or exited before syncing them. There is no in-memory “already durable”
cache or existence-based shortcut. Reconstructing `UserStorage` or restarting the
process does not bypass preparation.

## Failure and compatibility boundary

A creation/open/sync/close failure stops before the record temporary is created:
an old record remains unchanged, and an absent record remains absent. Errors name
the operation and affected path. A sync error takes precedence over a later close
error. A failed close, including `EINTR` on Linux, is not retried on a descriptor
that could have been reused. Other interrupted operations are surfaced so the
caller can retry the entire write.

Directories created before a failure remain in place. They may be incomplete or
not fully synchronized; later attempts prepare the entire chain again. Removing
them automatically could race other users/writers. Successful preparation does not
change the separate post-rename file-failure rules: a later atomic-save error may
still occur after the complete new record is visible.

This is deliberately stricter than creating a writable leaf: **every resolved
ancestor must be openable for directory synchronization and support `fsync`**.
Denied access or unsupported directory sync fails the save; there is no fallback
that silently skips an ancestor. Repeated saves add synchronous filesystem work
and can stall the caller. No frame-time or save-latency budget is claimed here.

Storage is still trusted local user storage, not an adversarial path sandbox.
Ordinary concurrent record writers remain last-rename-wins. A directory created
by another writer and reported as `EEXIST` must still pass directory verification
and synchronization. Concurrent ancestor rename/replacement, externally managed
symlink-entry durability and mount-topology changes are outside this contract.
Existing symlinks are resolved to synchronize the actual target ancestry, not the
textual alias ancestry. `AssetRoot::write_atomic` alone still requires its parent
to exist; it does not prepare or synchronize the entire ancestor chain.

Successful syscalls provide OS-reported synchronization, **not physical power-loss
certification**. Kernel/filesystem/device write-back behavior and volatile storage
still matter. The tests use temporary storage; an acknowledged sync on tmpfs does
not make it survive reboot. No device cache, mount or desktop setting is changed.

## Verification

The CPU-only `storage_directories` group links a dedicated executable with scoped
`mkdir`, `open`, `fsync`, `close` and `mkstemp` wrappers. Production executables have
no interception flags. Real file operations run in private directories; injected
failures target one preparation operation/path. Trace state checks every expected
sync/close finishes before the first record temporary, and verifies descriptor
release on error. The ten named cases cover:

- First and repeated configuration/slot saves, including reconstructed storage objects.
- Creation errors leaving a partial tree, followed by successful full-chain retry.
- Open, sync and close errors at the leaf, a private ancestor and `/`, for new and
  existing records; interrupted/unsupported sync and interrupted close are included.
- Primary sync errors surviving secondary close errors, with immediate descriptor
  reuse proving that the new descriptor is not closed again.
- Existing symlink components resolving to the real storage ancestry.
- Reads and writes agreeing after normalization of a missing component followed by `..`.
- `EEXIST` races producing either a directory or an obstructing regular file.
- Read-only queries and invalid envelopes/slots producing no creation/sync side effects.
- A preexisting unrelated file remaining intact when it blocks a directory path.
- A child process exiting during ancestor sync, with a later process retry syncing
  the now-existing chain before publishing its record.

The last case is process interruption, not simulated loss of kernel buffers or
power. Tests do not fill a disk or change system permissions/mounts.

```sh
cmake --preset debug
cmake --build --preset debug --target storage_directory_tests atomic_write_tests persistence_tests content_tests -j2
ctest --preset debug -R '^(storage_directories|atomic_write|persistence|content)$' --output-on-failure

cmake --preset sanitize
cmake --build --preset sanitize --target storage_directory_tests atomic_write_tests persistence_tests content_tests -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --preset sanitize -R '^(storage_directories|atomic_write|persistence|content)$' --output-on-failure
```

See [retained results](../validation/2026-10-24-storage-directories/RESULTS.md) for
the old-implementation comparison, commands and validation scope. Native runtime
inventory is now 58 groups; the CPU-only inventory is 32. Release adds three
packaging checks.
