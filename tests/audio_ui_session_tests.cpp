#include "../examples/feature_lab/game_draw.hpp"
#include "../examples/feature_lab/scene_flow.hpp"
#include "../examples/feature_lab/settings_draw.hpp"
#include "engine/window.hpp"
#include "scene_fixture.hpp"
#include <alsa/asoundlib.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <thread>

namespace {
std::atomic<bool> fail_write{};
std::atomic<unsigned> injected_errors{};
std::atomic<std::uint64_t> write_calls{};
} // namespace
extern "C" snd_pcm_sframes_t __real_snd_pcm_writei(snd_pcm_t*, const void*, snd_pcm_uframes_t);
extern "C" snd_pcm_sframes_t __wrap_snd_pcm_writei(snd_pcm_t* pcm, const void* data,
                                                   snd_pcm_uframes_t frames) {
    ++write_calls;
    if (fail_write.exchange(false)) {
        ++injected_errors;
        return -ENODEV;
    }
    return __real_snd_pcm_writei(pcm, data, frames);
}
namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
using namespace std::chrono_literals;
constexpr unsigned cycles = 16, max_steps = 3000;
InputFrame press(Key key) {
    InputFrame result{};
    result[static_cast<std::size_t>(key)].pressed = true;
    return result;
}
template <class F> void eventually(F condition) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!condition()) {
        CHECK(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(1ms);
    }
}
TEST(audio_unavailability_preserves_full_ui_gameplay_and_manual_restart) {
    SceneFiles files;
    UserStorage storage(UserPaths::discover(files.path / "user"));
    Window window{64, 64, "Audio UI session regression", false};
    Renderer renderer;
    const BitmapFont embedded(builtin_font());
    renderer.set_font(embedded);
    auto game = load_game(files.root);
    Overlays overlays(renderer, game);
    CollisionVisuals collision_visuals(renderer);
    SamplingGallery sampling_gallery(renderer);
    overlays.render_minimap(renderer, game, 640, 480, 1);
    const auto presentation = renderer.memory();
    const auto presentation_textures = renderer.live_textures();
    const auto presentation_targets = renderer.live_targets();
    FontAssets fonts;
    CHECK(fonts.reload(renderer, files.root));
    ShaderTools shaders;
    AnimationAssets animations;
    TilemapAssets tilemaps;
    SceneAssets scenes;
    TextureAssets textures(files.root);
    textures.load(renderer, game.map.data().texture, game.map.data().atlas_columns,
                  game.map.data().atlas_rows);
    textures.load(renderer, game.animations->texture, game.animations->atlas_columns,
                  game.animations->atlas_rows);
    game.world.each([&](const SceneNode& node) {
        if (node.sprite)
            textures.load(renderer, node.sprite->texture, 1, 1);
    });
    const auto target = renderer.create_target(640, 480);
    const auto resident = renderer.memory();
    const auto texture_count = renderer.live_textures();
    const auto target_count = renderer.live_targets();
    Settings menu;
    menu.audio = {.7F, .2F, .4F, true};
    Diagnostics diagnostics;
    diagnostics.controls.visible = diagnostics.controls.shapes = true;
    for (unsigned i = 0; i < 240; ++i)
        diagnostics.record({16.75, 2.25}, {1234, 12, 8}, {456, 0, 3}, 1);
    SceneFlow flow(game, false, &files.root);
    std::vector<Rect> staged_walls;
    flow.prepare_replacement = [&](const Game& candidate) {
        textures.validate_level(candidate);
        staged_walls = Overlays::stage(candidate);
    };
    const auto original = files.root.read("sounds/door.wav", SoundData::max_file_bytes);
    std::uint64_t ticks = 0, renders = 0, captures = 0;
    std::string text;
    text.reserve(256);
    const auto started = std::chrono::steady_clock::now();
    for (unsigned cycle = 0; cycle < cycles; ++cycle) {
        // Cover both levels in every mode, rather than coupling mode to level parity.
        const unsigned mode = (cycle / 2) % 4;
        CHECK(game.level == (cycle % 2 ? Level::annex : Level::facility));
        const auto calls_before = write_calls.load();
        if (mode == 3)
            files.root.write_text("sounds/door.wav", "invalid WAV");
        auto audio = std::make_unique<Soundtrack>(files.root, mode != 2, "null", menu.audio);
        if (mode == 3)
            files.root.write_atomic("sounds/door.wav", original);
        CHECK(audio->enabled() == (mode < 2));
        if (mode == 2)
            CHECK(write_calls.load() == calls_before);
        if (audio->enabled()) {
            audio->restored(game);
            eventually([&] { return audio->stats().active_voices == 2; });
        }
        const auto draw = [&] {
            overlays.render_minimap(renderer, game, 640, 480, 1);
            CHECK(overlays.minimap_stats().quads > 0);
            const auto camera = game.camera(640, 480, 1);
            renderer.begin(target, camera);
            draw_game(renderer, game, textures, 1, 640, 480, camera, overlays, *audio,
                      collision_visuals, sampling_gallery, diagnostics, shaders, fonts, animations,
                      tilemaps, scenes, menu.opened(), menu.bindings, "AUDIO UI SESSION", false,
                      false, text);
            diagnostics.draw_hud(renderer, game, 640, 480, .125, text);
            menu.draw(renderer, 640, 480);
            renderer.end();
            CHECK(renderer.healthy() && renderer.stats().quads > 100 && renderer.stats().draws > 0);
            CHECK(renderer.memory() == resident);
            CHECK(renderer.live_textures() == texture_count &&
                  renderer.live_targets() == target_count);
            ++renders;
        };
        const auto capture = [&](std::string_view state) {
            draw();
            renderer.screenshot((files.path / "frame.ppm").string());
            const auto bytes = files.root.read("frame.ppm", 4 * 1024 * 1024);
            CHECK(bytes.size() > 640 * 480 * 3);
            std::uint64_t hash = 1469598103934665603ULL;
            for (const auto byte : bytes) {
                hash ^= byte;
                hash *= 1099511628211ULL;
            }
            ++captures;
            std::cout << "[audio-ui-frame] cycle=" << cycle + 1 << " mode=" << mode
                      << " state=" << state << " hash=" << hash << '\n';
            return hash;
        };
        const auto input = [&](const InputFrame& frame) {
            const auto result = menu.update(game, frame, 640, 480, audio->enabled());
            if (result.audio_changed)
                audio->configure(menu.audio);
            audio->update(game, result.gameplay, 0);
            return result;
        };
        const auto widget = [&](UiId id) {
            const auto widgets = menu.ui().widgets();
            const auto found = std::find_if(widgets.begin(), widgets.end(),
                                            [&](const UiWidget& w) { return w.id == id; });
            CHECK(found != widgets.end());
            return *found;
        };
        const auto pointer = [&](UiId id) {
            InputFrame frame{};
            const auto w = widget(id);
            frame.pointer.position = (w.bounds.min + w.bounds.max) * .5F;
            frame.pointer.press_position = frame.pointer.release_position = frame.pointer.position;
            frame.pointer.inside = true;
            return frame;
        };
        const auto click = [&](UiId id) {
            auto frame = pointer(id);
            frame.pointer.primary = {true, true, false};
            const auto down = input(frame);
            frame.pointer.primary = {false, false, true};
            const auto up = input(frame);
            return down.audio_changed || up.audio_changed;
        };
        Replay route;
        unsigned steps = 0;
        for (; steps < max_steps && !game.won; ++steps) {
            if (steps == 120) {
                const auto checkpoint = encode_checkpoint(game.checkpoint());
                input(press(Key::settings));
                CHECK(menu.opened() && game.paused);
                capture("before");
                InputFrame held{};
                if (mode == 0) {
                    // Fail with a live checkbox press; rebuild must discard its stale release.
                    held = pointer(Settings::mute);
                    held.pointer.primary = {true, true, false};
                    input(held);
                    CHECK(menu.ui().captured() == Settings::mute);
                } else if (mode == 1) {
                    click(Settings::controls);
                    click(Settings::binding_first);
                    CHECK(menu.capturing_binding());
                }
                const auto settings = menu.audio;
                const auto bindings = menu.bindings;
                if (mode < 2) {
                    const auto errors = injected_errors.load();
                    fail_write = true;
                    eventually([&] { return audio->stats().device_error == -ENODEV; });
                    eventually([&] {
                        audio->update(game, {}, 0);
                        return !audio->enabled();
                    });
                    CHECK(injected_errors.load() == errors + 1);
                }
                CHECK(!audio->enabled() && audio->status() == "AUDIO OFF");
                CHECK(audio->sound_bytes() == 0 && audio->retained_sound_bytes() == 0);
                const auto disabled_calls = write_calls.load();
                if (mode == 0) {
                    held.pointer.primary = {true, false, false};
                    input(held); // Availability rebuild must clear a still-held capture.
                    CHECK(menu.ui().captured() == 0);
                    held.pointer.primary = {false, false, true};
                    CHECK(!input(held).audio_changed);
                } else
                    input({}); // Observe availability before drawing or accepting input.
                if (mode == 1) {
                    CHECK(menu.capturing_binding() && menu.controls_opened());
                    capture("capture_disabled");
                    input(press(Key::escape));
                    CHECK(!menu.capturing_binding() && menu.controls_opened());
                    CHECK(menu.bindings == bindings);
                    click(Settings::controls_back);
                }
                CHECK(menu.notice.empty());
                capture("unavailable");
                for (const auto id :
                     {Settings::master, Settings::music, Settings::effects, Settings::mute}) {
                    CHECK(!widget(id).enabled);
                    CHECK(!click(id));
                }
                CHECK(menu.audio == settings && audio->settings() == settings);
                // Repair/reload, settings and gameplay restart do not reconnect this object.
                CHECK(!audio->reload(files.root));
                audio->configure(settings);
                audio->restored(game);
                audio->update(game, press(Key::restart), 0);
                audio->collect();
                CHECK(!audio->enabled() && audio->reload_status() == "AUDIO BANK OFF");
                CHECK(write_calls.load() == disabled_calls);
                const Configuration config{menu.audio, menu.vsync, menu.bindings};
                storage.write_config(encode_config(config));
                const auto saved = storage.read_config();
                CHECK(saved && decode_config(*saved) == config);
                click(Settings::resume);
                CHECK(!menu.opened() && !game.paused);
                CHECK(encode_checkpoint(game.checkpoint()) == checkpoint);
                input({});
                capture("resumed_disabled");
            }
            const auto controls = route.next(game);
            const auto picked = game.update(controls);
            audio->update(game, controls, picked);
            if (steps > 120)
                CHECK(!audio->enabled());
            if (steps % 240 == 0)
                draw();
            if (steps % 16 == 0)
                std::this_thread::sleep_for(1ms);
        }
        CHECK(steps > 120 && game.won && game.count() == 3 && game.door_completions == 1);
        CHECK(!audio->enabled());
        ticks += game.ticks;
        capture("completed_disabled");
        // Explicitly constructing a new session is the current recovery contract.
        audio = std::make_unique<Soundtrack>(files.root, true, "null", menu.audio);
        CHECK(audio->enabled() && audio->settings() == menu.audio);
        audio->restored(game);
        eventually([&] { return audio->stats().active_voices == 1; });
        CHECK(!audio->stats().device_error && !audio->stats().queue_rejections &&
              !audio->stats().stale_controls && !audio->stats().dropped_plays);
        input(press(Key::settings));
        for (const auto id : {Settings::master, Settings::music, Settings::effects, Settings::mute})
            CHECK(widget(id).enabled);
        const auto was_muted = menu.audio.muted;
        CHECK(click(Settings::mute));
        CHECK(menu.audio.muted != was_muted && audio->settings() == menu.audio);
        capture("manual_restart");
        click(Settings::resume);
        CHECK(!menu.opened() && !game.paused && game.won);
        input({});
        std::cout << "[audio-ui-cycle] completed=" << cycle + 1 << " mode=" << mode
                  << " ticks=" << game.ticks << '\n';
        audio.reset();
        flow.advance_level(game, {});
        overlays.replaced(std::move(staged_walls));
    }
    for (const auto& key : textures.keys())
        renderer.release(textures.at(key).gpu);
    renderer.release(target);
    renderer.set_font(embedded);
    CHECK(renderer.memory() == presentation);
    CHECK(renderer.live_textures() == presentation_textures &&
          renderer.live_targets() == presentation_targets);
    const auto elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    std::cout << "[audio-ui-session] cycles=" << cycles << " ticks=" << ticks
              << " renders=" << renders << " captures=" << captures
              << " injected_errors=" << injected_errors.load() << " elapsed_seconds=" << elapsed
              << " presentation_baseline_restored=true\n";
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY"))
        return 77;
    return testing::run_tests();
}
