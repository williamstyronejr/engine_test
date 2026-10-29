#include "../engine/src/gl.hpp"
#include "engine/renderer.hpp"
#include "engine/window.hpp"
#include "test.hpp"
#include <GL/glx.h>
#include <cstdlib>

namespace {
using namespace engine;
using namespace testing;
TEST(renderer_memory_lifecycle_and_logical_store) {
    Window window(64, 32, "Renderer memory", false);
    const auto get_buffer = reinterpret_cast<PFNGLGETBUFFERPARAMETERIVPROC>(
        glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glGetBufferParameteriv")));
    const auto is_buffer = reinterpret_cast<PFNGLISBUFFERPROC>(
        glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glIsBuffer")));
    CHECK(get_buffer && is_buffer);
    for (int cycle = 0; cycle < 4; ++cycle) {
        GLint object{};
        {
            Renderer renderer;
            const auto baseline = renderer.memory();
            constexpr auto payload = Renderer::batch_capacity * 6 * 32;
            CHECK(baseline.cpu_vertex_reserved_bytes >= payload);
            CHECK(baseline.cpu_vertex_used_bytes == 0);
            CHECK(baseline.gpu_vertex_store_bytes == payload);
            CHECK(baseline.gpu_builtin_texture_bytes == 32);
            CHECK(baseline.gpu_font_texture_bytes == renderer.font_bytes());
            CHECK(baseline.gpu_uploaded_texture_bytes == 0 &&
                  baseline.gpu_target_texture_bytes == 0);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &object);
            CHECK(object > 0 && is_buffer(static_cast<GLuint>(object)));
            GLint store{};
            get_buffer(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &store);
            CHECK(static_cast<std::size_t>(store) == baseline.gpu_vertex_store_bytes);
            const auto target = renderer.create_target(64, 32);
            const auto texture = renderer.upload({1, 1, {255, 255, 255, 255}});
            CHECK(renderer.memory().gpu_uploaded_texture_bytes == 4);
            CHECK(renderer.memory().gpu_target_texture_bytes == 64 * 32 * 4);
            renderer.begin(target, {{}, 32});
            renderer.quad({}, {4, 4}, {1, 0, 0, 1});
            CHECK(renderer.memory().cpu_vertex_used_bytes == 6 * 32);
            CHECK(renderer.memory().cpu_vertex_reserved_bytes ==
                  baseline.cpu_vertex_reserved_bytes);
            CHECK(renderer.stats().draws == 0); // Observing memory must not flush.
            renderer.set_camera({{}, 32});
            CHECK(renderer.memory().cpu_vertex_used_bytes == 0 && renderer.stats().draws == 1);
            renderer.sprite(texture, Transform::from({}, 0, {4, 4}), {0, 1, 0, 1});
            renderer.end();
            CHECK(renderer.memory().cpu_vertex_used_bytes == 0);
            CHECK(renderer.pixel(32, 16)[1] == 255);
            get_buffer(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &store);
            CHECK(static_cast<std::size_t>(store) == baseline.gpu_vertex_store_bytes);
            TextureData replacement{2, 2, std::vector<std::uint8_t>(16, 255)};
            renderer.replace_textures(std::array{TextureReplacement{texture, replacement}});
            renderer.resize_target(target, 16, 8);
            CHECK(renderer.memory().gpu_uploaded_texture_bytes == 16);
            CHECK(renderer.memory().gpu_target_texture_bytes == 16 * 8 * 4);
            const auto resized = renderer.memory();
            TextureData invalid{1, 1, {}};
            rejects([&] {
                renderer.replace_textures(std::array{TextureReplacement{texture, invalid}});
            });
            rejects([&] { renderer.resize_target(target, 0, 8); });
            CHECK(renderer.memory() == resized);
            renderer.release(texture);
            renderer.release(target);
            CHECK(renderer.memory() == baseline);
            CHECK(renderer.healthy());
        }
        CHECK(!is_buffer(static_cast<GLuint>(object)));
    }
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY"))
        return 77;
    return testing::run_tests();
}
