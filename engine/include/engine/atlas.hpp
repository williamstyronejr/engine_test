#pragma once
#include "engine/math.hpp"
#include <cstdint>

namespace engine {
inline void validate_atlas(std::uint32_t columns, std::uint32_t rows) {
    if (columns == 0 || rows == 0 || columns > 256 || rows > 256)
        throw std::invalid_argument("Atlas grid must be in 1..256 on each axis");
}
inline Rect atlas_uv(std::uint32_t cell, std::uint32_t columns, std::uint32_t rows) {
    validate_atlas(columns, rows);
    if (cell >= columns * rows)
        throw std::out_of_range("Atlas cell outside grid");
    const auto x = cell % columns, y = cell / columns;
    return {{static_cast<float>(x) / static_cast<float>(columns),
             static_cast<float>(y) / static_cast<float>(rows)},
            {static_cast<float>(x + 1) / static_cast<float>(columns),
             static_cast<float>(y + 1) / static_cast<float>(rows)}};
}
inline void validate_atlas_image(std::uint32_t width, std::uint32_t height, std::uint32_t columns,
                                 std::uint32_t rows) {
    validate_atlas(columns, rows);
    if (!width || !height || width % columns || height % rows)
        throw std::invalid_argument("Texture dimensions must be divisible by atlas grid");
}
} // namespace engine
