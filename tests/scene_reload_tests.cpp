#include "scene_fixture.hpp"
#include "../examples/feature_lab/replay_file.hpp"

namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
TEST(scene_budget_overflow_rejects_reload_then_valid_content_retries) {
    SceneFiles f;
    auto game = load_game(f.root);
    auto candidate = parse_scene(f.text());
    while (candidate.size() < Game::scene_limits.entities)
        candidate.create(1000 + candidate.size(), "decoration", "decor");
    const auto full = serialize_scene(candidate);
    candidate.create(9999, "excess", "decor");
    const auto before = replay_hash(game);
    const auto player = game.player;
    const auto usage = game.world.memory_usage();
    rejects([&] { game.reload_scene(serialize_scene(candidate)); });
    CHECK(replay_hash(game) == before && game.world.valid(player));
    CHECK(game.world.memory_usage() == usage && game.world.limits() == Game::scene_limits);
    game.reload_scene(full);
    CHECK(!game.world.valid(player) && game.world.size() == Game::scene_limits.entities);
    CHECK(game.world.limits() == Game::scene_limits);
}
TEST(commit_rebinds_handles_preserves_progress_and_changes_save_identity) {
    SceneFiles f;
    auto game = load_game(f.root);
    Replay route;
    while (!game.count()) {
        CHECK(game.ticks < 1000);
        game.update(route.next(game));
    }
    game.paused = true;
    const auto before = game.checkpoint();
    const auto player = game.player, core = game.keys.back();
    const auto edited = with_wall(f.text());
    game.reload_scene(edited);
    const auto after = game.checkpoint();
    CHECK(!game.world.valid(player) && !game.world.valid(core));
    CHECK(game.world.valid(game.player) && game.world.get(game.player).id == 20);
    CHECK(after.content[0] != before.content[0] && after.content[1] == before.content[1] &&
          after.content[2] == before.content[2]);
    CHECK(after.collected_ids == before.collected_ids && after.animations == before.animations);
    CHECK(after.ticks == before.ticks && after.paused && after.random_state == before.random_state);
    NEAR(game.position.x, before.position.x);
    NEAR(game.position.y, before.position.y);
    CHECK(game.previous.x == game.position.x && game.previous_ticks == game.ticks);
    CHECK(game.world.component_count<CorePickup>() == 2);
    CHECK(!game.world.find(before.collected_ids.front()));
    CHECK(game.collisions.segment({-26, -9.5F}, {-24, -9.5F}, {8, 0, false}));
    rejects([&] { game.restore(before); });
    f.write(edited);
    auto fresh = load_game(f.root);
    fresh.restore(after);
    CHECK(fresh.count() == game.count() && fresh.content_id() == game.content_id());
}
TEST(scene_walls_affect_movement_queries_and_restart_uses_new_spawn) {
    SceneFiles f;
    auto game = load_game(f.root);
    auto scene = parse_scene(with_wall(f.text()));
    auto pose = scene.get(scene.find(20)).local;
    pose.position = {-28, -10};
    scene.set_transform(scene.find(20), pose);
    game.reload_scene(serialize_scene(scene));
    NEAR(game.position.x, -27);
    NEAR(game.spawn_position.x, -28);
    InputFrame right{};
    right[static_cast<std::size_t>(Key::right)].held = true;
    for (int i = 0; i < 40; ++i)
        game.update(right);
    NEAR(game.position.x, -25.3F);
    game.reload_scene(f.text());
    CHECK(!game.collisions.segment({-26, -9.5F}, {-24, -9.5F}, {8, 0, false}));
    for (int i = 0; i < 20; ++i)
        game.update(right);
    CHECK(game.position.x > -25);
    game.reload_scene(serialize_scene(scene));
    InputFrame restart{};
    restart[static_cast<std::size_t>(Key::restart)].pressed = true;
    game.update(restart);
    NEAR(game.position.x, -28);
    CHECK(game.ticks == 0);
}
TEST(rejected_edits_and_preflight_leave_world_and_runtime_untouched) {
    SceneFiles f;
    auto game = load_game(f.root);
    auto state = game.checkpoint();
    state.position = {-25, -10};
    game.restore(state);
    const auto before = game.checkpoint();
    const auto player = game.player;
    const auto bodies = game.collisions.bodies().size();
    for (int mode = 0; mode < 12; ++mode) {
        auto scene = parse_scene(f.text());
        const auto id = scene.find(30);
        std::string text;
        switch (mode) {
        case 0:
            text = "scene broken";
            break;
        case 1:
            scene.destroy(id);
            break;
        case 2: {
            auto extra = scene.create(99, "Extra", "core");
            scene.set_sprite(extra, {game.animations->texture});
            break;
        }
        case 3:
            scene.set_parent(scene.find(21), id);
            break;
        case 4:
            scene.set_collider(id, {{0.1F, 0.1F}});
            break;
        case 5:
            scene.set_parent(scene.find(40), scene.find(20));
            break;
        case 6:
            text = with_wall(f.text(), game.position);
            break;
        case 7:
            text = with_wall(f.text(), game.spawn_position);
            break;
        case 8: {
            auto t = scene.get(scene.find(1)).local;
            t.position.x = 1;
            scene.set_transform(scene.find(1), t);
            break;
        }
        case 9:
            scene.set_parent(scene.find(40), scene.find(50));
            break;
        case 10:
            scene.destroy(scene.find(20));
            break;
        case 11:
            scene.destroy(scene.find(21));
            break;
        }
        if (text.empty())
            text = serialize_scene(scene);
        bool prepared = false;
        rejects([&] { game.reload_scene(text, [&](const Game&) { prepared = true; }); });
        CHECK(!prepared && game.player == player && game.world.valid(player));
        CHECK(game.content_id() == before.content && game.collisions.bodies().size() == bodies);
        CHECK(game.checkpoint().animations == before.animations);
    }
    // Use a safe edit for the preflight and active-iteration failure paths.
    const auto safe = f.text() + "\n";
    rejects([&] {
        game.reload_scene(safe, [](const Game&) { throw std::runtime_error("view failed"); });
    });
    game.world.each([&](const SceneNode& node) {
        if (node.entity == player)
            rejects([&] { game.reload_scene(safe); });
    });
    CHECK(game.world.valid(player) && game.content_id() == before.content);
}
TEST(reordered_core_records_keep_persistent_collection_and_runtime_slots) {
    SceneFiles f;
    auto game = load_game(f.root);
    auto state = game.checkpoint();
    state.collected_ids = {30};
    game.restore(state);
    auto text = f.text();
    std::string moved;
    for (auto prefix : {"entity 31 ", "sprite 31 "}) {
        const auto begin = text.find(prefix), end = text.find('\n', begin) + 1;
        moved += text.substr(begin, end - begin);
        text.erase(begin, end - begin);
    }
    text.insert(text.find("end\n"), moved);
    game.reload_scene(text);
    CHECK(game.core_ids.back() == 31);
    CHECK(game.count() == 1 && !game.world.find(30));
    game.world.each_component<CorePickup>([&](Entity e, const CorePickup& pickup) {
        CHECK(game.core_ids[pickup.slot] == game.world.get(e).id);
        CHECK(!game.collected[pickup.slot]);
    });
    game.validate_checkpoint(game.checkpoint());
}
TEST(alarm_contacts_are_primed_without_extra_enter_and_animation_snapshots_retained) {
    SceneFiles f;
    auto game = load_game(f.root);
    auto state = game.checkpoint();
    state.position = game.alarm_position();
    game.restore(state);
    const auto count = game.alarm_entries;
    game.reload_scene(with_wall(f.text()));
    CHECK(game.alarm_touching && game.alarm_entries == count && game.collisions.events().empty());
    game.update({});
    CHECK(game.alarm_entries == count && game.collisions.events()[0].phase == TriggerPhase::stay);
    auto map = game.map.data();
    map.palette[1].atlas_cell = 1;
    game.reload_tilemap(map);
    auto animations = std::make_shared<AnimationSet>(*game.animations);
    animations->clips.front().frames.front().ticks += 1;
    game.reload_animations(animations);
    const auto content = game.content_id();
    game.reload_scene(f.text());
    CHECK(game.animations == animations && game.content_id()[1] == content[1] &&
          game.content_id()[2] == content[2]);
}
TEST(opening_and_completed_door_never_replays_completion_after_reload) {
    SceneFiles f;
    for (auto level : {Level::facility, Level::annex}) {
        auto game = load_game(f.root, level);
        Replay route;
        bool opening = false;
        while (!game.won) {
            CHECK(game.ticks < 2400);
            game.update(route.next(game));
            if (game.door_started && !opening) {
                const auto before = game.checkpoint();
                game.reload_scene(f.text(level) + "\n");
                CHECK(game.checkpoint().animations == before.animations && !game.door_open);
                opening = true;
            }
        }
        game.reload_scene(f.text(level));
        CHECK(game.won && game.door_completions == 1);
        auto scene = parse_scene(f.text(level));
        auto pose = scene.get(scene.find(21)).local;
        pose.position.y += 3;
        scene.set_transform(scene.find(21), pose);
        rejects([&] { game.reload_scene(serialize_scene(scene)); });
        CHECK(game.won && game.door_completions == 1);
        InputFrame restart{};
        restart[static_cast<std::size_t>(Key::restart)].pressed = true;
        game.update(restart);
        CHECK(game.ticks == 0 && game.count() == 0 && game.level == level);
    }
}
TEST(malformed_missing_escape_repair_repeated_replacement_and_lock) {
    SceneFiles f;
    auto game = load_game(f.root);
    const auto original = f.text();
    const auto content = game.content_id();
    const auto player = game.player;
    SceneAssets reload;
    for (int mode = 0; mode < 3; ++mode) {
        if (mode == 0)
            f.write("bad scene");
        if (mode == 1)
            std::filesystem::remove(f.path / "facility.scene");
        if (mode == 2)
            std::filesystem::create_symlink("/etc/passwd", f.path / "facility.scene");
        CHECK(!reload.reload(game, f.root) && reload.revision() == 1);
        CHECK(game.world.valid(player) && game.content_id() == content);
        CHECK(reload.diagnostic().find("facility.scene") != std::string::npos);
    }
    std::filesystem::remove(f.path / "facility.scene");
    f.write(original);
    const auto bytes = game.world.buffer_bytes(), collisions = game.collisions.buffer_bytes();
    for (int i = 0; i < 20; ++i) {
        CHECK(reload.reload(game, f.root));
        CHECK(game.world.buffer_bytes() == bytes && game.collisions.buffer_bytes() == collisions);
    }
    CHECK(reload.revision() == 21 && reload.diagnostic().empty());
    SceneAssets locked(true);
    f.write("bad during replay");
    CHECK(!locked.reload(game, f.root) && locked.revision() == 1 && locked.applied());
    CHECK(game.content_id() == content);
    reload.replaced();
    CHECK(reload.revision() == 1 && reload.applied());
}
} // namespace
int main() {
    return testing::run_tests();
}
