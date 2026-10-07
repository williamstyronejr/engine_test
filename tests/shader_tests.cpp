#include "../engine/src/gl.hpp"
#include "../examples/feature_lab/settings.hpp"
#include "../examples/feature_lab/shader_tools.hpp"
#include "engine/window.hpp"
#include "test.hpp"
#include <GL/glx.h>
#include <cstdlib>
#include <filesystem>

namespace {
using namespace testing;
using namespace engine;
std::string replace(std::string_view source, std::string_view old, std::string_view replacement) {
    std::string result(source);
    const auto at = result.find(old);
    CHECK(at != std::string::npos);
    result.replace(at, old.size(), replacement);
    return result;
}
struct Fixture {
    engine::Window window{64, 64, "Shader reload tests", false};
    Renderer renderer;
    RenderTargetHandle target = renderer.create_target(64, 64);
    std::array<unsigned char, 4> draw() {
        renderer.begin(target, {{0, 0}, 2}, {0, 0, 0, 1});
        renderer.quad({}, {2, 2}, {1, 0, 0, 1});
        renderer.end();
        return renderer.pixel(32, 32);
    }
    void retained(std::string_view vertex, std::string_view fragment, std::string_view message) {
        const auto before = draw();
        const auto revision = renderer.shader_revision();
        const auto result = renderer.reload_shaders(vertex, fragment);
        CHECK(!result.applied && result.revision == revision);
        CHECK(result.diagnostics.find(message) != std::string::npos);
        CHECK(result.diagnostics.size() < 13000);
        CHECK(renderer.shader_revision() == revision && draw() == before);
        CHECK(renderer.healthy());
    }
};
TEST(shipped_shaders_match_fallback_and_compile) {
    Fixture f;
    const AssetRoot assets(TEST_ASSET_ROOT);
    const auto vertex = assets.text("shaders/sprite.vert"),
               fragment = assets.text("shaders/sprite.frag");
    CHECK(vertex == Renderer::default_vertex_shader());
    CHECK(fragment == Renderer::default_fragment_shader());
    CHECK(f.renderer.shader_revision() == 1);
    const auto before = f.draw();
    const auto result = f.renderer.reload_shaders(vertex, fragment);
    CHECK(result.applied && result.revision == 2 && f.draw() == before);
    CHECK(f.renderer.healthy());
}
TEST(compile_and_link_errors_preserve_rendering_and_health) {
    Fixture f;
    const auto vertex = Renderer::default_vertex_shader(),
               fragment = Renderer::default_fragment_shader();
    f.retained("#version 460 core\n#error vertex failure\n", fragment, "sprite.vert");
    f.retained(vertex, "#version 460 core\n#error fragment failure\n", "sprite.frag");
    auto mismatch = replace(fragment, "in vec2 uv;", "in vec3 uv;");
    mismatch = replace(mismatch, "texture(atlas,uv)", "texture(atlas,uv.xy)");
    f.retained(vertex, mismatch, "link failed");
    CHECK(f.renderer.reload_shaders(vertex, fragment).applied);
    CHECK(f.renderer.shader_revision() == 2 && f.draw()[0] > 250);
}
TEST(interface_rejects_incompatible_locations_types_and_resources) {
    Fixture f;
    const auto vertex = Renderer::default_vertex_shader(),
               fragment = Renderer::default_fragment_shader();
    f.retained(replace(vertex, "location=0", "location=3"), fragment, "interface");
    f.retained(replace(vertex, "location=0", "location=0,component=2"), fragment, "component");
    f.retained(vertex, replace(fragment, "location=0) out", "location=1) out"), "interface");
    auto wrong_type = replace(fragment, "uniform int premultiplied", "uniform float premultiplied");
    wrong_type = replace(wrong_type, "premultiplied != 0", "premultiplied != 0.0");
    f.retained(vertex, wrong_type, "interface");
    auto extra = replace(fragment, "void main()", "uniform float gain;\nvoid main()");
    extra = replace(extra, "texture(atlas,uv)*color", "texture(atlas,uv)*color*gain");
    f.retained(vertex, extra, "resource count");
    auto storage = replace(fragment, "void main()",
                           "layout(std430,binding=0) buffer Extra { float gain; };\nvoid main()");
    storage = replace(storage, "texture(atlas,uv)*color", "texture(atlas,uv)*color*gain");
    f.retained(vertex, storage, "resource count");
    f.retained(vertex, replace(fragment, "if (premultiplied != 0)", "if (false)"),
               "resource count");
}
TEST(bounded_sources_and_reload_during_pass_are_transactional) {
    Fixture f;
    const auto vertex = Renderer::default_vertex_shader(),
               fragment = Renderer::default_fragment_shader();
    f.retained({}, fragment, "source must");
    f.retained(vertex, std::string(Renderer::max_shader_source_bytes + 1, ' '), "source must");
    auto nul = std::string(fragment) + '\0' + "#error hidden";
    f.retained(vertex, nul, "without NUL");
    auto padded = std::string(fragment);
    padded.resize(Renderer::max_shader_source_bytes, ' ');
    CHECK(f.renderer.reload_shaders(vertex, padded).applied);
    // Views need no terminating NUL; bytes beyond the view must not reach the compiler.
    auto backing = std::string(fragment) + "#error outside view\n";
    CHECK(f.renderer.reload_shaders(vertex, std::string_view(backing).substr(0, fragment.size()))
              .applied);
    f.renderer.begin(f.target, {{0, 0}, 2});
    f.renderer.quad({}, {2, 2}, {1, 0, 0, 1});
    const auto revision = f.renderer.shader_revision();
    rejects([&] { f.renderer.reload_shaders(vertex, fragment); });
    CHECK(f.renderer.shader_revision() == revision && f.renderer.stats().quads == 1);
    f.renderer.end();
    CHECK(f.renderer.pixel(32, 32)[0] > 250 && f.renderer.healthy());
}
TEST(success_changes_pixels_and_rebinds_sampler_and_premultiplied_uniform) {
    Fixture f;
    const auto vertex = Renderer::default_vertex_shader();
    auto fragment = replace(Renderer::default_fragment_shader(), "binding=0", "binding=3");
    fragment = replace(fragment, "location=0) uniform", "location=7) uniform");
    fragment = replace(fragment, "texture(atlas,uv)*color;",
                       "texture(atlas,uv)*color; output_color.rgb = output_color.bgr;");
    CHECK(f.renderer.reload_shaders(vertex, fragment).applied);
    const auto blue = f.draw();
    CHECK(blue[2] > 250 && blue[0] == 0);
    f.retained(vertex, "broken",
               "compilation failed"); // Retain the custom program, not the fallback.
    const auto uploaded = f.renderer.upload({1, 1, {255, 0, 0, 255}});
    f.renderer.begin(f.target, {{0, 0}, 2}, {0, 0, 0, 0});
    f.renderer.sprite(uploaded, Transform::from({}, 0, {2, 2}), {1, 1, 1, 0.5F});
    f.renderer.end();
    const auto transparent = f.renderer.pixel(32, 32);
    CHECK(transparent[2] > 180 && transparent[2] < 195 && transparent[3] == 128);
    const auto composite = f.renderer.create_target(64, 64);
    f.renderer.begin(composite, {{0, 0}, 2}, {0, 0, 0, 1});
    f.renderer.target_sprite(f.target, Transform::from({}, 0, {2, 2}), {1, 1, 1, 0.5F});
    f.renderer.end();
    const auto red = f.renderer.pixel(32, 32); // BGR swaps a second time on composition.
    CHECK(red[0] > 130 && red[0] < 145 && red[2] == 0 && red[3] == 255);
    CHECK(f.renderer.healthy());
}
TEST(repeated_reload_retires_programs_and_detaches_shader_objects) {
    Fixture f;
    Gl gl;
    const auto is_program = reinterpret_cast<PFNGLISPROGRAMPROC>(
        glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glIsProgram")));
    CHECK(is_program != nullptr);
    for (int i = 0; i < 24; ++i) {
        f.draw();
        GLint old{};
        gl.GetIntegerv(GL_CURRENT_PROGRAM, &old);
        CHECK(old != 0 && is_program(static_cast<GLuint>(old)));
        f.retained(Renderer::default_vertex_shader(), "broken", "compilation failed");
        CHECK(is_program(static_cast<GLuint>(old)));
        CHECK(f.renderer
                  .reload_shaders(Renderer::default_vertex_shader(),
                                  Renderer::default_fragment_shader())
                  .applied);
        CHECK(!is_program(static_cast<GLuint>(old)));
        f.draw();
        GLint current{}, attached{};
        gl.GetIntegerv(GL_CURRENT_PROGRAM, &current);
        gl.GetProgramiv(static_cast<GLuint>(current), GL_ATTACHED_SHADERS, &attached);
        CHECK(attached == 0);
        CHECK(f.renderer.live_targets() == 1 && f.renderer.live_textures() == 0);
        CHECK(f.renderer.healthy());
    }
    CHECK(f.renderer.shader_revision() == 25);
}
TEST(expected_compile_failure_does_not_hide_real_gl_errors) {
    Fixture f;
    f.retained(Renderer::default_vertex_shader(), "broken", "compilation failed");
    Gl gl;
    gl.Enable(0xffffffffU); // Deliberate API misuse, distinct from a compiler rejection.
    CHECK(!f.renderer.healthy());
}
TEST(feature_lab_file_failure_recovery_edges_and_menu_capture) {
    Fixture f;
    std::string pattern =
        (std::filesystem::temp_directory_path() / "engine-shaders-XXXXXX").string();
    CHECK(mkdtemp(pattern.data()) != nullptr);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    } cleanup{pattern};
    std::filesystem::create_directory(cleanup.path / "shaders");
    const AssetRoot files(cleanup.path);
    feature_lab::ShaderTools shaders;
    shaders.reload(f.renderer, files);
    CHECK(!shaders.result().applied && shaders.result().revision == 1);
    files.write_text("shaders/sprite.vert", Renderer::default_vertex_shader());
    files.write_text("shaders/sprite.frag", Renderer::default_fragment_shader());
    Input input;
    input.set(Key::reload_shaders, true);
    shaders.update(f.renderer, files, input.consume());
    CHECK(shaders.result().applied && f.renderer.shader_revision() == 2);
    input.set(Key::reload_shaders, true);
    shaders.update(f.renderer, files, input.consume());
    CHECK(f.renderer.shader_revision() == 2);
    shaders.fail_demo(f.renderer);
    CHECK(!shaders.result().applied && f.draw()[0] > 250);
    files.write_text("shaders/sprite.frag",
                     std::string(Renderer::max_shader_source_bytes + 1, ' '));
    shaders.reload(f.renderer, files);
    CHECK(!shaders.result().applied &&
          shaders.result().diagnostics.find("size limit") != std::string::npos);
    files.write_text("shaders/sprite.frag", Renderer::default_fragment_shader());
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    feature_lab::Settings settings;
    input.release_all();
    input.set(Key::settings, true);
    settings.update(game, input.consume(), 640, 480, false);
    input.set(Key::settings, false);
    input.set(Key::reload_shaders, true);
    input.set(Key::shader_error, true);
    shaders.update(f.renderer, files,
                   settings.update(game, input.consume(), 640, 480, false).gameplay);
    CHECK(f.renderer.shader_revision() == 2);
    input.set(Key::settings, true);
    shaders.update(f.renderer, files,
                   settings.update(game, input.consume(), 640, 480, false).gameplay);
    CHECK(f.renderer.shader_revision() == 2);
    input.release_all();
    settings.update(game, input.consume(), 640, 480, false);
    input.set(Key::reload_shaders, true);
    input.set(Key::shader_error, true);
    shaders.update(f.renderer, files,
                   settings.update(game, input.consume(), 640, 480, false).gameplay);
    CHECK(shaders.result().applied && f.renderer.shader_revision() == 3);
    CHECK(f.draw()[0] > 250 && f.renderer.healthy());
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY")) {
        std::cout << "SKIP: DISPLAY unavailable\n";
        return 77;
    }
    return testing::run_tests();
}
