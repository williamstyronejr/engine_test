#pragma once
#include "engine/bindings.hpp"
#include "engine/renderer.hpp"
#include "game.hpp"
#include "hud_text.hpp"

namespace feature_lab {
class CollisionVisuals {
  public:
    explicit CollisionVisuals(engine::Renderer& renderer) {
        engine::TextureData disk{32, 32, std::vector<std::uint8_t>(32 * 32 * 4)};
        for (unsigned y = 0; y < 32; ++y)
            for (unsigned x = 0; x < 32; ++x) {
                const float dx = static_cast<float>(x) + 0.5F - 16,
                            dy = static_cast<float>(y) + 0.5F - 16;
                const float radius = std::hypot(dx, dy);
                const auto i = static_cast<std::size_t>((y * 32 + x) * 4);
                for (unsigned c = 0; c < 3; ++c)
                    disk.rgba[i + c] = radius > 13 ? 255 : 175;
                disk.rgba[i + 3] = radius <= 16 ? 255 : 0;
            }
        disk_ = renderer.upload(disk);
    }
    void draw(engine::Renderer& r, const Game& game, float alpha,
              const engine::KeyBindings& bindings, std::string& text) const {
        using namespace engine;
        const Color red{1, 0.12F, 0.07F, 1}, green{0.05F, 0.9F, 0.5F, 1};
        const auto center = game.alarm_position();
        r.quad({center.x, game.level == Level::facility ? -10.0F : -4.0F}, {0.04F, 4},
               {0.6F, 0.25F, 0.15F, 0.5F});
        r.sprite(disk_, Transform::from(center, 0, {1.4F, 1.4F}),
                 game.alarm_disabled ? green : red);
        r.quad(game.switch_location(), {0.8F, 0.8F}, game.alarm_disabled ? green : red);
        r.text(game.switch_location() + Vec2{-1.4F, 1}, 0.035F,
               format_text(text, bindings.key_name(Key::interact), " / ALARM SWITCH"), green);
        r.text(center + Vec2{0.9F, 0.2F}, 0.035F, game.alarm_disabled ? "SAFE" : "ALARM",
               game.alarm_disabled ? green : red);
        if (game.alarm_touching) {
            const auto p = lerp(game.previous, game.position, alpha);
            r.quad(p, {1.1F, 1.1F}, {1, 0.12F, 0.07F, 0.35F});
        }
    }

  private:
    engine::TextureHandle disk_;
};
} // namespace feature_lab
