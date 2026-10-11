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
// Base-level linear filtering: map to the outer texel centers of a cell.
// One-texel cells collapse to a constant UV. Not a substitute for padded mipmaps.
inline Rect atlas_uv_inset(std::uint32_t cell, std::uint32_t columns, std::uint32_t rows,
                           std::uint32_t width, std::uint32_t height) {
    validate_atlas_image(width, height, columns, rows);
    if (width > 4096 || height > 4096)
        throw std::invalid_argument("Atlas exceeds texture dimension limit");
    if (cell >= columns * rows)
        throw std::out_of_range("Atlas cell outside grid");
    const auto cell_width = width / columns, cell_height = height / rows;
    const auto x = (cell % columns) * cell_width, y = (cell / columns) * cell_height;
    return {{(static_cast<float>(x) + 0.5F) / static_cast<float>(width),
             (static_cast<float>(y) + 0.5F) / static_cast<float>(height)},
            {(static_cast<float>(x + cell_width) - 0.5F) / static_cast<float>(width),
             (static_cast<float>(y + cell_height) - 0.5F) / static_cast<float>(height)}};
}
} // namespace engine
