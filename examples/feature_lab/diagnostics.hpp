#pragma once
#include "engine/debug_draw.hpp"
#include "engine/diagnostics.hpp"
#include "game.hpp"
#include "hud_text.hpp"
#include "overlays.hpp"
#include <string>

namespace feature_lab {
class Diagnostics {
  public:
    engine::DebugControls controls;
    std::span<const engine::GpuTimingSample> collect_gpu(engine::GpuTimer& timer,
                                                         std::uint64_t frame) {
        const auto samples = timer.collect();
        record_gpu(samples, timer.stats(), frame);
        return samples;
    }
    // Ingest already-collected timing data; also allows deterministic presentation fixtures.
    // Samples must be ordered, valid and no newer than frame, as returned by GpuTimer.
    void record_gpu(std::span<const engine::GpuTimingSample> samples, engine::GpuTimingStats stats,
                    std::uint64_t frame) {
        for (const auto sample : samples)
            gpu_history_.record(sample);
        gpu_stats_ = stats;
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
        std::string text;
        draw_hud(renderer, game, width, height, dropped_seconds, text);
    }
    void draw_hud(engine::Renderer& renderer, const Game& game, int width, int height,
                  double dropped_seconds, std::string& text) const {
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
        row(format_text(text, "TICK ", game.ticks, "  STEPS/FRAME ", steps_));
        row(format_text(text, "WINDOW ", frames.samples, "/240 FRAMES"));
        row(format_text(text, "WALL MEAN ", Fixed2{frames.wall_mean_ms}, " MS"));
        row(format_text(text, "CPU MEAN ", Fixed2{frames.cpu_mean_ms}, "  P95 ",
                        Fixed2{frames.cpu_p95_ms}, " MS"));
        row("CPU = UPDATE + SUBMIT / NO SWAP WAIT");
        const auto gpu = gpu_history_.summary();
        row(!gpu_stats_.supported ? "GPU TIMING UNAVAILABLE / NEEDS 64 BITS"
            : !gpu.samples        ? "GPU WAITING FOR COMPLETED SAMPLE"
                                  : format_text(text, "GPU MEAN ", Fixed2{gpu.mean_ms}, "  P95 ",
                                                Fixed2{gpu.p95_ms}, " MS"));
        row(gpu.samples ? format_text(text, "GPU SAMPLES ", gpu.samples, "  AGE ",
                                      frame_ - gpu.last_frame, " FRAMES")
                        : "GPU SAMPLES 0 / ASYNC READBACK");
        row(format_text(text, "GPU PENDING ", gpu_stats_.pending, "/8 SKIP ", gpu_stats_.skipped,
                        " BAD ", gpu_stats_.invalid));
        row(format_text(text, "CLOCK DROPPED ", Fixed2{dropped_seconds * 1000}, " MS TOTAL"));
        row(format_text(text, "QUADS WIN ", window_.quads, "  MAP ", minimap_.quads));
        row(format_text(text, "DRAWS WIN ", window_.draws, "  MAP ", minimap_.draws));
        row(format_text(text, "CULLED ", window_.culled + minimap_.culled,
                        " / GPU = RENDER INTERVAL"));
        row(format_text(text, "TEXTURES ", renderer.live_textures(), "  TARGETS ",
                        renderer.live_targets(), "  KB ",
                        Fixed2{static_cast<double>(renderer.target_bytes()) / 1024}, " FONT KB ",
                        Fixed2{static_cast<double>(renderer.font_bytes()) / 1024}));
        row(format_text(text, "BODIES ", game.collisions.bodies().size(), "  CONTACTS ",
                        game.collisions.contacts().size()));
        row(format_text(text, "GRID PAIRS ", collision.candidate_pairs, "  TESTS ",
                        collision.narrow_tests));
        row(controls.shapes ? "F3 SHAPES ON / RED CONTACT GOLD SENSOR"
                            : "F3 COLLISION / CAMERA SHAPES OFF");
        row("BLUE STATIC / CYAN MOVING AND CAMERA");
        row(format_text(text, "ENTITIES ", game.world.size(), "  PICKUPS ",
                        game.world.component_count<CorePickup>()));
        renderer.pop_clip();
    }

  private:
    engine::FrameHistory history_;
    engine::GpuTimingHistory gpu_history_;
    engine::GpuTimingStats gpu_stats_;
    std::uint64_t frame_{};
    engine::RenderStats window_, minimap_;
    unsigned steps_{};
};
} // namespace feature_lab
