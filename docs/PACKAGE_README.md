# Feature Lab — Linux 2D engine example

This archive contains a C++20/OpenGL 4.6 example game and an offline CPU benchmark.
Move or rename the complete extracted directory freely. Keep `bin/` and `share/`
together; copying only the executable loses the game assets. No installation or
administrator access is needed. Run from the extracted directory:

```sh
sha256sum -c FILES.sha256
./bin/feature_lab
```

The executable also works when invoked by absolute path or through a symlink from
another working directory. Assets are found relative to the real executable,
using Linux `/proc/self/exe`. `--assets DIRECTORY` explicitly selects another root.

## Host requirements

- Linux on the architecture recorded in `BUILD_INFO.txt`.
- Compatible system C/C++ runtimes, Xlib, GLX/OpenGL and ALSA libraries. Inspect
  `RUNTIME_DEPENDENCIES.txt` for the build host's direct/transitive dependency
  inventory and required symbol versions. These libraries are not bundled.
- An X11 or XWayland session with `DISPLAY` set and an OpenGL 4.6 Core driver for
  visible play. Native Wayland is not implemented. Driver internals and dynamically
  loaded GL/ALSA plugins remain host-managed.
- A working ALSA output device for audible play. `--no-audio` disables device
  startup. The CPU-only benchmark needs neither a display nor an audio device.

This package is relocatable on compatible hosts. It is not a self-contained Linux
runtime or a compatibility promise for older distributions. No system libraries,
driver binaries or third-party engine libraries are shipped. The tested machine
and outstanding hardware/manual checks are recorded under `share/doc/feature_lab/`.

## Play and verify

The title screen offers New Game, Continue, Resume Session and Quit. Collect all
three cores and reach the exit. Default controls: WASD/arrows move, +/- zoom,
Space pauses, R restarts, E interacts with the alarm switch, F1 opens settings,
F2 shows diagnostics, F3 shows collision shapes, F10 steps while paused, F11
requests fullscreen and Escape closes settings or quits. F5/F7 reload shaders/
textures. F6/F8 demonstrate failed reload recovery. Letter bindings can be changed
in settings. `--help` lists command-line options.

```sh
./bin/feature_lab --no-audio
./bin/feature_lab --verify
./bin/stress_bench --frames 60 --warmup 10 --cycles 2 --report /tmp/stress.json
./bin/feature_lab --stress --help
```

`--verify` runs the complete scripted gameplay simulation without creating a
window. The benchmark generates its own content and needs no asset directory.
Stress reports are written to the requested existing parent directory. Stress
mode uses offline mixing and changes no user settings.

Settings and saves normally use the user's XDG configuration/state directories.
To isolate an experiment, supply a writable path outside the package:

```sh
./bin/feature_lab --user-data /tmp/feature-lab-user --play
./bin/feature_lab --scripted --frames 1600 --no-audio \
  --user-data /tmp/feature-lab-user --record-replay /tmp/example.erpl
./bin/feature_lab --verify-replay /tmp/example.erpl
```

Existing ESAV v2 saves migrate to v3 random state; v1 saves remain unsupported.
Replays require matching content and simulation rules. This is an early engine
with an explicit coverage manifest, not completion of every planned system.

## Package contents and diagnostics

- `bin/feature_lab`, `bin/stress_bench`: dynamically linked Linux executables.
- `share/feature_lab/`: scene, tilemap, animation, textures, WAV audio and shaders.
- `share/doc/feature_lab/`: development README, engine plan, formats, coverage,
  packaging instructions and benchmark contracts/reference measurements.
- `BUILD_INFO.txt`, `RUNTIME_DEPENDENCIES.txt`: build identity and host requirements.
- `FILES.sha256`: hashes for every other packaged file. The archive's sibling
  `.tar.gz.sha256` checks the compressed download. Checksums detect corruption;
  they are not a signature or an update mechanism.

If startup fails, retain stderr and the two build/dependency files. Check `DISPLAY`,
driver support and host library compatibility. An asset-root error means the
package's directory layout is missing or an explicit override is wrong. Restore
changed files from the archive before using its manifest to check integrity.
