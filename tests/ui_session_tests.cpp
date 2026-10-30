#include "../examples/feature_lab/game_draw.hpp"
#include "../examples/feature_lab/scene_flow.hpp"
#include "../examples/feature_lab/settings_draw.hpp"
#include "engine/window.hpp"
#include "scene_fixture.hpp"
#include <chrono>
#include <thread>

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
TEST(repeated_full_game_ui_transitions_preserve_state_and_owned_payloads) {
    SceneFiles files;
    UserStorage storage(UserPaths::discover(files.path / "user"));
    Window window{64, 64, "Full UI session regression", false};
    Renderer renderer;
    // Normalize copied font capacity before taking a restoration baseline.
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
    Soundtrack audio(files.root, true, "null", menu.audio);
    CHECK(audio.enabled());
    Diagnostics diagnostics;
    for (unsigned i = 0; i < 240; ++i)
        diagnostics.record({16.75, 2.25}, {1234, 12, 8}, {456, 0, 3}, 1);
    SceneFlow flow(game, false, &files.root);
    std::vector<Rect> staged_walls;
    flow.prepare_replacement = [&](const Game& candidate) {
        textures.validate_level(candidate);
        staged_walls = Overlays::stage(candidate);
    };
    int width = 640, height = 480;
    std::uint64_t ticks = 0, renders = 0, captures = 0;
    std::string text;
    text.reserve(256);
    const auto started = std::chrono::steady_clock::now();
    const auto draw = [&] {
        overlays.render_minimap(renderer, game, width, height, 1);
        CHECK(overlays.minimap_stats().quads > 0);
        const auto camera = game.camera(width, height, 1);
        renderer.begin(target, camera);
        draw_game(renderer, game, textures, 1, width, height, camera, overlays, audio,
                  collision_visuals, sampling_gallery, diagnostics, shaders, fonts, animations,
                  tilemaps, scenes, menu.opened(), menu.bindings, "UI SESSION", false, false, text);
        diagnostics.draw_hud(renderer, game, width, height, 0.125, text);
        menu.draw(renderer, width, height);
        renderer.end();
        CHECK(renderer.stats().quads > 100 && renderer.stats().draws > 0 && renderer.healthy());
        CHECK(renderer.live_textures() == texture_count && renderer.live_targets() == target_count);
        auto expected = resident;
        // Exact RGBA8 payload for the output and production minimap targets.
        const auto map_height = static_cast<std::size_t>(std::ceil(138 * hud_scale(width, height)));
        expected.gpu_target_texture_bytes =
            (static_cast<std::size_t>(width * height) + 2 * map_height * map_height) * 4;
        CHECK(renderer.memory() == expected);
        ++renders;
    };
    const auto capture = [&](unsigned cycle, std::string_view state) {
        draw();
        renderer.screenshot((files.path / "frame.ppm").string());
        const auto bytes = files.root.read("frame.ppm", 4 * 1024 * 1024);
        std::uint64_t hash = 1469598103934665603ULL;
        for (auto byte : bytes) {
            hash ^= byte;
            hash *= 1099511628211ULL;
        }
        CHECK(bytes.size() > static_cast<std::size_t>(width * height * 3));
        ++captures;
        std::cout << "[ui-frame] cycle=" << cycle + 1 << " state=" << state << " width=" << width
                  << " height=" << height << " hash=" << hash << '\n';
        return hash;
    };
    const auto input = [&](const InputFrame& frame) {
        const auto result = menu.update(game, frame, width, height, audio.enabled());
        diagnostics.controls.update(result.gameplay);
        if (result.audio_changed)
            audio.configure(menu.audio);
        audio.update(game, {}, 0);
        CHECK(audio.enabled());
        return result;
    };
    const auto click = [&](UiId id) {
        const auto widgets = menu.ui().widgets();
        const auto found = std::find_if(widgets.begin(), widgets.end(),
                                        [&](const UiWidget& widget) { return widget.id == id; });
        CHECK(found != widgets.end());
        InputFrame frame{};
        frame.pointer.position = (found->bounds.min + found->bounds.max) * .5F;
        frame.pointer.press_position = frame.pointer.release_position = frame.pointer.position;
        frame.pointer.inside = true;
        frame.pointer.primary = {true, true, false};
        input(frame);
        frame.pointer.primary = {false, false, true};
        return input(frame);
    };
    for (unsigned cycle = 0; cycle < cycles; ++cycle) {
        CHECK(game.level == (cycle % 2 ? Level::annex : Level::facility));
        audio.restored(game);
        eventually([&] { return audio.stats().active_voices == 2; });
        Replay route;
        unsigned steps = 0;
        for (; steps < max_steps && !game.won; ++steps) {
            if (steps == 120) {
                const auto checkpoint = encode_checkpoint(game.checkpoint());
                const auto baseline_hash = capture(cycle, "gameplay");
                input(press(Key::diagnostics));
                CHECK(diagnostics.controls.visible);
                CHECK(capture(cycle, "diagnostics") != baseline_hash);
                input(press(Key::debug_shapes));
                CHECK(diagnostics.controls.shapes);
                capture(cycle, "shapes");
                auto opening = press(Key::settings);
                opening[static_cast<std::size_t>(Key::right)].held = true;
                const auto opened = input(opening);
                CHECK(menu.opened() && game.paused && opened.captured);
                CHECK(!button(opened.gameplay, Key::right).held);
                game.update(opened.gameplay);
                capture(cycle, "settings");
                // A modal diagnostics key must not change the underlying overlay.
                input(press(Key::diagnostics));
                CHECK(diagnostics.controls.visible);
                const auto was_muted = menu.audio.muted;
                click(Settings::mute);
                CHECK(menu.audio.muted != was_muted && audio.settings() == menu.audio);
                capture(cycle, "muted");
                click(Settings::controls);
                CHECK(menu.controls_opened());
                capture(cycle, "controls");
                click(Settings::binding_first);
                CHECK(menu.capturing_binding());
                capture(cycle, "capture");
                input(press(Key::escape));
                CHECK(!menu.capturing_binding() && menu.controls_opened() && menu.opened());
                capture(cycle, "canceled");
                click(Settings::binding_first);
                InputFrame binding{};
                binding.pressed_symbol = cycle % 2 ? 0xf1 : 0xf6;
                binding.pressed_modifiers = modifiers::control | modifiers::shift;
                CHECK(input(binding).bindings_changed && !menu.capturing_binding());
                CHECK(menu.bindings.symbols[0] == binding.pressed_symbol);
                CHECK(menu.bindings.modifier_masks[0] == binding.pressed_modifiers);
                capture(cycle, "rebound");
                click(Settings::controls_back);
                CHECK(!menu.controls_opened() && menu.opened());
                capture(cycle, "settings_return");
                click(Settings::resume);
                CHECK(!menu.opened() && !game.paused);
                CHECK(encode_checkpoint(game.checkpoint()) == checkpoint);
                input({}); // Release modal input gates before fresh toggles.
                input(press(Key::diagnostics));
                input(press(Key::debug_shapes));
                CHECK(!diagnostics.controls.visible && !diagnostics.controls.shapes);
                const Configuration config{menu.audio, menu.vsync, menu.bindings};
                storage.write_config(encode_config(config));
                const auto saved = storage.read_config();
                CHECK(saved && decode_config(*saved) == config);
                capture(cycle, "resumed");
                width = 320;
                height = 240;
                renderer.resize_target(target, width, height);
                capture(cycle, "small_viewport");
                width = 640;
                height = 480;
                renderer.resize_target(target, width, height);
                capture(cycle, "restored_viewport");
                const auto original = files.root.read(FontAssets::key, BitmapFont::max_file_bytes);
                const auto revision = fonts.revision();
                files.root.write_text(FontAssets::key, "invalid font");
                CHECK(!fonts.reload(renderer, files.root) && fonts.revision() == revision);
                CHECK(renderer.memory() == resident);
                capture(cycle, "font_retained");
                files.root.write_atomic(FontAssets::key, original);
                CHECK(fonts.reload(renderer, files.root) && fonts.revision() == revision + 1);
                capture(cycle, "font_repaired");
                CHECK(renderer.memory() == resident);
            }
            const auto controls = route.next(game);
            const auto picked = game.update(controls);
            audio.update(game, controls, picked);
            CHECK(audio.enabled());
            if (steps % 240 == 0)
                draw();
            if (steps % 16 == 0)
                std::this_thread::sleep_for(1ms);
        }
        CHECK(steps > 120 && game.won && game.count() == 3 && game.door_completions == 1);
        ticks += game.ticks;
        draw();
        eventually([&] { return audio.stats().active_voices == 1; });
        const auto stats = audio.stats();
        CHECK(!stats.queue_rejections && !stats.stale_controls && !stats.dropped_plays &&
              !stats.device_error);
        CHECK(renderer.memory() == resident);
        std::cout << "[ui-cycle] completed=" << cycle + 1 << " ticks=" << game.ticks << '\n';
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
    std::cout << "[ui-session] cycles=" << cycles << " ticks=" << ticks << " renders=" << renders
              << " captures=" << captures << " elapsed_seconds=" << elapsed
              << " presentation_baseline_restored=true\n";
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY"))
        return 77;
    return testing::run_tests();
}
