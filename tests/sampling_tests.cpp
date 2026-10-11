#include "../examples/feature_lab/sampling_gallery.hpp"
#include "engine/atlas.hpp"
#include "engine/window.hpp"
#include "test.hpp"
#include <GL/gl.h>
#include <cstdlib>
#include <limits>

namespace {
using namespace engine;
using namespace testing;
const TextureData colors{2, 1, {255, 0, 0, 255, 0, 0, 255, 255}};
constexpr TextureSampling linear{TextureFilter::linear, TextureFilter::linear};
struct Fixture {
    Window window{64, 64, "Texture sampling", false};
    Renderer renderer;
    RenderTargetHandle target = renderer.create_target(64, 64);
    void draw(TextureHandle texture, Rect uv = {{0, 0}, {1, 1}}) {
        renderer.begin(target, {{}, 2}, {0, 0, 0, 1});
        renderer.sprite(texture, Transform::from({}, 0, {2, 2}), {}, uv);
        renderer.end();
    }
    auto sample(TextureHandle texture, Vec2 uv) {
        draw(texture, {uv, uv});
        return renderer.pixel(32, 32);
    }
};
TEST(nearest_linear_magnification_and_independent_minification) {
    Fixture f;
    const auto handle = f.renderer.upload(colors);
    CHECK(f.renderer.sampling(handle) == TextureSampling{});
    auto pixel = f.sample(handle, {0.5F, 0.5F});
    CHECK(pixel[0] < 3 && pixel[2] > 250);
    f.renderer.set_sampling(handle, {TextureFilter::nearest, TextureFilter::linear});
    pixel = f.sample(handle, {0.5F, 0.5F});
    CHECK(pixel[0] >= 186 && pixel[0] <= 190 && pixel[2] >= 186 && pixel[2] <= 190);
    // Two texels over one destination pixel select MIN_FILTER, independent of MAG_FILTER.
    f.renderer.resize_target(f.target, 1, 1);
    f.renderer.set_sampling(handle, {TextureFilter::linear, TextureFilter::nearest});
    f.draw(handle);
    pixel = f.renderer.pixel(0, 0);
    CHECK(pixel[0] >= 186 && pixel[0] <= 190 && pixel[2] >= 186 && pixel[2] <= 190);
    f.renderer.set_sampling(handle, {});
    f.draw(handle);
    CHECK(f.renderer.pixel(0, 0)[2] > 250 && f.renderer.pixel(0, 0)[0] < 3);
    CHECK(f.renderer.healthy());
}
TEST(wrap_axes_negative_uv_flip_and_linear_seams) {
    Fixture f;
    const auto horizontal = f.renderer.upload(colors);
    CHECK(f.sample(horizontal, {-0.25F, 0.5F})[0] > 250);
    CHECK(f.sample(horizontal, {1.25F, 0.5F})[2] > 250);
    f.renderer.set_sampling(horizontal, {TextureFilter::nearest, TextureFilter::nearest,
                                         TextureWrap::repeat, TextureWrap::clamp});
    CHECK(f.sample(horizontal, {-0.25F, 0.5F})[2] > 250);
    CHECK(f.sample(horizontal, {1.25F, 0.5F})[0] > 250);
    const auto vertical =
        f.renderer.upload({1, 2, colors.rgba}, {TextureFilter::nearest, TextureFilter::nearest,
                                                TextureWrap::clamp, TextureWrap::repeat});
    CHECK(f.sample(vertical, {-3, -0.25F})[2] > 250);
    CHECK(f.sample(vertical, {4, 1.25F})[0] > 250);
    f.draw(horizontal, {{1.5F, 0}, {-0.5F, 1}});
    CHECK(f.renderer.pixel(0, 32)[0] > 250 && f.renderer.pixel(63, 32)[2] > 250);
    f.renderer.set_sampling(horizontal, {TextureFilter::linear, TextureFilter::linear,
                                         TextureWrap::repeat, TextureWrap::clamp});
    auto seam = f.sample(horizontal, {0, 0.5F});
    CHECK(seam[0] >= 186 && seam[0] <= 190 && seam[2] >= 186 && seam[2] <= 190);
    f.renderer.set_sampling(horizontal, linear);
    CHECK(f.sample(horizontal, {0, 0.5F})[0] > 250 && f.sample(horizontal, {0, 0.5F})[2] < 3);
    CHECK(f.renderer.healthy());
}
TEST(atlas_inset_removes_adjacent_cell_bleed_including_single_texels) {
    Fixture f;
    // Red cell contains two texels, immediately followed by a blue cell.
    const auto atlas = f.renderer.upload(
        {4, 1, {255, 0, 0, 255, 255, 0, 0, 255, 0, 0, 255, 255, 0, 0, 255, 255}}, linear);
    f.draw(atlas, atlas_uv(0, 2, 1));
    CHECK(f.renderer.pixel(63, 32)[2] > 100); // Uninset cell demonstrates the bleed.
    auto uv = atlas_uv_inset(0, 2, 1, 4, 1);
    f.draw(atlas, uv);
    for (int x : {0, 1, 31, 62, 63})
        CHECK(f.renderer.pixel(x, 32)[0] > 250 && f.renderer.pixel(x, 32)[2] < 3);
    std::swap(uv.min.x, uv.max.x);
    f.draw(atlas, uv);
    CHECK(f.renderer.pixel(0, 32)[2] < 3 && f.renderer.pixel(63, 32)[2] < 3);
    const auto tiny = f.renderer.upload(colors, linear);
    f.draw(tiny, atlas_uv_inset(0, 2, 1, 2, 1));
    CHECK(f.renderer.pixel(63, 32)[0] > 250 && f.renderer.pixel(63, 32)[2] < 3);
    CHECK(f.renderer.healthy());
}
TEST(state_changes_preserve_bindings_and_failed_changes_preserve_batches) {
    Fixture f;
    const auto a = f.renderer.upload(colors), b = f.renderer.upload(colors, linear);
    f.draw(b);
    GLint before{}, after{};
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &before);
    f.renderer.set_sampling(a, linear);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &after);
    CHECK(before == after);
    auto invalid = linear;
    invalid.min = static_cast<TextureFilter>(99);
    rejects([&] { f.renderer.set_sampling(a, invalid); });
    rejects([&] { f.renderer.upload(colors, invalid); });
    CHECK(f.renderer.sampling(a) == linear && f.renderer.live_textures() == 2);
    f.renderer.begin(f.target, {{}, 2});
    f.renderer.sprite(a, Transform::from({-0.5F, 0}, 0, {1, 2}), {}, {{0.5F, 0.5F}, {0.5F, 0.5F}});
    rejects([&] { f.renderer.set_sampling(a, {}); });
    f.renderer.sprite(a, Transform::from({0.5F, 0}, 0, {1, 2}), {}, {{0.5F, 0.5F}, {0.5F, 0.5F}});
    rejects([&] { f.renderer.sprite(a, {}, {}, {{0, 0}, {Renderer::max_texture_uv + 1, 1}}); });
    rejects([&] {
        f.renderer.sprite(a, {}, {}, {{0, 0}, {std::numeric_limits<float>::infinity(), 1}});
    });
    f.renderer.end();
    CHECK(f.renderer.stats().quads == 2 && f.renderer.stats().draws == 1);
    CHECK(f.renderer.pixel(16, 32) == f.renderer.pixel(48, 32));
    CHECK(f.renderer.pixel(16, 32)[0] >= 186 && f.renderer.pixel(16, 32)[0] <= 190);
    f.renderer.release(a);
    rejects([&] { f.renderer.sampling(a); });
    rejects([&] { f.renderer.set_sampling(a, {}); });
    const auto recycled = f.renderer.upload(colors);
    CHECK(f.renderer.sampling(recycled) == TextureSampling{});
    CHECK(f.renderer.healthy());
}
TEST(mixed_sampling_keeps_painter_order_and_linear_alpha) {
    Fixture f;
    const auto smooth = f.renderer.upload(colors, linear), sharp = f.renderer.upload(colors);
    const Rect center{{0.5F, 0.5F}, {0.5F, 0.5F}};
    f.renderer.begin(f.target, {{}, 2}, {0, 0, 0, 1});
    f.renderer.sprite(smooth, Transform::from({}, 0, {2, 2}), {}, center);
    f.renderer.sprite(sharp, Transform::from({0.5F, 0}, 0, {1, 2}), {}, center);
    f.renderer.sprite(smooth, Transform::from({}, 0, {0.3F, 0.3F}), {}, center);
    f.renderer.end();
    CHECK(f.renderer.stats().draws == 3 && f.renderer.stats().quads == 3);
    CHECK(f.renderer.pixel(8, 32)[0] >= 186 && f.renderer.pixel(8, 32)[0] <= 190);
    CHECK(f.renderer.pixel(56, 32)[0] < 3 && f.renderer.pixel(56, 32)[2] > 250);
    CHECK(f.renderer.pixel(32, 32)[0] >= 186 && f.renderer.pixel(32, 32)[0] <= 190);
    const auto alpha = f.renderer.upload({2, 1, {255, 0, 0, 255, 255, 0, 0, 0}}, linear);
    const auto pixel = f.sample(alpha, {0.5F, 0.5F});
    CHECK(pixel[0] >= 186 && pixel[0] <= 190 && pixel[1] < 3 && pixel[2] < 3);
    CHECK(f.renderer.healthy());
}
TEST(reload_preserves_sampling_and_failed_transaction_keeps_pixels) {
    Fixture f;
    const TextureSampling options{TextureFilter::linear, TextureFilter::linear, TextureWrap::repeat,
                                  TextureWrap::clamp};
    const auto a = f.renderer.upload(colors, options), b = f.renderer.upload(colors);
    const TextureData resized{
        4, 1, {255, 0, 0, 255, 255, 0, 0, 255, 0, 0, 255, 255, 0, 0, 255, 255}};
    const auto before = f.sample(a, {0, 0.5F});
    const auto bytes = f.renderer.texture_bytes();
    const TextureData invalid{1, 1, {}};
    rejects([&] {
        f.renderer.replace_textures(
            std::array{TextureReplacement{a, resized}, TextureReplacement{b, invalid}});
    });
    CHECK(f.renderer.sampling(a) == options && f.renderer.texture_bytes() == bytes);
    CHECK(f.sample(a, {0, 0.5F}) == before);
    for (int i = 0; i < 8; ++i) {
        f.renderer.replace_textures(std::array{TextureReplacement{a, resized}});
        CHECK(f.renderer.sampling(a) == options && f.renderer.live_textures() == 2);
        CHECK(f.sample(a, {0, 0.5F}) == before);
        GLint value{};
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &value);
        CHECK(value == GL_REPEAT);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &value);
        CHECK(value == GL_CLAMP_TO_EDGE);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &value);
        CHECK(value == GL_LINEAR);
    }
    f.renderer.release(a);
    f.renderer.release(b);
    CHECK(f.renderer.texture_bytes() == 0 && f.renderer.healthy());
}
TEST(gallery_draws_without_resource_growth_and_constructor_failure_cleans_up) {
    Fixture f;
    const auto base = f.renderer.live_textures();
    feature_lab::SamplingGallery gallery(f.renderer);
    f.renderer.resize_target(f.target, 600, 600);
    CHECK(f.renderer.live_textures() == base + 4 && f.renderer.texture_bytes() == 64);
    for (int i = 0; i < 8; ++i) {
        f.renderer.begin(f.target, {{-20.8F, -4.1F}, 6}, {0, 0, 0, 1});
        gallery.draw();
        f.renderer.end();
        CHECK(f.renderer.live_textures() == base + 4 && f.renderer.texture_bytes() == 64);
        CHECK(f.renderer.stats().quads > 4 && f.renderer.healthy());
        CHECK(f.renderer.stats().draws == 5);
        const Camera camera{{-20.8F, -4.1F}, 6};
        const auto at = [&](Vec2 world) {
            const auto screen = camera.world_to_screen(world, 600, 600);
            return f.renderer.pixel(static_cast<int>(std::round(screen.x)),
                                    600 - static_cast<int>(std::round(screen.y)));
        };
        const auto sharp = at({-22, -3}), smooth = at({-19.6F, -3});
        CHECK(sharp[0] < 3 && sharp[1] >= 218 && sharp[2] >= 208);
        CHECK(smooth[0] > 175 && smooth[0] < 200 && smooth[2] > 145 && smooth[2] < 165);
        CHECK(at({-22.7F, -3})[0] > 250);  // Clamp extends the orange border.
        CHECK(at({-22.7F, -5.2F})[0] < 3); // Repeat wraps to cyan.
    }
    while (f.renderer.live_textures() < Renderer::max_textures - 2)
        f.renderer.upload(colors);
    const auto count = f.renderer.live_textures(), bytes = f.renderer.texture_bytes();
    rejects([&] { feature_lab::SamplingGallery overflow(f.renderer); });
    CHECK(f.renderer.live_textures() == count && f.renderer.texture_bytes() == bytes);
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY")) {
        std::cout << "SKIP: DISPLAY unavailable\n";
        return 77;
    }
    return testing::run_tests();
}
