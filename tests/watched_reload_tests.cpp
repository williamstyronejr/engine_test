#include "../examples/feature_lab/asset_reload.hpp"
#include "engine/window.hpp"
#include "test.hpp"
#include <cstdlib>
#include <filesystem>
#include <unistd.h>

namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
using namespace std::chrono_literals;
struct Files {
    std::filesystem::path path;
    Files() {
        std::string pattern =
            (std::filesystem::temp_directory_path() / "watched-reload-XXXXXX").string();
        CHECK(mkdtemp(pattern.data()));
        path = pattern;
        std::filesystem::create_directories(path / "shaders");
        std::filesystem::create_directories(path / "textures");
        std::filesystem::create_directories(path / "fonts");
        AssetRoot root(path);
        root.write_atomic("fonts/ui.efnt", encode_font(builtin_font()));
        root.write_text("shaders/sprite.vert", Renderer::default_vertex_shader());
        root.write_text("shaders/sprite.frag", Renderer::default_fragment_shader());
        root.write_atomic("textures/test.etex", encode_texture({1, 1, {255, 0, 0, 255}}));
    }
    ~Files() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
TEST(watched_edits_apply_pixels_retain_failures_and_recover_without_resource_growth) {
    Files files;
    AssetRoot root(files.path);
    Window window(64, 64, "Watched reload", false);
    Renderer renderer;
    TextureAssets textures(root);
    textures.load(renderer, "textures/test.etex", 1, 1);
    ShaderTools shaders;
    FontAssets fonts;
    shaders.reload(renderer, root);
    AssetReload reload(root, textures);
    const auto handle = textures.at("textures/test.etex").gpu;
    const auto target = renderer.create_target(32, 32);
    const auto draw = [&] {
        renderer.begin(target, {{0, 0}, 2});
        renderer.sprite(handle, Transform::from({}, 0, {2, 2}));
        renderer.end();
        return renderer.pixel(16, 16);
    };
    auto time = AssetWatcher::Time{};
    const auto tick = [&] { reload.update(renderer, root, shaders, textures, fonts, 0, time); };
    const auto settle = [&] {
        time += 200ms;
        tick();
        time += 150ms;
        tick();
    };
    tick();
    time += 150ms;
    tick();
    CHECK(reload.shader_reloads() == 1 && reload.texture_reloads() == 1);
    CHECK(draw()[0] > 250);
    auto changed_shader = std::string(Renderer::default_fragment_shader());
    const auto expression = changed_shader.find("texture(atlas,uv)*color");
    CHECK(expression != std::string::npos);
    changed_shader.replace(expression, std::string("texture(atlas,uv)*color").size(),
                           "texture(atlas,uv)*color*vec4(0.25,1.0,1.0,1.0)");
    root.write_text("shaders/sprite.frag", changed_shader);
    settle();
    const auto tinted = draw();
    CHECK(tinted[0] > 120 && tinted[0] < 145 && tinted[1] < 3);
    const auto shader_revision = renderer.shader_revision();
    root.write_atomic("textures/test.etex", encode_texture({1, 1, {0, 255, 0, 255}}));
    settle();
    CHECK(draw()[1] > 250 && renderer.shader_revision() == shader_revision);
    const auto snapshot = textures.at("textures/test.etex").data;
    const auto revision = textures.revision();
    root.write_text("textures/test.etex", "invalid texture");
    root.write_text("shaders/sprite.frag", "#version 460 core\n#error watched failure\n");
    settle();
    CHECK(!textures.applied() && !shaders.result().applied);
    CHECK(textures.revision() == revision && renderer.shader_revision() == shader_revision);
    CHECK(textures.at("textures/test.etex").data == snapshot && draw()[1] > 250);
    const auto shader_attempts = reload.shader_reloads(),
               texture_attempts = reload.texture_reloads();
    time += 2s;
    tick();
    CHECK(reload.shader_reloads() == shader_attempts &&
          reload.texture_reloads() == texture_attempts);
    root.write_text("shaders/sprite.frag", Renderer::default_fragment_shader());
    root.write_atomic("textures/test.etex", encode_texture({1, 1, {0, 0, 255, 255}}));
    settle();
    CHECK(shaders.result().applied && textures.applied() && draw()[2] > 250);
    for (int cycle = 0; cycle < 5; ++cycle) {
        root.write_atomic("textures/test.etex", encode_texture({1, 1, {255, 0, 0, 255}}));
        settle();
        CHECK(draw()[0] > 250);
        CHECK(textures.at("textures/test.etex").gpu.slot == handle.slot &&
              textures.at("textures/test.etex").gpu.serial == handle.serial);
        CHECK(renderer.live_textures() == 1 && renderer.live_targets() == 1);
    }
    CHECK(renderer.healthy());
}
TEST(manual_reload_wins_over_a_ready_automatic_group) {
    Files files;
    AssetRoot root(files.path);
    Window window(64, 64, "Watch manual priority", false);
    Renderer renderer;
    TextureAssets textures(root);
    textures.load(renderer, "textures/test.etex", 1, 1);
    ShaderTools shaders;
    FontAssets fonts;
    AssetReload reload(root, textures);
    reload.update(renderer, root, shaders, textures, fonts, 0, AssetWatcher::Time{});
    InputFrame input{};
    input[static_cast<std::size_t>(Key::reload_shaders)].pressed = true;
    CHECK(shaders.update(renderer, root, input));
    const auto revision = renderer.shader_revision();
    reload.update(renderer, root, shaders, textures, fonts, AssetReload::shader_group,
                  AssetWatcher::Time{} + 150ms);
    CHECK(renderer.shader_revision() == revision && reload.shader_reloads() == 0);
    CHECK(reload.texture_reloads() == 1);
    reload.update(renderer, root, shaders, textures, fonts, 0, AssetWatcher::Time{} + 2s);
    CHECK(reload.shader_reloads() == 0 && renderer.healthy());
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY"))
        return 77;
    return testing::run_tests();
}
