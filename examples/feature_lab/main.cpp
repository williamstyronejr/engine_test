#include "collision_visuals.hpp"
#include "diagnostics.hpp"
#include "engine/assets.hpp"
#include "engine/audio.hpp"
#include "engine/renderer.hpp"
#include "engine/timing.hpp"
#include "engine/window.hpp"
#include "game.hpp"
#include "overlays.hpp"
#include "persistence.hpp"
#include "scene_flow_draw.hpp"
#include "settings_draw.hpp"
#include "shader_tools.hpp"
#include "soundtrack.hpp"
#include "texture_assets.hpp"
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
using namespace engine;
constexpr Color white{0.8F, 0.9F, 0.94F, 1}, muted{0.24F, 0.36F, 0.43F, 1},
    cyan{0.04F, 0.85F, 0.7F, 1};
using Textures = feature_lab::TextureAssets;
void draw(Renderer& r, const feature_lab::Game& game, const Textures& textures, float alpha,
          int width, int height, Camera camera, const feature_lab::Overlays& overlays,
          const feature_lab::Soundtrack& soundtrack,
          const feature_lab::CollisionVisuals& collision_visuals,
          const feature_lab::Diagnostics& diagnostics, const feature_lab::ShaderTools& shaders,
          bool settings_open, const KeyBindings& bindings) {
    const auto extent = camera.extent(width, height) * 0.5F;
    const Rect view{camera.center - extent, camera.center + extent};
    const auto& map = game.map.data();
    TileVisitStats visible;
    std::size_t layer = 0;
    const auto draw_layer = [&] {
        const auto stats = game.map.visit_visible(layer++, view, [&](TileInstance tile) {
            r.sprite(textures.at(map.texture).gpu,
                     Transform::from(tile.center, 0, {map.tile_size, map.tile_size}), {}, tile.uv);
        });
        visible.chunks += stats.chunks;
        visible.cells += stats.cells;
        visible.tiles += stats.tiles;
    };
    for (const auto entity : game.draw_order) {
        if (!game.world.valid(entity))
            continue;
        const auto& node = game.world.get(entity);
        const auto& sprite = *node.sprite;
        while (layer < map.layers.size() && map.layers[layer].order <= sprite.layer)
            draw_layer();
        auto model = game.world.world_transform(entity);
        Rect uv{{0, 0}, {1, 1}};
        if (node.tag == "player") {
            const auto p = lerp(game.previous, game.position, alpha);
            model.x = p.x;
            model.y = p.y;
            uv = game.player_animation.uv();
        } else if (node.tag == "core")
            uv = game.core_animation.uv();
        else if (node.tag == "machine")
            uv = game.machine_animation.uv();
        else if (node.tag == "exit")
            uv = game.door_animation.uv();
        r.sprite(textures.at(sprite.texture).gpu, model * Transform::from({}, 0, sprite.size),
                 {sprite.r, sprite.g, sprite.b, sprite.a}, uv);
    }
    while (layer < map.layers.size())
        draw_layer();
    collision_visuals.draw(r, game, alpha, bindings);
    diagnostics.draw_world(r, game, camera, width, height);
    const auto goal = game.location(game.exit);
    r.text(goal + Vec2{-0.7F, 1.4F}, 0.045F, game.door_open ? "EXIT OPEN" : "LOCKED", cyan);
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    r.set_camera({{w * 0.5F, -h * 0.5F}, h});
    const float scale = feature_lab::hud_scale(width, height);
    const auto label = [&](float x, float y, float size, std::string_view text, Color color) {
        r.text({x * scale, -y * scale}, size * scale, text, color);
    };
    r.quad({w * 0.5F, -48 * scale}, {w, 96 * scale}, {0.008F, 0.018F, 0.03F, 0.96F});
    label(24, 16, 4, "FEATURE LAB", white);
    label(25, 58, 1.7F, "14 / TITLE AND SCENE TRANSITIONS", cyan);
    const float right = w / scale - 360;
    label(right, 18, 1.8F,
          "CORES  " + std::to_string(game.count()) + " / " + std::to_string(game.total()), cyan);
    label(right, 48, 1.5F,
          game.door_open      ? "EXIT READY"
          : game.door_started ? "DOOR OPENING"
                              : "COLLECT CORES TO OPEN EXIT",
          white);
    r.quad({w * 0.5F, -h + 43 * scale}, {w, 86 * scale}, {0.008F, 0.018F, 0.03F, 0.96F});
    const float bottom = h / scale - 73;
    label(24, bottom, 1.55F,
          bindings.key_name(Key::up) + bindings.key_name(Key::left) + bindings.key_name(Key::down) +
              bindings.key_name(Key::right) + " / ARROWS  MOVE    +/-  ZOOM    SPACE  PAUSE",
          white);
    label(24, bottom + 24, 1.4F,
          bindings.key_name(Key::interact) +
              " SWITCH  F1 SETTINGS  F2 STATS  F3 SHAPES  F10 STEP  ESC QUIT",
          muted);
    label(24, bottom + 46, 1.4F,
          "VISIBLE TILES " + std::to_string(visible.tiles) + " / " +
              std::to_string(map.width * map.height * map.layers.size()) + "    CHUNKS " +
              std::to_string(visible.chunks) + " / " + std::to_string(game.map.chunk_count()) +
              "    COLLIDERS " + std::to_string(game.collision_candidates.size()),
          cyan);
    label(24, bottom - 84, 1.3F, textures.status(),
          textures.applied() ? cyan : Color{1, 0.3F, 0.12F, 1});
    label(24, bottom - 66, 1.3F, shaders.status(),
          shaders.result().applied ? cyan : Color{1, 0.3F, 0.12F, 1});
    label(24, bottom - 44, 1.3F,
          "ALARM ENTRIES " + std::to_string(game.alarm_entries) + " / " +
              (game.alarm_disabled ? "DISABLED" : "ACTIVE") + "    GRID PAIRS " +
              std::to_string(game.collisions.stats().candidate_pairs),
          cyan);
    label(24, bottom - 22, 1.3F, soundtrack.status(bindings), cyan);
    overlays.draw(r, game, width, height);
    if (!settings_open && (game.won || (game.paused && !diagnostics.controls.visible))) {
        r.quad({w * 0.5F, -h * 0.5F}, {650 * scale, 150 * scale}, {0.008F, 0.014F, 0.022F, 0.96F});
        label(w / scale * 0.5F - (game.won ? 240 : 90), h / scale * 0.5F - 35, 3,
              game.won ? "FACILITY COMPLETE" : "PAUSED", cyan);
        label(w / scale * 0.5F - 140, h / scale * 0.5F + 20, 1.5F,
              game.won ? "PRESS " + bindings.key_name(Key::restart) + " TO PLAY AGAIN"
                       : "SPACE RESUME / F10 STEP",
              white);
    }
}
int verify(const AssetRoot& assets) {
    auto a = feature_lab::load_game(assets);
    feature_lab::Game b(
        serialize_scene(parse_scene(assets.text("facility.scene"))),
        decode_tilemap(encode_tilemap(a.map.data())),
        std::make_shared<const AnimationSet>(decode_animations(encode_animations(*a.animations))));
    std::vector<Rect> obstacles;
    const auto original_player = a.player;
    feature_lab::Replay replay;
    for (int i = 0; i < 2400 && !a.won; ++i) {
        const auto input = replay.next(a);
        a.update(input);
        b.update(input);
        if (a.position.x != b.position.x || a.position.y != b.position.y ||
            a.collected != b.collected || a.door_open != b.door_open ||
            a.player_animation.frame_index() != b.player_animation.frame_index() ||
            a.door_animation.frame_index() != b.door_animation.frame_index())
            throw std::runtime_error("Replay diverged");
        const Rect player{a.position - Vec2{0.299F, 0.299F}, a.position + Vec2{0.299F, 0.299F}};
        obstacles.clear();
        a.append_obstacles(player, obstacles);
        for (const auto& wall : obstacles)
            if (overlaps(player, wall))
                throw std::runtime_error("Replay penetrated a wall");
    }
    if (!a.won || !b.won || a.door_completions != 1 || b.door_completions != 1) {
        std::cout << "{\"scenario\":\"collect_and_exit\",\"passed\":false}\n";
        return 1;
    }
    const auto completed_ticks = a.ticks;
    const auto completed_count = a.count();
    for (const auto handle : a.keys)
        if (a.world.valid(handle))
            throw std::runtime_error("Collected entity was not destroyed");
    InputFrame restart{};
    restart[static_cast<std::size_t>(Key::restart)].pressed = true;
    a.update(restart);
    if (a.world.valid(original_player) || a.count() != 0 || a.door_open || a.door_completions != 0)
        throw std::runtime_error("Restart retained stale entity state");
    std::cout << "{\"scenario\":\"collect_and_exit\",\"passed\":true,\"collected\":"
              << completed_count << ",\"ticks\":" << completed_ticks << "}\n";
    return 0;
}
} // namespace
int main(int argc, char** argv) {
    try {
        int frame_limit = 0;
        unsigned load_slot = 0, save_slot = 0;
        bool vsync_override = false;
        std::string user_directory;
        bool audio_enabled = true, scripted = false, verification = false, vsync = true;
        std::string screenshot, asset_directory;
        bool shader_error_demo = false, texture_error_demo = false, direct_play = false;
        bool missing_texture_demo = false, settings_demo = false, diagnostics_demo = false;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg(argv[i]);
            if (arg == "--assets" && i + 1 < argc)
                asset_directory = argv[++i];
            else if (arg == "--user-data" && i + 1 < argc)
                user_directory = argv[++i];
            else if ((arg == "--load-slot" || arg == "--save-slot") && i + 1 < argc) {
                const std::string_view value(argv[++i]);
                unsigned slot{};
                const auto [end, error] =
                    std::from_chars(value.data(), value.data() + value.size(), slot);
                if (error != std::errc{} || end != value.data() + value.size() || slot < 1 ||
                    slot > 3)
                    throw std::invalid_argument("Slot must be 1..3");
                if (arg == "--load-slot")
                    load_slot = slot;
                else
                    save_slot = slot;
            } else if (arg == "--missing-texture-demo")
                missing_texture_demo = true;
            else if (arg == "--settings-demo")
                settings_demo = true;
            else if (arg == "--diagnostics-demo")
                diagnostics_demo = true;
            else if (arg == "--shader-error-demo")
                shader_error_demo = true;
            else if (arg == "--texture-error-demo")
                texture_error_demo = true;
            else if (arg == "--play")
                direct_play = true;
            else if (arg == "--verify")
                verification = true;
            else if (arg == "--no-audio")
                audio_enabled = false;
            else if (arg == "--no-vsync") {
                vsync = false;
                vsync_override = true;
            } else if (arg == "--scripted")
                scripted = true;
            else if (arg == "--frames" && i + 1 < argc) {
                const std::string_view value(argv[++i]);
                const auto [end, error] =
                    std::from_chars(value.data(), value.data() + value.size(), frame_limit);
                if (error != std::errc{} || end != value.data() + value.size() ||
                    frame_limit <= 0 || frame_limit > 1000000)
                    throw std::invalid_argument("--frames requires an integer in 1..1000000");
            } else if (arg == "--screenshot" && i + 1 < argc)
                screenshot = argv[++i];
            else if (arg == "--help") {
                std::cout << "Feature Lab defaults: WASD/arrows move, +/- zoom, Space pause, R "
                             "restart, F11 "
                             "fullscreen, PageUp/PageDown scroll status, M mute, N/B music/effects "
                             "volume, E nearby alarm switch, F1 settings/rebind controls, F2 "
                             "diagnostics, F3 debug "
                             "shapes, "
                             "F5 reload shaders, F6 shader failure demo, F7 reload textures, "
                             "F8 texture failure demo, F10 single step while "
                             "paused, Escape close settings/quit.\n"
                             "--play (skip title) --verify (CPU gameplay test) --scripted --frames "
                             "N --no-audio "
                             "--no-vsync --screenshot FILE.ppm --assets DIRECTORY "
                             "--missing-texture-demo --settings-demo --diagnostics-demo "
                             "--shader-error-demo --texture-error-demo --user-data DIRECTORY "
                             "--load-slot 1..3 --save-slot 1..3 (on exit)\n";
                return 0;
            } else
                throw std::invalid_argument("Unknown/incomplete option: " + std::string(arg));
        }
        const AssetRoot assets = AssetRoot::discover(asset_directory);
        if (verification)
            return verify(assets);
        auto game = feature_lab::load_game(assets);
        feature_lab::SceneFlow flow(game,
                                    !(direct_play || scripted || load_slot || save_slot ||
                                      settings_demo || diagnostics_demo || missing_texture_demo ||
                                      shader_error_demo || texture_error_demo));
        std::unique_ptr<UserStorage> storage;
        feature_lab::Configuration config;
        std::string persistence_notice;
        try {
            storage = std::make_unique<UserStorage>(UserPaths::discover(user_directory));
            if (const auto bytes = storage->read_config())
                config = feature_lab::decode_config(*bytes);
        } catch (const std::exception& error) {
            std::cerr << "[persistence] " << error.what() << '\n';
            persistence_notice = "CONFIG UNAVAILABLE - USING DEFAULTS";
        }
        if (!vsync_override)
            vsync = config.vsync;
        if (load_slot) {
            if (!storage)
                throw std::runtime_error("User storage unavailable");
            feature_lab::load_slot(*storage, load_slot, game);
            scripted = false; // A restored human checkpoint has no scripted route cursor.
            std::cout << "[persistence] loaded slot=" << load_slot << " ticks=" << game.ticks
                      << '\n';
        }
        if (!screenshot.empty() && frame_limit == 0)
            frame_limit = 120;
        Window window(1280, 720, "Feature Lab | Linux 2D Engine");
        const bool vsync_available = window.set_vsync(vsync);
        if (!vsync_available)
            std::cerr << "[platform] Requested VSync control unavailable\n";
        Renderer renderer;
        GpuTimer gpu_timer;
        feature_lab::ShaderTools shaders;
        shaders.reload(renderer, assets);
        if (shader_error_demo)
            shaders.fail_demo(renderer);
        feature_lab::Overlays overlays(renderer, game);
        feature_lab::CollisionVisuals collision_visuals(renderer);
        feature_lab::Diagnostics diagnostics;
        diagnostics.controls = {diagnostics_demo, diagnostics_demo};
        Textures textures(assets);
        const auto load_texture = [&](const std::string& key, std::uint32_t columns,
                                      std::uint32_t rows) {
            textures.load(renderer, key, columns, rows,
                          missing_texture_demo && key == game.map.data().texture);
        };
        load_texture(game.map.data().texture, game.map.data().atlas_columns,
                     game.map.data().atlas_rows);
        load_texture(game.animations->texture, game.animations->atlas_columns,
                     game.animations->atlas_rows);
        game.world.each([&](const SceneNode& node) {
            if (node.sprite)
                load_texture(node.sprite->texture, 1, 1);
        });
        if (texture_error_demo)
            textures.reload(renderer, true);
        std::cout << "[assets] " << assets.directory() << " entities=" << game.world.size()
                  << " GPU textures=" << renderer.live_textures() << '\n';
        feature_lab::Soundtrack soundtrack(assets, audio_enabled, "default", config.audio,
                                           game.paused);
        soundtrack.locate_emitter(game);
        if (load_slot)
            soundtrack.restored(game);
        feature_lab::Replay replay;
        Input input;
        window.set_bindings(config.bindings, input);
        feature_lab::Settings settings;
        settings.audio = config.audio;
        settings.bindings = config.bindings;
        settings.notice = persistence_notice;
        settings.vsync = vsync;
        settings.vsync_available = vsync_available;
        bool config_dirty = false;
        const auto persist_config = [&] {
            if (!config_dirty)
                return;
            try {
                if (!storage)
                    throw std::runtime_error("User storage unavailable");
                storage->write_config(feature_lab::encode_config(config));
                config_dirty = false;
                settings.notice = "SETTINGS SAVED";
            } catch (const std::exception& error) {
                std::cerr << "[persistence] " << error.what() << '\n';
                settings.notice = "SETTINGS SAVE FAILED";
            }
        };
        if (settings_demo) {
            InputFrame open{};
            open[static_cast<std::size_t>(Key::settings)].pressed = true;
            settings.update(game, open, window.width(), window.height(), soundtrack.enabled());
        }
        FixedClock clock;
        using Clock = std::chrono::steady_clock;
        auto last = Clock::now(), report = last;
        std::size_t frames = 0, report_frames = 0;
        std::array<double, 8192> cpu_samples{};
        std::size_t sample_count = 0;
        double dropped = 0;
        bool running = true;
        while (running && window.poll(input)) {
            const auto now = Clock::now();
            const double elapsed = std::chrono::duration<double>(now - last).count();
            auto batch = clock.advance(elapsed);
            unsigned simulation_steps = 0;
            last = now;
            if (!scripted)
                dropped += batch.dropped;
            if (scripted && !window.drawable()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if (scripted) {
                batch.steps = 1;
                batch.alpha = 1;
            }
            for (int step = 0; step < batch.steps; ++step) {
                const auto routed = flow.update(game, input.consume(), window.width(),
                                                window.height(), storage.get());
                if (routed.quit) {
                    running = false;
                    break;
                }
                if (routed.transitioned) {
                    if (routed.replaced) {
                        soundtrack.restored(game);
                        replay = feature_lab::Replay{};
                        scripted = false;
                        overlays.reset();
                    } else {
                        soundtrack.update(game, {}, 0);
                    }
                    std::cout << "[scene] gameplay ticks=" << game.ticks << '\n';
                    continue;
                }
                if (flow.title_open())
                    continue;
                const auto physical = routed.gameplay;
                if (!settings.opened())
                    settings.audio = soundtrack.settings();
                const bool was_open = settings.opened();
                const auto menu = settings.update(game, physical, window.width(), window.height(),
                                                  soundtrack.enabled());
                if (menu.bindings_changed) {
                    window.set_bindings(settings.bindings, input);
                    config.bindings = settings.bindings;
                    config_dirty = true;
                    std::cout << "[bindings] updated" << '\n';
                }
                if (menu.title) {
                    flow.enter_title(game);
                    soundtrack.update(game, {}, 0);
                    persist_config();
                    std::cout << "[scene] title ticks=" << game.ticks << '\n';
                    continue;
                }
                const auto& controls = menu.gameplay;
                overlays.input(controls);
                diagnostics.controls.update(controls);
                shaders.update(renderer, assets, controls);
                textures.update(renderer, controls);
                if (menu.quit || button(controls, Key::escape).pressed)
                    running = false;
                if (menu.fullscreen || button(controls, Key::fullscreen).pressed)
                    window.toggle_fullscreen();
                if (menu.vsync_changed) {
                    if (window.set_vsync(settings.vsync)) {
                        vsync = settings.vsync;
                        config.vsync = vsync;
                        config_dirty = true;
                    } else
                        settings.reject_vsync(vsync);
                }
                if (menu.audio_changed)
                    soundtrack.configure(settings.audio);
                if (menu.save || menu.load) {
                    try {
                        if (!storage)
                            throw std::runtime_error("User storage unavailable");
                        if (menu.save) {
                            feature_lab::save_slot(*storage, settings.selected_slot, game,
                                                   settings.gameplay_paused(game));
                            settings.notice =
                                "SAVED SLOT " + std::to_string(settings.selected_slot);
                        } else {
                            feature_lab::load_slot(*storage, settings.selected_slot, game);
                            settings.loaded(game);
                            soundtrack.restored(game);
                            scripted = false;
                            settings.notice =
                                "LOADED SLOT " + std::to_string(settings.selected_slot);
                        }
                    } catch (const std::exception& error) {
                        std::cerr << "[persistence] " << error.what() << '\n';
                        settings.notice =
                            menu.save ? "SAVE FAILED - SEE LOG" : "LOAD FAILED - SEE LOG";
                    }
                }
                const bool restarting = button(controls, Key::restart).pressed;
                if (restarting)
                    replay = feature_lab::Replay{};
                auto gameplay = controls;
                if (scripted && !menu.captured && !restarting) {
                    // Debug/pause controls also work during scripted visual inspection.
                    const bool toggling = button(controls, Key::pause).pressed ||
                                          button(controls, Key::space).pressed;
                    const bool advancing =
                        toggling ? game.paused
                                 : (!game.paused || button(controls, Key::single_step).pressed);
                    if (advancing) {
                        const auto route = replay.next(game);
                        for (const auto key : {Key::left, Key::right, Key::up, Key::down})
                            gameplay[static_cast<std::size_t>(key)] = button(route, key);
                    }
                }
                const auto ticks_before = game.ticks;
                const int picked = game.update(gameplay);
                if (game.ticks > ticks_before)
                    ++simulation_steps;
                soundtrack.update(game, controls, picked);
                const auto audio = soundtrack.settings();
                if (!(config.audio == audio)) {
                    config.audio = audio;
                    config_dirty = true;
                }
                if (was_open && !settings.opened())
                    persist_config();
            }
            if (!window.drawable()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            diagnostics.collect_gpu(gpu_timer, frames + 1);
            gpu_timer.begin(frames + 1);
            const auto camera = game.camera(window.width(), window.height(), batch.alpha);
            if (!flow.title_open())
                overlays.render_minimap(renderer, game, window.width(), window.height(),
                                        batch.alpha);
            renderer.begin(window.width(), window.height(), camera);
            if (flow.title_open()) {
                flow.prepare(window.width(), window.height(), storage != nullptr);
                flow.draw(renderer, window.width(), window.height());
            } else {
                draw(renderer, game, textures, batch.alpha, window.width(), window.height(), camera,
                     overlays, soundtrack, collision_visuals, diagnostics, shaders,
                     settings.opened(), settings.bindings);
                diagnostics.draw_hud(renderer, game, window.width(), window.height(), dropped);
                settings.draw(renderer, window.width(), window.height());
            }
            renderer.end();
            gpu_timer.end();
            ++frames;
            ++report_frames;
            const auto submitted = Clock::now();
            const double cpu_ms =
                std::chrono::duration<double, std::milli>(submitted - now).count();
            diagnostics.record({elapsed * 1000, cpu_ms}, renderer.stats(), overlays.minimap_stats(),
                               simulation_steps);
            if (frames > 30 && sample_count < cpu_samples.size())
                cpu_samples[sample_count++] = cpu_ms;
            if (!screenshot.empty() && frames == static_cast<std::size_t>(frame_limit))
                renderer.screenshot(screenshot);
            window.present();
            if (!renderer.healthy())
                throw std::runtime_error("OpenGL validation failed");
            const double interval = std::chrono::duration<double>(now - report).count();
            if (interval >= 1) {
                const auto stats = renderer.stats();
                window.title("Feature Lab | " +
                             std::to_string(
                                 static_cast<int>(static_cast<double>(report_frames) / interval)) +
                             " FPS | " + std::to_string(stats.quads) + " quads | " +
                             std::to_string(stats.draws) + " draws");
                report = now;
                report_frames = 0;
            }
            if (frame_limit > 0 && frames >= static_cast<std::size_t>(frame_limit))
                break;
            if (!vsync)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        persist_config();
        if (save_slot) {
            if (!storage)
                throw std::runtime_error("User storage unavailable");
            feature_lab::save_slot(
                *storage, save_slot, game,
                (flow.title_open() ? flow.gameplay_paused(game) : settings.gameplay_paused(game)));
            std::cout << "[persistence] saved slot=" << save_slot << " ticks=" << game.ticks
                      << '\n';
        }
        std::cout << "[session] frames=" << frames << " ticks=" << game.ticks
                  << " door_completions=" << game.door_completions << " collected=" << game.count()
                  << " dropped_seconds=" << dropped << " window_draws=" << renderer.stats().draws
                  << " minimap_draws=" << overlays.minimap_stats().draws
                  << " scene=" << (flow.title_open() ? "title" : "gameplay")
                  << " texture_revision=" << textures.revision()
                  << " texture_applied=" << textures.applied()
                  << " shader_revision=" << renderer.shader_revision()
                  << " shader_applied=" << shaders.result().applied
                  << " targets=" << renderer.live_targets()
                  << " target_bytes=" << renderer.target_bytes() << '\n';
        diagnostics.collect_gpu(gpu_timer, frames); // Final poll only; never wait on shutdown.
        const auto gpu = diagnostics.gpu_summary();
        const auto gpu_stats = gpu_timer.stats();
        std::cout << "[gpu render interval ms] supported=" << gpu_stats.supported
                  << " counter_bits=" << gpu_stats.counter_bits << " mean=" << gpu.mean_ms
                  << " p95=" << gpu.p95_ms << " max=" << gpu.max_ms << " samples=" << gpu.samples
                  << " last_frame=" << gpu.last_frame
                  << " age_frames=" << (gpu.samples ? frames - gpu.last_frame : 0)
                  << " submitted=" << gpu_stats.submitted << " completed=" << gpu_stats.completed
                  << " pending=" << gpu_stats.pending << " skipped=" << gpu_stats.skipped
                  << " invalid=" << gpu_stats.invalid << '\n';
        const auto audio_stats = soundtrack.stats();
        std::cout << "[audio stats] enabled=" << soundtrack.enabled()
                  << " started=" << audio_stats.started << " completed=" << audio_stats.completed
                  << " dropped=" << audio_stats.dropped_plays
                  << " queue_rejections=" << audio_stats.queue_rejections << '\n';
        if (sample_count > 0) {
            std::sort(cpu_samples.begin(),
                      cpu_samples.begin() + static_cast<std::ptrdiff_t>(sample_count));
            const auto percentile = [&](std::size_t percent) {
                return cpu_samples[(sample_count - 1) * percent / 100];
            };
            std::cout << "[cpu update+submit ms] median=" << percentile(50)
                      << " p95=" << percentile(95) << " p99=" << percentile(99)
                      << " samples=" << sample_count << " viewport=" << window.width() << 'x'
                      << window.height() << " (excludes swap wait; not GPU time)\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[fatal] " << e.what() << '\n';
        return 1;
    }
}
