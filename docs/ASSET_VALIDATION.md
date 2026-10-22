# Offline Feature Lab asset validation

`validate_assets` checks Feature Lab content without a window, OpenGL context or
audio device. It links only the CPU engine and system C/C++ runtimes. It reads
assets without rewriting files, saving settings or substituting fallback content.

## Build and run

```sh
cmake --preset debug
cmake --build --preset debug --target validate_assets -j2
./build/debug/validate_assets --assets assets
```

It is also built with `ENGINE_BUILD_PLATFORM=OFF`. In a CPU-only build, pass an
explicit asset root: asset staging is part of the native example build.
The release archive includes the command:

```sh
./bin/validate_assets
./bin/validate_assets --assets /path/to/edited/assets
./bin/validate_assets --help
```

Without `--assets`, discovery checks `assets/` beside the real executable and then
`../share/feature_lab`, using `/proc/self/exe`. The working directory is never an
implicit asset root. Paths with spaces work. An explicit root must exist and be a
directory. Existing `AssetRoot` path validation and symlink confinement apply to
all reads. There is no network access or dependency download.

The command prints each check in stable order, with `PASS` or `FAIL`, its asset or
level name, and the decoder/semantic diagnostic on failure. Scene parsing retains
source and line numbers. Independent checks continue after failures, so one bad
asset may produce both a file error and dependent level errors. The final summary
counts checks, not unique files. Stock assets produce **20 checks, 0 failed**.

| Exit status | Meaning |
| --- | --- |
| 0 | All performed content checks passed, or help was requested |
| 1 | Content validation or asset-root discovery failed |
| 2 | Invalid command-line arguments |

Output is a human-readable report; its text is not a versioned machine-readable
schema. Redirect stdout/stderr to retain it. Root discovery errors go to stderr;
per-check errors and the summary go to stdout.

## Checked content

The command checks the fixed Feature Lab inventory, plus textures referenced by
successfully constructed levels. It does not recursively scan arbitrary extra
files or validate unrelated projects.

- `facility.scene` and `annex.scene`: bounded scene parsing with Feature Lab's
  256-entry budget, hierarchy, component and transform checks.
- `facility.etmp` and `annex.etmp`: bounded ETMP decoding and tilemap validation.
- `facility.eani`: bounded EANI decoding; both games additionally require `idle`,
  `walk`, `core_pulse`, `machine` and `door_open` with the expected loop modes.
- `fonts/ui.efnt`: strict EFNT decoding, including its embedded atlas and metrics.
- `sounds/pickup.wav`, `music.wav`, `machine.wav` and `door.wav`: the existing bounded
  PCM WAV decoder/resampler. No sound device is opened.
- `textures/actors.etex`, `tiles.etex`, `white.etex`, `checker.etex`, plus other
  referenced texture keys: strict ETEX decoding and dimensions compatible with
  every atlas grid used by either successfully constructed level. Missing or
  corrupt textures fail rather than using the game's runtime fallback artwork.
- `shaders/sprite.vert` and `sprite.frag`: readable source within the runtime's
  64 KiB limit, nonempty and without NUL bytes. These checks do **not** compile GLSL.
- Both level constructors: required player/exit/core roles, decorations, static
  collider restrictions, spawn placement, scene/component budgets and initial
  collision capacity checks. Animation reload validation also checks `walk`, which
  is not used at tick zero.
- Both transition directions: the starting level's resident texture keys and
  combined atlas grids must accommodate the destination. These checks run only
  when both level constructions succeed. Loading a new texture at startup does
  not guarantee it is resident when entering that level from another one.

Readers retain their existing file-size and format bounds; textures are decoded
one at a time. This is not a total process-memory budget. See [formats](FORMATS.md)
and [runtime scene limits](FORMATS.md#runtime-entity-contracts).

## Validation boundary

A passing report verifies content structure and the listed initial gameplay
conditions. It does not prove route reachability, future spawn/update capacity,
OpenGL shader compilation/interfaces, rendered pixels, GPU limits, audible output,
or performance. Run gameplay verification and native tests for those behaviors.
Files should be stable during validation: independent reads are not an atomic
snapshot of a directory being edited.

Tests use private copies to check independent corruption, repair/retry, 256/257
entity boundaries, missing gameplay roles, invalid spawn positions, animation loop
modes, atlas dimensions, missing texture references, cross-level residency, root
confinement and oversized shaders. CLI tests unset display variables, check exit
statuses and default/explicit discovery, and verify asset hashes remain unchanged.
Package checks run the installed validator after extraction and relocation, from
a misleading working directory, and verify missing-root rejection and explicit
override before publishing the archive.
