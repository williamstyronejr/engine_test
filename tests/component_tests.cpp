#include "engine/scene.hpp"
#include "test.hpp"
#include <array>
#include <memory>
#include <random>
#include <utility>

namespace {
using namespace engine;
using namespace testing;
struct Health {
    int value;
};
struct Velocity {
    float x, y;
};
struct alignas(128) Aligned {
    int value{9};
};
struct Resource {
    static inline int live{};
    int value;
    explicit Resource(int number) : value(number) { ++live; }
    ~Resource() { --live; }
};
struct Owned {
    std::unique_ptr<Resource> resource;
    explicit Owned(int value) : resource(std::make_unique<Resource>(value)) {}
};
struct Throws {
    int value;
    explicit Throws(int number) : value(number) {
        if (number < 0)
            throw std::runtime_error("component constructor");
    }
};
struct Reentrant {
    explicit Reentrant(Scene& scene) { scene.create(999, "constructor reentry"); }
};
struct ThrowingMove {
    ThrowingMove(ThrowingMove&&) noexcept(false);
    ThrowingMove& operator=(ThrowingMove&&) noexcept(false);
};
static_assert(Component<Health> && Component<Owned>);
static_assert(!Component<const Health> && !Component<ThrowingMove>);
static_assert(std::is_same_v<decltype(std::declval<const Scene&>().try_component<Health>({})),
                             const Health*>);

TEST(component_access_rejects_stale_foreign_and_missing_handles) {
    Scene scene, foreign;
    const auto a = scene.create(1, "a");
    const auto other = foreign.create(1, "foreign");
    CHECK(!scene.try_component<Health>(a));
    CHECK(!scene.try_component<Health>({}));
    rejects([&] { scene.get_component<Health>(a); });
    rejects([&] { scene.add_component<Health>(other, 100); });
    CHECK(scene.add_component<Health>(a, 100).value == 100);
    rejects([&] { scene.add_component<Health>(a, 5); });
    CHECK(!scene.try_component<Health>(other));
    CHECK(scene.component_count<Health>() == 1);
    scene.destroy(a);
    const auto replacement = scene.create(2, "reused slot");
    CHECK(a.index == replacement.index && a.generation != replacement.generation);
    scene.add_component<Health>(replacement, 20);
    CHECK(!scene.try_component<Health>(a));
    rejects([&] { scene.get_component<Health>(a); });
    rejects([&] { scene.remove_component<Health>(a); });
    rejects([&] { scene.add_component<Health>(a, 3); });
    CHECK(scene.get_component<Health>(replacement).value == 20);
}
TEST(swap_erase_keeps_other_components_and_owned_resources_intact) {
    CHECK(Resource::live == 0);
    {
        Scene scene;
        std::array<Entity, 5> entities{};
        for (std::size_t i = 0; i < entities.size(); ++i) {
            entities[i] = scene.create(i + 1, "resource");
            scene.add_component<Owned>(entities[i], static_cast<int>(i));
            scene.add_component<Health>(entities[i], static_cast<int>(i + 10));
        }
        CHECK(Resource::live == 5);
        CHECK(scene.remove_component<Owned>(entities[1]));
        CHECK(!scene.remove_component<Owned>(entities[1]));
        CHECK(Resource::live == 4);
        CHECK(scene.get_component<Owned>(entities[4]).resource->value == 4);
        CHECK(scene.get_component<Health>(entities[1]).value == 11);
        scene.destroy(entities[0]);
        CHECK(Resource::live == 3 && scene.component_count<Health>() == 4);
        CHECK(scene.get_component<Owned>(entities[4]).resource->value == 4);
        CHECK(scene.remove_component<Owned>(entities[3]));
        scene.add_component<Owned>(entities[1], 42);
        CHECK(scene.get_component<Owned>(entities[1]).resource->value == 42);
    }
    CHECK(Resource::live == 0);
}
TEST(component_queries_join_types_and_propagate_constness) {
    Scene scene;
    const auto a = scene.create(1, "a"), b = scene.create(2, "b"), c = scene.create(3, "c");
    scene.add_component<Health>(a, 1);
    scene.add_component<Health>(b, 2);
    scene.add_component<Velocity>(b, 3.F, 4.F);
    scene.add_component<Velocity>(c, 5.F, 6.F);
    const auto& aligned = scene.add_component<Aligned>(b);
    CHECK(reinterpret_cast<std::uintptr_t>(&aligned) % alignof(Aligned) == 0);
    int visits = 0;
    scene.each_component<Health, Velocity>([&](Entity entity, Health& h, Velocity& v) {
        CHECK(entity == b);
        h.value += 10;
        v.x += 1;
        ++visits;
    });
    CHECK(visits == 1 && scene.get_component<Health>(b).value == 12);
    std::as_const(scene).each_component<Velocity, Health>(
        [&](Entity entity, const Velocity& v, const Health& h) {
            CHECK(entity == b && h.value == 12 && v.x == 4);
            ++visits;
        });
    scene.each_component<Health, Owned>([](Entity, Health&, Owned&) { CHECK(false); });
    CHECK(visits == 2 && scene.component_count<Owned>() == 0);
    scene.each_component<Health, Velocity, Aligned>(
        [&](Entity entity, Health&, Velocity&, Aligned& value) {
            CHECK(entity == b && value.value == 9);
            ++visits;
        });
    CHECK(visits == 3);
}
TEST(nested_queries_guard_structure_and_release_guard_after_exceptions) {
    Scene scene;
    const auto a = scene.create(1, "a");
    scene.add_component<Health>(a, 5);
    scene.each_component<Health>([&](Entity entity, Health& h) {
        rejects([&] { scene.create(2, "blocked"); });
        rejects([&] { scene.destroy(entity); });
        rejects([&] { scene.add_component<Velocity>(entity, 1.F, 0.F); });
        rejects([&] { scene.remove_component<Health>(entity); });
        rejects([&] { scene.flush(); });
        rejects([&] { scene.set_transform(entity, {}); });
        scene.each(
            [&](const SceneNode&) { rejects([&] { scene.remove_component<Health>(entity); }); });
        scene.each_component<Health>([](Entity, Health& inner) { inner.value += 2; });
        CHECK(h.value == 7);
        rejects([&] {
            scene.each_component<Health>(
                [](Entity, Health&) { throw std::runtime_error("query"); });
        });
        rejects([&] { scene.remove_component<Health>(entity); }); // Outer guard remains active.
    });
    rejects([&] {
        scene.each_component<Health>([](Entity, Health&) { throw std::runtime_error("query"); });
    });
    CHECK(scene.remove_component<Health>(a));
    scene.each([&](const SceneNode&) { rejects([&] { scene.add_component<Health>(a, 1); }); });
    scene.add_component<Health>(a, 8);
}
TEST(deferred_subtree_destruction_removes_all_payloads_once) {
    CHECK(Resource::live == 0);
    Scene scene;
    const auto parent = scene.create(1, "parent"), child = scene.create(2, "child"),
               grandchild = scene.create(3, "grandchild"), survivor = scene.create(4, "survivor");
    scene.set_parent(child, parent);
    scene.set_parent(grandchild, child);
    for (auto e : {parent, child, grandchild, survivor}) {
        scene.add_component<Health>(e, 1);
        scene.add_component<Owned>(e, 2);
    }
    int visits = 0;
    scene.each_component<Owned>([&](Entity, Owned&) {
        scene.defer_destroy(parent);
        scene.defer_destroy(child); // Overlapping queued subtrees remain safe.
        ++visits;
    });
    CHECK(visits == 4 && Resource::live == 4);
    scene.flush();
    CHECK(Resource::live == 1 && scene.component_count<Health>() == 1);
    CHECK(scene.valid(survivor) && !scene.try_component<Owned>(grandchild));
    scene.flush();
    scene.destroy(survivor);
    CHECK(Resource::live == 0);
}
TEST(failed_construction_preserves_pools_and_prevents_structural_reentry) {
    Scene scene;
    const auto a = scene.create(1, "a"), b = scene.create(2, "b");
    const auto bytes = scene.buffer_bytes();
    rejects([&] { scene.add_component<Throws>(a, -1); });
    CHECK(scene.component_count<Throws>() == 0 && scene.buffer_bytes() == bytes);
    scene.add_component<Throws>(a, 7);
    const auto allocated = scene.buffer_bytes();
    rejects([&] { scene.add_component<Throws>(b, -1); });
    CHECK(scene.component_count<Throws>() == 1 && scene.buffer_bytes() == allocated);
    CHECK(scene.get_component<Throws>(a).value == 7);
    rejects([&] { scene.add_component<Reentrant>(b, scene); });
    CHECK(scene.size() == 2 && scene.component_count<Reentrant>() == 0);
    scene.add_component<Throws>(b, 8);
}
TEST(scene_moves_transfer_components_and_reject_active_source_or_destination) {
    CHECK(Resource::live == 0);
    {
        Scene source, target;
        const auto a = source.create(1, "source"), old = target.create(1, "old target");
        source.add_component<Owned>(a, 10);
        target.add_component<Owned>(old, 20);
        source.each_component<Owned>([&](Entity, Owned&) {
            rejects([&] { target = std::move(source); });
            rejects([&] { Scene invalid(std::move(source)); });
        });
        target.each_component<Owned>(
            [&](Entity, Owned&) { rejects([&] { target = std::move(source); }); });
        CHECK(Resource::live == 2 && source.valid(a) && target.valid(old));
        target = std::move(source);
        CHECK(Resource::live == 1 && target.valid(a) && !target.valid(old));
        CHECK(!source.try_component<Owned>(a) && source.component_count<Owned>() == 0);
        CHECK(target.get_component<Owned>(a).resource->value == 10);
        const auto fresh = source.create(2, "moved-from reuse");
        source.add_component<Owned>(fresh, 30);
        Scene moved(std::move(target));
        CHECK(moved.valid(a) && !target.valid(a));
        CHECK(moved.get_component<Owned>(a).resource->value == 10);
    }
    CHECK(Resource::live == 0);
}
TEST(scene_serialization_excludes_runtime_pools_and_replacement_cleans_them) {
    Scene scene;
    const auto a = scene.create(1, "saved");
    const auto text = serialize_scene(scene);
    scene.add_component<Health>(a, 100);
    CHECK(serialize_scene(scene) == text);
    rejects([&] { replace_scene(scene, "invalid"); });
    CHECK(scene.get_component<Health>(a).value == 100);
    auto loaded = parse_scene(text);
    CHECK(loaded.component_count<Health>() == 0);
    replace_scene(scene, text);
    CHECK(!scene.valid(a) && scene.component_count<Health>() == 0);
    scene.add_component<Health>(scene.find(1), 50);
}
template <std::size_t N> struct Marker {
    std::size_t value{N};
};
template <std::size_t... N>
void fill_types(Scene& scene, Entity entity, std::index_sequence<N...>) {
    (scene.add_component<Marker<N>>(entity), ...);
}
TEST(component_entity_and_type_limits_are_bounded) {
    Scene scene;
    for (std::size_t i = 0; i < Scene::capacity; ++i)
        scene.add_component<Health>(scene.create(i + 1, "full"), static_cast<int>(i));
    CHECK(scene.component_count<Health>() == Scene::capacity);
    const auto first = scene.find(1), last = scene.find(Scene::capacity);
    scene.destroy(first);
    const auto reused = scene.create(Scene::capacity + 1, "reused");
    scene.add_component<Health>(reused, 999);
    CHECK(scene.get_component<Health>(last).value == static_cast<int>(Scene::capacity - 1));
    CHECK(scene.component_count<Health>() == Scene::capacity);
    Scene types;
    const auto entity = types.create(1, "all types");
    fill_types(types, entity, std::make_index_sequence<Scene::max_component_types>{});
    rejects([&] { types.add_component<Marker<Scene::max_component_types>>(entity); });
    CHECK(types.get_component<Marker<0>>(entity).value == 0);
    // Empty pools are retained for reuse, including their registry slot.
    types.remove_component<Marker<0>>(entity);
    types.add_component<Marker<0>>(entity);
}
TEST(randomized_pool_operations_match_an_independent_entity_model) {
    Scene scene;
    struct Expected {
        Entity entity;
        int health{};
        bool present{};
    };
    std::array<Expected, 48> model{};
    std::mt19937 random(714);
    std::uint64_t id = 1;
    for (int step = 0; step < 2500; ++step) {
        auto& item = model[random() % model.size()];
        if (!item.entity)
            item.entity = scene.create(id++, "model");
        else if (random() % 4 == 0) {
            const auto old = item.entity;
            scene.destroy(old);
            item = {};
            CHECK(!scene.try_component<Health>(old));
        } else if (item.present) {
            CHECK(scene.remove_component<Health>(item.entity));
            item.present = false;
        } else {
            item.health = step;
            scene.add_component<Health>(item.entity, step);
            item.present = true;
        }
        std::size_t expected_count = 0, visited = 0;
        for (const auto& expected : model) {
            const auto* value = scene.try_component<Health>(expected.entity);
            CHECK((value != nullptr) == expected.present);
            if (expected.present) {
                CHECK(value->value == expected.health);
                ++expected_count;
            }
        }
        scene.each_component<Health>([&](Entity entity, const Health& value) {
            bool found = false;
            for (const auto& expected : model)
                if (expected.entity == entity) {
                    CHECK(expected.present && value.value == expected.health);
                    found = true;
                }
            CHECK(found);
            ++visited;
        });
        CHECK(visited == expected_count && scene.component_count<Health>() == expected_count);
    }
}
} // namespace
int main() {
    return testing::run_tests();
}
