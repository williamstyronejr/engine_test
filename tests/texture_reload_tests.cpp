#include "../examples/feature_lab/settings.hpp"
#include "../examples/feature_lab/texture_assets.hpp"
#include "engine/window.hpp"
#include "test.hpp"
#include <GL/gl.h>
#include <cstdlib>
#include <filesystem>

namespace {
using namespace testing;
using namespace engine;
const TextureData red{1, 1, {255, 0, 0, 255}}, green{1, 1, {0, 255, 0, 255}},
    blue{1, 1, {0, 0, 255, 255}}, invalid{1, 1, {}};
struct Fixture {
    engine::Window window{64, 64, "Texture reload tests", false};
    Renderer renderer;
    RenderTargetHandle target = renderer.create_target(64, 64);
    void draw(TextureHandle a, TextureHandle b) {
        renderer.begin(target, {{0, 0}, 2}, {0, 0, 0, 1});
        renderer.sprite(a, Transform::from({-0.5F, 0}, 0, {1, 2}));
        renderer.sprite(b, Transform::from({0.5F, 0}, 0, {1, 2}));
        renderer.end();
    }
    auto pixels() { return std::array{renderer.pixel(16, 32), renderer.pixel(48, 32)}; }
};
struct Files {
    std::filesystem::path path;
    Files() {
        std::string pattern =
            (std::filesystem::temp_directory_path() / "engine-textures-XXXXXX").string();
        if (!mkdtemp(pattern.data()))
            throw std::runtime_error("Cannot create texture test directory");
        path = pattern;
    }
    ~Files() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
TEST(replacement_preserves_shared_handles_and_completed_pixels) {
    Fixture f;
    const auto a = f.renderer.upload(red), b = f.renderer.upload(blue);
    f.draw(a, a);
    const auto before = f.pixels();
    const TextureData resized{2, 1, {0, 255, 0, 255, 0, 255, 0, 255}};
    f.renderer.replace_textures(
        std::array{TextureReplacement{a, resized}, TextureReplacement{b, red}});
    CHECK(f.pixels() == before); // A completed render target is not redrawn by a reload.
    f.draw(a, a);
    CHECK(f.pixels()[0][1] > 250 && f.pixels()[1][1] > 250);
    f.draw(a, b);
    CHECK(f.pixels()[0][1] > 250 && f.pixels()[1][0] > 250);
    CHECK(f.renderer.live_textures() == 2 && f.renderer.healthy());
}
TEST(invalid_batches_preserve_all_textures_and_pending_geometry) {
    Fixture f;
    const auto a = f.renderer.upload(red), b = f.renderer.upload(blue);
    f.draw(a, b);
    const auto before = f.pixels();
    rejects([&] {
        f.renderer.replace_textures(
            std::array{TextureReplacement{a, green}, TextureReplacement{b, invalid}});
    });
    rejects([&] {
        f.renderer.replace_textures(
            std::array{TextureReplacement{a, green}, TextureReplacement{a, blue}});
    });
    rejects([&] { f.renderer.replace_textures(std::array{TextureReplacement{{}, green}}); });
    rejects([&] {
        f.renderer.replace_textures(
            std::array{TextureReplacement{{a.slot, a.serial + 1000}, green}});
    });
    const auto stale = f.renderer.upload(red);
    f.renderer.release(stale);
    rejects([&] {
        f.renderer.replace_textures(
            std::array{TextureReplacement{a, green}, TextureReplacement{stale, blue}});
    });
    std::vector<TextureReplacement> too_many;
    for (std::size_t i = 0; i <= Renderer::max_textures; ++i)
        too_many.push_back({a, green});
    rejects([&] { f.renderer.replace_textures(too_many); });
    f.renderer.replace_textures({});
    f.draw(a, b);
    CHECK(f.pixels() == before);
    f.renderer.begin(f.target, {{0, 0}, 2});
    f.renderer.sprite(a, Transform::from({}, 0, {2, 2}));
    rejects([&] { f.renderer.replace_textures(std::array{TextureReplacement{a, green}}); });
    rejects([&] { f.renderer.replace_textures({}); });
    f.renderer.end();
    CHECK(f.renderer.pixel(32, 32)[0] > 250 && f.renderer.stats().quads == 1);
    CHECK(f.renderer.live_textures() == 2 && f.renderer.healthy());
}
TEST(full_handle_capacity_and_repeated_reload_retire_gpu_objects) {
    Fixture f;
    std::vector<TextureHandle> handles;
    std::vector<TextureReplacement> replacements;
    for (std::size_t i = 0; i < Renderer::max_textures; ++i) {
        handles.push_back(f.renderer.upload(red));
        replacements.push_back({handles.back(), green});
    }
    rejects([&] { f.renderer.upload(red); });
    for (int iteration = 0; iteration < 16; ++iteration) {
        f.draw(handles.front(), handles.back());
        GLint old{};
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &old);
        CHECK(old != 0 && glIsTexture(static_cast<GLuint>(old)));
        f.renderer.replace_textures(replacements);
        CHECK(!glIsTexture(static_cast<GLuint>(old)));
        CHECK(f.renderer.live_textures() == Renderer::max_textures);
        f.draw(handles.front(), handles.back());
        CHECK(f.pixels()[0][1] > 250 && f.pixels()[1][1] > 250);
    }
    for (auto handle : handles)
        f.renderer.release(handle);
    CHECK(f.renderer.live_textures() == 0 && f.renderer.healthy());
}
TEST(staging_budget_rejection_is_atomic) {
    Fixture f;
    const TextureData large{2048, 2048, std::vector<std::uint8_t>(16 * 1024 * 1024, 255)};
    std::vector<TextureReplacement> replacements;
    for (int i = 0; i < 5; ++i)
        replacements.push_back({f.renderer.upload(red), large});
    rejects([&] { f.renderer.replace_textures(replacements); });
    f.draw(replacements.front().handle, replacements.back().handle);
    CHECK(f.pixels()[0][0] > 250 && f.pixels()[0][1] < 3);
    CHECK(f.pixels()[1][0] > 250 && f.pixels()[1][1] < 3);
    CHECK(f.renderer.live_textures() == 5 && f.renderer.healthy());
}
TEST(reloaded_texture_keeps_atlas_filter_wrap_and_alpha_semantics) {
    Fixture f;
    const auto handle = f.renderer.upload(red);
    const TextureData atlas{2, 1, {0, 255, 0, 128, 0, 0, 255, 255}};
    f.renderer.replace_textures(std::array{TextureReplacement{handle, atlas}});
    f.renderer.begin(f.target, {{0, 0}, 2}, {0, 0, 0, 1});
    f.renderer.sprite(handle, Transform::from({}, 0, {2, 2}), {}, {{0, 0}, {1, 1}});
    f.renderer.end();
    for (const auto parameter : {GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T}) {
        GLint value{};
        glGetTexParameteriv(GL_TEXTURE_2D, static_cast<GLenum>(parameter), &value);
        CHECK(value == GL_CLAMP_TO_EDGE);
    }
    for (const auto parameter : {GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER}) {
        GLint value{};
        glGetTexParameteriv(GL_TEXTURE_2D, static_cast<GLenum>(parameter), &value);
        CHECK(value == GL_NEAREST);
    }
    const auto left = f.renderer.pixel(0, 32), right = f.renderer.pixel(63, 32);
    CHECK(left[1] > 180 && left[1] < 195 && left[2] < 3);
    CHECK(right[2] > 250 && right[1] < 3);
    CHECK(f.renderer.pixel(31, 32)[2] < 3 && f.renderer.pixel(32, 32)[1] < 3);
    CHECK(f.renderer.healthy());
}
TEST(file_reload_publishes_cpu_and_gpu_together_and_keeps_old_readers) {
    Fixture f;
    Files directory;
    AssetRoot files(directory.path);
    files.write_atomic("a.etex", encode_texture(red));
    files.write_atomic("b.etex", encode_texture(blue));
    feature_lab::TextureAssets textures(files);
    textures.load(f.renderer, "a.etex", 1, 1);
    textures.load(f.renderer, "b.etex", 1, 1);
    const auto a = textures.at("a.etex").gpu, b = textures.at("b.etex").gpu;
    const auto old_a = textures.at("a.etex").data, old_b = textures.at("b.etex").data;
    f.draw(a, b);
    const auto before = f.pixels();
    files.write_atomic("a.etex", encode_texture(green));
    files.write_text("b.etex", "broken");
    CHECK(!textures.reload(f.renderer) && textures.revision() == 1);
    CHECK(textures.at("a.etex").data == old_a && textures.at("b.etex").data == old_b);
    CHECK(!textures.diagnostic().empty());
    f.draw(a, b);
    CHECK(f.pixels() == before);
    files.write_atomic("b.etex", encode_texture(red));
    CHECK(!textures.reload(f.renderer, true) && textures.revision() == 1);
    CHECK(textures.at("a.etex").data == old_a);
    f.draw(a, b);
    CHECK(f.pixels() == before);
    CHECK(textures.reload(f.renderer) && textures.revision() == 2);
    CHECK(textures.diagnostic().empty() && textures.at("a.etex").data != old_a);
    CHECK(textures.at("a.etex").data->rgba == green.rgba && old_a->rgba == red.rgba);
    CHECK(textures.at("b.etex").data->rgba == red.rgba && old_b->rgba == blue.rgba);
    CHECK(textures.at("a.etex").gpu.serial == a.serial && textures.at("a.etex").gpu.slot == a.slot);
    f.draw(a, b);
    CHECK(f.pixels()[0][1] > 250 && f.pixels()[1][0] > 250);
    CHECK(f.renderer.live_textures() == 2 && f.renderer.healthy());
}
TEST(fallback_recovers_and_shared_atlas_constraints_survive_failed_reload) {
    Fixture f;
    Files directory;
    AssetRoot files(directory.path);
    feature_lab::TextureAssets textures(files);
    textures.load(f.renderer, "a.etex", 2, 1);
    textures.load(f.renderer, "a.etex", 3, 1);
    CHECK(textures.at("a.etex").fallback && textures.size() == 1);
    const auto handle = textures.at("a.etex").gpu;
    const TextureData good{6, 1, std::vector<std::uint8_t>(24, 255)};
    files.write_atomic("a.etex", encode_texture(good));
    CHECK(textures.reload(f.renderer) && !textures.at("a.etex").fallback);
    const auto old = textures.at("a.etex").data;
    const TextureData bad_grid{4, 1, std::vector<std::uint8_t>(16, 0)};
    files.write_atomic("a.etex", encode_texture(bad_grid));
    CHECK(!textures.reload(f.renderer) && textures.revision() == 2);
    CHECK(textures.at("a.etex").data == old);
    CHECK(textures.diagnostic().find("a.etex") != std::string::npos);
    rejects([&] { textures.load(f.renderer, "a.etex", 4, 1); });
    files.write_atomic("a.etex", encode_texture(good));
    CHECK(textures.reload(f.renderer) && textures.revision() == 3);
    f.draw(handle, handle);
    CHECK(f.pixels()[0][0] > 250 && f.pixels()[0][1] > 250);
    CHECK(f.renderer.live_textures() == 1 && f.renderer.healthy());
}
TEST(texture_controls_use_press_edges_and_settings_capture) {
    Fixture f;
    Files directory;
    AssetRoot files(directory.path);
    files.write_atomic("a.etex", encode_texture(red));
    feature_lab::TextureAssets textures(files);
    textures.load(f.renderer, "a.etex", 1, 1);
    Input input;
    input.set(Key::reload_textures, true);
    textures.update(f.renderer, input.consume());
    CHECK(textures.revision() == 2);
    input.set(Key::reload_textures, true);
    textures.update(f.renderer, input.consume());
    CHECK(textures.revision() == 2);
    input.set(Key::texture_error, true);
    textures.update(f.renderer, input.consume());
    CHECK(!textures.applied() && textures.revision() == 2);
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    feature_lab::Settings settings;
    input.release_all();
    input.set(Key::settings, true);
    settings.update(game, input.consume(), 640, 480, false);
    input.set(Key::settings, false);
    input.set(Key::reload_textures, true);
    input.set(Key::texture_error, true);
    textures.update(f.renderer, settings.update(game, input.consume(), 640, 480, false).gameplay);
    CHECK(textures.revision() == 2);
    input.set(Key::settings, true);
    textures.update(f.renderer, settings.update(game, input.consume(), 640, 480, false).gameplay);
    CHECK(textures.revision() == 2);
    input.release_all();
    settings.update(game, input.consume(), 640, 480, false);
    input.set(Key::reload_textures, true);
    input.set(Key::texture_error, true);
    textures.update(f.renderer, settings.update(game, input.consume(), 640, 480, false).gameplay);
    CHECK(textures.applied() && textures.revision() == 3 && f.renderer.healthy());
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY")) {
        std::cout << "SKIP: DISPLAY unavailable\n";
        return 77;
    }
    return testing::run_tests();
}
