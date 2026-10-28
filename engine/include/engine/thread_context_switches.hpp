#pragma once
#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <sys/resource.h>
#include <system_error>

namespace engine {
struct ThreadContextSwitches {
    std::uint64_t voluntary{}, involuntary{};
    bool operator==(const ThreadContextSwitches&) const = default;
};
// Linux calling-thread cumulative counts. Counts do not give durations or blocking causes.
inline ThreadContextSwitches thread_context_switches() {
    rusage usage{};
    if (getrusage(RUSAGE_THREAD, &usage) != 0)
        throw std::system_error(errno, std::generic_category(), "Thread context switches");
    if (usage.ru_nvcsw < 0 || usage.ru_nivcsw < 0)
        throw std::runtime_error("Negative thread context-switch counter");
    return {static_cast<std::uint64_t>(usage.ru_nvcsw),
            static_cast<std::uint64_t>(usage.ru_nivcsw)};
}
} // namespace engine
