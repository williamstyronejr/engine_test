#pragma once
#include "engine/assets.hpp"
#include "engine/font.hpp"
#include "engine/math.hpp"
#include "engine/sampling.hpp"
#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace engine {
struct Color {
    float r{1}, g{1}, b{1}, a{1};
}; // Linear RGB, straight alpha.
struct TextureHandle {
    std::uint32_t slot{};
    std::uint64_t serial{};
};
struct TextureReplacement {
    TextureHandle handle;
    const TextureData& data; // Borrowed for the duration of replace_textures().
};
struct RenderTargetHandle {
    std::uint32_t slot{};
    std::uint64_t serial{};
};
struct RenderTargetSize {
    int width{}, height{};
};
// Top-left origin, in pixels of the current render destination.
struct PixelRect {
    int x{}, y{}, width{}, height{};
};
struct ShaderReloadResult {
    bool applied{};
    std::uint64_t revision{}; // Unchanged on failure; starts at 1 for the built-in program.
    std::string diagnostics;  // Bounded compiler/linker warnings or failure details.
};
struct GraphicsInfo {
    std::string vendor, device, version;
};
struct RenderStats {
    std::size_t quads{}, culled{}, draws{};
};
// Owned payload snapshot, not total CPU/GPU memory or cumulative uploads.
// GPU store/texel sizes exclude driver overhead and retired orphaned stores.
struct RenderMemory {
    std::size_t cpu_vertex_reserved_bytes{}, cpu_vertex_used_bytes{};
    std::size_t gpu_vertex_store_bytes{};
    std::size_t gpu_builtin_texture_bytes{}, gpu_uploaded_texture_bytes{};
    std::size_t gpu_font_texture_bytes{}, gpu_target_texture_bytes{};
    bool operator==(const RenderMemory&) const = default;
};
// Optional CPU wall time inside begin-pass GL setup and nonempty batch flushes.
// These are driver-call intervals, not GPU execution times. Reset by each begin().
struct RenderCpuTiming {
    double setup_ms{}, upload_draw_ms{};
    double setup_cpu_ms{}, upload_draw_cpu_ms{}; // Optional calling-thread CPU, not GPU time.
    std::size_t batches{}, upload_bytes{};
};
// Must be constructed/destroyed while the owning window's context is current.
class Renderer {
  public:
    static constexpr std::size_t batch_capacity = 4096;
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    static constexpr std::size_t max_shader_source_bytes = 64 * 1024;
    static std::string_view default_vertex_shader();
    static std::string_view default_fragment_shader();
    // Owning context/thread only, between passes. Failed candidates preserve the live program.
    // Calling during begin/end is a logic error. Source/compile/link/interface failures return
    // false.
    ShaderReloadResult reload_shaders(std::string_view vertex, std::string_view fragment);
    std::uint64_t shader_revision() const;
    void begin(int width, int height, Camera camera, Color clear = {0.02F, 0.03F, 0.05F, 1});
    void begin(RenderTargetHandle target, Camera camera, Color clear = {0, 0, 0, 0});
    RenderTargetHandle create_target(int width, int height);
    void resize_target(RenderTargetHandle target, int width, int height);
    void release(RenderTargetHandle target);
    RenderTargetSize target_size(RenderTargetHandle target) const;
    std::size_t live_targets() const;
    std::size_t target_bytes() const;
    static constexpr std::size_t max_targets = 8;
    static constexpr std::size_t max_target_bytes = 64 * 1024 * 1024;
    void target_sprite(RenderTargetHandle target, Transform model, Color color = {},
                       Rect uv = {{0, 0}, {1, 1}});
    void push_clip(PixelRect rectangle); // Intersects the viewport and parent clip; max depth 16.
    void pop_clip();                     // Must balance all pushes before end().
    void set_camera(Camera camera);      // Flushes current geometry; keeps frame counters/clear.
    void quad(Vec2 center, Vec2 size, Color color, float angle = 0, bool checker = false);
    static constexpr std::size_t max_textures = 64;
    static constexpr std::size_t max_texture_reload_bytes = 64 * 1024 * 1024;
    TextureHandle upload(const TextureData& texture, TextureSampling sampling = {});
    // Between passes only; shared by every sprite using this handle and retained on reload.
    void set_sampling(TextureHandle texture, TextureSampling sampling);
    TextureSampling sampling(TextureHandle texture) const;
    // Between passes only. Validate/allocate all candidates before publishing any;
    // preserves handles and previous images on failure. Empty batches are no-ops.
    void replace_textures(std::span<const TextureReplacement> replacements);
    void release(TextureHandle texture);
    std::size_t live_textures() const;
    // Uploaded RGBA8 payload only; excludes built-in atlas, targets and driver overhead.
    std::size_t texture_bytes() const;
    GraphicsInfo graphics_info() const;
    static constexpr float max_texture_uv = 16384;
    // Finite UVs in [-max_texture_uv,max_texture_uv]; wrap/clamp applies to the whole image.
    void sprite(TextureHandle texture, Transform model, Color color = {},
                Rect uv = {{0, 0}, {1, 1}});
    // One current font per renderer. Copies/uploads atomically between passes;
    // failures preserve the previous snapshot. Built-in font is available at startup.
    void set_font(const BitmapFont& font);
    Vec2 text_size(std::string_view value, float pixel_size = 1) const;
    std::size_t font_bytes() const; // Font atlas payload, separate from texture_bytes().
    void text(Vec2 top_left, float pixel_size, std::string_view value, Color color);
    void end();
    RenderStats stats() const;
    // Owning thread only; allowed during a pass. No flush, allocation or GL query.
    // Vertex used bytes reset on flush/end; reservations survive until destruction.
    RenderMemory memory() const;
    // Between passes only. Disabled by default; disabled passes perform no timing reads.
    // thread_cpu additionally samples CLOCK_THREAD_CPUTIME_ID; requires enabled.
    void set_cpu_timing(bool enabled, bool thread_cpu = false);
    RenderCpuTiming cpu_timing() const;
    std::array<unsigned char, 4> pixel(int x, int y) const;
    void screenshot(
        std::string_view path) const; // PPM of last completed pass; before present for window.
    bool healthy() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace engine
