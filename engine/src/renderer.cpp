#include "engine/renderer.hpp"
#include "gl.hpp"
#include "shader_program.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace engine {
namespace {
struct Vertex {
    float x, y, u, v;
    Color color;
};
constexpr const char* vertex_source = R"(#version 460 core
layout(location=0) in vec2 position;
layout(location=1) in vec2 texcoord;
layout(location=2) in vec4 tint;
out vec2 uv;
out vec4 color;
void main() { gl_Position=vec4(position,0,1); uv=texcoord; color=tint; }
)";
constexpr const char* fragment_source = R"(#version 460 core
layout(binding=0) uniform sampler2D atlas;
layout(location=0) uniform int premultiplied;
in vec2 uv;
in vec4 color;
layout(location=0) out vec4 output_color;
void main() {
    output_color=texture(atlas,uv)*color;
    if (premultiplied != 0) output_color.rgb *= color.a;
}
)";
std::array<unsigned char, 7> glyph(char c) {
    switch (c) {
    case '%':
        return {25, 26, 2, 4, 8, 11, 19};
    case '+':
        return {0, 4, 4, 31, 4, 4, 0};
    case 'A':
        return {14, 17, 17, 31, 17, 17, 17};
    case 'B':
        return {30, 17, 17, 30, 17, 17, 30};
    case 'C':
        return {14, 17, 16, 16, 16, 17, 14};
    case 'D':
        return {30, 17, 17, 17, 17, 17, 30};
    case 'E':
        return {31, 16, 16, 30, 16, 16, 31};
    case 'F':
        return {31, 16, 16, 30, 16, 16, 16};
    case 'G':
        return {14, 17, 16, 23, 17, 17, 15};
    case 'H':
        return {17, 17, 17, 31, 17, 17, 17};
    case 'I':
        return {14, 4, 4, 4, 4, 4, 14};
    case 'J':
        return {7, 2, 2, 2, 18, 18, 12};
    case 'K':
        return {17, 18, 20, 24, 20, 18, 17};
    case 'L':
        return {16, 16, 16, 16, 16, 16, 31};
    case 'M':
        return {17, 27, 21, 21, 17, 17, 17};
    case 'N':
        return {17, 25, 25, 21, 19, 19, 17};
    case 'O':
        return {14, 17, 17, 17, 17, 17, 14};
    case 'P':
        return {30, 17, 17, 30, 16, 16, 16};
    case 'Q':
        return {14, 17, 17, 17, 21, 18, 13};
    case 'R':
        return {30, 17, 17, 30, 20, 18, 17};
    case 'S':
        return {15, 16, 16, 14, 1, 1, 30};
    case 'T':
        return {31, 4, 4, 4, 4, 4, 4};
    case 'U':
        return {17, 17, 17, 17, 17, 17, 14};
    case 'V':
        return {17, 17, 17, 17, 17, 10, 4};
    case 'W':
        return {17, 17, 17, 21, 21, 21, 10};
    case 'X':
        return {17, 17, 10, 4, 10, 17, 17};
    case 'Y':
        return {17, 17, 10, 4, 4, 4, 4};
    case 'Z':
        return {31, 1, 2, 4, 8, 16, 31};
    case '0':
        return {14, 17, 19, 21, 25, 17, 14};
    case '1':
        return {4, 12, 4, 4, 4, 4, 14};
    case '2':
        return {14, 17, 1, 2, 4, 8, 31};
    case '3':
        return {30, 1, 1, 14, 1, 1, 30};
    case '4':
        return {2, 6, 10, 18, 31, 2, 2};
    case '5':
        return {31, 16, 16, 30, 1, 1, 30};
    case '6':
        return {14, 16, 16, 30, 17, 17, 14};
    case '7':
        return {31, 1, 2, 4, 8, 8, 8};
    case '8':
        return {14, 17, 17, 14, 17, 17, 14};
    case '9':
        return {14, 17, 17, 15, 1, 1, 14};
    case '-':
        return {0, 0, 0, 31, 0, 0, 0};
    case '/':
        return {1, 1, 2, 4, 8, 16, 16};
    case ':':
        return {0, 4, 4, 0, 4, 4, 0};
    case '.':
        return {0, 0, 0, 0, 0, 4, 4};
    case ' ':
        return {};
    default:
        return {31, 17, 5, 4, 0, 4, 0};
    }
}
void APIENTRY debug_callback(GLenum source, GLenum, GLuint, GLenum severity, GLsizei length,
                             const GLchar* text, const void* user) {
    // Expected shader compiler diagnostics are returned by the reload operation.
    // API/driver errors still permanently mark the renderer unhealthy.
    if (source == GL_DEBUG_SOURCE_SHADER_COMPILER)
        return;
    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        ++*static_cast<unsigned int*>(const_cast<void*>(user));
        std::cerr << "[OpenGL error] " << std::string_view(text, static_cast<std::size_t>(length))
                  << '\n';
    }
}
} // namespace
struct Renderer::Impl {
    Gl gl;
    GLuint vao{}, vbo{}, texture{}, program{};
    GLint premultiplied_location{};
    std::uint64_t shader_revision{1};
    struct TextureSlot {
        GLuint object{};
        std::uint64_t serial{};
        std::size_t bytes{};
    };
    std::array<TextureSlot, max_textures> textures{};
    struct TargetSlot {
        GLuint framebuffer{}, texture{};
        int width{}, height{};
        std::uint64_t serial{};
        bool ready{};
    };
    std::array<TargetSlot, max_targets> targets{};
    std::size_t target_memory{};
    GLuint framebuffer{}, destination_texture{};
    std::array<PixelRect, 16> clips{};
    std::size_t clip_depth{};
    GLuint batch_texture{};
    bool batch_premultiplied{};
    unsigned int high_errors{};
    std::vector<Vertex> vertices;
    Camera camera;
    Vec2 extent;
    int width{}, height{};
    bool recording{};
    RenderStats stats;
    ~Impl() {
        gl.DebugMessageCallback(nullptr, nullptr);
        gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
        for (auto& target : targets)
            destroy_target(target);
        for (const auto& slot : textures)
            if (slot.object)
                gl.DeleteTextures(1, &slot.object);
        if (program)
            gl.DeleteProgram(program);
        if (texture)
            gl.DeleteTextures(1, &texture);
        if (vbo)
            gl.DeleteBuffers(1, &vbo);
        if (vao)
            gl.DeleteVertexArrays(1, &vao);
    }
    void initialize() {
        vertices.reserve(batch_capacity * 6);
        gl.Enable(GL_DEBUG_OUTPUT);
        gl.Enable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        gl.DebugMessageCallback(debug_callback, &high_errors);
        std::string diagnostics;
        try {
            const auto initial =
                detail::build_sprite_program(gl, vertex_source, fragment_source, diagnostics);
            program = initial.id;
            premultiplied_location = initial.premultiplied;
        } catch (const std::runtime_error& error) {
            throw std::runtime_error(diagnostics + error.what());
        }
        if (!diagnostics.empty())
            std::cerr << "[shader startup] " << diagnostics;
        gl.GenVertexArrays(1, &vao);
        gl.BindVertexArray(vao);
        gl.GenBuffers(1, &vbo);
        gl.BindBuffer(GL_ARRAY_BUFFER, vbo);
        gl.BufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(batch_capacity * 6 * sizeof(Vertex)),
                      nullptr, GL_STREAM_DRAW);
        gl.EnableVertexAttribArray(0);
        gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
        gl.EnableVertexAttribArray(1);
        gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                               reinterpret_cast<void*>(offsetof(Vertex, u)));
        gl.EnableVertexAttribArray(2);
        gl.VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                               reinterpret_cast<void*>(offsetof(Vertex, color)));
        // In-house 4x2 atlas: white left half, checker right half. No decoder/library.
        constexpr unsigned char pixels[] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                            255, 160, 160, 160, 255, 255, 255, 255, 255, 255, 255,
                                            255, 255, 160, 160, 160, 255, 255, 255, 255, 255};
        gl.GenTextures(1, &texture);
        gl.BindTexture(GL_TEXTURE_2D, texture);
        gl.TexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, 4, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      pixels);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    GLuint allocate_texture(const TextureData& data) {
        GLint maximum{}, binding{};
        gl.GetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
        if (data.width > static_cast<std::uint32_t>(maximum) ||
            data.height > static_cast<std::uint32_t>(maximum))
            throw std::invalid_argument("Texture exceeds driver size limit");
        gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
        if (gl.GetError() != GL_NO_ERROR)
            throw std::runtime_error("OpenGL error before texture allocation");
        GLuint object{};
        gl.GenTextures(1, &object);
        if (!object)
            throw std::runtime_error("Texture object allocation failed");
        gl.BindTexture(GL_TEXTURE_2D, object);
        gl.TexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, static_cast<GLsizei>(data.width),
                      static_cast<GLsizei>(data.height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      data.rgba.data());
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        const auto error = gl.GetError();
        gl.BindTexture(GL_TEXTURE_2D, static_cast<GLuint>(binding));
        if (error != GL_NO_ERROR) {
            gl.DeleteTextures(1, &object);
            throw std::runtime_error("Texture allocation/upload failed");
        }
        return object;
    }
    GLuint texture_object(TextureHandle handle) const {
        if (handle.slot >= textures.size() || !handle.serial ||
            textures[handle.slot].serial != handle.serial || !textures[handle.slot].object)
            throw std::invalid_argument("Invalid, foreign, or stale texture handle");
        return textures[handle.slot].object;
    }
    TargetSlot& target_slot(RenderTargetHandle handle) {
        if (handle.slot >= targets.size() || !handle.serial ||
            targets[handle.slot].serial != handle.serial || !targets[handle.slot].framebuffer)
            throw std::invalid_argument("Invalid, foreign, or stale render target handle");
        return targets[handle.slot];
    }
    static std::size_t bytes(int w, int h) {
        if (w <= 0 || h <= 0 || w > 4096 || h > 4096)
            throw std::invalid_argument("Render target dimensions must be in 1..4096");
        return static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4;
    }
    void destroy_target(TargetSlot& slot) {
        if (slot.framebuffer)
            gl.DeleteFramebuffers(1, &slot.framebuffer);
        if (slot.texture)
            gl.DeleteTextures(1, &slot.texture);
        slot = {};
    }
    TargetSlot allocate_target(int w, int h, std::size_t credit = 0) {
        if (bytes(w, h) > max_target_bytes - (target_memory - credit))
            throw std::runtime_error("Render target memory budget exceeded");
        GLint maximum{};
        gl.GetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
        if (w > maximum || h > maximum)
            throw std::invalid_argument("Render target exceeds driver limit");
        if (gl.GetError() != GL_NO_ERROR)
            throw std::runtime_error("OpenGL error before render target allocation");
        TargetSlot candidate{};
        candidate.width = w;
        candidate.height = h;
        try {
            gl.GenTextures(1, &candidate.texture);
            gl.BindTexture(GL_TEXTURE_2D, candidate.texture);
            gl.TexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                          nullptr);
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            gl.GenFramebuffers(1, &candidate.framebuffer);
            gl.BindFramebuffer(GL_FRAMEBUFFER, candidate.framebuffer);
            gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                    candidate.texture, 0);
            const auto status = gl.CheckFramebufferStatus(GL_FRAMEBUFFER);
            const auto error = gl.GetError();
            if (status != GL_FRAMEBUFFER_COMPLETE || error != GL_NO_ERROR)
                throw std::runtime_error("Render target framebuffer allocation failed");
        } catch (...) {
            destroy_target(candidate);
            gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            throw;
        }
        gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        return candidate;
    }
    void invalidate_readback(GLuint object) {
        if (framebuffer == object) {
            framebuffer = destination_texture = 0;
            width = height = 0;
            gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }
    void begin_pass(int w, int h, Camera c, Color clear, GLuint fbo, GLuint attachment) {
        if (recording)
            throw std::logic_error("Renderer::begin called before end");
        const auto size = c.extent(w, h);
        for (float value : {c.center.x, c.center.y, clear.r, clear.g, clear.b, clear.a})
            if (!std::isfinite(value))
                throw std::invalid_argument("Nonfinite camera or clear color");
        if (clear.r < 0 || clear.g < 0 || clear.b < 0 || clear.a < 0 || clear.a > 1)
            throw std::invalid_argument("Invalid clear color");
        extent = size;
        camera = c;
        width = w;
        height = h;
        framebuffer = fbo;
        destination_texture = attachment;
        recording = true;
        stats = {};
        clip_depth = 0;
        vertices.clear();
        gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        gl.Viewport(0, 0, w, h);
        gl.Disable(GL_SCISSOR_TEST);
        gl.Disable(GL_DEPTH_TEST);
        gl.Disable(GL_CULL_FACE);
        gl.Enable(GL_BLEND);
        gl.Enable(GL_FRAMEBUFFER_SRGB);
        // Offscreen storage is premultiplied in linear space, including its clear.
        const float alpha = attachment ? clear.a : 1.0F;
        gl.ClearColor(clear.r * alpha, clear.g * alpha, clear.b * alpha, clear.a);
        gl.Clear(GL_COLOR_BUFFER_BIT);
    }
    void apply_clip() {
        if (!clip_depth) {
            gl.Disable(GL_SCISSOR_TEST);
            return;
        }
        const auto r = clips[clip_depth - 1];
        gl.Enable(GL_SCISSOR_TEST);
        gl.Scissor(r.x, height - r.y - r.height, r.width, r.height);
    }
    void submit(Transform transform, Color color, Rect uv, GLuint object,
                bool premultiplied = false);
    void flush() {
        if (vertices.empty())
            return;
        gl.UseProgram(program);
        gl.Uniform1i(premultiplied_location, batch_premultiplied ? 1 : 0);
        gl.BlendFuncSeparate(batch_premultiplied ? GL_ONE : GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                             GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        gl.BindVertexArray(vao);
        gl.BindBuffer(GL_ARRAY_BUFFER, vbo);
        gl.ActiveTexture(GL_TEXTURE0);
        gl.BindTexture(GL_TEXTURE_2D, batch_texture);
        // Orphan storage before upload so the driver can retain earlier in-flight batches.
        gl.BufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(batch_capacity * 6 * sizeof(Vertex)),
                      nullptr, GL_STREAM_DRAW);
        gl.BufferSubData(GL_ARRAY_BUFFER, 0,
                         static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                         vertices.data());
        gl.DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
        vertices.clear();
        ++stats.draws;
    }
};
Renderer::Renderer() : impl_(std::make_unique<Impl>()) {
    impl_->initialize();
}
Renderer::~Renderer() = default;
std::string_view Renderer::default_vertex_shader() {
    return vertex_source;
}
std::string_view Renderer::default_fragment_shader() {
    return fragment_source;
}
std::uint64_t Renderer::shader_revision() const {
    return impl_->shader_revision;
}
ShaderReloadResult Renderer::reload_shaders(std::string_view vertex, std::string_view fragment) {
    auto& p = *impl_;
    if (p.recording)
        throw std::logic_error("Shader reload during active render pass");
    ShaderReloadResult result{false, p.shader_revision, {}};
    try {
        if (p.shader_revision == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("Shader revision exhausted");
        const auto candidate =
            detail::build_sprite_program(p.gl, vertex, fragment, result.diagnostics);
        // No throwing operations between candidate release and publication.
        const auto previous = p.program;
        p.program = candidate.id;
        p.premultiplied_location = candidate.premultiplied;
        result.revision = ++p.shader_revision;
        result.applied = true;
        p.gl.UseProgram(0); // Unbind the old program so deletion can retire it immediately.
        p.gl.DeleteProgram(previous);
    } catch (const std::runtime_error& error) {
        result.diagnostics += error.what();
    }
    return result;
}
void Renderer::begin(int width, int height, Camera camera, Color clear) {
    impl_->begin_pass(width, height, camera, clear, 0, 0);
}
void Renderer::begin(RenderTargetHandle target, Camera camera, Color clear) {
    auto& p = *impl_;
    auto& slot = p.target_slot(target);
    p.begin_pass(slot.width, slot.height, camera, clear, slot.framebuffer, slot.texture);
    slot.ready = false;
}
RenderTargetHandle Renderer::create_target(int width, int height) {
    auto& p = *impl_;
    if (p.recording)
        throw std::logic_error("Render target creation during frame");
    std::size_t index = 0;
    while (index < p.targets.size() && p.targets[index].framebuffer)
        ++index;
    if (index == p.targets.size())
        throw std::runtime_error("Render target count limit reached");
    static std::atomic<std::uint64_t> next_serial{1};
    const auto serial = next_serial.fetch_add(1, std::memory_order_relaxed);
    if (!serial)
        throw std::overflow_error("Render target serial exhausted");
    auto candidate = p.allocate_target(width, height);
    candidate.serial = serial;
    p.targets[index] = candidate;
    p.target_memory += Impl::bytes(width, height);
    return {static_cast<std::uint32_t>(index), serial};
}
void Renderer::resize_target(RenderTargetHandle target, int width, int height) {
    auto& p = *impl_;
    if (p.recording)
        throw std::logic_error("Render target resize during frame");
    auto& slot = p.target_slot(target);
    if (slot.width == width && slot.height == height)
        return;
    const auto old_bytes = Impl::bytes(slot.width, slot.height);
    auto candidate = p.allocate_target(width, height, old_bytes);
    candidate.serial = slot.serial;
    p.invalidate_readback(slot.framebuffer);
    p.destroy_target(slot);
    slot = candidate;
    p.target_memory = p.target_memory - old_bytes + Impl::bytes(width, height);
}
void Renderer::release(RenderTargetHandle target) {
    auto& p = *impl_;
    if (p.recording)
        throw std::logic_error("Render target release during frame");
    auto& slot = p.target_slot(target);
    p.target_memory -= Impl::bytes(slot.width, slot.height);
    p.invalidate_readback(slot.framebuffer);
    p.destroy_target(slot);
}
RenderTargetSize Renderer::target_size(RenderTargetHandle target) const {
    const auto& slot = impl_->target_slot(target);
    return {slot.width, slot.height};
}
std::size_t Renderer::live_targets() const {
    return static_cast<std::size_t>(
        std::count_if(impl_->targets.begin(), impl_->targets.end(),
                      [](const auto& slot) { return slot.framebuffer != 0; }));
}
std::size_t Renderer::target_bytes() const {
    return impl_->target_memory;
}
void Renderer::target_sprite(RenderTargetHandle target, Transform model, Color color, Rect uv) {
    auto& p = *impl_;
    const auto& slot = p.target_slot(target);
    if (p.recording && p.destination_texture == slot.texture)
        throw std::logic_error("Render target feedback is forbidden");
    if (!slot.ready)
        throw std::logic_error("Render target has no completed contents");
    p.submit(model, color, uv, slot.texture, true);
}
void Renderer::push_clip(PixelRect rectangle) {
    auto& p = *impl_;
    if (!p.recording)
        throw std::logic_error("Clip outside frame");
    if (rectangle.width < 0 || rectangle.height < 0)
        throw std::invalid_argument("Negative clip size");
    if (p.clip_depth == p.clips.size())
        throw std::overflow_error("Clip stack limit reached");
    const auto parent =
        p.clip_depth ? p.clips[p.clip_depth - 1] : PixelRect{0, 0, p.width, p.height};
    const auto intersect = [](int pos, int size, int base, int span) {
        const auto low =
            std::clamp<std::int64_t>(pos, base, static_cast<std::int64_t>(base) + span);
        const auto high = std::clamp<std::int64_t>(static_cast<std::int64_t>(pos) + size, low,
                                                   static_cast<std::int64_t>(base) + span);
        return std::array<int, 2>{static_cast<int>(low), static_cast<int>(high - low)};
    };
    const auto x = intersect(rectangle.x, rectangle.width, parent.x, parent.width);
    const auto y = intersect(rectangle.y, rectangle.height, parent.y, parent.height);
    p.flush();
    p.clips[p.clip_depth++] = {x[0], y[0], x[1], y[1]};
    p.apply_clip();
}
void Renderer::pop_clip() {
    auto& p = *impl_;
    if (!p.recording || !p.clip_depth)
        throw std::logic_error("Unbalanced clip pop");
    p.flush();
    --p.clip_depth;
    p.apply_clip();
}
void Renderer::set_camera(Camera camera) {
    auto& p = *impl_;
    if (!p.recording)
        throw std::logic_error("Camera switch outside frame");
    const auto extent = camera.extent(p.width, p.height);
    if (!std::isfinite(camera.center.x) || !std::isfinite(camera.center.y))
        throw std::invalid_argument("Nonfinite camera center");
    p.flush();
    p.camera = camera;
    p.extent = extent;
}
void Renderer::Impl::submit(Transform transform, Color color, Rect uv, GLuint object,
                            bool premultiplied) {
    auto& p = *this;
    if (!p.recording)
        throw std::logic_error("Draw outside begin/end");
    for (float value :
         {transform.a, transform.b, transform.c, transform.d, transform.x, transform.y, color.r,
          color.g, color.b, color.a, uv.min.x, uv.min.y, uv.max.x, uv.max.y})
        if (!std::isfinite(value))
            throw std::invalid_argument("Nonfinite sprite");
    for (float value : {uv.min.x, uv.min.y, uv.max.x, uv.max.y})
        if (value < 0 || value > 1)
            throw std::invalid_argument("UV outside [0,1]");
    constexpr std::array<Vec2, 4> local = {
        {{-0.5F, -0.5F}, {0.5F, -0.5F}, {0.5F, 0.5F}, {-0.5F, 0.5F}}};
    std::array<Vec2, 4> positions{};
    Rect bounds{{INFINITY, INFINITY}, {-INFINITY, -INFINITY}};
    for (std::size_t i = 0; i < positions.size(); ++i) {
        positions[i] = transform.apply(local[i]);
        bounds.min.x = std::min(bounds.min.x, positions[i].x);
        bounds.min.y = std::min(bounds.min.y, positions[i].y);
        bounds.max.x = std::max(bounds.max.x, positions[i].x);
        bounds.max.y = std::max(bounds.max.y, positions[i].y);
    }
    if (!overlaps(bounds, {p.camera.center - p.extent * 0.5F, p.camera.center + p.extent * 0.5F})) {
        ++p.stats.culled;
        return;
    }
    if (p.vertices.size() + 6 > batch_capacity * 6 || p.batch_texture != object ||
        p.batch_premultiplied != premultiplied)
        p.flush();
    p.batch_texture = object;
    p.batch_premultiplied = premultiplied;
    for (const std::size_t i : {0U, 1U, 2U, 0U, 2U, 3U}) {
        const Vec2 point = positions[i] - p.camera.center;
        p.vertices.push_back({point.x * 2 / p.extent.x, point.y * 2 / p.extent.y,
                              i == 0 || i == 3 ? uv.min.x : uv.max.x, i < 2 ? uv.min.y : uv.max.y,
                              color});
    }
    ++p.stats.quads;
}
void Renderer::quad(Vec2 center, Vec2 size, Color color, float angle, bool checker) {
    impl_->submit(Transform::from(center, angle, size), color,
                  checker ? Rect{{0.501F, 0.001F}, {0.999F, 0.999F}}
                          : Rect{{0.125F, 0.25F}, {0.125F, 0.25F}},
                  impl_->texture);
}
TextureHandle Renderer::upload(const TextureData& data) {
    auto& p = *impl_;
    if (p.recording)
        throw std::logic_error("Texture upload during frame");
    data.validate();
    std::size_t index = 0;
    while (index < p.textures.size() && p.textures[index].object)
        ++index;
    if (index == p.textures.size())
        throw std::runtime_error("GPU texture limit reached");
    static std::atomic<std::uint64_t> next_serial{1};
    const auto serial = next_serial.fetch_add(1, std::memory_order_relaxed);
    if (!serial)
        throw std::overflow_error("Texture handle serial exhausted");
    const auto object = p.allocate_texture(data);
    p.textures[index] = {object, serial, data.rgba.size()};
    return {static_cast<std::uint32_t>(index), serial};
}
void Renderer::replace_textures(std::span<const TextureReplacement> replacements) {
    auto& p = *impl_;
    if (p.recording)
        throw std::logic_error("Texture replacement during frame");
    if (replacements.size() > max_textures)
        throw std::invalid_argument("Too many texture replacements");
    std::array<bool, max_textures> seen{};
    std::size_t bytes = 0;
    for (const auto& replacement : replacements) {
        p.texture_object(replacement.handle);
        if (seen[replacement.handle.slot])
            throw std::invalid_argument("Duplicate texture replacement handle");
        seen[replacement.handle.slot] = true;
        replacement.data.validate();
        if (replacement.data.rgba.size() > max_texture_reload_bytes - bytes)
            throw std::runtime_error("Texture replacement exceeds 64 MiB staging budget");
        bytes += replacement.data.rgba.size();
    }
    struct Candidates {
        Gl& gl;
        std::array<GLuint, max_textures> objects{};
        ~Candidates() {
            for (const auto object : objects)
                if (object)
                    gl.DeleteTextures(1, &object);
        }
    } candidates{p.gl};
    for (std::size_t i = 0; i < replacements.size(); ++i)
        candidates.objects[i] = p.allocate_texture(replacements[i].data);
    // No throwing work during publication. RAII now retires the previous objects.
    for (std::size_t i = 0; i < replacements.size(); ++i) {
        auto& slot = p.textures[replacements[i].handle.slot];
        std::swap(slot.object, candidates.objects[i]);
        slot.bytes = replacements[i].data.rgba.size();
    }
}
void Renderer::release(TextureHandle texture) {
    auto& p = *impl_;
    if (p.recording)
        throw std::logic_error("Texture release during frame");
    const auto object = p.texture_object(texture);
    p.gl.DeleteTextures(1, &object);
    p.textures[texture.slot] = {};
}
std::size_t Renderer::live_textures() const {
    return static_cast<std::size_t>(
        std::count_if(impl_->textures.begin(), impl_->textures.end(),
                      [](const auto& slot) { return slot.object != 0; }));
}
std::size_t Renderer::texture_bytes() const {
    std::size_t bytes = 0;
    for (const auto& slot : impl_->textures)
        bytes += slot.bytes;
    return bytes;
}
GraphicsInfo Renderer::graphics_info() const {
    const auto read = [&](GLenum name) {
        const auto* value = impl_->gl.GetString(name);
        return value ? std::string(reinterpret_cast<const char*>(value))
                     : std::string("unavailable");
    };
    return {read(GL_VENDOR), read(GL_RENDERER), read(GL_VERSION)};
}
void Renderer::sprite(TextureHandle texture, Transform model, Color color, Rect uv) {
    impl_->submit(model, color, uv, impl_->texture_object(texture));
}
void Renderer::text(Vec2 origin, float size, std::string_view value, Color color) {
    if (!std::isfinite(size) || size <= 0)
        throw std::invalid_argument("Invalid text size");
    float cursor = origin.x;
    for (char c : value) {
        const auto rows = glyph(c);
        for (std::size_t y = 0; y < rows.size(); ++y)
            for (int x = 0; x < 5; ++x)
                if (rows[y] & (1U << (4 - x)))
                    quad({cursor + (static_cast<float>(x) + 0.5F) * size,
                          origin.y - (static_cast<float>(y) + 0.5F) * size},
                         {size, size}, color);
        cursor += size * 6;
    }
}
void Renderer::end() {
    if (!impl_->recording)
        throw std::logic_error("Renderer::end without begin");
    if (impl_->clip_depth)
        throw std::logic_error("Unbalanced clip stack at end");
    impl_->flush();
    impl_->recording = false;
    if (impl_->framebuffer)
        for (auto& target : impl_->targets)
            if (target.framebuffer == impl_->framebuffer)
                target.ready = true;
}
RenderStats Renderer::stats() const {
    return impl_->stats;
}
std::array<unsigned char, 4> Renderer::pixel(int x, int y) const {
    auto& p = *impl_;
    if (p.recording || x < 0 || y < 0 || x >= p.width || y >= p.height)
        throw std::invalid_argument("Invalid pixel read");
    std::array<unsigned char, 4> result{};
    p.gl.BindFramebuffer(GL_READ_FRAMEBUFFER, p.framebuffer);
    p.gl.ReadBuffer(p.framebuffer ? GL_COLOR_ATTACHMENT0 : GL_BACK);
    p.gl.ReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
    return result;
}
void Renderer::screenshot(std::string_view path) const {
    auto& p = *impl_;
    if (p.recording || p.width <= 0 || p.height <= 0)
        throw std::logic_error("Screenshot requires a completed frame");
    std::vector<unsigned char> pixels(static_cast<std::size_t>(p.width) *
                                      static_cast<std::size_t>(p.height) * 3);
    p.gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
    p.gl.BindFramebuffer(GL_READ_FRAMEBUFFER, p.framebuffer);
    p.gl.ReadBuffer(p.framebuffer ? GL_COLOR_ATTACHMENT0 : GL_BACK);
    p.gl.ReadPixels(0, 0, p.width, p.height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    p.gl.PixelStorei(GL_PACK_ALIGNMENT, 4);
    std::ofstream file(std::string(path), std::ios::binary);
    file << "P6\n" << p.width << ' ' << p.height << "\n255\n";
    const auto row = static_cast<std::size_t>(p.width) * 3;
    for (int y = p.height - 1; y >= 0; --y)
        file.write(reinterpret_cast<const char*>(pixels.data() + static_cast<std::size_t>(y) * row),
                   static_cast<std::streamsize>(row));
    if (!file)
        throw std::runtime_error("Screenshot write failed");
}
bool Renderer::healthy() const {
    return impl_->high_errors == 0 && impl_->gl.GetError() == GL_NO_ERROR;
}
} // namespace engine
