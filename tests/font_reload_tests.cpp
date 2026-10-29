#include "../examples/feature_lab/asset_reload.hpp"
#include "engine/staging_memory.hpp"
#include "engine/window.hpp"
#include "font_fixture.hpp"
#include "test.hpp"
#include <cstdlib>
#include <filesystem>
#include <unistd.h>

namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
using namespace std::chrono_literals;
struct Files {
    std::filesystem::path path;
    Files() {
        std::string pattern =
            (std::filesystem::temp_directory_path() / "font-reload-XXXXXX").string();
        CHECK(mkdtemp(pattern.data()));
        path = pattern;
        std::filesystem::create_directories(path / "shaders");
        std::filesystem::create_directories(path / "textures");
        AssetRoot root(path);
        root.write_atomic("textures/test.etex", encode_texture({1, 1, {255, 0, 0, 255}}));
        root.write_text("shaders/sprite.vert", Renderer::default_vertex_shader());
        root.write_text("shaders/sprite.frag", Renderer::default_fragment_shader());
    }
    ~Files() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    void write(const FontData& font) const {
        std::filesystem::create_directories(path / "fonts");
        AssetRoot(path).write_atomic(FontAssets::key, encode_font(font));
    }
};
FontData replacement_font() {
    auto font = metric_font();
    // Change both atlas allocation and layout, keeping existing glyph rectangles valid.
    font.atlas.height = 8;
    font.atlas.rgba.resize(128);
    font.atlas.rgba[16] = 0;
    font.atlas.rgba[17] = 255; // Top-left A changes red -> green.
    font.glyphs[2].advance = 7;
    font.glyphs[2].offset_x = 0;
    font.line_height = 9;
    return font;
}
struct Fixture {
    Files files;
    AssetRoot root{files.path};
    Window window{64, 64, "Font reload", false};
    Renderer renderer;
    FontAssets fonts;
    TextureAssets textures{root};
    ShaderTools shaders;
    RenderTargetHandle target = renderer.create_target(64, 64);
    Fixture() { textures.load(renderer, "textures/test.etex", 1, 1); }
    void draw() {
        renderer.begin(target, {{32, -32}, 64}, {0, 0, 0, 1});
        renderer.text({8, -8}, 4, "AA\nZ", {});
        renderer.end();
    }
    void check_metric() {
        draw();
        NEAR(renderer.text_size("AA\nZ").x, 8);
        NEAR(renderer.text_size("AA\nZ").y, 10);
        CHECK(renderer.font_bytes() == 64);
        CHECK(renderer.pixel(5, 50)[0] == 255);
        CHECK(renderer.pixel(21, 50)[0] == 255);
        CHECK(renderer.pixel(9, 34)[0] == 255);
    }
    void check_replacement() {
        draw();
        NEAR(renderer.text_size("AA\nZ").x, 14);
        NEAR(renderer.text_size("AA\nZ").y, 18);
        CHECK(renderer.font_bytes() == 128);
        CHECK(renderer.pixel(5, 50)[0] == 0);
        CHECK(renderer.pixel(9, 50)[1] == 255);
        CHECK(renderer.pixel(37, 50)[1] == 255);
        CHECK(renderer.pixel(9, 18)[0] == 255);
    }
};
TEST(font_reload_startup_fallback_strict_failure_retention_and_pass_boundary) {
    Fixture f;
    CHECK(!f.fonts.reload(f.renderer, f.root));
    CHECK(f.fonts.embedded() && f.fonts.revision() == 1);
    CHECK(f.fonts.diagnostic().find("fonts/ui.efnt") != std::string::npos);
    NEAR(f.renderer.text_size("AA").x, 12);
    f.files.write(metric_font());
    {
        StagingScope staging;
        CHECK(f.fonts.reload(f.renderer, f.root));
        const auto memory = f.renderer.memory();
        // Loader candidate and renderer copy coexist; committed old storage is excluded.
        CHECK(staging.snapshot().cpu_peak >=
              2 * (memory.cpu_font_atlas_used_bytes + memory.cpu_font_glyph_used_bytes));
        CHECK(staging.snapshot().gpu_peak == 64 && staging.snapshot().clean());
    }
    CHECK(!f.fonts.embedded() && f.fonts.revision() == 2 && f.fonts.diagnostic().empty());
    f.check_metric();
    const auto committed = f.renderer.memory();
    f.root.write_text(FontAssets::key, "damaged EFNT");
    {
        StagingScope staging;
        CHECK(!f.fonts.reload(f.renderer, f.root));
        CHECK(f.renderer.memory() == committed);
        CHECK(staging.snapshot().clean());
    }
    CHECK(!f.fonts.embedded() && f.fonts.revision() == 2);
    CHECK(f.fonts.status().find("FAILED - KEPT R2") != std::string::npos);
    f.check_metric();
    f.files.write(replacement_font());
    // A valid candidate during a pass is a programming error, not a disk failure.
    f.renderer.begin(f.target, {{32, -32}, 64});
    rejects([&] { f.fonts.reload(f.renderer, f.root); });
    f.renderer.end();
    CHECK(f.renderer.memory() == committed);
    CHECK(f.fonts.revision() == 2 && !f.fonts.applied());
    f.check_metric();
    CHECK(f.fonts.reload(f.renderer, f.root));
    CHECK(f.fonts.revision() == 3 && f.fonts.diagnostic().empty());
    f.check_replacement();
    CHECK(f.renderer.healthy());
}
TEST(watched_font_edits_publish_metrics_and_pixels_together_and_keep_good_failures) {
    Fixture f;
    f.files.write(metric_font());
    CHECK(f.fonts.reload(f.renderer, f.root));
    AssetReload watch(f.root, f.textures);
    auto time = AssetWatcher::Time{};
    const auto tick = [&] {
        watch.update(f.renderer, f.root, f.shaders, f.textures, f.fonts, 0, time);
    };
    const auto settle = [&] {
        time += 200ms;
        tick();
        time += 150ms;
        tick();
    };
    tick();
    time += 150ms;
    tick();
    CHECK(watch.font_reloads() == 1 && f.fonts.revision() == 3);
    const auto shader_revision = f.renderer.shader_revision(),
               texture_revision = f.textures.revision();
    f.files.write(replacement_font()); // Atomic editor-style replacement.
    time += 1ms;
    tick();
    f.check_metric(); // Debounce must not expose candidate metrics before its atlas.
    time += 149ms;
    tick();
    CHECK(watch.font_reloads() == 1);
    time += 1ms;
    tick();
    f.check_replacement();
    const auto revision = f.fonts.revision();
    auto bad_metrics = encode_font(metric_font());
    // Glyph 2 advance: header 20 + two 32-byte glyphs + field offset 28.
    std::fill_n(bad_metrics.begin() + 112, 4, 0);
    f.root.write_atomic(FontAssets::key, bad_metrics);
    settle();
    CHECK(!f.fonts.applied() && f.fonts.revision() == revision);
    f.check_replacement();
    const auto attempts = watch.font_reloads();
    time += 2s;
    tick();
    CHECK(watch.font_reloads() == attempts); // No retry loop for unchanged failure.
    std::filesystem::remove(f.files.path / FontAssets::key);
    settle();
    CHECK(!f.fonts.applied() && f.fonts.revision() == revision);
    f.check_replacement();
    for (int i = 0; i < 8; ++i) {
        f.files.write(i % 2 ? replacement_font() : metric_font());
        settle();
        CHECK(f.fonts.applied() && f.fonts.diagnostic().empty());
        if (i % 2)
            f.check_replacement();
        else
            f.check_metric();
        CHECK(f.renderer.live_textures() == 1 && f.renderer.live_targets() == 1);
        CHECK(f.renderer.target_bytes() == 64 * 64 * 4 && f.renderer.healthy());
    }
    CHECK(f.renderer.shader_revision() == shader_revision &&
          f.textures.revision() == texture_revision);
    CHECK(watch.shader_reloads() == 1 && watch.texture_reloads() == 1);
}
TEST(watched_font_missing_directory_recovers_and_rejects_symlink_escape) {
    Fixture f;
    CHECK(!f.fonts.reload(f.renderer, f.root));
    AssetReload watch(f.root, f.textures);
    auto time = AssetWatcher::Time{};
    const auto tick = [&] {
        watch.update(f.renderer, f.root, f.shaders, f.textures, f.fonts, 0, time);
    };
    tick();
    time += 150ms;
    tick();
    CHECK(f.fonts.embedded());
    CHECK(f.shaders.result().applied && f.textures.applied());
    f.files.write(metric_font());
    time += 1s;
    tick();
    time += 150ms;
    tick();
    CHECK(f.fonts.applied() && !f.fonts.embedded());
    f.check_metric();
    const auto revision = f.fonts.revision();
    Files outside;
    outside.write(replacement_font());
    std::filesystem::remove(f.files.path / FontAssets::key);
    std::filesystem::create_symlink(outside.path / FontAssets::key, f.files.path / FontAssets::key);
    time += 1s;
    tick();
    time += 150ms;
    tick();
    CHECK(!f.fonts.applied() && f.fonts.revision() == revision);
    CHECK(f.fonts.diagnostic().find("escapes") != std::string::npos);
    f.check_metric();
    std::filesystem::remove_all(f.files.path / "fonts");
    time += 1s;
    tick();
    time += 150ms;
    tick();
    f.files.write(replacement_font());
    time += 1s;
    tick();
    time += 150ms;
    tick();
    CHECK(f.fonts.applied());
    f.check_replacement();
}
TEST(explicit_font_reload_suppresses_only_its_ready_group) {
    Fixture f;
    f.files.write(metric_font());
    AssetReload watch(f.root, f.textures);
    watch.update(f.renderer, f.root, f.shaders, f.textures, f.fonts, 0, AssetWatcher::Time{});
    CHECK(f.fonts.reload(f.renderer, f.root));
    const auto revision = f.fonts.revision();
    watch.update(f.renderer, f.root, f.shaders, f.textures, f.fonts, AssetReload::font_group,
                 AssetWatcher::Time{} + 150ms);
    CHECK(f.fonts.revision() == revision && watch.font_reloads() == 0);
    CHECK(watch.shader_reloads() == 1 && watch.texture_reloads() == 1);
    f.check_metric();
    CHECK(f.renderer.healthy());
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY"))
        return 77;
    return testing::run_tests();
}
