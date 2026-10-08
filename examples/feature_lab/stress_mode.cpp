#include "engine/gpu_timer.hpp"
#include "engine/window.hpp"
#include "stress_benchmark.hpp"
#include "stress_render.hpp"
#include <iostream>
#include <thread>

namespace feature_lab {
int run_stress_mode(int argc, char** argv) {
    const auto options = parse_stress_options(argc, argv);
    if (options.help) {
        std::cout << stress_help();
        return 0;
    }
    if (options.cpu_only)
        return run_cpu_stress(options);
    engine::Window window(options.width, options.height, "Feature Lab | Stress Arena");
    engine::Renderer renderer;
    engine::GpuTimer timer;
    StressReport report(options);
    report.vsync_disable_accepted = window.set_vsync(false);
    const auto device = renderer.graphics_info();
    report.vendor = device.vendor;
    report.device = device.device;
    report.gl_version = device.version;
    const auto unloaded = stress_gpu_resources(renderer);
    engine::Input input;
    std::uint64_t frame = 0;
    bool running = true;
    const auto collect = [&] {
        for (const auto& sample : timer.collect())
            if ((sample.frame - 1) % (options.warmup + options.frames) >= options.warmup)
                report.gpu.add(sample.milliseconds);
    };
    for (std::size_t cycle = 0; cycle < options.cycles && running; ++cycle) {
        {
            const auto loading = StressClock::now();
            StressArena arena(options.workload);
            StressGraphics graphics(renderer, options.width, options.height);
            report.load.add(stress_ms(loading, StressClock::now()));
            const auto baseline = arena.resources();
            const auto loaded = stress_gpu_resources(renderer);
            report.texture_peak = std::max(report.texture_peak, loaded.textures);
            report.texture_bytes_peak = std::max(report.texture_bytes_peak, loaded.texture_bytes);
            report.target_peak = std::max(report.target_peak, loaded.targets);
            report.target_bytes_peak = std::max(report.target_bytes_peak, loaded.target_bytes);
            std::size_t step = 0;
            for (; step < options.warmup + options.frames;) {
                // Polling/presentation and HUD are excluded from CPU submission timings.
                const auto frame_start = StressClock::now();
                if (!window.poll(input) ||
                    engine::button(input.consume(), engine::Key::escape).pressed) {
                    running = false;
                    break;
                }
                if (!window.drawable()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                collect();
                const auto begin = StressClock::now();
                arena.simulate();
                const auto simulated = StressClock::now();
                arena.mix();
                const auto mixed = StressClock::now();
                timer.begin(++frame);
                const auto tiles = graphics.render(arena);
                timer.end();
                const auto submitted = StressClock::now();
                const auto stats = renderer.stats();
                const float w = static_cast<float>(window.width()),
                            h = static_cast<float>(window.height());
                renderer.begin(window.width(), window.height(), {{w * 0.5F, h * 0.5F}, h});
                const auto fit = std::min(w / static_cast<float>(options.width),
                                          h / static_cast<float>(options.height));
                renderer.target_sprite(
                    graphics.target(),
                    engine::Transform::from({w * 0.5F, h * 0.5F}, 0,
                                            {static_cast<float>(options.width) * fit,
                                             static_cast<float>(options.height) * fit}));
                const float scale = std::min({w / 900, h / 480, 1.0F});
                const auto text = [&](float y, std::string_view value, float size = 1.6F) {
                    renderer.text({16 * scale, h - y * scale}, size * scale, value,
                                  {0.1F, 0.95F, 0.8F, 1});
                };
                renderer.quad({w * 0.5F, h - 86 * scale}, {w, 172 * scale},
                              {0.005F, 0.012F, 0.022F, 0.97F});
                text(12, "FEATURE LAB / STRESS ARENA", 3);
                text(48, "CYCLE " + std::to_string(cycle + 1) + " / " +
                             std::to_string(options.cycles) + "   " +
                             (step < options.warmup ? "WARMUP " : "MEASURE ") +
                             std::to_string(step < options.warmup ? step + 1
                                                                  : step + 1 - options.warmup));
                text(72, "SPRITES " + std::to_string(options.workload.sprites) + "  TILES " +
                             std::to_string(options.workload.tiles) + "  ENTITIES " +
                             std::to_string(options.workload.entities));
                text(96, "BODIES " + std::to_string(options.workload.bodies) + "  PAIRS " +
                             std::to_string(arena.collisions().stats().candidate_pairs) +
                             "  MIXER VOICES " + std::to_string(baseline.voices));
                text(120, "QUADS " + std::to_string(stats.quads) + "  CULLED " +
                              std::to_string(stats.culled) + "  DRAWS " +
                              std::to_string(stats.draws));
                text(144, "FIXED " + std::to_string(options.width) + " X " +
                              std::to_string(options.height) + "   OFFLINE AUDIO   ESC ABORT");
                renderer.end();
                const bool final_frame =
                    cycle + 1 == options.cycles && step + 1 == options.warmup + options.frames;
                const auto before_capture = StressClock::now();
                if (final_frame && !options.screenshot.empty())
                    renderer.screenshot(options.screenshot);
                const auto after_capture = StressClock::now();
                window.present();
                const auto end = StressClock::now();
                if (!renderer.healthy())
                    throw std::runtime_error("Stress arena OpenGL validation failed");
                if (step >= options.warmup) {
                    report.simulation.add(stress_ms(begin, simulated));
                    report.audio.add(stress_ms(simulated, mixed));
                    report.submission.add(stress_ms(mixed, submitted));
                    report.frame.add(stress_ms(frame_start, end) -
                                     stress_ms(before_capture, after_capture));
                    report.quads += stats.quads;
                    report.culled += stats.culled;
                    report.draws += stats.draws;
                    report.tiles += tiles;
                    report.pairs += arena.collisions().stats().candidate_pairs;
                    report.contacts += arena.collisions().contacts().size();
                    ++report.measured_frames;
                }
                report.observe(arena, baseline);
                report.resources_stable &= stress_gpu_resources(renderer) == loaded;
                ++step;
            }
            if (step == options.warmup + options.frames)
                report.finish_cycle(arena);
        }
        report.resources_stable &= stress_gpu_resources(renderer) == unloaded;
    }
    collect(); // No GPU wait; outstanding measurements are reported explicitly.
    const auto gpu = timer.stats();
    report.gpu_supported = gpu.supported;
    report.gpu_submitted = gpu.submitted;
    report.gpu_completed = gpu.completed;
    report.gpu_pending = gpu.pending;
    report.gpu_skipped = gpu.skipped;
    report.gpu_invalid = gpu.invalid;
    report.window_width = window.width();
    report.window_height = window.height();
    report.complete = report.completed_cycles == options.cycles;
    report.rss_end = stress_rss_bytes();
    report.write(options);
    return report.complete && report.resources_stable && gpu.invalid == 0 ? 0 : 1;
}
} // namespace feature_lab
