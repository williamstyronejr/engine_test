#include "engine/collision_world.hpp"
#include <algorithm>
#include <cmath>
#include <tuple>

namespace engine {
std::size_t CollisionWorld::buffer_bytes() const {
    return (bodies_.capacity() + pending_.capacity()) * sizeof(Body2D) +
           cells_.capacity() * sizeof(Cell) +
           (contacts_.capacity() + next_contacts_.capacity() + active_.capacity() +
            next_active_.capacity()) *
               sizeof(Contact2D) +
           (events_.capacity() + next_events_.capacity()) * sizeof(TriggerEvent) + sizeof(seen_);
}
CollisionMemoryUsage CollisionWorld::memory_usage() const {
    CollisionMemoryUsage result;
    result.groups = {{
        {(bodies_.capacity() + pending_.capacity()) * sizeof(Body2D), sizeof(Body2D), 2,
         peak_entries_[0]},
        {cells_.capacity() * sizeof(Cell), sizeof(Cell), 1, peak_entries_[1]},
        {(contacts_.capacity() + next_contacts_.capacity()) * sizeof(Contact2D), sizeof(Contact2D),
         2, peak_entries_[2]},
        {(active_.capacity() + next_active_.capacity()) * sizeof(Contact2D), sizeof(Contact2D),
         2, peak_entries_[3]},
        {(events_.capacity() + next_events_.capacity()) * sizeof(TriggerEvent), sizeof(TriggerEvent),
         2, peak_entries_[4]},
    }};
    result.fixed_bytes = sizeof(seen_);
    return result;
}

namespace {
void point_valid(Vec2 p) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || std::abs(p.x) > 100000 ||
        std::abs(p.y) > 100000)
        throw std::invalid_argument("Collision coordinate outside finite +/-100000 range");
}
void validate(Shape2D s) {
    point_valid(s.center);
    if ((s.kind != ShapeKind::box && s.kind != ShapeKind::circle) || !std::isfinite(s.half.x) ||
        !std::isfinite(s.half.y) || s.half.x <= 0 || s.half.y <= 0 || s.half.x > 1024 ||
        s.half.y > 1024 || (s.kind == ShapeKind::circle && s.half.x != s.half.y))
        throw std::invalid_argument("Invalid collision shape");
}
bool boxes(Rect a, Rect b) {
    return a.min.x <= b.max.x && a.max.x >= b.min.x && a.min.y <= b.max.y && a.max.y >= b.min.y;
}
Rect raw_bounds(Shape2D s) {
    return {s.center - s.half, s.center + s.half};
}
bool narrow(Shape2D a, Shape2D b) {
    if (a.kind == ShapeKind::box && b.kind == ShapeKind::box)
        return boxes(raw_bounds(a), raw_bounds(b));
    if (a.kind == ShapeKind::box)
        std::swap(a, b);
    double dx{}, dy{}, radius = a.half.x;
    if (b.kind == ShapeKind::circle) {
        dx = static_cast<double>(a.center.x) - b.center.x;
        dy = static_cast<double>(a.center.y) - b.center.y;
        radius += b.half.x;
    } else {
        const auto box = raw_bounds(b);
        dx = static_cast<double>(a.center.x) - std::clamp(a.center.x, box.min.x, box.max.x);
        dy = static_cast<double>(a.center.y) - std::clamp(a.center.y, box.min.y, box.max.y);
    }
    return dx * dx + dy * dy <= radius * radius;
}
bool less(Contact2D a, Contact2D b) {
    return std::tie(a.a, a.b) < std::tie(b.a, b.b);
}
bool accepts(const Body2D& b, QueryFilter f) {
    return b.id != f.ignore && (b.layer & f.mask) && (f.sensors || !b.sensor);
}
struct Cast {
    double t{};
    Vec2 normal{};
};
std::optional<Cast> cast_box(Vec2 start, Vec2 end, Rect b) {
    double low = 0, high = 1;
    Vec2 normal{};
    for (int axis = 0; axis < 2; ++axis) {
        const double p = axis ? start.y : start.x;
        const double d =
            axis ? static_cast<double>(end.y) - start.y : static_cast<double>(end.x) - start.x;
        const double lo = axis ? b.min.y : b.min.x, hi = axis ? b.max.y : b.max.x;
        if (d == 0) {
            if (p < lo || p > hi)
                return {};
            continue;
        }
        double a = (lo - p) / d, z = (hi - p) / d;
        const float sign = d > 0 ? -1.0F : 1.0F;
        if (a > z)
            std::swap(a, z);
        if (a > low) {
            low = a;
            normal = axis ? Vec2{0, sign} : Vec2{sign, 0};
        }
        high = std::min(high, z);
        if (low > high)
            return {};
    }
    return Cast{low, normal};
}
std::optional<Cast> cast_circle(Vec2 start, Vec2 end, Shape2D s) {
    const double x = static_cast<double>(start.x) - s.center.x,
                 y = static_cast<double>(start.y) - s.center.y;
    const double dx = static_cast<double>(end.x) - start.x,
                 dy = static_cast<double>(end.y) - start.y;
    const double c = x * x + y * y - static_cast<double>(s.half.x) * s.half.x;
    if (c <= 0)
        return Cast{};
    const double a = dx * dx + dy * dy, b = x * dx + y * dy;
    if (a == 0 || b >= 0)
        return {};
    const double discriminant = b * b - a * c;
    if (discriminant < 0)
        return {};
    const double t = c / (-b + std::sqrt(discriminant)); // Stable smaller root.
    if (t < 0 || t > 1)
        return {};
    return Cast{t, normalized({static_cast<float>(x + dx * t), static_cast<float>(y + dy * t)})};
}
} // namespace
Rect bounds(Shape2D shape) {
    validate(shape);
    return raw_bounds(shape);
}
bool intersects(Shape2D a, Shape2D b) {
    validate(a);
    validate(b);
    return narrow(a, b);
}
CollisionWorld::CollisionWorld() : CollisionWorld(Limits{}) {}
CollisionWorld::CollisionWorld(Limits limits) : limits_(limits) {
    if (limits.contacts > max_pairs || limits.events > max_pairs * 2 ||
        limits.memberships > max_memberships)
        throw std::invalid_argument("Collision budgets exceed engine limits");
    bodies_.reserve(max_bodies);
    pending_.reserve(max_bodies);
    cells_.reserve(limits.memberships);
    for (auto* v : {&contacts_, &next_contacts_, &active_, &next_active_})
        v->reserve(limits.contacts);
    events_.reserve(limits.events);
    next_events_.reserve(limits.events);
}
void CollisionWorld::update(std::span<const Body2D> bodies) {
    if (pending_.capacity() < max_bodies)
        throw std::logic_error("Reassign a moved-from collision world before updating");
    if (bodies.size() > max_bodies)
        throw std::invalid_argument("Collision body limit exceeded");
    pending_.assign(bodies.begin(), bodies.end());
    std::sort(pending_.begin(), pending_.end(), [](auto& a, auto& b) { return a.id < b.id; });
    cells_.clear();
    next_contacts_.clear();
    next_active_.clear();
    next_events_.clear();
    seen_.fill(0);
    CollisionStats stats{};
    for (std::size_t i = 0; i < pending_.size(); ++i) {
        const auto& body = pending_[i];
        validate(body.shape);
        if (!body.id || !body.layer || (i && pending_[i - 1].id == body.id))
            throw std::invalid_argument(
                "Collision body ID/layer must be nonzero; IDs must be unique");
        if (!body.mask)
            continue; // Query-only bodies cannot participate in any reciprocal-filter pair.
        const auto b = raw_bounds(body.shape);
        const int x0 = static_cast<int>(std::floor(b.min.x / cell_size)),
                  x1 = static_cast<int>(std::floor(b.max.x / cell_size));
        const int y0 = static_cast<int>(std::floor(b.min.y / cell_size)),
                  y1 = static_cast<int>(std::floor(b.max.y / cell_size));
        const auto count =
            static_cast<std::size_t>(x1 - x0 + 1) * static_cast<std::size_t>(y1 - y0 + 1);
        if (count > 256)
            throw std::invalid_argument("Collision grid membership limit exceeded");
        if (count > limits_.memberships - cells_.size())
            throw std::length_error("Collision grid membership budget exceeded");
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                cells_.push_back({x, y, i});
    }
    std::sort(cells_.begin(), cells_.end(), [](auto a, auto b) {
        return std::tie(a.x, a.y, a.body) < std::tie(b.x, b.y, b.body);
    });
    for (std::size_t first = 0; first < cells_.size();) {
        std::size_t last = first + 1;
        while (last < cells_.size() && cells_[last].x == cells_[first].x &&
               cells_[last].y == cells_[first].y)
            ++last;
        for (std::size_t i = first; i < last; ++i)
            for (std::size_t j = i + 1; j < last; ++j) {
                const auto ai = cells_[i].body, bi = cells_[j].body;
                const auto bit = ai * max_bodies + bi;
                const auto mask = std::uint64_t{1} << (bit % 64);
                if (seen_[bit / 64] & mask)
                    continue;
                seen_[bit / 64] |= mask;
                ++stats.candidate_pairs;
                const auto& a = pending_[ai];
                const auto& b = pending_[bi];
                if ((!a.moving && !b.moving) || !(a.mask & b.layer) || !(b.mask & a.layer) ||
                    !boxes(raw_bounds(a.shape), raw_bounds(b.shape)))
                    continue;
                ++stats.narrow_tests;
                if (narrow(a.shape, b.shape)) {
                    if (next_contacts_.size() == limits_.contacts)
                        throw std::length_error("Collision contact budget exceeded");
                    next_contacts_.push_back({a.id, b.id, a.sensor || b.sensor});
                }
            }
        first = last;
    }
    std::sort(next_contacts_.begin(), next_contacts_.end(), less);
    for (auto c : next_contacts_)
        if (c.sensor)
            next_active_.push_back(c);
    std::size_t old = 0, now = 0;
    while (old < active_.size() || now < next_active_.size()) {
        if (next_events_.size() == limits_.events)
            throw std::length_error("Collision event budget exceeded");
        if (now == next_active_.size() ||
            (old < active_.size() && less(active_[old], next_active_[now]))) {
            const auto p = active_[old++];
            next_events_.push_back({p.a, p.b, TriggerPhase::exit});
        } else if (old == active_.size() || less(next_active_[now], active_[old])) {
            const auto p = next_active_[now++];
            next_events_.push_back({p.a, p.b, TriggerPhase::enter});
        } else {
            const auto p = next_active_[now++];
            ++old;
            next_events_.push_back({p.a, p.b, TriggerPhase::stay});
        }
    }
    stats.memberships = cells_.size();
    // Capture successful candidates before prime can suppress their events.
    const std::array entries{pending_.size(), cells_.size(), next_contacts_.size(),
                             next_active_.size(), next_events_.size()};
    for (std::size_t i = 0; i < entries.size(); ++i)
        peak_entries_[i] = std::max(peak_entries_[i], entries[i]);
    bodies_.swap(pending_);
    contacts_.swap(next_contacts_);
    active_.swap(next_active_);
    events_.swap(next_events_);
    stats_ = stats;
}
void CollisionWorld::prime(std::span<const Body2D> bodies) {
    update(bodies);
    events_.clear();
}
void CollisionWorld::query(Shape2D shape, std::vector<std::uint64_t>& output,
                           QueryFilter filter) const {
    validate(shape);
    output.clear();
    for (const auto& body : bodies_)
        if (accepts(body, filter) && boxes(raw_bounds(shape), raw_bounds(body.shape)) &&
            narrow(shape, body.shape))
            output.push_back(body.id);
}
std::optional<SegmentHit> CollisionWorld::segment(Vec2 start, Vec2 end, QueryFilter filter) const {
    point_valid(start);
    point_valid(end);
    std::optional<SegmentHit> best;
    double best_t = 2;
    for (const auto& body : bodies_) {
        if (!accepts(body, filter))
            continue;
        const auto box = cast_box(start, end, raw_bounds(body.shape));
        if (!box)
            continue;
        const auto hit =
            body.shape.kind == ShapeKind::box ? box : cast_circle(start, end, body.shape);
        if (hit && hit->t < best_t) {
            best_t = hit->t;
            const auto fraction = static_cast<float>(hit->t);
            best = SegmentHit{body.id, fraction, lerp(start, end, fraction), hit->normal};
        }
    }
    return best;
}
} // namespace engine
