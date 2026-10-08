#include "../examples/feature_lab/replay_file.hpp"
#include "test.hpp"
#include <limits>
#include <numbers>

namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
void same(Transform a, Transform b) {
    NEAR(a.a, b.a);
    NEAR(a.b, b.b);
    NEAR(a.c, b.c);
    NEAR(a.d, b.d);
    NEAR(a.x, b.x);
    NEAR(a.y, b.y);
}
auto game() {
    return load_game(AssetRoot(TEST_ASSET_ROOT));
}
InputFrame press(Key key) {
    InputFrame result{};
    result[static_cast<std::size_t>(key)].pressed = true;
    return result;
}
TEST(hierarchy_numeric_reference_and_read_only_sampling) {
    Scene scene;
    const auto parent = scene.create(1, "parent"), child = scene.create(2, "child"),
               leaf = scene.create(3, "leaf");
    scene.set_parent(child, parent);
    scene.set_parent(leaf, child);
    constexpr float quarter = std::numbers::pi_v<float> / 2;
    scene.set_transform(parent, {{10, -3}, quarter, {2, 1}});
    scene.set_transform(child, {{3, 2}, quarter, {-1, 0.5F}});
    scene.set_transform(leaf, {{1, -2}, 0, {3, 2}});
    const auto p = scene.world_transform(leaf).apply({1, 1});
    NEAR(p.x, 12);
    NEAR(p.y, 3);
    const auto before = serialize_scene(scene);
    int calls = 0;
    const auto sampled = scene
                             .sampled_world_transform(leaf,
                                                      [&](const SceneNode& node) {
                                                          ++calls;
                                                          auto pose = node.local;
                                                          if (node.entity == parent)
                                                              pose.rotation = 0;
                                                          return pose;
                                                      })
                             .apply({1, 1});
    CHECK(calls == 3);
    NEAR(sampled.x, 16);
    NEAR(sampled.y, -5);
    CHECK(serialize_scene(scene) == before);
    rejects([&] {
        scene.sampled_world_transform({}, [&](const SceneNode& node) {
            ++calls;
            return node.local;
        });
    });
    CHECK(calls == 3);
    rejects([&] {
        scene.sampled_world_transform(leaf, [](const SceneNode& node) {
            auto pose = node.local;
            pose.rotation = std::numeric_limits<float>::infinity();
            return pose;
        });
    });
}
TEST(camera_corners_resize_aspects_and_invalid_domains) {
    for (const auto [width, height] : std::array<std::array<int, 2>, 5>{
             {{1920, 1080}, {600, 900}, {1, 1}, {4096, 100}, {100, 4096}}}) {
        for (float zoom : {8.0F, 14.0F, 26.0F}) {
            const Camera c{{-13, 7}, zoom};
            const auto extent = c.extent(width, height);
            const auto top_left = c.screen_to_world({}, width, height);
            const auto bottom_right = c.screen_to_world(
                {static_cast<float>(width), static_cast<float>(height)}, width, height);
            NEAR(top_left.x, -13 - extent.x / 2);
            NEAR(top_left.y, 7 + zoom / 2);
            NEAR(bottom_right.x, -13 + extent.x / 2);
            NEAR(bottom_right.y, 7 - zoom / 2);
            for (auto screen :
                 {Vec2{0, 0}, Vec2{static_cast<float>(width), static_cast<float>(height)},
                  Vec2{static_cast<float>(width) * 0.25F, static_cast<float>(height) * 0.75F},
                  Vec2{-10, -20}}) {
                const auto round =
                    c.world_to_screen(c.screen_to_world(screen, width, height), width, height);
                CHECK(std::abs(round.x - screen.x) < 0.002F);
                CHECK(std::abs(round.y - screen.y) < 0.002F);
            }
        }
    }
    const auto inf = std::numeric_limits<float>::infinity();
    rejects([&] { Camera{{inf, 0}, 8}.screen_to_world({}, 10, 10); });
    rejects([&] { Camera{{0, 0}, 0}.extent(10, 10); });
    rejects([&] { Camera{}.extent(10, -1); });
    rejects([&] { Camera{{}, std::numeric_limits<float>::max()}.extent(100, 1); });
    // Avoid an overflowing intermediate when the actual aspect ratio is one.
    NEAR((Camera{{}, 1e35F}.extent(100000, 100000).x / 1e35F), 1);
}
TEST(follow_interpolation_edges_and_oversized_viewports) {
    auto g = game();
    g.camera_height = 8;
    g.previous = {0, 0};
    g.position = {4, 2};
    auto c = g.camera(800, 800, 0.25F);
    NEAR(c.center.x, 1);
    NEAR(c.center.y, 0.5F);
    const auto player_pose = g.render_transform(g.player, 0.25F);
    NEAR(player_pose.x, c.center.x);
    NEAR(player_pose.y, c.center.y);
    g.previous = g.position = {-100, 100};
    c = g.camera(800, 800, 1);
    NEAR(c.center.x, -28);
    NEAR(c.center.y, 12);
    c = g.camera(4096, 100, 1);
    NEAR(c.center.x, 0);
    NEAR(c.center.y, 12);
    c = g.camera(100, 4096, 1);
    NEAR(c.center.y, 12);
    g.camera_height = 26;
    c = g.camera(4096, 100, 1);
    NEAR(c.center.x, 0);
    NEAR(c.center.y, 3);
    for (int i = 0; i < 300; ++i) {
        InputFrame input{};
        input[static_cast<std::size_t>(Key::zoom_in)].held = true;
        g.update(input);
    }
    NEAR(g.camera_height, 8);
}
TEST(decorations_orbit_scale_mirror_shear_and_wrap) {
    auto g = game();
    const auto panel = g.world.find(51);
    const auto authored = serialize_scene(g.world);
    const std::array<Vec2, 5> centers{
        {{-25.25F, -4}, {-27, -2.25F}, {-28.75F, -4}, {-27, -5.75F}, {-25.25F, -4}}};
    for (unsigned checkpoint = 0; checkpoint < centers.size(); ++checkpoint) {
        while (g.ticks < checkpoint * 60)
            g.update({});
        const auto model = g.render_transform(panel, 1);
        NEAR(model.x, centers[checkpoint].x);
        NEAR(model.y, centers[checkpoint].y);
        CHECK(model.a * model.d - model.b * model.c < 0);              // Mirrored winding.
        CHECK(std::abs(model.a * model.c + model.b * model.d) > 0.1F); // Sheared axes.
        const float sx = checkpoint == 1 ? 1.25F : checkpoint == 3 ? 0.75F : 1.0F;
        const float sy = checkpoint == 1 ? 0.8F : checkpoint == 3 ? 1.2F : 1.0F;
        NEAR(std::abs(model.a * model.d - model.b * model.c), 0.8F * sx * sy);
    }
    const auto a = g.render_transform(panel, 0), b = g.render_transform(panel, 0.5F),
               c = g.render_transform(panel, 1);
    CHECK(a.y < b.y && b.y < c.y);
    CHECK(std::hypot(a.x - c.x, a.y - c.y) < 0.05F); // No full-turn interpolation jump.
    auto restored = game();
    restored.restore(g.checkpoint());
    const auto loaded = restored.render_transform(restored.world.find(51), 0);
    CHECK(c.a == loaded.a && c.b == loaded.b && c.c == loaded.c && c.d == loaded.d &&
          c.x == loaded.x && c.y == loaded.y);
    CHECK(serialize_scene(g.world) == authored);
    rejects([&] { g.render_transform(panel, -0.1F); });
    rejects([&] { g.render_transform(panel, std::numeric_limits<float>::quiet_NaN()); });
}
TEST(decorations_pause_step_restore_restart_and_large_ticks) {
    auto g = game();
    for (int i = 0; i < 61; ++i)
        g.update({});
    g.update(press(Key::pause));
    const auto frozen = g.render_transform(g.world.find(52), 0);
    for (int i = 0; i < 10; ++i)
        g.update({});
    same(frozen, g.render_transform(g.world.find(52), 1));
    g.update(press(Key::single_step));
    CHECK(g.ticks == 62 && g.paused);
    same(g.render_transform(g.world.find(52), 0), g.render_transform(g.world.find(52), 1));
    auto restored = game();
    restored.restore(decode_checkpoint(encode_checkpoint(g.checkpoint())));
    same(g.render_transform(g.world.find(52), 1),
         restored.render_transform(restored.world.find(52), 0));
    auto state = restored.checkpoint();
    state.ticks = static_cast<std::uint64_t>(INT64_MAX) - 10;
    restored.restore(state);
    const auto large = restored.render_transform(restored.world.find(52), 0.5F);
    state.ticks %= 240;
    restored.restore(state);
    same(large, restored.render_transform(restored.world.find(52), 0.5F));
    restored.update(press(Key::restart));
    const auto fresh = game();
    same(restored.render_transform(restored.world.find(52), 0),
         fresh.render_transform(fresh.world.find(52), 1));
}
TEST(decorations_replay_same_interpolated_pose_every_command) {
    auto source = game();
    ReplayRecorder recorder(source);
    for (int i = 0; i < 300; ++i) {
        const auto input = i == 40 || i == 80 ? press(Key::pause)
                           : i == 60          ? press(Key::single_step)
                                              : InputFrame{};
        recorder.step(source, input);
    }
    const auto file = decode_replay(encode_replay(recorder.file()));
    auto reference = game(), playback = game();
    playback.restore(file.initial);
    for (std::size_t i = 0; i < file.commands.size(); ++i) {
        reference.update(file.commands[i].input());
        playback_step(playback, file, i);
        for (float alpha : {0.0F, 0.5F, 1.0F})
            same(reference.render_transform(reference.world.find(52), alpha),
                 playback.render_transform(playback.world.find(52), alpha));
    }
}
TEST(animated_subtrees_reject_gameplay_and_collision_nodes) {
    const AssetRoot assets(TEST_ASSET_ROOT);
    const auto make = [&](Scene scene) {
        return Game(serialize_scene(scene),
                    decode_tilemap(assets.read("facility.etmp", 4 * 1024 * 1024)),
                    std::make_shared<const AnimationSet>(
                        decode_animations(assets.read("facility.eani", 1024 * 1024))));
    };
    auto scene = parse_scene(assets.text("facility.scene"));
    scene.set_collider(scene.find(52), {{0.2F, 0.2F}});
    rejects([&] { make(std::move(scene)); });
    scene = parse_scene(assets.text("facility.scene"));
    scene.set_parent(scene.find(30), scene.find(50));
    rejects([&] { make(std::move(scene)); });
}
} // namespace
int main() {
    return testing::run_tests();
}
