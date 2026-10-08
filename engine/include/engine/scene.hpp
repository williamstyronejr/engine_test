#pragma once
#include "engine/math.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace engine {
struct Entity {
    std::uint32_t index{std::numeric_limits<std::uint32_t>::max()};
    std::uint64_t generation{}, domain{};
    bool operator==(const Entity&) const = default;
    explicit operator bool() const { return domain != 0; }
};
struct LocalTransform {
    Vec2 position{};
    float rotation{};
    Vec2 scale{1, 1};
};
struct Sprite {
    std::string texture;
    Vec2 size{1, 1};
    float r{1}, g{1}, b{1}, a{1};
    int layer{};
};
struct Collider {
    Vec2 half{0.5F, 0.5F};
};
struct SceneNode {
    Entity entity;
    std::uint64_t id{};
    std::string name, tag;
    LocalTransform local;
    Entity parent;
    std::optional<Sprite> sprite;
    std::optional<Collider> collider;
};
class Scene {
  public:
    static constexpr std::size_t capacity = 4096, max_depth = 64;
    Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&& other);
    Scene& operator=(Scene&& other);
    Entity create(std::uint64_t persistent_id, std::string name, std::string tag = {});
    bool valid(Entity entity) const;
    Entity find(std::uint64_t persistent_id) const;
    const SceneNode& get(Entity entity) const;
    void set_transform(Entity entity, LocalTransform transform);
    void set_parent(Entity child, Entity parent);
    void set_sprite(Entity entity, Sprite sprite);
    void set_collider(Entity entity, Collider collider);
    Transform world_transform(Entity entity) const;
    // Sample local poses without mutating the scene (for interpolated/procedural rendering).
    // The callback must not mutate the scene and returns one LocalTransform per ancestor.
    template <class Sample>
    Transform sampled_world_transform(Entity entity, Sample&& sample) const {
        get(entity); // Reject null, stale and foreign handles before invoking the callback.
        Transform result;
        for (auto p = entity; p; p = get(p).parent) {
            const auto t = sample(get(p));
            result = Transform::from(t.position, t.rotation, t.scale) * result;
        }
        for (float value : {result.a, result.b, result.c, result.d, result.x, result.y})
            if (!std::isfinite(value) || std::abs(value) > 1e12F)
                throw std::runtime_error("World transform exceeds numeric limits");
        return result;
    }
    void destroy(Entity entity); // Removes subtree. Structural mutation only outside each().
    void defer_destroy(Entity entity);
    void flush();
    // Reserved vector payload bytes; excludes allocator overhead and string/hash storage.
    std::size_t buffer_bytes() const;
    std::size_t size() const { return nodes_.size(); }
    template <class F> void each(F&& function) const {
        ++iteration_depth_;
        try {
            for (const auto& node : nodes_)
                function(node);
        } catch (...) {
            --iteration_depth_;
            throw;
        }
        --iteration_depth_;
    }

  private:
    struct Slot {
        std::uint64_t generation{1};
        std::uint32_t dense{};
        bool alive{};
    };
    void require_mutable() const;
    SceneNode& mutable_node(Entity entity);
    std::uint64_t domain_{};
    std::vector<Slot> slots_;
    std::vector<SceneNode> nodes_;
    std::vector<Entity> pending_;
    std::unordered_map<std::uint64_t, Entity> ids_;
    mutable unsigned int iteration_depth_{};
};
Scene parse_scene(std::string_view text, std::string_view source = "<scene>");
std::string serialize_scene(const Scene& scene);
// Parses a complete replacement before committing; old handles become invalid on success.
void replace_scene(Scene& live, std::string_view text, std::string_view source = "<scene>");
} // namespace engine
