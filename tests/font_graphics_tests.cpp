#include "../examples/feature_lab/font_gallery.hpp"
#include "engine/renderer.hpp"
#include "engine/window.hpp"
#include "font_fixture.hpp"
#include "test.hpp"
#include <cstdlib>
#include <limits>

namespace {
using namespace engine;
using namespace testing;
TEST(font_atlas_pixels_offsets_spacing_fallback_and_batching) {
    Window window(64, 64, "Font pixels", false);
    Renderer renderer;
    renderer.set_font(BitmapFont(metric_font()));
    CHECK(renderer.font_bytes() == 64 && renderer.live_textures() == 0);
    const auto target = renderer.create_target(64, 64);
    renderer.begin(target, {{32, -32}, 64}, {0, 0, 0, 1});
    renderer.text({8, -8}, 4, "A ?\nZ", {});
    renderer.end();
    CHECK(renderer.stats().quads == 3 && renderer.stats().draws == 1);
    // Glyph A starts at x=4, top y=12; bottom-left texture storage must not flip it.
    CHECK(renderer.pixel(5, 50)[0] == 255);
    CHECK(renderer.pixel(9, 50)[0] == 0); // Transparent top-right texel.
    CHECK(renderer.pixel(5, 46)[1] == 255);
    CHECK(renderer.pixel(9, 46)[2] == 255);
    CHECK(renderer.pixel(33, 54)[0] == 255); // advance A=16 + space=8.
    CHECK(renderer.pixel(9, 34)[0] == 255);  // LF line-height=20, Z -> fallback.
    CHECK(renderer.healthy());
}
TEST(font_clipping_tint_ordering_and_offscreen_alpha) {
    Window window(64, 64, "Font alpha", false);
    Renderer renderer;
    renderer.set_font(BitmapFont(metric_font()));
    const auto target = renderer.create_target(64, 64);
    renderer.begin(target, {{32, -32}, 64});
    renderer.push_clip({10, 8, 2, 4});
    renderer.text({8, -8}, 4, "?", {0, 1, 0, 0.5F});
    renderer.pop_clip();
    renderer.end();
    const auto green = renderer.pixel(10, 54);
    CHECK(green[0] == 0 && green[1] >= 186 && green[1] <= 190);
    CHECK(green[3] >= 127 && green[3] <= 129);
    CHECK(renderer.pixel(9, 54)[3] == 0);
    renderer.begin(64, 64, {{32, -32}, 64}, {0, 0, 0, 1});
    renderer.target_sprite(target, Transform::from({32, -32}, 0, {64, 64}));
    renderer.text({8, -8}, 4, "?", {1, 0, 0, 1});
    renderer.quad({11, -10}, {2, 4}, {0, 0, 1, 1});
    renderer.end();
    CHECK(renderer.pixel(9, 54)[0] == 255);
    CHECK(renderer.pixel(10, 54)[2] == 255);
    CHECK(renderer.healthy());
}
TEST(font_replacement_boundaries_lifetime_and_invalid_text) {
    Window window(64, 64, "Font lifetime", false);
    Renderer renderer;
    const auto target = renderer.create_target(64, 64);
    for (int i = 0; i < 32; ++i) {
        {
            const BitmapFont candidate(i % 2 ? builtin_font() : metric_font());
            renderer.set_font(candidate);
        } // Renderer must own the copied snapshot and atlas.
        CHECK(renderer.font_bytes() == (i % 2 ? 16384U : 64U));
        renderer.begin(target, {{32, -32}, 64}, {0, 0, 0, 1});
        rejects([&] { renderer.set_font(BitmapFont(metric_font())); });
        rejects([&] { renderer.text({}, 0, "A", {}); });
        rejects([&] { renderer.text({std::numeric_limits<float>::quiet_NaN(), 0}, 1, "", {}); });
        rejects([&] { renderer.text({}, 1, "", {1, 1, 1, 2}); });
        rejects([&] { renderer.text({}, 1, std::string(4097, 'A'), {}); });
        CHECK(renderer.stats().quads == 0);
        renderer.text({8, -8}, 2, "A A", {});
        renderer.end();
        CHECK(renderer.stats().quads == 2 && renderer.stats().draws == 1);
        CHECK(renderer.live_textures() == 0 && renderer.live_targets() == 1);
        CHECK(renderer.healthy());
    }
    rejects([&] { renderer.text({}, 1, "", {}); });
}
TEST(font_gallery_draws_at_small_and_large_viewports) {
    Window window(64, 64, "Font gallery", false);
    Renderer renderer;
    const auto target = renderer.create_target(320, 240);
    for (const auto size : {RenderTargetSize{320, 240}, RenderTargetSize{1280, 720}}) {
        renderer.resize_target(target, size.width, size.height);
        renderer.begin(target, {{0, 0}, 2}, {0, 0, 0, 1});
        feature_lab::draw_font_gallery(renderer, size.width, size.height);
        renderer.end();
        CHECK(renderer.stats().quads > 150 && renderer.healthy());
        const auto panel = renderer.pixel(size.width / 2, size.height / 2);
        CHECK(panel[2] > 40);
    }
}
TEST(font_builtin_retains_original_pixel_shapes) {
    Window window(64, 64, "Fallback font", false);
    Renderer renderer;
    const auto target = renderer.create_target(64, 64);
    renderer.begin(target, {{32, -32}, 64}, {0, 0, 0, 1});
    renderer.text({0, 0}, 2, "A a", {});
    renderer.end();
    CHECK(renderer.stats().quads == 2);
    CHECK(renderer.pixel(0, 63)[0] == 0);
    CHECK(renderer.pixel(2, 63)[0] == 255);
    CHECK(renderer.pixel(0, 61)[0] == 255);
    CHECK(renderer.pixel(24, 63)[0] == 255); // Unknown lowercase uses visible '?'.
    CHECK(renderer.healthy());
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY"))
        return 77;
    return testing::run_tests();
}
