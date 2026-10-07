#pragma once
#include "engine/math.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace engine {
enum class ShapeKind { box, circle };
struct Shape2D {
    ShapeKind kind{ShapeKind::box};
    Vec2 center{}, half{0.5F, 0.5F}; // Circle uses half.x as radius; half.y must match.
    static Shape2D box(Vec2 center, Vec2 half) { return {ShapeKind::box, center, half}; }
    static Shape2D circle(Vec2 center, float radius) {
        return {ShapeKind::circle, center, {radius, radius}};
    }
};
Rect bounds(Shape2D shape);
bool intersects(Shape2D a, Shape2D b); // Closed shapes: touching counts as contact.
struct Body2D {
    std::uint64_t id{}; // Application-owned identity, nonzero and unique per snapshot.
    Shape2D shape;
    std::uint32_t layer{1}, mask{~0U};
    bool moving{true}, sensor{};
};
struct Contact2D {
    std::uint64_t a{}, b{}; // Sorted by ID, independent of input order.
    bool sensor{};
    bool operator==(const Contact2D&) const = default;
};
enum class TriggerPhase { enter, stay, exit };
struct TriggerEvent {
    std::uint64_t a{}, b{};
    TriggerPhase phase{};
    bool operator==(const TriggerEvent&) const = default;
};
struct QueryFilter {
    std::uint32_t mask{~0U};
    std::uint64_t ignore{};
    bool sensors{true};
};
struct SegmentHit {
    std::uint64_t id{};
    float fraction{};       // [0,1] along start->end; inside starts return zero.
    Vec2 point{}, normal{}; // Inside starts have zero normal.
};
struct CollisionStats {
    std::size_t memberships{}, candidate_pairs{}, narrow_tests{};
};
// Bounded uniform-grid detection. Caller integrates poses; this is not a rigid-body solver.
class CollisionWorld {
  public:
    static constexpr std::size_t max_bodies = 256, max_memberships = 16384;
    static constexpr std::size_t max_pairs = max_bodies * (max_bodies - 1) / 2;
    static constexpr float cell_size = 4;
    CollisionWorld();
    CollisionWorld(const CollisionWorld&) = delete;
    CollisionWorld& operator=(const CollisionWorld&) = delete;
    CollisionWorld(CollisionWorld&&) noexcept = default;
    CollisionWorld& operator=(CollisionWorld&&) noexcept = default;
    // Transactional snapshot. Invalid input/capacity failures preserve published state/events.
    void update(std::span<const Body2D> bodies);
    // Rebuild contact history on load/restart without emitting historical enter events.
    void prime(std::span<const Body2D> bodies);
    // Published snapshot; spans expire on update, prime, move, or destruction.
    std::span<const Body2D> bodies() const { return bodies_; }
    std::span<const Contact2D> contacts() const { return contacts_; }
    std::span<const TriggerEvent> events() const { return events_; }
    CollisionStats stats() const { return stats_; }
    void query(Shape2D shape, std::vector<std::uint64_t>& output, QueryFilter filter = {}) const;
    std::optional<SegmentHit> segment(Vec2 start, Vec2 end, QueryFilter filter = {}) const;

  private:
    struct Cell {
        int x{}, y{};
        std::size_t body{};
    };
    std::vector<Body2D> bodies_, pending_;
    std::vector<Cell> cells_;
    std::vector<Contact2D> contacts_, next_contacts_, active_, next_active_;
    std::vector<TriggerEvent> events_, next_events_;
    std::array<std::uint64_t, max_bodies * max_bodies / 64> seen_{};
    CollisionStats stats_{};
};
} // namespace engine
