#include "../examples/feature_lab/soundtrack.hpp"
#include "engine/audio.hpp"
#include "test.hpp"
#include <chrono>
#include <thread>

namespace {
using namespace engine;
using namespace testing;
template <class F> void eventually(F condition) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!condition()) {
        CHECK(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
TEST(output_owns_clips_and_orders_controls) {
    for (int cycle = 0; cycle < 5; ++cycle) {
        AudioOutput audio("null");
        const auto clip = audio.register_sound({1, {0.25F, -0.25F}}); // Temporary is destroyed now.
        CHECK(audio.sound_count() == 1 && audio.sound_bytes() == 8);
        CHECK(!audio.play({}, {}));
        CHECK(!audio.play(clip, {-1}));
        const auto voice = audio.play(clip, {1, 0, true});
        CHECK(voice);
        CHECK(audio.pause(voice, true));
        CHECK(audio.set_pan(voice, -0.5F));
        CHECK(audio.set_gain(voice, 0.5F));
        CHECK(audio.set_master_gain(0.2F));
        CHECK(audio.set_group_gain(AudioGroup::effects, 0.5F));
        CHECK(audio.pause_group(AudioGroup::effects, true));
        CHECK(audio.pause_group(AudioGroup::effects, false));
        CHECK(audio.pause(voice, false));
        CHECK(audio.stop(voice));
        eventually([&] { return audio.stats().processed >= 10; });
        const auto stats = audio.stats();
        CHECK(stats.started == 1 && stats.stopped == 1 && stats.active_voices == 0);
        CHECK(stats.dropped_plays == 0 && stats.stale_controls == 0 && audio.healthy());
        CHECK(audio.stop(voice));
        eventually([&] { return audio.stats().stale_controls == 1; });
    }
}
TEST(output_bank_bounds_foreign_handles_and_queue_pressure) {
    AudioOutput audio("null"), other("null");
    const auto foreign = other.register_sound({1, {0}});
    const auto clip = audio.register_sound({1, {0.2F}});
    CHECK(!audio.play(foreign));
    for (std::size_t i = 1; i < AudioOutput::max_sounds; ++i)
        audio.register_sound({1, {0}});
    rejects([&] { audio.register_sound({1, {0}}); });
    rejects([&] { audio.register_sound({1, {2}}); });
    std::uint64_t accepted = 0;
    for (int i = 0; i < 1000; ++i)
        if (audio.play(clip, {1, 0, true}))
            ++accepted;
    eventually([&] { return audio.stats().processed >= accepted; });
    const auto stats = audio.stats();
    CHECK(stats.active_voices == Mixer::max_voices);
    CHECK(stats.started + stats.dropped_plays == accepted);
    CHECK(stats.queue_rejections + accepted == 1000);
    CHECK(audio.stop_all());
    eventually([&] { return audio.stats().active_voices == 0; });
    CHECK(audio.healthy());
}
TEST(feature_lab_audio_controls_restart_and_completion) {
    const AssetRoot assets(TEST_ASSET_ROOT);
    auto game = feature_lab::load_game(assets);
    feature_lab::Soundtrack track(assets, true, "null");
    CHECK(track.enabled());
    track.locate_emitter(game);
    InputFrame controls{};
    controls[static_cast<std::size_t>(Key::mute)].pressed = true;
    controls[static_cast<std::size_t>(Key::music_volume)].pressed = true;
    controls[static_cast<std::size_t>(Key::effects_volume)].pressed = true;
    track.update(game, controls, 0);
    CHECK(track.status().find("M MUTED") != std::string::npos);
    CHECK(track.status().find("MUSIC 15") != std::string::npos);
    controls = {};
    controls[static_cast<std::size_t>(Key::pause)].pressed = true;
    game.update(controls);
    track.update(game, controls, 0);
    controls = {};
    controls[static_cast<std::size_t>(Key::restart)].pressed = true;
    game.update(controls);
    track.update(game, controls, 0);
    eventually([&] { return track.stats().started == 4 && track.stats().stopped == 2; });
    feature_lab::Replay replay;
    for (int i = 0; i < 2400 && !game.won; ++i) {
        const auto frame = replay.next(game);
        const int picked = game.update(frame);
        track.update(game, frame, picked);
        // Null output consumes without hardware pacing. Let commands drain as in real play.
        if (i % 10 == 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(game.won && track.enabled());
    eventually([&] { return track.stats().started == 8 && track.stats().active_voices == 1; });
    CHECK(track.stats().queue_rejections == 0 && track.stats().dropped_plays == 0);
}
TEST(settings_gain_validation_and_command_coalescing) {
    const AssetRoot assets(TEST_ASSET_ROOT);
    feature_lab::Soundtrack track(assets, true, "null");
    CHECK(track.enabled());
    eventually([&] { return track.stats().processed >= 2; });
    track.configure({0.4F, 0.6F, 0.8F, true});
    eventually([&] { return track.stats().processed >= 5; });
    const auto processed = track.stats().processed;
    for (int i = 0; i < 1000; ++i)
        track.configure(track.settings());
    CHECK(track.stats().queue_rejections == 0 && track.stats().processed == processed);
    rejects([&] { track.configure({NAN, 0, 0, false}); });
    rejects([&] { track.configure({1, -1, 0, false}); });
    NEAR(track.settings().master, 0.4F);
    NEAR(track.settings().music, 0.6F);
    InputFrame unmute{};
    unmute[static_cast<std::size_t>(Key::mute)].pressed = true;
    track.update(feature_lab::load_game(assets), unmute, 0);
    CHECK(!track.settings().muted);
    NEAR(track.settings().master, 0.4F);
}
TEST(checkpoint_restore_restarts_loops_without_historical_effects) {
    const AssetRoot assets(TEST_ASSET_ROOT);
    auto game = feature_lab::load_game(assets);
    feature_lab::Soundtrack track(assets, true, "null");
    CHECK(track.enabled());
    eventually([&] { return track.stats().started == 2; });
    feature_lab::Replay replay;
    while (!game.won)
        game.update(replay.next(game));
    track.restored(game);
    eventually([&] { return track.stats().started == 4 && track.stats().active_voices == 1; });
    CHECK(track.stats().stopped == 3); // Two old loops, then the new completed-level machine.
    CHECK(track.stats().queue_rejections == 0);
    const auto saved = game.checkpoint();
    game.restore(saved);
    game.paused = true;
    track.restored(game);
    eventually([&] { return track.stats().started == 6 && track.stats().active_voices == 1; });
    CHECK(track.enabled());
    feature_lab::Soundtrack initially_muted(assets, true, "null", {0.1F, 0.2F, 0.3F, true});
    CHECK(initially_muted.enabled() && initially_muted.settings().muted);
    eventually([&] { return initially_muted.stats().processed >= 2; });
    CHECK(initially_muted.stats().started == 2);
    feature_lab::Soundtrack disabled(assets, false);
    disabled.configure({0.2F, 0.4F, 0.6F, true});
    NEAR(disabled.settings().master, 0.2F);
    CHECK(disabled.settings().muted);
}
TEST(single_step_keeps_audio_groups_paused_until_resume) {
    const AssetRoot assets(TEST_ASSET_ROOT);
    auto game = feature_lab::load_game(assets);
    feature_lab::Soundtrack track(assets, true, "null");
    CHECK(track.enabled());
    eventually([&] { return track.stats().started == 2; });
    game.paused = true;
    const auto processed = track.stats().processed;
    track.update(game, {}, 0);
    eventually([&] { return track.stats().processed >= processed + 2; });
    feature_lab::Replay replay;
    int picked = 0;
    for (int i = 0; i < 2400 && !picked; ++i) {
        auto step = replay.next(game);
        step[static_cast<std::size_t>(Key::single_step)].pressed = true;
        picked = game.update(step);
        track.update(game, step, picked);
    }
    CHECK(picked == 1 && game.paused);
    eventually([&] { return track.stats().started == 3; });
    CHECK(track.stats().completed == 0);
    CHECK(track.stats().active_voices == 3);
    game.paused = false;
    track.update(game, {}, 0);
    eventually([&] { return track.stats().completed == 1; });
    CHECK(track.enabled() && track.stats().queue_rejections == 0);
}
TEST(missing_audio_device_is_rejected) {
    rejects([] { AudioOutput bad("engine_test_intentionally_missing_device"); });
    const AssetRoot assets(TEST_ASSET_ROOT);
    feature_lab::Soundtrack disabled(assets, true, "engine_test_intentionally_missing_device");
    CHECK(!disabled.enabled() && disabled.status() == "AUDIO OFF");
}
} // namespace
int main() {
    return testing::run_tests();
}
