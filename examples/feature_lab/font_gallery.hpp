#pragma once
#include "engine/renderer.hpp"
#include <algorithm>

namespace feature_lab {
// Optional visual fixture exercises the same font used by all menus and HUD text.
inline void draw_font_gallery(engine::Renderer& renderer, int width, int height) {
    using namespace engine;
    const float scale = std::min(static_cast<float>(width) / 680, static_cast<float>(height) / 330);
    const float x = (static_cast<float>(width) - 640 * scale) * 0.5F;
    const float y = (static_cast<float>(height) - 290 * scale) * 0.5F;
    renderer.set_camera({{static_cast<float>(width) * 0.5F, -static_cast<float>(height) * 0.5F},
                         static_cast<float>(height)});
    renderer.quad({x + 320 * scale, -y - 145 * scale}, {640 * scale, 290 * scale},
                  {0.015F, 0.03F, 0.045F, 0.98F});
    const auto text = [&](float row, float size, std::string_view value, Color color = {}) {
        renderer.text({x + 20 * scale, -y - row * scale}, size * scale, value, color);
    };
    text(20, 3, "BITMAP FONT / METRICS + FALLBACK", {0.1F, 0.9F, 0.7F, 1});
    text(58, 2, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    text(82, 2, "0123456789 % + - / : . ?");
    text(112, 2, "III WWW 111 ... / VARIABLE ADVANCES", {1, 0.7F, 0.2F, 1});
    text(144, 2, "LINE ONE\nLINE TWO / LINE HEIGHT");
    text(190, 2, "MISSING: abc @ [] / FALLBACK: ??????", {0.8F, 0.6F, 1, 1});
    const auto measured = renderer.text_size("MEASURED WIDTH", 2 * scale);
    text(222, 2, "MEASURED WIDTH");
    renderer.quad({x + 20 * scale + measured.x * 0.5F, -y - 240 * scale}, {measured.x, scale},
                  {0.1F, 0.9F, 0.7F, 1});
    renderer.push_clip({static_cast<int>(x + 20 * scale), static_cast<int>(y + 253 * scale),
                        static_cast<int>(190 * scale), static_cast<int>(20 * scale)});
    text(253, 2, "CLIPPED TEXT CONTINUES BEYOND PANEL", {0.3F, 0.7F, 1, 0.7F});
    renderer.pop_clip();
}
} // namespace feature_lab
