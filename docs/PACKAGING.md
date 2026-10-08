# Relocatable Linux release package

The `release_package` target stages a fresh install and creates a versioned `.tar.gz`
archive plus a SHA-256 sidecar. It contains Feature Lab, the CPU stress runner,
assets, documentation, build identity and a host dependency inventory. No engine
libraries, system libraries or driver binaries need to be copied beside the
executables. Engine code is linked into them; approved system libraries remain
dynamically linked on the host.

## Build and validate

Use a native, single-configuration Release build with platform targets enabled and
sanitizers disabled. Packaging uses CMake 3.21+ and `objdump` from the development
toolchain; it downloads nothing and needs no additional runtime library. Ordinary
game/CPU execution needs neither CMake nor objdump.

```sh
export PATH="$PWD/.tools/bin:$PATH" # Only if using the local development tools.
cmake --preset release
cmake --build --preset release -j2
ctest --preset release --output-on-failure
cmake --build --preset release --target release_package
```

Archives are written under `build/release/dist/`, named
`feature-lab-0.1.0-Linux-<architecture>-<revision>.tar.gz`. The configure-time source
revision gains `-dirty` for tracked modifications; reconfigure after committing to
produce an archive named for that commit. Without Git the revision is `unknown`.
The build flags/compiler/configuration and build host's `/etc/os-release` are
included in `BUILD_INFO.txt`. This is a provenance record, not a reproducible-build
claim: archive timestamps and host toolchains can change compressed bytes.

Packaging first installs into a fresh staging directory. It enumerates direct ELF
NEEDED entries, required GLIBC/GLIBCXX/CXXABI symbol versions and recursively
resolved host libraries using objdump/CMake. Unresolved/conflicting dependencies or
RPATH/RUNPATH entries fail packaging. Dynamically selected GL/ALSA drivers and
plugins are explicitly outside this static inventory. Nothing from that inventory
is bundled, and their absolute build-host paths are not runtime search paths.

The packager hashes every payload file, creates the archive, extracts it under
`/tmp/feature-lab-package-tests`, renames its directory to a path containing spaces,
and runs mandatory CPU smoke checks. Only after those pass does it replace the
archive in `dist/` and write its checksum sidecar. Each packaging invocation uses
fresh scratch directories; failure preserves its diagnostics/work tree and leaves
the previously published archive intact until the publication step.

A separate installed layout is still available:

```sh
cmake --install build/release --prefix "$PWD/build/package"
(cd /tmp && /absolute/path/to/build/package/bin/feature_lab --verify)
```

Normal installs include both executables, assets and linked documentation.
Top-level README/build/dependency/hash records are added by the archive target.
CPU-only builds continue to install `stress_bench` and documentation; they do not
produce the full game archive. Debug, sanitizer and cross builds cannot use the
full release archive target.

## Test gates

`package_manifest` runs in all test configurations. It checks corruption, missing
files, duplicate/malformed/path-traversing records, symlink payloads, unlisted files
and mismatched compressed-archive checksums. The Release platform build adds three
CTest tests; fixture dependencies ensure the archive is generated first:

```sh
ctest --preset release -L packaging --output-on-failure
```

- `package_build`: install/archive/hash, dependency inventory and mandatory
  extracted/relocated CPU smoke checks before publication.
- `package_relocation`: verify compressed and payload checksums, move an extracted
  directory, execute gameplay verification through both the binary and a symlink,
  compare both CPU benchmark entry points, and reject changed/unlisted payloads.
- `package_graphics`: from a relocated copy, complete a 1,600-frame OpenGL game,
  save a checkpoint and replay, verify that replay with DISPLAY unset, render two
  stress cycles with stable resources, and capture the final game/arena images.

The CPU smoke runs unset DISPLAY, LD_LIBRARY_PATH and LD_PRELOAD. Their working
directory contains deliberately invalid `assets/` data. Removing the extracted
package's real assets must fail, proving that current-directory/source assets
cannot mask an incomplete package; an explicit `--assets` override is tested too.
The original source tree and built assets are never renamed or altered.

Graphics tests retain the display session, disable audio device startup and use
an isolated writable user-data directory outside the package. They run serially
and skip explicitly when DISPLAY is absent. A skip does not prove OpenGL packaging
works. Missing libraries, insufficient GL support or other failures with a display
are failures. Mandatory headless package construction never requires a GPU context.
Tests verify the complete payload manifest again after execution, proving that the
smoke scenarios did not modify the package. They do not make the payload read-only
or exercise audible physical output.

`ENGINE_PACKAGE_TEST_ROOT` can select another absolute scratch path at configure
time; keep it outside the source/build trees to retain the relocation test's
intent. Successful packager-internal scratch directories are removed. Standalone
smoke tests print and retain their evidence directories, including screenshots
and isolated user data, under that scratch root. Manifest-unit scratch directories
are removed on success.

## Run the archive

Check the external checksum in the directory containing the archive. Extract it
and keep the entire directory together. Replace the placeholder filename below
with the generated name:

```sh
sha256sum -c feature-lab-0.1.0-Linux-ARCH-REV.tar.gz.sha256
tar -xzf feature-lab-0.1.0-Linux-ARCH-REV.tar.gz
cd feature-lab-0.1.0-Linux-ARCH-REV
sha256sum -c FILES.sha256
./bin/feature_lab --no-audio
```

The archive's README explains controls, saves, stress/replay commands and host
requirements. Assets resolve through `/proc/self/exe`, independently of the working
directory or launch symlink. User configuration/saves use XDG directories or the
explicit `--user-data` override, not the installed asset directory.

Relocation is tested on the build PC's Linux ABI and graphics stack. Architecture,
C/C++ symbol versions, X11/GLX/ALSA libraries and OpenGL 4.6 Core support still need
to be compatible on another machine. XWayland validation does not establish native
Wayland support. This package does not bundle a portable runtime, perform system
installation, implement automatic updates or certify older Linux distributions.
Broader hardware/manual gates and remaining engine features stay tracked in
[STATUS.md](STATUS.md).
