#pragma once
#include "engine/font.hpp"
#include <algorithm>
inline engine::FontData metric_font() {
    engine::FontData font;
    font.atlas = {4, 4, std::vector<std::uint8_t>(64)};
    // Two-by-two A: top red/transparent; bottom green/blue.
    const auto pixel = [&](unsigned x, unsigned y, std::array<std::uint8_t, 4> rgba) {
        std::copy(rgba.begin(), rgba.end(), font.atlas.rgba.begin() + (y * 4 + x) * 4);
    };
    pixel(0, 1, {255, 0, 0, 255});
    pixel(0, 0, {0, 255, 0, 255});
    pixel(1, 0, {0, 0, 255, 255});
    pixel(3, 3, {255, 255, 255, 255});
    font.line_height = 5;
    font.glyphs = {{32, 0, 0, 0, 0, 0, 0, 2},
                   {'?', 3, 3, 1, 1, 0, 0, 2},
                   {'A', 0, 0, 2, 2, -1, 1, 4},
                   {'I', 3, 3, 1, 1, 1, -1, 3}};
    return font;
}
