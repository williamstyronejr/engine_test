# Offline asset validation

`validate_assets` checks individual engine assets or a complete Feature Lab bundle
without a window, OpenGL context or audio device. It links only the CPU engine and
system C/C++ runtimes. It reads assets without rewriting files, saving settings or
substituting fallback content.

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

Text remains the default; its wording is not a versioned machine-readable schema.
Redirect stdout/stderr to retain it. In text mode root discovery errors go to stderr;
per-check errors and the summary go to stdout. Use `--json` for structured output.

## JSON reports

Add `--json` once, in any option position, to emit one JSON object and a newline
on stdout. There are no text banners or summaries on stdout in this mode. The tool
opens no report file itself; shell redirection can retain the result:

```sh
./build/release/validate_assets --assets assets --json > build/assets-report.json
./build/release/validate_assets --json --assets assets --type scene --file facility.scene --scene-budget 256 > build/scene-report.json
```

The report schema is **`engine_asset_validation_v1`**. It describes validation
results; engine assets and saves retain their existing formats. All fields below
are present. JSON booleans and numbers are actual booleans and numbers.

| Field | Type and meaning |
| --- | --- |
| `schema` | String, exactly `engine_asset_validation_v1` |
| `string_encoding` | String, exactly `byte_values`; string-byte convention below |
| `mode` | `bundle` or `file` |
| `format` | Selected type string in file mode; null in bundle mode |
| `file` | Requested root-relative key in file mode; null in bundle mode |
| `scene_budget` | 256 in bundle mode; selected/default limit for a scene file; null for other formats |
| `requested_root` | Explicit `--assets` value, or null for automatic discovery |
| `asset_root` | Canonical discovered root, or null if discovery failed |
| `complete` | Whether the validator returned its check results |
| `passed` | True only when validation completed and no performed checks failed |
| `check_count` | Number of entries in `checks` |
| `failed_checks` | Number of entries with `passed: false` |
| `checks` | Ordered array of `{name, passed, diagnostic}` records |
| `fatal_error` | Null normally; error string if validation could not complete |

Each check's `name` is its asset or level label, `passed` is a Boolean, and
`diagnostic` is null on success or a string on failure. Diagnostic wording is not a
stable error-code API. Source/line context is preserved when supplied by the reader.

Content failures produce a complete report with `passed: false` and exit 1. Failed
bundle prerequisites can prevent dependent checks from running: for example, the
two texture-transition checks are omitted when either level cannot be constructed.
Counts describe performed checks, not a fixed inventory or distinct file count.
`complete` does not mean every possible runtime check ran or that content passed.

Fatal errors, including missing roots, produce `complete: false`, `passed: false`,
zero counts, an empty `checks` array and a non-null `fatal_error`, with exit 1.
Requested mode/type/key/budget/root context remains available. Use `passed` and the
exit status, rather than interpreting zero failed checks as success.

Argument errors retain exit 2, stderr help/diagnostics and empty stdout, even when
`--json` is present. `--help` still works only on its own. Invalid arguments do not
produce a JSON report. A failed stdout write returns 1 with a stderr diagnostic;
process termination, a closed pipe or other output failures can leave no usable
report. JSON mode does not alter any validation rules or substitute fallback assets.

Output order is deterministic for unchanged inputs, options and environment; there
are no timestamps. Consumers should check `schema`, tolerate added fields, and avoid
depending on field order or exact diagnostic wording. Incompatible interpretation
changes require a new schema identifier.

Strings use the existing profiler's reversible byte convention: printable ASCII is
retained, quotes/backslashes are escaped, and every control or non-ASCII byte is
written as `\u00XX`. This guarantees ASCII JSON even for invalid UTF-8 Linux path
bytes. After JSON decoding, map U+0000..U+00FF back to byte values to recover original
strings. UTF-8 filenames require that byte reconstruction before UTF-8 decoding;
the JSON strings are not normalized Unicode filenames. `string_encoding` explicitly
identifies this convention. Numeric formatting is independent of the host locale.

Tests parse CLI output with an independent JSON reader, compare repeat runs, cover
all six formats and zero/default budgets, retain content/fatal failure context,
and exercise quotes, backslashes, newlines, non-ASCII/control bytes and numeric
locale changes. They also check invalid arguments and rejected stdout writes.
The relocated package emits and parses a JSON scene report as part of its CPU checks.

## Single-file format checks

Choose a format explicitly and supply a key relative to the selected asset root.
The directory can contain only the work-in-progress file:

```sh
./build/debug/validate_assets --assets /path/to/work --type scene --file draft.scene
./build/debug/validate_assets --assets /path/to/work --type scene --file draft.scene --scene-budget 256
./build/debug/validate_assets --assets /path/to/work --type texture --file image.etex
```

The same options work in the installed command. Option order is flexible;
`--type` and `--file` must appear together. Duplicate options, unknown types,
missing/empty values and incompatible options return 2 before asset discovery.
`--help` is accepted only on its own. Absolute file keys, `..` traversal, missing
files and symlinks escaping the asset root fail validation with exit 1.
Use `--assets` to select the containing directory, including one with spaces.

| `--type` | Selected decoder and validation |
| --- | --- |
| `scene` | Scene text, IDs, hierarchy, transforms, inline components and entity/slot budget |
| `tilemap` | ETMP structure, dimensions, layers, palette, atlas-cell indices and coordinate bounds |
| `animation` | EANI structure, clip names, durations, loops and atlas-cell indices |
| `texture` | ETEX dimensions and exact RGBA payload |
| `font` | EFNT metrics, glyphs and embedded atlas |
| `sound` | Supported PCM WAV structure, bounds and CPU resampling |

Extensions are not inspected; the selected decoder determines validity. Existing
reader size limits and `AssetRoot` path rules apply. There is no generic shader mode.
Exactly one file check is reported, with the selected format and scope displayed.
A success summary is `Asset validation: 1 check, 0 failed`.

Generic scenes default to the engine's **4,096-entry** budget. `--scene-budget N`
accepts a decimal integer from 0 through 4,096 only with `--type scene --file KEY`.
Zero accepts an empty scene. Overflow returns 1 with a source/line diagnostic;
malformed or out-of-range budget arguments return 2. Feature Lab bundle mode
continues to use its fixed 256-entry budget and rejects this override.

These checks do not read referenced textures, compare an atlas grid with an external
image, require Feature Lab roles/clip names, construct a game, or test transitions.
A scene without a player can pass format validation; a missing parent within that
scene cannot. Use bundle mode to check Feature Lab's dependencies and semantics.
The `validate_asset_file` helper follows the same rule: non-scene formats require
default scene limits.

## Feature Lab bundle checks

With neither `--type` nor `--file`, the command checks the fixed Feature Lab
inventory, plus textures referenced by successfully constructed levels. It does not recursively scan arbitrary extra
files. Generic files from other projects can use the single-file mode above.

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

A passing report verifies the selected format, or the listed bundle conditions
when using Feature Lab mode. It does not prove route reachability, future spawn/update capacity,
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

Single-file regressions decode every format in an otherwise empty root, reject
truncated data, repair and retry, and preserve original bytes. They distinguish
generic scenes from Feature Lab semantics, check custom/zero scene budgets,
source/line diagnostics, wrong decoders, missing files, confinement and size limits.
CLI tests cover all formats, renamed extensions, argument combinations and budget
syntax. Relocated package checks exercise a single scene with a 256-entry budget.
