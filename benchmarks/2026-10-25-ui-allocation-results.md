# UI allocation coverage — 2026-10-25 UTC

Increment 59 measures settings, controls, diagnostics and the enabled-audio HUD
without optimizing those paths. Three Release reports from runtime **`4e860f5`**
match in every field across all 18 scenarios. Each scenario renders 300 frozen
frames to a 1280×800 target, excluding 30 warmup frames. Counts below describe
one 270-frame measured scenario, not a full interactive gameplay session.

Toolchain: GNU 16.2.1, Release, `-O3 -DNDEBUG`, sanitizers off. The retained reports
identify the GL device and host environment. Soundtrack fixtures use ALSA `null`;
all counters here are from the rendering thread. The separate audio-worker workload
and earlier replay measurements retain their original contracts.

See the [workload/scope contract](UI_ALLOCATIONS.md),
[validation](../validation/2026-10-25-ui-allocations/RESULTS.md), and raw reports
[1](../validation/2026-10-25-ui-allocations/ui-1.json),
[2](../validation/2026-10-25-ui-allocations/ui-2.json),
[3](../validation/2026-10-25-ui-allocations/ui-3.json).

## Steady redraws

Facility and Annex produce the same allocation counts in these fixtures:

| Scenario | Calls per frame | Requested bytes per frame | Calls / measured run | Requested bytes / measured run |
| --- | ---: | ---: | ---: | ---: |
| Audio off | 0 | 0 | 0 | 0 |
| Default enabled-audio HUD | 0 | 0 | 0 | 0 |
| Remapped enabled-audio HUD | 0 | 0 | 0 | 0 |
| Open settings | 0 | 0 | 0 | 0 |
| Remapped controls page | 0 | 0 | 0 | 0 |
| Binding capture | 0 | 0 | 0 | 0 |
| Binding accepted | 0 | 0 | 0 | 0 |
| Diagnostics | 16 | 537 | 4,320 | 144,990 |
| Diagnostics plus debug shapes | 16 | 537 | 4,320 | 144,990 |

All measured allocations occur in `diagnostics_submit`. Idle settings update,
minimap, world drawing, the ordinary HUD, settings drawing and renderer end record
zero calls/bytes in these states. The inclusive frame exactly equals its subscopes
and must not be added to them. Debug shapes add geometry without changing this
fixture's allocation count. The existing diagnostics implementation constructs
owning strings repeatedly; counters do not retain per-allocation call stacks.

These zero results include current international/mouse chord formatting on this
standard-library implementation. They are not a guarantee for all labels, content,
standard libraries, live input or changing states. Diagnostics uses fixed CPU
history and its default unsupported-GPU-timing branch, with no query history.
Completed/waiting GPU labels and changing numeric widths need additional coverage.

## Menu and binding transitions

These samples occur once during fixture setup, before warmup. They remain visible
in a separate `transitions` object:

| Transition | Calls | Requested bytes |
| --- | ---: | ---: |
| Open settings | 5 | 983 |
| Enter remapped controls page | 16 | 1,884 |
| Start capture | 1 | 33 |
| Accept replacement binding | 16 | 1,880 |

Both levels and all repeats agree. Entering controls and accepting a binding rebuild
retained widget labels/layout; capture allocates its notice text. These totals
include the real input-handling calls, not an estimate of individual allocations.
They do not include startup asset decoding, graphics/audio creation, initial
simulation, serialization or teardown.

The accepted binding label is `MOVE LEFT : C+S+OUML`. The remapped audio label is
`C+S+A+NTILDE MUTED / C+S+U+03BB MUSIC 15 / C+S+A+U+1F642 EFFECTS 25`.
The CTest checker validates these strings and the actual menu/capture flags.

## Rendering equivalence and limits

The normal game and new probe share `draw_game`. Its body matches the previous
implementation after whitespace removal, with identical color constants moved
inside the function. Eight ordinary-game before/after PPM pairs are byte-identical:
Facility and Annex during replay frames 90, 121 and 3000, plus each level's open
settings demo. [Provenance](../validation/2026-10-25-ui-allocations/draw-provenance.json)
and [comparison commands/hashes](../validation/2026-10-25-ui-allocations/production-screens.json)
retain the source/binary comparison. Eighteen new fixed-state captures document the
probe's rendered output; raw PPMs remain under ignored `build/ui-validation/`.
[Comparison record](../validation/2026-10-25-ui-allocations/comparison.json) retains
their sizes/hashes and the repeated allocation results.

No speedup or process-memory reduction is claimed. Requested bytes are cumulative
successful C++ allocation requests, not peak/live heap or RSS. Window events, swap,
real-time simulation/audio updates, C allocation, driver/GPU storage and physical
audio/display acceptance are outside this probe. Earlier production replay tests
still exercise the main loop separately.

Next: reuse diagnostics text storage, compare these same scopes, and cover changing
GPU-timing labels and combined overlays. Keep one-off layout/rebinding costs and
all other exclusions explicit before choosing broader workload/memory budgets.
