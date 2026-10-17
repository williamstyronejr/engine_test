#include "animation_assets.hpp"
#include "asset_reload.hpp"
#include "collision_visuals.hpp"
#include "diagnostics.hpp"
#include "engine/assets.hpp"
#include "engine/audio.hpp"
#include "engine/renderer.hpp"
#include "engine/timing.hpp"
#include "engine/window.hpp"
#include "font_gallery.hpp"
#include "game.hpp"
#include "overlays.hpp"
#include "persistence.hpp"
#include "replay_file.hpp"
#include "replay_profile.hpp"
#include "spike_profile.hpp"
#include "sampling_gallery.hpp"
#include "scene_assets.hpp"
#include "scene_flow_draw.hpp"
#include "settings_draw.hpp"
#include "shader_tools.hpp"
#include "soundtrack.hpp"
#include "stress_benchmark.hpp"
#include "texture_assets.hpp"
#include "tilemap_assets.hpp"
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
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
          const feature_lab::SamplingGallery& sampling_gallery,
          const feature_lab::Diagnostics& diagnostics, const feature_lab::ShaderTools& shaders,
          const feature_lab::FontAssets& fonts, const feature_lab::AnimationAssets& animations,
          const feature_lab::TilemapAssets& tilemaps, const feature_lab::SceneAssets& scenes,
          bool settings_open, const KeyBindings& bindings, std::string_view replay_status,
          bool replay_session,
          bool playing_replay,
          std::chrono::steady_clock::time_point* world_submitted = nullptr) {
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
    bool gallery_drawn = false;
    const auto draw_gallery = [&] {
        // Insert the visual station at layer 5, below the player and foreground tiles.
        while (layer < map.layers.size() && map.layers[layer].order <= 5)
            draw_layer();
        sampling_gallery.draw();
        gallery_drawn = true;
    };
    for (const auto entity : game.draw_order) {
        if (!game.world.valid(entity))
            continue;
        const auto& node = game.world.get(entity);
        const auto& sprite = *node.sprite;
        if (!gallery_drawn && sprite.layer >= 5)
            draw_gallery();
        while (layer < map.layers.size() && map.layers[layer].order <= sprite.layer)
            draw_layer();
        const auto model = game.render_transform(entity, alpha);
        Rect uv{{0, 0}, {1, 1}};
        if (node.tag == "player") {
            uv = game.player_animation.uv();
        } else if (node.tag == "core")
            uv = game.core_animation.uv();
        else if (node.tag == "machine")
            uv = game.machine_animation.uv();
        else if (node.tag == "exit")
            uv = game.door_animation.uv();
        const float indicator = node.tag == "machine"
                                    ? 0.5F + static_cast<float>(game.random.value() >> 56) / 510.0F
                                    : 1.0F;
        r.sprite(textures.at(sprite.texture).gpu, model * Transform::from({}, 0, sprite.size),
                 {sprite.r * indicator, sprite.g * indicator, sprite.b * indicator, sprite.a}, uv);
        if (node.tag == "rotor")
            r.text({model.x - 1.8F, model.y + 2.7F}, 0.025F, "ROTATE / SCALE / PARENT", cyan);
    }
    if (!gallery_drawn)
        draw_gallery();
    while (layer < map.layers.size())
        draw_layer();
    collision_visuals.draw(r, game, alpha, bindings);
    diagnostics.draw_world(r, game, camera, width, height);
    const auto goal = game.location(game.exit);
    r.text(goal + Vec2{-0.7F, 1.4F}, 0.045F, game.door_open ? "EXIT OPEN" : "LOCKED", cyan);
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    r.set_camera({{w * 0.5F, -h * 0.5F}, h});
    if (world_submitted)
        *world_submitted = std::chrono::steady_clock::now();
    const float scale = feature_lab::hud_scale(width, height);
    const auto label = [&](float x, float y, float size, std::string_view text, Color color) {
        r.text({x * scale, -y * scale}, size * scale, text, color);
    };
    r.quad({w * 0.5F, -48 * scale}, {w, 96 * scale}, {0.008F, 0.018F, 0.03F, 0.96F});
    label(24, 16, 4, "FEATURE LAB", white);
    label(25, 58, 1.7F, replay_status, cyan);
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
          playing_replay ? "RECORDED GAMEPLAY / LIVE MOVEMENT AND PAUSE DISABLED"
                         : bindings.key_name(Key::up) + "/" + bindings.key_name(Key::left) + "/" +
                               bindings.key_name(Key::down) + "/" + bindings.key_name(Key::right) +
                               " / ARROWS  MOVE    +/-  ZOOM    SPACE  PAUSE",
          white);
    label(24, bottom + 24, 1.4F,
          playing_replay
              ? "INPUT FROM REPLAY  F11 FULLSCREEN  ESC EXIT"
              : bindings.key_name(Key::interact) +
                    (replay_session
                         ? " SWITCH  F1 LOCKED  F10 STEP  ESC SAVE REPLAY"
                         : " SWITCH  F1 SETTINGS  F2 STATS  F3 SHAPES  F10 STEP  ESC QUIT"),
          muted);
    label(24, bottom + 46, 1.4F,
          "VISIBLE TILES " + std::to_string(visible.tiles) + " / " +
              std::to_string(map.width * map.height * map.layers.size()) + "    CHUNKS " +
              std::to_string(visible.chunks) + " / " + std::to_string(game.map.chunk_count()) +
              "    COLLIDERS " + std::to_string(game.collision_candidates.size()),
          cyan);
    label(24, bottom - 174, 1.3F, soundtrack.reload_status(),
          soundtrack.reload_applied() ? cyan : Color{1, 0.3F, 0.12F, 1});
    label(24, bottom - 156, 1.3F, scenes.status(),
          scenes.applied() ? cyan : Color{1, 0.3F, 0.12F, 1});
    label(24, bottom - 138, 1.3F, tilemaps.status(),
          tilemaps.applied() ? cyan : Color{1, 0.3F, 0.12F, 1});
    label(24, bottom - 120, 1.3F, animations.status(),
          animations.applied() ? cyan : Color{1, 0.3F, 0.12F, 1});
    label(24, bottom - 102, 1.3F, fonts.status(),
          fonts.applied() ? cyan : Color{1, 0.3F, 0.12F, 1});
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
              game.won ? std::string(feature_lab::level_name(game.level)) + " COMPLETE" : "PAUSED",
              cyan);
        label(w / scale * 0.5F - 140, h / scale * 0.5F + 20, 1.5F,
              playing_replay                ? "RECORDED STATE / ESC EXIT"
              : game.won && !replay_session ? bindings.key_name(Key::interact) + " NEXT / " +
                                                  bindings.key_name(Key::restart) + " RESTART"
              : game.won ? "PRESS " + bindings.key_name(Key::restart) + " TO PLAY AGAIN"
                         : "SPACE RESUME / F10 STEP",
              white);
    }
}
int verify(const AssetRoot& assets, feature_lab::Level level) {
    auto a = feature_lab::load_game(assets, level);
    feature_lab::Game b(
        serialize_scene(parse_scene(
            assets.text(level == feature_lab::Level::facility ? "facility.scene" : "annex.scene"))),
        decode_tilemap(encode_tilemap(a.map.data())),
        std::make_shared<const AnimationSet>(decode_animations(encode_animations(*a.animations))),
        level);
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
        for (int i = 1; i < argc; ++i)
            if (std::string_view(argv[i]) == "--stress")
                return feature_lab::run_stress_mode(argc, argv);
        int frame_limit = 0;
        unsigned load_slot = 0, save_slot = 0;
        bool vsync_override = false;
        auto selected_level = feature_lab::Level::facility;
        bool level_tour = false;
        bool watch_assets = false;
        bool font_demo = false;
        unsigned level_changes = 0;
        std::string user_directory, record_path, playback_path, verify_path, profile_path;
        std::size_t profile_warmup = 30;
        bool profile_requested = false, profile_warmup_set = false, spike_requested = false;
        std::optional<std::uint64_t> seed;
        bool audio_enabled = true, scripted = false, verification = false, vsync = true;
        std::string screenshot, asset_directory;
        bool shader_error_demo = false, texture_error_demo = false, direct_play = false;
        bool missing_texture_demo = false, settings_demo = false, diagnostics_demo = false;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg(argv[i]);
            if (arg == "--assets" && i + 1 < argc)
                asset_directory = argv[++i];
            else if (arg == "--font-demo")
                font_demo = true;
            else if (arg == "--watch-assets")
                watch_assets = true;
            else if (arg == "--level" && i + 1 < argc) {
                const std::string_view name(argv[++i]);
                if (name == "facility")
                    selected_level = feature_lab::Level::facility;
                else if (name == "annex")
                    selected_level = feature_lab::Level::annex;
                else
                    throw std::invalid_argument("--level must be facility or annex");
                direct_play = true;
            } else if (arg == "--level-tour") {
                level_tour = scripted = true;
            } else if (arg == "--spike-report" && i + 1 < argc) {
                profile_path = argv[++i];
                spike_requested = true;
            } else if (arg == "--profile-report" && i + 1 < argc) {
                profile_path = argv[++i];
                profile_requested = true;
            } else if (arg == "--profile-warmup" && i + 1 < argc) {
                const std::string_view value(argv[++i]);
                const auto [end, error] =
                    std::from_chars(value.data(), value.data() + value.size(), profile_warmup);
                if (error != std::errc{} || end != value.data() + value.size() ||
                    profile_warmup > 35999)
                    throw std::invalid_argument("--profile-warmup requires 0..35999");
                profile_warmup_set = true;
            } else if (arg == "--record-replay" && i + 1 < argc)
                record_path = argv[++i];
            else if (arg == "--replay" && i + 1 < argc)
                playback_path = argv[++i];
            else if (arg == "--verify-replay" && i + 1 < argc)
                verify_path = argv[++i];
            else if (arg == "--seed" && i + 1 < argc) {
                const std::string_view value(argv[++i]);
                std::uint64_t parsed{};
                const auto [end, error] =
                    std::from_chars(value.data(), value.data() + value.size(), parsed);
                if (error != std::errc{} || end != value.data() + value.size())
                    throw std::invalid_argument("--seed requires an unsigned 64-bit integer");
                seed = parsed;
            } else if (arg == "--user-data" && i + 1 < argc)
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
                std::cout
                    << "Feature Lab defaults: WASD/arrows move, +/- zoom, Space pause, R "
                       "restart, F11 "
                       "fullscreen, PageUp/PageDown scroll status, M mute, N/B music/effects "
                       "volume, E nearby alarm switch, F1 settings/rebind controls, F2 "
                       "diagnostics, F3 debug "
                       "shapes, "
                       "F5 reload shaders, F6 shader failure demo, F7 reload textures, "
                       "F8 texture failure demo, F10 single step while "
                       "paused, Escape close settings/quit.\n"
                       "--level facility|annex --level-tour (script both levels) "
                       "--watch-assets (automatic visual/gameplay asset reload) --font-demo (text "
                       "gallery) "
                       "--play (skip title) --verify (CPU gameplay test) --scripted --frames "
                       "N --no-audio "
                       "--no-vsync --screenshot FILE.ppm --assets DIRECTORY "
                       "--missing-texture-demo --settings-demo --diagnostics-demo "
                       "--shader-error-demo --texture-error-demo --user-data DIRECTORY "
                       "--load-slot 1..3 --save-slot 1..3 (on exit)\n"
                       "--seed UINT64 --record-replay FILE.erpl --replay FILE.erpl "
                       "--verify-replay FILE.erpl (CPU only). Replay modes disable F1; "
                       "Escape ends recording/playback.\n"
                       "--profile-report FILE.json --profile-warmup N "
                       "(requires --replay; unpaced, silent, default config)\n"
                       "--spike-report FILE.json (scripted reload/transition profiling)\n"
                       "--stress --help (configurable arena and benchmark reports)\n";
                return 0;
            } else
                throw std::invalid_argument("Unknown/incomplete option: " + std::string(arg));
        }
        const int replay_modes = static_cast<int>(!record_path.empty()) +
                                 static_cast<int>(!playback_path.empty()) +
                                 static_cast<int>(!verify_path.empty());
        const bool playing_file = !playback_path.empty() || !verify_path.empty();
        if (replay_modes > 1 || (replay_modes && (settings_demo || verification)) ||
            (playing_file && (seed || load_slot || scripted)) || (seed && load_slot))
            throw std::invalid_argument("Conflicting replay/seed/startup options");
        if (level_tour &&
            (replay_modes || load_slot || selected_level != feature_lab::Level::facility))
            throw std::invalid_argument(
                "--level-tour requires an unsaved facility start without replay");
        if (profile_warmup_set && !profile_requested)
            throw std::invalid_argument("--profile-warmup requires --profile-report");
        if (profile_requested) {
            feature_lab::validate_profile_arguments(argc, argv);
            if (profile_path.empty() || playback_path.empty())
                throw std::invalid_argument("Replay profiling requires --replay and a report path");
            std::error_code error;
            if (std::filesystem::absolute(profile_path).lexically_normal() ==
                    std::filesystem::absolute(playback_path).lexically_normal() ||
                std::filesystem::equivalent(profile_path, playback_path, error))
                throw std::invalid_argument("Profile report must not replace its replay input");
            audio_enabled = false;
            vsync = false;
            vsync_override = true;
        }
        if (spike_requested) {
            feature_lab::validate_spike_arguments(argc, argv);
            scripted = direct_play = true;
            audio_enabled = vsync = false;
            vsync_override = true;
            seed = 1;
            frame_limit = static_cast<int>(feature_lab::SpikeProfile::frame_limit);
        }
        const AssetRoot assets = AssetRoot::discover(asset_directory);
        if (verification)
            return verify(assets, selected_level);
        auto game = feature_lab::load_game(assets, selected_level);
        if (seed) {
            game.random_seed = *seed;
            game.random.restore(*seed);
        }
        std::optional<feature_lab::ReplayFile> playback;
        std::size_t playback_index = 0;
        if (playing_file) {
            playback = feature_lab::read_replay(verify_path.empty() ? playback_path : verify_path);
            feature_lab::restore_game(game, playback->initial, &assets);
            if (!verify_path.empty()) {
                for (; playback_index < playback->commands.size(); ++playback_index)
                    feature_lab::playback_step(game, *playback, playback_index);
                std::cout << "[replay] verified commands=" << playback_index
                          << " ticks=" << game.ticks << " hash=" << feature_lab::replay_hash(game)
                          << '\n';
                return 0;
            }
        }
        std::optional<feature_lab::SpikeProfile> spikes;
        std::optional<feature_lab::ReplayProfile> profile;
        if (spike_requested) {
            spikes.emplace();
            profile.emplace(feature_lab::SpikeProfile::frame_limit, feature_lab::SpikeProfile::warmup, 0);
        }
        if (profile_requested)
            profile.emplace(playback->commands.size(), profile_warmup,
                            feature_lab::replay_fingerprint(feature_lab::encode_replay(*playback)));
        feature_lab::SceneFlow flow(
            game,
            !(replay_modes || direct_play || scripted || load_slot || save_slot || settings_demo ||
              diagnostics_demo || missing_texture_demo || shader_error_demo || texture_error_demo),
            &assets);
        std::unique_ptr<UserStorage> storage;
        feature_lab::Configuration config;
        std::string persistence_notice;
        if (!profile) {
            try {
                storage = std::make_unique<UserStorage>(UserPaths::discover(user_directory));
                if (const auto bytes = storage->read_config())
                    config = feature_lab::decode_config(*bytes);
            } catch (const std::exception& error) {
                std::cerr << "[persistence] " << error.what() << '\n';
                persistence_notice = "CONFIG UNAVAILABLE - USING DEFAULTS";
            }
        }
        if (!vsync_override)
            vsync = config.vsync;
        if (load_slot) {
            if (!storage)
                throw std::runtime_error("User storage unavailable");
            feature_lab::load_slot(*storage, load_slot, game, &assets);
            scripted = false; // A restored human checkpoint has no scripted route cursor.
            std::cout << "[persistence] loaded slot=" << load_slot << " ticks=" << game.ticks
                      << '\n';
        }
        std::optional<feature_lab::ReplayRecorder> recorder;
        if (!record_path.empty())
            recorder.emplace(game);
        if (!screenshot.empty() && frame_limit == 0 && !playback && !level_tour)
            frame_limit = 120;
        Window window(1280, 720, "Feature Lab | Linux 2D Engine");
        const bool vsync_available = window.set_vsync(vsync);
        if (!vsync_available)
            std::cerr << "[platform] Requested VSync control unavailable\n";
        Renderer renderer;
        feature_lab::FontAssets fonts;
        fonts.reload(renderer, assets);
        GpuTimer gpu_timer;
        feature_lab::ShaderTools shaders;
        shaders.reload(renderer, assets);
        if (shader_error_demo)
            shaders.fail_demo(renderer);
        feature_lab::Overlays overlays(renderer, game);
        feature_lab::CollisionVisuals collision_visuals(renderer);
        feature_lab::SamplingGallery sampling_gallery(renderer);
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
        feature_lab::AnimationAssets animation_assets(recorder.has_value() || playback.has_value());
        feature_lab::TilemapAssets tilemap_assets(recorder.has_value() || playback.has_value());
        feature_lab::SceneAssets scene_assets(recorder.has_value() || playback.has_value());
        std::function<void()> reload_animations;
        if (!animation_assets.locked())
            reload_animations = [&] { animation_assets.reload(game, assets); };
        std::function<bool(std::string_view)> reload_tilemap;
        if (!tilemap_assets.locked())
            reload_tilemap = [&](std::string_view key) {
                if (key != feature_lab::TilemapAssets::key(game.level))
                    return false;
                std::vector<Rect> walls;
                if (tilemap_assets.reload(game, assets, [&](const TileMap& candidate) {
                        walls = feature_lab::Overlays::stage(candidate, game.walls);
                    }))
                    overlays.reload(std::move(walls));
                return true;
            };
        std::cout << "[assets] " << assets.directory() << " entities=" << game.world.size()
                  << " GPU textures=" << renderer.live_textures() << '\n';
        feature_lab::Soundtrack soundtrack(assets, audio_enabled, "default", config.audio,
                                           game.paused);
        soundtrack.locate_emitter(game);
        if (load_slot || playback)
            soundtrack.restored(game);
        std::function<bool(std::string_view)> reload_scene;
        if (!scene_assets.locked())
            reload_scene = [&](std::string_view key) {
                if (key != feature_lab::SceneAssets::key(game.level))
                    return false;
                std::vector<Rect> walls;
                if (scene_assets.reload(game, assets, [&](const feature_lab::Game& candidate) {
                        textures.validate_level(candidate);
                        walls = feature_lab::Overlays::stage(candidate);
                    })) {
                    overlays.reload(std::move(walls));
                    soundtrack.locate_emitter(game);
                }
                return true;
            };
        std::function<bool()> reload_audio;
        if (soundtrack.enabled())
            reload_audio = [&] {
                if (!soundtrack.enabled())
                    return false;
                soundtrack.reload(assets);
                return true;
            };
        std::unique_ptr<feature_lab::AssetReload> asset_reload;
        if (watch_assets)
            asset_reload = std::make_unique<feature_lab::AssetReload>(
                assets, textures, reload_animations, reload_tilemap, reload_scene, reload_audio);
        bool reject_spike_transition = false;
        std::vector<Rect> staged_walls;
        flow.prepare_replacement = [&](const feature_lab::Game& candidate) {
            textures.validate_level(candidate);
            staged_walls = feature_lab::Overlays::stage(candidate);
            if (reject_spike_transition)
                throw std::runtime_error("Deliberate spike-profile transition preflight rejection");
        };
        const auto replacement_applied = [&] {
            animation_assets.replaced();
            tilemap_assets.replaced();
            scene_assets.replaced();
            overlays.replaced(std::move(staged_walls));
            soundtrack.restored(game);
        };
        const auto spike_resources = [&] {
            return feature_lab::SpikeResources{renderer.live_textures(), renderer.texture_bytes(),
                renderer.live_targets(), renderer.target_bytes(), renderer.font_bytes()};
        };
        const auto spike_state = [&] {
            return feature_lab::SpikeState{feature_lab::replay_hash(game), renderer.shader_revision(),
                textures.revision(), fonts.revision(), static_cast<unsigned>(game.level), spike_resources()};
        };
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
        bool running = true, suspended = false;
        while (running) {
            const auto frame_started = profile ? Clock::now() : Clock::time_point{};
            if (!window.poll(input))
                break;
            const auto now = Clock::now();
            if (!window.drawable()) {
                if (!suspended)
                    std::cout << "[platform] suspended ticks=" << game.ticks << std::endl;
                suspended = true;
                clock = FixedClock{};
                last = now;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if (suspended) {
                std::cout << "[platform] resumed ticks=" << game.ticks << std::endl;
                suspended = false;
                last = now; // Resume without hidden time or a partial catch-up step.
            }
            const double elapsed = std::chrono::duration<double>(now - last).count();
            auto batch = clock.advance(elapsed);
            unsigned simulation_steps = 0;
            std::uint32_t manual_reloads = 0;
            last = now;
            if (!scripted && !playback)
                dropped += batch.dropped;
            if (scripted || playback) {
                batch.steps = 1;
                batch.alpha = 1;
            }
            for (int step = 0; step < batch.steps; ++step) {
                auto physical_input = input.consume();
                if (spikes) {
                    if (button(physical_input, Key::escape).pressed) {
                        running = false;
                        break;
                    }
                    physical_input = {}; // Only Escape/close can steer this declared workload.
                    if (const auto action = spikes->next(frames + 1, game.won)) {
                        using A = feature_lab::SpikeAction;
                        feature_lab::SpikeEvent event;
                        event.frame = frames + 1;
                        event.action = *action;
                        event.before = spike_state();
                        event.verified = true;
                        const auto operation_started = Clock::now();
                        try {
                            switch (*action) {
                            case A::shader_reload: case A::shader_recover:
                                shaders.reload(renderer, assets);
                                event.applied = shaders.result().applied;
                                event.diagnostic = shaders.result().diagnostics;
                                break;
                            case A::shader_reject:
                                shaders.fail_demo(renderer);
                                event.applied = shaders.result().applied;
                                event.diagnostic = shaders.result().diagnostics;
                                break;
                            case A::texture_reload: case A::texture_recover: case A::texture_reject:
                                event.applied = textures.reload(renderer, *action == A::texture_reject);
                                event.diagnostic = textures.diagnostic();
                                break;
                            case A::font_reload:
                                fonts.reload(renderer, assets);
                                event.applied = fonts.applied();
                                event.diagnostic = fonts.diagnostic();
                                break;
                            case A::transition_reject: case A::transition:
                                reject_spike_transition = *action == A::transition_reject;
                                flow.advance_level(game, physical_input);
                                replacement_applied();
                                replay = feature_lab::Replay{};
                                ++level_changes;
                                event.applied = true;
                                break;
                            }
                        } catch (const std::exception& error) {
                            event.diagnostic = error.what();
                        }
                        event.milliseconds = feature_lab::stress_ms(operation_started, Clock::now());
                        reject_spike_transition = false;
                        event.after = spike_state();
                        spikes->event(std::move(event));
                        if (spikes->failed() || *action == A::transition || *action == A::transition_reject)
                            continue;
                    }
                }
                if (recorder || playback) {
                    // A replay is one uninterrupted game session. Menus and slot loads
                    // alter state outside Game::update and are unavailable in this mode.
                    physical_input[static_cast<std::size_t>(Key::settings)] = {};
                    if (button(physical_input, Key::escape).pressed) {
                        running = false;
                        break;
                    }
                }
                if (playback) {
                    if (!profile && button(physical_input, Key::fullscreen).pressed)
                        window.toggle_fullscreen();
                    if (playback_index == playback->commands.size()) {
                        running = false;
                        break;
                    }
                    const auto ticks_before = game.ticks;
                    const auto controls = playback->commands[playback_index].input();
                    const int picked =
                        feature_lab::playback_step(game, *playback, playback_index++);
                    simulation_steps += static_cast<unsigned>(game.ticks > ticks_before);
                    soundtrack.update(game, controls, picked);
                    if (playback_index == playback->commands.size())
                        running = false;
                    continue;
                }
                const auto routed = flow.update(game, physical_input, window.width(),
                                                window.height(), storage.get());
                if (routed.quit) {
                    running = false;
                    break;
                }
                if (routed.transitioned) {
                    if (routed.replaced) {
                        replacement_applied();
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
                if (shaders.update(renderer, assets, controls))
                    manual_reloads |= feature_lab::AssetReload::shader_group;
                if (textures.update(renderer, controls))
                    manual_reloads |= feature_lab::AssetReload::texture_group;
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
                            feature_lab::load_slot(*storage, settings.selected_slot, game, &assets,
                                                   flow.prepare_replacement);
                            settings.loaded(game);
                            replacement_applied();
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
                if (!recorder && !menu.captured && game.won &&
                    (button(controls, Key::interact).pressed ||
                     (level_tour && level_changes == 0))) {
                    try {
                        flow.advance_level(game, physical_input);
                        replacement_applied();
                        replay = feature_lab::Replay{};
                        ++level_changes;
                        std::cout << "[level] entered=" << feature_lab::level_name(game.level)
                                  << " spawn=" << game.position.x << ',' << game.position.y << '\n';
                    } catch (const std::exception& error) {
                        flow.notice = "LEVEL LOAD FAILED - SESSION RETAINED";
                        std::cerr << "[level] " << error.what() << '\n';
                        if (level_tour)
                            throw;
                    }
                    continue;
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
                const int picked =
                    recorder ? recorder->step(game, gameplay) : game.update(gameplay);
                if (game.ticks > ticks_before)
                    ++simulation_steps;
                soundtrack.update(game, controls, picked);
                if (level_tour && level_changes == 1 && game.won)
                    running = false;
                const auto audio = soundtrack.settings();
                if (!(config.audio == audio)) {
                    config.audio = audio;
                    config_dirty = true;
                }
                if (was_open && !settings.opened())
                    persist_config();
                if (recorder && recorder->full()) {
                    running = false; // Save a complete bounded recording on normal shutdown.
                    break;
                }
            }
            if (spikes && !running)
                break;
            if (profile && playback && !running && playback_index < playback->commands.size())
                break; // Abort without inventing a frame/command pair.
            soundtrack.collect();
            if (asset_reload)
                asset_reload->update(renderer, assets, shaders, textures, fonts, manual_reloads);
            const auto simulated = profile ? Clock::now() : Clock::time_point{};
            const auto gpu_samples = diagnostics.collect_gpu(gpu_timer, frames + 1);
            if (profile)
                profile->collect_gpu(gpu_samples);
            if (spikes)
                spikes->collect_gpu(gpu_samples);
            const auto rendering = profile ? Clock::now() : Clock::time_point{};
            gpu_timer.begin(frames + 1);
            const auto camera = game.camera(window.width(), window.height(), batch.alpha);
            if (!flow.title_open())
                overlays.render_minimap(renderer, game, window.width(), window.height(),
                                        batch.alpha);
            const auto minimap_submitted = profile ? Clock::now() : Clock::time_point{};
            Clock::time_point world_submitted{};
            renderer.begin(window.width(), window.height(), camera);
            if (flow.title_open()) {
                flow.prepare(window.width(), window.height(), storage != nullptr);
                flow.draw(renderer, window.width(), window.height());
            } else {
                draw(renderer, game, textures, batch.alpha, window.width(), window.height(), camera,
                     overlays, soundtrack, collision_visuals, sampling_gallery, diagnostics,
                     shaders, fonts, animation_assets, tilemap_assets, scene_assets,
                     settings.opened(), settings.bindings,
                     recorder ? "15 / RECORD " + std::to_string(recorder->file().commands.size()) +
                                    " / 36000"
                     : playback ? "15 / REPLAY " + std::to_string(playback_index) + " / " +
                                      std::to_string(playback->commands.size())
                     : !flow.notice.empty()
                         ? flow.notice
                         : "36 / " + std::string(feature_lab::level_name(game.level)) +
                               (watch_assets ? " / AUTO RELOAD" : " / AUDIO BANK RELOAD"),
                     recorder.has_value() || playback.has_value(), playback.has_value(),
                     profile ? &world_submitted : nullptr);
                diagnostics.draw_hud(renderer, game, window.width(), window.height(), dropped);
                settings.draw(renderer, window.width(), window.height());
            }
            if (font_demo)
                feature_lab::draw_font_gallery(renderer, window.width(), window.height());
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
            if (!screenshot.empty() &&
                (!running || frames == static_cast<std::size_t>(frame_limit)))
                renderer.screenshot(screenshot);
            const auto presenting = profile ? Clock::now() : Clock::time_point{};
            window.present();
            const auto presented = profile ? Clock::now() : Clock::time_point{};
            if (profile) {
                const std::array<double, feature_lab::ReplayProfile::scope_count> frame_ms{
                    feature_lab::stress_ms(now, simulated),
                    feature_lab::stress_ms(rendering, minimap_submitted),
                    feature_lab::stress_ms(minimap_submitted, world_submitted),
                    feature_lab::stress_ms(world_submitted, submitted),
                    feature_lab::stress_ms(presenting, presented),
                    feature_lab::stress_ms(frame_started, presented)};
                profile->record(frame_ms, window.width(), window.height(), renderer.stats(),
                                overlays.minimap_stats(), simulation_steps);
                if (spikes) {
                    spikes->record(frame_ms, spike_resources());
                    if (spikes->failed())
                        running = false;
                }
            }
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
            if (!vsync && !profile)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        persist_config();
        if (recorder) {
            feature_lab::write_replay(record_path, recorder->file());
            std::cout << "[replay] saved commands=" << recorder->file().commands.size()
                      << " seed=" << recorder->file().initial.random_seed << '\n';
        }
        if (playback)
            std::cout << "[replay] played commands=" << playback_index << "/"
                      << playback->commands.size() << " hash=" << feature_lab::replay_hash(game)
                      << '\n';
        if (save_slot) {
            if (!storage)
                throw std::runtime_error("User storage unavailable");
            feature_lab::save_slot(
                *storage, save_slot, game,
                (flow.title_open() ? flow.gameplay_paused(game) : settings.gameplay_paused(game)));
            std::cout << "[persistence] saved slot=" << save_slot << " ticks=" << game.ticks
                      << '\n';
        }
        if (level_tour &&
            (level_changes != 1 || game.level != feature_lab::Level::annex || !game.won))
            throw std::runtime_error("Level tour did not complete both levels");
        std::cout << "[session] frames=" << frames << " ticks=" << game.ticks
                  << " door_completions=" << game.door_completions << " collected=" << game.count()
                  << " dropped_seconds=" << dropped << " window_draws=" << renderer.stats().draws
                  << " minimap_draws=" << overlays.minimap_stats().draws
                  << " level=" << feature_lab::level_name(game.level)
                  << " level_changes=" << level_changes
                  << " core_components=" << game.world.component_count<feature_lab::CorePickup>()
                  << " watched_shaders=" << (asset_reload ? asset_reload->shader_reloads() : 0)
                  << " watched_textures=" << (asset_reload ? asset_reload->texture_reloads() : 0)
                  << " watched_audio=" << (asset_reload ? asset_reload->audio_reloads() : 0)
                  << " audio_revision=" << soundtrack.revision()
                  << " audio_reload_applied=" << soundtrack.reload_applied()
                  << " sound_bytes=" << soundtrack.sound_bytes()
                  << " retained_sound_bytes=" << soundtrack.retained_sound_bytes()
                  << " watched_scenes=" << (asset_reload ? asset_reload->scene_reloads() : 0)
                  << " scene_revision=" << scene_assets.revision()
                  << " scene_applied=" << scene_assets.applied()
                  << " scene_locked=" << scene_assets.locked()
                  << " watched_tilemaps=" << (asset_reload ? asset_reload->tilemap_reloads() : 0)
                  << " tilemap_revision=" << tilemap_assets.revision()
                  << " tilemap_applied=" << tilemap_assets.applied()
                  << " tilemap_locked=" << tilemap_assets.locked() << " watched_animations="
                  << (asset_reload ? asset_reload->animation_reloads() : 0)
                  << " animation_revision=" << animation_assets.revision()
                  << " animation_applied=" << animation_assets.applied()
                  << " animation_locked=" << animation_assets.locked()
                  << " watched_fonts=" << (asset_reload ? asset_reload->font_reloads() : 0)
                  << " font_revision=" << fonts.revision() << " font_applied=" << fonts.applied()
                  << " font_embedded=" << fonts.embedded()
                  << " font_bytes=" << renderer.font_bytes()
                  << " scene=" << (flow.title_open() ? "title" : "gameplay")
                  << " texture_revision=" << textures.revision()
                  << " texture_applied=" << textures.applied()
                  << " shader_revision=" << renderer.shader_revision()
                  << " shader_applied=" << shaders.result().applied
                  << " fullscreen=" << window.fullscreen() << " focused=" << window.focused()
                  << " targets=" << renderer.live_targets()
                  << " target_bytes=" << renderer.target_bytes() << '\n';
        const auto final_gpu_samples = diagnostics.collect_gpu(gpu_timer, frames); // Final poll only.
        if (profile)
            profile->collect_gpu(final_gpu_samples);
        if (spikes)
            spikes->collect_gpu(final_gpu_samples);
        const auto gpu = diagnostics.gpu_summary();
        const auto gpu_stats = gpu_timer.stats();
        std::cout << "[gpu render interval ms] supported=" << gpu_stats.supported
                  << " method=" << gpu_timing_method_name(gpu_stats.method)
                  << " counter_bits=" << gpu_stats.counter_bits << " mean=" << gpu.mean_ms
                  << " p95=" << gpu.p95_ms << " max=" << gpu.max_ms << " samples=" << gpu.samples
                  << " last_frame=" << gpu.last_frame
                  << " age_frames=" << (gpu.samples ? frames - gpu.last_frame : 0)
                  << " submitted=" << gpu_stats.submitted << " completed=" << gpu_stats.completed
                  << " pending=" << gpu_stats.pending << " skipped=" << gpu_stats.skipped
                  << " invalid=" << gpu_stats.invalid << '\n';
        const auto gpu_failures = gpu_timer.failures();
        for (const auto& failure : gpu_failures)
            std::cerr << "[gpu timestamp reversed] frame=" << failure.frame
                      << " slot=" << failure.slot << " start_ns=" << failure.start_ns
                      << " end_ns=" << failure.end_ns
                      << " last_attempted_frame=" << failure.last_attempted_frame << '\n';
        if (gpu_stats.invalid > gpu_failures.size())
            std::cerr << "[gpu timestamp diagnostics] omitted="
                      << gpu_stats.invalid - gpu_failures.size() << '\n';
        const auto audio_stats = soundtrack.stats();
        std::cout << "[audio stats] enabled=" << soundtrack.enabled()
                  << " started=" << audio_stats.started << " completed=" << audio_stats.completed
                  << " dropped=" << audio_stats.dropped_plays
                  << " queue_rejections=" << audio_stats.queue_rejections
                  << " written_frames=" << audio_stats.written_frames
                  << " underruns=" << audio_stats.underruns
                  << " suspensions=" << audio_stats.suspensions
                  << " recoveries=" << audio_stats.recoveries << " waits=" << audio_stats.waits
                  << " device_error=" << audio_stats.device_error << '\n';
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
        if (profile) {
            profile->write(profile_path, profile->json(feature_lab::replay_hash(game), game.ticks,
                           feature_lab::level_name(game.level), renderer.graphics_info(),
                           vsync_available, gpu_stats, gpu_failures, spikes ? &*spikes : nullptr));
            return profile->complete() && (!spikes || spikes->complete()) && gpu_stats.invalid == 0 ? 0 : 1;
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[fatal] " << e.what() << '\n';
        return 1;
    }
}
