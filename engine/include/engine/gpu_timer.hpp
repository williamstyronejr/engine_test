#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace engine {
struct GpuTimingSample {
    std::uint64_t frame{};
    double milliseconds{};
};
// Raw reversed timestamp pair. This records an observation, not its cause.
struct GpuTimingFailure {
    std::uint64_t frame{}, start_ns{}, end_ns{}, last_attempted_frame{};
    std::size_t slot{};
};
struct GpuTimingStats {
    int counter_bits{};
    bool supported{};
    std::size_t pending{};
    std::uint64_t submitted{}, completed{}, skipped{}, invalid{};
};
// Owning GL context/thread only, including construction/destruction. No CPU waits.
class GpuTimer {
  public:
    static constexpr std::size_t capacity = 8;
    static constexpr std::size_t failure_capacity = 8;
    GpuTimer();
    ~GpuTimer();
    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;
    // Tags must be nonzero and strictly increasing. Always pair begin/end, even if false.
    // False means unsupported counters or a full pending pool; rendering continues normally.
    bool begin(std::uint64_t frame);
    void end();
    // Between scopes only. Poll once; never wait or read an unavailable result.
    // Returned span remains valid until the next collect or destruction.
    std::span<const GpuTimingSample> collect();
    // First failure_capacity reversed pairs, in collection order, for this timer's
    // lifetime. No allocations, extra GL reads or waits. Later failures only raise
    // stats().invalid; omitted count = invalid - failures().size().
    // Returned span remains valid until the next collect or destruction.
    std::span<const GpuTimingFailure> failures() const;
    GpuTimingStats stats() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace engine
