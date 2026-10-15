#include "../examples/feature_lab/asset_reload.hpp"
#include "../examples/feature_lab/overlays.hpp"
#include "engine/window.hpp"
#include "tilemap_fixture.hpp"
#include <cstdlib>

namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
using namespace std::chrono_literals;
struct Fixture {
    TilemapFiles files;
    Game game = load_game(files.root);
    Window window{64, 64, "Tilemap reload", false};
    Renderer renderer;
    Overlays overlays{renderer, game};
    TextureAssets textures{files.root};
    ShaderTools shaders;
    FontAssets fonts;
    TilemapAssets tilemaps;
    bool reject_presentation{};
    RenderTargetHandle target = renderer.create_target(32, 32);
    Fixture() {
        TextureData image{6, 1, std::vector<std::uint8_t>(24, 0)};
        for (std::size_t i = 0; i < 6; ++i) {
            image.rgba[i * 4 + (i == 2 ? 1 : 0)] = 255;
            image.rgba[i * 4 + 3] = 255;
        }
        files.root.write_atomic(game.map.data().texture, encode_texture(image));
        textures.load(renderer, game.map.data().texture, 6, 1);
    }
    bool reload(std::string_view key) {
        if (key != TilemapAssets::key(game.level))
            return false;
        std::vector<Rect> staged;
        if (tilemaps.reload(game, files.root, [&](const TileMap& candidate) {
                staged = Overlays::stage(candidate, game.walls);
                if (reject_presentation)
                    throw std::runtime_error("Minimap staging failure");
            }))
            overlays.reload(std::move(staged));
        return true;
    }
    auto pixel() {
        renderer.begin(target, {{-24.5F, -9.5F}, 2}, {0, 0, 0, 1});
        for (std::size_t layer = 0; layer < game.map.data().layers.size(); ++layer)
            game.map.visit_visible(
                layer, {{-25.5F, -10.5F}, {-23.5F, -8.5F}}, [&](TileInstance tile) {
                    renderer.sprite(textures.at(game.map.data().texture).gpu,
                                    Transform::from(tile.center, 0, {1, 1}), {}, tile.uv);
                });
        renderer.end();
        return renderer.pixel(16, 16);
    }
    auto minimap() {
        overlays.render_minimap(renderer, game, 1280, 720, 1);
        return renderer.pixel(32, 28); // Center of the edited cell, away from marker/outline.
    }
};
TEST(watched_tilemap_publishes_render_collision_and_minimap_together) {
    Fixture f;
    AssetReload watch(f.files.root, f.textures, {}, [&](auto key) { return f.reload(key); });
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
    CHECK(watch.tilemap_reloads() == 0);
    now += 1ms;
    tick();
    CHECK(watch.tilemap_reloads() == 1 && f.tilemaps.revision() == 2);
    const auto original = f.game.map.data();
    CHECK(f.pixel()[0] == 255);
    const auto old_map_pixel = f.minimap();
    f.game.paused = true;
    auto edited = original;
    put(edited, {-25, -10}, 3);
    f.files.write(edited);
    settle();
    CHECK(f.pixel()[1] == 255 && f.pixel()[0] == 0);
    const auto new_map_pixel = f.minimap();
    CHECK(new_map_pixel[2] > old_map_pixel[2] + 20);
    CHECK(f.game.collisions.segment({-26, -9.5F}, {-24, -9.5F}, {8, 0, false}));
    CHECK(f.game.paused && f.game.ticks == 0 && f.tilemaps.revision() == 3);
    const auto bytes = f.renderer.texture_bytes(), targets = f.renderer.target_bytes();
    const auto collision_bytes = f.game.collisions.buffer_bytes();
    f.files.root.write_text("facility.etmp", "invalid");
    settle();
    CHECK(!f.tilemaps.applied() && f.tilemaps.revision() == 3);
    CHECK(f.pixel()[1] == 255 && f.minimap() == new_map_pixel);
    now += 2s;
    tick();
    CHECK(watch.tilemap_reloads() == 3);
    // A dependent presentation failure must also retain the complete old bank.
    f.reject_presentation = true;
    f.files.write(original);
    settle();
    CHECK(!f.tilemaps.applied() && f.tilemaps.revision() == 3);
    CHECK(f.pixel()[1] == 255 && f.minimap() == new_map_pixel);
    CHECK(f.game.collisions.segment({-26, -9.5F}, {-24, -9.5F}, {8, 0, false}));
    f.reject_presentation = false;
    for (int i = 0; i < 12; ++i) {
        f.files.write(i % 2 ? edited : original);
        settle();
        CHECK(f.tilemaps.applied() && f.tilemaps.diagnostic().empty());
        CHECK(f.pixel()[static_cast<std::size_t>(i % 2)] == 255);
        CHECK(f.minimap() == (i % 2 ? new_map_pixel : old_map_pixel));
        CHECK(f.renderer.live_textures() == 1 && f.renderer.live_targets() == 2);
        CHECK(f.renderer.texture_bytes() == bytes && f.renderer.target_bytes() == targets);
        CHECK(f.game.collisions.buffer_bytes() == collision_bytes);
    }
    CHECK(watch.shader_reloads() == 1 && watch.texture_reloads() == 1 && watch.font_reloads() == 1);
    CHECK(f.renderer.healthy());
}
TEST(level_watch_groups_ignore_inactive_edits_follow_replacement_and_honor_manual_mask) {
    Fixture f;
    auto now = AssetWatcher::Time{};
    AssetReload watch(f.files.root, f.textures, {}, [&](auto key) { return f.reload(key); });
    const auto tick = [&](std::uint32_t mask = 0) {
        watch.update(f.renderer, f.files.root, f.shaders, f.textures, f.fonts, mask, now);
    };
    tick();
    now += 150ms;
    tick(AssetReload::facility_map_group);
    CHECK(watch.tilemap_reloads() == 0 && f.tilemaps.revision() == 1);
    const auto annex = decode_tilemap(f.files.root.read("annex.etmp", 4 * 1024 * 1024));
    f.files.root.write_text("annex.etmp", "inactive invalid file");
    now += 1s;
    tick();
    now += 150ms;
    tick();
    CHECK(watch.tilemap_reloads() == 0 && f.tilemaps.applied());
    f.files.write(annex, Level::annex);
    now += 1s;
    tick();
    now += 150ms;
    tick();
    f.game.replace(load_game(f.files.root, Level::annex));
    f.overlays.replaced(Overlays::stage(f.game));
    f.tilemaps.replaced();
    auto edited = annex;
    edited.palette[1].atlas_cell = 1;
    f.files.write(edited, Level::annex);
    now += 1s;
    tick();
    now += 150ms;
    tick();
    CHECK(watch.tilemap_reloads() == 1 && f.tilemaps.revision() == 2);
    CHECK(f.game.content_id()[1] == crc32(encode_tilemap(edited)));
    f.files.root.write_text("facility.etmp", "now inactive");
    now += 1s;
    tick();
    now += 150ms;
    tick();
    CHECK(watch.tilemap_reloads() == 1 && f.tilemaps.applied());
}
TEST(fixed_content_watch_omits_both_tilemaps) {
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
    for (auto key : {"facility.etmp", "annex.etmp"})
        f.files.root.write_text(key, "invalid during replay");
    now += 1s;
    tick();
    now += 150ms;
    tick();
    CHECK(watch.tilemap_reloads() == 0 && f.game.content_id() == content);
    CHECK(watch.shader_reloads() == 1 && watch.font_reloads() == 1);
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY"))
        return 77;
    return testing::run_tests();
}
