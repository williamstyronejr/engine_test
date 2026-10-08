#include "../examples/feature_lab/stress_render.hpp"
#include "engine/window.hpp"
#include "test.hpp"
#include <cstdlib>
namespace {
using namespace testing;
using namespace engine;
using namespace feature_lab;
Renderer* renderer{};
TEST(repeated_gpu_load_draw_unload_and_rollover) {
    const auto baseline = stress_gpu_resources(*renderer);
    StressArena arena({8192, 1024, 16, 16, 2, 4, false});
    for (int cycle = 0; cycle < 12; ++cycle) {
        {
            StressGraphics graphics(*renderer, 256, 144);
            CHECK(renderer->texture_bytes() == baseline.texture_bytes + 16);
            CHECK(renderer->target_bytes() == baseline.target_bytes + 256 * 144 * 4);
            CHECK(graphics.render(arena) == 1024);
            CHECK(renderer->stats().quads == 1024 + 6144);
            CHECK(renderer->stats().culled == 2048);
            CHECK(renderer->stats().draws == 2);
            const auto pixel = renderer->pixel(128, 72);
            CHECK(pixel[3] == 255);
        }
        CHECK(stress_gpu_resources(*renderer) == baseline);
    }
    CHECK(renderer->healthy());
}
TEST(texture_byte_accounting_replacement_failure_and_cleanup) {
    const auto baseline = renderer->texture_bytes();
    const auto first = renderer->upload({1, 1, {255, 0, 0, 255}});
    const auto second = renderer->upload({2, 1, {255, 0, 0, 255, 0, 255, 0, 255}});
    CHECK(renderer->texture_bytes() == baseline + 12);
    TextureData larger{4, 4, std::vector<std::uint8_t>(64, 255)};
    renderer->replace_textures(std::array{TextureReplacement{first, larger}});
    CHECK(renderer->texture_bytes() == baseline + 72);
    TextureData invalid{1, 1, {}};
    rejects([&] { renderer->replace_textures(std::array{TextureReplacement{first, invalid}}); });
    CHECK(renderer->texture_bytes() == baseline + 72);
    renderer->release(first);
    renderer->release(second);
    CHECK(renderer->texture_bytes() == baseline);
    rejects([&] { StressGraphics bad(*renderer, 0, 144); });
    CHECK(renderer->texture_bytes() == baseline);
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY"))
        return 77;
    try {
        Window window(256, 144, "Engine stress graphics tests", false);
        Renderer instance;
        renderer = &instance;
        return run_tests();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
