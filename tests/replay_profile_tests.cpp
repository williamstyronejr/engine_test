#include "../examples/feature_lab/replay_profile.hpp"
#include "test.hpp"

namespace {
using namespace feature_lab;
using namespace testing;
TEST(warmup_counters_delayed_gpu_and_completion) {
    ReplayProfile profile(3, 1, UINT64_MAX);
    const std::array<double, ReplayProfile::scope_count> ms{1, 2, 3, 4, 5, 20};
    profile.record(ms, 1280, 720, {10, 0, 2}, {20, 0, 1}, 1);
    CHECK(!profile.complete());
    profile.record(ms, 640, 480, {10, 0, 2}, {20, 0, 1}, 0); // Startup resize in warmup.
    profile.record(ms, 640, 480, {10, 0, 2}, {20, 0, 1}, 1);
    CHECK(profile.complete());
    const std::array<engine::GpuTimingSample, 3> gpu{{{1, 100}, {2, 2}, {3, 4}}};
    profile.collect_gpu(gpu);
    const auto json = profile.json(UINT64_MAX, 2, "facility", {}, true, {}, {});
    CHECK(json.find("\"replay_fingerprint\":\"18446744073709551615\"") != std::string::npos);
    CHECK(json.find("\"measured_frames\":2") != std::string::npos);
    CHECK(json.find("\"window_quads\":20") != std::string::npos);
    CHECK(json.find("\"minimap_quads\":40") != std::string::npos);
    CHECK(json.find("\"simulation_steps\":1") != std::string::npos);
    CHECK(json.find("\"gpu_render\":{\"samples\":2,\"mean\":3") != std::string::npos);
    rejects([&] { profile.record(ms, 640, 480, {}, {}, 1); });
    rejects([&] { profile.collect_gpu(gpu); });
}
TEST(bounds_invalid_samples_and_viewport_change) {
    rejects([] { ReplayProfile p(0, 0, 0); });
    rejects([] { ReplayProfile p(SIZE_MAX, 0, 0); });
    rejects([] { ReplayProfile p(2, 2, 0); });
    ReplayProfile p(2, 0, 0);
    std::array<double, ReplayProfile::scope_count> ms{};
    ms[0] = -1;
    rejects([&] { p.record(ms, 64, 64, {}, {}, 0); });
    ms[0] = 0;
    p.record(ms, 64, 64, {}, {}, 0);
    p.record(ms, 128, 64, {}, {}, 0);
    CHECK(!p.complete());
    const auto json = p.json(0, 0, "annex", {}, false, {}, {});
    CHECK(json.find("\"resized\":true") != std::string::npos);
    CHECK(json.find("\"gpu_render\":{\"samples\":0") != std::string::npos);
}
TEST(profile_option_isolation_and_fingerprint) {
    char program[] = "game", replay[] = "--replay", file[] = "test.erpl", bad[] = "--watch-assets";
    char* good[]{program, replay, file};
    validate_profile_arguments(3, good);
    char* wrong[]{program, replay, file, bad};
    rejects([&] { validate_profile_arguments(4, wrong); });
    rejects([&] { validate_profile_arguments(2, good); });
    const std::array<std::uint8_t, 3> bytes{1, 2, 3}, changed{1, 2, 4};
    CHECK(replay_fingerprint(bytes) == 15035938162879559083ULL);
    CHECK(replay_fingerprint(bytes) != replay_fingerprint(changed));
}
TEST(incomplete_run_and_exact_gpu_failure_evidence) {
    ReplayProfile p(2, 0, 1);
    p.record({}, 64, 64, {}, {}, 0);
    CHECK(!p.complete());
    engine::GpuTimingStats stats{};
    stats.method = engine::GpuTimingMethod::timestamp_pair;
    stats.counter_bits = 64;
    stats.invalid = 9;
    std::array<engine::GpuTimingFailure, 8> failures{};
    for (auto& failure : failures)
        failure = {1, UINT64_MAX, UINT64_MAX - 1, 2, 0};
    const auto json = p.json(0, 0, "facility", {}, true, stats, failures);
    CHECK(json.find("\"method\":\"timestamp_pair\"") != std::string::npos);
    CHECK(json.find("\"counter_bits\":64") != std::string::npos);
    CHECK(json.find("\"complete\":false") != std::string::npos);
    CHECK(json.find("\"failures_omitted\":1") != std::string::npos);
    CHECK(json.find("\"start_ns\":\"18446744073709551615\"") != std::string::npos);
    const std::array<engine::GpuTimingSample, 1> invalid{{{1, -1}}};
    rejects([&] { p.collect_gpu(invalid); });
    const std::array<engine::GpuTimingSample, 1> valid{{{1, 1}}};
    p.collect_gpu(valid);
}
} // namespace
int main() {
    return testing::run_tests();
}
