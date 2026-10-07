#pragma once
#include "engine/debug_draw.hpp"
#include "engine/diagnostics.hpp"
#include "game.hpp"
#include "overlays.hpp"
#include <charconv>
#include <string>

namespace feature_lab {
class Diagnostics {
  public:
    engine::DebugControls controls;
    void collect_gpu(engine::GpuTimer& timer, std::uint64_t frame) {
        for (const auto sample : timer.collect())
            gpu_history_.record(sample);
        gpu_stats_ = timer.stats();
        frame_ = frame;
    }
    engine::GpuTimingSummary gpu_summary() const { return gpu_history_.summary(); }
    void record(engine::FrameSample sample, engine::RenderStats window, engine::RenderStats minimap,
                unsigned simulation_steps) {
        history_.record(sample);
        window_ = window;
        minimap_ = minimap;
        steps_ = simulation_steps;
    }
    void draw_world(engine::Renderer& renderer, const Game& game, engine::Camera camera, int width,
                    int height) const {
        using namespace engine;
        if (!controls.shapes)
            return;
        const float thickness = camera.height / static_cast<float>(height) * 1.5F;
        for (const auto& body : game.collisions.bodies()) {
            const bool contact = std::any_of(
                game.collisions.contacts().begin(), game.collisions.contacts().end(),
                [&](const Contact2D& pair) { return pair.a == body.id || pair.b == body.id; });
            const Color color = contact       ? Color{1, 0.12F, 0.12F, 1}
                                : body.sensor ? Color{1, 0.65F, 0.06F, 1}
                                : body.moving ? Color{0.1F, 1, 0.8F, 1}
                                              : Color{0.25F, 0.55F, 1, 1};
            debug_shape(renderer, body.shape, thickness, color);
        }
        const auto half = camera.extent(width, height) * 0.5F - Vec2{thickness, thickness};
        debug_rect(renderer, {camera.center - half, camera.center + half}, thickness,
                   {0.1F, 1, 0.8F, 1});
    }
    // Uses the caller's pixel camera. Completed-pass counters lag by one rendered frame.
    void draw_hud(engine::Renderer& renderer, const Game& game, int width, int height,
                  double dropped_seconds) const {
        using namespace engine;
        if (!controls.visible)
            return;
        const float scale = hud_scale(width, height);
        renderer.quad({254 * scale, -314 * scale}, {460 * scale, 424 * scale},
                      {0.008F, 0.018F, 0.03F, 0.98F});
        renderer.push_clip({static_cast<int>(24 * scale), static_cast<int>(102 * scale),
                            static_cast<int>(460 * scale), static_cast<int>(424 * scale)});
        float y = 114;
        const auto row = [&](std::string_view value, Color color = {0.8F, 0.9F, 0.94F, 1}) {
            renderer.text({36 * scale, -y * scale}, 1.35F * scale, value, color);
            y += 21;
        };
        const auto frames = history_.summary();
        const auto collision = game.collisions.stats();
        row("DIAGNOSTICS / F2", {0.04F, 0.85F, 0.7F, 1});
        row(game.won ? "COMPLETE" : game.paused ? "PAUSED / F10 SINGLE STEP" : "RUNNING / 60 HZ");
        row("TICK " + std::to_string(game.ticks) + "  STEPS/FRAME " + std::to_string(steps_));
        row("WINDOW " + std::to_string(frames.samples) + "/240 FRAMES");
        row("WALL MEAN " + number(frames.wall_mean_ms) + " MS");
        row("CPU MEAN " + number(frames.cpu_mean_ms) + "  P95 " + number(frames.cpu_p95_ms) +
            " MS");
        row("CPU = UPDATE + SUBMIT / NO SWAP WAIT");
        const auto gpu = gpu_history_.summary();
        row(!gpu_stats_.supported ? "GPU TIMING UNAVAILABLE / NEEDS 64 BITS"
            : !gpu.samples
                ? "GPU WAITING FOR COMPLETED SAMPLE"
                : "GPU MEAN " + number(gpu.mean_ms) + "  P95 " + number(gpu.p95_ms) + " MS");
        row(gpu.samples ? "GPU SAMPLES " + std::to_string(gpu.samples) + "  AGE " +
                              std::to_string(frame_ - gpu.last_frame) + " FRAMES"
                        : "GPU SAMPLES 0 / ASYNC READBACK");
        row("GPU PENDING " + std::to_string(gpu_stats_.pending) + "/8 SKIP " +
            std::to_string(gpu_stats_.skipped) + " BAD " + std::to_string(gpu_stats_.invalid));
        row("CLOCK DROPPED " + number(dropped_seconds * 1000) + " MS TOTAL");
        row("QUADS WIN " + std::to_string(window_.quads) + "  MAP " +
            std::to_string(minimap_.quads));
        row("DRAWS WIN " + std::to_string(window_.draws) + "  MAP " +
            std::to_string(minimap_.draws));
        row("CULLED " + std::to_string(window_.culled + minimap_.culled) +
            " / GPU = RENDER INTERVAL");
        row("TEXTURES " + std::to_string(renderer.live_textures()) + "  TARGETS " +
            std::to_string(renderer.live_targets()) + "  KB " +
            number(static_cast<double>(renderer.target_bytes()) / 1024));
        row("BODIES " + std::to_string(game.collisions.bodies().size()) + "  CONTACTS " +
            std::to_string(game.collisions.contacts().size()));
        row("GRID PAIRS " + std::to_string(collision.candidate_pairs) + "  TESTS " +
            std::to_string(collision.narrow_tests));
        row(controls.shapes ? "F3 SHAPES ON / RED CONTACT GOLD SENSOR"
                            : "F3 COLLISION / CAMERA SHAPES OFF");
        row("BLUE STATIC / CYAN MOVING AND CAMERA");
        renderer.pop_clip();
    }

  private:
    static std::string number(double value) {
        std::array<char, 64> buffer{};
        const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                                std::chars_format::fixed, 2);
        return error == std::errc{} ? std::string(buffer.data(), end) : "OVERFLOW";
    }
    engine::FrameHistory history_;
    engine::GpuTimingHistory gpu_history_;
    engine::GpuTimingStats gpu_stats_;
    std::uint64_t frame_{};
    engine::RenderStats window_, minimap_;
    unsigned steps_{};
};
} // namespace feature_lab
