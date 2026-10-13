#pragma once
#include "engine/component_pool.hpp"
#include "engine/math.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace engine {
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
    static constexpr std::size_t max_component_types = 64;
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
    void destroy(Entity entity); // Removes subtree and components; only outside iteration.
    void defer_destroy(Entity entity);
    void flush();
    // Runtime components are not part of the scene text format. The application
    // rebuilds them after loading. Structural operations invalidate references;
    // store Entity handles, not payload pointers, across structural changes.
    template <Component T, class... Args> T& add_component(Entity entity, Args&&... args) {
        require_mutable();
        get(entity);
        IterationScope guard(*this); // Reject constructor-driven structural reentry.
        if (auto* existing = pool<T>())
            return existing->add(entity, std::forward<Args>(args)...);
        if (components_.size() == max_component_types)
            throw std::length_error("Scene component type limit reached");
        auto candidate = std::make_unique<detail::ComponentPool<T>>(capacity);
        auto& result = candidate->add(entity, std::forward<Args>(args)...);
        components_.emplace(std::type_index(typeid(T)), std::move(candidate));
        return result;
    }
    template <Component T> T* try_component(Entity entity) {
        auto* values = pool<T>();
        return valid(entity) && values ? values->find(entity) : nullptr;
    }
    template <Component T> const T* try_component(Entity entity) const {
        const auto* values = pool<T>();
        return valid(entity) && values ? values->find(entity) : nullptr;
    }
    template <Component T> T& get_component(Entity entity) {
        if (auto* value = try_component<T>(entity))
            return *value;
        throw std::invalid_argument("Invalid entity or missing component");
    }
    template <Component T> const T& get_component(Entity entity) const {
        if (const auto* value = try_component<T>(entity))
            return *value;
        throw std::invalid_argument("Invalid entity or missing component");
    }
    template <Component T> bool remove_component(Entity entity) {
        require_mutable();
        get(entity);
        auto* values = pool<T>();
        if (!values || !values->find(entity))
            return false;
        IterationScope guard(*this);
        values->erase(entity);
        return true;
    }
    template <Component T> std::size_t component_count() const {
        const auto* values = pool<T>();
        return values ? values->size() : 0;
    }
    // Queries scan the first pool and intersect subsequent types via sparse
    // lookup. Order is dense insertion/swap-erase order, never a persistence ID.
    // Payload edits and defer_destroy are allowed; structural edits are rejected.
    template <Component First, Component... Rest, class F> void each_component(F&& function) {
        visit_components<First, Rest...>(*this, function);
    }
    template <Component First, Component... Rest, class F> void each_component(F&& function) const {
        visit_components<First, Rest...>(*this, function);
    }
    // Reserved node/handle/pool vector bytes; excludes allocator/registry overhead
    // and allocations owned by strings or user components.
    std::size_t buffer_bytes() const;
    std::size_t size() const { return nodes_.size(); }
    template <class F> void each(F&& function) const {
        IterationScope guard(*this);
        for (const auto& node : nodes_)
            function(node);
    }

  private:
    struct IterationScope {
        const Scene& scene;
        explicit IterationScope(const Scene& owner) : scene(owner) { ++scene.iteration_depth_; }
        ~IterationScope() { --scene.iteration_depth_; }
        IterationScope(const IterationScope&) = delete;
        IterationScope& operator=(const IterationScope&) = delete;
    };
    template <Component T> detail::ComponentPool<T>* pool() {
        const auto found = components_.find(std::type_index(typeid(T)));
        return found == components_.end()
                   ? nullptr
                   : static_cast<detail::ComponentPool<T>*>(found->second.get());
    }
    template <Component T> const detail::ComponentPool<T>* pool() const {
        const auto found = components_.find(std::type_index(typeid(T)));
        return found == components_.end()
                   ? nullptr
                   : static_cast<const detail::ComponentPool<T>*>(found->second.get());
    }
    template <Component First, Component... Rest, class Self, class F>
    static void visit_components(Self& scene, F& function) {
        auto* first = scene.template pool<First>();
        const auto others = std::tuple{scene.template pool<Rest>()...};
        if (!first ||
            !std::apply([](auto*... pools) { return (... && (pools != nullptr)); }, others))
            return;
        IterationScope guard(scene);
        for (auto& entry : first->entries())
            std::apply(
                [&](auto*... pools) {
                    const auto matches = std::tuple{pools->find(entry.entity)...};
                    std::apply(
                        [&](auto*... values) {
                            if ((... && (values != nullptr)))
                                std::invoke(function, entry.entity, entry.value, *values...);
                        },
                        matches);
                },
                others);
    }
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
    std::unordered_map<std::type_index, std::unique_ptr<detail::ComponentStorage>> components_;
    mutable unsigned int iteration_depth_{};
};
Scene parse_scene(std::string_view text, std::string_view source = "<scene>");
std::string serialize_scene(const Scene& scene);
// Parses a complete replacement before committing; old handles become invalid on success.
void replace_scene(Scene& live, std::string_view text, std::string_view source = "<scene>");
} // namespace engine
