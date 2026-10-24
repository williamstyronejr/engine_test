# Save-directory preparation validation — 2026-10-24 UTC

Implementation: **`51c25298dee5a895d3baa725059cdd47bbd64439`**, based on `ca433c0`.
`UserStorage` now creates the resolved storage chain and synchronizes every
ancestor before creating a record temporary. Every attempt rechecks existing
ancestors, including after a failed/interrupted attempt or reconstructed storage
object. See the [contract and limits](../../docs/STORAGE_DIRECTORY_VALIDATION.md).

## Baseline comparison

The previous implementation only called `create_directories` before the atomic
file write. The initial eight-case draft produced seven failures against it;
[that output](initial-before-tests.log) is retained. The final nine-case suite was
also compiled against exported `persistence.cpp` and `assets.cpp` from `ca433c0`,
using unchanged public headers. It produced **eight failures and one pass**, as
expected: [final comparison](baseline-final-tests.log),
[exact compile/run commands, revisions and source hashes](baseline.json).

These are failures to meet the new preparation, interception and diagnostic
contract, not eight independent claims of observed disk corruption. The normal
write trace demonstrates the central gap: the old implementation reaches record
creation without the required ancestor synchronization. The new code passed the
[focused Debug check](focused-debug-tests.log) before being committed.

## Verification at the committed implementation

| Configuration | Passed | Evidence |
| --- | --- | --- |
| GCC 16.2.1 Debug, full runtime | 58/58 groups | [log](gcc-debug-tests.log), [XML](gcc-debug-tests.xml) |
| GCC 16.2.1 Release, full runtime | 58/58 groups | [log](gcc-release-tests.log), [XML](gcc-release-tests.xml) |
| Clang 23.1.3 Debug, storage/atomic-write/persistence/content | 4/4 groups | [log](clang-debug-tests.log), [XML](clang-debug-tests.xml) |
| GCC CPU ASan/UBSan, same four groups, leak detection enabled | 4/4 groups | [log](cpu-sanitize-tests.log), [XML](cpu-sanitize-tests.xml) |

All post-change runs passed with no skips, sanitizer findings, timeout increases
or suppressions. Existing build directories were reconfigured/rebuilt; these are
incremental builds, not fresh/offline-build claims. [Commands](commands.json) retain
the explicit compiler, configuration and platform/sanitizer options plus UTC start,
duration and exit status. [Environment](environment.log) records the host/tools.

The new group contains **nine named cases** with loops over error points/codes,
new versus existing records, and configuration versus slot storage. The full native
inventory contains 34 CPU, 21 hardware and three platform groups. CPU-only builds
discover 32 groups; only the four relevant groups were run under sanitizers here.
Packaging adds three separate checks, recorded in
[implementation status](../../docs/STATUS.md#increment-56-save-directory-chain-preparation).

Tests verify complete record bytes, absence of record temporaries on preparation
failure, preserved old/absent records, released descriptors, retry after partial
creation/process exit, real descriptor reuse, resolved symlink ancestry and
nondirectory obstructions. Reads and rejected inputs have no preparation effects.
The executable uses linker wrappers; production code has no test interception.

## Remaining limits

The tests exercise actual temporary files and real directory sync calls, with
selected deterministic errors. They establish the implemented ordering and error
contract. They do not emulate kernel/device crashes or certify physical power-loss
durability; volatile storage remains volatile. All resolved ancestors must permit
directory open/sync operations, and the added synchronous I/O has no measured
save-latency guarantee. External symlink durability, concurrent ancestor replacement
and mount changes remain outside the contract.

Earlier full native sanitizer, TSan and offline-build records retain their own
source revision and scope. This follow-up does not relabel them as results for
the new implementation. Performance/memory acceptance and physical display/audio
checks remain outstanding.
