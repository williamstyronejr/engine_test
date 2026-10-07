#include "../examples/feature_lab/persistence.hpp"
#include "../examples/feature_lab/settings.hpp"
#include "engine/diagnostics.hpp"
#include "engine/timing.hpp"
#include "test.hpp"
#include <limits>

namespace {
using namespace engine;
using namespace testing;
TEST(timing_history_statistics_and_rejection) {
    FrameHistory history;
    CHECK(history.summary().samples == 0);
    for (int i = 1; i <= 100; ++i)
        history.record({static_cast<double>(2 * i), static_cast<double>(i)});
    const auto summary = history.summary();
    CHECK(summary.samples == 100);
    NEAR(summary.wall_mean_ms, 101);
    NEAR(summary.cpu_mean_ms, 50.5);
    NEAR(summary.cpu_p95_ms, 95);
    NEAR(summary.cpu_max_ms, 100);
    for (const double bad : {-1.0, std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()}) {
        rejects([&] { history.record({bad, 1}); });
        rejects([&] { history.record({1, bad}); });
    }
    CHECK(history.summary().samples == 100);
    NEAR(history.summary().cpu_mean_ms, 50.5);
}
TEST(timing_history_rollover_clear_and_large_samples) {
    FrameHistory history;
    for (std::size_t i = 0; i < FrameHistory::capacity; ++i)
        history.record({100, 10});
    for (std::size_t i = 0; i < FrameHistory::capacity; ++i)
        history.record({20, 2});
    CHECK(history.summary().samples == FrameHistory::capacity);
    NEAR(history.summary().wall_mean_ms, 20);
    NEAR(history.summary().cpu_max_ms, 2);
    history.clear();
    CHECK(history.summary().samples == 0);
    history.record({0, 0});
    NEAR(history.summary().wall_mean_ms, 0);
    history.clear();
    const auto large = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < FrameHistory::capacity; ++i)
        history.record({large, large});
    CHECK(std::isfinite(history.summary().cpu_mean_ms));
    CHECK(history.summary().cpu_mean_ms == large);
}
TEST(debug_toggle_edges_and_focus_cancel) {
    DebugControls controls;
    Input input;
    input.set(Key::diagnostics, true);
    input.set(Key::debug_shapes, true);
    controls.update(input.consume());
    CHECK(controls.visible && controls.shapes);
    input.set(Key::diagnostics, true);
    controls.update(input.consume());
    CHECK(controls.visible && controls.shapes);
    input.release_all();
    controls.update(input.consume());
    CHECK(controls.visible && controls.shapes);
    input.set(Key::diagnostics, true);
    controls.update(input.consume());
    CHECK(!controls.visible && controls.shapes);
}
TEST(single_step_consumes_one_edge_across_catchup) {
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    game.paused = true;
    Input input;
    input.set(Key::right, true);
    input.set(Key::single_step, true);
    FixedClock clock;
    const auto batch = clock.advance(1);
    CHECK(batch.steps == FixedClock::max_steps && batch.dropped > 0);
    for (int i = 0; i < batch.steps; ++i)
        game.update(input.consume());
    CHECK(game.ticks == 1 && game.paused);
    NEAR(game.position.x, -27 + 5.0F / 60);
    NEAR(game.previous.x, game.position.x);
    input.set(Key::single_step, true); // OS repeat cannot advance simulation.
    game.update(input.consume());
    CHECK(game.ticks == 1);
    input.set(Key::single_step, false);
    input.set(Key::single_step, true);
    input.set(Key::single_step, false); // Quick tap still steps once.
    game.update(input.consume());
    CHECK(game.ticks == 2 && game.paused);
    const auto saved = game.checkpoint();
    game.restore(feature_lab::decode_checkpoint(feature_lab::encode_checkpoint(saved)));
    CHECK(game.ticks == 2 && game.paused);
    NEAR(game.previous.x, game.position.x);
}
TEST(single_step_matches_complete_unpaused_replay) {
    auto running = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    auto stepped = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    stepped.paused = true;
    feature_lab::Replay replay;
    for (int i = 0; i < 2400 && !running.won; ++i) {
        auto frame = replay.next(running);
        const auto picked = running.update(frame);
        frame[static_cast<std::size_t>(Key::single_step)].pressed = true;
        CHECK(stepped.update(frame) == picked);
        CHECK(stepped.paused && stepped.ticks == running.ticks);
        NEAR(stepped.previous.x, stepped.position.x);
        NEAR(stepped.previous.y, stepped.position.y);
        CHECK(stepped.collisions.contacts().size() == running.collisions.contacts().size());
        if (i % 100 == 0 || running.won) {
            auto expected = running.checkpoint();
            expected.paused = true;
            CHECK(feature_lab::encode_checkpoint(expected) ==
                  feature_lab::encode_checkpoint(stepped.checkpoint()));
        }
        const auto ticks = stepped.ticks;
        stepped.update({});
        CHECK(stepped.ticks == ticks);
    }
    CHECK(running.won && stepped.won && stepped.door_completions == 1);
    CHECK(stepped.alarm_entries > 0);
    InputFrame step{};
    step[static_cast<std::size_t>(Key::single_step)].pressed = true;
    stepped.update(step);
    CHECK(stepped.ticks == running.ticks); // Win is terminal even when stepping.
}
TEST(single_step_pause_and_restart_priority) {
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    InputFrame frame{};
    frame[static_cast<std::size_t>(Key::single_step)].pressed = true;
    game.update(frame);
    CHECK(game.ticks == 1 && !game.paused); // No extra tick while running.
    frame[static_cast<std::size_t>(Key::pause)].pressed = true;
    game.update(frame);
    CHECK(game.ticks == 1 && game.paused); // Pause wins over simultaneous step.
    game.update(frame);
    CHECK(game.ticks == 2 && !game.paused);
    frame[static_cast<std::size_t>(Key::restart)].pressed = true;
    game.update(frame);
    CHECK(game.ticks == 0 && !game.paused);
}
TEST(settings_capture_step_and_debug_controls_until_release) {
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    game.paused = true;
    feature_lab::Settings settings;
    DebugControls debug;
    Input input;
    const auto update = [&] {
        const auto routed = settings.update(game, input.consume(), 1280, 720, false);
        game.update(routed.gameplay);
        debug.update(routed.gameplay);
    };
    input.set(Key::settings, true);
    update();
    input.set(Key::settings, false);
    input.set(Key::single_step, true);
    input.set(Key::diagnostics, true);
    input.set(Key::debug_shapes, true);
    update();
    input.set(Key::settings, true);
    update();
    CHECK(!settings.opened() && game.paused);
    update();
    CHECK(game.ticks == 0 && !debug.visible && !debug.shapes);
    input.release_all();
    update();
    input.set(Key::single_step, true);
    input.set(Key::diagnostics, true);
    input.set(Key::debug_shapes, true);
    update();
    CHECK(game.ticks == 1 && game.paused && debug.visible && debug.shapes);
}
} // namespace
int main() {
    return testing::run_tests();
}
