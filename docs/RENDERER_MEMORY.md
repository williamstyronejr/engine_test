# Renderer owned-memory snapshots

`Renderer::memory()` returns a `RenderMemory` snapshot on the owning thread,
including during an active pass. It does not allocate, flush, query OpenGL, reset
counters or change drawing. The snapshot covers explicitly tracked payloads, not
the renderer's total heap, resident memory or physical GPU memory.

| Field | Meaning |
| --- | --- |
| `cpu_vertex_reserved_bytes` | Actual vector capacity × vertex size; includes unused reserved slots |
| `cpu_vertex_used_bytes` | Current pending vertices × vertex size; a subset of the reservation |
| `gpu_vertex_store_bytes` | Logical size requested for the one live VBO store |
| `gpu_builtin_texture_bytes` | Built-in white/checker atlas RGBA8 texels |
| `gpu_uploaded_texture_bytes` | Current user-uploaded texture texels; same scope as `texture_bytes()` |
| `gpu_font_texture_bytes` | Current font atlas texels; same scope as `font_bytes()` |
| `gpu_target_texture_bytes` | Current render-target color texels; same scope as `target_bytes()` |

On the tested ABI, a vertex is 32 bytes. The 4,096-quad batch reserves 24,576
vertices (786,432 CPU payload bytes); the VBO requests 786,432 bytes. The vector
reports its actual capacity rather than assuming `reserve()` chooses an exact
capacity. Both GL buffer allocation sites and reporting share one size constant.
The built-in atlas is 4×2×4 = 32 bytes, with a compile-time payload-size check.

At initialization, pending CPU bytes are zero. Quads add six vertices; a batch
flush clears pending bytes but retains capacity. Empty passes and `end()` also
leave no pending vertices. State changes can flush before the capacity limit.
Invalid or culled submissions do not add vertices. Repeated VBO orphaning does
not accumulate the reported logical store size. Texture/target replacement changes
only committed payload sizes; rejected replacements preserve the old snapshot.
Releasing uploaded textures or targets removes their payloads, while vertex
storage and built-in/font atlases remain until renderer destruction.

GPU sizes describe logical payload requests for a healthy renderer. They do not
measure driver allocation success independently, physical residency, alignment,
compression, retired stores, upload copies or allocation metadata. In particular,
the driver may retain previous orphaned VBO stores while draws are in flight.
Use the existing renderer health/error checks; a snapshot is not a health test.

CPU exclusions include `Impl`/object storage, allocator metadata, the copied font
atlas/glyph vectors, strings, shader compilation and transient staging. GPU
exclusions include program, VAO, FBO and query objects, driver bookkeeping,
default-window attachments and delayed retirement. `GpuTimer` owns its queries
separately. Do not add CPU used bytes to reserved bytes or multiply the logical
VBO size by draw count to claim live memory. No new global memory budget is set.

## Stress report integration

`memory.renderer_baseline` is an additive object in `feature_lab_stress_v1`:

- `coverage`: `owned_buffers_and_texels_v1`.
- `observed`: true for graphics, false for CPU-only runs.
- The seven fields above, in bytes. CPU-only values are zero placeholders with
  `observed:false`, not measurements of an instantiated renderer.

Graphics samples this baseline immediately after renderer creation and before
workload textures/targets exist. Thus uploaded/target payloads and pending CPU
vertices are zero; the vertex reservation, logical VBO and built-in/font atlases
are present. The existing `peak_uploaded_texture_bytes` and `peak_target_bytes`
remain separate workload observations. The baseline is not a peak or a complete
combined budget. Existing `tracked_cpu_buffer_bytes` keeps its workload-only scope.

Resource-stability checks now compare these snapshots at completed-frame and
load/unload boundaries as well as the existing handle/texel counts. They do not
sample transient peaks within draws or loading. New scalar snapshot storage is
part of the fixed report object, not included in the vector-payload inventory.
The JSON change affects optional benchmark evidence, not engine assets or saves.

## Validation scope

The new native `renderer_memory` group compares the reported VBO size with
`GL_BUFFER_SIZE`, checks getter non-interference, replacement/resize rollback,
release-to-baseline behavior and VBO deletion across renderer lifetimes. Existing
capacity-edge tests inspect pending bytes around rollover, culling/rejection,
end and empty reset. Font tests compare memory snapshots across atlas replacements.
CPU/native report tests distinguish absent renderer observations from a graphics
baseline and require consistent fields across all trace/presentation modes.

Run ten focused groups on GNU Debug/Release and Clang Debug, two CPU groups under
ASan/UBSan with leak checks enabled, and eight native groups with driver leak
checks disabled. Compare nine stress-mode images in all four native configurations
with the pre-change Release binary. Keep failures/skips and exact source/build
identities. This is correctness/coverage validation, not a performance adoption
trial; earlier timing failures and rejected optimizations remain unchanged.
