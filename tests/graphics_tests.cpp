#include "engine/atlas.hpp"
#include "engine/renderer.hpp"
#include "engine/window.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY")) {
        std::cerr << "SKIP: DISPLAY is unavailable\n";
        return 77;
    }
    try {
        engine::Window window(128, 128, "Engine graphics tests", false);
        engine::Renderer renderer;
        engine::Camera camera{{0, 0}, 2};
        renderer.begin(128, 128, camera, {0, 0, 0, 1});
        renderer.quad({0, 0}, {1.5F, 1.5F}, {1, 0, 0, 1});
        renderer.quad({0, 0}, {0.5F, 0.5F}, {0, 0, 1, 0.5F});
        renderer.quad({100, 100}, {1, 1}, {1, 1, 1, 1});
        renderer.end();
        const auto center = renderer.pixel(64, 64), red = renderer.pixel(32, 64),
                   black = renderer.pixel(0, 0);
        check(center[0] > 180 && center[0] < 195 && center[2] > 180 && center[2] < 195 &&
                  center[1] < 3,
              "Linear alpha blend/sRGB output or draw order is incorrect");
        check(red[0] > 250 && red[1] < 3 && red[2] < 3, "Opaque quad failed");
        check(black[0] < 3 && black[1] < 3 && black[2] < 3, "Clear/camera coverage failed");
        check(renderer.stats().draws == 1 && renderer.stats().quads == 2 &&
                  renderer.stats().culled == 1,
              "Batch/culling counters incorrect");
        renderer.begin(128, 128, camera, {0, 0, 0, 1});
        renderer.quad({0, 0}, {1, 1}, {1, 1, 1, 1}, 0, true);
        renderer.end();
        const auto white = renderer.pixel(40, 40), gray = renderer.pixel(88, 40);
        check(white[0] > 250 && gray[0] >= 155 && gray[0] <= 165,
              "Atlas sampling or texture sRGB decoding failed");
        renderer.begin(128, 128, camera);
        for (std::size_t i = 0; i < engine::Renderer::batch_capacity + 1; ++i)
            renderer.quad({0, 0}, {0.01F, 0.01F}, {1, 1, 1, 1});
        renderer.end();
        check(renderer.stats().draws == 2, "Batch rollover failed");
        window.resize(192, 96);
        engine::Input input;
        window.poll(input);
        check(window.width() == 192 && window.height() == 96, "Resize event dimensions incorrect");
        renderer.begin(window.width(), window.height(), camera, {0, 1, 0, 1});
        renderer.end();
        check(renderer.pixel(0, 0)[1] > 250, "Resize rendering failed");
        const auto uploaded = renderer.upload({2, 1, {255, 0, 0, 255, 0, 0, 255, 255}});
        renderer.begin(window.width(), window.height(), camera, {0, 0, 0, 1});
        renderer.sprite(uploaded, engine::Transform::from({}, 0, {2, 2}));
        renderer.end();
        check(renderer.pixel(80, 48)[0] > 250 && renderer.pixel(112, 48)[2] > 250,
              "Uploaded texture pixels incorrect");
        renderer.begin(window.width(), window.height(), camera, {0, 0, 0, 1});
        renderer.sprite(uploaded, engine::Transform::from({}, 0, {2, 2}), {}, {{1, 0}, {0, 1}});
        renderer.end();
        check(renderer.pixel(80, 48)[2] > 250 && renderer.pixel(112, 48)[0] > 250,
              "UV flip incorrect");
        renderer.begin(window.width(), window.height(), camera, {0, 0, 0, 1});
        renderer.sprite(uploaded, engine::Transform::from({}, 0, {2, 2}), {},
                        engine::atlas_uv(1, 2, 1));
        renderer.set_camera({{96, 48}, 96});
        renderer.quad({20, 20}, {20, 20}, {0, 1, 0, 1});
        renderer.end();
        check(renderer.pixel(96, 48)[2] > 250, "Atlas frame or world camera flush failed");
        check(renderer.pixel(20, 20)[1] > 250, "HUD camera projection failed");
        check(renderer.stats().quads == 2 && renderer.stats().draws == 2,
              "Camera switch cleared frame counters");
        renderer.release(uploaded);
        const auto replacement = renderer.upload({1, 1, {0, 255, 0, 255}});
        check(renderer.live_textures() == 1, "Texture release/reuse leaked resources");
        bool rejected = false;
        try {
            renderer.release(uploaded);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected, "Stale GPU texture handle accepted");
        renderer.release(replacement);
        check(renderer.live_textures() == 0, "Texture resources did not return to baseline");
        check(renderer.healthy(), "OpenGL reported errors");
        std::cout << "PASS GPU pixels, alpha/sRGB, atlas, ordering, culling, batch rollover, "
                     "resize and clean GL state\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL graphics: " << e.what() << '\n';
        return 1;
    }
}
