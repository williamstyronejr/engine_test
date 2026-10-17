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
    GpuTimingMethod method;
    GLint bits{};
    bool active{};
    explicit TimerBackend(GpuTimingMethod selected = GpuTimingMethod::elapsed) : method(selected) {
        gl.GetQueryiv(method == GpuTimingMethod::elapsed ? GL_TIME_ELAPSED : GL_TIMESTAMP,
                      GL_QUERY_COUNTER_BITS, &bits);
        if (bits != 64)
            return;
        gl.GenQueries(query_count(), queries.data());
        if (std::find(queries.begin(), queries.begin() + query_count(), 0U) !=
            queries.begin() + query_count()) {
            gl.DeleteQueries(query_count(), queries.data());
            throw std::runtime_error("GPU timer query allocation failed");
        }
    }
    TimerBackend(const TimerBackend&) = delete;
    TimerBackend& operator=(const TimerBackend&) = delete;
    GLsizei query_count() const {
        return static_cast<GLsizei>(GpuTimer::capacity *
                                    (method == GpuTimingMethod::elapsed ? 1 : 2));
    }
    ~TimerBackend() {
        if (active)
            gl.EndQuery(GL_TIME_ELAPSED);
        if (bits == 64)
            gl.DeleteQueries(query_count(), queries.data());
    }
    void begin(std::size_t slot) {
        if (method == GpuTimingMethod::timestamp_pair) {
            gl.QueryCounter(queries[slot * 2], GL_TIMESTAMP);
            return;
        }
        GLint current{};
        gl.GetQueryiv(GL_TIME_ELAPSED, GL_CURRENT_QUERY, &current);
        if (current)
            throw std::logic_error("GPU elapsed query already active in this context");
        gl.BeginQuery(GL_TIME_ELAPSED, queries[slot]);
        active = true;
    }
    void end(std::size_t slot) {
        if (method == GpuTimingMethod::timestamp_pair)
            gl.QueryCounter(queries[slot * 2 + 1], GL_TIMESTAMP);
        else {
            gl.EndQuery(GL_TIME_ELAPSED);
            active = false;
        }
    }
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
