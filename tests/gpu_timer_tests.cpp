#include "../engine/src/gpu_timer_backend.hpp"
#include "../examples/feature_lab/diagnostics.hpp"
#include "engine/gpu_timer.hpp"
#include "engine/renderer.hpp"
#include "engine/window.hpp"
#include "test.hpp"
#include <GL/glx.h>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <vector>

namespace {
using namespace engine;
using namespace testing;
void flush_commands() {
    const auto flush = reinterpret_cast<PFNGLFLUSHPROC>(
        glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glFlush")));
    CHECK(flush != nullptr);
    flush(); // Test-only submission on hidden windows; no wait for GPU completion.
}
template <class F> void eventually(F condition) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!condition()) {
        CHECK(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
TEST(real_queries_return_ordered_finite_timings_without_changing_pixels) {
    engine::Window window(64, 64, "GPU timer pixels", false);
    Renderer renderer;
    GpuTimer timer;
    CHECK(timer.stats().supported && timer.stats().counter_bits == 64);
    const auto target = renderer.create_target(64, 64);
    for (std::uint64_t frame = 1; frame <= 2; ++frame) {
        CHECK(timer.begin(frame));
        renderer.begin(target, {{0, 0}, 2}, {0, 0, 0, 1});
        renderer.quad({}, {2, 2}, {1, 0, 0, 1});
        renderer.end();
        timer.end();
    }
    CHECK(timer.stats().submitted == 2 && timer.stats().pending == 2);
    flush_commands();
    std::vector<GpuTimingSample> samples;
    eventually([&] {
        const auto completed = timer.collect();
        samples.insert(samples.end(), completed.begin(), completed.end());
        return samples.size() == 2;
    });
    CHECK(samples[0].frame == 1 && samples[1].frame == 2);
    for (const auto& sample : samples)
        CHECK(std::isfinite(sample.milliseconds) && sample.milliseconds >= 0);
    CHECK(timer.stats().completed == 2 && timer.stats().pending == 0);
    CHECK(timer.collect().empty());
    CHECK(renderer.pixel(32, 32)[0] > 250 && renderer.healthy());
}
TEST(real_full_pool_skip_recovery_and_shader_reload) {
    engine::Window window(64, 64, "GPU timer saturation", false);
    Renderer renderer;
    GpuTimer timer;
    const auto target = renderer.create_target(32, 32);
    for (std::uint64_t i = 1; i <= GpuTimer::capacity; ++i) {
        CHECK(timer.begin(i));
        renderer.begin(target, {{0, 0}, 2});
        renderer.quad({}, {2, 2}, {0, 1, 0, 1});
        renderer.end();
        timer.end();
    }
    CHECK(!timer.begin(9)); // No collect: ready or pending slots are both still owned.
    timer.end();
    CHECK(timer.stats().skipped == 1 && timer.stats().pending == GpuTimer::capacity);
    CHECK(
        renderer
            .reload_shaders(Renderer::default_vertex_shader(), Renderer::default_fragment_shader())
            .applied);
    renderer.resize_target(target, 64, 64);
    flush_commands();
    eventually([&] {
        timer.collect();
        return timer.stats().pending == 0;
    });
    CHECK(timer.begin(10));
    renderer.begin(target, {{0, 0}, 2}, {0, 1, 0, 1});
    renderer.end();
    timer.end();
    flush_commands();
    std::uint64_t last{};
    eventually([&] {
        for (const auto sample : timer.collect())
            last = sample.frame;
        return last == 10;
    });
    CHECK(timer.stats().submitted == 9 && timer.stats().completed == 9 &&
          timer.stats().invalid == 0);
    CHECK(renderer.pixel(32, 32)[1] > 250 && renderer.healthy());
}
TEST(query_objects_are_deleted_with_pending_or_unfinished_scopes) {
    engine::Window window(16, 16, "GPU timer lifetimes", false);
    Renderer renderer;
    const auto is_query = reinterpret_cast<PFNGLISQUERYPROC>(
        glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glIsQuery")));
    CHECK(is_query != nullptr);
    for (int cycle = 0; cycle < 8; ++cycle) {
        std::array<GLuint, GpuTimer::capacity * 2> names{};
        {
            detail::TimerBackend backend;
            names = backend.queries;
            for (std::size_t i = 0; i < names.size(); ++i) {
                backend.stamp(i);
                CHECK(is_query(names[i]));
            }
        }
        for (const auto name : names)
            CHECK(!is_query(name));
        {
            GpuTimer timer;
            CHECK(timer.begin(1)); // Timestamp scopes need no EndQuery during teardown.
        }
        CHECK(renderer.healthy());
    }
}
TEST(overlay_displays_collected_samples_and_keeps_resources_stable) {
    engine::Window window(64, 64, "GPU overlay", false);
    Renderer renderer;
    GpuTimer timer;
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    feature_lab::Diagnostics diagnostics;
    diagnostics.controls.visible = true;
    const auto target = renderer.create_target(64, 64);
    timer.begin(1);
    renderer.begin(target, {{0, 0}, 2}, {1, 0, 0, 1});
    renderer.end();
    timer.end();
    flush_commands();
    eventually([&] {
        diagnostics.collect_gpu(timer, 2);
        return diagnostics.gpu_summary().samples == 1;
    });
    CHECK(diagnostics.gpu_summary().last_frame == 1);
    const auto textures = renderer.live_textures();
    for (const auto [width, height] :
         {std::pair{320, 240}, {640, 480}, {1280, 720}, {1050, 1360}}) {
        renderer.resize_target(target, width, height);
        const float w = static_cast<float>(width), h = static_cast<float>(height);
        renderer.begin(target, {{w / 2, -h / 2}, h}, {1, 0, 0, 1});
        diagnostics.draw_hud(renderer, game, width, height, 0);
        renderer.end();
        CHECK(renderer.stats().quads > 100);
        const auto scale = feature_lab::hud_scale(width, height);
        // The enlarged panel contains the extra GPU rows; clipping still isolates its edge.
        CHECK(renderer.pixel(static_cast<int>(26 * scale),
                             height - 1 - static_cast<int>(520 * scale))[0] < 100);
        CHECK(renderer.pixel(static_cast<int>(20 * scale),
                             height - 1 - static_cast<int>(520 * scale))[0] > 240);
        CHECK(renderer.live_textures() == textures && renderer.live_targets() == 1);
    }
    CHECK(renderer.healthy());
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY")) {
        std::cout << "SKIP: DISPLAY unavailable\n";
        return 77;
    }
    {
        engine::Window probe(16, 16, "GPU timestamp support", false);
        Gl gl;
        GLint bits{};
        gl.GetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &bits);
        if (bits != 64) {
            std::cout << "SKIP: 64-bit GPU timestamps unavailable\n";
            return 77;
        }
    }
    return testing::run_tests();
}
