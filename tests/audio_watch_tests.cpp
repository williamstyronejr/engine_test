#include "../examples/feature_lab/asset_reload.hpp"
#include "../examples/feature_lab/replay_file.hpp"
#include "../examples/feature_lab/soundtrack.hpp"
#include "engine/window.hpp"
#include "scene_fixture.hpp"
#include <cstdlib>
#include <thread>

namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
using namespace std::chrono_literals;
TEST(watched_bank_is_atomic_recovers_and_preserves_recorded_simulation) {
    SceneFiles files;
    auto game = load_game(files.root);
    auto reference = load_game(files.root);
    ReplayRecorder recorder(game), reference_recorder(reference);
    Window window(64, 64, "Audio watch", false);
    Renderer renderer;
    TextureAssets textures(files.root);
    textures.load(renderer, "textures/white.etex", 1, 1);
    ShaderTools shaders;
    FontAssets fonts;
    Soundtrack track(files.root, true, "null", {.4F, .2F, .3F, true}, true);
    CHECK(track.enabled());
    AssetReload watch(files.root, textures, {}, {}, {}, [&] {
        track.reload(files.root);
        return true;
    });
    auto now = AssetWatcher::Time{};
    const auto tick = [&](std::uint32_t manual = 0) {
        watch.update(renderer, files.root, shaders, textures, fonts, manual, now);
        track.collect();
    };
    const auto settle = [&] {
        now += 200ms;
        tick();
        now += 150ms;
        tick();
    };
    const auto acknowledged = [&] {
        const auto deadline = std::chrono::steady_clock::now() + 3s;
        while (track.retained_sound_bytes() != track.sound_bytes()) {
            CHECK(std::chrono::steady_clock::now() < deadline);
            track.collect();
            std::this_thread::sleep_for(1ms);
        }
    };
    tick();
    now += 149ms;
    tick();
    CHECK(watch.audio_reloads() == 0);
    now += 1ms;
    tick();
    acknowledged();
    CHECK(watch.audio_reloads() == 1 && track.revision() == 2);
    const auto original_bytes = track.sound_bytes();
    const auto original = files.root.read("sounds/music.wav", SoundData::max_file_bytes);
    files.root.write_atomic("sounds/music.wav", encode_wav({2, {.2F, -.2F, .4F, -.4F}}));
    files.root.write_text("sounds/door.wav", "invalid WAV");
    settle();
    CHECK(!track.reload_applied() && track.revision() == 2 &&
          track.sound_bytes() == original_bytes);
    CHECK(track.enabled() && track.settings().muted && watch.audio_reloads() == 2);
    now += 2s;
    tick();
    CHECK(watch.audio_reloads() == 2);
    files.root.write_atomic("sounds/door.wav", encode_wav({1, {.1F, .2F}}));
    settle();
    acknowledged();
    CHECK(track.reload_applied() && track.revision() == 3 && track.sound_bytes() != original_bytes);
    CHECK(track.stats().started == 2 && track.stats().active_voices == 2 &&
          track.stats().completed == 0);
    for (int i = 0; i < 12; ++i) {
        files.root.write_atomic("sounds/music.wav", i % 2 ? original : encode_wav({1, {.1F}}));
        settle();
        acknowledged();
        CHECK(track.reload_applied() && track.stats().started == 2 && track.stats().completed == 0);
        InputFrame input{};
        input[static_cast<std::size_t>(Key::right)].held = true;
        recorder.step(game, input);
        reference_recorder.step(reference, input);
    }
    CHECK(encode_replay(recorder.file()) == encode_replay(reference_recorder.file()));
    CHECK(watch.animation_reloads() == 0 && watch.tilemap_reloads() == 0 &&
          watch.scene_reloads() == 0);
    CHECK(watch.texture_reloads() == 1 && watch.shader_reloads() == 1 && renderer.healthy());
}
TEST(audio_watch_manual_priority_and_disabled_output_omission) {
    SceneFiles files;
    Window window(64, 64, "Audio watch controls", false);
    Renderer renderer;
    TextureAssets textures(files.root);
    textures.load(renderer, "textures/white.etex", 1, 1);
    ShaderTools shaders;
    FontAssets fonts;
    std::size_t calls = 0;
    AssetReload watch(files.root, textures, {}, {}, {}, [&] {
        ++calls;
        return true;
    });
    const auto now = AssetWatcher::Time{};
    watch.update(renderer, files.root, shaders, textures, fonts, 0, now);
    watch.update(renderer, files.root, shaders, textures, fonts, AssetReload::audio_group,
                 now + 150ms);
    CHECK(calls == 0 && watch.audio_reloads() == 0);
    AssetReload disabled(files.root, textures);
    disabled.update(renderer, files.root, shaders, textures, fonts, 0, now);
    disabled.update(renderer, files.root, shaders, textures, fonts, 0, now + 150ms);
    files.root.write_text("sounds/pickup.wav", "bad while audio off");
    disabled.update(renderer, files.root, shaders, textures, fonts, 0, now + 1s);
    disabled.update(renderer, files.root, shaders, textures, fonts, 0, now + 2s);
    CHECK(disabled.audio_reloads() == 0);
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY"))
        return 77;
    return testing::run_tests();
}
