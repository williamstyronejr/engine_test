#include "../examples/feature_lab/replay_file.hpp"
#include "../examples/feature_lab/scene_flow.hpp"
#include "../examples/feature_lab/spike_profile.hpp"
#include "test.hpp"
#include <fstream>

namespace {
using namespace feature_lab;
using namespace testing;
SpikeEvent make_event(std::uint64_t frame, SpikeAction action) {
    SpikeEvent e{frame, action, 1.25, true, true, {UINT64_MAX, 2, 1, 2, 0, {}}, {}, {}};
    e.after = e.before;
    switch (action) {
    case SpikeAction::shader_reload:
    case SpikeAction::shader_recover:
        ++e.after.shader;
        break;
    case SpikeAction::texture_reload:
    case SpikeAction::texture_recover:
        ++e.after.textures;
        break;
    case SpikeAction::font_reload:
        ++e.after.font;
        break;
    case SpikeAction::transition:
        e.after.level = 1;
        --e.after.hash;
        break;
    default:
        e.applied = false;
        e.diagnostic = "deliberate \"failure\"\n";
        break;
    }
    return e;
}
TEST(schedule_complete_evidence_and_delayed_gpu_correlation) {
    SpikeProfile p;
    for (std::size_t frame = 1; frame <= SpikeProfile::frame_limit; ++frame) {
        if (const auto action = p.next(frame, frame > 1000))
            p.event(make_event(frame, *action));
        p.record({1, 2, 3, 4, 5, 20}, {});
    }
    CHECK(p.complete() && !p.failed());
    const std::array<engine::GpuTimingSample, 2> gpu{{{120, 2.5}, {240, 0}}};
    p.collect_gpu(gpu);
    const auto json = p.json();
    CHECK(json.find("\"gpu_render_ms\":2.5") != std::string::npos);
    CHECK(json.find("\"gpu_render_ms\":0") != std::string::npos);
    CHECK(json.find("\"gpu_render_ms\":null") != std::string::npos);
    CHECK(json.find("18446744073709551615") != std::string::npos);
    CHECK(json.find("deliberate \\\"failure\\\"\\u000a") != std::string::npos);
    CHECK(json.find("\"frame\":118,\"cpu_ms\":[1,2,3,4,5,20]") != std::string::npos);
    rejects([&] { p.collect_gpu(gpu); });
    rejects([&] { p.record({}, {}); });
}
TEST(failure_preserves_event_and_partial_neighborhood) {
    SpikeProfile p;
    for (int frame = 1; frame < 120; ++frame)
        p.record({}, {});
    auto e = make_event(120, SpikeAction::shader_reload);
    e.applied = false;
    e.after = e.before;
    e.diagnostic = "unexpected failure";
    p.event(e);
    CHECK(p.failed() && !p.complete());
    p.record({}, {});
    const auto json = p.json();
    CHECK(json.find("\"verified\":false") != std::string::npos);
    CHECK(json.find("unexpected failure") != std::string::npos);
    CHECK(json.find("\"frame\":121") == std::string::npos);
}
TEST(rejection_must_preserve_state_and_resources) {
    for (int fault = 0; fault < 4; ++fault) {
        SpikeProfile p;
        for (int frame = 1; frame < 240; ++frame) {
            if (frame == 120)
                p.event(make_event(120, SpikeAction::shader_reload));
            p.record({}, {});
        }
        auto e = make_event(240, SpikeAction::shader_reject);
        if (fault == 0)
            --e.after.hash;
        if (fault == 1)
            ++e.after.resources.textures;
        if (fault == 2)
            ++e.after.shader;
        if (fault == 3)
            e.diagnostic.clear();
        p.event(e);
        CHECK(p.failed());
    }
}
TEST(bounds_and_warmup_resource_baseline) {
    SpikeProfile p;
    rejects([&] { p.event(make_event(120, SpikeAction::shader_reload)); });
    auto ms = std::array<double, ReplayProfile::scope_count>{};
    ms[0] = NAN;
    rejects([&] { p.record(ms, {}); });
    const std::array<engine::GpuTimingSample, 1> early{{{1, 1}}};
    rejects([&] { p.collect_gpu(early); });
    for (std::size_t i = 0; i < SpikeProfile::warmup; ++i)
        p.record({}, {});
    p.record({}, {1, 10, 1, 20, 30});
    CHECK(!p.failed());
    p.record({}, {1, 11, 1, 20, 30});
    CHECK(p.failed());
    char program[] = "game", option[] = "--spike-report", file[] = "out.json", bad[] = "--replay";
    char* good[]{program, option, file};
    validate_spike_arguments(3, good);
    char* wrong[]{program, option, file, bad};
    rejects([&] { validate_spike_arguments(4, wrong); });
    rejects([&] { validate_spike_arguments(2, good); });
}
// Independent CPU route/transition reference; native tests compare event hashes and frames.
void reference(const char* path) {
    const engine::AssetRoot assets(TEST_ASSET_ROOT);
    auto game = load_game(assets);
    SceneFlow flow(game, false, &assets);
    Replay route;
    SpikeProfile spikes;
    ReplayProfile profile(SpikeProfile::frame_limit, SpikeProfile::warmup, 0);
    std::uint64_t shader = 2, textures = 1, font = 2;
    for (std::size_t frame = 1; frame <= SpikeProfile::frame_limit; ++frame) {
        bool transition = false;
        if (const auto action = spikes.next(frame, game.won)) {
            auto e = make_event(frame, *action);
            e.before = {
                replay_hash(game), shader, textures, font, static_cast<unsigned>(game.level), {}};
            switch (*action) {
            case SpikeAction::shader_reload:
            case SpikeAction::shader_recover:
                ++shader;
                break;
            case SpikeAction::texture_reload:
            case SpikeAction::texture_recover:
                ++textures;
                break;
            case SpikeAction::font_reload:
                ++font;
                break;
            case SpikeAction::transition:
                flow.advance_level(game, {});
                route = {};
                transition = true;
                break;
            case SpikeAction::transition_reject:
                transition = true;
                break;
            default:
                break;
            }
            e.after = {
                replay_hash(game), shader, textures, font, static_cast<unsigned>(game.level), {}};
            spikes.event(std::move(e));
        }
        const auto ticks = game.ticks;
        if (!transition)
            game.update(route.next(game));
        profile.record({}, 64, 64, {}, {}, static_cast<unsigned>(game.ticks > ticks));
        spikes.record({}, {});
    }
    CHECK(spikes.complete() && game.won && game.level == Level::facility);
    profile.write(path, profile.json(replay_hash(game), game.ticks, level_name(game.level), {},
                                     false, {}, {}, &spikes));
}
} // namespace
int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--reference") {
        try {
            reference(argv[2]);
            return 0;
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            return 1;
        }
    }
    return testing::run_tests();
}
