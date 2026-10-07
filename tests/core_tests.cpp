#include "../examples/feature_lab/game.hpp"
#include "engine/assets.hpp"
#include "engine/collision.hpp"
#include "engine/input.hpp"
#include "engine/mixer.hpp"
#include "engine/timing.hpp"
#include "test.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace testing;
using namespace engine;
TEST(camera_round_trip) {
    std::mt19937 random(1234);
    std::uniform_real_distribution<float> coordinate(-100, 100);
    Camera c{{13, -7}, 23};
    for (int i = 0; i < 2000; ++i) {
        const Vec2 p{coordinate(random), coordinate(random)};
        const auto q = c.screen_to_world(c.world_to_screen(p, 1920, 1080), 1920, 1080);
        NEAR(p.x, q.x);
        NEAR(p.y, q.y);
    }
    NEAR(c.screen_to_world({960, 540}, 1920, 1080).x, 13);
    rejects([&] { c.extent(0, 720); });
    c.height = std::numeric_limits<float>::quiet_NaN();
    rejects([&] { c.extent(1280, 720); });
}
TEST(transform_composition) {
    const auto parent = Transform::from({5, 6}, 1.1F, {2, 3});
    const auto child = Transform::from({-2, 1}, 0.4F, {0.5F, 2});
    const Vec2 p{1, 3};
    const auto a = (parent * child).apply(p), b = parent.apply(child.apply(p));
    NEAR(a.x, b.x);
    NEAR(a.y, b.y);
}
TEST(input_edges_survive_until_consumed) {
    Input input;
    input.set(Key::up, true);
    input.set(Key::up, true);
    const auto first = input.consume();
    CHECK(button(first, Key::up).held);
    CHECK(button(first, Key::up).pressed);
    const auto second = input.consume();
    CHECK(button(second, Key::up).held);
    CHECK(!button(second, Key::up).pressed);
    input.set(Key::up, false);
    CHECK(button(input.consume(), Key::up).released);
    CHECK(!button(input.consume(), Key::up).released);
}
TEST(quick_tap_and_focus_loss) {
    Input input;
    input.set(Key::left, true);
    input.set(Key::left, false);
    const auto tap = input.consume();
    CHECK(button(tap, Key::left).pressed);
    CHECK(button(tap, Key::left).released);
    CHECK(!button(tap, Key::left).held);
    input.set(Key::right, true);
    input.release_all();
    const auto cleared = input.consume();
    CHECK(!button(cleared, Key::right).held);
    CHECK(!button(cleared, Key::right).pressed);
}
TEST(fixed_clock_fraction_and_catchup) {
    FixedClock clock;
    const auto a = clock.advance(FixedClock::step / 2);
    CHECK(a.steps == 0);
    NEAR(a.alpha, 0.5F);
    const auto b = clock.advance(FixedClock::step / 2);
    CHECK(b.steps == 1);
    NEAR(b.alpha, 0);
    const auto c = clock.advance(2);
    CHECK(c.steps == 8);
    CHECK(c.dropped > 1.8);
    CHECK(c.alpha >= 0 && c.alpha < 1);
    rejects([&] { clock.advance(-1); });
    rejects([&] { clock.advance(std::numeric_limits<double>::infinity()); });
}
TEST(fixed_clock_conserves_time) {
    FixedClock clock;
    int ticks = 0;
    double input = 0, dropped = 0;
    float alpha = 0;
    for (int i = 0; i < 10000; ++i) {
        const double elapsed = (i % 31 == 0) ? 0.4 : 0.007;
        input += elapsed;
        const auto batch = clock.advance(elapsed);
        ticks += batch.steps;
        dropped += batch.dropped;
        alpha = batch.alpha;
    }
    NEAR(input, static_cast<double>(ticks) * FixedClock::step + dropped +
                    static_cast<double>(alpha) * FixedClock::step);
}
TEST(swept_box_stops_at_wall) {
    const std::array<Rect, 1> walls = {{{{2, -10}, {3, 10}}}};
    const auto p = move_box({0, 0}, {0.5F, 0.5F}, {100, 2}, walls);
    NEAR(p.x, 1.5F);
    NEAR(p.y, 2);
    const auto q = move_box({5, 0}, {0.5F, 0.5F}, {-100, -2}, walls);
    NEAR(q.x, 3.5F);
    NEAR(q.y, -2);
}
TEST(collision_contact_and_corner) {
    CHECK(!overlaps({{0, 0}, {1, 1}}, {{1, 0}, {2, 1}}));
    const std::array<Rect, 2> walls = {{{{2, -10}, {3, 10}}, {{-10, 2}, {10, 3}}}};
    const auto p = move_box({0, 0}, {0.5F, 0.5F}, {100, 100}, walls);
    NEAR(p.x, 1.5F);
    NEAR(p.y, 1.5F);
}
TEST(mixer_pan_completion_and_reuse) {
    Mixer mixer;
    constexpr std::array<float, 2> clip{0.5F, -0.5F};
    std::array<float, 8> output{};
    CHECK(mixer.play(clip, 1, 1));
    mixer.mix(output);
    NEAR(output[0], 0);
    NEAR(output[1], 0.5F);
    NEAR(output[3], -0.5F);
    NEAR(output[5], 0);
    CHECK(mixer.play(clip, 1, -1));
    mixer.mix(output);
    NEAR(output[0], 0.5F);
    NEAR(output[1], 0);
    mixer.mix(output);
    for (float value : output)
        NEAR(value, 0);
}
TEST(mixer_voice_limit_clipping_and_invalid_input) {
    Mixer mixer;
    constexpr std::array<float, 1> clip{1};
    std::array<float, 2> output{};
    for (std::size_t i = 0; i < Mixer::max_voices; ++i)
        CHECK(mixer.play(clip, 1));
    CHECK(!mixer.play(clip, 1));
    mixer.mix(output);
    NEAR(output[0], 1);
    NEAR(output[1], 1);
    CHECK(mixer.play(clip, 1));
    CHECK(!mixer.play({}, 1));
    CHECK(!mixer.play(clip, -1));
    std::array<float, 3> odd{};
    rejects([&] { mixer.mix(odd); });
}
TEST(game_pause_and_restart) {
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    Input input;
    input.set(Key::right, true);
    game.update(input.consume());
    CHECK(game.position.x > -27);
    input.set(Key::pause, true);
    game.update(input.consume());
    const auto p = game.position;
    game.update(input.consume());
    NEAR(game.position.x, p.x);
    CHECK(game.paused);
    input.set(Key::restart, true);
    game.update(input.consume());
    CHECK(!game.paused);
    CHECK(game.ticks == 0);
    NEAR(game.position.x, -27);
}
TEST(game_door_pause_zoom_and_camera) {
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    const auto goal = game.location(game.exit);
    game.position = goal + Vec2{-3, 0};
    InputFrame right{};
    right[static_cast<std::size_t>(Key::right)].held = true;
    for (int i = 0; i < 100; ++i)
        game.update(right);
    NEAR(game.position.x, goal.x - 1.5F);
    CHECK(!game.door_open && !game.won);
    // Collect through normal gameplay, then stop on the first opening tick.
    InputFrame restart{};
    restart[static_cast<std::size_t>(Key::restart)].pressed = true;
    game.update(restart);
    feature_lab::Replay replay;
    for (int i = 0; i < 2400 && !game.door_started; ++i)
        game.update(replay.next(game));
    CHECK(game.door_started && !game.door_open);
    InputFrame pause{};
    pause[static_cast<std::size_t>(Key::pause)].pressed = true;
    game.update(pause);
    const auto frame = game.door_animation.frame_index();
    const auto machine = game.machine_animation.frame_index();
    const auto tick = game.ticks;
    for (int i = 0; i < 100; ++i)
        game.update({});
    CHECK(game.door_animation.frame_index() == frame);
    CHECK(game.machine_animation.frame_index() == machine);
    CHECK(game.ticks == tick && !game.door_open);
    game.update(pause);
    for (int i = 0; i < 100; ++i)
        game.update({});
    CHECK(game.door_open && game.door_completions == 1);
    InputFrame zoom{};
    zoom[static_cast<std::size_t>(Key::zoom_out)].held = true;
    for (int i = 0; i < 200; ++i)
        game.update(zoom);
    NEAR(game.camera_height, 26.0F);
    const auto camera = game.camera(1920, 1080, 1);
    const auto extent = camera.extent(1920, 1080) * 0.5F;
    CHECK(camera.center.x + extent.x <= game.map.bounds().max.x);
    CHECK(camera.center.y + extent.y <= game.map.bounds().max.y);
    const auto wide = game.camera(4096, 100, 1);
    NEAR(wide.center.x, 0.0F);
    game.update(restart);
    CHECK(!game.door_open && game.door_completions == 0);
    CHECK(game.door_animation.paused());
    CHECK(game.door_animation.frame_index() == 0);
    NEAR(game.camera_height, 14.0F);
}
TEST(game_route_completes) {
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    feature_lab::Replay replay;
    for (int i = 0; i < 2400 && !game.won; ++i)
        game.update(replay.next(game));
    CHECK(game.won);
    CHECK(game.count() == 3);
    CHECK(game.door_open);
    CHECK(game.door_completions == 1);
}
} // namespace
int main() {
    return testing::run_tests();
}
