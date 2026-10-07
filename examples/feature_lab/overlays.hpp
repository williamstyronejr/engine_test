#pragma once
#include "engine/renderer.hpp"
#include "game.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace feature_lab {
inline float hud_scale(int width, int height) {
    return std::clamp(
        std::min(static_cast<float>(width) / 1280.0F, static_cast<float>(height) / 720.0F), 0.35F,
        1.25F);
}
// Renderer owns the target lifetime. This example retains only a checked handle.
class Overlays {
  public:
    Overlays(engine::Renderer& renderer, const Game& game)
        : minimap_(renderer.create_target(276, 138)) {
        walls_.reserve(game.map.solid_cells() + game.walls.size());
        game.map.append_colliders(game.map.bounds(), walls_);
        walls_.insert(walls_.end(), game.walls.begin(), game.walls.end());
    }
    void input(const engine::InputFrame& frame) {
        using namespace engine;
        scroll_ = std::clamp(scroll_ + (static_cast<float>(button(frame, Key::panel_down).held) -
                                        static_cast<float>(button(frame, Key::panel_up).held)) *
                                           2.0F,
                             0.0F, 112.0F);
        if (button(frame, Key::restart).pressed)
            scroll_ = 0;
    }
    void render_minimap(engine::Renderer& r, const Game& game, int width, int height, float alpha) {
        using namespace engine;
        const float scale = hud_scale(width, height);
        const int target_height = std::max(1, static_cast<int>(std::ceil(138 * scale)));
        r.resize_target(minimap_, target_height * 2, target_height);
        const auto bounds = game.map.bounds();
        const auto size = bounds.max - bounds.min;
        const Camera overview{(bounds.min + bounds.max) * 0.5F, std::max(size.y, size.x * 0.5F)};
        r.begin(minimap_, overview, {0.012F, 0.025F, 0.037F, 1});
        r.quad(overview.center, size, {0.04F, 0.075F, 0.095F, 1});
        for (const auto& wall : walls_)
            r.quad((wall.min + wall.max) * 0.5F, wall.max - wall.min, {0.19F, 0.32F, 0.39F, 1});
        for (std::size_t i = 0; i < game.keys.size(); ++i)
            if (!game.collected[i])
                r.quad(game.location(game.keys[i]), {1.3F, 1.3F}, {1, 0.65F, 0.08F, 1}, 0.785F);
        const auto goal = game.location(game.exit);
        r.quad(goal, {1.6F, 1.6F},
               game.door_open ? Color{0.1F, 1, 0.4F, 1} : Color{0.8F, 0.18F, 0.12F, 1});
        r.quad(lerp(game.previous, game.position, alpha), {1.3F, 1.3F}, {0.04F, 1, 0.8F, 1});
        const auto camera = game.camera(width, height, alpha);
        const auto extent = camera.extent(width, height);
        constexpr Color outline{0.1F, 0.8F, 0.7F, 0.65F};
        for (float sign : {-1.0F, 1.0F}) {
            r.quad(camera.center + Vec2{0, sign * extent.y * 0.5F}, {extent.x, 0.22F}, outline);
            r.quad(camera.center + Vec2{sign * extent.x * 0.5F, 0}, {0.22F, extent.y}, outline);
        }
        r.end();
        minimap_stats_ = r.stats();
    }
    void draw(engine::Renderer& r, const Game& game, int width, int height) const {
        using namespace engine;
        const float scale = hud_scale(width, height);
        const float x = static_cast<float>(width) / scale - 300;
        constexpr Color accent{0.04F, 0.85F, 0.7F, 1};
        const auto box = [&](float px, float py, float w, float h, Color color) {
            r.quad({(px + w * 0.5F) * scale, -(py + h * 0.5F) * scale}, {w * scale, h * scale},
                   color);
        };
        const auto text = [&](float px, float py, std::string_view value,
                              Color color = {0.8F, 0.9F, 0.94F, 1}) {
            r.text({px * scale, -py * scale}, 1.4F * scale, value, color);
        };
        const auto clip = [&](float px, float py, float w, float h) {
            const int left = static_cast<int>(std::floor(px * scale));
            const int top = static_cast<int>(std::floor(py * scale));
            r.push_clip({left, top, static_cast<int>(std::ceil((px + w) * scale)) - left,
                         static_cast<int>(std::ceil((py + h) * scale)) - top});
        };
        box(x - 8, 102, 292, 170, {0.008F, 0.018F, 0.03F, 0.96F});
        text(x, 108, "FACILITY MAP", accent);
        // Nested clipping keeps both the texture and its camera outline inside the panel.
        clip(x - 8, 102, 292, 170);
        clip(x, 126, 276, 138);
        r.target_sprite(minimap_, Transform::from({(x + 138) * scale, -195 * scale}, 0,
                                                  {276 * scale, 138 * scale}));
        r.pop_clip();
        r.pop_clip();
        box(x - 8, 284, 292, 156, {0.008F, 0.018F, 0.03F, 0.96F});
        text(x, 292, "STATUS / PGUP PGDN", accent);
        const std::array<std::string, 9> rows = {
            game.won      ? "MISSION COMPLETE"
            : game.paused ? "MISSION PAUSED"
                          : "MISSION ACTIVE",
            "CORE A  " + std::string(game.collected[0] ? "RECOVERED" : "AWAITING PICKUP"),
            "CORE B  " + std::string(game.collected.size() > 1 && game.collected[1]
                                         ? "RECOVERED"
                                         : "AWAITING PICKUP"),
            "CORE C  " + std::string(game.collected.size() > 2 && game.collected[2]
                                         ? "RECOVERED"
                                         : "AWAITING PICKUP"),
            game.door_open      ? "EXIT DOOR OPEN"
            : game.door_started ? "EXIT DOOR OPENING"
                                : "EXIT DOOR LOCKED",
            "CYAN  PLAYER / VIEW",
            "GOLD  REMAINING CORES",
            "RED   LOCKED EXIT",
            "GREEN OPEN EXIT"};
        clip(x, 316, 264, 104);
        for (std::size_t i = 0; i < rows.size(); ++i)
            text(x + 2, 320 + static_cast<float>(i) * 24 - scroll_, rows[i]);
        r.pop_clip();
        box(x + 270, 316, 3, 104, {0.07F, 0.14F, 0.18F, 1});
        box(x + 270, 316 + scroll_ / 112 * 72, 3, 32, accent);
        text(x, 426, "SCROLL FOR MAP LEGEND", {0.3F, 0.45F, 0.5F, 1});
    }
    engine::RenderStats minimap_stats() const { return minimap_stats_; }

  private:
    engine::RenderTargetHandle minimap_;
    std::vector<engine::Rect> walls_;
    float scroll_{};
    engine::RenderStats minimap_stats_;
};
} // namespace feature_lab
