#include "../examples/feature_lab/asset_reload.hpp"
#include "../examples/feature_lab/overlays.hpp"
#include "engine/window.hpp"
#include "scene_fixture.hpp"
#include <cstdlib>

namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
using namespace std::chrono_literals;
struct Fixture {
    SceneFiles files;
    Game game = load_game(files.root);
    Window window{64, 64, "Scene reload", false};
    Renderer renderer;
    Overlays overlays{renderer, game};
    TextureAssets textures{files.root};
    ShaderTools shaders;
    FontAssets fonts;
    SceneAssets scenes;
    bool reject_presentation{};
    RenderTargetHandle target = renderer.create_target(32, 32);
    Fixture() {
        textures.load(renderer, game.map.data().texture, 6, 1);
        textures.load(renderer, game.animations->texture, game.animations->atlas_columns,
                      game.animations->atlas_rows);
        game.world.each([&](const SceneNode& node) {
            if (node.sprite)
                textures.load(renderer, node.sprite->texture, 1, 1);
        });
    }
    bool reload(std::string_view key) {
        if (key != SceneAssets::key(game.level))
            return false;
        std::vector<Rect> staged;
        if (scenes.reload(game, files.root, [&](const Game& candidate) {
                textures.validate_level(candidate);
                staged = Overlays::stage(candidate);
                if (reject_presentation)
                    throw std::runtime_error("Minimap staging failure");
            }))
            overlays.reload(std::move(staged));
        return true;
    }
    auto pixel() {
        renderer.begin(target, {{-24.5F, -9.5F}, 2}, {0, 0, 0, 1});
        for (auto e : game.draw_order) {
            if (!game.world.valid(e))
                continue;
            const auto& node = game.world.get(e);
            const auto& s = *node.sprite;
            renderer.sprite(textures.at(s.texture).gpu,
                            game.render_transform(e, 1) * Transform::from({}, 0, s.size),
                            {s.r, s.g, s.b, s.a});
        }
        renderer.end();
        return renderer.pixel(16, 16);
    }
    auto minimap() {
        overlays.render_minimap(renderer, game, 1280, 720, 1);
        return renderer.pixel(32, 28);
    }
};
TEST(scene_watch_commits_pixels_collision_and_minimap_and_retains_all_on_failure) {
    Fixture f;
    AssetReload watch(f.files.root, f.textures, {}, {}, [&](auto key) { return f.reload(key); });
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
    CHECK(watch.scene_reloads() == 0);
    now += 1ms;
    tick();
    CHECK(watch.scene_reloads() == 1 && f.scenes.revision() == 2);
    const auto original = f.files.text();
    const auto before = f.pixel(), old_map = f.minimap();
    const auto edited = with_wall(original);
    f.game.paused = true;
    auto player = f.game.player;
    f.files.write(edited);
    settle();
    CHECK(f.scenes.applied() && f.scenes.revision() == 3 && !f.game.world.valid(player));
    CHECK(f.pixel()[1] == 255 && f.pixel()[0] == 0 && f.pixel() != before);
    const auto new_map = f.minimap();
    CHECK(new_map[2] > old_map[2] + 20);
    CHECK(f.game.collisions.segment({-26, -9.5F}, {-24, -9.5F}, {8, 0, false}));
    CHECK(f.game.paused && f.game.ticks == 0);
    player = f.game.player;
    const auto content = f.game.content_id();
    for (int mode = 0; mode < 4; ++mode) {
        f.reject_presentation = mode == 3;
        if (mode == 0)
            f.files.write("invalid scene");
        if (mode == 1)
            std::filesystem::remove(f.files.path / "facility.scene");
        if (mode == 2)
            f.files.write(with_wall(original, {-24.5F, -9.5F}, "textures/new.etex"));
        if (mode == 3)
            f.files.write(original);
        settle();
        CHECK(!f.scenes.applied() && f.scenes.revision() == 3 && f.game.world.valid(player));
        CHECK(f.game.content_id() == content && f.pixel()[1] == 255 && f.minimap() == new_map);
    }
    f.reject_presentation = false;
    const auto texture_bytes = f.renderer.texture_bytes(), target_bytes = f.renderer.target_bytes();
    for (int i = 0; i < 12; ++i) {
        f.files.write(i % 2 ? edited : original);
        settle();
        CHECK(f.scenes.applied() && f.scenes.diagnostic().empty());
        CHECK(f.pixel() == (i % 2 ? std::array<std::uint8_t, 4>{0, 255, 0, 255} : before));
        CHECK(f.minimap() == (i % 2 ? new_map : old_map));
        CHECK(f.renderer.texture_bytes() == texture_bytes &&
              f.renderer.target_bytes() == target_bytes);
        CHECK(f.renderer.live_textures() == 4 && f.renderer.live_targets() == 2);
    }
    CHECK(watch.tilemap_reloads() == 0 && watch.animation_reloads() == 0);
    CHECK(watch.shader_reloads() == 1 && watch.texture_reloads() == 1 && f.renderer.healthy());
}
TEST(scene_groups_follow_active_level_and_honor_manual_mask) {
    Fixture f;
    AssetReload watch(f.files.root, f.textures, {}, {}, [&](auto key) { return f.reload(key); });
    auto now = AssetWatcher::Time{};
    const auto tick = [&](std::uint32_t mask = 0) {
        watch.update(f.renderer, f.files.root, f.shaders, f.textures, f.fonts, mask, now);
    };
    const auto settle = [&] {
        now += 1s;
        tick();
        now += 150ms;
        tick();
    };
    tick();
    now += 150ms;
    tick(AssetReload::facility_scene_group);
    CHECK(watch.scene_reloads() == 0);
    const auto annex = f.files.text(Level::annex);
    f.files.write("invalid inactive", Level::annex);
    settle();
    CHECK(watch.scene_reloads() == 0 && f.scenes.applied());
    f.files.write(annex, Level::annex);
    settle();
    f.game.replace(load_game(f.files.root, Level::annex));
    f.overlays.replaced(Overlays::stage(f.game));
    f.scenes.replaced();
    f.files.write(annex + "\n", Level::annex);
    settle();
    CHECK(watch.scene_reloads() == 1 && f.scenes.revision() == 2);
    CHECK(f.game.content_id() == load_game(f.files.root, Level::annex).content_id());
    f.files.write("invalid inactive facility");
    settle();
    CHECK(watch.scene_reloads() == 1 && f.scenes.applied());
}
TEST(recording_playback_watch_omits_scene_files) {
    Fixture f;
    AssetReload watch(f.files.root, f.textures);
    auto now = AssetWatcher::Time{};
    const auto tick = [&] {
        watch.update(f.renderer, f.files.root, f.shaders, f.textures, f.fonts, 0, now);
    };
    tick();
    now += 150ms;
    tick();
    const auto content = f.game.content_id();
    const auto player = f.game.player;
    for (auto level : {Level::facility, Level::annex})
        f.files.write("invalid during replay", level);
    now += 1s;
    tick();
    now += 150ms;
    tick();
    CHECK(watch.scene_reloads() == 0 && f.game.content_id() == content &&
          f.game.world.valid(player));
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY"))
        return 77;
    return testing::run_tests();
}
