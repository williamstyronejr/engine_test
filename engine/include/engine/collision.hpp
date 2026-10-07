#pragma once
#include "engine/math.hpp"
#include <span>

namespace engine {
// Axis-separated swept AABB movement against static boxes. Preconditions: positive
// half-extents, no initial penetration. Supports wall sliding, not moving bodies.
inline Vec2 move_box(Vec2 position, Vec2 half, Vec2 delta, std::span<const Rect> walls) {
    float dx = delta.x;
    for (const auto& w : walls) {
        if (position.y + half.y <= w.min.y || position.y - half.y >= w.max.y)
            continue;
        if (dx > 0 && position.x + half.x <= w.min.x)
            dx = std::min(dx, w.min.x - position.x - half.x);
        if (dx < 0 && position.x - half.x >= w.max.x)
            dx = std::max(dx, w.max.x - position.x + half.x);
    }
    position.x += dx;
    float dy = delta.y;
    for (const auto& w : walls) {
        if (position.x + half.x <= w.min.x || position.x - half.x >= w.max.x)
            continue;
        if (dy > 0 && position.y + half.y <= w.min.y)
            dy = std::min(dy, w.min.y - position.y - half.y);
        if (dy < 0 && position.y - half.y >= w.max.y)
            dy = std::max(dy, w.max.y - position.y + half.y);
    }
    position.y += dy;
    return position;
}
} // namespace engine
