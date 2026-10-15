#include "../examples/feature_lab/asset_reload.hpp"
#include "animation_fixture.hpp"
#include "engine/window.hpp"
#include <cstdlib>

namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
using namespace std::chrono_literals;
struct Fixture {
    AnimationFiles files;
    Game game = load_game(files.root);
    Window window{64, 64, "Animation reload", false};
    Renderer renderer;
    TextureAssets textures{files.root};
    ShaderTools shaders;
    FontAssets fonts;
    AnimationAssets animations;
    RenderTargetHandle target = renderer.create_target(32, 32);
    Fixture() {
        TextureData image{8, 3, std::vector<std::uint8_t>(8 * 3 * 4, 0)};
        for (std::size_t i = 0; i < 24; ++i) {
            image.rgba[i * 4 + (i == 0 ? 0 : 1)] = 255;
            image.rgba[i * 4 + 3] = 255;
        }
        files.root.write_atomic(game.animations->texture, encode_texture(image));
        textures.load(renderer, game.animations->texture, 8, 3);
    }
    auto pixel() {
        renderer.begin(target, {{0, 0}, 2}, {0, 0, 0, 1});
        renderer.sprite(textures.at(game.animations->texture).gpu, Transform::from({}, 0, {2, 2}),
                        {}, game.player_animation.uv());
        renderer.end();
        return renderer.pixel(16, 16);
    }
};
TEST(watched_animation_edits_debounce_publish_pixels_and_retain_failures) {
    Fixture f;
    AssetReload watch(f.files.root, f.textures, [&] { f.animations.reload(f.game, f.files.root); });
    auto now = AssetWatcher::Time{};
    const auto tick = [&] {
        watch.update(f.renderer, f.files.root, f.shaders, f.textures, f.fonts, 0, now);
    };
    const auto settle = [&] {
        now += 200ms;
        tick();
        now += 150ms;
        tick();
    };
    tick();
    now += 149ms;
    tick();
    CHECK(watch.animation_reloads() == 0);
    now += 1ms;
    tick();
    CHECK(watch.animation_reloads() == 1 && f.animations.revision() == 2);
    CHECK(f.pixel()[0] == 255);
    f.game.paused = true;
    f.game.player_animation.advance(30);
    auto changed = *f.game.animations;
    clip(changed, "idle").frames = {{1, 120}};
    f.files.write(changed);
    settle();
    CHECK(f.pixel()[1] == 255 && f.pixel()[0] == 0);
    CHECK(f.game.paused && f.game.ticks == 0 && f.game.player_animation.position() == 60);
    CHECK(f.animations.revision() == 3 && watch.animation_reloads() == 2);
    const auto good = f.game.animations;
    const auto bytes = f.renderer.texture_bytes(), font_bytes = f.renderer.font_bytes();
    f.files.root.write_text(AnimationAssets::key, "broken");
    settle();
    CHECK(!f.animations.applied() && f.animations.revision() == 3);
    CHECK(f.game.animations == good && f.pixel()[1] == 255);
    now += 2s;
    tick();
    CHECK(watch.animation_reloads() == 3); // Unchanged failures are not retried.
    std::filesystem::remove(f.files.path / AnimationAssets::key);
    settle();
    CHECK(!f.animations.applied() && f.game.animations == good);
    for (int i = 0; i < 12; ++i) {
        clip(changed, "idle").frames[0].atlas_cell = static_cast<std::uint32_t>(i % 2);
        f.files.write(changed);
        settle();
        CHECK(f.animations.applied() && f.animations.diagnostic().empty());
        CHECK(f.pixel()[static_cast<std::size_t>(i % 2)] == 255);
        CHECK(f.renderer.live_textures() == 1 && f.renderer.live_targets() == 1);
        CHECK(f.renderer.texture_bytes() == bytes && f.renderer.font_bytes() == font_bytes);
    }
    CHECK(watch.shader_reloads() == 1 && watch.texture_reloads() == 1 && watch.font_reloads() == 1);
    CHECK(f.renderer.healthy());
}
TEST(animation_group_manual_priority_and_omission_for_fixed_content_sessions) {
    Fixture f;
    auto now = AssetWatcher::Time{};
    {
        AssetReload watch(f.files.root, f.textures,
                          [&] { f.animations.reload(f.game, f.files.root); });
        watch.update(f.renderer, f.files.root, f.shaders, f.textures, f.fonts, 0, now);
        now += 150ms;
        watch.update(f.renderer, f.files.root, f.shaders, f.textures, f.fonts,
                     AssetReload::animation_group, now);
        CHECK(watch.animation_reloads() == 0 && f.animations.revision() == 1);
        CHECK(watch.shader_reloads() == 1 && watch.font_reloads() == 1);
    }
    AssetReload locked(f.files.root, f.textures); // No animation callback or watched EANI key.
    now += 1s;
    locked.update(f.renderer, f.files.root, f.shaders, f.textures, f.fonts, 0, now);
    now += 150ms;
    locked.update(f.renderer, f.files.root, f.shaders, f.textures, f.fonts, 0, now);
    const auto original = f.game.animations;
    f.files.root.write_text(AnimationAssets::key, "invalid during replay");
    now += 1s;
    locked.update(f.renderer, f.files.root, f.shaders, f.textures, f.fonts, 0, now);
    now += 150ms;
    locked.update(f.renderer, f.files.root, f.shaders, f.textures, f.fonts, 0, now);
    CHECK(locked.animation_reloads() == 0 && f.game.animations == original);
    CHECK(locked.shader_reloads() == 1 && locked.texture_reloads() == 1 &&
          locked.font_reloads() == 1);
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY"))
        return 77;
    return testing::run_tests();
}
