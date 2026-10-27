#pragma once
#include <cerrno>
#include <cstdint>
#include <system_error>
#include <time.h>

namespace engine {
// Linux calling-thread user + system CPU time; excludes other threads and time not running.
// Diagnostic callers opt in explicitly. Errors throw rather than manufacture a zero sample.
inline std::uint64_t thread_cpu_nanoseconds() {
    timespec value{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) != 0)
        throw std::system_error(errno, std::generic_category(), "Thread CPU clock");
    return static_cast<std::uint64_t>(value.tv_sec) * 1000000000ULL +
           static_cast<std::uint64_t>(value.tv_nsec);
}
inline std::uint64_t thread_cpu_resolution_nanoseconds() {
    timespec value{};
    if (clock_getres(CLOCK_THREAD_CPUTIME_ID, &value) != 0)
        throw std::system_error(errno, std::generic_category(), "Thread CPU clock resolution");
    return static_cast<std::uint64_t>(value.tv_sec) * 1000000000ULL +
           static_cast<std::uint64_t>(value.tv_nsec);
}
} // namespace engine
