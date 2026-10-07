#pragma once
#include "engine/renderer.hpp"
#include "gl.hpp"
#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace engine::detail {
struct SpriteProgram {
    GLuint id{};
    GLint premultiplied{};
};
struct ProgramCandidate {
    Gl& gl;
    GLuint vertex{}, fragment{}, program{};
    explicit ProgramCandidate(Gl& functions) : gl(functions) {}
    ProgramCandidate(const ProgramCandidate&) = delete;
    ProgramCandidate& operator=(const ProgramCandidate&) = delete;
    ~ProgramCandidate() {
        if (program)
            gl.DeleteProgram(program);
        if (vertex)
            gl.DeleteShader(vertex);
        if (fragment)
            gl.DeleteShader(fragment);
    }
};
inline void validate_source(std::string_view source, std::string_view name) {
    if (source.empty() || source.size() > Renderer::max_shader_source_bytes ||
        source.find('\0') != std::string_view::npos)
        throw std::runtime_error(std::string(name) + ": source must be 1..65536 bytes without NUL");
}
inline void append_log(Gl& gl, GLuint object, bool program, std::string_view name,
                       std::string& diagnostics) {
    std::array<char, 4096> log{};
    GLint required{};
    GLsizei length{};
    if (program) {
        gl.GetProgramiv(object, GL_INFO_LOG_LENGTH, &required);
        gl.GetProgramInfoLog(object, static_cast<GLsizei>(log.size()), &length, log.data());
    } else {
        gl.GetShaderiv(object, GL_INFO_LOG_LENGTH, &required);
        gl.GetShaderInfoLog(object, static_cast<GLsizei>(log.size()), &length, log.data());
    }
    if (length > 0) {
        diagnostics += std::string(name) + ":\n";
        diagnostics.append(log.data(), static_cast<std::size_t>(length));
        diagnostics += '\n';
    }
    if (required > static_cast<GLint>(log.size()))
        diagnostics += "[log truncated to 4095 bytes]\n";
}
inline void compile(Gl& gl, GLuint& id, GLenum stage, std::string_view source,
                    std::string_view name, std::string& diagnostics) {
    id = gl.CreateShader(stage);
    if (!id)
        throw std::runtime_error(std::string(name) + ": shader allocation failed");
    const char* data = source.data();
    const auto length = static_cast<GLint>(source.size());
    gl.ShaderSource(id, 1, &data, &length);
    gl.CompileShader(id);
    append_log(gl, id, false, name, diagnostics);
    GLint ok{};
    gl.GetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok)
        throw std::runtime_error(std::string(name) + ": compilation failed");
}
inline void count(Gl& gl, GLuint program, GLenum interface, GLint expected) {
    GLint actual{};
    gl.GetProgramInterfaceiv(program, interface, GL_ACTIVE_RESOURCES, &actual);
    if (actual != expected)
        throw std::runtime_error("Sprite interface: unexpected active resource count (interface " +
                                 std::to_string(interface) + ", expected " +
                                 std::to_string(expected) + ", got " + std::to_string(actual) +
                                 ")");
}
inline GLint resource(Gl& gl, GLuint program, GLenum interface, const char* name, GLenum type,
                      GLint location = -1) {
    const auto index = gl.GetProgramResourceIndex(program, interface, name);
    if (index == GL_INVALID_INDEX)
        throw std::runtime_error(std::string("Sprite interface: missing active ") + name);
    constexpr std::array<GLenum, 3> properties{GL_TYPE, GL_ARRAY_SIZE, GL_LOCATION};
    std::array<GLint, 3> values{};
    gl.GetProgramResourceiv(program, interface, index, 3, properties.data(), 3, nullptr,
                            values.data());
    if (values[0] != static_cast<GLint>(type) || values[1] != 1 || values[2] < 0 ||
        (location >= 0 && values[2] != location))
        throw std::runtime_error(
            std::string("Sprite interface: incompatible type/array/location: ") + name);
    if (interface == GL_PROGRAM_INPUT || interface == GL_PROGRAM_OUTPUT) {
        const GLenum property = GL_LOCATION_COMPONENT;
        GLint component{};
        gl.GetProgramResourceiv(program, interface, index, 1, &property, 1, nullptr, &component);
        if (component != 0)
            throw std::runtime_error(std::string("Sprite interface: unsupported component: ") +
                                     name);
    }
    if (interface == GL_UNIFORM || interface == GL_PROGRAM_OUTPUT) {
        const GLenum property = interface == GL_UNIFORM ? GL_BLOCK_INDEX : GL_LOCATION_INDEX;
        GLint value{};
        gl.GetProgramResourceiv(program, interface, index, 1, &property, 1, nullptr, &value);
        if (value != (interface == GL_UNIFORM ? -1 : 0))
            throw std::runtime_error(
                std::string("Sprite interface: incompatible block/output index: ") + name);
    }
    return values[2];
}
inline SpriteProgram build_sprite_program(Gl& gl, std::string_view vertex,
                                          std::string_view fragment, std::string& diagnostics) {
    validate_source(vertex, "sprite.vert");
    validate_source(fragment, "sprite.frag");
    ProgramCandidate candidate(gl);
    compile(gl, candidate.vertex, GL_VERTEX_SHADER, vertex, "sprite.vert", diagnostics);
    compile(gl, candidate.fragment, GL_FRAGMENT_SHADER, fragment, "sprite.frag", diagnostics);
    candidate.program = gl.CreateProgram();
    if (!candidate.program)
        throw std::runtime_error("Sprite program allocation failed");
    gl.AttachShader(candidate.program, candidate.vertex);
    gl.AttachShader(candidate.program, candidate.fragment);
    gl.LinkProgram(candidate.program);
    append_log(gl, candidate.program, true, "sprite link", diagnostics);
    GLint linked{};
    gl.GetProgramiv(candidate.program, GL_LINK_STATUS, &linked);
    if (!linked)
        throw std::runtime_error("Sprite program link failed");
    count(gl, candidate.program, GL_PROGRAM_INPUT, 3);
    count(gl, candidate.program, GL_PROGRAM_OUTPUT, 1);
    count(gl, candidate.program, GL_UNIFORM, 2);
    count(gl, candidate.program, GL_UNIFORM_BLOCK, 0);
    count(gl, candidate.program, GL_SHADER_STORAGE_BLOCK, 0);
    count(gl, candidate.program, GL_ATOMIC_COUNTER_BUFFER, 0);
    count(gl, candidate.program, GL_VERTEX_SUBROUTINE_UNIFORM, 0);
    count(gl, candidate.program, GL_FRAGMENT_SUBROUTINE_UNIFORM, 0);
    resource(gl, candidate.program, GL_PROGRAM_INPUT, "position", GL_FLOAT_VEC2, 0);
    resource(gl, candidate.program, GL_PROGRAM_INPUT, "texcoord", GL_FLOAT_VEC2, 1);
    resource(gl, candidate.program, GL_PROGRAM_INPUT, "tint", GL_FLOAT_VEC4, 2);
    resource(gl, candidate.program, GL_PROGRAM_OUTPUT, "output_color", GL_FLOAT_VEC4, 0);
    const auto atlas = resource(gl, candidate.program, GL_UNIFORM, "atlas", GL_SAMPLER_2D);
    const auto premultiplied = resource(gl, candidate.program, GL_UNIFORM, "premultiplied", GL_INT);
    // Initialize candidate uniforms without changing the currently bound program.
    gl.ProgramUniform1i(candidate.program, atlas, 0);
    gl.ProgramUniform1i(candidate.program, premultiplied, 0);
    gl.DetachShader(candidate.program, candidate.vertex);
    gl.DetachShader(candidate.program, candidate.fragment);
    return {std::exchange(candidate.program, 0), premultiplied};
}
} // namespace engine::detail
