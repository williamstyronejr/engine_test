#pragma once
#include "engine/collision_world.hpp"
#include "engine/renderer.hpp"
#include <numbers>

namespace engine {
namespace debug_detail {
inline void validate(Vec2 point, float width, Color color) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || std::abs(point.x) > 1000000 ||
        std::abs(point.y) > 1000000 || !std::isfinite(width) || width <= 0 || width > 1000000 ||
        !std::isfinite(color.r) || !std::isfinite(color.g) || !std::isfinite(color.b) ||
        !std::isfinite(color.a))
        throw std::invalid_argument("Invalid debug geometry");
}
} // namespace debug_detail
// World-space thickness; ordered quad geometry inherits camera culling and scissor.
// Coordinates/width are bounded to +/-1,000,000; a zero-length line draws nothing.
inline void debug_line(Renderer& renderer, Vec2 start, Vec2 end, float width, Color color) {
    debug_detail::validate(start, width, color);
    debug_detail::validate(end, width, color);
    const auto delta = end - start;
    const float length = std::hypot(delta.x, delta.y);
    if (length == 0)
        return;
    renderer.quad((start + end) * 0.5F, {length, width}, color, std::atan2(delta.y, delta.x));
}
inline void debug_rect(Renderer& renderer, Rect rectangle, float width, Color color) {
    debug_detail::validate(rectangle.min, width, color);
    debug_detail::validate(rectangle.max, width, color);
    if (rectangle.min.x >= rectangle.max.x || rectangle.min.y >= rectangle.max.y)
        throw std::invalid_argument("Empty debug rectangle");
    const Vec2 a = rectangle.min, b{rectangle.max.x, rectangle.min.y}, c = rectangle.max,
               d{rectangle.min.x, rectangle.max.y};
    debug_line(renderer, a, b, width, color);
    debug_line(renderer, b, c, width, color);
    debug_line(renderer, c, d, width, color);
    debug_line(renderer, d, a, width, color);
}
// Circles use 32 segments; these are outlines of simulation shapes, not interpolated sprites.
inline void debug_shape(Renderer& renderer, Shape2D shape, float width, Color color) {
    const auto box = bounds(shape); // Validate the complete shape before drawing anything.
    debug_detail::validate(box.min, width, color);
    debug_detail::validate(box.max, width, color);
    if (shape.kind == ShapeKind::box) {
        debug_rect(renderer, box, width, color);
        return;
    }
    constexpr unsigned segments = 32;
    Vec2 previous = shape.center + Vec2{shape.half.x, 0};
    for (unsigned i = 1; i <= segments; ++i) {
        const float angle =
            static_cast<float>(i % segments) * (2 * std::numbers::pi_v<float> / segments);
        const auto point = shape.center + Vec2{std::cos(angle), std::sin(angle)} * shape.half.x;
        debug_line(renderer, previous, point, width, color);
        previous = point;
    }
}
} // namespace engine
