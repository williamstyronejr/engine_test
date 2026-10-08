#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace engine {
struct Vec2 {
    float x{}, y{};
    constexpr Vec2 operator+(Vec2 b) const { return {x + b.x, y + b.y}; }
    constexpr Vec2 operator-(Vec2 b) const { return {x - b.x, y - b.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
};
inline Vec2 lerp(Vec2 a, Vec2 b, float t) {
    return a + (b - a) * t;
}
inline Vec2 normalized(Vec2 v) {
    const float length = std::hypot(v.x, v.y);
    return length > 0 ? v * (1.0F / length) : Vec2{};
}
struct Rect {
    Vec2 min, max;
};
inline bool overlaps(Rect a, Rect b) {
    return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y && a.max.y > b.min.y;
}
struct Camera {
    Vec2 center{};
    float height{18.0F};
    Vec2 extent(int width, int pixels_high) const {
        if (width <= 0 || pixels_high <= 0 || !std::isfinite(height) || height <= 0 ||
            !std::isfinite(center.x) || !std::isfinite(center.y))
            throw std::invalid_argument("Invalid camera viewport or height");
        const float horizontal =
            height * (static_cast<float>(width) / static_cast<float>(pixels_high));
        if (!std::isfinite(horizontal) || horizontal <= 0)
            throw std::invalid_argument("Camera extent exceeds numeric limits");
        return {horizontal, height};
    }
    Vec2 screen_to_world(Vec2 p, int width, int pixels_high) const {
        const auto size = extent(width, pixels_high);
        return center + Vec2{(p.x / static_cast<float>(width) - 0.5F) * size.x,
                             (0.5F - p.y / static_cast<float>(pixels_high)) * size.y};
    }
    Vec2 world_to_screen(Vec2 p, int width, int pixels_high) const {
        const auto size = extent(width, pixels_high);
        return {(0.5F + (p.x - center.x) / size.x) * static_cast<float>(width),
                (0.5F - (p.y - center.y) / size.y) * static_cast<float>(pixels_high)};
    }
};
// Translation/rotation/nonuniform scale composed as an affine 2x3 matrix.
struct Transform {
    float a{1}, b{}, c{}, d{1}, x{}, y{};
    static Transform from(Vec2 position, float angle, Vec2 scale = {1, 1}) {
        const float co = std::cos(angle), si = std::sin(angle);
        return {co * scale.x, si * scale.x, -si * scale.y, co * scale.y, position.x, position.y};
    }
    Vec2 apply(Vec2 p) const { return {a * p.x + c * p.y + x, b * p.x + d * p.y + y}; }
    Transform operator*(const Transform& q) const {
        return {a * q.a + c * q.b, b * q.a + d * q.b,     a * q.c + c * q.d,
                b * q.c + d * q.d, a * q.x + c * q.y + x, b * q.x + d * q.y + y};
    }
};
} // namespace engine
