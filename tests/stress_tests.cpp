#include "../examples/feature_lab/spike_profile.hpp"
#include "../examples/feature_lab/stress_benchmark.hpp"
#include "engine/font.hpp"
#include "test.hpp"
#include <array>
#include <cstddef>
#include <cstdlib>
#include <initializer_list>
#include <new>

// CPU-only executable: count C++ heap allocations and live blocks, including
// Scene's hash nodes. Driver/C-library allocations are deliberately outside this check.
namespace allocation_check {
std::size_t calls{}, live{};
[[gnu::noinline]] void* allocate(std::size_t size, std::size_t alignment) {
    void* result{};
    if (alignment <= alignof(std::max_align_t))
        result = std::malloc(size ? size : 1);
    else if (posix_memalign(&result, alignment, size ? size : 1))
        result = nullptr;
    if (!result)
        throw std::bad_alloc();
    ++calls;
    ++live;
    return result;
}
[[gnu::noinline]] void release(void* pointer) noexcept {
    if (pointer) {
        --live;
        std::free(pointer);
    }
}
} // namespace allocation_check
void* operator new(std::size_t size) {
    return allocation_check::allocate(size, alignof(std::max_align_t));
}
void* operator new[](std::size_t size) {
    return ::operator new(size);
}
void* operator new(std::size_t size, std::align_val_t alignment) {
    return allocation_check::allocate(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment) {
    return ::operator new(size, alignment);
}
void operator delete(void* p) noexcept {
    allocation_check::release(p);
}
void operator delete[](void* p) noexcept {
    allocation_check::release(p);
}
void operator delete(void* p, std::size_t) noexcept {
    allocation_check::release(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    allocation_check::release(p);
}
void operator delete(void* p, std::align_val_t) noexcept {
    allocation_check::release(p);
}
void operator delete[](void* p, std::align_val_t) noexcept {
    allocation_check::release(p);
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
    allocation_check::release(p);
}
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept {
    allocation_check::release(p);
}
namespace {
using namespace feature_lab;
using namespace testing;
TEST(small_collision_budgets_reuse_storage_with_full_query_capacity) {
    using namespace engine;
    CollisionWorld world(CollisionWorld::Limits{3, 6, 12});
    std::array<Body2D, CollisionWorld::max_bodies> bodies;
    for (std::size_t i = 0; i < bodies.size(); ++i)
        bodies[i] = {i < 3 ? i + 1 : i + 1000,
                     Shape2D::circle({2, 2}, 0.5F),
                     1,
                     i < 3 ? 1U : 0U,
                     true,
                     true};
    world.prime(bodies);
    const auto bytes = world.buffer_bytes();
    const auto calls = allocation_check::calls;
    for (int tick = 0; tick < 120; ++tick) {
        for (std::size_t i = 0; i < 3; ++i) {
            bodies[i].id += 3;
            bodies[i].shape.center = tick % 2 ? Vec2{2, 2} : Vec2{0, 0};
        }
        world.update(bodies);
        CHECK(world.stats().memberships == (tick % 2 ? 3U : 12U));
        CHECK(world.bodies().size() == CollisionWorld::max_bodies);
        CHECK(world.contacts().size() == 3 && world.events().size() == 6);
    }
    CHECK(allocation_check::calls == calls && world.buffer_bytes() == bytes);
}
TEST(scene_usage_and_component_peaks_allocate_nothing_after_setup) {
    using namespace engine;
    Scene scene(Scene::Limits{2});
    SpikeProfile profile;
    const auto a = scene.create(1, "a"), b = scene.create(2, "b");
    scene.add_component<int>(a, 1);
    const auto calls = allocation_check::calls;
    for (unsigned tick = 0; tick < 100; ++tick) {
        scene.add_component<int>(b, 2);
        scene.remove_component<int>(b);
        const auto usage = scene.memory_usage();
        CHECK(usage.reserved_bytes() == scene.buffer_bytes());
        CHECK(usage.components.peak_entries_sum == 2);
        profile.observe_scene(tick % 2, scene);
    }
    scene.defer_destroy(a);
    scene.defer_destroy(b);
    scene.flush();
    CHECK(scene.memory_usage().core[2].peak_entries == 2);
    CHECK(allocation_check::calls == calls);
}
TEST(stale_deletion_requests_do_not_grow_small_scene_buffers) {
    using namespace engine;
    Scene scene(Scene::Limits{2});
    const auto warm = scene.create(1, "warm");
    scene.destroy(warm);
    const auto bytes = scene.buffer_bytes();
    for (int tick = 0; tick < 100; ++tick) {
        const auto a = scene.create(1, "a"), b = scene.create(2, "b");
        scene.defer_destroy(a);
        scene.defer_destroy(b);
        scene.destroy(a); // Its queued generation is now stale.
        const auto c = scene.create(3, "reused");
        const auto calls = allocation_check::calls;
        scene.defer_destroy(c); // Retain b, retire stale a, append c without growing.
        scene.defer_destroy(c);
        CHECK(allocation_check::calls == calls && scene.buffer_bytes() == bytes);
        CHECK(scene.memory_usage().core[2].peak_entries == 2);
        scene.flush();
        CHECK(scene.size() == 0 && !scene.valid(b) && !scene.valid(c));
    }
}
TEST(default_grid_saturation_allocates_nothing_after_setup) {
    using namespace engine;
    CollisionWorld world;
    std::array<Body2D, 64> bodies;
    for (std::size_t i = 0; i < bodies.size(); ++i)
        bodies[i] = {i + 1, Shape2D::box({}, {29, 29}), 2, 1, true, false};
    const auto calls = allocation_check::calls;
    for (int tick = 0; tick < 4; ++tick) {
        world.update(bodies);
        CHECK(world.stats().memberships == CollisionWorld::max_memberships);
        world.update({});
    }
    CHECK(allocation_check::calls == calls);
}
TEST(collision_usage_and_worst_case_event_batches_allocate_nothing_after_setup) {
    using namespace engine;
    CollisionWorld world;
    SpikeProfile profile;
    std::array<Body2D, CollisionWorld::max_bodies> bodies;
    for (std::size_t i = 0; i < bodies.size(); ++i)
        bodies[i] = {i + 1, Shape2D::circle({2, 2}, 0.5F), 1, 1, true, true};
    const auto calls = allocation_check::calls;
    world.prime(bodies);
    for (auto& body : bodies)
        body.id += bodies.size();
    world.update(bodies);
    const auto usage = world.memory_usage();
    for (unsigned i = 0; i < 100; ++i)
        profile.observe_collision(i % 2, world);
    CHECK(allocation_check::calls == calls);
    CHECK(usage.groups[0].peak_entries == CollisionWorld::max_bodies);
    CHECK(usage.groups[2].peak_entries == CollisionWorld::max_pairs);
    CHECK(usage.groups[3].peak_entries == CollisionWorld::max_pairs);
    CHECK(usage.groups[4].peak_entries == CollisionWorld::max_pairs * 2);
    CHECK(usage.groups[4].reserved_bytes ==
          usage.groups[4].peak_entries * usage.groups[4].entry_bytes * usage.groups[4].buffers);
}
TEST(empty_scenes_and_scene_moves_do_not_allocate) {
    using engine::Scene;
    const auto baseline = allocation_check::live;
    {
        const auto empty_calls = allocation_check::calls;
        Scene source, destination;
        Scene moved_empty(std::move(destination));
        destination = std::move(moved_empty);
        CHECK(allocation_check::calls == empty_calls);
        CHECK(source.buffer_bytes() == 0 && destination.buffer_bytes() == 0);
        rejects([&] { source.create(0, "invalid"); });
        CHECK(source.size() == 0 && source.buffer_bytes() == 0);

        const auto entity = source.create(1, "one");
        const auto reserved = source.buffer_bytes();
        CHECK(reserved > 0);
        source.defer_destroy(entity);
        const auto move_calls = allocation_check::calls;
        Scene moved(std::move(source));
        destination = std::move(moved);
        CHECK(allocation_check::calls == move_calls);
        CHECK(destination.valid(entity) && !source.valid(entity) && !moved.valid(entity));
        CHECK(destination.buffer_bytes() == reserved);
        destination.flush(); // Pending destruction moves with the entity storage.
        CHECK(destination.size() == 0 && destination.buffer_bytes() == reserved);

        const auto fresh = source.create(1, "reused");
        CHECK(source.valid(fresh) && !source.valid(entity) && !destination.valid(fresh));
        CHECK(source.buffer_bytes() == reserved);
        for (std::size_t i = 1; i <= Scene::capacity; ++i)
            destination.create(i, "queued");
        CHECK(destination.buffer_bytes() == reserved);
        const auto queue_calls = allocation_check::calls;
        destination.each([&](const engine::SceneNode& node) {
            destination.defer_destroy(node.entity);
            destination.defer_destroy(node.entity);
        });
        CHECK(allocation_check::calls == queue_calls);
        CHECK(destination.buffer_bytes() == reserved);
        for (const auto& group : destination.memory_usage().core)
            CHECK(group.peak_entries == Scene::capacity);
    }
    CHECK(allocation_check::live == baseline);
}
TEST(font_measurement_and_layout_allocate_nothing_after_setup) {
    const auto baseline = allocation_check::live;
    {
        const engine::BitmapFont font(engine::builtin_font());
        const std::string text = "SCORE 123 / TEST\nFALLBACK abc @ []";
        const auto calls = allocation_check::calls;
        std::size_t count = 0;
        float width = 0;
        for (int i = 0; i < 1000; ++i) {
            width += font.measure(text).x;
            engine::TextCursor cursor(font, text);
            engine::PositionedGlyph glyph;
            while (cursor.next(glyph))
                ++count;
        }
        const bool stable = calls == allocation_check::calls;
        CHECK(stable && count == 27000 && width == 102000);
    }
    CHECK(allocation_check::live == baseline);
}
TEST(component_queries_and_pool_reuse_allocate_nothing_after_setup) {
    struct Counter {
        std::uint64_t value{};
    };
    struct Increment {
        std::uint64_t value{1};
    };
    const auto baseline = allocation_check::live;
    {
        engine::Scene scene;
        std::array<engine::Entity, 256> entities{};
        for (std::size_t i = 0; i < entities.size(); ++i) {
            entities[i] = scene.create(i + 1, "component stress");
            scene.add_component<Counter>(entities[i]);
            scene.add_component<Increment>(entities[i]);
        }
        const auto bytes = scene.buffer_bytes();
        const auto calls = allocation_check::calls;
        for (int tick = 0; tick < 120; ++tick) {
            scene.each_component<Counter, Increment>(
                [](engine::Entity, Counter& counter, const Increment& increment) {
                    counter.value += increment.value;
                });
            // Swap compaction followed by in-place reconstruction reuses reserved storage.
            scene.remove_component<Increment>(entities[0]);
            scene.add_component<Increment>(entities[0]);
        }
        const bool no_allocations = calls == allocation_check::calls;
        CHECK(no_allocations && scene.buffer_bytes() == bytes);
        for (auto entity : entities)
            CHECK(scene.get_component<Counter>(entity).value == 120);
    }
    CHECK(allocation_check::live == baseline);
}
TEST(deterministic_workloads_and_exact_tile_occupancy) {
    StressConfig config{37, 113, 20, 32, 4, 123, false};
    StressArena a(config), b(config);
    for (int step = 0; step < 7; ++step) {
        a.simulate();
        a.mix();
        b.simulate();
        b.mix();
    }
    CHECK(a.checksum() == b.checksum());
    CHECK(a.resources() == b.resources());
    CHECK(a.resources().entities == 20 && a.resources().voices == 4);
    CHECK(a.ticks() == 7);
    CHECK(a.scene().world_transform(a.scene().find(1)).x > 0);
    std::size_t visited{};
    a.tiles().visit_visible(0, {{-16, -16}, {16, 16}}, [&](auto) { ++visited; });
    CHECK(visited == config.tiles);
    ++config.seed;
    StressArena different(config);
    for (int step = 0; step < 7; ++step) {
        different.simulate();
        different.mix();
    }
    CHECK(a.checksum() != different.checksum());
}
TEST(dense_collision_pairs_match_all_pairs_reference) {
    StressConfig config{0, 0, 0, 64, 0, 1, true};
    StressArena dense(config);
    dense.simulate();
    CHECK(dense.collisions().contacts().size() == 64 * 63 / 2);
    for (const auto& contact : dense.collisions().contacts()) {
        const auto bodies = dense.collisions().bodies();
        CHECK(engine::intersects(bodies[contact.a - 1].shape, bodies[contact.b - 1].shape));
    }
    config.dense = false;
    StressArena sparse(config);
    sparse.simulate();
    CHECK(sparse.collisions().contacts().empty());
    CHECK(sparse.collisions().stats().candidate_pairs < dense.collisions().stats().candidate_pairs);
}
TEST(steady_state_allocations_and_repeated_unload_baseline) {
    const StressConfig config{512, 4096, 128, 64, 16, 9, true};
    const auto baseline = allocation_check::live;
    std::uint64_t checksum{};
    for (int cycle = 0; cycle < 8; ++cycle) {
        {
            StressArena arena(config);
            arena.simulate();
            arena.mix();
            const auto buffers = arena.resources();
            const auto calls = allocation_check::calls;
            for (int step = 0; step < 12; ++step) {
                arena.simulate();
                arena.mix();
            }
            const bool allocation_free = calls == allocation_check::calls;
            CHECK(allocation_free);
            CHECK(arena.resources() == buffers);
            if (cycle)
                CHECK(arena.checksum() == checksum);
            checksum = arena.checksum();
        }
        CHECK(allocation_check::live == baseline);
    }
}
TEST(capacities_empty_arena_and_rejected_overflow) {
    StressConfig config{65536, 65536, 4096, 256, 16, UINT64_MAX, true};
    {
        StressArena arena(config);
        arena.simulate();
        arena.mix();
        CHECK(arena.resources().sprites == 65536);
        CHECK(arena.resources().entities == 4096);
        CHECK(arena.collisions().contacts().size() == engine::CollisionWorld::max_pairs);
    }
    for (const auto field : {&StressConfig::sprites, &StressConfig::tiles, &StressConfig::entities,
                             &StressConfig::bodies, &StressConfig::voices}) {
        auto bad = config;
        ++(bad.*field);
        rejects([&] { StressArena arena(bad); });
    }
    StressArena empty({0, 0, 0, 0, 0, 0, false});
    empty.simulate();
    empty.mix();
    CHECK(empty.scene().size() == 0);
    CHECK(empty.collisions().contacts().empty());
    CHECK(empty.resources().voices == 0);
}
TEST(percentiles_warmup_capacity_and_nonfinite_samples) {
    Measurements samples(100);
    for (int i = 100; i >= 1; --i)
        samples.add(i);
    const auto summary = samples.summary();
    CHECK(summary.samples == 100);
    NEAR(summary.mean, 50.5);
    NEAR(summary.median, 50);
    NEAR(summary.p95, 95);
    NEAR(summary.p99, 99);
    NEAR(summary.maximum, 100);
    rejects([&] { samples.add(1); });
    Measurements invalid(2);
    rejects([&] { invalid.add(-1); });
    rejects([&] { invalid.add(std::numeric_limits<double>::infinity()); });
    CHECK(invalid.summary().samples == 0);
}
TEST(options_bounds_json_escaping_and_resource_growth_detection) {
    StressOptions options;
    options.frames = 0;
    rejects([&] { options.validate(); });
    options.frames = 10000;
    options.cycles = 7;
    rejects([&] { options.validate(); });
    options.frames = 2;
    options.cycles = 2;
    options.warmup = 0;
    options.label = "test\"\\\n";
    CHECK(json_string(options.label) == "\"test\\\"\\\\\\u000a\"");
    StressReport report(options);
    StressArena arena({0, 0, 0, 0, 0, 0, false});
    auto wrong = arena.resources();
    ++wrong.buffer_bytes;
    report.observe(arena, wrong);
    CHECK(!report.resources_stable);
    const auto json = report.json(options);
    CHECK(json.find("\"resources_stable\":false") != std::string::npos);
    CHECK(json.find("\"samples\":0") != std::string::npos);
    char program[] = "stress_bench", frames[] = "--frames", bad[] = "18446744073709551616";
    char* arguments[] = {program, frames, bad};
    rejects([&] { parse_stress_options(3, arguments); });
}
TEST(presentation_options_defaults_conflicts_and_cpu_rejection) {
    const auto parse = [](std::initializer_list<const char*> values) {
        std::vector<char*> args;
        for (const auto* value : values)
            args.push_back(const_cast<char*>(value));
        return parse_stress_options(static_cast<int>(args.size()), args.data());
    };
    CHECK(parse({"stress"}).presentation == StressPresentation::immediate);
    CHECK(!parse({"stress"}).presentation_explicit);
    CHECK(parse({"stress", "--presentation", "vsync"}).presentation == StressPresentation::vsync);
    CHECK(parse({"stress", "--presentation", "immediate", "--no-vsync"}).presentation_explicit);
    CHECK(parse({"stress", "--no-vsync", "--cpu-only"}).cpu_only);
    rejects([&] { parse({"stress", "--presentation"}); });
    rejects([&] { parse({"stress", "--presentation", "adaptive"}); });
    rejects([&] { parse({"stress", "--presentation", "vsync", "--presentation", "immediate"}); });
    rejects([&] { parse({"stress", "--no-vsync", "--presentation", "vsync"}); });
    rejects([&] { parse({"stress", "--presentation", "vsync", "--no-vsync"}); });
    rejects([&] { parse({"stress", "--cpu-only", "--presentation", "immediate"}); });
    rejects([&] { parse({"stress", "--presentation", "vsync", "--cpu-only"}); });
    StressOptions invalid;
    invalid.presentation = static_cast<StressPresentation>(99);
    rejects([&] { invalid.validate(); });
}
TEST(presentation_report_distinguishes_request_from_confirmation) {
    StressOptions options;
    options.presentation = StressPresentation::vsync;
    StressReport report(options);
    auto json = report.json(options);
    CHECK(json.find("\"vsync_requested\":true,\"vsync_disable_accepted\":false") !=
          std::string::npos);
    CHECK(json.find("\"mode\":\"vsync\",\"swap_interval_requested\":1,\"request_issued\":false") !=
          std::string::npos);
    report.presentation_request_issued = true;
    report.present_calls = 30;
    json = report.json(options);
    CHECK(json.find("\"request_issued\":true,\"present_calls\":30") != std::string::npos);
    options.presentation = StressPresentation::immediate;
    json = report.json(options);
    CHECK(json.find("\"vsync_requested\":false,\"vsync_disable_accepted\":true") !=
          std::string::npos);
    CHECK(json.find("\"mode\":\"immediate\",\"swap_interval_requested\":0") != std::string::npos);
    options.cpu_only = true;
    StressReport cpu(options);
    json = cpu.json(options);
    CHECK(json.find("\"mode\":\"none\",\"swap_interval_requested\":null,\"request_issued\":false,"
                    "\"present_calls\":0") != std::string::npos);
}
TEST(frame_samples_pair_delayed_gpu_across_warmup_without_allocating) {
    StressFrames trace(2, 2, 2, true);
    const auto calls = allocation_check::calls;
    const std::array<double, StressFrames::scope_count> cpu{1, 2, 3, 4, 5, 6, 21};
    trace.record(3, cpu, true);
    trace.collect({1, 99}, 3); // Warmup result is never paired to a measured row.
    trace.record(4, cpu, true);
    trace.collect({3, 0}, 5); // Valid zero duration, collected during next cycle's warmup.
    trace.collect({4, 8}, 6);
    trace.record(7, cpu, false); // Skipped/unsupported query.
    trace.record(8, cpu, true);  // Still uncollected at shutdown.
    CHECK(allocation_check::calls == calls);
    const auto rows = trace.samples();
    CHECK(rows.size() == 4 && rows[0].tag == 3 && rows[2].tag == 7);
    CHECK(rows[0].gpu_ms == 0 && rows[0].collected_after == 5);
    CHECK(rows[1].gpu_ms == 8 && rows[1].collected_after == 6);
    CHECK(rows[2].gpu_ms == -1 && !rows[2].submitted);
    CHECK(rows[3].gpu_ms == -1 && rows[3].submitted);
    CHECK(trace.buffer_bytes() == 4 * sizeof(StressFrames::Sample));
}
TEST(frame_samples_reject_invalid_tags_values_duplicates_and_overflow) {
    rejects([] { StressFrames trace(0, 0, 1, true); });
    rejects([] { StressFrames trace(10000, 0, 7, true); });
    StressFrames trace(2, 0, 1, true);
    std::array<double, StressFrames::scope_count> cpu{};
    rejects([&] { trace.record(0, cpu, true); });
    rejects([&] { trace.record(2, cpu, true); });
    cpu[0] = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { trace.record(1, cpu, true); });
    cpu[0] = -1;
    rejects([&] { trace.record(1, cpu, true); });
    CHECK(trace.samples().empty());
    cpu[0] = 0;
    trace.record(1, cpu, true);
    rejects([&] { trace.collect({0, 1}, 1); });
    rejects([&] { trace.collect({1, 1}, 0); });
    rejects([&] { trace.collect({1, 1}, 3); });
    rejects([&] { trace.collect({2, 1}, 2); });
    rejects([&] { trace.collect({1, -1}, 1); });
    rejects([&] { trace.collect({1, std::numeric_limits<double>::infinity()}, 1); });
    CHECK(trace.samples()[0].gpu_ms == -1);
    trace.collect({1, 3}, 1);
    rejects([&] { trace.collect({1, 4}, 1); });
    CHECK(trace.samples()[0].gpu_ms == 3);
    trace.record(2, cpu, false);
    rejects([&] { trace.collect({2, 1}, 2); });
    rejects([&] { trace.record(3, cpu, false); });
    CHECK(trace.samples().size() == 2);
}
TEST(frame_samples_disabled_and_maximum_capacity_have_bounded_storage) {
    const auto baseline = allocation_check::live;
    {
        const auto calls = allocation_check::calls;
        StressFrames disabled(10000, 600, 6, false);
        disabled.record(0, {}, false);
        disabled.collect({}, 0);
        CHECK(!disabled.enabled() && disabled.buffer_bytes() == 0 && disabled.samples().empty());
        CHECK(allocation_check::calls == calls);
        StressFrames full(10000, 600, 6, true);
        const auto recording = allocation_check::calls;
        for (std::uint64_t cycle = 0; cycle < 6; ++cycle)
            for (std::uint64_t step = 601; step <= 10600; ++step) {
                const auto tag = cycle * 10600 + step;
                full.record(tag, {}, true);
                full.collect({tag, 0.5}, tag);
            }
        CHECK(allocation_check::calls == recording && full.samples().size() == 60000);
        CHECK(full.samples().back().tag == 63600);
    }
    CHECK(allocation_check::live == baseline);
}
TEST(frame_sample_report_is_opt_in_and_preserves_partial_and_missing_results) {
    StressOptions options;
    options.frames = 3;
    options.warmup = 0;
    options.cycles = 1;
    StressReport plain(options);
    CHECK(plain.json(options).find("frame_samples") == std::string::npos);
    options.frame_samples = true;
    StressReport report(options);
    report.frame_samples.record(1, {}, true);
    report.frame_samples.collect({1, 0}, 1);
    report.frame_samples.record(2, {}, false);
    const auto partial = report.json(options);
    CHECK(partial.find("\"complete\":false") != std::string::npos);
    CHECK(partial.find("\"gpu_status\":\"completed\",\"gpu_ms\":0") != std::string::npos);
    CHECK(partial.find("\"gpu_status\":\"not_submitted\",\"gpu_ms\":null") != std::string::npos);
    report.frame_samples.record(3, {}, true);
    CHECK(report.json(options).find("\"gpu_status\":\"uncollected\",\"gpu_ms\":null") !=
          std::string::npos);
    char program[] = "stress_bench", flag[] = "--frame-samples";
    char* args[] = {program, flag};
    CHECK(parse_stress_options(2, args).frame_samples);
}
TEST(submission_samples_bounds_partitions_and_allocation_free_recording) {
    rejects([] { StressSubmission trace(0, 0, 1, true); });
    rejects([] { StressSubmission trace(10000, 0, 7, true); });
    const auto calls = allocation_check::calls;
    StressSubmission disabled(1, 0, 1, false);
    disabled.record(0, -1, -1, -1, 0, 1);
    CHECK(disabled.buffer_bytes() == 0 && disabled.samples().empty());
    CHECK(allocation_check::calls == calls);
    StressSubmission trace(1, 2, 2, true);
    rejects([&] { trace.record(1, 3, 1, 1, 1, 192); });
    rejects([&] { trace.record(3, 1, 1, 1, 1, 192); });
    rejects([&] { trace.record(3, -1, 0, 0, 0, 0); });
    rejects([&] { trace.record(3, std::numeric_limits<double>::infinity(), 0, 0, 0, 0); });
    rejects([&] { trace.record(3, 3, 1, 1, 0, 192); });
    trace.record(3, 3, 1, 1, 1, 192);
    CHECK(trace.samples()[0].prepare_other == 1);
    rejects([&] { trace.record(3, 0, 0, 0, 0, 0); });
    trace.record(6, 0, 0, 0, 0, 0);
    rejects([&] { trace.record(9, 0, 0, 0, 0, 0); });
    StressSubmission full(10000, 600, 6, true);
    const auto recording = allocation_check::calls;
    for (std::uint64_t cycle = 0; cycle < 6; ++cycle)
        for (std::uint64_t step = 601; step <= 10600; ++step)
            full.record(cycle * 10600 + step, 3, 1, 1, 1, 192);
    CHECK(full.samples().size() == 60000 && allocation_check::calls == recording);
    CHECK(full.buffer_bytes() == 60000 * sizeof(StressSubmission::Sample));
}
TEST(submission_samples_require_graphics_and_paired_frames) {
    char program[] = "stress", flag[] = "--submission-samples", frames[] = "--frame-samples",
         cpu[] = "--cpu-only";
    char* args[] = {program, flag, frames, cpu};
    rejects([&] { parse_stress_options(2, args); });
    auto options = parse_stress_options(3, args);
    CHECK(options.submission_samples && options.frame_samples);
    rejects([&] { parse_stress_options(4, args); });
    StressReport report(options);
    report.submission_samples.record(61, 3, 1, 1, 1, 192);
    const auto json = report.json(options);
    CHECK(json.find("stress_submission_samples_v1") != std::string::npos);
    CHECK(json.find("\"cpu_ms\":[1,1,1],\"render_ms\":3,\"batches\":1,\"upload_bytes\":192") !=
          std::string::npos);
    options.submission_samples = false;
    StressReport plain(options);
    CHECK(plain.json(options).find("submission_sample") == std::string::npos);
}
TEST(gpu_failure_report_preserves_uint64_values_and_omitted_count) {
    StressOptions options;
    StressReport report(options);
    report.gpu_method = engine::GpuTimingMethod::timestamp_pair;
    report.gpu_counter_bits = 64;
    report.gpu_invalid = 10;
    report.gpu_failure_count = report.gpu_failures.size();
    for (auto& failure : report.gpu_failures)
        failure = {9007199254740993ULL, UINT64_MAX, UINT64_MAX - 1, UINT64_MAX, 7};
    const auto json = report.json(options);
    CHECK(json.find("\"method\":\"timestamp_pair\"") != std::string::npos);
    CHECK(json.find("\"counter_bits\":64") != std::string::npos);
    CHECK(json.find("\"failure_capacity\":8,\"failures_omitted\":2") != std::string::npos);
    const std::string entry =
        "{\"frame\":\"9007199254740993\",\"start_ns\":\"18446744073709551615\","
        "\"end_ns\":\"18446744073709551614\",\"last_attempted_frame\":\"18446744073709551615\","
        "\"slot\":7}";
    std::size_t found{}, count{};
    while ((found = json.find(entry, found)) != std::string::npos) {
        ++count;
        found += entry.size();
    }
    CHECK(count == 8);
}
} // namespace
int main() {
    return run_tests();
}
