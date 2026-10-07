#pragma once
#include "engine/assets.hpp"
#include "engine/math.hpp"
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
struct RenderStats {
    std::size_t quads{}, culled{}, draws{};
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
    TextureHandle upload(const TextureData& texture);
    // Between passes only. Validate/allocate all candidates before publishing any;
    // preserves handles and previous images on failure. Empty batches are no-ops.
    void replace_textures(std::span<const TextureReplacement> replacements);
    void release(TextureHandle texture);
    std::size_t live_textures() const;
    void sprite(TextureHandle texture, Transform model, Color color = {},
                Rect uv = {{0, 0}, {1, 1}});
    void text(Vec2 top_left, float pixel_size, std::string_view value, Color color);
    void end();
    RenderStats stats() const;
    std::array<unsigned char, 4> pixel(int x, int y) const;
    void screenshot(
        std::string_view path) const; // PPM of last completed pass; before present for window.
    bool healthy() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace engine
