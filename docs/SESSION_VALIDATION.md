# Bounded repeated-session regression

`long_session` exercises repeated gameplay, transactional reload failures and
resource retirement in one process. It is a correctness fixture for R6 coverage,
not a frame-time benchmark or a real-time endurance certification.

## Fixed workload

The test completes **64 routes**, alternating Facility and Annex, with a maximum
of **3,000 route updates per cycle**. It uses the normal `Game`, `Replay`,
`SceneFlow`, `SceneAssets`, `TextureAssets`, `Soundtrack` and save/load APIs.
Each cycle must collect all three cores, finish the door exactly once, win and
advance through the production level-transition path. The same renderer, resident
texture registry, scene coordinator and save directory survive the entire run.

At route step 120 in every cycle, it:

- Saves the live checkpoint, advances the live game, restores the save and checks
  exact checkpoint bytes. Injects a bad checksum through the private asset API,
  verifies load rejection leaves state unchanged, and repairs the saved bytes.
- Replaces the private scene file with invalid text, verifies failed reload retains
  the revision, entity and checkpoint, then restores the original file and reloads.
  Successful publication must preserve checkpoint bytes and invalidate the old entity.
- Rejects an invalid texture replacement batch, verifies its revision and renderer
  inventory stay unchanged, then reloads the original resident bank. Instrumented
  texture staging must return to zero after both outcomes.
- Rejects an invalid private WAV, repairs it, publishes the original audio bank,
  and waits for retired bank bytes to return to the live bank size.
- Pauses and resumes the game and audio groups; a paused update must not advance
  the simulation tick.

A real ALSA worker uses the **null** PCM. The soundtrack is restored after save/load,
its emitter follows each level, and it restarts loops at transitions. Every eight
cycles, the output is destroyed and recreated: eight workers are opened and joined
across the run. Every completed route must settle to one active music voice with
no retired banks, queue rejections, dropped plays, stale controls or device error.
Worker waits have a three-second deadline. A one-millisecond cooperative sleep
occurs every 16 route updates; this does not synchronize simulation to real time.

A hidden 64×64 GLX window owns the context. World sprites render to a 64×64 target
every 120 route updates and at cycle completion. Each pass must submit geometry
and leave a healthy renderer. Each cycle also creates, resizes and releases a
transient target. This exercises sprite rendering and target lifetimes; it does
not exercise the full HUD, tile layers, minimap or physical presentation.

## Resource checks and limits

After each rendered pass, all eleven `Renderer::memory()` fields must equal the
resident baseline, including zero pending vertex usage. Texture/target counts
stay fixed. After explicit release of every uploaded texture and target, counts
are zero and the renderer inventory matches its pre-load snapshot.

Collision buffer bytes remain fixed. Completed-world buffer bytes match the
first completed cycle of the same level. Audio live bank bytes stay fixed and
retired bank bytes drain before the next cycle. These checks cover the named
payloads; they do not measure RSS, total live heap, driver caches, staging peaks
for every operation or post-destruction process memory. See
[renderer memory scope](RENDERER_MEMORY.md) and
[workload acceptance](../benchmarks/WORKLOAD_ACCEPTANCE.md).

The fixture prints every cycle and a final record containing cycles, maximum
steps, committed gameplay ticks, render passes, output lifetimes, elapsed seconds
and resource restoration. Simulated duration is ticks divided by 60. A restored
speculative update is not counted in the committed tick total. Compiler/sanitizer
runs must agree on the workload counts; elapsed time is diagnostic only.

The initial validated workload commits **66,720 ticks** (1,112 simulated seconds,
18 minutes 32 seconds), renders **640 passes**, and completes all 64 cycles.
This accelerated workload does not prove stability during 18 minutes of wall time.
It uses private copied assets and private `UserStorage` beneath the test scratch
root. Original assets and the user's settings/saves are not edited.

## Run and interpret

With the project's installed dependencies and a working display:

```sh
cmake --preset debug
cmake --build --preset debug --target long_session_tests -j2
ctest --preset debug -R '^long_session$' --output-on-failure
```

The test is native-only, labeled `hardware`, runs serially and has a 180-second
CTest timeout. Without DISPLAY it returns skip code 77; a skip is not a pass.
Use the [disk-backed scratch recipe](RELEASE_READINESS.md#reproduce-the-automated-checks)
for compiler and private test files. No benchmark ceilings, timeout adjustments
or sanitizer suppressions are introduced by this fixture.

[Validation evidence](../validation/2026-10-30-long-session/RESULTS.md) records the
source commit, exact commands, fresh targeted builds, per-cycle output and the
initial save-corruption fixture error. Related unit groups continue to cover
individual behaviors; this fixture adds their repeated composition.

Physical input/display (R4), audible sound/device disconnect/suspend (R5),
wall-time endurance, settings/HUD/interactive coverage, host timing instability,
broader memory coverage (R6), and support/signoff (R8) remain open. It does not
exercise physical audio, driver leak detection, artificial device failure or
background loading. Passing this test does not complete milestone 6.
