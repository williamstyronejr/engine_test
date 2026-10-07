#pragma once
#include "engine/gpu_timer.hpp"
#include "gl.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
namespace engine::detail {
struct TimerBackend {
    Gl gl;
    std::array<GLuint, GpuTimer::capacity * 2> queries{};
    GLint bits{};
    TimerBackend() {
        gl.GetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &bits);
        if (bits != 64)
            return;
        gl.GenQueries(static_cast<GLsizei>(queries.size()), queries.data());
        if (std::find(queries.begin(), queries.end(), 0U) != queries.end()) {
            gl.DeleteQueries(static_cast<GLsizei>(queries.size()), queries.data());
            throw std::runtime_error("GPU timer query allocation failed");
        }
    }
    TimerBackend(const TimerBackend&) = delete;
    TimerBackend& operator=(const TimerBackend&) = delete;
    ~TimerBackend() {
        // Timestamp queries need no matching EndQuery, even for an interrupted scope.
        if (bits == 64)
            gl.DeleteQueries(static_cast<GLsizei>(queries.size()), queries.data());
    }
    void stamp(std::size_t index) { gl.QueryCounter(queries[index], GL_TIMESTAMP); }
    bool available(std::size_t index) {
        GLuint ready{};
        gl.GetQueryObjectuiv(queries[index], GL_QUERY_RESULT_AVAILABLE, &ready);
        return ready != 0;
    }
    std::uint64_t result(std::size_t index) {
        GLuint64 value{};
        gl.GetQueryObjectui64v(queries[index], GL_QUERY_RESULT, &value);
        return value;
    }
};
} // namespace engine::detail
