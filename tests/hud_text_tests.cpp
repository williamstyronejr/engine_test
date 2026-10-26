#include "../examples/feature_lab/animation_assets.hpp"
#include "../examples/feature_lab/diagnostics.hpp"
#include "../examples/feature_lab/font_assets.hpp"
#include "../examples/feature_lab/hud_text.hpp"
#include "../examples/feature_lab/scene_assets.hpp"
#include "../examples/feature_lab/shader_tools.hpp"
#include "../examples/feature_lab/tilemap_assets.hpp"
#include "../profiling/allocation_profile.hpp"
#include "test.hpp"
#include <filesystem>

namespace {
using namespace testing;
using namespace feature_lab;
TEST(integer_extremes_and_shorter_replacements_preserve_exact_text) {
    std::string text;
    text.reserve(256);
    const auto before = allocation_profile::snapshot();
    CHECK(format_text(text, "R", UINT64_MAX, " / ", INT64_MIN, " / ", 0) ==
          "R18446744073709551615 / -9223372036854775808 / 0");
    CHECK(format_text(text, "PAUSED") == "PAUSED");
    CHECK(format_text(text, "", 42U, " / ", -17) == "42 / -17");
    CHECK(format_text(text).empty());
    CHECK(allocation_profile::snapshot().calls == before.calls);
}
TEST(fixed_decimals_preserve_rounding_zero_and_overflow_without_allocations) {
    std::string text;
    text.reserve(256);
    const auto before = allocation_profile::snapshot();
    for (int i = 0; i < 1000; ++i) {
        CHECK(format_text(text, Fixed2{-0.0}, " / ", Fixed2{12.125}, " / ", Fixed2{9999.5}) ==
              "-0.00 / 12.12 / 9999.50");
        CHECK(format_text(text, "CPU ", Fixed2{std::numeric_limits<double>::max()}, " MS") ==
              "CPU OVERFLOW MS");
        CHECK(format_text(text, Fixed2{std::numeric_limits<double>::denorm_min()}) == "0.00");
        CHECK(format_text(text, Fixed2{-12.5}) == "-12.50");
        CHECK(format_text(text, Fixed2{std::numeric_limits<double>::infinity()}) == "inf");
        CHECK(format_text(text, Fixed2{std::numeric_limits<double>::quiet_NaN()}) == "nan");
        CHECK(format_text(text, "GPU MEAN ", Fixed2{0}, "  P95 ", Fixed2{0}, " MS") ==
              "GPU MEAN 0.00  P95 0.00 MS");
    }
    CHECK(allocation_profile::snapshot().calls == before.calls);
}
TEST(diagnostic_gpu_ingestion_preserves_delayed_samples_without_allocations) {
    Diagnostics debug;
    engine::GpuTimingStats stats;
    stats.supported = true;
    stats.counter_bits = 64;
    stats.pending = 8;
    const auto before = allocation_profile::snapshot();
    debug.record_gpu({}, stats, 1);
    CHECK(debug.gpu_summary().samples == 0);
    const std::array samples{engine::GpuTimingSample{1, 12.125},
                             engine::GpuTimingSample{2, 9999.5}};
    stats.pending = 0;
    stats.completed = 2;
    debug.record_gpu(samples, stats, 8);
    const auto result = debug.gpu_summary();
    CHECK(result.samples == 2 && result.last_frame == 2 && result.last_ms == 9999.5);
    CHECK(result.mean_ms == 5005.8125 && result.p95_ms == 12.125);
    debug.record_gpu({}, stats, 20);
    CHECK(debug.gpu_summary().samples == 2 && debug.gpu_summary().last_frame == 2);
    CHECK(allocation_profile::snapshot().calls == before.calls);
}
TEST(long_labels_grow_losslessly_and_then_reuse_capacity) {
    const std::string long_label(4096, 'X');
    std::string text;
    text.reserve(32);
    CHECK(format_text(text, long_label, " / ", UINT64_MAX) ==
          long_label + " / 18446744073709551615");
    const auto capacity = text.capacity();
    const auto before = allocation_profile::snapshot();
    for (int i = 0; i < 1000; ++i) {
        CHECK(format_text(text, "SHORT") == "SHORT");
        CHECK(format_text(text, long_label, " / ", UINT64_MAX).size() == long_label.size() + 23);
    }
    CHECK(text.capacity() == capacity && allocation_profile::snapshot().calls == before.calls);
}
TEST(reload_labels_follow_success_failure_lock_and_replacement) {
    const engine::AssetRoot assets(TEST_ASSET_ROOT);
    auto game = load_game(assets);
    const auto path = std::filesystem::temp_directory_path() / "feature-lab-hud-empty-XXXXXX";
    auto spelling = path.string();
    CHECK(mkdtemp(spelling.data()) != nullptr);
    struct Cleanup {
        std::string path;
        ~Cleanup() { std::filesystem::remove_all(path); }
    } cleanup{spelling};
    const engine::AssetRoot missing(spelling);
    std::string text;
    text.reserve(256);
    const auto check = [&](auto& asset, std::string_view name) {
        const auto ready1 = std::string(name) + " READY R1";
        const auto ready2 = std::string(name) + " READY R2";
        const auto failed = std::string(name) + " FAILED - KEPT R2";
        CHECK(asset.status(text) == ready1 && asset.status() == ready1);
        CHECK(asset.reload(game, assets));
        CHECK(asset.status(text) == ready2 && asset.status() == ready2);
        CHECK(!asset.reload(game, missing));
        CHECK(asset.status(text) == failed && asset.status() == failed);
        asset.replaced();
        CHECK(asset.status(text) == ready1);
    };
    SceneAssets scene;
    TilemapAssets tilemap;
    AnimationAssets animation;
    check(scene, "SCENE");
    check(tilemap, "TILEMAP");
    check(animation, "ANIMATION");
    SceneAssets locked_scene(true);
    TilemapAssets locked_tilemap(true);
    AnimationAssets locked_animation(true);
    CHECK(!locked_scene.reload(game, assets) &&
          locked_scene.status(text) == "SCENE LOCKED / REPLAY CONTENT");
    CHECK(!locked_tilemap.reload(game, assets) &&
          locked_tilemap.status(text) == "TILEMAP LOCKED / REPLAY CONTENT");
    CHECK(!locked_animation.reload(game, assets) &&
          locked_animation.status(text) == "ANIMATION LOCKED / REPLAY CONTENT");
}
TEST(repeated_status_formatting_reuses_the_callers_storage) {
    std::string text;
    text.reserve(256);
    SceneAssets scene(true);
    TilemapAssets tilemap;
    AnimationAssets animation;
    FontAssets font;
    ShaderTools shader;
    const auto before = allocation_profile::snapshot();
    for (int i = 0; i < 3000; ++i) {
        CHECK(scene.status(text) == "SCENE LOCKED / REPLAY CONTENT");
        CHECK(tilemap.status(text) == "TILEMAP READY R1");
        CHECK(animation.status(text) == "ANIMATION READY R1");
        CHECK(font.status(text) == "FONT READY R1 / EMBEDDED");
        CHECK(shader.status(text) == "SHADER READY R1 / F5 RELOAD / F6 TEST ERROR");
        CHECK(format_text(text, "VISIBLE TILES ", 512, " / ", 1024, "    CHUNKS ", 12, " / ", 30,
                          "    COLLIDERS ",
                          42) == "VISIBLE TILES 512 / 1024    CHUNKS 12 / 30    COLLIDERS 42");
    }
    CHECK(allocation_profile::snapshot().calls == before.calls);
}
} // namespace
int main() {
    return testing::run_tests();
}
