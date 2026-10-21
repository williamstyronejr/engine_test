#include "../examples/feature_lab/replay_file.hpp"
#include "../examples/feature_lab/scene_flow.hpp"
#include "test.hpp"
#include <filesystem>
#include <unistd.h>

namespace {
using namespace testing;
using namespace engine;
using namespace feature_lab;
const AssetRoot assets(TEST_ASSET_ROOT);
TEST(sample_collision_budgets_survive_restart_reload_restore_and_transition) {
    const CollisionWorld expected(Game::collision_limits);
    const auto bytes = expected.buffer_bytes();
    for (const auto level : {Level::facility, Level::annex}) {
        auto game = load_game(assets, level);
        std::size_t participating = 0;
        for (const auto& body : game.collisions.bodies())
            if (body.mask) {
                ++participating;
                CHECK(body.shape.half.x * 2 < CollisionWorld::cell_size);
                CHECK(body.shape.half.y * 2 < CollisionWorld::cell_size);
            }
        CHECK(participating == 2 && Game::collision_limits.memberships >= participating * 4);
        const auto check = [&] {
            CHECK(game.collisions.limits() == Game::collision_limits);
            CHECK(game.collisions.buffer_bytes() == bytes);
        };
        check();
        game.reload_tilemap(game.map.data());
        check();
        game.reload_scene(serialize_scene(game.world));
        check();
        game.restore(game.checkpoint());
        check();
        InputFrame restart{};
        restart[static_cast<std::size_t>(Key::restart)].pressed = true;
        game.update(restart);
        check();
        SceneFlow flow(game, false, &assets);
        game.won = true;
        flow.advance_level(game, {});
        CHECK(game.level == next_level(level));
        check();
    }
}
void finish(Game& game) {
    Replay route;
    for (int step = 0; step < 3000 && !game.won; ++step)
        game.update(route.next(game));
    CHECK(game.won && game.count() == 3 && game.door_completions == 1);
}
struct Files {
    std::filesystem::path path;
    Files() {
        auto pattern = (std::filesystem::temp_directory_path() / "levels-XXXXXX").string();
        CHECK(mkdtemp(pattern.data()));
        path = pattern;
        for (const auto name :
             {"facility.scene", "facility.etmp", "facility.eani", "annex.scene", "annex.etmp"})
            std::filesystem::copy_file(assets.directory() / name, path / name);
    }
    ~Files() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
TEST(authored_levels_have_distinct_content_spawns_and_complete_routes) {
    auto facility = load_game(assets);
    auto annex = load_game(assets, Level::annex);
    CHECK(facility.content_id() != annex.content_id());
    CHECK(annex.map.data().width == 40 && facility.map.data().width == 64);
    NEAR(annex.spawn_position.x, -15);
    NEAR(annex.spawn_position.y, 0);
    finish(facility);
    finish(annex);
    InputFrame restart{};
    restart[static_cast<std::size_t>(Key::restart)].pressed = true;
    annex.update(restart);
    CHECK(annex.level == Level::annex && !annex.won && annex.ticks == 0);
    NEAR(annex.position.x, -15);
    NEAR(annex.position.y, 0);
}
TEST(core_components_follow_pickup_save_restart_and_level_replacement) {
    auto game = load_game(assets);
    const auto initial = game.checkpoint();
    CHECK(game.world.component_count<CorePickup>() == 3);
    const auto first_core = game.keys.front();
    auto near_core = game.checkpoint();
    near_core.position = game.location(first_core);
    game.restore(near_core);
    const auto picked = game.keys.front();
    CHECK(game.update({}) == 1);
    CHECK(!game.world.valid(picked) && !game.world.try_component<CorePickup>(picked));
    CHECK(game.world.component_count<CorePickup>() == 2 && game.count() == 1);
    const auto progress = game.checkpoint();
    game.restore(initial);
    CHECK(game.world.component_count<CorePickup>() == 3);
    game.restore(progress);
    CHECK(game.world.component_count<CorePickup>() == 2 && game.count() == 1);
    game.restore(initial);
    finish(game);
    CHECK(game.world.component_count<CorePickup>() == 0);
    SceneFlow flow(game, false, &assets);
    flow.advance_level(game, {});
    CHECK(game.level == Level::annex && game.world.component_count<CorePickup>() == 3);
    finish(game);
    CHECK(game.world.component_count<CorePickup>() == 0);
    InputFrame restart{};
    restart[static_cast<std::size_t>(Key::restart)].pressed = true;
    game.update(restart);
    CHECK(game.world.component_count<CorePickup>() == 3);
}
TEST(level_commit_resets_world_and_blocks_held_input) {
    auto game = load_game(assets);
    SceneFlow flow(game, false, &assets);
    rejects([&] { flow.advance_level(game, {}); });
    game.random_seed = 42;
    game.random.restore(42);
    finish(game);
    const auto old = game.player;
    Input input;
    input.set(Key::right, true);
    input.set(Key::interact, true);
    flow.advance_level(game, input.consume());
    CHECK(game.level == Level::annex && !game.world.valid(old));
    CHECK(game.ticks == 0 && game.previous_ticks == 0 && game.count() == 0 && !game.won);
    CHECK(game.random_seed == 42 && game.random.state() == 42);
    CHECK(!button(flow.update(game, input.consume(), 640, 480, nullptr).gameplay, Key::right).held);
    input.set(Key::right, false);
    flow.update(game, input.consume(), 640, 480, nullptr);
    input.set(Key::right, true);
    CHECK(
        button(flow.update(game, input.consume(), 640, 480, nullptr).gameplay, Key::right).pressed);
    flow.start_new(game);
    CHECK(game.level == Level::facility && game.ticks == 0);
    NEAR(game.position.x, -27);
}
TEST(missing_corrupt_and_invalid_spawn_levels_retain_the_live_world) {
    Files files;
    const AssetRoot root(files.path);
    auto game = load_game(root);
    finish(game);
    SceneFlow flow(game, false, &root);
    const auto before = encode_checkpoint(game.checkpoint());
    const auto player = game.player;
    const auto original = root.text("annex.scene");
    for (int failure = 0; failure < 4; ++failure) {
        if (failure == 0)
            std::filesystem::remove(files.path / "annex.scene");
        else if (failure == 1)
            root.write_text("annex.scene", "broken scene");
        else {
            auto invalid = original;
            const auto start = invalid.find("1 -15 0 0 1 1");
            CHECK(start != std::string::npos);
            invalid.replace(start, std::string("1 -15 0 0 1 1").size(),
                            failure == 2 ? "1 200 0 0 1 1" : "1 0 -5 0 1 1");
            root.write_text("annex.scene", invalid);
        }
        rejects([&] { flow.advance_level(game, {}); });
        CHECK(game.world.valid(player) && encode_checkpoint(game.checkpoint()) == before);
    }
    root.write_text("annex.scene", original);
    flow.advance_level(game, {});
    CHECK(game.level == Level::annex);
}
TEST(resource_preparation_and_iteration_failures_are_transactional) {
    auto game = load_game(assets);
    finish(game);
    SceneFlow flow(game, false, &assets);
    const auto state = encode_checkpoint(game.checkpoint());
    flow.prepare_replacement = [](const Game&) { throw std::runtime_error("resource failure"); };
    rejects([&] { flow.advance_level(game, {}); });
    CHECK(encode_checkpoint(game.checkpoint()) == state);
    flow.prepare_replacement = {};
    bool tried = false;
    game.world.each([&](const SceneNode&) {
        if (tried)
            return;
        tried = true;
        rejects([&] { flow.advance_level(game, {}); });
    });
    CHECK(tried && encode_checkpoint(game.checkpoint()) == state);
    flow.advance_level(game, {});
    finish(game);
    flow.advance_level(game, {});
    CHECK(game.level == Level::facility && game.count() == 0);
}
TEST(cross_level_save_continue_and_new_game_preserve_content_identity) {
    Files files;
    UserStorage storage({files.path / "config", files.path / "state"});
    auto annex = load_game(assets, Level::annex);
    for (int i = 0; i < 20; ++i)
        annex.update({});
    save_slot(storage, 1, annex, false);
    auto game = load_game(assets);
    SceneFlow flow(game, true, &assets);
    flow.continue_slot(game, &storage);
    CHECK(game.level == Level::annex && !flow.title_open());
    CHECK(encode_checkpoint(game.checkpoint()) == encode_checkpoint(annex.checkpoint()));
    const auto before = encode_checkpoint(game.checkpoint());
    auto bad = annex.checkpoint();
    bad.content[0] ^= 1;
    rejects([&] { restore_game(game, bad, &assets); });
    CHECK(encode_checkpoint(game.checkpoint()) == before);
    flow.start_new(game);
    CHECK(game.level == Level::facility && game.ticks == 0);
}
TEST(replay_recording_in_annex_resolves_from_a_facility_start) {
    auto recorded = load_game(assets, Level::annex);
    ReplayRecorder recorder(recorded);
    Replay route;
    for (int i = 0; i < 50; ++i)
        recorder.step(recorded, route.next(recorded));
    const auto file = decode_replay(encode_replay(recorder.file()));
    auto played = load_game(assets);
    restore_game(played, file.initial, &assets);
    for (std::size_t i = 0; i < file.commands.size(); ++i)
        playback_step(played, file, i);
    CHECK(played.level == Level::annex && replay_hash(played) == replay_hash(recorded));
}
} // namespace
int main() {
    return testing::run_tests();
}
