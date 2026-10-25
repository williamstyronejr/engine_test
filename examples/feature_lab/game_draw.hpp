#pragma once
#ifdef ENGINE_ALLOCATION_PROFILE
#include "../../profiling/allocation_profile.hpp"
#endif
#include "animation_assets.hpp"
#include "collision_visuals.hpp"
#include "diagnostics.hpp"
#include "font_assets.hpp"
#include "sampling_gallery.hpp"
#include "scene_assets.hpp"
#include "shader_tools.hpp"
#include "soundtrack.hpp"
#include "texture_assets.hpp"
#include "tilemap_assets.hpp"
#include <chrono>

namespace feature_lab {
// Shared by ordinary gameplay and the development-only UI allocation workload.
inline void
draw_game(engine::Renderer& r, const feature_lab::Game& game, const TextureAssets& textures,
          float alpha, int width, int height, engine::Camera camera,
          const feature_lab::Overlays& overlays, const feature_lab::Soundtrack& soundtrack,
          const feature_lab::CollisionVisuals& collision_visuals,
          const feature_lab::SamplingGallery& sampling_gallery,
          const feature_lab::Diagnostics& diagnostics, const feature_lab::ShaderTools& shaders,
          const feature_lab::FontAssets& fonts, const feature_lab::AnimationAssets& animations,
          const feature_lab::TilemapAssets& tilemaps, const feature_lab::SceneAssets& scenes,
          bool settings_open, const engine::KeyBindings& bindings, std::string_view replay_status,
          bool replay_session, bool playing_replay, std::string& text,
          std::chrono::steady_clock::time_point* world_submitted = nullptr
#ifdef ENGINE_ALLOCATION_PROFILE
          ,
          allocation_profile::Count* world_allocations = nullptr
#endif
) {
    using namespace engine;
    constexpr Color white{0.8F, 0.9F, 0.94F, 1}, muted{0.24F, 0.36F, 0.43F, 1},
        cyan{0.04F, 0.85F, 0.7F, 1};
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
    collision_visuals.draw(r, game, alpha, bindings, text);
    diagnostics.draw_world(r, game, camera, width, height);
    const auto goal = game.location(game.exit);
    r.text(goal + Vec2{-0.7F, 1.4F}, 0.045F, game.door_open ? "EXIT OPEN" : "LOCKED", cyan);
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    r.set_camera({{w * 0.5F, -h * 0.5F}, h});
    if (world_submitted)
        *world_submitted = std::chrono::steady_clock::now();
#ifdef ENGINE_ALLOCATION_PROFILE
    if (world_allocations)
        *world_allocations = allocation_profile::snapshot();
#endif
    using feature_lab::format_text;
    const float scale = feature_lab::hud_scale(width, height);
    const auto label = [&](float x, float y, float size, std::string_view value, Color color) {
        r.text({x * scale, -y * scale}, size * scale, value, color);
    };
    r.quad({w * 0.5F, -48 * scale}, {w, 96 * scale}, {0.008F, 0.018F, 0.03F, 0.96F});
    label(24, 16, 4, "FEATURE LAB", white);
    label(25, 58, 1.7F, replay_status, cyan);
    const float right = w / scale - 360;
    label(right, 18, 1.8F, format_text(text, "CORES  ", game.count(), " / ", game.total()), cyan);
    label(right, 48, 1.5F,
          game.door_open      ? "EXIT READY"
          : game.door_started ? "DOOR OPENING"
                              : "COLLECT CORES TO OPEN EXIT",
          white);
    r.quad({w * 0.5F, -h + 43 * scale}, {w, 86 * scale}, {0.008F, 0.018F, 0.03F, 0.96F});
    const float bottom = h / scale - 73;
    label(24, bottom, 1.55F,
          playing_replay
              ? "RECORDED GAMEPLAY / LIVE MOVEMENT AND PAUSE DISABLED"
              : format_text(text, bindings.key_name(Key::up), "/", bindings.key_name(Key::left),
                            "/", bindings.key_name(Key::down), "/", bindings.key_name(Key::right),
                            " / ARROWS  MOVE    +/-  ZOOM    SPACE  PAUSE"),
          white);
    label(
        24, bottom + 24, 1.4F,
        playing_replay
            ? "INPUT FROM REPLAY  F11 FULLSCREEN  ESC EXIT"
            : format_text(text, bindings.key_name(Key::interact),
                          (replay_session
                               ? " SWITCH  F1 LOCKED  F10 STEP  ESC SAVE REPLAY"
                               : " SWITCH  F1 SETTINGS  F2 STATS  F3 SHAPES  F10 STEP  ESC QUIT")),
        muted);
    label(24, bottom + 46, 1.4F,
          format_text(text, "VISIBLE TILES ", visible.tiles, " / ",
                      map.width * map.height * map.layers.size(), "    CHUNKS ", visible.chunks,
                      " / ", game.map.chunk_count(), "    COLLIDERS ",
                      game.collision_candidates.size()),
          cyan);
    label(24, bottom - 174, 1.3F, soundtrack.reload_status(text),
          soundtrack.reload_applied() ? cyan : Color{1, 0.3F, 0.12F, 1});
    label(24, bottom - 156, 1.3F, scenes.status(text),
          scenes.applied() ? cyan : Color{1, 0.3F, 0.12F, 1});
    label(24, bottom - 138, 1.3F, tilemaps.status(text),
          tilemaps.applied() ? cyan : Color{1, 0.3F, 0.12F, 1});
    label(24, bottom - 120, 1.3F, animations.status(text),
          animations.applied() ? cyan : Color{1, 0.3F, 0.12F, 1});
    label(24, bottom - 102, 1.3F, fonts.status(text),
          fonts.applied() ? cyan : Color{1, 0.3F, 0.12F, 1});
    label(24, bottom - 84, 1.3F, textures.status(text),
          textures.applied() ? cyan : Color{1, 0.3F, 0.12F, 1});
    label(24, bottom - 66, 1.3F, shaders.status(text),
          shaders.result().applied ? cyan : Color{1, 0.3F, 0.12F, 1});
    label(24, bottom - 44, 1.3F,
          format_text(text, "ALARM ENTRIES ", game.alarm_entries, " / ",
                      game.alarm_disabled ? "DISABLED" : "ACTIVE", "    GRID PAIRS ",
                      game.collisions.stats().candidate_pairs),
          cyan);
    label(24, bottom - 22, 1.3F, soundtrack.status(text, bindings), cyan);
    overlays.draw(r, game, width, height);
    if (!settings_open && (game.won || (game.paused && !diagnostics.controls.visible))) {
        r.quad({w * 0.5F, -h * 0.5F}, {650 * scale, 150 * scale}, {0.008F, 0.014F, 0.022F, 0.96F});
        label(w / scale * 0.5F - (game.won ? 240 : 90), h / scale * 0.5F - 35, 3,
              game.won ? format_text(text, feature_lab::level_name(game.level), " COMPLETE")
                       : "PAUSED",
              cyan);
        label(w / scale * 0.5F - 140, h / scale * 0.5F + 20, 1.5F,
              playing_replay ? "RECORDED STATE / ESC EXIT"
              : game.won && !replay_session
                  ? format_text(text, bindings.key_name(Key::interact), " NEXT / ",
                                bindings.key_name(Key::restart), " RESTART")
              : game.won
                  ? format_text(text, "PRESS ", bindings.key_name(Key::restart), " TO PLAY AGAIN")
                  : "SPACE RESUME / F10 STEP",
              white);
    }
}
} // namespace feature_lab
