#include "../examples/feature_lab/collision_visuals.hpp"
#include "../examples/feature_lab/diagnostics.hpp"
#include "../examples/feature_lab/overlays.hpp"
#include "../examples/feature_lab/scene_flow_draw.hpp"
#include "../examples/feature_lab/settings_draw.hpp"
#include "engine/renderer.hpp"
#include "engine/window.hpp"
#include "test.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numbers>
#include <unistd.h>

namespace {
using namespace engine;
using namespace testing;
// PPM readback also exercises screenshots after offscreen passes.
struct Capture {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
                                 ("engine-target-test-" + std::to_string(getpid()) + ".ppm");
    ~Capture() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
    std::vector<unsigned char> read(Renderer& renderer, int width, int height) {
        renderer.screenshot(path.string());
        std::ifstream file(path, std::ios::binary);
        std::string magic;
        int w{}, h{}, range{};
        file >> magic >> w >> h >> range;
        CHECK(magic == "P6" && w == width && h == height && range == 255);
        CHECK(file.get() == '\n');
        std::vector<unsigned char> bytes(static_cast<std::size_t>(w * h * 3));
        file.read(reinterpret_cast<char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        CHECK(file && file.peek() == std::char_traits<char>::eof());
        return bytes;
    }
};
TEST(offscreen_orientation_and_composition) {
    Window window(96, 96, "Target pixels", false);
    Renderer renderer;
    const auto target = renderer.create_target(64, 32);
    CHECK(renderer.live_targets() == 1 && renderer.target_bytes() == 64 * 32 * 4);
    renderer.begin(target, {{0, 0}, 2}, {0, 0, 0, 1});
    renderer.quad({-1, 0.5F}, {2, 1}, {1, 0, 0, 1});
    renderer.quad({1, -0.5F}, {2, 1}, {0, 1, 0, 1});
    renderer.end();
    CHECK(renderer.pixel(8, 24)[0] > 250);
    CHECK(renderer.pixel(56, 8)[1] > 250);
    CHECK(renderer.pixel(8, 8)[0] < 3);
    Capture capture;
    const auto pixels = capture.read(renderer, 64, 32);
    CHECK(pixels[(7 * 64 + 8) * 3] > 250); // PPM flips bottom-left GL rows to top-first.
    CHECK(pixels[(23 * 64 + 56) * 3 + 1] > 250);
    renderer.begin(96, 96, {{0, 0}, 2}, {0, 0, 1, 1});
    renderer.target_sprite(target, Transform::from({}, 0, {2, 2}));
    renderer.end();
    CHECK(renderer.pixel(12, 72)[0] > 250);
    CHECK(renderer.pixel(84, 24)[1] > 250);
    CHECK(renderer.pixel(12, 24)[2] < 3);
    renderer.begin(96, 96, {{0, 0}, 2});
    renderer.target_sprite(target, Transform::from({}, 0, {2, 2}), {}, {{0, 1}, {1, 0}});
    renderer.end();
    CHECK(renderer.pixel(12, 24)[0] > 250);
    CHECK(renderer.healthy());
}
TEST(transparent_targets_preserve_linear_alpha) {
    Window window(64, 64, "Target alpha", false);
    Renderer renderer;
    const auto a = renderer.create_target(32, 32), b = renderer.create_target(32, 32);
    renderer.begin(a, {{0, 0}, 2}, {0, 1, 0, 0});
    renderer.quad({}, {2, 2}, {1, 0, 0, 0.5F});
    renderer.end();
    const auto raw = renderer.pixel(16, 16);
    CHECK(raw[0] >= 186 && raw[0] <= 190 && raw[1] < 3);
    CHECK(raw[3] >= 127 && raw[3] <= 129);
    renderer.begin(b, {{0, 0}, 2});
    renderer.target_sprite(a, Transform::from({}, 0, {2, 2}));
    renderer.end();
    CHECK(renderer.pixel(16, 16) == raw);
    renderer.begin(64, 64, {{0, 0}, 2}, {0, 0, 1, 1});
    renderer.target_sprite(b, Transform::from({}, 0, {2, 2}));
    renderer.end();
    const auto blended = renderer.pixel(32, 32);
    CHECK(blended[0] >= 186 && blended[0] <= 190);
    CHECK(blended[2] >= 186 && blended[2] <= 190 && blended[3] == 255);
    renderer.begin(64, 64, {{0, 0}, 2}, {0, 0, 1, 1});
    renderer.target_sprite(b, Transform::from({}, 0, {2, 2}), {1, 1, 1, 0.5F});
    renderer.end();
    const auto tinted = renderer.pixel(32, 32);
    CHECK(tinted[0] >= 135 && tinted[0] <= 139);
    CHECK(tinted[2] >= 223 && tinted[2] <= 227);
    // A translucent clear uses the same premultiplied storage convention.
    renderer.begin(a, {{0, 0}, 2}, {0, 1, 0, 0.5F});
    renderer.end();
    CHECK(renderer.pixel(0, 0)[1] >= 186 && renderer.pixel(0, 0)[3] >= 127);
    CHECK(renderer.healthy());
}
TEST(nested_clips_flush_and_restore) {
    Window window(64, 64, "Target clipping", false);
    Renderer renderer;
    const auto target = renderer.create_target(64, 32);
    renderer.begin(target, {{32, -16}, 32}, {0, 0, 0, 1});
    const auto fill = [&](Color color) { renderer.quad({32, -16}, {64, 32}, color); };
    fill({1, 0, 0, 1});
    renderer.push_clip({8, 4, 24, 20});
    fill({0, 1, 0, 1});
    renderer.push_clip({20, 0, 24, 16});
    fill({0, 0, 1, 1});
    renderer.pop_clip();
    renderer.quad({10, -20}, {2, 2}, {1, 1, 1, 1});
    renderer.pop_clip();
    renderer.quad({2, -30}, {2, 2}, {1, 1, 0, 1});
    renderer.end();
    const auto at = [&](int x, int y) { return renderer.pixel(x, 31 - y); };
    CHECK(at(2, 2)[0] > 250 && at(2, 2)[1] < 3);
    CHECK(at(9, 6)[1] > 250 && at(9, 6)[2] < 3);
    CHECK(at(21, 6)[2] > 250 && at(21, 6)[1] < 3);
    CHECK(at(33, 6)[0] > 250 && at(33, 6)[2] < 3);
    CHECK(at(10, 20)[0] > 250 && at(10, 20)[1] > 250);
    CHECK(at(2, 30)[0] > 250 && at(2, 30)[1] > 250);
    CHECK(renderer.stats().draws == 5);
    renderer.begin(64, 64, {{0, 0}, 2}, {0, 0, 1, 1});
    renderer.end();
    CHECK(renderer.pixel(63, 63)[2] > 250); // Full window clear/viewport restored.
    CHECK(renderer.healthy());
}
TEST(clip_limits_empty_and_error_recovery) {
    Window window(32, 32, "Clip validation", false);
    Renderer renderer;
    rejects([&] { renderer.push_clip({0, 0, 1, 1}); });
    rejects([&] { renderer.pop_clip(); });
    renderer.begin(32, 32, {{0, 0}, 2}, {0, 0, 0, 1});
    rejects([&] { renderer.push_clip({0, 0, -1, 1}); });
    for (int i = 0; i < 16; ++i)
        renderer.push_clip({0, 0, 32, 32});
    rejects([&] { renderer.push_clip({0, 0, 32, 32}); });
    rejects([&] { renderer.end(); });
    for (int i = 0; i < 16; ++i)
        renderer.pop_clip();
    rejects([&] { renderer.pop_clip(); });
    for (const auto rectangle :
         {PixelRect{0, 0, 0, 32}, PixelRect{33, 0, 3, 32},
          PixelRect{std::numeric_limits<int>::max(), 0, std::numeric_limits<int>::max(), 32},
          PixelRect{std::numeric_limits<int>::min(), 0, 10, 32}}) {
        renderer.push_clip(rectangle);
        renderer.quad({}, {2, 2}, {1, 0, 0, 1});
        renderer.pop_clip();
    }
    renderer.push_clip({-4, -4, 12, 12});
    renderer.set_camera({{50, 50}, 2}); // Clip remains in destination pixels.
    renderer.quad({50, 50}, {2, 2}, {0, 1, 0, 1});
    renderer.pop_clip();
    renderer.end();
    CHECK(renderer.pixel(0, 31)[1] > 250);
    CHECK(renderer.pixel(8, 31)[1] < 3);
    CHECK(renderer.pixel(16, 16)[0] < 3);
    CHECK(renderer.healthy());
}
TEST(target_lifetime_resize_and_failure_preservation) {
    Window window(32, 32, "Target lifetime", false);
    Renderer renderer;
    const auto target = renderer.create_target(16, 16);
    renderer.begin(32, 32, {{0, 0}, 2});
    rejects([&] { renderer.target_sprite(target, {}); }); // Undefined storage is never sampled.
    rejects([&] { renderer.create_target(4, 4); });
    rejects([&] { renderer.resize_target(target, 4, 4); });
    rejects([&] { renderer.release(target); });
    renderer.end();
    renderer.begin(target, {{0, 0}, 2}, {1, 0, 0, 1});
    rejects([&] { renderer.target_sprite(target, {}); }); // Reject framebuffer feedback.
    rejects([&] { renderer.begin(32, 32, {{0, 0}, 2}); });
    renderer.end();
    rejects([&] { renderer.resize_target(target, 0, 16); });
    rejects([&] { renderer.resize_target(target, 4097, 16); });
    const auto other = renderer.create_target(1, 1);
    rejects([&] { renderer.resize_target(target, 4096, 4096); }); // Budget, before allocation.
    CHECK(renderer.target_size(target).width == 16);
    CHECK(renderer.pixel(0, 0)[0] > 250);   // Failed resize/create retained last read destination.
    renderer.resize_target(target, 16, 16); // Same size is a no-op preserving pixels.
    CHECK(renderer.pixel(0, 0)[0] > 250);
    for (int i = 1; i <= 40; ++i) {
        renderer.resize_target(target, i, i + 1);
        CHECK(renderer.target_size(target).height == i + 1);
        CHECK(renderer.target_bytes() == static_cast<std::size_t>(i * (i + 1) * 4 + 4));
        renderer.begin(target, {{0, 0}, 2}, {0, 1, 0, 1});
        renderer.end();
        CHECK(renderer.pixel(i - 1, i)[1] > 250);
    }
    renderer.release(target);
    rejects([&] { renderer.pixel(0, 0); });
    rejects([&] { renderer.begin(target, {}); });
    rejects([&] { renderer.release(target); });
    const auto replacement = renderer.create_target(8, 8);
    CHECK(replacement.serial != target.serial);
    rejects([&] { renderer.target_size(target); });
    renderer.release(replacement);
    renderer.release(other);
    CHECK(renderer.live_targets() == 0 && renderer.target_bytes() == 0);
    CHECK(renderer.live_textures() == 0 && renderer.healthy());
}
TEST(feature_lab_minimap_resize_and_scrolling_panel) {
    Window window(640, 480, "Feature Lab overlays", false);
    Renderer renderer;
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    feature_lab::Overlays overlays(renderer, game);
    overlays.render_minimap(renderer, game, 640, 480, 1);
    CHECK(renderer.live_targets() == 1);
    CHECK(renderer.target_bytes() == 138 * 69 * 4);
    const auto player = renderer.pixel(10, 13);
    CHECK(player[1] > 200 && player[2] > 150); // Spawn marker in full-map projection.
    Capture capture;
    const auto draw = [&] {
        renderer.begin(640, 480, {{320, -240}, 480}, {0.1F, 0.1F, 0.1F, 1});
        overlays.draw(renderer, game, 640, 480);
        renderer.end();
        return capture.read(renderer, 640, 480);
    };
    const auto before = draw();
    InputFrame scroll{};
    scroll[static_cast<std::size_t>(Key::panel_down)].held = true;
    for (int i = 0; i < 80; ++i)
        overlays.input(scroll);
    const auto after = draw();
    std::size_t changes = 0;
    for (int y = 0; y < 480; ++y)
        for (int x = 0; x < 640; ++x) {
            const auto index = static_cast<std::size_t>((y * 640 + x) * 3);
            if (before[index] == after[index] && before[index + 1] == after[index + 1] &&
                before[index + 2] == after[index + 2])
                continue;
            ++changes;
            // Only the clipped text viewport and its adjacent scrollbar may change.
            CHECK(x >= 490 && x < 627 && y >= 158 && y < 210);
        }
    CHECK(changes > 50);
    scroll = {};
    scroll[static_cast<std::size_t>(Key::restart)].pressed = true;
    overlays.input(scroll);
    CHECK(draw() == before);
    overlays.render_minimap(renderer, game, 1280, 720, 1);
    CHECK(renderer.target_bytes() == 276 * 138 * 4);
    overlays.render_minimap(renderer, game, 640, 480, 1);
    CHECK(renderer.target_bytes() == 138 * 69 * 4 && renderer.live_targets() == 1);
    CHECK(renderer.healthy());
}
TEST(ui_pixels_focus_values_and_clip_restore) {
    Window window(320, 240, "UI pixels", false);
    Renderer renderer;
    Ui ui;
    const std::array widgets{UiWidget{1, UiKind::button, {{10, 10}, {210, 50}}, "PLAY"},
                             UiWidget{2, UiKind::checkbox, {{10, 60}, {210, 100}}, "MUTE"},
                             UiWidget{3, UiKind::slider, {{10, 110}, {210, 170}}, "VOLUME", 0.25F}};
    ui.layout({{20, 0}, {220, 180}}, widgets);
    const auto draw = [&] {
        renderer.begin(320, 240, {{160, -120}, 240}, {0, 0, 0, 1});
        draw_ui(renderer, ui);
        renderer.quad({280, -210}, {10, 10}, {1, 0, 0, 1}); // UI restored clip stack.
        renderer.end();
    };
    draw();
    CHECK(renderer.pixel(15, 209)[0] == 0); // Widget clipped on left.
    CHECK(renderer.pixel(280, 29)[0] > 250);
    const auto border = renderer.pixel(100, 229);
    CHECK(border[1] > 200 && border[0] < 100);
    const auto unchecked = renderer.pixel(190, 159);
    const auto empty_rail = renderer.pixel(155, 84);
    InputFrame tab{};
    tab[static_cast<std::size_t>(Key::tab)].pressed = true;
    ui.update(tab);
    InputFrame accept{};
    accept[static_cast<std::size_t>(Key::accept)].pressed = true;
    ui.update(accept);
    ui.set_value(3, 0.75F);
    draw();
    CHECK(renderer.pixel(100, 229) != border); // Focus moved to checkbox.
    CHECK(renderer.pixel(190, 159)[1] > unchecked[1] + 80);
    CHECK(renderer.pixel(155, 84)[1] > empty_rail[1] + 50);
    CHECK(renderer.healthy());
}
TEST(settings_render_at_small_and_large_sizes) {
    Window window(640, 480, "Settings resize", false);
    Renderer renderer;
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    feature_lab::Settings settings;
    InputFrame open{};
    open[static_cast<std::size_t>(Key::settings)].pressed = true;
    settings.update(game, open, 640, 480, true);
    const auto target = renderer.create_target(640, 480);
    const auto render_sizes = [&] {
        for (const auto size : {RenderTargetSize{640, 480}, RenderTargetSize{320, 240},
                                RenderTargetSize{1280, 720}}) {
            renderer.resize_target(target, size.width, size.height);
            settings.update(game, {}, size.width, size.height, true);
            renderer.begin(target, {{0, 0}, 2}, {0, 0, 0, 1});
            settings.draw(renderer, size.width, size.height);
            renderer.end();
            CHECK(renderer.pixel(size.width / 2, size.height / 2)[2] > 20);
            CHECK(renderer.stats().quads > 100 && renderer.healthy());
        }
    };
    render_sizes();
    settings.update(game, {}, 640, 480, true);
    // Exercise the new controls page through the same pointer workflow as the game.
    for (const auto& widget : settings.ui().widgets()) {
        if (widget.id != feature_lab::Settings::controls)
            continue;
        const auto point = (widget.bounds.min + widget.bounds.max) * 0.5F;
        Input pointer;
        pointer.move_pointer(point);
        pointer.set_primary(true);
        settings.update(game, pointer.consume(), 640, 480, true);
        pointer.set_primary(false);
        settings.update(game, pointer.consume(), 640, 480, true);
        break; // The layout was replaced; do not advance the old iterator.
    }
    CHECK(settings.controls_opened());
    render_sizes();
}
TEST(title_renders_before_simulation_and_reuses_gpu_resources) {
    Window window(640, 480, "Title flow", false);
    Renderer renderer;
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    feature_lab::SceneFlow flow(game, true);
    const auto target = renderer.create_target(640, 480);
    for (const auto size : {RenderTargetSize{320, 240}, RenderTargetSize{640, 480},
                            RenderTargetSize{1280, 720}, RenderTargetSize{1050, 1360}}) {
        renderer.resize_target(target, size.width, size.height);
        for (int cycle = 0; cycle < 3; ++cycle) {
            flow.prepare(size.width, size.height, true);
            renderer.begin(target, {{0, 0}, 2});
            flow.draw(renderer, size.width, size.height);
            renderer.end();
            CHECK(renderer.pixel(size.width / 2, size.height / 2)[2] > 20);
            CHECK(renderer.stats().quads > 100 && renderer.healthy());
            CHECK(renderer.live_targets() == 1 && renderer.live_textures() == 0);
            flow.start_new(game);
            flow.enter_title(game);
        }
    }
}
TEST(collision_station_draws_circle_and_switch_state) {
    Window window(320, 240, "Collision station", false);
    Renderer renderer;
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    feature_lab::CollisionVisuals visuals(renderer);
    const auto draw = [&] {
        renderer.begin(320, 240, {{-25, -10}, 10}, {0, 0, 0, 1});
        visuals.draw(renderer, game, 1);
        renderer.end();
    };
    draw();
    const auto p = Camera{{-25, -10}, 10}.world_to_screen(game.alarm_position(), 320, 240);
    const auto red = renderer.pixel(static_cast<int>(p.x), 239 - static_cast<int>(p.y));
    CHECK(red[0] > red[1] + 50);
    InputFrame press{};
    press[static_cast<std::size_t>(Key::interact)].pressed = true;
    game.update(press);
    draw();
    const auto green = renderer.pixel(static_cast<int>(p.x), 239 - static_cast<int>(p.y));
    CHECK(green[1] > green[0] + 50 && renderer.healthy());
}
TEST(debug_shapes_pixels_clipping_and_invalid_input) {
    Window window(128, 128, "Debug geometry", false);
    Renderer renderer;
    const auto target = renderer.create_target(128, 128);
    renderer.begin(target, {{64, 64}, 128}, {0, 0, 0, 1});
    debug_shape(renderer, Shape2D::box({32, 32}, {16, 16}), 2, {1, 0, 0, 1});
    debug_shape(renderer, Shape2D::circle({96, 32}, 16), 2, {0, 1, 0, 1});
    renderer.push_clip({0, 0, 64, 128});
    debug_line(renderer, {16, 96}, {112, 96}, 4, {0, 0, 1, 1});
    renderer.pop_clip();
    const auto quads = renderer.stats().quads;
    CHECK(quads == 37);
    debug_line(renderer, {0, 0}, {0, 0}, 1, {});
    rejects([&] { debug_shape(renderer, Shape2D::circle({}, -1), 1, {}); });
    rejects([&] { debug_shape(renderer, Shape2D::box({}, {1, 1}), 0, {}); });
    rejects([&] { debug_rect(renderer, {{1, 1}, {0, 0}}, 1, {}); });
    rejects([&] { debug_line(renderer, {}, {INFINITY, 0}, 1, {}); });
    rejects([&] { debug_shape(renderer, Shape2D::circle({}, 1), 1, {NAN, 0, 0, 1}); });
    CHECK(renderer.stats().quads == quads);
    renderer.end();
    const auto red = renderer.pixel(16, 32), green = renderer.pixel(111, 32);
    const auto blue = renderer.pixel(48, 96), clipped = renderer.pixel(80, 96);
    CHECK(red[0] > 240 && red[1] == 0);
    CHECK(green[1] > 240 && green[0] == 0);
    CHECK(blue[2] > 240 && blue[0] == 0);
    CHECK(clipped[0] == 0 && clipped[1] == 0 && clipped[2] == 0);
    CHECK(renderer.pixel(32, 32)[0] == 0); // Outlines do not fill interiors.
    CHECK(renderer.healthy());
}
TEST(diagnostics_overlay_sizes_toggles_and_resources) {
    Window window(128, 128, "Diagnostic overlay", false);
    Renderer renderer;
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    feature_lab::Diagnostics diagnostics;
    const auto textures = renderer.live_textures();
    for (const auto [width, height] :
         {std::pair{320, 240}, {640, 480}, {1280, 720}, {1050, 1360}}) {
        const auto target = renderer.create_target(width, height);
        const float w = static_cast<float>(width), h = static_cast<float>(height);
        const Camera camera{{w / 2, -h / 2}, h};
        diagnostics.controls = {};
        renderer.begin(target, camera, {1, 0, 0, 1});
        diagnostics.draw_hud(renderer, game, width, height, 0);
        diagnostics.draw_world(renderer, game, game.camera(width, height, 1), width, height);
        CHECK(renderer.stats().quads == 0);
        renderer.end();
        diagnostics.controls = {true, true};
        diagnostics.record({16, 2}, {100, 20, 3}, {10, 0, 1}, 1);
        renderer.begin(target, camera, {1, 0, 0, 1});
        diagnostics.draw_hud(renderer, game, width, height, 0.5);
        renderer.end();
        CHECK(renderer.stats().quads > 100);
        const float scale = feature_lab::hud_scale(width, height);
        const auto panel = renderer.pixel(static_cast<int>(26 * scale),
                                          height - 1 - static_cast<int>(105 * scale));
        const auto outside = renderer.pixel(static_cast<int>(20 * scale),
                                            height - 1 - static_cast<int>(105 * scale));
        CHECK(panel[0] < 100 && outside[0] > 240);
        renderer.begin(target, game.camera(width, height, 1));
        diagnostics.draw_world(renderer, game, game.camera(width, height, 1), width, height);
        renderer.end();
        CHECK(renderer.stats().quads > 4);
        CHECK(renderer.live_textures() == textures && renderer.live_targets() == 1);
        renderer.release(target);
        CHECK(renderer.live_targets() == 0 && renderer.target_bytes() == 0);
    }
    CHECK(renderer.healthy());
}
TEST(hierarchical_decoration_pixels_across_camera_zoom_and_resize) {
    Window window(64, 64, "Hierarchy pixels", false);
    Renderer renderer;
    const auto target = renderer.create_target(256, 128);
    const auto texture = renderer.upload({2, 1, {255, 0, 0, 255, 0, 0, 255, 255}});
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    const auto entity = game.world.find(51);
    for (unsigned tick : {0U, 60U, 120U, 239U, 240U}) {
        while (game.ticks < tick)
            game.update({});
        for (const auto [width, height] :
             std::array<std::array<int, 2>, 3>{{{256, 128}, {128, 256}, {192, 192}}}) {
            renderer.resize_target(target, width, height);
            for (float zoom : {5.0F, 8.0F}) {
                const auto model = game.render_transform(entity, 0.5F) *
                                   Transform::from({}, 0, game.world.get(entity).sprite->size);
                const Camera camera{{model.x - 0.3F, model.y + 0.15F}, zoom};
                renderer.begin(target, camera, {0, 0, 0, 1});
                renderer.sprite(texture, model);
                renderer.end();
                for (float x : {-0.25F, 0.25F}) {
                    const auto screen = camera.world_to_screen(model.apply({x, 0}), width, height);
                    const auto pixel = renderer.pixel(static_cast<int>(screen.x),
                                                      height - 1 - static_cast<int>(screen.y));
                    // Negative scale reverses winding; inherited nonuniform scale produces shear.
                    CHECK(pixel[x < 0 ? 0 : 2] > 250);
                    CHECK(pixel[x < 0 ? 2 : 0] < 3 && pixel[1] < 3);
                }
                CHECK(renderer.pixel(0, 0)[0] < 3);
                CHECK(renderer.stats().quads == 1 && renderer.stats().culled == 0);
            }
        }
    }
    renderer.release(texture);
    renderer.release(target);
    CHECK(renderer.live_textures() == 0 && renderer.live_targets() == 0);
    CHECK(renderer.healthy());
}
TEST(transformed_culling_keeps_partially_visible_rotated_sprite) {
    Window window(64, 64, "Rotated culling", false);
    Renderer renderer;
    const auto target = renderer.create_target(128, 128);
    const auto texture = renderer.upload({1, 1, {255, 255, 255, 255}});
    const Camera camera{{}, 8};
    Scene scene;
    const auto parent = scene.create(1, "parent"), child = scene.create(2, "child");
    scene.set_parent(child, parent);
    scene.set_transform(parent, {{4.5F, 0}, std::numbers::pi_v<float> / 4, {2, 1}});
    scene.set_transform(child, {{}, 0, {2, 1}});
    renderer.begin(target, camera, {0, 0, 0, 1});
    renderer.sprite(texture, scene.world_transform(child));
    scene.set_transform(parent, {{8, 0}, std::numbers::pi_v<float> / 4, {2, 1}});
    renderer.sprite(texture, scene.world_transform(child));
    renderer.end();
    const auto screen = camera.world_to_screen({3.6F, -0.9F}, 128, 128);
    CHECK(renderer.pixel(static_cast<int>(screen.x), 127 - static_cast<int>(screen.y))[0] > 250);
    CHECK(renderer.stats().quads == 1 && renderer.stats().culled == 1);
    CHECK(renderer.healthy());
}
TEST(target_count_limit_and_stale_handles) {
    Window window(16, 16, "Target limits", false);
    Renderer renderer;
    std::array<RenderTargetHandle, Renderer::max_targets> targets{};
    for (auto& target : targets)
        target = renderer.create_target(8, 8);
    rejects([&] { renderer.create_target(8, 8); });
    rejects([&] { renderer.target_size({0, targets[0].serial + 1000}); });
    CHECK(renderer.live_targets() == targets.size());
    for (auto target : targets)
        renderer.release(target);
    CHECK(renderer.live_targets() == 0 && renderer.target_bytes() == 0);
    CHECK(renderer.healthy());
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY")) {
        std::cout << "SKIP: DISPLAY unavailable\n";
        return 77;
    }
    return testing::run_tests();
}
