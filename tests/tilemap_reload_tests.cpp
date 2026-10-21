#include "tilemap_fixture.hpp"

namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
TEST(tilemap_replacement_keeps_the_current_world_budgets) {
    TilemapFiles f;
    auto game = load_game(f.root);
    const auto published = game.collisions.bodies();
    const std::vector<Body2D> bodies(published.begin(), published.end());
    const CollisionWorld::Limits limits{4, 8, 8};
    game.collisions = CollisionWorld(limits);
    game.collisions.prime(bodies);
    const auto bytes = game.collisions.buffer_bytes();
    game.reload_tilemap(game.map.data());
    CHECK(game.collisions.limits() == limits && game.collisions.buffer_bytes() == bytes);
}
TEST(tilemap_commit_preserves_game_and_entities_updates_collision_and_save_identity) {
    TilemapFiles f;
    auto game = load_game(f.root);
    auto state = game.checkpoint();
    state.position = {-27, -9.5F};
    game.restore(state);
    game.update({});
    game.paused = true;
    game.previous.x -= .1F;
    const auto before = game.checkpoint();
    const auto player = game.player, core = game.keys.front();
    const auto original = game.map.data();
    auto edited = original;
    put(edited, {-25, -10}, 3);
    game.reload_tilemap(edited);
    CHECK(game.world.valid(player) && game.world.valid(core) && game.player == player);
    const auto after = game.checkpoint();
    CHECK(after.content[0] == before.content[0] && after.content[2] == before.content[2]);
    CHECK(after.content[1] != before.content[1]);
    CHECK(after.ticks == before.ticks && after.paused && after.animations == before.animations);
    CHECK(after.random_state == before.random_state && after.collected_ids == before.collected_ids);
    CHECK(game.position.x == before.position.x && game.previous.x == game.position.x);
    CHECK(game.collision_candidates.empty());
    CHECK(game.collisions.segment({-26, -9.5F}, {-24, -9.5F}, {8, 0, false}));
    rejects([&] { game.restore(before); });
    f.write(edited);
    auto fresh = load_game(f.root);
    fresh.restore(after);
    CHECK(fresh.content_id() == game.content_id());
    game.paused = false;
    InputFrame right{};
    right[static_cast<std::size_t>(Key::right)].held = true;
    for (int i = 0; i < 40; ++i)
        game.update(right);
    NEAR(game.position.x, -25.3F);
    game.reload_tilemap(original);
    CHECK(!game.collisions.segment({-26, -9.5F}, {-24, -9.5F}, {8, 0, false}));
    for (int i = 0; i < 20; ++i)
        game.update(right);
    CHECK(game.position.x > -25);
}
TEST(player_and_restart_spawn_are_checked_before_any_publication) {
    TilemapFiles f;
    auto game = load_game(f.root);
    auto state = game.checkpoint();
    state.position = {-25, -10};
    game.restore(state);
    const auto before = game.content_id();
    const auto bodies = game.collisions.bodies().size();
    for (auto point : {game.position, game.spawn_position}) {
        auto bad = game.map.data();
        put(bad, point, 3);
        bool prepared = false;
        rejects([&] { game.reload_tilemap(bad, [&](const TileMap&) { prepared = true; }); });
        CHECK(!prepared && game.content_id() == before);
        CHECK(game.collisions.bodies().size() == bodies && game.world.valid(game.player));
    }
    state.position = {-25.3F, -9.5F};
    game.restore(state);
    auto edited = game.map.data();
    put(edited, {-25, -10}, 3);
    game.reload_tilemap(edited); // A swept-movement contact is not penetration.
}
TEST(geometry_atlas_invalid_cells_and_body_capacity_rejections_are_atomic) {
    TilemapFiles f;
    auto game = load_game(f.root);
    const auto original = game.map.data();
    const auto content = game.content_id();
    for (int mode = 0; mode < 11; ++mode) {
        auto bad = original;
        switch (mode) {
        case 0:
            ++bad.width;
            break;
        case 1:
            ++bad.height;
            break;
        case 2:
            ++bad.origin.x;
            break;
        case 3:
            ++bad.origin.y;
            break;
        case 4:
            bad.tile_size = 2;
            break;
        case 5:
            bad.texture = "other.etex";
            break;
        case 6:
            ++bad.atlas_columns;
            break;
        case 7:
            ++bad.atlas_rows;
            break;
        case 8:
            bad.layers[0].cells[0] = 255;
            break;
        case 9:
            bad.layers[0].order = bad.layers[1].order;
            break;
        case 10:
            std::fill(bad.layers[1].cells.begin(), bad.layers[1].cells.end(), 3);
            break;
        }
        rejects([&] { game.reload_tilemap(std::move(bad)); });
        CHECK(game.content_id() == content);
    }
    auto edited = original;
    put(edited, {-25, -10}, 3);
    rejects([&] {
        game.reload_tilemap(edited, [&](const TileMap& staged) {
            CHECK(staged.solid_cells() == game.map.solid_cells() + 1);
            throw std::runtime_error("Dependent minimap allocation failed");
        });
    });
    CHECK(game.content_id() == content);
    CHECK(!game.collisions.segment({-26, -9.5F}, {-24, -9.5F}, {8, 0, false}));
}
TEST(reload_primes_sensor_contacts_without_manufacturing_alarm_entries) {
    TilemapFiles f;
    auto game = load_game(f.root);
    auto state = game.checkpoint();
    state.position = game.alarm_position();
    game.restore(state);
    CHECK(game.alarm_touching);
    const auto count = game.alarm_entries;
    const auto contacts = std::vector<Contact2D>(game.collisions.contacts().begin(),
                                                 game.collisions.contacts().end());
    auto edited = game.map.data();
    put(edited, {0, 0}, 1, 0);
    game.reload_tilemap(edited);
    CHECK(game.alarm_touching && game.alarm_entries == count && game.collisions.events().empty());
    CHECK(std::vector<Contact2D>(game.collisions.contacts().begin(),
                                 game.collisions.contacts().end()) == contacts);
    game.update({});
    CHECK(game.alarm_entries == count);
    CHECK(game.collisions.events().size() == 1 &&
          game.collisions.events()[0].phase == TriggerPhase::stay);
}
TEST(both_levels_reload_layers_and_restart_with_the_installed_map) {
    TilemapFiles f;
    for (auto level : {Level::facility, Level::annex}) {
        auto game = load_game(f.root, level);
        const auto original_chunks = game.map.chunk_count();
        auto edited = game.map.data();
        auto extra = edited.layers.back();
        extra.order += 1;
        std::fill(extra.cells.begin(), extra.cells.end(), 0);
        extra.cells[0] = 1;
        edited.layers.push_back(extra);
        f.write(edited, level);
        TilemapAssets reload;
        CHECK(reload.reload(game, f.root) && reload.revision() == 2);
        CHECK(game.map.chunk_count() > original_chunks);
        const auto content = game.content_id();
        InputFrame restart{};
        restart[static_cast<std::size_t>(Key::restart)].pressed = true;
        game.update(restart);
        CHECK(game.content_id() == content && game.level == level && game.ticks == 0);
        reload.replaced();
        CHECK(reload.revision() == 1 && reload.applied());
    }
}
TEST(malformed_missing_and_escaping_files_retain_state_repair_and_lock) {
    TilemapFiles f;
    auto game = load_game(f.root);
    const auto original = game.map.data();
    const auto content = game.content_id();
    TilemapAssets reload;
    const auto key = TilemapAssets::key(game.level);
    for (int mode = 0; mode < 3; ++mode) {
        if (mode == 0)
            f.root.write_text(key, "broken ETMP");
        if (mode == 1)
            std::filesystem::remove(f.path / key);
        if (mode == 2)
            std::filesystem::create_symlink("/etc/passwd", f.path / key);
        CHECK(!reload.reload(game, f.root) && reload.revision() == 1);
        CHECK(game.content_id() == content && reload.diagnostic().find(key) != std::string::npos);
    }
    std::filesystem::remove(f.path / key);
    f.write(original);
    const auto map_bytes = game.map.buffer_bytes(),
               collision_bytes = game.collisions.buffer_bytes();
    for (int i = 0; i < 20; ++i) {
        CHECK(reload.reload(game, f.root));
        CHECK(game.map.buffer_bytes() == map_bytes &&
              game.collisions.buffer_bytes() == collision_bytes);
    }
    CHECK(reload.revision() == 21 && reload.diagnostic().empty());
    TilemapAssets locked(true);
    f.root.write_text(key, "broken during replay");
    CHECK(!locked.reload(game, f.root) && locked.revision() == 1 && locked.applied());
    CHECK(game.content_id() == content);
}
TEST(finished_game_retains_progress_after_visual_map_reload) {
    TilemapFiles f;
    auto game = load_game(f.root);
    Replay route;
    while (!game.won) {
        CHECK(game.ticks < 2400);
        game.update(route.next(game));
    }
    auto edited = game.map.data();
    edited.palette[1].atlas_cell = 1;
    const auto before = game.checkpoint();
    game.reload_tilemap(edited);
    const auto after = game.checkpoint();
    CHECK(game.won && game.door_open && game.door_completions == 1);
    CHECK(before.ticks == after.ticks && before.animations == after.animations &&
          before.collected_ids == after.collected_ids);
    game.restore(after);
    CHECK(game.won && game.door_completions == 1);
}
} // namespace
int main() {
    return testing::run_tests();
}
