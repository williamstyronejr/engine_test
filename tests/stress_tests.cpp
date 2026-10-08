#include "../examples/feature_lab/stress_benchmark.hpp"
#include "test.hpp"
#include <cstddef>
#include <cstdlib>
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
} // namespace
int main() {
    return run_tests();
}
