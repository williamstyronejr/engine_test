#include "../examples/feature_lab/replay_file.hpp"
#include "../examples/feature_lab/scene_flow.hpp"
#include "../examples/feature_lab/spike_profile.hpp"
#include "engine/font.hpp"
#include "scene_fixture.hpp"
#include "test.hpp"
#include <fstream>

namespace {
using namespace feature_lab;
using namespace testing;
TEST(asset_staging_overlaps_encoded_and_decoded_payload_once) {
    using namespace engine;
    const AssetRoot assets(TEST_ASSET_ROOT);
    TextureCache cache(assets);
    {
        StagingScope scope;
        const auto texture = cache.load("textures/actors.etex");
        const auto bytes = texture->rgba.capacity();
        CHECK(scope.snapshot().cpu_peak == bytes * 2 + 16);
        CHECK(scope.snapshot().clean()); // Returned snapshot is no longer cache staging.
        const auto shared = cache.load("textures/actors.etex");
        CHECK(shared == texture && scope.snapshot().cpu_peak == bytes * 2 + 16);
    }
    {
        const auto encoded = assets.read("fonts/ui.efnt", BitmapFont::max_file_bytes);
        StagingScope scope;
        const auto font = load_font(assets, "fonts/ui.efnt");
        const auto decoded =
            font.data().atlas.rgba.capacity() + font.data().glyphs.capacity() * sizeof(FontGlyph);
        CHECK(scope.snapshot().cpu_peak == encoded.capacity() + decoded);
        CHECK(scope.snapshot().clean());
    }
}
TEST(staging_overlap_transfer_unwind_and_overflow) {
    using namespace engine;
    {
        StagingBytes inactive(123);
    }
    StagingScope scope;
    rejects([] { StagingScope nested; });
    {
        StagingBytes cpu(100);
        {
            StagingBytes gpu(40, StagingKind::gpu);
            cpu.set(80);
            StagingBytes transferred(std::move(gpu));
            CHECK(scope.snapshot().gpu_remaining == 40);
        }
        CHECK(scope.snapshot().gpu_remaining == 0);
        rejects([] { StagingBytes huge(SIZE_MAX); });
        CHECK(scope.snapshot().cpu_remaining == 80);
        try {
            StagingBytes failed(50);
            throw std::runtime_error("unwind");
        } catch (const std::runtime_error&) {
        }
        CHECK(scope.snapshot().cpu_remaining == 80);
    }
    const auto result = scope.snapshot();
    CHECK(result.measured && result.clean());
    CHECK(result.cpu_peak == 130 && result.gpu_peak == 40 && result.combined_peak == 140);
}
TEST(transition_candidate_overlap_and_rejection_cleanup) {
    using namespace engine;
    const AssetRoot assets(TEST_ASSET_ROOT);
    auto game = load_game(assets);
    SceneFlow flow(game, false, &assets);
    game.won = true;
    const auto original_hash = replay_hash(game);
    std::size_t candidate_bytes = 0;
    flow.prepare_replacement = [&](const Game& candidate) {
        candidate_bytes = candidate.staging_buffer_bytes();
        StagingBytes overlay(256);
        throw std::runtime_error("reject candidate");
    };
    {
        StagingScope scope;
        rejects([&] { flow.advance_level(game, {}); });
        CHECK(scope.snapshot().clean());
        // Construction no longer exceeds the completed candidate/overlay overlap.
        CHECK(candidate_bytes > 0 && scope.snapshot().cpu_peak == candidate_bytes + 256);
        CHECK(scope.snapshot().gpu_peak == 0 && replay_hash(game) == original_hash);
    }
    flow.prepare_replacement = {};
    {
        StagingScope scope;
        flow.advance_level(game, {});
        CHECK(scope.snapshot().clean() && scope.snapshot().cpu_peak == candidate_bytes);
        CHECK(game.level == Level::annex && !game.won);
    }
}
TEST(parser_buffers_survive_until_validation_and_unwind_on_trailing_data) {
    using namespace engine;
    const AssetRoot assets(TEST_ASSET_ROOT);
    auto tile_bytes = assets.read("annex.etmp", 4 * 1024 * 1024);
    const auto tiles = decode_tilemap(tile_bytes);
    auto animation_bytes = assets.read("facility.eani", 1024 * 1024);
    const auto animations = decode_animations(animation_bytes);
    for (const bool malformed : {false, true}) {
        if (malformed) {
            tile_bytes.push_back(0);
            animation_bytes.push_back(0);
        }
        {
            StagingScope scope;
            if (malformed)
                rejects([&] { decode_tilemap(tile_bytes); });
            else
                decode_tilemap(tile_bytes);
            CHECK(scope.snapshot().cpu_peak == tiles.buffer_bytes());
            CHECK(scope.snapshot().clean());
        }
        {
            StagingScope scope;
            if (malformed)
                rejects([&] { decode_animations(animation_bytes); });
            else
                decode_animations(animation_bytes);
            CHECK(scope.snapshot().cpu_peak == animations.buffer_bytes());
            CHECK(scope.snapshot().clean());
        }
    }
    {
        StagingScope scope;
        const auto text = assets.text("annex.scene");
        CHECK(scope.snapshot().cpu_peak == text.size() * 2);
        CHECK(scope.snapshot().clean());
    }
}
TEST(scene_parser_tracks_lazy_reservation_before_later_validation) {
    using namespace engine;
    Scene populated;
    populated.create(1, "one");
    const auto reserved = populated.buffer_bytes();
    const std::string text = "scene 1\nentity 1 \"one\" \"\" 0 0 0 0 1 1\nend\n";
    std::size_t complete_peak = 0;
    {
        StagingScope scope;
        auto scene = parse_scene(text);
        CHECK(scene.buffer_bytes() == reserved);
        complete_peak = scope.snapshot().cpu_peak;
        CHECK(complete_peak > reserved && scope.snapshot().clean());
    }
    {
        StagingScope scope;
        rejects([&] { parse_scene(text + "unexpected\n"); });
        CHECK(scope.snapshot().cpu_peak == complete_peak && scope.snapshot().clean());
    }
    {
        StagingScope scope;
        // The entity exists before transform validation throws.
        rejects([] { parse_scene("scene 1\nentity 1 \"one\" \"\" 0 invalid 0 0 1 1\nend\n"); });
        CHECK(scope.snapshot().cpu_peak > reserved && scope.snapshot().clean());
    }
    {
        StagingScope scope;
        auto empty = parse_scene("scene 1\nend\n");
        CHECK(empty.buffer_bytes() == 0);
        CHECK(scope.snapshot().cpu_peak < reserved && scope.snapshot().clean());
    }
    {
        StagingScope scope;
        rejects([] { parse_scene("scene 1\nentity 1 \"unterminated"); });
        CHECK(scope.snapshot().cpu_peak < reserved && scope.snapshot().clean());
    }
}
TEST(malformed_transition_inputs_retain_live_game_and_release_staging) {
    using namespace engine;
    for (int fault = 0; fault < 7; ++fault) {
        SceneFiles files;
        auto game = load_game(files.root);
        SceneFlow flow(game, false, &files.root);
        game.won = true;
        const auto hash = replay_hash(game);
        const auto player = game.player;
        bool prepared = false;
        flow.prepare_replacement = [&](const Game&) { prepared = true; };
        if (fault < 2) {
            auto bytes = files.root.read("annex.etmp", 4 * 1024 * 1024);
            if (fault == 0)
                bytes.pop_back();
            else
                bytes.push_back(0);
            files.root.write_atomic("annex.etmp", bytes);
        } else if (fault == 2) {
            auto bytes = files.root.read("facility.eani", 1024 * 1024);
            bytes.push_back(0);
            files.root.write_atomic("facility.eani", bytes);
        } else {
            auto scene = files.text(Level::annex);
            if (fault == 3)
                scene += "unexpected\n";
            if (fault == 4)
                scene = "scene 1\nentity 1 \"unterminated";
            if (fault == 5) {
                const auto pos = scene.find("\"player\"");
                CHECK(pos != std::string::npos);
                scene.replace(pos, 8, "\"unused\"");
            }
            if (fault == 6) {
                const auto pos = scene.find("\"player\" 1");
                CHECK(pos != std::string::npos);
                scene.replace(pos, 10, "\"player\" 999");
            }
            files.write(scene, Level::annex);
        }
        {
            StagingScope scope;
            rejects([&] { flow.advance_level(game, {}); });
            const auto memory = scope.snapshot();
            CHECK(memory.clean() && memory.cpu_peak > 0 && memory.gpu_peak == 0);
            // Scene/semantic failures overlap the already constructed members.
            if (fault >= 3)
                CHECK(memory.cpu_peak > game.collisions.buffer_bytes());
            CHECK(!prepared && replay_hash(game) == hash && game.world.valid(player));
        }
        std::filesystem::copy(TEST_ASSET_ROOT, files.path,
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing);
        {
            StagingScope scope;
            flow.advance_level(game, {});
            CHECK(prepared && game.level == Level::annex && scope.snapshot().clean());
        }
    }
}
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
    for (int fault = 0; fault < 5; ++fault) {
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
        if (fault == 4)
            e.staging.cpu_remaining = 1;
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
