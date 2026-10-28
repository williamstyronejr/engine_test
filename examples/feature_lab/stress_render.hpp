#pragma once
#include "engine/renderer.hpp"
#include "engine/thread_cpu_clock.hpp"
#include "stress.hpp"
#include "stress_context.hpp"
#include "stress_preparation.hpp"
#include <chrono>

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
    std::size_t render(const StressArena& arena, StressPreparationPhases* phases = nullptr,
                       StressContextPhases* contexts = nullptr) {
        if (contexts && !phases)
            throw std::invalid_argument("Context samples require preparation phases");
        const auto phase = [&](std::size_t index, auto&& work) {
            if (!phases) {
                work();
                return;
            }
            // begin() resets cumulative GL measurements; subsequent phases use deltas.
            const auto before = index ? renderer_.cpu_timing() : engine::RenderCpuTiming{};
            // Counter snapshots enclose the timed phase, including GL work. Their calls
            // lie outside phase timings, inside the enclosing render interval.
            const auto switches_before =
                contexts ? engine::thread_context_switches() : engine::ThreadContextSwitches{};
            const auto start = std::chrono::steady_clock::now();
            const auto cpu_start = engine::thread_cpu_nanoseconds();
            work();
            const auto cpu_end = engine::thread_cpu_nanoseconds();
            const auto end = std::chrono::steady_clock::now();
            if (contexts)
                (*contexts)[index] = {switches_before, engine::thread_context_switches()};
            const auto after = renderer_.cpu_timing();
            (*phases)[index] = {std::chrono::duration<double, std::milli>(end - start).count(),
                                static_cast<double>(cpu_end - cpu_start) / 1e6,
                                (after.setup_ms + after.upload_draw_ms) -
                                    (before.setup_ms + before.upload_draw_ms),
                                (after.setup_cpu_ms + after.upload_draw_cpu_ms) -
                                    (before.setup_cpu_ms + before.upload_draw_cpu_ms)};
        };
        const engine::Camera camera{{0, 0}, 36};
        const auto size = renderer_.target_size(target_);
        const auto half = camera.extent(size.width, size.height) * 0.5F;
        phase(0, [&] { renderer_.begin(target_, camera, {0.01F, 0.025F, 0.04F, 1}); });
        std::size_t tiles{};
        phase(1, [&] {
            tiles = arena.tiles()
                        .visit_visible(0, {{-half.x, -half.y}, half},
                                       [&](const auto& tile) {
                                           renderer_.sprite(texture_,
                                                            engine::Transform::from(
                                                                tile.center, 0, {0.125F, 0.125F}),
                                                            {}, tile.uv);
                                       })
                        .tiles;
        });
        phase(2, [&] {
            for (const auto& sprite : arena.sprites())
                renderer_.sprite(texture_,
                                 engine::Transform::from(sprite.position, 0, {0.12F, 0.12F}), {},
                                 engine::atlas_uv(sprite.cell, 2, 2));
        });
        phase(3, [&] { renderer_.end(); });
        return tiles;
    }
    engine::RenderTargetHandle target() const { return target_; }

  private:
    engine::Renderer& renderer_;
    engine::TextureHandle texture_;
    engine::RenderTargetHandle target_;
};
} // namespace feature_lab
