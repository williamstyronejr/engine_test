#pragma once
#include "engine/renderer.hpp"
#include "stress.hpp"

namespace feature_lab {
struct StressGpuResources {
    std::size_t textures{}, texture_bytes{}, targets{}, target_bytes{};
    bool operator==(const StressGpuResources&) const = default;
};
inline StressGpuResources stress_gpu_resources(const engine::Renderer& renderer) {
    return {renderer.live_textures(), renderer.texture_bytes(), renderer.live_targets(),
            renderer.target_bytes()};
}
// Renderer/context must outlive this session. Handles are released between passes.
class StressGraphics {
  public:
    StressGraphics(engine::Renderer& renderer, int width, int height) : renderer_(renderer) {
        texture_ = renderer_.upload(
            {2, 2, {24, 60, 85, 255, 35, 100, 135, 255, 30, 195, 170, 255, 240, 170, 60, 255}});
        try {
            target_ = renderer_.create_target(width, height);
        } catch (...) {
            renderer_.release(texture_);
            throw;
        }
    }
    ~StressGraphics() {
        try {
            renderer_.release(target_);
            renderer_.release(texture_);
        } catch (...) {
            // If a failed pass is still open during unwinding, Renderer retains
            // ownership and cleans these objects before its context is destroyed.
        }
    }
    StressGraphics(const StressGraphics&) = delete;
    StressGraphics& operator=(const StressGraphics&) = delete;
    std::size_t render(const StressArena& arena) {
        const engine::Camera camera{{0, 0}, 36};
        const auto size = renderer_.target_size(target_);
        const auto half = camera.extent(size.width, size.height) * 0.5F;
        renderer_.begin(target_, camera, {0.01F, 0.025F, 0.04F, 1});
        const auto visited =
            arena.tiles().visit_visible(0, {{-half.x, -half.y}, half}, [&](const auto& tile) {
                renderer_.sprite(texture_,
                                 engine::Transform::from(tile.center, 0, {0.125F, 0.125F}), {},
                                 tile.uv);
            });
        for (const auto& sprite : arena.sprites())
            renderer_.sprite(texture_, engine::Transform::from(sprite.position, 0, {0.12F, 0.12F}),
                             {}, engine::atlas_uv(sprite.cell, 2, 2));
        renderer_.end();
        return visited.tiles;
    }
    engine::RenderTargetHandle target() const { return target_; }

  private:
    engine::Renderer& renderer_;
    engine::TextureHandle texture_;
    engine::RenderTargetHandle target_;
};
} // namespace feature_lab
