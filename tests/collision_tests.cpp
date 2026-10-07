#include "../examples/feature_lab/persistence.hpp"
#include "engine/collision_world.hpp"
#include "test.hpp"
#include <algorithm>
#include <random>

namespace {
using namespace engine;
using namespace testing;
TEST(shape_pairs_touching_corners_and_validation) {
    CHECK(intersects(Shape2D::circle({0, 0}, 1), Shape2D::circle({2, 0}, 1)));
    CHECK(!intersects(Shape2D::circle({0, 0}, 1), Shape2D::circle({2.01F, 0}, 1)));
    const auto box = Shape2D::box({}, {1, 1});
    CHECK(intersects(box, Shape2D::circle({2, 0}, 1)));
    CHECK(!intersects(box, Shape2D::circle({2, 2}, 1)));
    CHECK(intersects(box, Shape2D::circle({1.5F, 1.5F}, 0.71F)));
    CHECK(intersects(Shape2D::circle({}, 0.1F), box));
    CHECK(intersects(box, Shape2D::box({2, 0}, {1, 1})));
    rejects([&] { intersects(box, Shape2D::circle({}, 0)); });
    rejects([&] { intersects(box, Shape2D::box({NAN, 0}, {1, 1})); });
    rejects([&] { intersects(box, {ShapeKind::circle, {}, {1, 2}}); });
}
TEST(grid_matches_brute_force_under_motion_and_input_reordering) {
    CollisionWorld world;
    std::mt19937 random(1421);
    std::uniform_real_distribution<float> position(-30, 30), radius(0.15F, 3);
    std::vector<Body2D> bodies;
    for (std::uint64_t i = 1; i <= 100; ++i) {
        const Vec2 p{position(random), position(random)};
        bodies.push_back({i,
                          i % 2 ? Shape2D::circle(p, radius(random))
                                : Shape2D::box(p, {radius(random), radius(random)}),
                          1U << (i % 3), static_cast<std::uint32_t>(i % 8), i % 4 != 0,
                          i % 5 == 0});
    }
    for (int tick = 0; tick < 80; ++tick) {
        for (auto& b : bodies)
            if (b.moving)
                b.shape.center = b.shape.center + Vec2{0.07F, -0.03F};
        std::shuffle(bodies.begin(), bodies.end(), random);
        world.update(bodies);
        std::vector<Contact2D> expected;
        for (std::size_t a = 0; a < bodies.size(); ++a)
            for (std::size_t b = a + 1; b < bodies.size(); ++b) {
                const auto& x = bodies[a];
                const auto& y = bodies[b];
                if ((x.moving || y.moving) && (x.mask & y.layer) && (y.mask & x.layer) &&
                    intersects(x.shape, y.shape))
                    expected.push_back(
                        {std::min(x.id, y.id), std::max(x.id, y.id), x.sensor || y.sensor});
            }
        std::sort(expected.begin(), expected.end(),
                  [](auto a, auto b) { return std::pair{a.a, a.b} < std::pair{b.a, b.b}; });
        CHECK(std::equal(expected.begin(), expected.end(), world.contacts().begin(),
                         world.contacts().end()));
        CHECK(world.stats().candidate_pairs < bodies.size() * (bodies.size() - 1) / 2);
    }
}
TEST(grid_cell_edges_deduplication_and_static_pairs) {
    CollisionWorld world;
    std::array bodies{Body2D{9, Shape2D::box({-4, -4}, {4, 4})},
                      Body2D{2, Shape2D::box({4, -4}, {4, 4})}};
    world.update(bodies);
    CHECK(world.contacts().size() == 1 && world.contacts()[0].a == 2);
    CHECK(world.stats().candidate_pairs == 1 && world.stats().narrow_tests == 1);
    bodies[0].moving = bodies[1].moving = false;
    world.update(bodies);
    CHECK(world.contacts().empty());
    std::vector<std::uint64_t> found;
    world.query(Shape2D::circle({0, -4}, 0.1F), found);
    CHECK((found == std::vector<std::uint64_t>{2, 9})); // Queries include static bodies.
}
TEST(trigger_enter_stay_exit_filters_removal_and_prime) {
    CollisionWorld world;
    std::array bodies{Body2D{1, Shape2D::box({}, {1, 1}), 1, 2, true, false},
                      Body2D{2, Shape2D::circle({}, 1), 2, 1, true, true}};
    world.update(bodies);
    CHECK(world.events().size() == 1 && world.events()[0].phase == TriggerPhase::enter);
    world.update(bodies);
    CHECK(world.events()[0].phase == TriggerPhase::stay);
    bodies[0].mask = 0;
    world.update(bodies);
    CHECK(world.events()[0].phase == TriggerPhase::exit && world.contacts().empty());
    world.update(bodies);
    CHECK(world.events().empty());
    bodies[0].mask = 2;
    world.prime(bodies);
    CHECK(world.events().empty() && world.contacts().size() == 1);
    world.update(bodies);
    CHECK(world.events()[0].phase == TriggerPhase::stay);
    world.update({});
    CHECK(world.events().size() == 1 && world.events()[0].phase == TriggerPhase::exit);
    world.update(bodies);
    CHECK(world.events()[0].phase == TriggerPhase::enter);
    bodies[1].sensor = false;
    world.update(bodies);
    CHECK(world.events()[0].phase == TriggerPhase::exit && !world.contacts()[0].sensor);
}
TEST(query_masks_ignore_sensor_and_order) {
    CollisionWorld world;
    const std::array bodies{Body2D{7, Shape2D::circle({}, 1), 1, 0, false, true},
                            Body2D{3, Shape2D::box({1, 0}, {1, 1}), 2, 0, false, false}};
    world.update(bodies);
    std::vector<std::uint64_t> found{999};
    world.query(Shape2D::box({}, {2, 2}), found, {2, 0, true});
    CHECK((found == std::vector<std::uint64_t>{3}));
    world.query(Shape2D::box({}, {2, 2}), found, {~0U, 3, true});
    CHECK((found == std::vector<std::uint64_t>{7}));
    world.query(Shape2D::box({}, {2, 2}), found, {~0U, 0, false});
    CHECK((found == std::vector<std::uint64_t>{3}));
    world.query(Shape2D::circle({10, 10}, 1), found);
    CHECK(found.empty());
}
TEST(segment_nearest_ties_normals_inside_and_degenerate) {
    CollisionWorld world;
    const std::array bodies{Body2D{5, Shape2D::circle({5, 0}, 1)},
                            Body2D{2, Shape2D::box({5, 0}, {1, 1})}};
    world.update(bodies);
    const auto hit = world.segment({0, 0}, {10, 0});
    CHECK(hit && hit->id == 2);
    NEAR(hit->fraction, 0.4F);
    NEAR(hit->normal.x, -1.0F);
    NEAR(hit->point.x, 4.0F);
    CHECK(world.segment({5, 0}, {5, 0})->fraction == 0);
    CHECK(!world.segment({0, 0}, {0, 0}));
    CHECK(!world.segment({0, 2}, {10, 2}));
    const auto circle = world.segment({0, 0}, {10, 0}, {~0U, 2, true});
    CHECK(circle && circle->id == 5);
    NEAR(circle->fraction, 0.4F);
    const auto tangent = world.segment({0, 1}, {10, 1}, {~0U, 2, true});
    CHECK(tangent);
    NEAR(tangent->fraction, 0.5F);
    const auto endpoint = world.segment({0, 0}, {4, 0});
    CHECK(endpoint);
    NEAR(endpoint->fraction, 1.0F);
    rejects([&] { world.segment({NAN, 0}, {}); });
}
TEST(transactional_failure_and_capacity_bounds) {
    CollisionWorld world;
    const std::array good{Body2D{1, Shape2D::circle({}, 1), 1, 1, true, true},
                          Body2D{2, Shape2D::circle({}, 1)}};
    world.update(good);
    const auto stats = world.stats();
    const auto check = [&] {
        CHECK(world.contacts().size() == 1 && world.events()[0].phase == TriggerPhase::enter);
        CHECK(world.stats().memberships == stats.memberships);
    };
    auto bad = good;
    bad[1].id = 1;
    rejects([&] { world.update(bad); });
    check();
    bad = good;
    bad[1].shape.half = {1024, 1024};
    rejects([&] { world.update(bad); });
    check();
    std::vector<Body2D> crowded(CollisionWorld::max_bodies + 1, good[0]);
    rejects([&] { world.update(crowded); });
    check();
    crowded.resize(CollisionWorld::max_bodies);
    for (std::size_t i = 0; i < crowded.size(); ++i) {
        crowded[i].id = i + 1;
        crowded[i].shape = Shape2D::box({}, {29, 29});
    }
    rejects([&] { world.update(crowded); });
    check(); // Each fits; total memberships exceed capacity.
    for (auto& body : crowded)
        body.shape = Shape2D::circle({}, 0.1F);
    world.update(crowded);
    CHECK(world.contacts().size() == CollisionWorld::max_pairs);
    CHECK(world.events().size() == CollisionWorld::max_pairs);
    world.update({});
    CHECK(world.events().size() == CollisionWorld::max_pairs);
}
TEST(feature_lab_alarm_switch_pause_and_checkpoint_continuation) {
    const AssetRoot assets(TEST_ASSET_ROOT);
    auto game = feature_lab::load_game(assets);
    InputFrame press{};
    press[static_cast<std::size_t>(Key::interact)].pressed = true;
    game.update(press);
    CHECK(game.alarm_disabled);
    game.update(press);
    CHECK(!game.alarm_disabled);
    feature_lab::Replay replay;
    while (!game.alarm_touching && game.ticks < 150)
        game.update(replay.next(game));
    CHECK(game.alarm_touching && game.alarm_entries == 1);
    const auto bytes = feature_lab::encode_checkpoint(game.checkpoint());
    auto restored = feature_lab::load_game(assets);
    restored.restore(feature_lab::decode_checkpoint(bytes));
    CHECK(restored.alarm_touching && restored.collisions.events().empty());
    for (int i = 0; i < 300; ++i) {
        const auto input = replay.next(game);
        game.update(input);
        restored.update(input);
        CHECK(game.alarm_entries == restored.alarm_entries &&
              game.alarm_touching == restored.alarm_touching);
    }
    const auto ticks = game.ticks, alarms = game.alarm_entries;
    game.paused = true;
    for (int i = 0; i < 30; ++i)
        game.update(press);
    CHECK(game.ticks == ticks && game.alarm_entries == alarms && !game.alarm_disabled);
    game.paused = false;
    game.update(press); // Out of range; switch does nothing.
    CHECK(!game.alarm_disabled);
    auto bad = game.checkpoint();
    bad.alarm_entries = UINT64_MAX;
    rejects([&] { game.restore(bad); });
}
TEST(feature_lab_switch_ray_occlusion_and_disabled_restore) {
    const AssetRoot assets(TEST_ASSET_ROOT);
    auto normal = feature_lab::load_game(assets);
    auto scene = assets.text("facility.scene");
    scene.insert(scene.rfind("end"),
                 "entity 60 \"Screen\" \"wall\" 0 -27 -8.8 0 1 1\ncollider 60 0.6 0.2\n");
    feature_lab::Game blocked(scene, normal.map.data(), normal.animations);
    InputFrame press{};
    press[static_cast<std::size_t>(Key::interact)].pressed = true;
    blocked.update(press);
    CHECK(!blocked.alarm_disabled);
    normal.update(press);
    CHECK(normal.alarm_disabled);
    const auto snapshot = feature_lab::encode_checkpoint(normal.checkpoint());
    auto restored = feature_lab::load_game(assets);
    restored.restore(feature_lab::decode_checkpoint(snapshot));
    CHECK(restored.alarm_disabled && !restored.alarm_touching);
    CHECK(feature_lab::encode_checkpoint(restored.checkpoint()) == snapshot);
    auto old = snapshot;
    old[4] = 1;
    old.resize(old.size() - 4);
    old = seal_record(std::move(old));
    rejects([&] { feature_lab::decode_checkpoint(old); });
}

} // namespace
int main() {
    return testing::run_tests();
}
